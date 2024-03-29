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
// Created by tlea on 9/23/19.
//

#ifndef ZILKER_OTAUPGRADECLUSTER_H
#define ZILKER_OTAUPGRADECLUSTER_H

#include <icBuildtime.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "zigbeeCluster.h"

ZigbeeCluster *otaUpgradeClusterCreate(void);

/**
 * Notify a device that uses the OTA Upgrade cluster that we have a new firmware image for them.
 * @param eui64
 * @param endpointId
 *
 * @return true on success
 */
bool otaUpgradeClusterImageNotify(uint64_t eui64, uint8_t endpointId);

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE

#endif //ZILKER_OTAUPGRADECLUSTER_H
