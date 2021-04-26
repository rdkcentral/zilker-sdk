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
#include <string.h>
#include <icUtil/array.h>
#include <icUtil/stringUtils.h>
#include <zigbeeClusters/iasZoneCluster.h>
#include <deviceModelHelper.h>
#include <resourceTypes.h>
#include <zigbeeClusters/legacySecurityCluster.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <icTime/timeUtils.h>
#include <zigbeeLegacySecurityCommon/uc_common.h>
#include "zigbeeDriverCommon.h"
#include "zigbeeLegacySirenRepeaterDeviceDriver.h"

#define DRIVER_NAME "ZigbeeLegacySirenRepeaterDD"
#define DC_VERSION 1

#define LEGACY_SIREN_REPEATER_ENDPOINT_ID 1
#define LEGACY_SIREN_REPEATER_ENDPOINT_NAME "1"

#define SIREN_DEFAULT_VOLUME         10
#define STROBE_DEFAULT_BRIGHTNESS   100
#define STROBE_DEFAULT_ON_TIME       1

/* Private Data */
/* Intentionally empty to force claimDevice to claim devices based on deviceType */
static uint16_t myDeviceIds[] = { };
static ZigbeeCluster *legacySecurityCluster;

/* ZigbeeDriverCommonCallbacks */
static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *discoveredDeviceDetails);
static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices);

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails);

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

static void initiateFirmwareUpgrade(ZigbeeDriverCommon *ctx, const char *deviceUuid, const DeviceDescriptor *dd);

static void firmwareUpgradeFailed(ZigbeeDriverCommon *ctx, uint64_t eui64);

static void upgradeInProgress(uint64_t eui64,
                              bool inProgress,
                              const void *ctx);

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);
static void postDeviceRemoved(ZigbeeDriverCommon *ctx, icDevice *device);

/* Legacy Security Cluster callbacks */
static bool isGodparentPingSupported(const LegacyDeviceDetails *details, const void *ctx);

void deviceStatusChanged(uint64_t eui64,
                         uint8_t endpointId,
                         const uCStatusMessage *status,
                         const void *ctx);

void deviceInfoReceived(uint64_t eui64,
                        uint8_t endpointId,
                        uint32_t firmwareVersion,
                        const void *ctx);

/* Private functions */

DeviceDriver *zigbeeLegacySirenRepeaterDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    icLogDebug(DRIVER_NAME, "%s", __FUNCTION__);

    static const ZigbeeDriverCommonCallbacks myHooks = {
            .devicesLoaded = devicesLoaded,
            .configureDevice = configureDevice,
            .writeEndpointResource = writeEndpointResource,
            .postDeviceRemoved = postDeviceRemoved,
            .claimDevice = claimDevice,
            .fetchInitialResourceValues = fetchInitialResourceValues,
            .registerResources = registerResources,
            .mapDeviceIdToProfile = mapDeviceIdToProfile,
            .initiateFirmwareUpgrade = initiateFirmwareUpgrade,
            .firmwareUpgradeFailed = firmwareUpgradeFailed
    };

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DRIVER_NAME,
                                                                  WARNING_DEVICE_DC,
                                                                  DC_VERSION,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  deviceService,
                                                                  &myHooks);

    static const LegacySecurityClusterCallbacks legacySecurityClusterCallbacks = {
            .deviceStatusChanged = deviceStatusChanged,
            .isGodparentPingSupported = isGodparentPingSupported,
            .firmwareVersionReceived = deviceInfoReceived,
            .upgradeInProgress = upgradeInProgress,
    };

    legacySecurityCluster = legacySecurityClusterCreate(&legacySecurityClusterCallbacks, deviceService, myDriver);
    zigbeeDriverCommonAddCluster(myDriver, legacySecurityCluster);

    zigbeeDriverCommonSetBatteryBackedUp(myDriver);

    // Disable standard HA configuration - not supported by legacy devices
    zigbeeDriverCommonSkipConfiguration(myDriver);

    return myDriver;
}

static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *discoveredDeviceDetails)
{
    icLogDebug(DRIVER_NAME, "%s", __FUNCTION__);

    bool claimed;
    icHashMap *includedDevices = hashMapCreate();

    uint8_t type = REPEATER_SIREN_1;
    hashMapPutCopy(includedDevices, &type, 1, NULL, 0);

    type = MTL_REPEATER_SIREN;
    hashMapPutCopy(includedDevices, &type, 1, NULL, 0);

    claimed = legacySecurityClusterClaimDevice(legacySecurityCluster,
                                               discoveredDeviceDetails,
                                               includedDevices,
                                               NULL);
    hashMapDestroy(includedDevices, NULL);

    return claimed;
}

