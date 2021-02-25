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
// Created by tlea on 4/17/19
//

#include <icBuildtime.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <stdio.h>
#include <math.h>
#include <resourceTypes.h>
#include <commonDeviceDefs.h>
#include <deviceModelHelper.h>
#include <memory.h>
#include <zconf.h>
#include <zhal/zhal.h>
#include <zigbeeClusters/levelControlCluster.h>
#include <icUtil/stringUtils.h>
#include "zigbeeClusters/onOffCluster.h"
#include "zigbeeDriverCommon.h"
#include "zigbeeLegacyLightDeviceDriver.h"

#define LOG_TAG "zigbeeLegacyLightDD"
#define DRIVER_NAME "zigbeeLegacyLight"
#define DEVICE_CLASS_NAME "light"
#define MY_DC_VERSION 1
#define MAX_LEGACY_APP_VERSION 20

// only compile if we support zigbee
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#define ADJUST_LEVEL_METADATA "adjustLevel"
#define PREVENT_BINDING_METADATA "preventBinding"

static uint16_t myDeviceIds[] = {};

static uint8_t adjustLevel(uint64_t eui64, uint8_t endpointNumber, uint8_t level, bool toDevice);

static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details);

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues);

static void onOffStateChangedCallback(uint64_t eui64,
                                      uint8_t endpointId,
                                      bool isOn,
                                      const void *ctx);

static void levelChangedCallback(uint64_t eui64,
                                 uint8_t endpointId,
                                 uint8_t level,
                                 const void *ctx);

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource);

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details);

static void handleClusterCommand(ZigbeeDriverCommon *ctx, ReceivedClusterCommand *command);

static bool preConfigureCluster(ZigbeeDriverCommon *ctx,
                                ZigbeeCluster *cluster,
                                DeviceConfigurationContext *deviceConfigContext);

static const ZigbeeDriverCommonCallbacks commonCallbacks =
        {
                .claimDevice = claimDevice,
                .fetchInitialResourceValues = fetchInitialResourceValues,
                .registerResources = registerResources,
                .mapDeviceIdToProfile = mapDeviceIdToProfile,
                .writeEndpointResource = writeEndpointResource,
                .synchronizeDevice = synchronizeDevice,
                .handleClusterCommand = handleClusterCommand,
                .preConfigureCluster = preConfigureCluster,
        };

static const OnOffClusterCallbacks onOffClusterCallbacks =
        {
                .onOffStateChanged = onOffStateChangedCallback,
        };

static const LevelControlClusterCallbacks levelControlClusterCallbacks =
        {
                .levelChanged = levelChangedCallback,
        };

static DeviceServiceCallbacks *deviceServiceCallbacks = NULL;

DeviceDriver *zigbeeLegacyLightDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DRIVER_NAME,
                                                                  DEVICE_CLASS_NAME,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  sizeof(myDeviceIds) / sizeof(uint16_t),
                                                                  deviceService,
                                                                  &commonCallbacks);

    deviceServiceCallbacks = deviceService;

    zigbeeDriverCommonAddCluster(myDriver, onOffClusterCreate(&onOffClusterCallbacks, myDriver));

    ZigbeeCluster *levelControlCluster = levelControlClusterCreate(&levelControlClusterCallbacks, myDriver);

    zigbeeDriverCommonAddCluster(myDriver, levelControlCluster);

    //allow all legacy lights
    myDriver->neverReject = true;

    return myDriver;
}

static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = false;

    //we claim this device if the manufacturer name matches and also
    // if at least one endpoint reports an appVersion less than MAX_LEGACY_APP_VERSION
    //  Devices that come here with a non-zero firmware version have already loaded it from
    // the OTA upgrade cluster, which legacy lights do not support
    if (details->firmwareVersion == 0 && stringCompare(details->manufacturer, "CentraLite Systems", false) == 0)
    {
        for (int i = 0; i < details->numEndpoints; i++)
        {
            if(details->appVersion < MAX_LEGACY_APP_VERSION &&
                    (details->endpointDetails[i].appDeviceId == ON_OFF_LIGHT_DEVICE_ID ||
                     details->endpointDetails[i].appDeviceId == DIMMABLE_LIGHT_DEVICE_ID))
            {
                icLogDebug(LOG_TAG, "%s: claiming this device as a legacy light", __FUNCTION__);

                //TODO we dont support firmware upgrades on these yet and we arent rejecting any of these for any
                // reason.  Just assume the 'latest' legacy light firmware (we arent really supporting these
                // anymore anyway).
                details->firmwareVersion = details->appVersion;

                result = true;
                break;
            }
        }
    }

    return result;
}

