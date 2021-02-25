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
// Created by wboyd747 on 6/26/18.
//

#include <icBuildtime.h>
#include <string.h>

#include <automationService.h>
#include <automationAction.h>

#include <icSystem/hardwareCapabilities.h>
#include <icSystem/softwareCapabilities.h>
#include <securityService/securityService_ipc.h>

#include "systemState.h"

// Handles arm/disarm requests via rules
//
static cJSON *handleSecurityRequest(const cJSON *id, const cJSON *params)
{
    cJSON *response = NULL;
    bool success = false;

    unused(id);


    if (supportAlarms())
    {

        cJSON *json;

        json = cJSON_GetObjectItem(params, "action");

        if (json && json->valuestring)
        {// Determine if we have a disarm action or arm action
            // If not disarm, then its arm
            //
            if (strcmp(json->valuestring, "disarm") == 0)
            {
                disarmResult *result = create_disarmResult();
                // Copy token because its part of the json object
                //
                char *tokenCopy = strdup(cJSON_GetObjectItem(params, "token")->valuestring);

                if (securityService_request_DISARM_SYSTEM_FOR_RULE(tokenCopy, result) == IPC_SUCCESS)
                {
                    switch (result->result)
                    {
                        case SYSTEM_DISARM_SUCCESS:
                        {
                            success = true;
                            break;
                        }
                        case SYSTEM_DISARM_ALREADY_DISARMED:
                        {
                            icLogInfo(LOG_TAG, "%s: system already disarmed, unable to disarm from rule", __FUNCTION__);
                            break;
                        }
                        case SYSTEM_DISARM_INVALID_ARGS:
                        {
                            icLogError(LOG_TAG, "%s:  invalid token, unable to disarm from rule", __FUNCTION__);
                            break;
                        }
                        case SYSTEM_DISARM_SYS_FAILURE:
                        case SYSTEM_DISARM_FAIL_USERCODE:
                        default:
                        {
                            // other than SYSTEM_DISARM_SYS_FAILURE, we shouldn't get any of the other reasons
                            // just log a generic error
                            icLogError(LOG_TAG, "%s:  generic system failure, unable to disarm from rule",
                                       __FUNCTION__);
                        }
                    }
                }
                else
                {
                    icLogError(LOG_TAG
                                       "%s:  failed disarm IPC call, unable to disarm from rule", __FUNCTION__);
                }
                destroy_disarmResult(result);
                free(tokenCopy);
            }
            else
            {
                armForRuleRequest *cmd = create_armForRuleRequest();

                // Determine the arm method
                //
                if (strcmp(json->valuestring, "away") == 0)
                {
                    cmd->armMode = ARM_METHOD_AWAY;
                }
                else if (strcmp(json->valuestring, "stay") == 0)
                {
                    cmd->armMode = ARM_METHOD_STAY;
                }
                else if (strcmp(json->valuestring, "night") == 0)
                {
                    cmd->armMode = ARM_METHOD_NIGHT;
                }
                else
                {
                    // Bail because we don't have a valid arm method
                    //
                    icLogError(LOG_TAG, "%s: failed to handle security request, invalid arm method supplied:  %s",
                               __FUNCTION__, json->valuestring);
                    destroy_armForRuleRequest(cmd);
                    response = jsonrpc_create_response_error(id,
                                                             IPC_GENERAL_ERROR,
                                                             "Failure to handle system action.",
                                                             NULL);
                    return response;
                }

                armResult *result = create_armResult();

                // Copy token because its part of the json object
                //
                cmd->token = strdup(cJSON_GetObjectItem(params, "token")->valuestring);

                // Use default
                cmd->exitDelayOverrideSeconds = 0;

                if (securityService_request_ARM_SYSTEM_FOR_RULE(cmd, result) == IPC_SUCCESS)
                {
                    switch (result->result)
                    {
                        case SYSTEM_ARM_SUCCESS:
                        {
                            success = true;
                            break;
                        }
                        case SYSTEM_ARM_INVALID_ARGS:
                        {
                            icLogError(LOG_TAG, "%s: invalid arguments provided, unable to arm from rule,  args = %s",
                                       __FUNCTION__,
                                       cJSON_PrintUnformatted(encode_armResult_toJSON(result)));
                            break;
                        }
                        case SYSTEM_ARM_ALREADY_ARMED:
                        {
                            icLogInfo(LOG_TAG, "%s:  system already armed, unable to arm from rule", __FUNCTION__);
                            break;
                        }
                        case SYSTEM_ARM_FAIL_ACCOUNT_DEACTIVATED:
                        case SYSTEM_ARM_FAIL_ACCOUNT_SUSPENDED:
                        {
                            icLogWarn(LOG_TAG, "%s:  system suspended/deactivated, unable to arm from rule",
                                      __FUNCTION__);
                            break;
                        }
                        case SYSTEM_ARM_FAIL_TOO_MANY_SECURITY_DEVICES:
                        {
                            icLogError(LOG_TAG, "%s:  too many security devices, unable to arm from rule",
                                       __FUNCTION__);
                            break;
                        }
                        case SYSTEM_ARM_FAIL_TROUBLE:
                        case SYSTEM_ARM_FAIL_ZONE:
                        {
                            icLogWarn(LOG_TAG, "%s:  zone event/trouble preventing arming, unable to arm from rule",
                                      __FUNCTION__);
                            break;
                        }
                        case SYSTEM_ARM_FAIL_UPGRADE:
                        {
                            icLogWarn(LOG_TAG, "%s:  system is upgrading, unable to arm from rule", __FUNCTION__);
                            break;
                        }
                        case SYSTEM_ARM_FAIL_USERCODE:
                        case SYSTEM_ARM_SYS_FAILURE:
                        default:
                        {
                            //shouldn't run into use code reason
                            //generic error message
                            icLogError(LOG_TAG, "%s:  generic system error, unable to arm from rule", __FUNCTION__);
                            break;
                        }
                    }
                }
                else
                {
                    icLogError(LOG_TAG, "%s: IPC call to arm failed", __FUNCTION__);
                }

                // cleanup
                destroy_armResult(result);
                destroy_armForRuleRequest(cmd);
            }
        }
    }

    if (id)
    {
        if (success)
        {
            response = jsonrpc_create_response_success(id, NULL);
        }
        else
        {
            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle system action.",
                                                     NULL);
        }
    }

    return response;
}

