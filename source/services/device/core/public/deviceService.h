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

#ifndef ZILKER_DEVICESERVICE_H
#define ZILKER_DEVICESERVICE_H

#include <icTypes/icLinkedList.h>
#include <device/icDevice.h>
#include <device/icDeviceResource.h>
#include <device/icDeviceEndpoint.h>
#include <deviceDescriptors.h>
#include "propsMgr/propsService_event.h"

typedef enum {
    POWER_EVENT_AC_LOST,
    POWER_EVENT_AC_RESTORED,
    POWER_EVENT_LPM_ENTER,
    POWER_EVENT_LPM_EXIT
} DeviceServiceSystemPowerEventType;

static const char * const DeviceServiceSystemPowerEventTypeLabels[] =
        {
                "AC_LOST",
                "AC_RESTORED",
                "LPM_ENTER",
                "LPM_EXIT",
        };

typedef struct
{
    bool zigbeeReady; // true if the zigbee subsystem is ready
    icLinkedList *supportedDeviceClasses; // the list of currently supported device classes
    bool discoveryRunning; // true if discovery is actively running
    icLinkedList *discoveringDeviceClasses; // the list of device classes currently being discovered (or empty for all)
    uint32_t discoveryTimeoutSeconds; // the number of seconds the discovery was requested to run (or 0 for forever)
} DeviceServiceStatus;

/*
 * Restore configuration data from another directory.  This is the standard RMA restore case.
 *
 * @param tempRestoreDir the directory containing the config files to restore from
 * @param dynamicConfigPath the current location to store config files
 *
 * @returns true on success
 */
bool deviceServiceRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath);

/*
 * For each of the provided device classes, find registered device drivers and instruct them to start discovery.
 *
 * @param deviceClasses - a list of device classes to discover
 * @param timeoutSeconds - the number of seconds to perform discovery before automatically stopping
 * @param findOrphanedDevices - if true, limit discovery to orphaned previously paired devices
 *
 * @returns true if at least one device driver successfully started discovery.
 */
bool deviceServiceDiscoverStart(icLinkedList* deviceClasses, uint16_t timeoutSeconds, bool findOrphanedDevices);

/*
 * Stop any discovery in progress.
 *
 * @param deviceClasses list of device classes to stop discovery for or NULL to stop all discovery
 *
 * @returns true if discovery stops successfully.
 */
bool deviceServiceDiscoverStop(icLinkedList* deviceClasses);

/*
 * Determine if any device discovery is currently running.
 *
 * @return true if any discovery is running
 */
bool deviceServiceIsDiscoveryActive();

/*
 * Determine if we are in recovery mode or not
 *
 * @return true if we are in device recovery mode
 */
bool deviceServiceIsInRecoveryMode();

/*
 * Retrieve a list of all icDevices in the system.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @returns a linked list of all icDevices in the system.
 */
icLinkedList *deviceServiceGetAllDevices();

/*
 * Retrieve a list of devices that contains the metadataId or
 * contains the metadataId value that is equal to valueToCompare
 *
 * If valueToCompare is NULL, will only look if the metadata exists.
 * Otherwise will only add devices that equal the metadata Id and it's value.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @return - linked list of icDevices with found metadata, or NULL if error occurred
 */
icLinkedList *deviceServiceGetDevicesByMetadata(const char *metadataId, const char *valueToCompare);

/*
 * Retrieve an icDevice by its universally unique identifier.
 *
 * Caller is responsible for calling deviceDestroy() the non-NULL returned device.
 *
 * @param uuid - the device's universally unique identifier
 *
 * @returns the icDevice matching the uuid or NULL if not found.
 */
icDevice *deviceServiceGetDevice(const char *uuid);

/*
 * Check if the provided device uuid is known to device service.  This is meant to be fast/efficient.
 *
 * @param uuid - the device's universally unique identifier
 *
 * @returns true if the device is known
 */
