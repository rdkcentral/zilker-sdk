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
// Created by Boyd, Weston on 3/27/18.
//

#include <stdint.h>
#include <cjson/cJSON.h>
#include <icIpc/eventProducer.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <icIpc/eventConsumer.h>
#include <icIpc/baseEvent.h>
#include <nanomsg/nn.h>
#include <nanomsg/pipeline.h>
#include <nanomsg/pubsub.h>
#include <zconf.h>
#include <pthread.h>

#define BASE_PRODUCER_ID 15
#define BASE_EVENT_ID 20
#define BASE_VALUE 30
#define BASE_TIME 40

static void suball_handler(int32_t eventCode, int32_t eventValue, cJSON* jsonPayload)
{
    printf("all handler: %d\n", eventCode);

    if ((eventCode < BASE_PRODUCER_ID) || (eventCode > (BASE_PRODUCER_ID + 4))) {
        printf("Failed to get eventCode %d\n", eventCode);
        exit(EXIT_FAILURE);
    }

    if ((eventValue < BASE_VALUE) || (eventValue > (BASE_VALUE + 4))) {
        printf("Failed to get eventValue %d\n", eventValue);
        exit(EXIT_FAILURE);
    }
}

static void sub15_handler(int32_t eventCode, int32_t eventValue, cJSON* jsonPayload)
{
    const int code = BASE_PRODUCER_ID;
    const int value = BASE_VALUE;

    if (eventCode != code) {
        printf("Failed to get eventCode %d\n", code);
        exit(EXIT_FAILURE);
    }

    if (eventValue != value) {
        printf("Failed to get eventValue %d\n", value);
        exit(EXIT_FAILURE);
    }
}

static void sub16_handler(int32_t eventCode, int32_t eventValue, cJSON* jsonPayload)
{
    const int code = BASE_PRODUCER_ID + 1;
    const int value = BASE_VALUE + 1;

    if (eventCode != code) {
        printf("Failed to get eventCode %d\n", code);
        exit(EXIT_FAILURE);
    }

    if (eventValue != value) {
        printf("Failed to get eventValue %d\n", value);
        exit(EXIT_FAILURE);
    }
}

static void sub17_handler(int32_t eventCode, int32_t eventValue, cJSON* jsonPayload)
{
    const int code = BASE_PRODUCER_ID + 2;
    const int value = BASE_VALUE + 2;

    if (eventCode != code) {
        printf("Failed to get eventCode %d\n", code);
        exit(EXIT_FAILURE);
    }

    if (eventValue != value) {
        printf("Failed to get eventValue %d\n", value);
        exit(EXIT_FAILURE);
    }
}

static void sub18_handler(int32_t eventCode, int32_t eventValue, cJSON* jsonPayload)
{
    const int code = BASE_PRODUCER_ID + 3;
    const int value = BASE_VALUE + 3;

    if (eventCode != code) {
        printf("Failed to get eventCode %d\n", code);
        exit(EXIT_FAILURE);
    }

    if (eventValue != value) {
        printf("Failed to get eventValue %d\n", value);
        exit(EXIT_FAILURE);
    }
}

static void sub19_handler(int32_t eventCode, int32_t eventValue, cJSON* jsonPayload)
{
    const int code = BASE_PRODUCER_ID + 4;
    const int value = BASE_VALUE + 4;

    if (eventCode != code) {
        printf("Failed to get eventCode %d\n", code);
        exit(EXIT_FAILURE);
    }

    if (eventValue != value) {
        printf("Failed to get eventValue %d\n", value);
        exit(EXIT_FAILURE);
    }
}

#define PUBSUB_PUBLISHER "ipc:///tmp/zilker-publisher.ipc"
#define PUBSUB_SUBSCRIBER "ipc:///tmp/zilker-subscriber.ipc"

static void* pipe2pub_thread(void* args)
{
    int sockfd_pipe;
    int sockfd_pub;

    sockfd_pipe = nn_socket(AF_SP, NN_PULL);
    sockfd_pub = nn_socket(AF_SP, NN_PUB);

    nn_bind(sockfd_pipe, PUBSUB_PUBLISHER);
    nn_bind(sockfd_pub, PUBSUB_SUBSCRIBER);

    while (1) {
        int bytes;

        char* buffer = NULL;
        char* control = NULL;

        struct nn_msghdr header;
        struct nn_iovec iov;

        iov.iov_base = &buffer;
        iov.iov_len = NN_MSG;

        /* THere is only one buffer since we use the JSON buffer (malloc) directly.
         * This can be optimized if we only allocate with nanomsg zero-copy buffers.
         */
        header.msg_iov = &iov;
        header.msg_iovlen = 1;
        header.msg_control = &control;
        header.msg_controllen = NN_MSG;

        bytes = nn_recvmsg(sockfd_pipe, &header, 0);
        if (bytes < 0) {
            printf("Failed receiving from publisher pipe. [%s]\n", nn_strerror(errno));
            exit(1);
        }

        if (bytes) {
            if (nn_sendmsg(sockfd_pub, &header, 0) < 0) {
                printf("Failed sending to subscribers. [%s]\n", nn_strerror(errno));
                exit(1);
            }
        }
    }

    return NULL;
}

int pubsubTest_main(int argc, const char *argv[])
{
    EventProducer producer[5];
    int i;

#if 1
    pthread_attr_t tattr;
    pthread_t event_thread;

    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    pthread_create(&event_thread, &tattr, pipe2pub_thread, NULL);
#endif

    for (i = 0; i < 5; i++) {
        producer[i] = initEventProducer((uint16_t) (BASE_PRODUCER_ID + i));
        if (producer[i] == NULL) {
            printf("Failed to initialize event producer.\n");
            return EXIT_FAILURE;
        }
    }

    startEventListener(EVENTCONSUMER_SUBSCRIBE_ALL, suball_handler);

    startEventListener(BASE_PRODUCER_ID + 0, sub15_handler);
    startEventListener(BASE_PRODUCER_ID + 1, sub16_handler);
    startEventListener(BASE_PRODUCER_ID + 2, sub17_handler);
    startEventListener(BASE_PRODUCER_ID + 3, sub18_handler);
    startEventListener(BASE_PRODUCER_ID + 4, sub19_handler);

    for (i = 0; i < 5; i++) {
        BaseEvent baseEvent = {
                (uint64_t) (BASE_EVENT_ID + i),
                BASE_PRODUCER_ID + i,
                BASE_VALUE + i,
                { BASE_TIME + i, 0 }
        };

        cJSON* json = baseEvent_toJSON(&baseEvent);
        broadcastEvent(producer[i], json);
        cJSON_Delete(json);
    }

    sleep(2);

    stopEventListener(BASE_PRODUCER_ID + 4);
    stopEventListener(BASE_PRODUCER_ID + 3);
    stopEventListener(BASE_PRODUCER_ID + 2);
    stopEventListener(BASE_PRODUCER_ID + 1);
    stopEventListener(BASE_PRODUCER_ID + 0);

    shutdownEventListener();

    for (i = 0; i < 5; i++) {
        shutdownEventProducer(producer[i]);
    }

    return EXIT_SUCCESS;
}
