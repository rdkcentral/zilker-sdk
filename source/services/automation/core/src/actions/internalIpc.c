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
// Created by tlea on 4/26/18.
//

/*
 * This message target enables automations to send arbitrary ipc requests and events to the local platform.
 */

#include <string.h>
#include <icLog/logging.h>
#include <stdbool.h>
#include <icIpc/ipcSender.h>
#include <cjson/cJSON.h>
#include <automationService.h>
#include "internalIpc.h"
#include "automationAction.h"

#define SEND_IPC_RESPONSE "sendIpcResponse"
#define MESSAGE_TYPE "type"
#define PORT_PARAM "port"
#define MSG_CODE_PARAM "msgCode"
#define PAYLOAD_PARAM "payload"

static cJSON* handleSendIpc(const cJSON* id, const cJSON* params)
{
    cJSON* response = cJSON_CreateObject();
    bool success = false;

    cJSON* port = cJSON_GetObjectItem(params, PORT_PARAM);
    cJSON* msgCode = cJSON_GetObjectItem(params, MSG_CODE_PARAM);
    cJSON* payload = cJSON_GetObjectItem(params, PAYLOAD_PARAM);

    cJSON_AddStringToObject(response, MESSAGE_TYPE, SEND_IPC_RESPONSE);

    if(port != NULL && msgCode != NULL)
    {
        IPCMessage ipcRequest;
        IPCMessage ipcResponse;

        memset(&ipcRequest, 0, sizeof(ipcRequest));
        memset(&ipcResponse, 0, sizeof(ipcResponse));

        //a request without a payload is ok, but encode it if one was provided
        if(payload != NULL)
        {
            ipcRequest.payload = cJSON_PrintUnformatted(payload);
            ipcRequest.payloadLen = (uint32_t) strlen(ipcRequest.payload) + 1; //1 for null
        }

        ipcRequest.msgCode = msgCode->valueint;

        IPCCode rc = sendServiceRequest((uint16_t) port->valueint, &ipcRequest, &ipcResponse);
        if(rc == IPC_SUCCESS)
        {
            //a response without a payload is ok, but if one is provided we need to send it back to the automation
            if(ipcResponse.payloadLen > 0)
            {
                cJSON* ipcResponseJson = cJSON_Parse(ipcResponse.payload);
                cJSON_AddItemToObject(response, PAYLOAD_PARAM, ipcResponseJson);
            }

            //use the msg code from the request since the response wont have one.
            cJSON_AddNumberToObject(response, MSG_CODE_PARAM, ipcRequest.msgCode);

            success = true;
        }

        free(ipcRequest.payload);
        free(ipcResponse.payload);
    }

    if (id) {
        if (success) {
            response = jsonrpc_create_response_success(id, response);
        } else {
            cJSON_Delete(response);

            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle notification action.",
                                                     NULL);
        }
    } else {
        cJSON_Delete(response);
        response = NULL;
    }

    return response;
}

void ipcMessageTargetInit(void)
{
    automationActionRegisterOps("sendIpcAction", handleSendIpc);
}
