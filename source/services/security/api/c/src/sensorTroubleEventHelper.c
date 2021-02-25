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

#include <stdlib.h>
#include <jsonHelper/jsonHelper.h>
#include <securityService/sensorTroubleEventHelper.h>

#define ZONE_NUMBER "zoneNumber"
#define ZONE_TYPE_KEY "zoneType"
#define CURRENT_SYSTEMSTATUS_KEY "currentSystemMode"
#define ALARM_ARM_TYPE_KEY "alarmArmType"
#define ALARM_STATUS_TYPE_KEY "alarmStatusType"

/**
 * Encode sensor trouble structure into JSON
 *
 * @param sensorTroublePayload the payload to encode
 * @return JSON version of the payload
 */
cJSON *encodeSensorTroublePayload(SensorTroublePayload *sensorTroublePayload)
{
    cJSON *json = NULL;
    if (sensorTroublePayload != NULL && sensorTroublePayload->deviceTrouble != NULL)
    {
        // first encode the DeviceTroublePayload
        json = encodeDeviceTroublePayload(sensorTroublePayload->deviceTrouble);

        // add our attributes at the same node-level.  this allows us to generically parse
        // the JSON string as a DeviceTroublePayload first
        cJSON_AddItemToObjectCS(json, ZONE_NUMBER, cJSON_CreateNumber(sensorTroublePayload->zoneNumber));
        cJSON_AddItemToObjectCS(json, ZONE_TYPE_KEY, cJSON_CreateNumber(sensorTroublePayload->zoneType));
        cJSON_AddItemToObjectCS(json, ALARM_ARM_TYPE_KEY, cJSON_CreateNumber(sensorTroublePayload->armMode));
        cJSON_AddItemToObjectCS(json, ALARM_STATUS_TYPE_KEY, cJSON_CreateNumber(sensorTroublePayload->alarmStatus));
        cJSON_AddItemToObjectCS(json, CURRENT_SYSTEMSTATUS_KEY, cJSON_CreateString(sensorTroublePayload->currentSystemMode));
    }

    return json;
}

/**
 * Decode sensor trouble structure from JSON
 *
 * @param sensorTroublePayloadJSON the JSON to decode
 * @return the structure data, or NULL if some error.  Caller must destroy this
 *
 * @see sensorTroublePayloadDestroy
 */
SensorTroublePayload *decodeSensorTroublePayload(cJSON *sensorTroublePayloadJSON)
{
    SensorTroublePayload *payload = NULL;
    if (sensorTroublePayloadJSON != NULL)
    {
        int value;

        // first decode the DeviceTroublePayload
        payload = sensorTroublePayloadCreate();
        deviceTroublePayloadDestroy(payload->deviceTrouble);
        payload->deviceTrouble = decodeDeviceTroublePayload(sensorTroublePayloadJSON);

        // now extract our attributes from the same node-level
        getCJSONInt(sensorTroublePayloadJSON, ZONE_NUMBER, &value);
        payload->zoneNumber = (uint32_t) value;

        getCJSONInt(sensorTroublePayloadJSON, ZONE_TYPE_KEY, &value);
        payload->zoneType = (securityZoneType) value;

        getCJSONInt(sensorTroublePayloadJSON, ALARM_ARM_TYPE_KEY, &value);
        payload->armMode = (armModeType) value;

        getCJSONInt(sensorTroublePayloadJSON, ALARM_STATUS_TYPE_KEY, &value);
        payload->alarmStatus = (alarmStatusType) value;

        payload->currentSystemMode = getCJSONString(sensorTroublePayloadJSON, CURRENT_SYSTEMSTATUS_KEY);
    }

    return payload;
}

/**
 * Create an empty sensor trouble payload structure
 *
 * @return the structure data. Caller must destroy this
 *
 * @see sensorTroublePayloadDestroy
 */
SensorTroublePayload *sensorTroublePayloadCreate()
{
    SensorTroublePayload *payload = (SensorTroublePayload *)calloc(1, sizeof(SensorTroublePayload));
    payload->deviceTrouble = deviceTroublePayloadCreate();

    return payload;
}

/**
 * Destroy a sensor trouble payload structure
 *
 * @param sensorTroublePayload the payload to destroy
 */
void sensorTroublePayloadDestroy(SensorTroublePayload *sensorTroublePayload)
{
    if (sensorTroublePayload != NULL)
    {
        deviceTroublePayloadDestroy(sensorTroublePayload->deviceTrouble);

        free(sensorTroublePayload->currentSystemMode);
        free(sensorTroublePayload);
    }
}
