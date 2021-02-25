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
// Created by Thomas Lea on 7/28/15.
//

#ifndef ZILKER_DEVICEDRIVER_H
#define ZILKER_DEVICEDRIVER_H

#include <icTypes/icLinkedList.h>
#include <deviceDescriptor.h>
#include <device/icDevice.h>
#include <device/icDeviceResource.h>
#include <device/icInitialResourceValues.h>
#include <deviceService/securityState.h>
#include <deviceService.h>

typedef bool (*ConfigureDeviceFunc)(void *ctx, icDevice *device, DeviceDescriptor *descriptor);

typedef bool (*FetchInitialResourceValuesFunc)(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);

/*
 * Register the resources provided by this device.
 *
 * @param ctx the callbackContext value provided by this device driver
 * @param device - the device that should register its resources
 * @param initialResourceValue - initial values to use for resources
 */
typedef bool (*RegisterResourcesFunc)(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);

/*
 * This callback is invoked once a device has been configured, resources registered, and persisted as a functional
 * device in the database.
 */
typedef bool (*DevicePersistedFunc)(void *ctx, icDevice *device);

typedef struct
{
    /*
     * The name of the device driver such as 'openHomeCameraDeviceDriver'. Must be unique.
     */
    char *driverName;

    /*
     * The name of the subsystem used by this device driver or NULL if none.
     */
    char *subsystemName;

    /*
     * A linked list of strings indicating which device class(es) this driver supports.
     */
    icLinkedList *supportedDeviceClasses;

    /*
     * This opaque context pointer will be passed to all callback functions defined below.
     */
    void *callbackContext;

    /*
     * This prevents deviceService from rejecting this device for any reason.
     */
    bool neverReject;

    /*
     * Perform any required startup processing.
     *
     * @param ctx the callbackContext value provided by this device driver
     */
    void (*startup)(void *ctx);

    /*
     * Shut down any background processing and release all resources.
     *
     * @param ctx the callbackContext value provided by this device driver
     */
    void (*shutdown)(void *ctx);

    /*
     * Start discovering devices of the specified device class.  This call must return immediately and
     * any long running operations must be done in the background.
     *
     * As devices are discovered, the driver should invoke DeviceServiceInterface.deviceFound with the discovered
     * details.  If a device descriptor is found for the device and we want to continue with it,
     * DeviceDriver.configureDevice will be called.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param deviceClass the device class to discover
     *
     * returns true if discovery started successfully
     */
    bool (*discoverDevices)(void *ctx, const char *deviceClass);

    /*
     * Start recovring devices of the specified device class.  This call must return immediately and
     * any long running operations must be done in the background.
     *
     * As devices are discovered, the driver should invoke DeviceServiceInterface.deviceFound with the discovered
     * details.  If a device descriptor is found for the device and we want to continue with it,
     * DeviceDriver.configureDevice will be called.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param deviceClass the device class to recover
     *
     * returns true if recovery started successfully
     */
    bool (*recoverDevices)(void *ctx, const char *deviceClass);

    /*
     * Stop discovering devices of a specific device class or all device classes if not specifiedl.
     * This call must return immediately and any long running operations must be done in the background.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param deviceClass the device class to stop discovering or NULL to stop all discovery
     */
    void (*stopDiscoveringDevices)(void *ctx, const char *deviceClass);

    /*
     * Device Service removed the specified device from inventory.  This allows the device driver to perform any cleanup.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device - the device that was removed
     */
    void (*deviceRemoved)(void *ctx, icDevice *device);

    /*
     * Apply any initial configuration to the discovered device, including anything specified in the device descriptor.
     * This call blocks until the device is either successfully configured or fails configuration.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device - a partially initialized icDevice containing device resources for:
     *                  uuid, deviceClass, device driver name, manufacturer,
     *                  model, hardware version, and firmware version.
     *
     * returns true if the device has been successfully configured
     */
    ConfigureDeviceFunc configureDevice;

    /*
     * Fetch initial values for resources
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device - the device for which we are fetching resource values
     * @param initialResourceValue - populate with initial resource values
     */
    FetchInitialResourceValuesFunc fetchInitialResourceValues;

    /*
     * Register the resources provided by this device.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device - the device that should register its resources
     * @param initialResourceValue - initial values to use for resources
     */
    RegisterResourcesFunc registerResources;

    /*
     * This callback is invoked once a device has been configured, resources registered, and persisted as a functional
     * device in the database.
     */
    DevicePersistedFunc devicePersisted;

    /*
     * Retrieve an resource from a device.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param resource - the resource to write
     * @param value - the value output parameter
     *
     * returns true on success
     */
    bool (*readResource)(void *ctx, icDeviceResource *resource, char **value);

    /*
     * Write an resource to a device.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param resource - the resource to write
     * @param previousValue - the previous value
     * @param newValue - the new value
     *
     * returns true on success
     */
    bool (*writeResource)(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);

    /*
     * Execute a resource on a device.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param resource - the resource to write
     * @param arg - optional JSON argument object
     * @param response - a pointer that will hold the response or null if no response is expected/desired (caller frees if non-null value returned)
     *
     * returns true on success
     */
    bool (*executeResource)(void *ctx, icDeviceResource *resource, const char *arg, char **response);

    /*
     * Examine the given device and its descriptor and apply any required changes or initiate firmware upgrades.
     *
     * Note that this is called for each device at startup and again if the device descriptor list changes.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device the device to process
     * @param dd the device descriptor to process for the provided device
     *
     * return true on success
     */
    bool (*processDeviceDescriptor)(void *ctx,
                                    icDevice *device,
                                    DeviceDescriptor *dd);

    /*
     * The specified device has been identified as in communication failure.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device the device in communication failure
     */
    void (*communicationFailed)(void *ctx, icDevice *device);

    /*
     * The specified device is no longer in communication failure
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device the device restored from communication failure
     */
    void (*communicationRestored)(void *ctx, icDevice *device);

    /*
     * Synchronize our cached resources with the device.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device the device to synchronize
     */
    void (*synchronizeDevice)(void *ctx, icDevice *device);

    /*
     * Return true if the provided device needs to be reconfigured.  This could be due to device class or
     * profile version mismatches or other required changes in a device's configuration.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device the device to synchronize
     * @return true if the device should be reconfigured through a call to reconfigureDevice below
     */
    bool (*deviceNeedsReconfiguring)(void *ctx, icDevice *device);

     /*
     * Callback to deal with RMA
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param tempRestoreDir the directory with the RMA backup
     * @param dynamicConfigPath our config directory to use
     */
    bool (*restoreConfig)(void *ctx, const char *tempRestoreDir, const char *dynamicConfigPath);

    /*
     * Callback for when an endpoint is disabled
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param endpoint the endpoint being disabled
     */
    void (*endpointDisabled)(void *ctx, icDeviceEndpoint *endpoint);

    /*
     * Callback for notification of system power events
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param powerState the most recent system power event
     */
    void (*systemPowerEvent)(void *ctx, DeviceServiceSystemPowerEventType powerEvent);

    /*
     * Callback for notification of property changes.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param event the property change event
     */
    void (*propertyChanged)(void *ctx, cpePropertyEvent *event);

    /*
     * Callback for collection of device-specific runtime statistics.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param output the target object for the statistics
     */
    void (*fetchRuntimeStats)(void *ctx,  icStringHashMap *output);

    /*
     * Callback to deal with pre restore configuration with RMA
     *
     * @param ctx the callbackContext value provided by this device driver
     */
    void (*preRestoreConfig)(void *ctx);

    /*
     * Callback to deal with post restore configuration with RMA
     *
     * @param ctx the callbackContext value provided by this device driver
     */
    void (*postRestoreConfig)(void *ctx);

    /*
     * Retrieve the device class version for the provided device class.  Return false on failure.
     */
    bool (*getDeviceClassVersion)(void *ctx, const char *deviceClass, uint8_t *version);

    /*
     * Callback for notification that subsystem has been initialized.
     */
    void (*subsystemInitialized)(void *ctx);

} DeviceDriver;

