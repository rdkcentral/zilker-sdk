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

#ifndef ZILKER_BASICCLUSTER_H
#define ZILKER_BASICCLUSTER_H

#include <icBuildtime.h>
#include "zigbeeCluster.h"

#define REBOOT_REASON_DEFAULT 0xFF

static const char* basicClusterRebootReasonLabels[] = {
    "UNKNOWN",
    "BATTERY",
    "BROWNOUT",
    "WATCHDOG",
    "RESET_PIN",
    "MEMORY_HARDWARE_FAULT",
    "SOFTWARE_EXCEPTION",
    "OTA_BOOTLOAD_SUCCESS",
    "SOFTWARE_RESET",
    "POWER_BUTTON",
    "TEMPERATURE",
    "BOOTLOAD_FAILURE"
};
typedef enum {
    UNKNOWN,
    BATTERY,
    BROWNOUT,
    WATCHDOG,
    RESET_PIN,
    MEMORY_HARDWARE_FAULT,
    SOFTWARE_EXCEPTION,
    OTA_BOOTLOAD_SUCCESS,
    SOFTWARE_RESET,
    POWER_BUTTON,
    TEMPERATURE,
    BOOTLOAD_FAILURE
} basicClusterRebootReason;

typedef struct
{
    void (*rebootReasonChanged)(void *ctx, uint64_t eui64, uint8_t endpointId, basicClusterRebootReason reason);
} BasicClusterCallbacks;

ZigbeeCluster *basicClusterCreate(const BasicClusterCallbacks *callbacks, void *callbackContext);

/**
 * Performs a device reboot.  This is a manufacturer specific extension to the Basic cluster and is not
 * available on all devices.
 *
 * @param eui64
 * @param endpointId
 * @param mfgId
 * @return
 */
bool basicClusterRebootDevice(uint64_t eui64, uint8_t endpointId, uint16_t mfgId);

/**
 * Set whether or not to configure reboot reason reports. By default, reports are disabled.
 * @param deviceConfigurationContext the configuration context
 * @param configure true to configure, false otherwise
 */
void basicClusterSetConfigureRebootReason(const DeviceConfigurationContext *deviceConfigurationContext, bool configure);

/**
 * Defaults the reboot reason
 *
 * @param eui64
 * @param endpointId
 *
 * @return 0 if success, failure otherwise
 */
int basicClusterResetRebootReason(uint64_t eui64, uint8_t endPointId);

#endif //ZILKER_BASICCLUSTER_H