static void onOffStateChangedCallback(uint64_t eui64,
                                      uint8_t endpointId,
                                      bool isOn,
                                      const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: light is now %s", __FUNCTION__, isOn ? "on" : "off");

    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           LIGHT_PROFILE_RESOURCE_IS_ON,
                                           isOn ? "true" : "false",
                                           NULL);

    free(uuid);
}

static void levelChangedCallback(uint64_t eui64,
                                 uint8_t endpointId,
                                 uint8_t level,
                                 const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: light is now  at level %"
            PRIu8, __FUNCTION__, level);

    char epName[4]; //max uint8_t + \0
    sprintf(epName, "%" PRIu8, endpointId);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    char *levelStr = levelControlClusterGetLevelString(adjustLevel(eui64, endpointId, level, false));

    deviceServiceCallbacks->updateResource(uuid,
                                           epName,
                                           LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL,
                                           levelStr,
                                           NULL);

    free(levelStr);
    free(uuid);
}

//fetch resource values related to a light endpoint (not a switch)
static bool fetchInitialLightResourceValues(icDevice *device,
                                            IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                            uint64_t eui64,
                                            uint8_t endpointId,
                                            uint16_t deviceId,
                                            const char *epName,
                                            icInitialResourceValues *initialResourceValues)
{
    //add the on/off stuff
    bool isOn;
    if (onOffClusterIsOn(eui64, endpointId, &isOn) == false)
    {
        icLogError(LOG_TAG, "%s: failed to read initial on off attribute value", __FUNCTION__);
        return false;
    }

    initialResourceValuesPutEndpointValue(initialResourceValues, epName, LIGHT_PROFILE_RESOURCE_IS_ON,
                                          isOn ? "true" : "false");

    //add the level stuff
    if (icDiscoveredDeviceDetailsEndpointHasCluster(discoveredDeviceDetails,
                                                    endpointId,
                                                    LEVEL_CONTROL_CLUSTER_ID,
                                                    true))
    {
        uint8_t level;
        if (levelControlClusterGetLevel(eui64, endpointId, &level) == false)
        {
            icLogError(LOG_TAG, "%s: failed to read initial level attribute value", __FUNCTION__);
            return false;
        }

        char *levelStr = levelControlClusterGetLevelString(level);
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL,
                                              levelStr);
        free(levelStr);

        // Whether dimming is enabled for this device
        initialResourceValuesPutEndpointValue(initialResourceValues, epName, LIGHT_PROFILE_RESOURCE_IS_DIMMABLE_MODE,
                                              "true");
    }

    return true;
}

//register resources related to a light endpoint (not a switch)
static bool registerLightResources(icDevice *device,
                                   IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                   uint64_t eui64,
                                   uint8_t endpointId,
                                   uint16_t deviceId,
                                   const char *epName,
                                   icInitialResourceValues *initialResourceValues)
{
    icDeviceEndpoint *endpoint = createEndpoint(device, epName, LIGHT_PROFILE, true);

    //add the on/off stuff
    bool result = createEndpointResourceIfAvailable(endpoint,
                                                    LIGHT_PROFILE_RESOURCE_IS_ON,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                                    CACHING_POLICY_ALWAYS);


    //add the level stuff: optional
    createEndpointResourceIfAvailable(endpoint,
                                      LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL,
                                      initialResourceValues,
                                      RESOURCE_TYPE_LIGHT_LEVEL,
                                      RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |
                                      RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                      CACHING_POLICY_ALWAYS);

    // Whether dimming is enabled for this device: optional
    createEndpointResourceIfAvailable(endpoint,
                                      LIGHT_PROFILE_RESOURCE_IS_DIMMABLE_MODE,
                                      initialResourceValues,
                                      RESOURCE_TYPE_BOOLEAN,
                                      RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                      CACHING_POLICY_ALWAYS);

    // store a flag indicating that this device requires level adjustment (0-100 <--> 0-255)
    if(discoveredDeviceDetails->appVersion == 5 || discoveredDeviceDetails->appVersion == 6)
    {
        createEndpointMetadata(endpoint, ADJUST_LEVEL_METADATA, "true");
    }

    // legacy in-wall switches get whacked if we set any bindings at all.
    if(strcmp(discoveredDeviceDetails->model, "Relay Switch") == 0 ||
       strcmp(discoveredDeviceDetails->model, "Dimmer Switch") == 0)
    {
        createEndpointMetadata(endpoint, PREVENT_BINDING_METADATA, "true");
    }

    zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);

    return result;
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    //get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        uint16_t deviceId = discoveredDeviceDetails->endpointDetails[i].appDeviceId;

        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        if (!(result = fetchInitialLightResourceValues(device,
                                                       discoveredDeviceDetails,
                                                       eui64,
                                                       endpointId,
                                                       deviceId,
                                                       epName,
                                                       initialResourceValues)))
        {
            icLogError(LOG_TAG, "%s: failed to fetch initial light resource values", __FUNCTION__);
            result = false;
            break;
        }
    }

    return result;
}

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    //get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        uint16_t deviceId = discoveredDeviceDetails->endpointDetails[i].appDeviceId;

        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        if (!(result = registerLightResources(device,
                                              discoveredDeviceDetails,
                                              eui64,
                                              endpointId,
                                              deviceId,
                                              epName,
                                              initialResourceValues)))
        {
            icLogError(LOG_TAG, "%s: failed to register light resources", __FUNCTION__);
            result = false;
            break;
        }
    }

    return result;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    if(deviceId == ON_OFF_LIGHT_DEVICE_ID || deviceId == DIMMABLE_LIGHT_DEVICE_ID)
    {
        return LIGHT_PROFILE;
    }
    else
    {
        return NULL;
    }
}