typedef struct
{
    void *ctx;
    const DeviceDriver *deviceDriver;
} DeviceMigratorCallbackContext;

typedef struct
{
    DeviceMigratorCallbackContext *callbackContext;

    /*
     * Apply any initial configuration to the discovered device, including anything specified in the device descriptor.
     * This call blocks until the device is either successfully configured or fails configuration.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device - a partially initialized icDevice containing device resources for:
     *                  uuid, deviceClass, device driver name, manufacturer,
     *                  model, hardware version, and firmware version.
     *
     * returns true if the device has been successfully configured
     */
    ConfigureDeviceFunc configureDevice;

    /*
     * Fetch initial values for resources
     * @param ctx the callbackContext value provided by this device driver
     * @param device - the device for which we are fetching resource values
     * @param initialResourceValue - populate with initial resource values
     */
    FetchInitialResourceValuesFunc fetchInitialResourceValues;

    /*
     * Register the resources provided by this device.
     *
     * @param ctx the callbackContext value provided by this device driver
     * @param device - the device that should register its resources
     * @param initialResourceValue - initial values to use for resources
     */
    RegisterResourcesFunc registerResources;

    /*
     * This callback is invoked once a device has been configured, resources registered, and persisted as a functional
     * device in the database.
     */
    DevicePersistedFunc devicePersisted;
} DeviceMigrator;

