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
// Created by wboyd747 on 6/22/18.
//

#include <string.h>

#include <automationService.h>
#include <automationAction.h>
#include <commMgr/commService_pojo.h>
#include <errno.h>
#include <commMgr/commService_ipc.h>

#include "camera.h"

static cJSON* handlePictureRequest(const cJSON* id, const cJSON* params)
{
    cJSON *json;
    uploadPicturesFromCamera *cmd;
    cJSON *response = NULL;
    bool success;

    cmd = create_uploadPicturesFromCamera();

    json = cJSON_GetObjectItem(params, "cameraId");
    //If we don't have the cameraId, then we are supposed to take and upload from ALL the cameras. Comm will handle this.
    cmd->cameraUid = (json == NULL || cJSON_IsNull(json) == true || cJSON_IsString(json) == false) ? NULL : strdup(
            cJSON_GetStringValue(json));

    json = cJSON_GetObjectItem(params, "count");
    if (json == NULL || cJSON_IsNumber(json) == true || cJSON_IsNull(json) == true)
    {
        cmd->numPics = ((json == NULL) || cJSON_IsNull(json)) ? 5 : (uint32_t) json->valueint;
    }
    else
    {
        icLogError(LOG_TAG, "%s: Invalid count provided. Bailing", __FUNCTION__);
        success = false;
        goto cleanupPictureRequest;
    }

    json = cJSON_GetObjectItem(params, "eventId");
    if (json == NULL || cJSON_IsNumber(json) == true || cJSON_IsNull(json) == true)
    {
        cmd->eventId = ((json == NULL) || cJSON_IsNull(json)) ? 0 : (uint64_t) json->valuedouble;
    }
    else
    {
        icLogError(LOG_TAG, "%s: Invalid eventId provided. Bailing", __FUNCTION__);
        success = false;
        goto cleanupPictureRequest;
    }

    json = cJSON_GetObjectItem(params, "ruleId");
    if (json != NULL && cJSON_IsNumber(json) == true)
    {
        cmd->ruleId = (uint64_t) json->valuedouble;
    }
    else
    {
        icLogError(LOG_TAG, "%s: Invalid ruleId provided. Bailing", __FUNCTION__);
        success = false;
        goto cleanupPictureRequest;
    }


    cmd->doAsync = true;

    //Now, what branch are we coming from?
    //We can get here through entry delay, alarm, and zone alarm session events
    json = cJSON_GetObjectItem(params, "eventCode");
    cmd->eventCode = ((json == NULL) || cJSON_IsNull(json)) ? 0 : (int32_t) json->valuedouble;
    success = (commService_request_UPLOAD_PICTURES_FROM_CAMERA(cmd) == IPC_SUCCESS);

    cleanupPictureRequest:

    destroy_uploadPicturesFromCamera(cmd);

    if (id) {
        if (success) {
            response = jsonrpc_create_response_success(id, NULL);
        } else {
            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle camera action.",
                                                     NULL);
        }
    }

    return response;
}

static cJSON* handleVideoRequest(const cJSON* id, const cJSON* params)
{
    cJSON* json;
    uploadVideoFromCamera* cmd;
    cJSON* response = NULL;
    bool success = false;

    cmd = create_uploadVideoFromCamera();

    json = cJSON_GetObjectItem(params, "cameraId");
    if (json != NULL && cJSON_IsString(json) == true) {
        cmd->cameraUid = strdup(json->valuestring);

        json = cJSON_GetObjectItem(params, "duration");
        if (json == NULL || cJSON_IsNumber(json) == true || cJSON_IsNull(json) == true)
        {
            cmd->duration = ((json == NULL) || cJSON_IsNull(json)) ? 10 : (uint32_t) json->valueint;
        }
        else
        {
            icLogError(LOG_TAG, "%s: Invalid duration provided. Bailing", __FUNCTION__);
            success = false;
            goto cleanupVideoRequest;
        }

        json = cJSON_GetObjectItem(params, "eventId");
        if (json == NULL || cJSON_IsNumber(json) == true || cJSON_IsNull(json) == true)
        {
            cmd->eventId = ((json == NULL) || cJSON_IsNull(json)) ? 0 : (uint64_t) json->valuedouble;
        }
        else
        {
            icLogError(LOG_TAG, "%s: Invalid eventId provided. Bailing", __FUNCTION__);
            success = false;
            goto cleanupVideoRequest;
        }

        json = cJSON_GetObjectItem(params, "ruleId");
        if (json != NULL && cJSON_IsNumber(json) == true)
        {
            cmd->ruleId = (uint64_t) json->valuedouble;
        }
        else
        {
            icLogError(LOG_TAG, "%s: Invalid ruleId provided. Bailing", __FUNCTION__);
            success = false;
            goto cleanupVideoRequest;
        }

        cmd->doAsync = false;

        json = cJSON_GetObjectItem(params, "eventCode");
        cmd->eventCode = ((json == NULL) || cJSON_IsNull(json)) ? 0 : (int32_t) json->valuedouble;

        success = (commService_request_UPLOAD_VIDEO_FROM_CAMERA(cmd) == IPC_SUCCESS);
    }

    cleanupVideoRequest:

    destroy_uploadVideoFromCamera(cmd);

    if (id) {
        if (success) {
            response = jsonrpc_create_response_success(id, NULL);
        } else {
            response = jsonrpc_create_response_error(id,
                                                     IPC_GENERAL_ERROR,
                                                     "Failure to handle camera action.",
                                                     NULL);
        }
    }

    return response;
}

void cameraMessageTargetInit(void)
{
    automationActionRegisterOps("takePictureAction", handlePictureRequest);
    automationActionRegisterOps("takeVideoAction", handleVideoRequest);
}
