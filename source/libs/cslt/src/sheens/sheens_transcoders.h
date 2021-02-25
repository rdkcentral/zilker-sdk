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
// Created by Boyd, Weston on 4/17/18.
//

#ifndef ZILKER_SHEENS_TRANSCODERS_H
#define ZILKER_SHEENS_TRANSCODERS_H

/* DeviceSerivceResourceUpdatedEvent : { "resource": "DSResource": { "id", "value", "ownerClass" } } */
#define DEVICE_RESOURCE_UPDATED_EVENT_NAME      "DeviceServiceResourceUpdatedEvent"
#define DEVICE_RESOURCE_UPDATED_EVENT_RESOURCE  "resource"
#define DS_RESOURCE                             "DSResource"
#define DS_RESOURCE_ID                          "id"
#define DS_RESOURCE_VALUE                       "value"
#define DS_RESOURCE_OWNER_CLASS                 "ownerClass"
#define DS_RESOURCE_ROOT_DEVICE_CLASS           "rootDeviceClass"

#define AUTOMATION_EVENT_NAME                   "automationEvent"
#define AUTOMATION_EVENT_RULE_ID                "ruleId"

void sheens_transcoder_init(icHashMap* settings);

/**
 * Map a server identifier to a device service id and endpoint id
 * @param deviceId the input server device id to map
 * @param mappedDeviceId the mapped device id
 * @param mappedEndpointId the mapped endpoint id
 * @return the url
 */
bool sheens_transcoder_map_device_id(const char* deviceId, char **mappedDeviceId, char **mappedEndpointId);

#endif //ZILKER_SHEENS_TRANSCODERS_H