bool deviceServiceIsDeviceKnown(const char *uuid);

/*
 * Check if the provided device uuid is blacklisted by CPE property.
 *
 * @param uuid - the device's universally unique identifier
 *
 * @returns true if the device is blacklisted
 */
bool deviceServiceIsDeviceBlacklisted(const char *uuid);

/*
 * Remove a device by uuid.
 *
 * @param uuid - the device's universally unique identifier
 *
 * @returns true on success
 */
bool deviceServiceRemoveDevice(const char *uuid);

/*
 * Retrieve an icDevice by URI.
 *
 * Caller is responsible for calling deviceDestroy() the non-NULL result
 *
 * @param uri - the URI of the device
 *
 * @returns the icDevice or NULL if not found
 */
icDevice *deviceServiceGetDeviceByUri(const char *uri);

/*
 * Retrieve a list of icDevices that have at least one endpoint supporting the provided profile.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @param profileId - the profile id for the lookup
 *
 * @returns a linked list of icDevices that have at least one endpoint supporting the provided profile.
 */
icLinkedList *deviceServiceGetDevicesByProfile(const char *profileId);

/*
 * Retrieve a list of icDevices that have the provided device class
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @param deviceClass - the device class for the lookup
 *
 * @returns a linked list of icDevices that support the provided class.
 */
icLinkedList *deviceServiceGetDevicesByDeviceClass(const char *deviceClass);

/*
 * Retrieve a list of icDevices that have the provided device driver
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @param deviceDriver - the device driver for the lookup
 *
 * @returns a linked list of icDevices that use the provided driver.
 */
icLinkedList *deviceServiceGetDevicesByDeviceDriver(const char *deviceDriver);

/*
 * Retrieve a list of icDevices that belong to the provided subsystem.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @param subsystem - the name of the subsystem
 *
 * @returns a linked list of icDevices that belong to the provided subsystem
 */
icLinkedList *deviceServiceGetDevicesBySubsystem(const char *subsystem);

/*
 * Retrieve a list of icDeviceEndpoints that support the provided profile.
 *
 * Caller is responsible for calling endpointDestroy() on each returned endpoint.
 *
 * @param profileId - the profile id for the lookup
 *
 * @returns a linked list of icDeviceEndpoints that support the provided profile.
 */
icLinkedList *deviceServiceGetEndpointsByProfile(const char *profileId);

/*
 * Retrieve an icDeviceResource by URI.
 *
 * Caller is responsible for calling resourceDestroy() on the non-NULL result
 *
 * @param uri - the URI of the resource
 *
 * @returns the icDeviceResource or NULL if not found
 */
icDeviceResource *deviceServiceGetResourceByUri(const char *uri);

/*
 * Retrieve an icDeviceEndpoint by URI.
 *
 * Caller is responsible for calling endpointDestroy() on the non-NULL result
 *
 * @param uri - the URI of the endpoint
 *
 * @returns the icDeviceEndpoint or NULL if not found
 */
icDeviceEndpoint *deviceServiceGetEndpointByUri(const char *uri);

/*
 * Retrieve an icDeviceEndpoint by its uuid.
 *
 * Caller is responsible for calling endpointDestroy() on the non-NULL result
 *
 * @param deviceUuid - the universally unique identifier for the device
 * @param endpointId - the id of the endpoint
 *
 * @returns the icDeviceEndpoint or NULL if not found
 */
icDeviceEndpoint *deviceServiceGetEndpointById(const char *deviceUuid, const char *endpointId);

/*
 * Remove an icDeviceEndpoint by its uuid.
 *
 * @param deviceUuid - the universally unique identifier for the device
 * @param endpointId - the id of the endpoint
 *
 * @returns true on success
 */
bool deviceServiceRemoveEndpointById(const char *deviceUuid, const char *endpointId);

/*
 * Write a resource.
 *
 * @param uri - the URI of the resource
 * @param value - the new value
 *
 * @returns true on success
 */
