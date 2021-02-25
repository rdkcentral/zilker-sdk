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
// Created by wboyd747 on 7/27/18.
//

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <icTypes/icHashMap.h>

#include <jsonrpc/jsonrpc.h>

#define JSONRPC_VERSION "2.0"

#define JSONRPC_KEY_JSONRPC "jsonrpc"
#define JSONRPC_KEY_ID "id"
#define JSONRPC_KEY_METHOD "method"
#define JSONRPC_KEY_PARAMS "params"
#define JSONRPC_KEY_RESULT "result"
#define JSONRPC_KEY_ERROR "error"
#define JSONRPC_KEY_CODE "code"
#define JSONRPC_KEY_MESSAGE "message"
#define JSONRPC_KEY_DATA "data"

#define HASHMAP_STRING_SIZEOF(x) ((uint16_t) strlen((x)))

#ifndef unused
#define unused(v) ((void) (v))
#endif

#ifndef unlikely
#if defined(__GNUC__) || defined(__clang__)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define unlikely(x) (x)
#endif
#endif

static void jsonrpc_method_map_delete(void* key, void* value)
{
    free(key);
    unused(value);
}

static cJSON* jsonrpc_create_message(const cJSON* id, const char* method, cJSON* params)
{
    cJSON* json;

    if (unlikely((method == NULL) || (method[0] == '\0'))) {
        errno = EINVAL;
        return NULL;
    }

    json = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(json, JSONRPC_KEY_JSONRPC, cJSON_CreateStringReference(JSONRPC_VERSION));
    cJSON_AddItemToObjectCS(json, JSONRPC_KEY_METHOD, cJSON_CreateString(method));

    if (id) {
        cJSON_AddItemToObjectCS(json, JSONRPC_KEY_ID, cJSON_Duplicate(id, true));
    }

    if (params) {
        cJSON_AddItemToObjectCS(json, JSONRPC_KEY_PARAMS, params);
    }

    return json;
}

cJSON* jsonrpc_create_notification(const char* method, cJSON* params)
{
    return jsonrpc_create_message(NULL, method, params);
}

cJSON* jsonrpc_create_request(const cJSON* id, const char* method, cJSON* params)
{
    if (unlikely(id == NULL)) {
        errno = EINVAL;
        return NULL;
    }

    return jsonrpc_create_message(id, method, params);
}

cJSON* jsonrpc_create_response_success(const cJSON* id, cJSON* result)
{
    cJSON* json;

    if (unlikely(id == NULL)) {
        errno = EINVAL;
        return NULL;
    }

    json = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(json, JSONRPC_KEY_JSONRPC, cJSON_CreateStringReference(JSONRPC_VERSION));
    cJSON_AddItemToObjectCS(json, JSONRPC_KEY_ID, cJSON_Duplicate(id, true));
    cJSON_AddItemToObjectCS(json, JSONRPC_KEY_RESULT, (result == NULL) ? cJSON_CreateObject() : result);

    return json;
}

cJSON* jsonrpc_create_response_error(const cJSON* id, int code, const char* message, cJSON* data)
{
    cJSON *json, *error_json;

    if (unlikely(id == NULL)) {
        errno = EINVAL;
        return NULL;
    }

    if (unlikely(message == NULL)) {
        errno = EINVAL;
        return NULL;
    }

    json = cJSON_CreateObject();
    error_json = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(error_json, JSONRPC_KEY_CODE, cJSON_CreateNumber(code));
    cJSON_AddItemToObjectCS(error_json, JSONRPC_KEY_MESSAGE, cJSON_CreateString(message));

    if (data) {
        cJSON_AddItemToObjectCS(error_json, JSONRPC_KEY_DATA, data);
    }

    cJSON_AddItemToObjectCS(json, JSONRPC_KEY_JSONRPC, cJSON_CreateStringReference(JSONRPC_VERSION));
    cJSON_AddItemToObjectCS(json, JSONRPC_KEY_ID, cJSON_Duplicate(id, true));
    cJSON_AddItemToObjectCS(json, JSONRPC_KEY_ERROR, error_json);

    return json;
}

bool jsonrpc_is_response_success(const cJSON* response)
{
    return (response != NULL) && (cJSON_GetObjectItem(response, "result") != NULL);
}

bool jsonrpc_is_response_error(const cJSON* response)
{
    return (response != NULL) && (cJSON_GetObjectItem(response, "error") != NULL);
}

int jsonrpc_get_response_success(const cJSON* response,
                                 const cJSON** id,
                                 const cJSON** result)
{
    if (unlikely(response == NULL)) return EINVAL;
    if (unlikely(id == NULL)) return EINVAL;

    if (!jsonrpc_is_response_success(response)) return EBADMSG;

    *id = cJSON_GetObjectItem(response, JSONRPC_KEY_ID);
    if (unlikely(*id == NULL)) return EBADMSG;

    if (result) {
        *result = cJSON_GetObjectItem(response, JSONRPC_KEY_RESULT);
    }

    return 0;
}

