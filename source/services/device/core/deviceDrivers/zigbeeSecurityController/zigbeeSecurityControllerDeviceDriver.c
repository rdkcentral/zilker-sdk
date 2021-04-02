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

#include <icBuildtime.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include <deviceDriver.h>
#include <errno.h>
#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icUtil/array.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include <zigbeeClusters/iasZoneCluster.h>
#include <deviceModelHelper.h>
#include <resourceTypes.h>
#include <icTime/timeUtils.h>
#include <zigbeeClusters/pollControlCluster.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <deviceService/zoneChanged.h>
#include <zigbeeClusters/helpers/comcastBatterySavingHelper.h>
#include "zigbeeSecurityControllerDeviceDriver.h"
#include "zigbeeDriverCommon.h"
#include "zigbeeClusters/helpers/iasZoneHelper.h"

#define KEYPAD_DRIVER_NAME "ZigbeeKeypadDD"
#define KEYFOB_DRIVER_NAME "ZigbeeKeyfobDD"
#define KEYPAD_DC_VERSION 1
#define KEYFOB_DC_VERSION 1

#define ACE_DEVICE_METADATA_SEND_ZONE_STATUS            "securityController.sendZoneStatusChanged"
#define ACE_DEVICE_METADATA_SEND_BYPASS_LIST            "securityController.sendSetBypassedZoneList"
#define ACE_DEVICE_METADATA_SEND_PANEL_STATUS           "securityController.sendPanelStatusChanged"
#define ACE_DEVICE_METADATA_SEND_EXIT_ENTRY_DELAY       "securityController.sendPanelStatusCountdown"

/* Intentionally empty - force claimDevice to claim based on zone type */
static uint16_t myDeviceIds[] = {};

/* ZigbeeDriverCommonCallbacks */
static void preStartup(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds);
static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *discoveredDeviceDetails);

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues);
static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource);
static bool preConfigureCluster(ZigbeeDriverCommon *ctx,
                         ZigbeeCluster *cluster,
                         DeviceConfigurationContext *deviceConfigContext);

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

/* ACE Cluster callbacks */
void onZoneStatusChanged(uint64_t eui64,
                         uint8_t endpointId,
                         const IASZoneStatusChangedNotification *status,
                         const ComcastBatterySavingData *batterySavingData,
                         const void *ctx);

static void onGetPanelStatusReceived(uint64_t eui64,
                                     uint8_t endpointId,
                                     const void *ctx);

static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices);

/* Private functions */
/**
 * Create an IAS ACE driver with a particular driver name and class. Keypads and keyfobs are really both IAS ACE
 * "securityController" devices
 */
static DeviceDriver *zigbeeSecurityControllerDeviceDriverCreate(DeviceServiceCallbacks *deviceService,
                                                                const char *driverName,
                                                                const char *deviceClass,
                                                                uint8_t dcVersion);

static void updateLastInteractionDate(uint64_t eui64, const DeviceDriver *_this);

static DeviceDriver *zigbeeSecurityControllerDeviceDriverCreate(DeviceServiceCallbacks *deviceService,
                                                                const char *driverName,
                                                                const char *deviceClass,
                                                                uint8_t dcVersion)
{
    icLogDebug(driverName, "%s", __FUNCTION__);

    static const ZigbeeDriverCommonCallbacks myHooks = {
            .preStartup = preStartup,
            .claimDevice = claimDevice,
            .fetchInitialResourceValues = fetchInitialResourceValues,
            .registerResources = registerResources,
            .writeEndpointResource = writeEndpointResource,
            .preConfigureCluster = preConfigureCluster,
            .mapDeviceIdToProfile = mapDeviceIdToProfile,
            .devicesLoaded = devicesLoaded
    };

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(driverName,
                                                                  deviceClass,
                                                                  dcVersion,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  deviceService,
                                                                  &myHooks);

    static const IASZoneClusterCallbacks iasZoneClusterCallbacks = {
            .onZoneEnrollRequested = NULL,
            .onZoneStatusChanged = onZoneStatusChanged
    };

    zigbeeDriverCommonAddCluster(myDriver, iasZoneClusterCreate(&iasZoneClusterCallbacks, myDriver));

    return myDriver;
}

