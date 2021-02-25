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
// Created by mkoch201 on 5/22/18.
//

#include <securityService/cameraTroubleEventHelper.h>
#include <jsonHelper/jsonHelper.h>
#include <stdlib.h>

#define REASON "reason"

/**
 * Encode camera trouble structure into JSON
 *
 * @param cameraTroublePayload the payload to encode
 * @return JSON version of the payload
 */
cJSON *encodeCameraTroublePayload(CameraTroublePayload *cameraTroublePayload)
{
    cJSON *json = NULL;
    if (cameraTroublePayload != NULL && cameraTroublePayload->deviceTrouble != NULL)
    {
        // first encode the DeviceTroublePayload
        json = encodeDeviceTroublePayload(cameraTroublePayload->deviceTrouble);

        // add our attributes at the same node-level.  this allows us to generically parse
        // the JSON string as a DeviceTroublePayload first
        cJSON_AddItemToObjectCS(json, REASON, cJSON_CreateString(cameraTroublePayload->reason));
    }

    return json;
}


/**
 * Decode camera trouble structure from JSON
 *
 * @param cameraTroublePayloadJSON the JSON to decode
 * @return the structure data, or NULL if some error.  Caller must destroy this
 *
 * @see cameraTroublePayloadDestroy
 */
CameraTroublePayload *decodeCameraTroublePayload(cJSON *cameraTroublePayloadJSON)
{
    CameraTroublePayload *payload = NULL;
    if (cameraTroublePayloadJSON != NULL)
    {
        // first decode the DeviceTroublePayload
        payload = cameraTroublePayloadCreate();
        deviceTroublePayloadDestroy(payload->deviceTrouble);
        payload->deviceTrouble = decodeDeviceTroublePayload(cameraTroublePayloadJSON);

        // now extract our attributes from the same node-level
        payload->reason = getCJSONString(cameraTroublePayloadJSON, REASON);
    }

    return payload;
}

/**
 * Create an empty camera trouble payload structure
 *
 * @return the structure data. Caller must destroy this
 *
 * @see cameraTroublePayloadDestroy
 */
CameraTroublePayload *cameraTroublePayloadCreate()
{
    CameraTroublePayload *payload = (CameraTroublePayload *)calloc(1, sizeof(CameraTroublePayload));
    payload->deviceTrouble = deviceTroublePayloadCreate();

    return payload;
}

/**
 * Destroy a camera trouble payload structure
 *
 * @param cameraTroublePayload the payload to destroy
 */
void cameraTroublePayloadDestroy(CameraTroublePayload *cameraTroublePayload)
{
    if (cameraTroublePayload != NULL)
    {
        deviceTroublePayloadDestroy(cameraTroublePayload->deviceTrouble);
        free(cameraTroublePayload->reason);

        free(cameraTroublePayload);
    }
}