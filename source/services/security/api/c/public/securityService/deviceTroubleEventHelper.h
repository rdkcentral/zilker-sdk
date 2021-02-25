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

#ifndef ZILKER_DEVICETROUBLEEVENTHELPER_H
#define ZILKER_DEVICETROUBLEEVENTHELPER_H

#include <cjson/cJSON.h>
#include <stdbool.h>

/**
 * Base structure for device data defined in deviceTrouble.json schema.
 * Stored in the troubleObj->extra object and generally persisted somewhere.
 * This particular object should be assumed when troubleObj->troubleType == TROUBLE_TYPE_DEVICE
 * and the deviceClass != "camera" and != "sensor".
 *
 * Think of this as a pointer back to the device that the trouble is
 * associated with (has enough info so we can get the device object).
 */
typedef struct {
    char *rootId;       // deviceId
    char *ownerUri;     // device URI
    char *resourceUri;  // URI if the resource that is troubled (ex: /e0606614bea2/r/communicationFailure)
    char *deviceClass;  // same string as device->deviceClass   (ex: camera, sensor, thermostat)
} DeviceTroublePayload;

/**
 * Encode device trouble structure into JSON
 *
 * @param deviceTroublePayload the payload to encode
 * @return JSON version of the payload
 */
cJSON *encodeDeviceTroublePayload(DeviceTroublePayload *deviceTroublePayload);

/**
 * Decode device trouble structure from JSON
 *
 * @param deviceTroublePayloadJSON the JSON to decode
 * @return the structure data, or NULL if some error.  Caller must destroy this
 *
 * @see deviceTroublePayloadDestroy
 */
DeviceTroublePayload *decodeDeviceTroublePayload(cJSON *deviceTroublePayloadJSON);

/**
 * Create an empty device trouble payload structure
 *
 * @return the structure data. Caller must destroy this
 *
 * @see deviceTroublePayloadDestroy
 */
DeviceTroublePayload *deviceTroublePayloadCreate();

/**
 * Destroy a device trouble payload structure
 *
 * @param deviceTroublePayload the payload to destroy
 */
void deviceTroublePayloadDestroy(DeviceTroublePayload *deviceTroublePayload);

/**
 * Check if two payloads are matching
 *
 * @param payload1 the first payload
 * @param payload2 the second payload
 * @return true if they match, false otherwise
 */
bool isMatchingDeviceTroublePayload(cJSON *payload1, cJSON *payload2);

#endif //ZILKER_DEVICETROUBLEEVENTHELPER_H
