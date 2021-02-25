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
#include <automationService.h>

#include "test.h"
#include "automationAction.h"

static cJSON* testActionHandler(const cJSON* id, const cJSON* params)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON* response = NULL;
    bool success = false;

    unused(id);

    if (params != NULL)
    {
        cJSON* requestType = cJSON_GetObjectItem(params, "requestType");

        if(requestType != NULL && strcmp(requestType->valuestring, "dummyRequest") == 0)
        {
            success = true;
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: invalid message", __FUNCTION__);
    }

    if (id) {
        if (success) {
            response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "type", "dummyResponse");

            response = jsonrpc_create_response_success(id, response);
        } else {
            response = jsonrpc_create_response_error(id,
                                                     -1,
                                                     "Failure to handle system action.",
                                                     NULL);
        }
    }

    return response;
}

void testMessageTargetInit(void)
{
    automationActionRegisterOps("test", testActionHandler);
}
