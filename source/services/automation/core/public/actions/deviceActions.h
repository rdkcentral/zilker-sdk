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

#ifndef ZILKER_AUTOMATION_DEVICEACTIONS_H
#define ZILKER_AUTOMATION_DEVICEACTIONS_H

/**
 * JSONRPC request parameter for device read/write/execute URI
 */
#define AUTOMATION_DEV_RESOURCE_PARAM_URI "uri"

/**
 * JSONRPC response key indicating the response type (see keys below)
 */
#define AUTOMATION_DEV_RESPONSE_TYPE "type"

/**
 * JSONRPC request parameter for a resource URI to read; action is suppressed when "true"
 * This is applicable to write and execute operations.
 */
#define AUTOMATION_DEV_RESOURCE_PARAM_ACTION_SUPPRESS_RESOURCE_URI "actionSuppressResourceURI"

/*
 * Read Resource
 */
/**
 * JSONRPC method for the 'read resource' action
 */
#define AUTOMATION_DEV_READ_RESOURCE_METHOD "readDeviceResource"

/**
 * JSONRPC device 'read resource' response type
 */
#define AUTOMATION_DEV_READ_RESOURCE_RESPONSE_TYPE "readResourceResponse"

/**
 * JSONRPC response key for the resource value
 */
#define AUTOMATION_DEV_READ_RESOURCE_RESPONSE_VALUE "value"

/*
 * Write Resource
 */
/**
 * JSONRPC method for the 'write resource' action
 */
#define AUTOMATION_DEV_WRITE_RESOURCE_METHOD "writeDeviceResource"

/**
 * JSONRPC request parameter for resource value to write
 */
#define AUTOMATION_DEV_WRITE_RESOURCE_PARAM_VALUE "value"

/**
 * JSONRPC device resource write response type
 */
#define AUTOMATION_DEV_WRITE_RESOURCE_RESPONSE_TYPE "writeResourceResponse"

/*
 * Execute Resource
 */
/**
 * JSONRPC method for 'execute resource' action
 */
#define AUTOMATION_DEV_EXEC_RESOURCE_METHOD "executeDeviceResource"

/**
 * JSONRPC request parameter for argument to execute resource with
 */
#define AUTOMATION_DEV_EXEC_RESOURCE_PARAM_ARG "arg"

/**
 * JSONRPC device resource execute response type
 */
#define AUTOMATION_DEV_EXEC_RESOURCE_RESPONSE_TYPE "executeResourceResponse"

/**
 * JSONRPC response key for the device resource execute result
 */
#define AUTOMATION_DEV_EXEC_RESOURCE_RESPONSE_RESULT "result"

#endif //ZILKER_AUTOMATION_DEVICEACTIONS_H