// FIXME: Device drivers shouldn't have to own profile creation and maintenance.
// The goal is to ship this to an endpoint profile library and have a common layer trigger any
// maintenance, registration, etc.
//
static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices)
{
    const DeviceDriver *_this = (DeviceDriver *) ctx;
    sbIcLinkedListIterator *devIt = linkedListIteratorCreate(devices);
    const DeviceServiceCallbacks *deviceService = zigbeeDriverCommonGetDeviceService(ctx);

    while (linkedListIteratorHasNext(devIt) == true)
    {
        icDevice *device = linkedListIteratorGetNext(devIt);
        sbIcLinkedListIterator *epIt = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(epIt) == true)
        {
            icDeviceEndpoint *ep = linkedListIteratorGetNext(epIt);
            if (ep->profileVersion < SECURITY_CONTROLLER_PROFILE_VERSION &&
                stringCompare(ep->profile, SECURITY_CONTROLLER_PROFILE, false) == 0)
            {
                bool migrationOk = true;
                icLogInfo(_this->driverName,
                          "Migrating securityController profile version %d -> %d on %s/%s",
                          ep->profileVersion,
                          SECURITY_CONTROLLER_PROFILE_VERSION,
                          device->uuid,
                          ep->id);

                if (ep->profileVersion < 2)
                {
                    icDeviceResource *res = createEndpointResource(ep,
                                                                   SECURITY_CONTROLLER_PROFILE_RESOURCE_ZONE_CHANGED,
                                                                   NULL,
                                                                   RESOURCE_TYPE_ZONE_CHANGED,
                                                                   RESOURCE_MODE_WRITEABLE,
                                                                   CACHING_POLICY_NEVER);
                    if (res == NULL)
                    {
                        icLogWarn(_this->driverName,
                                  "Failed to add "
                                          SECURITY_CONTROLLER_PROFILE_RESOURCE_ZONE_CHANGED
                                          " resource! on %s/%s",
                                  device->uuid,
                                  ep->id);
                        migrationOk = false;
                    }
                }

                if (migrationOk == true)
                {
                    icLogInfo(_this->driverName, "Profile migration for %s/%s complete!", device->uuid, ep->id);
                    ep->profileVersion = SECURITY_CONTROLLER_PROFILE_VERSION;
                    // FIXME: This is mapped to 'deviceServiceUpdateEndpoint'
                    deviceService->enableEndpoint(device, ep);
                }
                else
                {
                    icLogWarn(_this->driverName, "Profile migration for %s/%s failed!", device->uuid, ep->id);
                }
            }
        }
    }
}

static RequestSource getRequestSource(ZigbeeDriverCommon *ctx);

DeviceDriver *zigbeeKeypadDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    return zigbeeSecurityControllerDeviceDriverCreate(deviceService, KEYPAD_DRIVER_NAME, KEYPAD_DC, KEYPAD_DC_VERSION);
}

DeviceDriver *zigbeeKeyfobDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    return zigbeeSecurityControllerDeviceDriverCreate(deviceService, KEYFOB_DRIVER_NAME, KEYFOB_DC, KEYFOB_DC_VERSION);
}

static void preStartup(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds)
{
    const char *deviceClass = zigbeeDriverCommonGetDeviceClass(ctx);
    if (strcmp(deviceClass, KEYFOB_DC) == 0)
    {
        *commFailTimeoutSeconds = 0;
    }
}
static bool preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext)
{
    if (cluster->clusterId == POLL_CONTROL_CLUSTER_ID)
    {
        //5 * 60 * 4 == 5 minutes in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, LONG_POLL_INTERVAL_QS_METADATA, "1200");

        //2 == half second in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, SHORT_POLL_INTERVAL_QS_METADATA, "2");

        //10 * 4 == 10 seconds in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, FAST_POLL_TIMEOUT_QS_METADATA, "40");

        //27 * 60 * 4 == 27 minutes in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, CHECK_IN_INTERVAL_QS_METADATA, "6480");
    }

    return true;
}