static cJSON *handleSceneRequest(const cJSON *id, const cJSON *params)
{
    cJSON *response = NULL;
    bool success = false;

    unused(id);

    if (supportSystemMode())
    {
        char *currentMode;

        if (securityService_request_GET_CURRENT_SYSTEM_MODE(&currentMode) == IPC_SUCCESS)
        {
            cJSON *json;

            json = cJSON_GetObjectItem(params, "name");
            if (json && json->valuestring)
            {
                if (strcmp(json->valuestring, currentMode) == 0)
                {
                    /* We are already this "mode" thus just tell the
                     * system that we successfully transitioned.
                     */
                    success = true;
                }
                else
                {
                    systemModeRequest *cmd = create_systemModeRequest();
                    bool _success = false;

                    cmd->requestId = 0;
                    cmd->systemMode = json->valuestring;

                    if (securityService_request_SET_CURRENT_SYSTEM_MODE(cmd, &_success) == IPC_SUCCESS)
                    {
                        success = _success;
                    }

                    cmd->systemMode = NULL;
                    destroy_systemModeRequest(cmd);
                }
            }

            free(currentMode);
        }
    }

    if (id)
    {
        if (success)
        {
            response = jsonrpc_create_response_success(id, NULL);
        }
        else
        {
            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle system action.",
                                                     NULL);
        }
    }

    return response;
}

void systemMessageTargetInit(void)
{
//    automationActionRegisterOps("playAudioAction", systemActionHandler);
    automationActionRegisterOps("sceneChangeAction", handleSceneRequest);
    automationActionRegisterOps("securityChangeAction", handleSecurityRequest);
}
