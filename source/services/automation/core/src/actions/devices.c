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
#include <icLog/logging.h>
#include <deviceService/deviceService_ipc.h>
#include <automationService.h>
#include "actions/deviceActions.h"
#include "devices.h"
#include "automationAction.h"

#define MAX_RESOURCE_OP_TIMEOUT_SECS 60

static bool isActionAllowed(const cJSON *requestParams, const char *action)
{
    bool actionAllowed = true;
    cJSON *actionSuppressResourceURI = cJSON_GetObjectItem(requestParams, AUTOMATION_DEV_RESOURCE_PARAM_ACTION_SUPPRESS_RESOURCE_URI);

    if (actionSuppressResourceURI != NULL && cJSON_IsString(actionSuppressResourceURI))
    {
        AUTO_CLEAN(pojo_destroy__auto) DSResourceList *resources = create_DSResourceList();
        IPCCode rc = deviceService_request_QUERY_RESOURCES_BY_URI_timeout(cJSON_GetStringValue(actionSuppressResourceURI),
                                                                          resources,
                                                                          MAX_RESOURCE_OP_TIMEOUT_SECS);
        if (rc == IPC_SUCCESS)
        {
            sbIcLinkedListIterator *it = linkedListIteratorCreate(resources->resourceList);
            while (linkedListIteratorHasNext(it))
            {
                DSResource *resource = linkedListIteratorGetNext(it);
                if (resource->value != NULL)
                {
                    actionAllowed = strcmp(resource->value, "false") == 0;
                }

                if (!actionAllowed)
                {
                    icLogDebug(LOG_TAG, "%s: '%s' action suppressed by resource %s", __FUNCTION__, action, resource->uri);
                    break;
                }
            }
        }
        else
        {
            icLogWarn(LOG_TAG,
                      "%s: action suppression resource read failed: %s, proceeding with '%s'",
                      __FUNCTION__,
                      IPCCodeLabels[rc],
                      action);
        }
    }

    return actionAllowed;
}

static cJSON* handleReadResource(const cJSON* id, const cJSON* params)
{
    cJSON* response = cJSON_CreateObject();
    bool success;

    cJSON* uri = cJSON_GetObjectItem(params, AUTOMATION_DEV_RESOURCE_PARAM_URI);

    cJSON_AddStringToObject(response, AUTOMATION_DEV_RESPONSE_TYPE, AUTOMATION_DEV_READ_RESOURCE_RESPONSE_TYPE);

    if (uri == NULL)
    {
        icLogWarn(LOG_TAG, "%s: missing uri", __FUNCTION__);
        return false;
    }

    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, uri->valuestring);

    DSReadResourceResponse *readResponse = create_DSReadResourceResponse();
    IPCCode rc = deviceService_request_READ_RESOURCE_timeout(uri->valuestring, readResponse,
                                                             MAX_RESOURCE_OP_TIMEOUT_SECS);
    success = readResponse->success;

    if (rc == IPC_SUCCESS && readResponse->success)
    {
        cJSON_AddStringToObject(response, AUTOMATION_DEV_READ_RESOURCE_RESPONSE_VALUE, readResponse->response);
    }
    else
    {
        icLogError(LOG_TAG, "%s: READ_RESOURCE failed (rc=%d, success=%s)", __FUNCTION__, rc,
                   readResponse->success ? "true" : "false");
    }

    destroy_DSReadResourceResponse(readResponse);

    if (id)
    {
        if (success)
        {
            response = jsonrpc_create_response_success(id, response);
        }
        else
        {
            cJSON_Delete(response);

            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle read resource action.",
                                                     NULL);
        }
    }
    else
    {
        cJSON_Delete(response);
        response = NULL;
    }

    return response;
}

