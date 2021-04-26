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

/*-----------------------------------------------
 * IPCReceiver.c
 *
 * Service-side functions that serve as the foundation
 * for receiving IPC requests, processing them, and
 * supplying a response.  Requires the Service to provide
 * a handler so that requests can be processed as they arrive.
 *
 * Services should be able to process requests from
 * both Native & Java clients and send back responses
 * that can be parsed by these clients.
 *
 * Note: this correlates closely to IPCReceiver.java
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icConcurrent/threadPool.h>
#include <icUtil/stringUtils.h>
#include <icIpc/ipcReceiver.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/threadUtils.h>
#include "ipcCommon.h"
#include "transport/transport.h"

#define INTERNAL_SHUTDOWN_MSG   "byebye"

#define ADD_TASK_RETRY_US 100
#define ADD_TASK_MAX_TRIES 3

typedef enum {
    IPC_REC_RUNNING,
    IPC_REC_SHUTDOWN,
    IPC_REC_DEAD
} ipcState;

typedef struct _ipcReceiver
{
    int                     serverSock;
    pthread_t               mainThread;         // see mainLoop()
    serviceRequestHandler   handler;            // callback when messages arrive
    asyncShutdownNotifyFunc shutdownNotifyFunc; // callback when this receiver is being shut down
    uint16_t                servicePortNum;     // port number we're listening on
    icThreadPool            *threadPool;        // pool for processing requests
    ipcState                state;              // if running, shutting down, or finished
    pthread_mutex_t         ipcMtx;             // mutex for this IPCReceiver instance
    pthread_cond_t          ipcCond;            // conditional broadcast to notify threads
    uint8_t                 refcnt;
} ipcReceiver;

typedef struct _ipcWorkerArgs
{
    int sockfd;
    transport_control_t control;
    IPCMessage* input;
    serviceRequestHandler   handler;
    ipcReceiver *receiver;
} ipcWorkerArgs;

/* private function declarations */
static int establishServerSocket(uint16_t servicePortNum, serviceVisibility scope);
static void requestWorkerTask(void *args);
static void *mainLoop(void *ipcRecieverPtr);
static void acquireReceiver(ipcReceiver *receiver);
static void releaseReceiver(ipcReceiver *receiver);
static void requestWorkTaskArgFreeFunc(void *arg);

/*
 * Creates a server-socket to listen on 'servicePortNum' and process incoming message
 * requests.  As each arrives, it will be placed into a processing thread and
 * forwarded to the 'serviceRequestHandler' for processing.
 *
 * @param servicePortNum - the TCP port to listen on
 * @param serviceRequestHandler - the handler to utilize as messages arrive
 * @param scope - the visibility of the handler (used when registering with ServiceRegistry)
 * @param minThreads - the min number of threads for the 'threadPool'
 * @param maxThreads - the max number of threads for the 'threadPool'
 * @param maxQueueSize - the max number of requests to keep in the threadPool's queue
 * @param shutdownNotifyFunc - callback to be notified when the returned receiver is shutting down and should no longer be used
 *
 * @return IPC_SUCCESS when the handler exits cleanly
 * @see shutdownRequestHandler()
 */
IpcReceiver startRequestHandler(uint16_t servicePortNum, serviceRequestHandler handler, serviceVisibility scope,
                                uint16_t minThreads, uint16_t maxThreads, uint32_t maxQueueSize,
                                asyncShutdownNotifyFunc shutdownNotifyFunc)
{
    const char *serviceHost = LOCAL_LOOPBACK;       // TODO: if multi-host environment, bind to ANY instead of assuming LOCAL_LOOPBACK

    ipcReceiver* receiver = (ipcReceiver*)calloc(1, sizeof(ipcReceiver));
    receiver->state = IPC_REC_RUNNING;

    // create our listening socket
    //
    receiver->serverSock = transport_establish(serviceHost, servicePortNum);
    if (receiver->serverSock < 0)
    {
        // failed, so return
        //
        icLogWarn(API_LOG_CAT, "failed to open server-socket on port %" PRIu16": %s", servicePortNum, strerror(errno));
        free(receiver);
        return NULL;
    }

    // make mutext/condition for state changes
    //
    pthread_mutex_init(&receiver->ipcMtx, NULL);
    initTimedWaitCond(&receiver->ipcCond);

    // make a thread pool for this to use for processing IPC requests
    //
    char poolName[64];
    sprintf(poolName, "ipcTP:%"PRIu16, servicePortNum);
    receiver->threadPool = threadPoolCreate(poolName, minThreads, maxThreads, maxQueueSize);

    // save the callback handler and other variables
    //
    receiver->handler = handler;
    receiver->servicePortNum = servicePortNum;
    receiver->shutdownNotifyFunc = shutdownNotifyFunc;

    // start the main loop thread
    //
    char *threadName = stringBuilder("ipcRec:%"PRIu16, receiver->servicePortNum);
    createThread(&receiver->mainThread, mainLoop, receiver, threadName);
    free(threadName);

    return (IpcReceiver)receiver;
}

/*
 *
 */