static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *discoveredDeviceDetails)
{
    bool claimed = false;
    uint64_t temp;
    uint16_t zoneType;
    const char *deviceClass = zigbeeDriverCommonGetDeviceClass(ctx);
    const DeviceDriver *_this = (DeviceDriver *) ctx;

    icLogDebug(_this->driverName, "%s", __FUNCTION__);

    if (discoveredDeviceDetails->numEndpoints > 0 && discoveredDeviceDetails->endpointDetails[0].appDeviceId == IAS_ACE_DEVICE_ID)
    {
        for (int i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
        {
            bool haveZoneType = false;
            // For migration case, need to look at the discovered details rather than reading from device
            IcDiscoveredAttributeValue *value;
            if (icDiscoveredDeviceDetailsClusterGetAttributeValue(discoveredDeviceDetails,
                                                                  discoveredDeviceDetails->endpointDetails[i].endpointId,
                                                                  IAS_ZONE_CLUSTER_ID, true, IAS_ZONE_TYPE_ATTRIBUTE_ID,
                                                                  &value) == true && value->dataLen > 0)
            {
                sbZigbeeIOContext *ioCtx = zigbeeIOInit(value->data, value->dataLen, ZIO_READ);
                zoneType = zigbeeIOGetUint16(ioCtx);
                haveZoneType = true;
            }
            else if (zigbeeSubsystemReadNumber(discoveredDeviceDetails->eui64,
                                          discoveredDeviceDetails->endpointDetails[i].endpointId,
                                          IAS_ZONE_CLUSTER_ID,
                                          true,
                                          IAS_ZONE_TYPE_ATTRIBUTE_ID,
                                          &temp) == 0)
            {
                zoneType = temp;
                haveZoneType = true;
            }
            else
            {
                icLogError(_this->driverName, "%s: failed to read zone type attribute", __FUNCTION__);
            }

            if (haveZoneType)
            {
                icLogDebug(_this->driverName, "Zone type: 0x%04"PRIx16, zoneType);
                if (strcmp(deviceClass, KEYPAD_DC) == 0)
                {
                    claimed = zoneType == IAS_ZONE_TYPE_KEYPAD;
                }
                else if(strcmp(deviceClass, KEYFOB_DC) == 0)
                {
                    claimed = zoneType == IAS_ZONE_TYPE_KEYFOB;
                }

                if (claimed)
                {
                    icLogDebug(_this->driverName, "Claimed device 0x%016" PRIx64, discoveredDeviceDetails->eui64);
                    break;
                }
            }
        }
    }

    return claimed;
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool ok = false;
    DeviceDriver *_this = (DeviceDriver *) ctx;

    for (int i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        const uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        if (icDiscoveredDeviceDetailsEndpointHasCluster(discoveredDeviceDetails, endpointId, IAS_ACE_CLUSTER_ID, false))
        {
            AUTO_CLEAN(free_generic__auto) const char *endpointNumber = zigbeeSubsystemEndpointIdAsString(endpointId);

            ok = iasZoneFetchInitialResourceValues(device, endpointNumber, SECURITY_CONTROLLER_PROFILE, endpointId,
                                                   discoveredDeviceDetails, initialResourceValues);
        }
    }

    if (!ok)
    {
        icLogError(_this->driverName, "No discovered endpoints have an IAS ACE client cluster");
    }

    return ok;
}

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues)
{
    bool ok = false;
    DeviceDriver *_this = (DeviceDriver *) ctx;

    for (int i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        const uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        if (icDiscoveredDeviceDetailsEndpointHasCluster(discoveredDeviceDetails, endpointId, IAS_ACE_CLUSTER_ID, false))
        {
            AUTO_CLEAN(free_generic__auto) const char *endpointNumber = zigbeeSubsystemEndpointIdAsString(endpointId);

            // FIXME: profile version should be set in constructor to avoid missing setting it.
            icDeviceEndpoint *endpoint = createEndpoint(device, endpointNumber, SECURITY_CONTROLLER_PROFILE, true);
            endpoint->profileVersion = SECURITY_CONTROLLER_PROFILE_VERSION;

            ok = iasZoneRegisterResources(device, endpoint, endpointId, discoveredDeviceDetails, initialResourceValues);

            /*
             * res is only used to check for a failure; it is saved in endpoint->resources by createEndpointResource.
             */
            icDeviceResource *res = NULL;
            if (ok)
            {
                res = createEndpointResource(endpoint,
                                             SECURITY_CONTROLLER_PROFILE_RESOURCE_ZONE_CHANGED,
                                             NULL,
                                             RESOURCE_TYPE_ZONE_CHANGED,
                                             RESOURCE_MODE_WRITEABLE,
                                             CACHING_POLICY_NEVER);
            }

            ok &= res != NULL;
            if (ok)
            {
                zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);
            }
            else
            {
                /* IAS Zone helper will report its own error(s) */
                icLogError(_this->driverName, "Unable to register resources");
                break;
            }
        }
    }

    if (!ok)
    {
        icLogError(_this->driverName, "No discovered endpoints have an IAS ACE client cluster");
    }

    return ok;
}