int jsonrpc_get_response_error(const cJSON* response,
                               const cJSON** id,
                               int* code,
                               const char** message,
                               const cJSON** data)
{
    const cJSON *error, *json;

    if (unlikely(response == NULL)) return EINVAL;
    if (unlikely(id == NULL)) return EINVAL;
    if (unlikely(code == NULL)) return EINVAL;

    if (!jsonrpc_is_response_error(response)) return EBADMSG;

    *id = cJSON_GetObjectItem(response, JSONRPC_KEY_ID);
    if (unlikely(*id == NULL)) return EBADMSG;

    error = cJSON_GetObjectItem(response, JSONRPC_KEY_ERROR);
    if (unlikely(error == NULL)) return EBADMSG;

    json = cJSON_GetObjectItem(error, JSONRPC_KEY_CODE);
    if (unlikely(json == NULL)) return EBADMSG;

    *code = json->valueint;

    if (message) {
        json = cJSON_GetObjectItem(error, JSONRPC_KEY_MESSAGE);
        if (json) {
            *message = json->valuestring;
        } else {
            *message = NULL;
        }
    }

    if (data) {
        *data = cJSON_GetObjectItem(error, JSONRPC_KEY_DATA);
    }

    return 0;
}

bool jsonrpc_is_valid(const cJSON* object)
{
    const cJSON* version;

    if (unlikely(object == NULL)) return false;

    version = cJSON_GetObjectItem(object, JSONRPC_KEY_JSONRPC);

    return (cJSON_IsString(version) &&
            (version->valuestring != NULL) &&
            (strcmp(version->valuestring, JSONRPC_VERSION) == 0));
}

const char* jsonrpc_get_method(const cJSON* request)
{
    const char* ret = NULL;

    if (jsonrpc_is_valid(request)) {
        cJSON* method;

        method = cJSON_GetObjectItem(request, JSONRPC_KEY_METHOD);
        if (method && cJSON_IsString(method)) {
            ret = method->valuestring;
        }
    }

    return ret;
}

int jsonrpc_init(jsonrpc_t* rpc)
{
    pthread_mutex_init(&rpc->mutex, NULL);

    rpc->request_map = hashMapCreate();

    return 0;
}

void jsonrpc_destroy(jsonrpc_t* rpc)
{
    pthread_mutex_lock(&rpc->mutex);
    hashMapDestroy(rpc->request_map, jsonrpc_method_map_delete);
    pthread_mutex_unlock(&rpc->mutex);

    pthread_mutex_destroy(&rpc->mutex);
}

bool jsonrpc_register_method(jsonrpc_t* rpc, const char* name, jsonrpc_method_t method)
{
    bool ret = false;
    void* inmap;

    if (unlikely(rpc == NULL)) return false;
    if (unlikely(name == NULL) || unlikely(strlen(name) == 0)) return false;

    pthread_mutex_lock(&rpc->mutex);
    inmap = hashMapGet(rpc->request_map, (void*) name, HASHMAP_STRING_SIZEOF(name));
    if (inmap) {
        if (inmap == method) {
            ret = true;
        }
    } else {
        ret = hashMapPut(rpc->request_map, strdup(name), HASHMAP_STRING_SIZEOF(name), method);
    }
    pthread_mutex_unlock(&rpc->mutex);

    return ret;
}

int jsonrpc_execute(jsonrpc_t* rpc, const cJSON* request, cJSON** response)
{
    cJSON *method, *id, *resp;
    jsonrpc_method_t func;
    jsonrpc_method_t all_notifications = NULL;

    if (response) {
        *response = NULL;
    }

    method = cJSON_GetObjectItem(request, JSONRPC_KEY_METHOD);
    if (unlikely((method == NULL) || (!cJSON_IsString(method)))) {
        return EBADMSG;
    }

    id = cJSON_GetObjectItem(request, JSONRPC_KEY_ID);

    // This is a notification
    pthread_mutex_lock(&rpc->mutex);
    func = hashMapGet(rpc->request_map, method->valuestring, HASHMAP_STRING_SIZEOF(method->valuestring));

    if (id == NULL) {
        all_notifications = hashMapGet(rpc->request_map, ALL_NOTIFICATIONS, HASHMAP_STRING_SIZEOF(ALL_NOTIFICATIONS));
    }
    pthread_mutex_unlock(&rpc->mutex);

    if (unlikely(func == NULL)) {
        return ENOTSUP;
    }

    resp = func(id, cJSON_GetObjectItem(request, JSONRPC_KEY_PARAMS));

    /* No response on notifications. Thus we do not need to check
     * for said response.
     *
     * The method name will be passed in place of the ID as there is NO
     * ID for notifications, and the receiver doesn't know what
     * method is being called.
     */
    if (all_notifications) {
        all_notifications(method, cJSON_GetObjectItem(request, JSONRPC_KEY_PARAMS));
    }

    if (response && id) {
        *response = resp;
    } else if (resp != NULL) {
        cJSON_Delete(resp);
    }

    return 0;
}
