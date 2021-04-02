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
// Created by tlea on 2/25/19.
//

#ifndef ZILKER_LEGACYSECURITYCLUSTER_H
#define ZILKER_LEGACYSECURITYCLUSTER_H

#include <icBuildtime.h>
#include <zigbeeLegacySecurityCommon/uc_common.h>
#include <device/icDevice.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "zigbeeCluster.h"

typedef struct
{
    /**
     * Handle a keypad message. It will indicate the requested action and a 4 character numeric code if applicable
     * @param eui64
     * @param endpointid
     * @param keypadMessage
     * @param ctx
     */
    void (*handleKeypadMessage)(uint64_t eui64,
                                uint8_t endpointId,
                                const uCKeypadMessage *keypadMessage,
                                const void *ctx);

    /**
     * Handle a keyfob message. It will indicate the requested action as a LegacyActionButton in 'buttons'
     * @param eui64
     * @param endpointid
     * @param keyfobMessage
     * @param ctx
     */
    void (*handleKeyfobMessage)(uint64_t eui64,
                                uint8_t endpointId,
                                const uCKeyfobMessage *keyfobMessage,
                                const void *ctx);

} SecurityControllerCallbacks;

typedef struct
{
    /**
     * Handle a device status change, e.g., to update profile-specific resources
     * @note common resources will be updated before this is called.
     */
    void (*deviceStatusChanged)(uint64_t eui64,
                                uint8_t endpointId,
                                const uCStatusMessage *status,
                                const void *ctx);

    void (*firmwareVersionReceived)(uint64_t eui64,
                                    uint8_t endpointId,
                                    uint32_t firmwareVersion,
                                    const void *ctx);

    /**
     * Inform whether or not an upgrade is in progress.
     *
     * @param eui64
     * @param inProgress
     * @param ctx
     */
    void (*upgradeInProgress)(uint64_t eui64,
                              bool inProgress,
                              const void *ctx);

    /**
     * Return false if godparent ping is NOT supported (defaults to true when not implemented)
     * @param details the current device's details
     * @param ctx the callbackContext this cluster was created with
     */
    bool (*isGodparentPingSupported)(const LegacyDeviceDetails *details, const void *ctx);

    const SecurityControllerCallbacks *securityControllerCallbacks;

} LegacySecurityClusterCallbacks;

ZigbeeCluster *legacySecurityClusterCreate(const LegacySecurityClusterCallbacks *callbacks,
                                           const DeviceServiceCallbacks *deviceServiceCallbacks,
                                           const void *callbackContext);

/**
 * Load legacy device info and make the cluster ready to use with that device
 * @param cluster This legacy cluster instance
 * @param deviceService
 * @param eui64 the device address
 */
void legacySecurityClusterDevicesLoaded(const ZigbeeCluster *cluster,
                                        const DeviceServiceCallbacks *deviceService,
                                        icLinkedList *devices);

/**
 * TODO: remove
 */
void legacySecurityClusterDeviceLoaded(const ZigbeeCluster *cluster,
                                       const DeviceServiceCallbacks *deviceService,
                                       uint64_t eui64);

void legacySecurityClusterDeviceRemoved(const ZigbeeCluster *cluster,
                                        uint64_t eui64);

bool legacySecurityClusterConfigureDevice(const ZigbeeCluster *cluster,
                                          uint64_t eui64,
                                          icDevice *device,
                                          const DeviceDescriptor *deviceDescriptor);

/**
 * Fetch the LegacyDeviceDetails for the specified device or NULL if not found.
 * When the result is not NULL, call legacySecurityClusterReleaseDetails when finished with it.
 */
LegacyDeviceDetails *legacySecurityClusterAcquireDetails(const ZigbeeCluster *cluster, uint64_t eui64);

/**
 * Get the legacy device endpoint id
 * @return A positive number when the device details exist
 */
uint8_t legacySecurityClusterGetEndpointId(const ZigbeeCluster *cluster, uint64_t eui64);

/**
 * Get a copy of the legacy device details
 * Call legacyDeviceDetailsDestroy on the result when finished.
 * @see legacyDeviceDetailsDestroy
 */
LegacyDeviceDetails *legacySecurityClusterGetDetailsCopy(const ZigbeeCluster *cluster, uint64_t eui64);

/**
 * Release device details acquired by legacySecurityClusterAcquireDetails
 */
void legacySecurityClusterReleaseDetails(const ZigbeeCluster *cluster);

/**
 * If the discovered device should be owned by this cluster instance, it will return true.
 *
 * @param cluster
 * @param details
 * @param deviceTypeInclusionSet a set of uCDeviceType that must contain device types to accept (or NULL)
 * @Param deviceTypeExclusionSet a set of uCDeviceType that we want to ignore (or NULL)
 * @return true if this cluster instance claims this device
 */
bool legacySecurityClusterClaimDevice(const ZigbeeCluster *cluster,
                                      IcDiscoveredDeviceDetails *details,
                                      icHashMap *deviceTypeInclusionSet,
                                      icHashMap *deviceTypeExclusionSet);

/**
 * Popuplate initial resource values for a legacy security device
 * @param cluster
 * @param eui64
 * @param device
 * @param discoveredDeviceDetails
 * @param initialResourceValues
 * @return
 */
bool legacySecurityClusterFetchInitialResourceValues(const ZigbeeCluster *cluster,
                                                     uint64_t eui64,
                                                     icDevice *device,
                                                     IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                                     icInitialResourceValues *initialResourceValues);

/**
 * Register resources for a legacy security device
 * @param cluster
 * @param eui64
 * @param device
 * @param discoveredDeviceDetails
 * @param initialResourceValues
 * @return
 */
bool legacySecurityClusterRegisterResources(const ZigbeeCluster *cluster,
                                            uint64_t eui64,
                                            icDevice *device,
                                            IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                            icInitialResourceValues *initialResourceValues);

/**
 * Initiate a firmware upgrade for the specified device as soon as we are ready.
 *
 * @param cluster
 * @param eui64
 * @param dd
 */
void legacySecurityClusterUpgradeFirmware(const ZigbeeCluster *cluster, uint64_t eui64, const DeviceDescriptor *dd);

/**
 * Updates legacy details when an upgrade fails for a legacy security device
 *
 * @param cluster
 * @param eui64
 */
void legacySecurityClusterHandleFirmwareUpgradeFailed(const ZigbeeCluster *cluster, uint64_t eui64);

/**
 * Convert a code string to an array of numbers
 * @param code
 * @return an array of 4 numbers to be freed by the caller
 */
uint8_t *legacySecurityClusterStringToCode(const char *code);

/**
 * Set a repeater warning (tone/strobe)
 * @param cluster
 * @param eui64
 * @param message
 * @return
 */
bool legacySecurityClusterRepeaterSetWarning(ZigbeeCluster *cluster, uint64_t eui64, const uCWarningMessage *message);

/**
 * Send the DEVICE_REMOVE command which will default the device (PIM only?) and it will leave the network.
 *
 * @param eui64 the EUI64 of the device to send the message to
 * @param endpointId the endpoint to send the message to
 */
bool legacySecurityClusterSendDeviceRemove(uint64_t eui64, uint8_t endpointId);

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE

#endif //ZILKER_LEGACYSECURITYCLUSTER_H
