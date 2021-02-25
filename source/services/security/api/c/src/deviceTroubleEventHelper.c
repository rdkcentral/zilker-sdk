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

#include <securityService/deviceTroubleEventHelper.h>
#include <jsonHelper/jsonHelper.h>

#include <stdlib.h>
#include <string.h>

#define ROOT_ID "rootId"
#define OWNER_URI "ownerUri"
#define RESOURCE_URI "resourceUri"
#define DEVICE_CLASS "deviceClass"

/**
 * Encode device trouble structure into JSON
 *
 * @param deviceTroublePayload the payload to encode
 * @return JSON version of the payload
 */
cJSON *encodeDeviceTroublePayload(DeviceTroublePayload *deviceTroublePayload)
{
    cJSON *json = NULL;
    if (deviceTroublePayload != NULL)
    {
        json = cJSON_CreateObject();
        cJSON_AddItemToObjectCS(json, ROOT_ID, cJSON_CreateString(deviceTroublePayload->rootId));
        cJSON_AddItemToObjectCS(json, OWNER_URI, cJSON_CreateString(deviceTroublePayload->ownerUri));
        cJSON_AddItemToObjectCS(json, RESOURCE_URI, cJSON_CreateString(deviceTroublePayload->resourceUri));
        cJSON_AddItemToObjectCS(json, DEVICE_CLASS, cJSON_CreateString(deviceTroublePayload->deviceClass));
    }

    return json;
}

/**
 * Decode device trouble structure from JSON
 *
 * @param deviceTroublePayloadJSON the JSON to decode
 * @return the structure data, or NULL if some error.  Caller must destroy this
 *
 * @see deviceTroublePayloadDestroy
 */
DeviceTroublePayload *decodeDeviceTroublePayload(cJSON *deviceTroublePayloadJSON)
{
    DeviceTroublePayload *deviceTroublePayload = NULL;
    if (deviceTroublePayloadJSON != NULL)
    {
        deviceTroublePayload = deviceTroublePayloadCreate();
        deviceTroublePayload->rootId = getCJSONString(deviceTroublePayloadJSON, ROOT_ID);
        deviceTroublePayload->ownerUri = getCJSONString(deviceTroublePayloadJSON, OWNER_URI);
        deviceTroublePayload->resourceUri = getCJSONString(deviceTroublePayloadJSON, RESOURCE_URI);
        deviceTroublePayload->deviceClass = getCJSONString(deviceTroublePayloadJSON, DEVICE_CLASS);
    }

    return deviceTroublePayload;
}

/**
 * Create an empty device trouble payload structure
 *
 * @return the structure data. Caller must destroy this
 *
 * @see deviceTroublePayloadDestroy
 */
DeviceTroublePayload *deviceTroublePayloadCreate()
{
    return (DeviceTroublePayload *)calloc(1, sizeof(DeviceTroublePayload));
}

/**
 * Destroy a device trouble payload structure
 *
 * @param deviceTroublePayload the payload to destroy
 */
void deviceTroublePayloadDestroy(DeviceTroublePayload *deviceTroublePayload)
{
    if (deviceTroublePayload != NULL)
    {
        free(deviceTroublePayload->rootId);
        free(deviceTroublePayload->ownerUri);
        free(deviceTroublePayload->resourceUri);
        free(deviceTroublePayload->deviceClass);

        free(deviceTroublePayload);
    }
}

/**
 * Check if two payloads are matching
 *
 * @param payload1 the first payload
 * @param payload2 the second payload
 * @return true if they match, false otherwise
 */
bool isMatchingDeviceTroublePayload(cJSON *payload1, cJSON *payload2)
{
    bool isMatching = false;
    if (payload1 != NULL && payload2 != NULL)
    {
        cJSON *resourceUri1 = cJSON_GetObjectItem(payload1, RESOURCE_URI);
        cJSON *resourceUri2 = cJSON_GetObjectItem(payload2, RESOURCE_URI);
        if (resourceUri1 != NULL && resourceUri2 != NULL)
        {
            char *val1 = cJSON_GetStringValue(resourceUri1);
            char *val2 = cJSON_GetStringValue(resourceUri2);
            if (val1 != NULL && val2 != NULL && strcmp(val1, val2) == 0)
            {
                isMatching = true;
            }
        }
        else if (resourceUri1 == NULL && resourceUri2 == NULL)
        {
            // both are missing the resourceURI.  quite possible this is an EXIT_ERROR or SWINGER_SHUTDOWN.
            // see if they are at least the same device.  we are assuming that whatever uses this already
            // validated it was the same trouble type & reason.  we're just making sure it's the same device.
            //
            cJSON *owner1 = cJSON_GetObjectItem(payload1, OWNER_URI);
            cJSON *owner2 = cJSON_GetObjectItem(payload2, OWNER_URI);
            if (owner1 != NULL && owner2 != NULL)
            {
                char *val1 = cJSON_GetStringValue(owner1);
                char *val2 = cJSON_GetStringValue(owner2);
                if (val1 != NULL && val2 != NULL && strcmp(val1, val2) == 0)
                {
                    isMatching = true;
                }
            }
        }
    }

    return isMatching;
}
