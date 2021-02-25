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

#ifndef ZILKER_CAMERATROUBLEEVENTHELPER_H
#define ZILKER_CAMERATROUBLEEVENTHELPER_H

#include "deviceTroubleEventHelper.h"

#define CAMERA_TROUBLE_REASON_COMMFAIL "commFail"
#define CAMERA_TROUBLE_REASON_AUTHFAIL "authFail"

/**
 * Camera structure is extension of device structure.
 * Stored in the troubleObj->extra object and generally persisted somewhere.
 *
 * This particular object should be assumed when troubleObj->troubleType == TROUBLE_TYPE_DEVICE
 * and the deviceClass == "camera"
 *
 * Due to the way this is stored/read, it's safe to first parse the JSON as a DeviceTroublePayload,
 * then can re-parse as a CameraTroublePayload (once the deviceClass is realized)
 */
typedef struct {
    DeviceTroublePayload *deviceTrouble;
    char *reason;
} CameraTroublePayload;

/**
 * Encode camera trouble structure into JSON
 *
 * @param cameraTroublePayload the payload to encode
 * @return JSON version of the payload
 */
cJSON *encodeCameraTroublePayload(CameraTroublePayload *cameraTroublePayload);

/**
 * Decode camera trouble structure from JSON
 *
 * @param cameraTroublePayloadJSON the JSON to decode
 * @return the structure data, or NULL if some error.  Caller must destroy this
 *
 * @see cameraTroublePayloadDestroy
 */
CameraTroublePayload *decodeCameraTroublePayload(cJSON *cameraTroublePayloadJSON);

/**
 * Create an empty camera trouble payload structure
 *
 * @return the structure data. Caller must destroy this
 *
 * @see cameraTroublePayloadDestroy
 */
CameraTroublePayload *cameraTroublePayloadCreate();

/**
 * Destroy a camera trouble payload structure
 *
 * @param cameraTroublePayload the payload to destroy
 */
void cameraTroublePayloadDestroy(CameraTroublePayload *cameraTroublePayload);

#endif //ZILKER_CAMERATROUBLEEVENTHELPER_H
