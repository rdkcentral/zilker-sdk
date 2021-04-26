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
// Created by tlea on 2/18/19.
//

#ifndef ZILKER_POWERCONFIGURATIONCLUSTER_H
#define ZILKER_POWERCONFIGURATIONCLUSTER_H

#include <icBuildtime.h>
#include "zigbeeCluster.h"

typedef struct
{
    void (*batteryVoltageUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t decivolts);
    void (*batteryPercentageRemainingUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t percent);
    void (*batteryChargeStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isLow);
    void (*batteryBadStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBad);
    void (*batteryMissingStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isMissing);
    void (*batteryTemperatureStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isHigh);
    void (*acMainsStatusUpdated)(void *ctx, uint64_t eui64, uint8_t endpointId, bool isAcMainsConnected);
    void (*batteryRechargeCyclesChanged)(void *ctx, uint64_t eui64, uint16_t rechargeCycles);
} PowerConfigurationClusterCallbacks;

ZigbeeCluster *powerConfigurationClusterCreate(const PowerConfigurationClusterCallbacks *callbacks, void *callbackContext);

/**
 * Set whether or not to set a binding on this cluster.  By default we bind the cluster.
 * @param deviceConfigurationContext the configuration context
 * @param bind true to bind or false not to
 */
void powerConfigurationClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind);

/**
 * Retrieves the current battery voltage from the device.
 * @param eui64
 * @param endpointId
 * @param decivolts
 * @return
 */
bool powerConfigurationClusterGetBatteryVoltage(uint64_t eui64, uint8_t endpointId, uint8_t *decivolts);

/**
 * Retrieves the remaining battery life as a percentage of the full battery capacity.
 * @param eui64
 * @param endpointId
 * @param decivolts
 * @return
 */
bool powerConfigurationClusterGetBatteryPercentageRemaining(uint64_t eui64, uint8_t endpointId, uint8_t *percent);

/**
 * Retrieves the current ac mains voltage from the device.
 * @param eui64
 * @param endpointId
 * @param decivolts 
 * @return
 */
bool powerConfigurationClusterGetMainsVoltage(uint64_t eui64, uint8_t endpointId, uint16_t *decivolts);

/**
 * Set whether or not to configure battery alarm state reporting.  By default battery alarm state reporting will be
 * configured unless explicitly told not to
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryAlarmState(const DeviceConfigurationContext *deviceConfigurationContext, bool configure);

/**
 * Set whether or not to configure battery alarm mask.  By default battery alarm mask will not be configured unless
 * explicitly told to
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryAlarmMask(const DeviceConfigurationContext *deviceConfigurationContext, bool configure);

/**
 * Set whether or not to configure battery voltage reporting.  By default battery voltage will not be configured unless
 * explicitly told to
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryVoltage(const DeviceConfigurationContext *deviceConfigurationContext, bool configure);

/**
 * Set whether or not to configure battery percent remaining reports.  By default, reports are disabled.
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryPercentage(const DeviceConfigurationContext *deviceConfigurationContext, bool configure);

/**
 * Set whether or not to configure battery recharge cycle reports. By default, reports are disabled.
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void powerConfigurationClusterSetConfigureBatteryRechargeCycles(const DeviceConfigurationContext *deviceConfigurationContext, bool configure);

/**
 * Set the max interval for the battery voltage configuration. By default, it's 27 minutes.
 * @param deviceConfigurationContext the configuration context
 * @param max interval
 */
void powerConfigurationClusterSetConfigureBatteryVoltageMaxInterval(const DeviceConfigurationContext *deviceConfigurationContext, uint64_t interval);

/**
 * Gives initial recharge cycle value
 *
 * @param eui64
 * @param endpointId
 * @param rechargeCycles
 *
 * @return 0 if read was successful, -1 otherwise
 */

int powerConfigurationClusterReadBatteryRechargeCyclesInitialValue(uint64_t eui64, uint8_t endpointId, uint64_t *rechargeCycles);

#endif //ZILKER_POWERCONFIGURATIONCLUSTER_H
