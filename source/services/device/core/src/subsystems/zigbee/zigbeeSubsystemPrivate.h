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

/*
 * These are called internally by the event handler.
 *
 * Created by Thomas Lea on 3/29/16.
 */

#ifndef ZILKER_ZIGBEESUBSYSTEMPRIVATE_H_H
#define ZILKER_ZIGBEESUBSYSTEMPRIVATE_H_H

#include <stdbool.h>
#include <stdint.h>

void zigbeeSubsystemDeviceDiscovered(IcDiscoveredDeviceDetails *details);

void zigbeeSubsystemAttributeReportReceived(ReceivedAttributeReport* report);

void zigbeeSubsystemClusterCommandReceived(ReceivedClusterCommand* command);

void zigbeeSubsystemDeviceLeft(uint64_t eui64);

void zigbeeSubsystemDeviceRejoined(uint64_t eui64, bool isSecure);

void zigbeeSubsystemLinkKeyUpdated(uint64_t eui64, bool isUsingHashBasedKey);

void zigbeeSubsystemApsAckFailure(uint64_t eui64);

void zigbeeSubsystemDeviceFirmwareUpgrading(uint64_t eui64);
void zigbeeSubsystemDeviceFirmwareUpgradeCompleted(uint64_t eui64);
void zigbeeSubsystemDeviceFirmwareUpgradeFailed(uint64_t eui64);
void zigbeeSubsystemDeviceFirmwareVersionNotify(uint64_t eui64, uint32_t currentVersion);

/*
 * Reconfigure the network with the specified parameters
 */
bool zigbeeSubsystemReconfigureNetwork(uint64_t eui64, const char *networkBlob, const char *cpeId);

bool zigbeeSubsystemClaimDiscoveredDevice(IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator);

/**
 * Add a premature cluster command
 * @param command the command to add
 */
void zigbeeSubsystemAddPrematureClusterCommand(const ReceivedClusterCommand *command);

#endif //ZILKER_ZIGBEESUBSYSTEMPRIVATE_H_H
