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

#ifndef ZILKER_SENSORTROUBLEEVENTHELPER_H
#define ZILKER_SENSORTROUBLEEVENTHELPER_H

#include <inttypes.h>
#include <securityService/securityService_pojo.h>
#include "deviceTroubleEventHelper.h"

/**
 * Sensor structure is extension of device structure.
 * Stored in the troubleObj->extra object and generally persisted somewhere.
 *
 * This particular object should be assumed when troubleObj->troubleType == TROUBLE_TYPE_DEVICE
 * and the deviceClass == "sensor".
 *
 * Due to the way this is stored/read, it's safe to first parse the JSON as a DeviceTroublePayload,
 * then can re-parse as a SensorTroublePayload (once the deviceClass is realized)
 */
typedef struct {
    DeviceTroublePayload *deviceTrouble;

    uint32_t zoneNumber;
    securityZoneType zoneType;

    char* currentSystemMode;

    alarmStatusType alarmStatus;
    armModeType armMode;
} SensorTroublePayload;

/**
 * Encode sensor trouble structure into JSON
 *
 * @param sensorTroublePayload the payload to encode
 * @return JSON version of the payload
 */
cJSON *encodeSensorTroublePayload(SensorTroublePayload *sensorTroublePayload);

/**
 * Decode sensor trouble structure from JSON
 *
 * @param sensorTroublePayloadJSON the JSON to decode
 * @return the structure data, or NULL if some error.  Caller must destroy this
 *
 * @see sensorTroublePayloadDestroy
 */
SensorTroublePayload *decodeSensorTroublePayload(cJSON *sensorTroublePayloadJSON);

/**
 * Create an empty sensor trouble payload structure
 *
 * @return the structure data. Caller must destroy this
 *
 * @see sensorTroublePayloadDestroy
 */
SensorTroublePayload *sensorTroublePayloadCreate();

/**
 * Destroy a sensor trouble payload structure
 *
 * @param sensorTroublePayload the payload to destroy
 */
void sensorTroublePayloadDestroy(SensorTroublePayload *sensorTroublePayload);

#endif //ZILKER_SENSORTROUBLEEVENTHELPER_H
