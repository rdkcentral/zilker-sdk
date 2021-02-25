/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Created by Thomas Lea on 3/15/16.
 */

#include <icBuildtime.h>
#include <sys/socket.h>
#include <icLog/logging.h>
#include <string.h>
#include <sys/errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <cjson/cJSON.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/threadUtils.h>
#include <icUtil/stringUtils.h>
#include "zhalAsyncReceiver.h"
#include "zhalPrivate.h"

#define ZHAL_EVENT_PORT 8711
#define ZHAL_EVENT_MULTICAST_GROUP "225.0.0.51"
#define ASYNC_RECVBUF_SIZE (64 * 1024)
#define STOP_WAIT_MILLIS (250L)

static pthread_t asyncThread;
static void *asyncReceiverThreadProc(void*);
static bool asyncReceiverThreadRunning = false;

static zhalIpcResponseHandler myIpcHandler = NULL;
static zhalEventHandler myEventHandler = NULL;
static int pipeFDs[2] = {-1,-1};
static pthread_mutex_t STARTUP_MTX = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t STARTUP_COND = PTHREAD_COND_INITIALIZER;

static void *invokeIpcHandlerCallback(void *arg);
static void *invokeEventHandlerCallback(void *arg);

// the host name of the event producer
static char *eventProducerHostname = NULL;

int zhalAsyncReceiverStart(const char *host, zhalIpcResponseHandler ipcHandler, zhalEventHandler eventHandler)
{
    myIpcHandler = ipcHandler;
    myEventHandler = eventHandler;

    pipe(pipeFDs);

    initTimedWaitCond(&STARTUP_COND);

    pthread_mutex_lock(&STARTUP_MTX);

    eventProducerHostname = strdup(host);

    createThread(&asyncThread, asyncReceiverThreadProc, NULL, "zhalAsyncRcvr");

    // We want to make sure the thread is up and running and listening on the socket before continuing...
    incrementalCondTimedWait(&STARTUP_COND, &STARTUP_MTX, 10);
    bool didStart = asyncReceiverThreadRunning;
    pthread_mutex_unlock(&STARTUP_MTX);

    if (didStart)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}

int zhalAsyncReceiverStop()
{
#ifndef CONFIG_DEBUG_ZITH_CI_TESTS
    /* Give the workers a moment to finish consuming any in-flight events from ZigbeeCore */
    /* FIXME: set this up to block until all network messages and worker threads are finished */
    struct timespec timeout = {
            .tv_sec = 0,
            .tv_nsec = STOP_WAIT_MILLIS * 1000L * 1000L
    };

    /* coverity[sleep] kludge to wait for some detached workers to finish */
    nanosleep(&timeout, NULL);
#endif

    pthread_mutex_lock(&STARTUP_MTX);
    asyncReceiverThreadRunning = false;
    free(eventProducerHostname);
    eventProducerHostname = NULL;
    pthread_mutex_unlock(&STARTUP_MTX);

    // Write to the pipe to signal the socket receiver to stop
    char dummy = '\0';
    write(pipeFDs[1], &dummy, 1);

    // Wait for it to finish
    pthread_join(asyncThread, NULL);

    // Cleanup the pipe
    close(pipeFDs[0]);
    close(pipeFDs[1]);

    return 0;
}

static int setupAsyncSocket()
{
    // create the UDP multicast socket
    //
    int32_t sockFD = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockFD < 0)
    {
        icLogError(LOG_TAG, "unable to create event listening socket : %s", strerror(errno));
        return -1;
    }

    // allow multiple sockets/receivers to bind to this port number
    //
    int reuse = 1;
    int rc = setsockopt(sockFD, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse, sizeof(reuse));
    if (rc < 0)
    {
        icLogError(LOG_TAG, "unable to set SO_REUSEPORT for event listener : %s", strerror(errno));
        close(sockFD);
        return -1;
    }
#ifdef SO_REUSEPORT
    // not available on some platforms like ngHub and xb3
    if (setsockopt(sockFD, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) != 0)
    {
        icLogWarn(LOG_TAG, "unable to set SO_REUSEPORT for event listener : %s", strerror(errno));
    }