// Legacy lights with app version 5 or 6 used 0-100 for level.  Convert the input
// if required.  If toDevice is true, convert standard level (0-254) to legacy level.
//  If toDevice is false, convert legacy level to standard.
static uint8_t adjustLevel(uint64_t eui64, uint8_t endpointNumber, uint8_t level, bool toDevice)
{
    uint8_t result = level;

    AUTO_CLEAN(free_generic__auto) char *uuid = zigbeeSubsystemEui64ToId(eui64);
    char epid[4]; //max uint8_t in decimal plus null
    sprintf(epid, "%" PRIu8, endpointNumber);

    AUTO_CLEAN(free_generic__auto) char *adjustMetadata = deviceServiceCallbacks->getMetadata(uuid,
                                                                                              epid,
                                                                                              ADJUST_LEVEL_METADATA);

    if(adjustMetadata != NULL && strcmp(adjustMetadata, "true") == 0)
    {
        //hmm... just ported exactly what we did in legacy
        if(toDevice)
        {
            result = (uint8_t) round(((level == 0xfeu) ? 100 : ((level & 0xffu) * 100) / 255.0));
        }
        else
        {
            result = (uint8_t) round((level & 0xffu) * 255 / 100.0);

            //funky, funky.  We need to map 0-100 to 0-254.  The above is off by one unless it is zero.
            if((result & 0xffu) > 0 && (result & 0xffu) < 255)
            {
                result++;
            }
        }
    }

    return result;
}

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource)
{
    bool result = false;

    (void) baseDriverUpdatesResource; //unused

    if (resource == NULL || endpointNumber == 0 || newValue == NULL)
    {
        icLogDebug(LOG_TAG, "writeEndpointResource: invalid arguments");
        return false;
    }

    icLogDebug(LOG_TAG, "writeEndpointResource on endpoint %s: id=%s, previousValue=%s, newValue=%s",
               resource->endpointId, resource->id, previousValue, newValue);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);

    if (strcmp(LIGHT_PROFILE_RESOURCE_IS_ON, resource->id) == 0)
    {
        result = onOffClusterSetOn(eui64, (uint8_t) endpointNumber, strcmp(newValue, "true") == 0);
    }
    else if (strcmp(LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL, resource->id) == 0)
    {
        uint8_t level = adjustLevel(eui64, (uint8_t) endpointNumber,
                levelControlClusterGetLevelFromString(newValue), true);
        result = levelControlClusterSetLevel(eui64, (uint8_t) endpointNumber, level);

        //These legacy CentraLite devices have a bug when the onLevel attribute is set such that you cannot
        // raise the dim level higher than the onLevel from the switch directly.  Setting this attribute to
        // 0xff disables onLevel.  DE11949.  This overrides what the level control cluster call above does.
        if(zigbeeSubsystemWriteNumber(eui64,
                                      endpointNumber,
                                      LEVEL_CONTROL_CLUSTER_ID,
                                      true,
                                      LEVEL_CONTROL_ON_LEVEL_ATTRIBUTE_ID,
                                      ZCL_INT8U_ATTRIBUTE_TYPE,
                                      0xffu,
                                      1) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to reset on level", __FUNCTION__);
            result = false;
        }
    }

    return result;
}

