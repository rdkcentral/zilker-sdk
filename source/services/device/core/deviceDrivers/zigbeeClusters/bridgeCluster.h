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
// Created by mkoch201 on 3/18/19.
//

#ifndef ZILKER_BRIDGECLUSTER_H
#define ZILKER_BRIDGECLUSTER_H

#include <icBuildtime.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "zigbeeCluster.h"


typedef struct
{
    void (*refreshRequested)(uint64_t eui64,
                             uint8_t endpointId,
                             const void *ctx);
    void (*refreshCompleted)(uint64_t eui64,
                             uint8_t endpointId,
                             const void *ctx);
    void (*bridgeTamperStatusChanged)(uint64_t eui64,
                                      uint8_t endpointId,
                                      const void *ctx,
                                      bool isTampered);
} BridgeClusterCallbacks;

ZigbeeCluster *bridgeClusterCreate(const BridgeClusterCallbacks *callbacks, const void *callbackContext);

bool bridgeClusterRefresh(uint64_t eui64, uint8_t endpointId);

bool bridgeClusterStartConfiguration(uint64_t eui64, uint8_t endpointId);

bool bridgeClusterStopConfiguration(uint64_t eui64, uint8_t endpointId);

bool bridgeClusterReset(uint64_t eui64, uint8_t endpointId);

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE

#endif //ZILKER_BRIDGECLUSTER_H