#endif

    // bind to the port all services broadcast events on
    //
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    if (stringCompare("127.0.0.1", eventProducerHostname, false) == 0)
    {
        // the event producer is on loopback, so just bind that
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    else
    {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    addr.sin_port = htons(ZHAL_EVENT_PORT);

    rc = bind(sockFD, (struct sockaddr *) &addr, sizeof(addr));
    if (rc < 0)
    {
        icLogError(LOG_TAG, "unable to bind listener : %s", strerror(errno));
        close(sockFD);
        return -1;
    }

    // join the multicast group
    //
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ZHAL_EVENT_MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_LOOPBACK);
    rc = setsockopt(sockFD, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    if (rc < 0)
    {
        icLogError(LOG_TAG, "unable to join multicast group for listener : %s", strerror(errno));
        close(sockFD);
        return -1;
    }

    return sockFD;
}

static void *asyncReceiverThreadProc(void *arg)
{
    icLogInfo(LOG_TAG, "asyncReceiverThreadProc starting");

    int sockFD = setupAsyncSocket();

    pthread_mutex_lock(&STARTUP_MTX);
    // Set that we are up and running and broadcast to waiters
    asyncReceiverThreadRunning = true;
    pthread_cond_broadcast(&STARTUP_COND);
    pthread_mutex_unlock(&STARTUP_MTX);

    fd_set fds;
    int maxFD = sockFD > pipeFDs[0] ? sockFD+1 : pipeFDs[0]+1;

    char *recvBuf = (char*)malloc(ASYNC_RECVBUF_SIZE * sizeof(char));

    while(sockFD >= 0 && asyncReceiverThreadRunning)
    {
        struct sockaddr_in remoteAddr;
        unsigned int remoteAddrLen = sizeof(remoteAddr);

        // Setup for the select
        FD_ZERO(&fds);
        // coverity[uninit_use]
        FD_SET(pipeFDs[0], &fds);
        // coverity[uninit_use]
        FD_SET(sockFD, &fds);

        // Just keep waiting until we get one or the other
        select(maxFD, &fds, NULL, NULL, NULL);

        // Check if we should terminate
        if (FD_ISSET(pipeFDs[0], &fds) && !asyncReceiverThreadRunning)
        {
            icLogDebug(LOG_TAG, "Told to shutdown async receiver thread...");
            break;
        }

        // block until something shows on this UDP socket
        ssize_t nbytes = recvfrom(sockFD,
                                  recvBuf,
                                  ASYNC_RECVBUF_SIZE-1,
                                  0,
                                  (struct sockaddr *) &remoteAddr,
                                  &remoteAddrLen);
        if(nbytes < 0)
        {
            // read error, loop back around to check if we should cancel
            //
            icLogWarn(LOG_TAG, "failed to receive async");
            continue;
        }

        recvBuf[nbytes] = '\0'; //null terminate

        //icLogDebug(LOG_TAG, "received: %s", recvBuf);

        cJSON *asyncJson = cJSON_Parse(recvBuf);
        if(asyncJson == NULL)
        {
            icLogWarn(LOG_TAG, "Unable to parse async data: %s", recvBuf);
            continue;
        }

        cJSON *eventType = cJSON_GetObjectItem(asyncJson, "eventType");
        if(eventType != NULL)
        {
            if(strcmp("ipcResponse", eventType->valuestring) == 0)
            {
                if(myIpcHandler != NULL)
                {
                    createDetachedThread(invokeIpcHandlerCallback, asyncJson, "zhalIPCWorker");
                    asyncJson = NULL; // the thread will clean up
                }
            }
            else
            {
                if(myEventHandler != NULL)
                {
                    createDetachedThread(invokeEventHandlerCallback, asyncJson, "zhalEventWorker");
                    asyncJson = NULL; // the thread will clean up
                }
            }
        }

        if(asyncJson != NULL)
        {
            cJSON_Delete(asyncJson);
        }
    }

    free(recvBuf);
    if(sockFD >=0)
    {
        close(sockFD);
    }

    icLogInfo(LOG_TAG, "asyncReceiverThreadProc exiting");
    return NULL;
}

static void *invokeIpcHandlerCallback(void *arg)
{
    cJSON *json = (cJSON*)arg;
    if(myIpcHandler(json) == 0)
    {
        //it was not handled, so we have to free it here
        cJSON_Delete(json);
    }
    return NULL;
}

static void *invokeEventHandlerCallback(void *arg)
{
    cJSON *json = (cJSON*)arg;
    if(myEventHandler(json) == 0)
    {
        //it was not handled, so we have to free it here
        cJSON_Delete(json);
    }
    return NULL;
}