static void clearBindings(uint64_t eui64)
{
    icLinkedList *bindings = zigbeeSubsystemBindingGet(eui64);
    if(bindings != NULL)
    {
        icLinkedListIterator *it = linkedListIteratorCreate(bindings);
        while(linkedListIteratorHasNext(it))
        {
            zhalBindingTableEntry *entry = linkedListIteratorGetNext(it);

            icLogDebug(LOG_TAG, "%s: clearing binding to %016"PRIx64,
                       __FUNCTION__,
                       entry->destination.extendedAddress.eui64);

            zhalBindingClearTarget(eui64,
                                   entry->sourceEndpoint,
                                   entry->clusterId,
                                   entry->destination.extendedAddress.eui64,
                                   entry->destination.extendedAddress.endpoint);
        }
        linkedListIteratorDestroy(it);
    }
    linkedListDestroy(bindings, NULL);
}

static void synchronizeDevice(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    icLinkedListIterator *it = linkedListIteratorCreate(device->endpoints);
    while(linkedListIteratorHasNext(it))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint*) linkedListIteratorGetNext(it);
        uint8_t endpointNumber = zigbeeDriverCommonGetEndpointNumber(ctx, endpoint);

        if(icDiscoveredDeviceDetailsEndpointHasCluster(details, endpointNumber, ON_OFF_CLUSTER_ID, true))
        {
            bool isOn;
            if (onOffClusterIsOn(eui64, endpointNumber, &isOn))
            {
                deviceServiceCallbacks->updateResource(device->uuid,
                                                       endpoint->id,
                                                       LIGHT_PROFILE_RESOURCE_IS_ON,
                                                       isOn ? "true" : "false",
                                                       NULL);
            }

            //We could have missed the fact that the light rebooted and needs attribute reporting reconfigured.
            // Just do it proactively in case.
            onOffClusterSetAttributeReporting(eui64, endpointNumber);
        }

        if(icDiscoveredDeviceDetailsEndpointHasCluster(details, endpointNumber, LEVEL_CONTROL_CLUSTER_ID, true))
        {
            uint8_t level;
            if (levelControlClusterGetLevel(eui64, endpointNumber, &level))
            {
                deviceServiceCallbacks->updateResource(device->uuid,
                                                       endpoint->id,
                                                       LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL,
                                                       levelControlClusterGetLevelString(adjustLevel(eui64,
                                                                           endpointNumber, level, false)),
                                                       NULL);
            }

            //We could have missed the fact that the light rebooted and needs attribute reporting reconfigured.
            // Just do it proactively in case.
            levelControlClusterSetAttributeReporting(eui64, endpointNumber);
        }
    }
    linkedListIteratorDestroy(it);
}

static void handleClusterCommand(ZigbeeDriverCommon *ctx, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if(command->clusterId == BASIC_CLUSTER_ID &&
       command->mfgSpecific &&
       command->commandId == 0x00)
    {
        //The light rebooted, reconfigure reporting
        onOffClusterSetAttributeReporting(command->eui64, command->sourceEndpoint);
        levelControlClusterSetAttributeReporting(command->eui64, command->sourceEndpoint);
    }
}

static bool preConfigureCluster(ZigbeeDriverCommon *ctx,
                                ZigbeeCluster *cluster,
                                DeviceConfigurationContext *deviceConfigContext)
{
    bool preventBinding = (strcmp(deviceConfigContext->discoveredDeviceDetails->model, "Relay Switch") == 0 ||
            strcmp(deviceConfigContext->discoveredDeviceDetails->model, "Dimmer Switch") == 0);

    if (cluster->clusterId == ON_OFF_CLUSTER_ID)
    {
        // some of these legacy lights can have full binding tables which will prevent them from working at all.
        // these bindings are not removed when the device is reset to factory.
        // remove all entries as part of this configuration.
        clearBindings(deviceConfigContext->eui64);

        if(preventBinding)
        {
            onOffClusterSetBindingEnabled(deviceConfigContext, false);
        }
    }
    else if (cluster->clusterId == LEVEL_CONTROL_CLUSTER_ID)
    {
        if(preventBinding)
        {
            levelControlClusterSetBindingEnabled(deviceConfigContext, false);
        }
    }

    return true;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE


