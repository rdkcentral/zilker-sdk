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

//
// Created by Boyd, Weston on 3/26/18.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pipeline.h>

#include <icLog/logging.h>
#include <icIpc/ipcMessage.h>

#include "../ipcCommon.h"
#include "transport.h"

#define USE_DEVLOOP_REQREP 0
#define USE_STANDARD_PUBSUB 0
#define USE_PIPED_PUBSUB 1


/* We have to use the "tcp:" URI
 * due to limitations with nanomsg's
 * reqrep pattern. Their pattern does not allow
 * for an "ipc:" to actively be receiving while
 * handling a request and wanting to send.
 *
 * Example:
 * t0: recv(<message>)
 * t1: spawn thread with input data
 * t2: recv(<message>)
 * t3: thread handles data
 * t4: thread calls send(<message>)
 *
 * This example will cause an error due to invalid
 * state.
 *
 * If we use the "tcp:" URI *and* set AF_RAW_SP then
 * we can manage the state ourselves. Thus we can handle
 * the above example.
 */
#define IPC_HOSTNAME_TEMPLATE "tcp://%s:%u"

/* The original protocol was required to perform its own packet
 * framing. Thus it would send the message code
 * separate from the payload data (JSON).
 *
 * However, nanomsg wants to be more transactional and does
 * not allow sending data in piece-meal. Thus to make
 * things easier the message code is separated from
 * the framing and placed within the JSON.
 *
 * TODO: The best solution is to change the code generator so that it
 * includes the message code directly into the JSON.
 */
#define MSGCODE_JSON_KEY "_autogen_msgcode"

#define PUBSUB_MEM_DEFAULT_SIZE 1024
#define PUBSUB_MEM_STEPSIZE 1024

#if USE_STANDARD_PUBSUB
#define PUBSUB_CHANNEL_TEMPLATE "ipc:///tmp/%s-pubsub.ipc"
#define PUBSUB_DEFAULT_CHANNEL "zilker"
#elif USE_DEVLOOP_REQREP
#define PUBSUB_CHANNEL_TEMPLATE "tcp://%s:22222"
#define PUBSUB_DEFAULT_CHANNEL "127.0.0.1"

#define PUBSUB_DEFAULT_PUBTEST "tcp://127.0.0.1:22101"
#define PUBSUB_DEFAULT_SUBTEST "tcp://127.0.0.1:22100"
#elif USE_PIPED_PUBSUB
#define PUBSUB_PUBLISHER "ipc:///tmp/zilker-publisher.ipc"
#define PUBSUB_SUBSCRIBER "ipc:///tmp/zilker-subscriber.ipc"
#else
#error No PubSub implementation specified.
#endif

/* nanomsg uses zero copy buffers to allocate
 * its data.
 */
void* transport_malloc_msg(size_t size)
{
    return nn_allocmsg(size, 0);
}

void* transport_realloc_msg(void* ptr, size_t size)
{
    return nn_reallocmsg(ptr, size);
}

/* nanomsg uses zero copy buffers to allocate
 * its data so we must free these specially.
 */
void transport_free_msg(void* ptr)
{
    if (ptr) nn_freemsg(ptr);
}

int transport_connect(const char* host, uint16_t servicePortNum)
{
    int sockfd;
    char ipc_name[255];

    /* Use a "normal" AF_SP socket. This is for clients. */
    if ((sockfd = nn_socket(AF_SP, NN_REQ)) < 0) {
        icLogError(API_LOG_CAT, "error opening socket on servicePort %"PRIu16": %s", servicePortNum, nn_strerror(errno));
        return -1;
    }

    sprintf(ipc_name, IPC_HOSTNAME_TEMPLATE, host, servicePortNum);
    icLogDebug(API_LOG_CAT, "Connect to [%s]", ipc_name);

    if (nn_connect(sockfd, ipc_name) < 0) {
        icLogWarn(API_LOG_CAT, "error connecting to socket on servicePort %d: %s", servicePortNum, nn_strerror(errno));
        nn_close(sockfd);
        return -1;
    }

    return sockfd;
}

int transport_establish(const char* host, uint16_t servicePortNum)
{
    int sockfd;
    char ipc_name[255];

    /* THe response handlers must use the "raw" socket to be asynchronous. */
    if ((sockfd = nn_socket(AF_SP_RAW, NN_REP)) < 0) {
        icLogWarn(API_LOG_CAT, "unable to create listening socket: %s", nn_strerror(errno));
        return -1;
    }

    sprintf(ipc_name, IPC_HOSTNAME_TEMPLATE, host, servicePortNum);
    icLogDebug(API_LOG_CAT, "Establish connection on [%s]", ipc_name);

    if (nn_bind(sockfd, ipc_name) < 0) {
        icLogWarn(API_LOG_CAT, "Unable to bind to listening port: %s", nn_strerror(errno));
        nn_close(sockfd);
        return -1;
    }

    return sockfd;
}

#if USE_DEVLOOP_REQREP
#include <pthread.h>
#include <unistd.h>

static int devloop_pub = -1;
static int devloop_sub = -1;

static void* devloop_thread(void* args)
{
    printf("Startup devloop!\n");
    devloop_pub = nn_socket(AF_SP_RAW, NN_REQ);
    if (nn_bind(devloop_pub, PUBSUB_DEFAULT_PUBTEST) < 0) {
        printf("failed bind pub: %s\n", nn_strerror(errno));
    }

    devloop_sub = nn_socket(AF_SP_RAW, NN_REP);
    if (nn_bind(devloop_sub, PUBSUB_DEFAULT_SUBTEST) < 0) {
        printf("failed bind sub: %s\n", nn_strerror(errno));
    }

    if (nn_device(devloop_sub, devloop_pub) != 0) {
        printf("failed making loop: %s\n", nn_strerror(errno));
    }

    printf("Exiting devloop thread!\n");
    return NULL;
}

int transport_pub_register(const char* channel)
{
    char ipc_name[255];
    const char* __channel;
    int sockfd;

#if 1
    if (devloop_pub == -1) {
        pthread_t mythread;
        pthread_create(&mythread, NULL, devloop_thread, NULL);
        sleep(2);
    }
#endif

    __channel = (channel == NULL || strlen(channel) == 0) ? PUBSUB_DEFAULT_CHANNEL : channel;

    sprintf(ipc_name, PUBSUB_CHANNEL_TEMPLATE, __channel);

    if ((sockfd = nn_socket(AF_SP, NN_REQ)) < 0) {
        icLogError(API_LOG_CAT, "error creating pub channel [%s]\n", ipc_name);
        return -1;
    }

    if (nn_connect(sockfd, PUBSUB_DEFAULT_SUBTEST) < 0) {
        icLogError(API_LOG_CAT, "error binding pub channel [%s]\n", ipc_name);
        nn_close(sockfd);
        return -1;
    }

    return sockfd;
}

int transport_sub_register(const char* channel)
{
    char ipc_name[255];
    const char* __channel;
    int sockfd;

    __channel = (channel == NULL || strlen(channel) == 0) ? PUBSUB_DEFAULT_CHANNEL : channel;

    sprintf(ipc_name, PUBSUB_CHANNEL_TEMPLATE, __channel);

    if ((sockfd = nn_socket(AF_SP, NN_REP)) < 0) {
        icLogError(API_LOG_CAT, "error creating sub [%s]\n", ipc_name);
        return -1;
    }

    if (nn_connect(sockfd, PUBSUB_DEFAULT_PUBTEST) < 0) {
        icLogError(API_LOG_CAT, "error connecting sub channel [%s]\n", ipc_name);
        nn_close(sockfd);
        return -1;
    }

    return sockfd;
}
#elif USE_STANDARD_PUBSUB
int transport_pub_register(const char* channel)
{
    char ipc_name[255];
    const char* __channel;
    int sockfd;

    __channel = (channel == NULL || strlen(channel) == 0) ? PUBSUB_DEFAULT_CHANNEL : channel;

    sprintf(ipc_name, PUBSUB_CHANNEL_TEMPLATE, __channel);

    if ((sockfd = nn_socket(AF_SP, NN_PUB)) < 0) {
        icLogError(API_LOG_CAT, "error creating pub channel [%s]\n", ipc_name);
        return -1;
    }

    if (nn_bind(sockfd, ipc_name) < 0) {
        icLogError(API_LOG_CAT, "error binding pub channel [%s]\n", ipc_name);
        nn_close(sockfd);
        return -1;
    }

    return sockfd;
}