static void initiateFirmwareUpgrade(ZigbeeDriverCommon *ctx, const char *deviceUuid, const DeviceDescriptor *dd)
{
    icLogDebug(DRIVER_NAME, "%s: deviceUuid=%s", __FUNCTION__, deviceUuid);
    uint64_t eui64 = zigbeeSubsystemIdToEui64(deviceUuid);
    //let the cluster know its ok to upgrade
    legacySecurityClusterUpgradeFirmware(legacySecurityCluster, eui64, dd);
}

static void firmwareUpgradeFailed(ZigbeeDriverCommon *ctx, uint64_t eui64)
{
    (void) ctx;

    legacySecurityClusterHandleFirmwareUpgradeFailed(legacySecurityCluster, eui64);
}

static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices)
{
    const DeviceServiceCallbacks *deviceService = zigbeeDriverCommonGetDeviceService(ctx);

    legacySecurityClusterDevicesLoaded(legacySecurityCluster, deviceService, devices);
}

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails)
{
    const DeviceServiceCallbacks *deviceService = zigbeeDriverCommonGetDeviceService(ctx);
    icLogDebug(DRIVER_NAME, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    bool ok = legacySecurityClusterConfigureDevice(legacySecurityCluster, eui64, device, descriptor);
    return ok;
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool ok = false;
    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
    LegacyDeviceDetails *details = legacySecurityClusterGetDetailsCopy(legacySecurityCluster,
                                                                       discoveredDeviceDetails->eui64);

    if (details)
    {
        ok = legacySecurityClusterFetchInitialResourceValues(legacySecurityCluster,
                                                             discoveredDeviceDetails->eui64,
                                                             device,
                                                             discoveredDeviceDetails, initialResourceValues);

        initialResourceValuesPutEndpointValue(initialResourceValues, LEGACY_SIREN_REPEATER_ENDPOINT_NAME,
                                              COMMON_ENDPOINT_RESOURCE_TAMPERED,
                                              details->isTampered ? "true" : "false");

        initialResourceValuesPutEndpointValue(initialResourceValues, LEGACY_SIREN_REPEATER_ENDPOINT_NAME,
                                              WARNING_DEVICE_RESOURCE_TONE,
                                              WARNING_DEVICE_TONE_NONE);
    }

    return ok;
}

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues)
{
    bool ok = false;
    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
        LegacyDeviceDetails *details = legacySecurityClusterGetDetailsCopy(legacySecurityCluster,
                                                                           discoveredDeviceDetails->eui64);

    if (details)
    {
        icDeviceEndpoint *endpoint = NULL;
        uint8_t endpointId = LEGACY_SIREN_REPEATER_ENDPOINT_ID;

        ok = legacySecurityClusterRegisterResources(legacySecurityCluster,
                                                    discoveredDeviceDetails->eui64,
                                                    device,
                                                    discoveredDeviceDetails,
                                                    initialResourceValues);

        endpoint = createEndpoint(device, LEGACY_SIREN_REPEATER_ENDPOINT_NAME, WARNING_DEVICE_PROFILE, true);
        if (endpoint == NULL)
        {
            icLogError(DRIVER_NAME, "%s: unable to create endpoint on device %s", __FUNCTION__, device->uuid);
            ok = false;
        }

        if (ok && createEndpointResource(endpoint,
                                         WARNING_DEVICE_RESOURCE_SECURITY_STATE,
                                         NULL,
                                         RESOURCE_TYPE_SECURITY_STATE,
                                         RESOURCE_MODE_WRITEABLE,
                                         CACHING_POLICY_NEVER) == NULL)
        {
            icLogError(DRIVER_NAME,
                       "Unable to register resource %s on endpoint %s",
                       WARNING_DEVICE_RESOURCE_SECURITY_STATE,
                       LEGACY_SIREN_REPEATER_ENDPOINT_NAME);
            ok = false;
        }

        if (ok && createEndpointResourceIfAvailable(endpoint,
                                                    COMMON_ENDPOINT_RESOURCE_TAMPERED,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) == NULL)
        {
            icLogError(DRIVER_NAME,
                       "Unable to register resource %s on endpoint %s",
                       COMMON_ENDPOINT_RESOURCE_TAMPERED,
                       LEGACY_SIREN_REPEATER_ENDPOINT_NAME);

            ok = false;
        }

        if (ok && createEndpointResourceIfAvailable(endpoint,
                                                    WARNING_DEVICE_RESOURCE_TONE,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_WARNING_TONE,
                                                    RESOURCE_MODE_WRITEABLE,
                                                    CACHING_POLICY_ALWAYS) == NULL)
        {
            icLogError(DRIVER_NAME,
                       "Unable to register resource %s on endpoint %s",
                       WARNING_DEVICE_RESOURCE_TONE,
                       LEGACY_SIREN_REPEATER_ENDPOINT_NAME);

            ok = false;
        }

        zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);
    }

    return ok;
}

