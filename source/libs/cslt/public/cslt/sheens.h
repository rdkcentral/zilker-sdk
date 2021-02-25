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
// Created by Boyd, Weston on 4/29/18.
//

#ifndef ZILKER_CSLT_SHEENS_H
#define ZILKER_CSLT_SHEENS_H

#include <stdbool.h>

#define TRANSCODER_NAME_SHEENS "sheens"

#define SHEENS_TRANSCODER_SETTING_ACTION_LIST_DIR "actionListDir"
#define SHEENS_TRANSCODER_DEVICE_ID_MAPPER "deviceIdMapper"

// Function definition for deviceIdMapper
typedef bool (*DeviceIdMapperFunc)(const char* deviceId, char **mappedDeviceId, char **mappedEndpointId);

#endif //ZILKER_CSLT_SHEENS_H
