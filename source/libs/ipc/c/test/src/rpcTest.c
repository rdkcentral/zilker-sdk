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
 * rpcTest.c
 *
 * Simple test to see that the service-side of
 * libicIpc is properly functioning.  Should
 * be run against both C and Java test clients
 * (see parent directory)
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icLog/logging.h>
#include <icIpc/ipcMessage.h>
#include <icIpc/ipcReceiver.h>
#include <zconf.h>
#include <pthread.h>
#include <icIpc/ipcSender.h>
#include <assert.h>
#include <icConcurrent/threadUtils.h>

#define SERVICE_PORT    15000
#define REQUEST_MSGCODE 100

//#define SINGLE_TEST     1

/*
 * handle incoming requests
 */
static IPCCode requestHandler(IPCMessage *request, IPCMessage *response)
{
    if (request->msgCode != REQUEST_MSGCODE) {
        printf("Received invalid request code.\n");
        return IPC_INVALID_ERROR;
    }

    if (request->payloadLen <= 0) {
        printf("Invalid payload length received: [%d]\n", request->payloadLen);
        return IPC_INVALID_ERROR;
    }

    populateIPCMessageWithJSON(response, request->payload);

    return IPC_SUCCESS;
}

static void* service_thread_handler(void* args)
{
    // start our receiver, should block until something kills it
    //
    waitForRequestHandlerToShutdown(args);
    pthread_exit(NULL);
    return NULL;
}

static void* sendRequestTest(void* args)
{
    IPCMessage	*input = createIPCMessage();
    IPCMessage  *output = createIPCMessage();

    // build up a simple test of a string message to make sure
    // all of the library is functioning properly
    //

    input->msgCode = REQUEST_MSGCODE;

    populateIPCMessageWithJSON(input, "{\"test\": \"this is a string test\"}");

    // send the message request
    //
    IPCCode rc = sendServiceRequest(SERVICE_PORT, input, output);

    if (rc != IPC_SUCCESS) {
        printf("Send service request failed [%s].\n", IPCCodeLabels[rc]);
        freeIPCMessage(input);
        freeIPCMessage(output);
        return (void*) -1;
    }

    if (output->msgCode != IPC_SUCCESS) {
        printf("Request failed to be handled properly.\n");
        freeIPCMessage(input);
        freeIPCMessage(output);
        return (void*) -1;
    }

    if (output->payloadLen == 0) {
        printf("Invalid payload length\n");
        freeIPCMessage(input);
        freeIPCMessage(output);
        return (void*) -1;
    }

    cJSON* json = cJSON_Parse(output->payload);

    // Clean up
    freeIPCMessage(input);
    freeIPCMessage(output);

    if (json == NULL) {
        printf("Json not present.\n");
        return (void*) -1;
    }

    cJSON* json_item = cJSON_GetObjectItem(json, "test");
    if (json_item == NULL) {
        printf("Json test item not present.\n");
        cJSON_Delete(json);
        return (void*) -1;
    }

    if (strcmp("this is a string test", json_item->valuestring) != 0) {
        printf("Returned value does not match test [%s]\n", json_item->valuestring);
        cJSON_Delete(json);
        return (void*) -1;
    }

    cJSON_Delete(json);

    return NULL;
}

int rpcTest_main(int argc, const char *argv[])
{
    pthread_t service_thread;
    pthread_t client_thread[5];

    IpcReceiver recv;

    int i;

    initIcLogger();

    recv = startRequestHandler(SERVICE_PORT, requestHandler, SERVICE_VISIBLE_LOCAL_HOST, 2, 5, 5, NULL);

    createThread(&service_thread, service_thread_handler, recv, NULL);

    if (!waitForServiceAvailable(SERVICE_PORT, 10)) {
        printf("WARN: Service never became available.\n");
        closeIcLogger();
        return EXIT_FAILURE;
    }

#ifdef SINGLE_TEST
    printf("Create 'sendRequest' test\n");
    if (sendRequestTest(NULL) != 0) {
        printf("Send request test failed.");
        return EXIT_FAILURE;
    }
#else
    printf("Create 'sendRequest' test threads.\n");
    for (i = 0; i < 5; i++) {
        pthread_create(&client_thread[i], NULL, sendRequestTest, NULL);
    }

    for (i = 0; i < 5; i++) {
        int ret;

        printf("Waiting for 'sendRequest' test thread %d to complete.\n", (i+1));
        pthread_join(client_thread[i], (void*)&ret);

        if (ret != 0) {
            return EXIT_FAILURE;
        }
    }
#endif

    printf("Shutting down Service...\n");
    shutdownRequestHandler(recv);

    printf("Waiting for thread to complete...\n");
    pthread_join(service_thread, NULL);
    closeIcLogger();

    return EXIT_SUCCESS;
}