bool deviceServiceWriteResource(const char *uri, const char *value);

/*
 * Execute a resource.
 *
 * @param uri - the URI of the resource
 * @param arg - optional JSON object with arguments
 * @param response - an optional response
 *
 * @returns true on success
 */
bool deviceServiceExecuteResource(const char *uri, const char *arg, char **response);

/*
 * Change the mode of a resource.
 *
 * @param uri = the URI of the resource
 * @param newMode - the new mode bits for the resource
 *
 * @returns true on success
 */
bool deviceServiceChangeResourceMode(const char *uri, uint8_t newMode);

/*
 * Retrieve a device service system property. Caller frees value output.
 *
 * @param name - the name of the property to retrieve
 * @param value - receives the value of the property if successful (caller frees)
 *
 * @returns true on success
 */
bool deviceServiceGetSystemProperty(const char *name, char **value);

/*
 * Set a device service system property.
 *
 * @param name - the name of the property to set
 * @param value - the value of the property to set
 *
 * @return true on success
 */
bool deviceServiceSetSystemProperty(const char *name, const char *value);

/*
 * Get the value of a metadata item at the provided URI.
 *
 * @param uri - the URI to a metadata item
 * @param value - receives the value of the metadata if successful.  Note, value may be null.  Caller frees.
 *
 * @returns true on success (even though the value may be null)
 */
bool deviceServiceGetMetadata(const char *uri, char **value);

/*
 * Set the value of a metadata item.
 *
 * @param uri - the URI of the metadata to set
 * @param value - the value of the metadata to set
 *
 * @return true on success
 */
bool deviceServiceSetMetadata(const char *uri, const char *value);

/*
 * Reload the database from storage
 *
 * @return true on success
 */
bool deviceServiceReloadDatabase();

/*
 * Get a device descriptor for a device
 * @param device the device to retrieve the descriptor for
 * @return the device descriptor or NULL if its not found.  Caller is responsible for calling deviceDescriptorDestroy()
 */
DeviceDescriptor *deviceServiceGetDeviceDescriptorForDevice(icDevice *device);

/*
 * Get a debug appropriate dump of our Zigbee configuration.  Caller frees.
 */
char* deviceServiceDumpZigbeeConfig();

/*
 * Query for metadata based on a uri pattern, currently only supported matching is with wildcards, e.g. *
 *
 * @param uriPattern the uri pattern to search with
 * @return the list of matching metadata
 */
icLinkedList *deviceServiceGetMetadataByUriPattern(char *uriPattern);

/*
 * Query for resources based on a uri pattern, currently only supported matching is with wildcards, e.g. *
 *
 * @param uriPattern the uri pattern to search with
 * @return the list of matching resources
 */
icLinkedList* deviceServiceGetResourcesByUriPattern(char* uriPattern);

/*
 * Check if we are ready to start working with devices
 */
bool deviceServiceIsReadyForDevices();

/*
 * Notify the device service of system power state changes
 */
void deviceServiceNotifySystemPowerEvent(DeviceServiceSystemPowerEventType powerEvent);

/*
 * Notify the device service of property change events.
 */
void deviceServiceNotifyPropertyChange(cpePropertyEvent *event);

/*
 * Retrieve the current status of device service. Caller frees result with deviceServiceDestroyServiceStatus.
 */
DeviceServiceStatus *deviceServiceGetStatus();

/*
 * Free resources owned by the provided DeviceServiceStatus
 */
void deviceServiceDestroyServiceStatus(DeviceServiceStatus *status);

/*
 * Checks if a provided device is in comm fail. Will return true if the device is in comm fail, false otherwise
 * or on error.
 */
bool deviceServiceIsDeviceInCommFail(const char *deviceUuid);

/*
 * Retrieve the firmware version of a device.  Caller frees
 */
char *deviceServiceGetDeviceFirmwareVersion(const char *deviceUuid);

#endif //ZILKER_DEVICESERVICE_H