static cJSON* handleWriteResource(const cJSON* id, const cJSON* params)
{
    cJSON* response = cJSON_CreateObject();
    bool success = false;

    cJSON* uri = cJSON_GetObjectItem(params, AUTOMATION_DEV_RESOURCE_PARAM_URI);
    cJSON* value = cJSON_GetObjectItem(params, AUTOMATION_DEV_WRITE_RESOURCE_PARAM_VALUE);

    cJSON_AddStringToObject(response, AUTOMATION_DEV_RESPONSE_TYPE, AUTOMATION_DEV_WRITE_RESOURCE_RESPONSE_TYPE);

    if (value != NULL && uri != NULL)
    {
        icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, uri->valuestring);
        if (isActionAllowed(params, "writeResource"))
        {
            DSWriteResourceRequest *writeRequest = create_DSWriteResourceRequest();
            writeRequest->uri = uri->valuestring;
            writeRequest->value = value->valuestring;

            IPCCode rc = deviceService_request_WRITE_RESOURCE_timeout(writeRequest, &success,
                                                                      MAX_RESOURCE_OP_TIMEOUT_SECS);

            if (rc != IPC_SUCCESS || !success)
            {
                icLogError(LOG_TAG, "%s: WRITE_RESOURCE failed (rc=%d, success=%s)", __FUNCTION__, rc,
                           success ? "true" : "false");
            }

            // cleanup
            writeRequest->uri = NULL;
            writeRequest->value = NULL;
            destroy_DSWriteResourceRequest(writeRequest);
        }
    }
    else
    {
        icLogError(LOG_TAG,
                   "%s: missing %s and/or %s for write resource request",
                   __FUNCTION__,
                   AUTOMATION_DEV_RESOURCE_PARAM_URI,
                   AUTOMATION_DEV_WRITE_RESOURCE_PARAM_VALUE);
    }

    if (id)
    {
        if (success)
        {
            response = jsonrpc_create_response_success(id, response);
        }
        else
        {
            cJSON_Delete(response);

            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle write resource action.",
                                                     NULL);
        }
    }
    else
    {
        cJSON_Delete(response);
        response = NULL;
    }

    return response;
}

static cJSON* handleExecResource(const cJSON* id, const cJSON* params)
{
    cJSON* response = cJSON_CreateObject();
    bool success = false;

    cJSON* uri = cJSON_GetObjectItem(params, AUTOMATION_DEV_RESOURCE_PARAM_URI);
    cJSON* arg = cJSON_GetObjectItem(params, AUTOMATION_DEV_EXEC_RESOURCE_PARAM_ARG);

    cJSON_AddStringToObject(response, AUTOMATION_DEV_RESPONSE_TYPE, AUTOMATION_DEV_EXEC_RESOURCE_RESPONSE_TYPE);

    if (arg != NULL && uri != NULL)
    {
        icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, uri->valuestring);
        if (isActionAllowed(params, "executeResource"))
        {
            DSExecuteResourceRequest *execRequest = create_DSExecuteResourceRequest();
            execRequest->uri = uri->valuestring;
            execRequest->arg = arg->valuestring;

            DSExecuteResourceResponse *execResponse = create_DSExecuteResourceResponse();

            IPCCode rc = deviceService_request_EXECUTE_RESOURCE_timeout(execRequest, execResponse,
                                                                        MAX_RESOURCE_OP_TIMEOUT_SECS);

            if (rc == IPC_SUCCESS && execResponse->success)
            {
                success = true;
                cJSON_AddStringToObject(response, AUTOMATION_DEV_EXEC_RESOURCE_RESPONSE_RESULT, execResponse->response);
            } else
            {
                icLogError(LOG_TAG, "%s: EXECUTE_RESOURCE failed (rc=%s, success=false)", __FUNCTION__, IPCCodeLabels[rc]);
            }

            execRequest->uri = NULL;
            execRequest->arg = NULL;
            destroy_DSExecuteResourceRequest(execRequest);
            destroy_DSExecuteResourceResponse(execResponse);
        }
    }
    else
    {
        icLogError(LOG_TAG,
                   "%s: missing %s and/or %s for execute resource request",
                   __FUNCTION__,
                   AUTOMATION_DEV_RESOURCE_PARAM_URI,
                   AUTOMATION_DEV_EXEC_RESOURCE_PARAM_ARG);
    }

    if (id)
    {
        if (success)
        {
            response = jsonrpc_create_response_success(id, response);
        }
        else
        {
            cJSON_Delete(response);

            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle execute resource action.",
                                                     NULL);
        }
    }
    else
    {
        cJSON_Delete(response);
        response = NULL;
    }

    return response;
}

void deviceMessageTargetInit(void)
{
    automationActionRegisterOps(AUTOMATION_DEV_WRITE_RESOURCE_METHOD, handleWriteResource);
    automationActionRegisterOps(AUTOMATION_DEV_READ_RESOURCE_METHOD, handleReadResource);
    automationActionRegisterOps(AUTOMATION_DEV_EXEC_RESOURCE_METHOD, handleExecResource);
}