static void setState(ipcReceiver *receiver, ipcState state)
{
    // set state
    pthread_mutex_lock(&receiver->ipcMtx);
    receiver->state = state;

    // broadcast the change
    pthread_cond_broadcast(&receiver->ipcCond);
    pthread_mutex_unlock(&receiver->ipcMtx);
}

/*
 *
 */
static ipcState getState(ipcReceiver *receiver)
{
    pthread_mutex_lock(&receiver->ipcMtx);
    ipcState retVal = receiver->state;
    pthread_mutex_unlock(&receiver->ipcMtx);

    return retVal;
}

/*
 * Stops the receive thread.  Generally called during Service shutdown
 */
void shutdownRequestHandler(IpcReceiver receiver)
{
    ipcReceiver* r = (ipcReceiver*) receiver;

    if (r != NULL && getState(r) == IPC_REC_RUNNING) {
        // flag this receiver as 'shutting down'
        //
        setState(r, IPC_REC_SHUTDOWN);
        icLogInfo(API_LOG_CAT, "shutting down IPCReceiver for port %"PRIu16, r->servicePortNum);

        // close the socket so we don't attempt to read anything more
        //
        if (r->serverSock >= 0) {
            transport_close(r->serverSock);
            r->serverSock = -1;

            // wait for the receiver to transition to IPC_REC_DEAD
            //
            waitForRequestHandlerToShutdown((IpcReceiver)r);
        }

        // now safe to destroy the thread pool since nothing
        // else could possibly come in and be scheduled
        //
        if (r->threadPool != NULL)
        {
            threadPoolDestroy(r->threadPool);
            r->threadPool = NULL;
        }

        pthread_mutex_lock(&r->ipcMtx);
        while (r->refcnt != 0)
        {
            // Tell any threads monitoring the receiver that they should check again for shutdown
            pthread_cond_broadcast(&r->ipcCond);
            icLogInfo(API_LOG_CAT, "IPCReceiver waiting for %"PRIu8 " reference(s) to be released", r->refcnt);
            incrementalCondTimedWait(&r->ipcCond, &r->ipcMtx, 1);
        }
        pthread_mutex_unlock(&r->ipcMtx);

        pthread_join(r->mainThread, NULL);

        // cleanup mutex/condition
        pthread_mutex_destroy(&r->ipcMtx);
        pthread_cond_destroy(&r->ipcCond);

        // Notify callback that the receiver shut down
        if (r->shutdownNotifyFunc != NULL)
        {
            r->shutdownNotifyFunc();
        }

        icLogInfo(API_LOG_CAT, "IPCReceiver on port %" PRIu16 " shut down successfully", r->servicePortNum);
        // cleanup memory
        free(receiver);
    }
}

/*
 * Suspends the calling thread until the receiver shuts down
 */
void waitForRequestHandlerToShutdown(IpcReceiver receiver)
{
    ipcReceiver *r = (ipcReceiver*)receiver;
    if (r != NULL)
    {
        acquireReceiver(r);
        while (getState(r) != IPC_REC_DEAD)
        {
            pthread_mutex_lock(&r->ipcMtx);
            incrementalCondTimedWait(&r->ipcCond, &r->ipcMtx, 5);
            pthread_mutex_unlock(&r->ipcMtx);
        }

        icLogInfo(API_LOG_CAT, "IPCReceiver on port %" PRIu16 " exited", r->servicePortNum);
        releaseReceiver(r);
    }
}

/*
 * collect statistics about the IPC handlers, and populate
 * them into the supplied runtimeStatsPojo container
 */
void collectIpcStatistics(IpcReceiver receiver, runtimeStatsPojo *container, bool thenClear)
{
    // for now, just get the thread pool stats
    //
    ipcReceiver *r = (ipcReceiver*)receiver;
    if (r != NULL && r->threadPool != NULL)
    {
        threadPoolStats *tstats = threadPoolGetStatistics(r->threadPool, thenClear);
        if (tstats != NULL)
        {
            // transfer each value into the container
            //
            put_long_in_runtimeStatsPojo(container, "ipcTpoolTotalRan", tstats->totalTasksRan);
            put_long_in_runtimeStatsPojo(container, "ipcTpoolTotalQueued", tstats->totalTasksQueued);
            put_long_in_runtimeStatsPojo(container, "ipcTpoolMaxQueued", tstats->maxTasksQueued);
            put_long_in_runtimeStatsPojo(container, "ipcTpoolMaxConcurrent", tstats->maxConcurrentTasks);
            free(tstats);
        }
    }
}