static bool isGodparentPingSupported(const LegacyDeviceDetails *details, const void *ctx)
{
    bool isSupported = false;
    uint32_t firmwareVer = convertLegacyFirmwareVersionToUint32(details->firmwareVer);

    /* Godparent ping is only supported on non-broken siren firmware */
    if (firmwareVer >= 0x00000304 && firmwareVer != 0x00000306)
    {
        isSupported = true;
    }

    return isSupported;
}

void deviceStatusChanged(const uint64_t eui64,
                         const uint8_t endpointId,
                         const uCStatusMessage *status,
                         const void *ctx)
{
    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    ZigbeeDriverCommon *_this = (ZigbeeDriverCommon *) ctx;
    const DeviceServiceCallbacks *deviceService = zigbeeDriverCommonGetDeviceService(_this);

    icLogDebug(DRIVER_NAME, "%s", __FUNCTION__);

    deviceService->updateResource(uuid,
                                  LEGACY_SIREN_REPEATER_ENDPOINT_NAME,
                                  COMMON_ENDPOINT_RESOURCE_TAMPERED,
                                  status->status.fields1.tamper ? "true" : "false",
                                  NULL);

    zigbeeDriverCommonUpdateAcMainsStatus((DeviceDriver *) _this, eui64, status->status.fields1.externalPowerFail == 0);

    /*
     * Siren repeaters may repeatedly send questionable bad battery reports.
     */
    zigbeeDriverCommonUpdateBatteryBadStatus((DeviceDriver *) _this, eui64, false);

    // Cleanup
    free(uuid);
}

void deviceInfoReceived(uint64_t eui64,
                        uint8_t endpointId,
                        uint32_t firmwareVersion,
                        const void *ctx)
{
    const DeviceServiceCallbacks *deviceService = zigbeeDriverCommonGetDeviceService((ZigbeeDriverCommon *) ctx);
}

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource)
{
    bool ok = true;
    const uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);

    icLogDebug(DRIVER_NAME, "%s: newValue: %s => %s", __FUNCTION__, resource->id, newValue);

    *baseDriverUpdatesResource = false;

    if (strcmp(resource->id, WARNING_DEVICE_RESOURCE_TONE) == 0)
    {
        ucTakeoverSirenSound sound = sirenSoundOff;
        uCSetWhiteLedMessage strobeMode = {
                .brightness = STROBE_DEFAULT_BRIGHTNESS,
                .onTime = STROBE_DEFAULT_ON_TIME
        };

        if (strcmp(newValue, WARNING_DEVICE_TONE_FIRE) == 0)
        {
            sound = sirenSoundFire;
        }
        else if (strcmp(newValue, WARNING_DEVICE_TONE_WARBLE) == 0 || strcmp(newValue, WARNING_DEVICE_TONE_CO) == 0)
        {
            sound = sirenSoundAlarm;
        }
        else
        {
            strobeMode.brightness = 0;
            strobeMode.onTime = 0;
        }

        uCWarningMessage warningMessage = {
                .sound = sound,
                .volume = SIREN_DEFAULT_VOLUME,
                .strobeMode = strobeMode
        };

        ok = legacySecurityClusterRepeaterSetWarning(legacySecurityCluster, eui64, &warningMessage);
    }

    return ok;
}

static void upgradeInProgress(uint64_t eui64,
                              bool inProgress,
                              const void *ctx)
{
    zigbeeDriverCommonSetBlockingUpgrade((ZigbeeDriverCommon *) ctx, eui64, inProgress);
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case LEGACY_ICONTROL_SENSOR_DEVICE_ID:
            profile = WARNING_DEVICE_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}

static void postDeviceRemoved(ZigbeeDriverCommon *ctx, icDevice *device)
{
    icLogDebug(DRIVER_NAME, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    legacySecurityClusterDeviceRemoved(legacySecurityCluster, eui64);
}


#endif //CONFIG_SERVICE_DEVICE_ZIGBEE