int transport_sub_register(const char* channel)
{
    char ipc_name[255];
    const char* __channel;
    int sockfd;

    __channel = (channel == NULL || strlen(channel) == 0) ? PUBSUB_DEFAULT_CHANNEL : channel;

    sprintf(ipc_name, PUBSUB_CHANNEL_TEMPLATE, __channel);

    if ((sockfd = nn_socket(AF_SP, NN_SUB)) < 0) {
        icLogError(API_LOG_CAT, "error creating sub [%s]\n", ipc_name);
        return -1;
    }

    transport_subscribe(sockfd, TRANSPORT_SUBSCRIBE_ALL);

    if (nn_connect(sockfd, ipc_name) < 0) {
        icLogError(API_LOG_CAT, "error connecting sub channel [%s]\n", ipc_name);
        printf("errno: %s\n", nn_strerror(errno));
        nn_close(sockfd);
        return -1;
    }

    return sockfd;
}
#elif USE_PIPED_PUBSUB
int transport_pub_register(const char* channel)
{
    int sockfd;

    if ((sockfd = nn_socket(AF_SP, NN_PUSH)) < 0) {
        icLogError(API_LOG_CAT, "error creating pub channel [%s]\n", PUBSUB_PUBLISHER);
        return -1;
    }

    if (nn_connect(sockfd, PUBSUB_PUBLISHER) < 0) {
        icLogError(API_LOG_CAT, "error connecting pub channel [%s]\n", PUBSUB_PUBLISHER);
        nn_close(sockfd);
        return -1;
    }

    return sockfd;
}

int transport_sub_register(const char* channel)
{
    int sockfd;

    if ((sockfd = nn_socket(AF_SP, NN_SUB)) < 0) {
        icLogError(API_LOG_CAT, "error creating sub [%s]\n", PUBSUB_SUBSCRIBER);
        return -1;
    }

    if (nn_connect(sockfd, PUBSUB_SUBSCRIBER) < 0) {
        icLogError(API_LOG_CAT, "error connecting sub channel [%s]\n", PUBSUB_SUBSCRIBER);
        printf("errno: %s\n", nn_strerror(errno));
        nn_close(sockfd);
        return -1;
    }

    return sockfd;
}
#endif

int transport_subscribe(int sockfd, int id)
{
    char topic[6];

    if (id == TRANSPORT_SUBSCRIBE_ALL) {
        topic[0] = '\0';
    } else {
        sprintf(topic, "%05d", id);
    }

    if (nn_setsockopt(sockfd, NN_SUB, NN_SUB_SUBSCRIBE, topic, 0) < 0) {
        icLogError(API_LOG_CAT, "error subscribing to topic [%d]\n", id);
        return -1;
    }

    return 0;
}

void transport_publish(int sockfd, const cJSON* event)
{
    void* buffer = nn_allocmsg(PUBSUB_MEM_DEFAULT_SIZE, 0);
    size_t cursize = PUBSUB_MEM_STEPSIZE;
    bool json_generated = false;

    while (buffer && !json_generated) {
        if (cJSON_PrintPreallocated((cJSON*) event, buffer, (const int) cursize, 0)) {
            json_generated = true;
        } else {
            cursize += PUBSUB_MEM_STEPSIZE;
            buffer = nn_reallocmsg(buffer, cursize);
        }
    }

    if (buffer) {
        /* If we specify that the buffer is NN_MSG then
         * nanomsg will manage the buffer itself.
         */
        if (nn_send(sockfd, &buffer, NN_MSG, 0) < 0) {
            icLogError(API_LOG_CAT, "errno publishing event %s\n", nn_strerror(errno));
        }
    }
}

int transport_sub_recv(int sockfd, cJSON** event)
{
    char* buffer = NULL;

    if (nn_recv(sockfd, &buffer, NN_MSG, 0) < 0) {
        if (errno != EBADF) {
            icLogWarn(API_LOG_CAT, "error receiving on subscription.");
        }

        return -1;
    }

    *event = cJSON_Parse(buffer);

    nn_freemsg(buffer);

    if (*event == NULL) {
        icLogError(API_LOG_CAT, "error parsing incoming subscription event.");
        return -1;
    }

    return 0;
}

void transport_close(int sockfd)
{
    /* A nanomsg socket file descriptor may be '0' */
    if (sockfd >= 0) {
        nn_close(sockfd);
    }
}

void transport_abortmsg(transport_control_t *control)
{
    /* Stub! tell the requester their message can't be processed */
}

IPCCode transport_send(int sockfd, const IPCMessage* msg, time_t timeoutSecs)
{
    return transport_sendmsg(sockfd, NULL, msg, timeoutSecs);
}