static void requestWorkerTask(void *args)
{
    ipcWorkerArgs *wargs = (ipcWorkerArgs *) args;
    IPCMessage *input = wargs->input;
    IPCMessage *output = NULL;
    IPCCode rc;
    bool shutdownSelf = false;

    // assume there is a reply, so create the output container
    //
    output = createIPCMessage();

    // look for special codes (PING, etc)
    //
    if (input->msgCode == PING_REQUEST)
    {
        // simply respond since we successfully read the message
        //
#ifdef CONFIG_DEBUG_IPC
        icLogDebug(API_LOG_CAT, "received PING_REQUEST");
#endif
        output->msgCode = PING_RESPONSE;
    }
    else
    {
#ifdef CONFIG_DEBUG_IPC
        icLogDebug(API_LOG_CAT, "forwarding message code %d to handler", input->msgCode);
#endif
        // forward to the handler and save handler return as response->msgCode
        //
        if (wargs->handler) {
            output->msgCode = wargs->handler(input, output);
            if (output->msgCode == IPC_SHUT_DOWN)
            {
                shutdownSelf = true;
                output->msgCode = IPC_SUCCESS;
            }
        }
    }

    // send response, using rc as the msgCode (even if error).  note that
    // the send will timeout if the socket is not ready within 5 seconds.
    //
    int32_t msgCode = input->msgCode;
    if ((rc = transport_sendmsg(wargs->sockfd, &wargs->control, output, 5)) != IPC_SUCCESS)
    {
        // error sending, just fall through so we can cleanup
        //
        icLogWarn(API_LOG_CAT, "unable to send response to incoming request msgCode=%d, reason=%d", msgCode, rc);
    }

    // Check if its time for us to shutdown
    if (shutdownSelf == true)
    {
        shutdownRequestHandler(wargs->receiver);
    }

    // clean up the output
    //
    freeIPCMessage(output);
}

static void *mainLoop(void *ipcRecieverPtr)
{
    ipcReceiver* receiver = (ipcReceiver*) ipcRecieverPtr;

    acquireReceiver(receiver);
    icLogInfo(API_LOG_CAT, "starting IPCReceiver mainLoop for port %"PRIu16,
              receiver->servicePortNum);

    // loop until told to shutdown
    //
    while (getState(receiver) == IPC_REC_RUNNING && receiver->serverSock >= 0) {
        int ret;
        IPCMessage *input = NULL;
        transport_control_t control;
        memset(&control, 0, sizeof(transport_control_t));

        ret = transport_recvmsg(receiver->serverSock, &control, &input, 60);
        switch (ret) {
            case IPC_SUCCESS: {
                // stuff information about the request into a container, so
                // it can be handed to the thread pool for processing.
                // pass in the socket and handler for the thread to use.
                //
                ipcWorkerArgs *args = (ipcWorkerArgs*) malloc(sizeof(ipcWorkerArgs));

                args->receiver = receiver;
                args->sockfd = receiver->serverSock;
                args->handler = receiver->handler;
                args->input = input;
                args->control.control_block = control.control_block;

#ifdef CONFIG_DEBUG_IPC
                icLogDebug(API_LOG_CAT, "adding message request to task queue for port %" PRIu16, receiver->servicePortNum);
#endif

                if(threadPoolAddTask(receiver->threadPool, requestWorkerTask, args, requestWorkTaskArgFreeFunc) == false)
                {
                    icLogError(API_LOG_CAT, "Unable to add worker task to thread pool! msgCode=[%"PRId32"]", input->msgCode);
                }

                break;
            }

            case IPC_TIMEOUT:
                break;

            case IPC_SERVICE_DISABLED:
                // at this point all of the 'receiver sockets' should be closed.
                // we'll now rely on 'shutdownRequestHandler()' to clean the rest
                setState(receiver, IPC_REC_SHUTDOWN);
                break;

            default:
                icLogWarn(API_LOG_CAT, "error handling request: %s", IPCCodeLabels[ret]);
        }
    }

    // NOTE: assuming something else will cleanup the IPCReceiver object
    //
    icLogInfo(API_LOG_CAT, "exiting IPCReceiver mainLoop for port %" PRIu16, receiver->servicePortNum);

    // set our state to DEAD so any threads waiting on us can react
    //
    setState(receiver, IPC_REC_DEAD);

    releaseReceiver(receiver);

    pthread_exit(NULL);
    return NULL;
}

//TODO: C11 atomics will make this lockless - blocked by flex toolchain (GCC >= 4.9 required)
static void acquireReceiver(ipcReceiver *receiver)
{
    pthread_mutex_lock(&receiver->ipcMtx);
    if (receiver->refcnt < UINT8_MAX)
    {
        receiver->refcnt++;
    }
    else
    {
        icLogWarn(API_LOG_CAT, "IPCReceiver refcnt is saturated, acquisition not tracked");
    }
    pthread_mutex_unlock(&receiver->ipcMtx);
}

static void releaseReceiver(ipcReceiver *receiver)
{
    pthread_mutex_lock(&receiver->ipcMtx);
    if(receiver->refcnt > 0)
    {
        receiver->refcnt--;

        // Inform all threads they should check refcnt
        pthread_cond_broadcast(&receiver->ipcCond);
    }
    else
    {
        icLogWarn(API_LOG_CAT, "attempted to decrement IPCReceiver refcnt from 0");
    }

    pthread_mutex_unlock(&receiver->ipcMtx);
}

static void requestWorkTaskArgFreeFunc(void *arg)
{
    ipcWorkerArgs *wargs = (ipcWorkerArgs *) arg;

    if (wargs != NULL)
    {
        // close the socket and free containers
        //
        freeIPCMessage(wargs->input);
        transport_abortmsg(&wargs->control);
        free(wargs);
    }

}
