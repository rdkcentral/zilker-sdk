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
// Created by wboyd747 on 6/11/18.
//

#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include <icConcurrent/icBlockingQueue.h>
#include <malloc.h>
//#include <littlesheens/machines.h>
#include <errno.h>
#include <engines/sheens.h>
#include <jsonrpc/jsonrpc.h>
#include <icIpc/baseEvent.h>
#include <icTime/timeUtils.h>
#include <icConcurrent/threadUtils.h>
#include <icUtil/stringUtils.h>

#include "automationEngine.h"
#include "automationService.h"
#include "automationSunTime.h"

static icBlockingQueue* messageQueue;
static icLinkedList* engineList;

static pthread_t messageThread;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t engineMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

static bool running = false;
static bool initialized = false;

#define SLOW_PROCESS_THRESHOLD_MS 1000U

/**
 * Safely get the running status.
 *
 * @return Current message thread running status.
 */
static bool isRunning(void)
{
    bool ret;

    pthread_mutex_lock(&mutex);
    ret = running;
    pthread_mutex_unlock(&mutex);

    return ret;
}

/**
 * Stop the Engine factory message thread.
 *
 * This is code duplication saving routine as both 'destroy'
 * and 'stop' will call it.
 *
 * Warning: Must be called with mutex held.
 */
static void stopEngineThread(void)
{
    if (running) {
        running = false;

        // This will force the engine thread to wakeup and terminate
        blockingQueueDisable(messageQueue);
        pthread_cond_wait(&condition, &mutex);
    }
}

static void freeEngineList(void* value)
{
    automationEngineOps* ops = value;

    if (ops->destroy) {
        ops->destroy();
    }
}

static void injectRequiredJSON(cJSON* json)
{
    time_t sunrise, sunset;

    automationGetSunTimes(&sunrise, &sunset);

    if (jsonrpc_is_valid(json)) {
        cJSON* params = cJSON_GetObjectItem(json, "params");

        if (params == NULL) {
            params = cJSON_CreateObject();
            cJSON_AddItemToObjectCS(json, "params", params);
        }

        json = params;
    }

    // sunrise/sunset get stored as time_t, but all event times we use are millis, so do the conversion
    uint64_t sunriseMillis = ((uint64_t)sunrise)*1000UL;
    uint64_t sunsetMillis = ((uint64_t)sunset)*1000UL;
    // Make sure nothing changes the settings underneath us.
    cJSON_AddItemToObjectCS(json, "_sunrise", cJSON_CreateNumber(sunriseMillis));
    cJSON_AddItemToObjectCS(json, "_sunset", cJSON_CreateNumber(sunsetMillis));

    if (!cJSON_HasObjectItem(json, EVENT_TIME_JSON_KEY)) {
        uint64_t now = getCurrentUnixTimeMillis();

        cJSON_AddItemToObjectCS(json, EVENT_TIME_JSON_KEY, cJSON_CreateNumber(now));
    }
}