IPCCode transport_sendmsg(int sockfd, transport_control_t* control, const IPCMessage* msg, time_t timeoutSecs)
{
    cJSON *jroot;
    char* json;
    IPCCode ret = IPC_SUCCESS;

    struct nn_msghdr header;
    struct nn_iovec iov;

    /* We *must* have a payload for the message code. Thus generate one
     * if the message does not contain one.
     */
    if (msg->payload) {
        jroot = cJSON_Parse((const char*) msg->payload);
    } else {
        jroot = cJSON_CreateObject();
    }

    if (cJSON_HasObjectItem(jroot, MSGCODE_JSON_KEY)) {
        cJSON* jsonMsgCode = cJSON_GetObjectItem(jroot, MSGCODE_JSON_KEY);
        cJSON_SetNumberValue(jsonMsgCode, msg->msgCode);
    } else {
        cJSON_AddNumberToObject(jroot, MSGCODE_JSON_KEY, msg->msgCode);
    }

    json = cJSON_PrintUnformatted(jroot);

    /* Setup the nanomsg base headers pointing to the JSON data buffer. */
    iov.iov_base = json;
    iov.iov_len = strlen(json) + 1;

    /* THere is only one buffer since we use the JSON buffer (malloc) directly.
     * This can be optimized if we only allocate with nanomsg zero-copy buffers.
     */
    header.msg_iov = &iov;
    header.msg_iovlen = 1;

    if (control) {
        /* This message is a response to a request. The response needs to be
         * "associated" with that request. Thus we reuse the original requesters
         * control block.
         *
         * The use of NN_MSG will actually tell nanomsg to determine for itself
         * how many entries there are. There is no other way for "us" to get
         * that information as it was not passed back in the recv.
         */
        header.msg_control = &control->control_block;
        header.msg_controllen = NN_MSG;
    } else {
        /* Tell nanomsg to keep its control block to itself. */
        header.msg_control = NULL;
        header.msg_controllen = 0;
    }

    if (nn_sendmsg(sockfd, &header, 0) < 0) {
        icLogWarn(API_LOG_CAT, "send [%s]: failed to send message code %"PRIi32,
                  nn_strerror(errno), msg->msgCode);
        ret = IPC_SEND_ERROR;
    }

    cJSON_Delete(jroot);
    free(json);

    return ret;
}

IPCCode transport_recv(int sockfd, IPCMessage** msg, time_t timeoutSecs)
{
    return transport_recvmsg(sockfd, NULL, msg, timeoutSecs);
}

IPCCode transport_recvmsg(int sockfd, transport_control_t* control, IPCMessage** msg, long timeoutSecs)
{
    cJSON *jroot, *jitem;
    char* buffer = NULL;
    IPCCode ret = IPC_SUCCESS;

    struct nn_msghdr header;
    struct nn_iovec iov;

    /* We want nanomsg to generate the buffer for us using zero-copy buffers. */
    iov.iov_base = &buffer;
    iov.iov_len = NN_MSG;

    header.msg_iov = &iov;
    header.msg_iovlen = 1;

    if (control) {
        /* Tell nanomsg that we want the control block back. We do this
         * so that a response can be associated (on another thread) with the
         * request.
         */
        control->control_block = NULL;

        header.msg_control = &control->control_block;
        header.msg_controllen = NN_MSG;
    } else {
        header.msg_control = NULL;
        header.msg_controllen = 0;
    }

    if (nn_recvmsg(sockfd, &header, 0) <= 0) {
        if (errno == EBADF) {
            /* Someone closed the socket. Most likely due to
             * shutting down the request handler.
             */
            return IPC_SERVICE_DISABLED;
        } else {
            icLogWarn(API_LOG_CAT, "read: failed to read message payload; error=%s", nn_strerror(errno));
            return IPC_READ_ERROR;
        }
    }

    jroot = cJSON_Parse(buffer);
    jitem = cJSON_GetObjectItem(jroot, MSGCODE_JSON_KEY);

    if (cJSON_IsNumber(jitem)) {
        IPCMessage* __msg = (*msg == NULL) ? createIPCMessage() : *msg;

        __msg->msgCode = jitem->valueint;
        __msg->payloadLen = (uint32_t) strlen(buffer);
        __msg->payload = buffer;

        *msg = __msg;
    } else {
        printf("wtf? [%s]\n", buffer);
        ret = IPC_READ_ERROR;
    }

    cJSON_Delete(jroot);

    return ret;
}
