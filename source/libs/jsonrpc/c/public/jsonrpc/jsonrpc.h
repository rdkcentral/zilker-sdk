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

#ifndef _CCATX_jsonrpc_H
#define _CCATX_jsonrpc_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include <icTypes/icHashMap.h>

#include <cjson/cJSON.h>

/** Registration macro to receive all
 * notifications sent through 'execute'.
 */
#define ALL_NOTIFICATIONS "all_notifications"

/**
 * JSONRPC method callback routine definition.
 *
 * @param id The ID of the JSONRPC request, NULL if a notification.
 * @param params JSON object/array of parameters. May be NULL if no
 * parameters were sent.
 */
typedef cJSON* (*jsonrpc_method_t)(const cJSON* id, const cJSON* params);

typedef struct jsonrpc {
    icHashMap* request_map;

    pthread_mutex_t mutex;
} jsonrpc_t;

/**
 * Create a new JSON representation of a JSONRPC notification.
 *
 * Notifications do not expect a return response thus no ID is allowed.
 *
 * @param method The name of the RPC method this notification will be sent to.
 * @param params A JSON object/array of parameters, may be NULL if no parameters
 * are required.
 * @return The JSON object that represents the notification. The caller
 * is responsible for the JSON object.
 */
cJSON* jsonrpc_create_notification(const char* method, cJSON* params);

/**
 * Create a new JSON representation of a JSONRPC request.
 *
 * @param id The unique identifier that will be used to match the request
 * with the return response.
 * @param method The name of the RPC method this request will be sent to.
 * @param params A JSON object/array of parameters, may be NULL if no parameters
 * are required.
 * @return The JSON object that represents the request. The caller
 * is responsible for the JSON object.
 */
cJSON* jsonrpc_create_request(const cJSON* id, const char* method, cJSON* params);

/**
 * Create a new JSON representation of a JSONRPC successful response.
 *
 * @param id The unique identifier supplied via the JSONRPC request.
 * @param result The resultant data from the RPC call. A NULL result
 * may be provided if no result is necessary.
 * @return The JSON object that represents the response. The caller
 * is responsible for the JSON object.
 */
cJSON* jsonrpc_create_response_success(const cJSON* id, cJSON* result);

/**
 * Create a new JSON representation of a JSONRPC error response.
 *
 * @param id The unique identifier supplied via the JSONRPC request.
 * @param code The error code that represents the failure reason.
 * @param message A concise textual description of the error.
 * @param data Optional further data that may describe the error. A
 * value of NULL may be passed in.
 * @return The JSON object that represents the response. The caller
 * is responsible for the JSON object.
 */
cJSON* jsonrpc_create_response_error(const cJSON* id, int code, const char* message, cJSON* data);

/**
 * Verify if the received response is a "success" response.
 *
 * @param response The JSONRPC response object.
 * @return True if the object is a success response.
 */
bool jsonrpc_is_response_success(const cJSON* response);
/**
 * Verify if the received response is a "error" response.
 *
 * @param response The JSONRPC response object.
 * @return True if the object is a error response.
 */
bool jsonrpc_is_response_error(const cJSON* response);

/**
 * Break down a JSONRPC success response into individual
 * components.
 *
 * @param response The JSONRPC response object.
 * @param id The unique identifier supplied via the JSONRPC request.
 * @param result The resultant data from the RPC call. A NULL result
 * may be provided if no result is necessary.
 * @return A value of '0' on success, otherwise a valid error code.
 */
int jsonrpc_get_response_success(const cJSON* response,
                                 const cJSON** id,
                                 const cJSON** result);

/**
 * Break down a JSONRPC error response into individual
 * components.
 *
 * @param response The JSONRPC response object.
 * @param id The unique identifier supplied via the JSONRPC request.
 * @param code The error code that represents the failure reason.
 * @param message A concise textual description of the error.
 * @param data Optional further data that may describe the error. A
 * value of NULL may be passed in.
 * @return A value of '0' on success, otherwise a valid error code.
 */
int jsonrpc_get_response_error(const cJSON* response,
                               const cJSON** id,
                               int* code,
                               const char** message,
                               const cJSON** data);
/**
 * Verify if the JSON object is a valid JSONRPC 2.0
 * request, or response, object.
 *
 * @param object The JSONRPC object.
 * @return True if a valid JSONRPC 2.0 object.
 */
bool jsonrpc_is_valid(const cJSON* object);

/**
 * Retrieve the method name from a JSONRPC request object.
 *
 * @param request A valid JSONRPC request.
 * @return The string name of the method, otherwise a NULL pointer.
 */
const char* jsonrpc_get_method(const cJSON* request);

/**
 * Initialize a new JSONRPC execution class.
 *
 * @param rpc The JSONRPC class that will be associated
 * with a grouping of RPC routines.
 * @return A value of '0' on success, otherwise a valid error code.
 */
int jsonrpc_init(jsonrpc_t* rpc);

/**
 * Destroy the JSONRPC execution class. The object
 * may not be used unless initialized again.
 *
 * @param rpc The JSONRPC execution class.
 */
void jsonrpc_destroy(jsonrpc_t* rpc);

/**
 * Register a new RPC method with a unique RPC function name.
 *
 * @param rpc The JSONRPC execution class.
 * @param name The name of the RPC command that will be supplied within
 * a JSONRPC request.
 * @param method The callback routine that will be called whenever
 * a JSONRPC request is received with the given name.
 * @return True if the RPC method was registered.
 */
bool jsonrpc_register_method(jsonrpc_t* rpc, const char* name, jsonrpc_method_t method);

/**
 * Execute a received JSONRPC request/notification by parsing the JSONRPC
 * object and calling the registered JSONRPC method.
 *
 * @param rpc The JSONRPC execution class.
 * @param request The JSONRPC request object that was received.
 * @param response A pointer that will be updated with a JSONRPC
 * response object <em>if</em> the request is a notification. If the
 * reponse pointer is NULL then no response will be provided.
 * @return A value of '0' on success, otherwise a valid error code.
 */
int jsonrpc_execute(jsonrpc_t* rpc, const cJSON* request, cJSON** response);

#endif //_CCATX_jsonrpc_H