static void* engineMessageThread(void* args)
{
    unused(args);

    while (isRunning()) {
        cJSON* message;
        int errorCode;
        icLinkedListIterator* iterator;

        /* We don't need to hold the mutex here as the thread will be shutdown
         * before the message queue is.
         */
        message = blockingQueuePopTimeout(messageQueue, 5);
        errorCode = errno;

        if (message == NULL) {
            /* Fail on all real errors. If a timeout occurs just
             * go back over the loop.
             */
            if (errorCode == ETIMEDOUT) {
                continue;
            } else {
                icLogError(LOG_TAG, "Error getting next message from queue. [%s]", strerror(errorCode));
                break;
            }
        }

        if (!isRunning()) {
            cJSON_Delete(message);

            break;
        }

        /*
         * Messages may have a useful name in an array/object key (e.g., when messages are events)
         */
        const char *msgName = NULL;
        const cJSON *element = NULL;
        cJSON_ArrayForEach(element, message)
        {
            if (cJSON_IsObject(element))
            {
                msgName = element->string;
                break;
            }
        }

        pthread_mutex_lock(&engineMutex);

        uint64_t start = 0;
        uint64_t elapsedMs = 0;
        struct timespec tmp;
        dbg(VERBOSITY_LEVEL_1, "Processing message %s", stringCoalesceAlt(msgName, ""));
        getCurrentTime(&tmp, true);
        start = convertTimespecToUnixTimeMillis(&tmp);

        iterator = linkedListIteratorCreate(engineList);
        while (linkedListIteratorHasNext(iterator)) {
            automationEngineOps* ops = linkedListIteratorGetNext(iterator);

            if (ops->process) {
                dbg(VERBOSITY_LEVEL_0, "Engine %s processing", ops->name);

                cJSON* actions = ops->process(message, NULL);
                if (actions) cJSON_Delete(actions);

                dbg(VERBOSITY_LEVEL_0, "Engine %s processing complete", ops->name);
            }
        }

        cJSON_Delete(message);
        linkedListIteratorDestroy(iterator);

        getCurrentTime(&tmp, true);
        elapsedMs = convertTimespecToUnixTimeMillis(&tmp) - start;

        if (elapsedMs < SLOW_PROCESS_THRESHOLD_MS)
        {
            dbg(VERBOSITY_LEVEL_1, "Processing completed in %"PRIu64 "ms", elapsedMs);
        }
        else
        {
            icLogWarn(LOG_TAG,
                      "Processing completed %"PRIu64 "ms (warning threshold is %ums). "
                          "Actions may be noticeably delayed.",
                      elapsedMs,
                      SLOW_PROCESS_THRESHOLD_MS);
        }

        pthread_mutex_unlock(&engineMutex);
    }

    pthread_mutex_lock(&mutex);
    running = false;
    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void automationEngineInit(void)
{
    pthread_mutex_lock(&mutex);
    if (!initialized) {
        messageQueue = blockingQueueCreate(BLOCKINGQUEUE_MAX_CAPACITY);
        engineList = linkedListCreate();

        sheensEngineInit();

        initialized = true;
    }
    pthread_mutex_unlock(&mutex);

}

static void destroyMessageQueueJSON(void *item)
{
    if (item != NULL)
    {
        cJSON_Delete((cJSON *)item);
    }
}

void automationEngineDestroy(void)
{
    pthread_mutex_lock(&mutex);
    if (initialized) {
        stopEngineThread();

        blockingQueueDestroy(messageQueue, destroyMessageQueueJSON);
        linkedListDestroy(engineList, freeEngineList);

        initialized = false;
    }
    pthread_mutex_unlock(&mutex);
}

void automationEngineRegister(automationEngineOps* ops)
{
    pthread_mutex_lock(&engineMutex);
    linkedListAppend(engineList, ops);
    pthread_mutex_unlock(&engineMutex);
}

void automationEngineStart(void)
{
    pthread_mutex_lock(&mutex);
    if (initialized && !running) {
        running = true;

        blockingQueueClear(messageQueue, destroyMessageQueueJSON);
        createThread(&messageThread, engineMessageThread, NULL, "engMsgHandler");
    }
    pthread_mutex_unlock(&mutex);
}

void automationEngineStop(void)
{
    pthread_mutex_lock(&mutex);
    stopEngineThread();
    pthread_mutex_unlock(&mutex);
}

bool automationEngineEnable(const char* specId, const char* specification)
{
    icLinkedListIterator* iterator;
    bool ret = false;

    pthread_mutex_lock(&engineMutex);
    iterator = linkedListIteratorCreate(engineList);
    while (linkedListIteratorHasNext(iterator)) {
        automationEngineOps* ops = linkedListIteratorGetNext(iterator);

        if (ops->enable) {
            ret |= ops->enable(specId, specification);
        }
    }
    linkedListIteratorDestroy(iterator);
    pthread_mutex_unlock(&engineMutex);

    return ret;
}

void automationEngineDisable(const char* specId)
{
    icLinkedListIterator* iterator;

    pthread_mutex_lock(&engineMutex);
    iterator = linkedListIteratorCreate(engineList);
    while (linkedListIteratorHasNext(iterator)) {
        automationEngineOps* ops = linkedListIteratorGetNext(iterator);

        if (ops->disable) {
            ops->disable(specId);
        }
    }
    linkedListIteratorDestroy(iterator);
    pthread_mutex_unlock(&engineMutex);
}

cJSON* automationEngineGetState(const char* specId)
{
    icLinkedListIterator* iterator;
    cJSON* ret = NULL;

    pthread_mutex_lock(&engineMutex);
    iterator = linkedListIteratorCreate(engineList);
    while ((ret == NULL) && linkedListIteratorHasNext(iterator)) {
        automationEngineOps* ops = linkedListIteratorGetNext(iterator);

        if (ops->get_state) {
            ret = ops->get_state(specId);
        }
    }
    linkedListIteratorDestroy(iterator);
    pthread_mutex_unlock(&engineMutex);

    return ret;
}

bool automationEnginePost(const cJSON* message)
{
    bool ret = false;

    if (message && isRunning()) {
        cJSON* msg = cJSON_Duplicate(message, true);

        injectRequiredJSON(msg);

        ret = blockingQueuePush(messageQueue, msg);
        if (ret == false)
        {
            icLogWarn(LOG_TAG, "Failed to enqueue automation event");
            // Probably shutting down, cleanup.
            cJSON_Delete(msg);
        }
    }

    return ret;
}
