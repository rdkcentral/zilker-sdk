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
// Created by mkoch201 on 5/16/19.
//

#ifndef ZILKER_USERVERDEVICEHELPER_H
#define ZILKER_USERVERDEVICEHELPER_H

#include <stdbool.h>

/**
 * Convert a SMAP DeviceId to a device UUID/endpointId combo.  Handles the 5 different formats:
 * PreZdif(zigbee) - eui64 in decimal format
 * Zdif(zigbee) - eui64.endpointId where eui64 is 0 padded 16 digit hex, and endpointId is decimal
 * Zilker(zigbee) - premiseId.eui64 where premiseId is decimal number and eui64 is 0 padded 16 digit hex
 * PreZilker(camera) - premiseId.cameraId where cameraId is a decimal number
 * Zilker(camera) - premiseId.cameraId where cameraId is camera mac address without colons
 * @param input the SMAP device ID
 * @param deviceId the device UUID will be populated here, caller must free
 * @param endpointId the endpointId will be populated here(may be wildcard), caller must free
 * @return true if success, false if valid isn't in one of the known formats
 */
bool mapUserverDeviceId(const char *input, char **deviceId, char **endpointId);

#endif //ZILKER_USERVERDEVICEHELPER_H
