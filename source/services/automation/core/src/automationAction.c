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
// Created by tlea on 4/16/18.
//

#include <string.h>
#include <pthread.h>

#include <icLog/logging.h>
#include <icTypes/icHashMap.h>
#include <icConcurrent/threadPool.h>

#include "automationService.h"
#include "automationAction.h"

#include "actions/camera.h"
#include "actions/test.h"
#include "actions/devices.h"
#include "actions/internalIpc.h"
#include "actions/timer.h"
#include "actions/notifications.h"
#include "automationEngine.h"

static icHashMap* targetMap;

static pthread_mutex_t targetsMtx = PTHREAD_MUTEX_INITIALIZER;

static icThreadPool* threadPool;

static jsonrpc_t rpc;

static bool initialized = false;

/**
 * Hash map free routine that does nothing.
 *
 * @param key unused
 * @param value unused
 */
static void targetsFreeFunc(void* key, void* value)
{
    unused(key);
    unused(value);
}

/**
 * Main emit message handler thread for a single machine.
 *
 * This thread belongs to the sub-system thread pool and should
 * be release as soon as possible.
 *
 * @param arg The machine object.
 */
static void actionTaskThread(void* arg)
{
    cJSON* action = arg;
    cJSON* response = NULL;

    icLogDebug(LOG_TAG, "Handling action [%s]", jsonrpc_get_method(action));

    if (jsonrpc_execute(&rpc, action, &response) != 0) {
        icLogError(LOG_TAG, "%s: no handler registered for target", __func__);
    }

    if (response) {
        automationEnginePost(response);
    }
}

/**
 * Handle a single JSON <em>Object</em> that is an emit request.
 *
 * @param containerId The ID of the machine that is being processed.
 * @param json The JSON Object that represents the emit request.
 */
static bool handleActionMessage(cJSON* json)
{
    bool ret = false;

    if (jsonrpc_is_valid(json)) {
        if (threadPoolAddTask(threadPool, actionTaskThread, json, (threadPoolTaskArgFreeFunc)cJSON_Delete)) {
            ret = true;
        } else {
            icLogError(LOG_TAG, "Failed to add action message to thread pool.");
        }
    } else {
        icLogError(LOG_TAG, "Invalid action format specified.");
        cJSON_Delete(json);
    }

    return ret;
}

/**
 * Walk through each entry in an emit message JSON Array to determine
 * if it is a sub-array of emit messages, or a emit message JSON Object.
 *
 * @param containerId The ID of the machine that is being processed.
 * @param json A JSON Array contains either sub-arrays or emit JSON objects.
 */
static bool handleActionArray(cJSON* json)
{
    bool ret = false;

    while (cJSON_GetArraySize(json) > 0) {
        cJSON* entry = cJSON_DetachItemFromArray(json, 0);

        if (cJSON_IsArray(entry)) {
            ret = handleActionArray(entry);
        } else {
            ret = handleActionMessage(entry);
        }
    }

    cJSON_Delete(json);

    return ret;
}

//bool automationActionInit(automationActionResponseHandler callback)
bool automationActionInit(void)
{
    pthread_mutex_lock(&targetsMtx);
    if (!initialized) {
        jsonrpc_init(&rpc);

        targetMap = hashMapCreate();

        threadPool = threadPoolCreate("automationAction", 1, 10, MAX_QUEUE_SIZE);

        initialized = true;
    }
    pthread_mutex_unlock(&targetsMtx);

    testMessageTargetInit(); // TODO: Move this to the test case.
    ipcMessageTargetInit();
    deviceMessageTargetInit();
    timersMessageTargetInit();
    notificationMessageTargetInit();
    cameraMessageTargetInit();

    return true;
}

void automationActionDestroy(void)
{
    pthread_mutex_lock(&targetsMtx);
    if (initialized) {
        threadPoolDestroy(threadPool);

        hashMapDestroy(targetMap, targetsFreeFunc);

        jsonrpc_destroy(&rpc);

        initialized = false;
    }
    pthread_mutex_unlock(&targetsMtx);

    // Cleanup any that require cleanup
    timersMessageTargetDestroy();
    notificationMessageTargetDestroy();
}

void automationActionRegisterOps(const char* name, automationActionHandler_t handler)
{
    pthread_mutex_lock(&targetsMtx);
    if (initialized) {
        jsonrpc_register_method(&rpc, name, handler);
    }
    pthread_mutex_unlock(&targetsMtx);
}

bool automationActionPost(const char* id, cJSON* action)
{
    bool ret = false;

    if (unlikely(id == NULL) || unlikely(id[0] == '\0')) {
        icLogError(LOG_TAG, "Invalid container ID specified.");
        return false;
    }

    if (unlikely(action == NULL)) {
        icLogError(LOG_TAG, "Invalid actions specified.");
        return false;
    }

    pthread_mutex_lock(&targetsMtx);
    if (initialized) {
        if (cJSON_IsArray(action)) {
            ret = handleActionArray(action);
        } else {
            ret = handleActionMessage(action);
        }
    }
    pthread_mutex_unlock(&targetsMtx);

    return ret;
}