void onZoneStatusChanged(const uint64_t eui64,
                         const uint8_t endpointId,
                         const IASZoneStatusChangedNotification *status,
                         const ComcastBatterySavingData *batterySavingData,
                         const void *ctx)
{
    iasZoneStatusChangedHelper(eui64,
                               endpointId,
                               status,
                               (ZigbeeDriverCommon *) ctx);

    if(batterySavingData != NULL)
    {
        comcastBatterySavingHelperUpdateResources(eui64, batterySavingData, (ZigbeeDriverCommon *) ctx);
    }
}

static RequestSource getRequestSource(ZigbeeDriverCommon *ctx)
{
    const char *deviceClass = zigbeeDriverCommonGetDeviceClass(ctx);
    RequestSource source = REQUEST_SOURCE_INVALID;

    if (strcmp(deviceClass, KEYPAD_DC) == 0)
    {
        source = REQUEST_SOURCE_WIRELESS_KEYPAD;
    }
    else if (strcmp(deviceClass, KEYFOB_DC) == 0)
    {
        source = REQUEST_SOURCE_WIRELESS_KEYFOB;
    }

    return source;
}

static void updateLastInteractionDate(const uint64_t eui64, const DeviceDriver *_this)
{
    const DeviceServiceCallbacks *deviceServiceCallbacks = zigbeeDriverCommonGetDeviceService((ZigbeeDriverCommon *) _this);
    AUTO_CLEAN(free_generic__auto) const char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    AUTO_CLEAN(free_generic__auto) const char *dateStr = stringBuilder("%"PRIu64, getCurrentUnixTimeMillis());

    /*
     * This resource is created by iasZoneHelper, also count an arm/disarm/panic request as an interaction.
     * Panel status requests are usually passive, so they should not count as an interaction.
     */
    deviceServiceCallbacks->updateResource(deviceUuid,
                                           NULL,
                                           COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE,
                                           dateStr,
                                           NULL);
}

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource)
{
    bool ok = true;
    AUTO_CLEAN(free_generic__auto) char *sendStatus = NULL;
    DeviceDriver *_this = (DeviceDriver *) ctx;
    const char *mdName = NULL;
    const char *driverName = _this->driverName;
    const DeviceServiceCallbacks *deviceServiceCallbacks = zigbeeDriverCommonGetDeviceService(ctx);

    *baseDriverUpdatesResource = false;

    icLogDebug(driverName,
               "%s: endpoint %s: id=%s, previousValue=%s, newValue=%s",
               __FUNCTION__,
               resource->endpointId,
               resource->id,
               previousValue,
               newValue);

    if (stringCompare(resource->id, SECURITY_CONTROLLER_PROFILE_RESOURCE_ZONE_CHANGED, false) == 0)
    {
        sendStatus = deviceServiceCallbacks->getMetadata(resource->deviceUuid,
                                                         NULL,
                                                         ACE_DEVICE_METADATA_SEND_ZONE_STATUS);

        ok = true;

        if (stringToBool(sendStatus) == true)
        {
            AUTO_CLEAN(zoneChangedDestroy__auto) ZoneChanged *zoneChanged = zoneChangedFromJSON(newValue);

            if (zoneChanged == NULL)
            {
                /* Parse errors will be reported by zoneChangedFromJSON */
                return false;
            }
        }
    }

    return ok;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;
    switch (deviceId)
    {
        case IAS_ACE_DEVICE_ID:
            profile = SECURITY_CONTROLLER_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE
