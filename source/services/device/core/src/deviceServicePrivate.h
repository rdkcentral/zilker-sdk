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
// Created by Thomas Lea on 7/27/15.
//

#ifndef ZILKER_DEVICESERVICEPRIVATE_H
#define ZILKER_DEVICESERVICEPRIVATE_H

#include <stdbool.h>
#include <deviceService/securityState.h>
#include <icIpc/baseEvent.h>
#include <securityService/securityService_pojo.h>
#include "deviceDriver.h"

bool deviceServiceInitialize(bool block);
void deviceServiceShutdown();

bool deviceServiceDeviceFound(DeviceFoundDetails* deviceFoundDetails, bool neverReject);

void deviceConfigured(icDevice* device);

/*
 * Get the age (in milliseconds) since the provided resource was last updated/sync'd with the device.
 *
 * @param deviceUuid - the universally unique identifier for the device
 * @param endpointId - the id of the desired endpoint
 * @param resourceId - the unique id of the desired resource
 * @param ageMillis  - the amount of time elapsed since the resource was last sync'd (valid if result is true)
 *
 * @returns true on success
 */
bool deviceServiceGetResourceAgeMillis(const char *deviceUuid,
                                       const char *endpointId,
                                       const char *resourceId,
                                       uint64_t *ageMillis);

/*
 * Retrieve an icDeviceResource by id.
 *
 * Caller is responsible for calling resourceDestroy() on the non-NULL result
 *
 * @param deviceUuid - the universally unique identifier for the device
 * @param endpointId - the id of the desired endpoint
 * @param resourceId - the unique id of the desired resource
 *
 * @returns the icDeviceResource or NULL if not found
 */
icDeviceResource* deviceServiceGetResourceById(const char* deviceUuid, const char* endpointId, const char* resourceId);

/*
 * Retrieve an icDeviceResource by id.  This will not look on any endpoints, but only on the device itself
 *
 * Caller should NOT destroy the icDeviceResource on the non-NULL result, as it belongs to the icDevice object
 *
 * @param device - the device
 * @param resourceId - the unique id of the desired resource
 *
 * @returns the icDeviceResource or NULL if not found
 */
icDeviceResource* deviceServiceFindDeviceResourceById(icDevice* device, const char* resourceId);

void updateResource(const char* deviceUuid,
                    const char* endpointId,
                    const char* resourceId,
                    const char* newValue,
                    cJSON* metadata);

void setMetadata(const char* deviceUuid, const char* endpointId, const char* name, const char* value);
char* getMetadata(const char* deviceUuid, const char* endpointId, const char* name);
void setBooleanMetadata(const char* deviceUuid, const char* endpointId, const char* name, bool value);
bool getBooleanMetadata(const char* deviceUuid, const char* endpointId, const char* name);
void deviceServiceProcessDeviceDescriptors();
void timeZoneChanged(const char* timeZone);
void processBlacklistedDevices(const char *propValue);
char* getMetadataUri(const char* deviceUuid, const char* endpointId, const char* name);

/**
 * Caller must call resourceOperationResultDestroy
 */

/**
 * Read the exit delay from the security service.
 * @note In the event of failure, this will return 0.
 * @return the exit delay setting, in seconds
 */
uint32_t deviceServiceGetSecurityExitDelay(void);

/**
 * Ask the security service to arm or disarm the system to match a desired state
 */
ArmDisarmNotification deviceServiceRequestPanelStatusChange(PanelStatus desiredState, const char *accessCode, RequestSource source);

 /**
  * Ask the security service to toggle bypass a zone.
  * @param zoneNumber The zone number to bypass
  * @param code The user access code
  * @return true when the bypass was toggled.
  */
bool deviceServiceRequestBypassZoneToggle(uint32_t zoneNumber, const char *code, RequestSource source);

PanelStatus deviceServiceConvertSystemPanelStatus(systemPanelStatus *sysPanelStatus, alarmReasonType alarmReason, alarmPanicType panicReason);
SecurityIndication deviceServiceConvertSystemIndication(indicationType securityServiceIndication);

/**
 * Get the system security state
 * @return a pointer to a heap allocated SecurityState object.
 */
SecurityState *deviceServiceGetSecurityState(void);

/**
 * Set devices' OTA firmware upgrade delay
 * @param delaySeconds Time in seconds
 */
void deviceServiceSetOtaUpgradeDelay(uint32_t delaySeconds);

/**
 * Add an endpoint to an existing device, persist to database and send out events.  The endpoint provided
 * must already be added to the device provided.
 * @param device
 * @param endpoint
 */
void deviceServiceAddEndpoint(icDevice *device, icDeviceEndpoint *endpoint);

/**
 * Re-enable an endpoint for a device, persist to database and send out events.
 * @param device the device
 * @param endpoint the endpoint
 */
void deviceServiceUpdateEndpoint(icDevice *device, icDeviceEndpoint *endpoint);

/*
 * Tells all of the subsystem to enter LPM
 */
void deviceServiceEnterLowPowerMode();

/*
 * Tells all of the subsystem to exit LPM
 */
void deviceServiceExitLowPowerMode();

/*
 * Determines if system is in LPM
 */
bool deviceServiceIsInLowPowerMode();

/**
 * Indicate that we have successfully communicated with the given device and update its timestamp.
 * @param deviceUuid
 */
void updateDeviceDateLastContacted(const char *deviceUuid);

/**
 * Retrieve the last contact date for a device
 *
 * @param deviceUuid
 *
 * @return the date of last contact in millis
 */
uint64_t getDeviceDateLastContacted(const char *deviceUuid);

/**
 * Retrieve a string representing the current time.
 *
 * @return a timestamp string.  Caller frees.
 */
char *getCurrentTimestampString();

#endif //ZILKER_DEVICESERVICEPRIVATE_H