typedef enum
{
    updateResourceEventNever, // Never send an event when the resource is updated
    updateResourceEventChanged // Only send an event if the value of the resource actually changed
} UpdateResourceEventMethod;

typedef struct
{
    const DeviceDriver *deviceDriver;
    const DeviceMigrator *deviceMigrator;
    const char *subsystem;
    const char *deviceClass;
    uint8_t deviceClassVersion;
    const char *deviceUuid;
    const char *manufacturer;
    const char *model;
    const char *hardwareVersion;
    const char *firmwareVersion;
    icStringHashMap *endpointProfileMap;
    icStringHashMap *metadata;
} DeviceFoundDetails;

typedef enum
{
    REQUEST_SOURCE_INVALID = 0,
    REQUEST_SOURCE_WIRELESS_KEYPAD,
    REQUEST_SOURCE_WIRELESS_KEYFOB,
    REQUEST_SOURCE_TAKEOVER_KEYPAD
} RequestSource;

typedef enum
{
    ARM_NOTIF_INVALID = 0,
    ARM_NOTIF_DISARMED,
    ARM_NOTIF_ARMED_HOME,
    ARM_NOTIF_ARMED_NIGHT,
    ARM_NOTIF_ARMED_ALL,
    ARM_NOTIF_NOT_READY,
    ARM_NOTIF_ALREADY_DISARMED,
    ARM_NOTIF_ALREADY_ARMED,
    ARM_NOTIF_BAD_ACCESS_CODE,
    ARM_NOTIF_TROUBLE
} ArmDisarmNotification;

/*
 * These are the callbacks that device drivers use to communicate back to Device Service.
 *
 * It is done this way to prevent circular dependencies.
 */
typedef struct
{
    /*
     * returns true if the device is accepted or false if rejected.  If neverReject is true, it will not be rejected
     * due to missing device descriptor; if inRepairMode is true, then we do not fail because the device
     * already exists in the database
     */
    bool (*deviceFound)(DeviceFoundDetails *deviceFoundDetails, bool neverReject);

    icLinkedList *(*getDevicesByDeviceDriver)(const char *deviceDriver);
    icDevice *(*getDevice)(const char *deviceUuid);
    icDeviceEndpoint *(*getEndpoint)(const char *deviceUuid, const char *endpointId);
    icDeviceResource *(*getResource)(const char *deviceUuid, const char *endpointId, const char *resourceId);

    /*
     * Update a resource, persist to database, and optionally send out an event if the newValue was different than
     * what was in the database and the mode supports eventing.
     */
    void (*updateResource)(const char *deviceUuid,
                           const char *endpointId,
                           const char *resourceId,
                           const char *newValue,
                           cJSON *metadata);

    /*
     * Metadata are simple name/value string pairs that can be used to store arbitrary data on endpoints.
     */
    void (*setMetadata)(const char *deviceUuid, const char *endpointId, const char *name, const char *value);
    char *(*getMetadata)(const char *deviceUuid, const char *endpointId, const char *name);

    bool (*removeDevice)(const char *deviceUuid);

    bool (*discoverStart)(icLinkedList *deviceClasses, uint16_t timeoutSeconds, bool findOrphanedDevices);
    bool (*discoverStop)(icLinkedList *deviceClasses);

    /**
     * Request a panel state change (arm/disarm/panic)
     * @param desiredState
     * @param accessCode
     * @param source The requesting device
     * @return a notification describing the
     */
    ArmDisarmNotification (*requestPanelStateChange)(const PanelStatus desiredState, const char *accessCode, const RequestSource source);

    /**
     * Add an endpoint to an existing device, persist to database and send out events.  The endpoint provided
     * must already be added to the device provided.
     * @param device
     * @param endpoint
     */
     void (*addEndpoint)(icDevice *device, icDeviceEndpoint *endpoint);

    /**
     * Update an endpoint, persist to database and send out events.
     * FIXME: This should be called 'updateEndpoint.'
     * @param device the device
     * @param endpoint the endpoint
     */
    void (*enableEndpoint)(icDevice *device, icDeviceEndpoint *endpoint);

    /**
     * Get a security state descriptor pointer.
     * The returned object must be destroyed with securityStateDestroy and its freed
     */
    SecurityState *(*getSecurityState)(void);

} DeviceServiceCallbacks;

#endif //ZILKER_DEVICEDRIVER_H
