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
#include <deviceServicePrivate.h>
#include <resourceTypes.h>
#include <zigbeeClusters/legacySecurityCluster.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <icTime/timeUtils.h>
#include "zigbeeDriverCommon.h"
#include "zigbeeLegacySecurityControllerDeviceDriver.h"

#define KEYPAD_DRIVER_NAME "ZigbeeLegacyKeypadDD"
#define KEYFOB_DRIVER_NAME "ZigbeeLegacyKeyfobDD"
#define KEYPAD_DC_VERSION 1
#define KEYFOB_DC_VERSION 1

#define LED_COMMAND_LENGTH 3
#define LED_DURATION_DEFAULT_S 3

#define LEGACY_SECURITY_CONTROLLER_ENDPOINT_ID "1"

#define ALL_PANIC_DISABLED_META_DATA "allPanicsDisabled"

/* Private Data */
/* Intentionally empty to force claimDevice to claim devices based on deviceType */
static uint16_t myDeviceIds[] = { };

typedef enum
{
    LED_MODE_OFF = 0,
    LED_MODE_SOLID,
    LED_MODE_FAST,
    LED_MODE_SLOW
} LEDMode;

typedef enum
{
    LED_COLOR_RED = 0,
    LED_COLOR_GREEN,
    LED_COLOR_AMBER
} LEDColor;

typedef struct {
    /* Do not free. Owned by driverCommon */
    ZigbeeCluster *legacySecurityCluster;
} PrivateData;


/* ZigbeeDriverCommonCallbacks */
static void preStartup(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds);
static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *discoveredDeviceDetails);
static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices);

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails);

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

static void upgradeInProgress(uint64_t eui64,
                              bool inProgress,
                              const void *ctx);

static void initiateFirmwareUpgrade(ZigbeeDriverCommon *ctx, const char *deviceUuid, const DeviceDescriptor *dd);

static void postShutdown(ZigbeeDriverCommon *ctx);

static void firmwareUpgradeFailed(ZigbeeDriverCommon *ctx, uint64_t eui64);

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);
static void postDeviceRemoved(ZigbeeDriverCommon *ctx, icDevice *device);

/* Legacy Security Cluster callbacks */
static void handleKeypadMessage(uint64_t eui64,
                                uint8_t endpointid,
                                const uCKeypadMessage *message,
                                const void *ctx);

static void handleKeyfobMessage(uint64_t eui64,
                                uint8_t endpointid,
                                const uCKeyfobMessage *message,
                                const void *ctx);

static void handleLegacySecurityControllerAction(ZigbeeDriverCommon *ctx,
                                                 uint64_t eui64,
                                                 uint8_t endpointId,
                                                 LegacyActionButton button,
                                                 const char *accessCode);

static bool isGodparentPingSupported(const LegacyDeviceDetails *details, const void *ctx);

/* Private functions */
static void sendLEDCommand(const ZigbeeDriverCommon *ctx,
                           uint64_t eui64,
                           uint8_t endpointId,
                           LEDMode ledMode,
                           LEDColor color,
                           uint8_t duration);

/**
 * Create an IAS ACE driver with a particular driver name and class. Keypads and keyfobs are really both IAS ACE
 * "securityController" devices
 */
static DeviceDriver *zigbeeLegacySecurityControllerDeviceDriverCreate(DeviceServiceCallbacks *deviceService,
                                                                      const char *driverName,
                                                                      const char *deviceClass,
                                                                      uint8_t dcVersion);

static DeviceDriver *zigbeeLegacySecurityControllerDeviceDriverCreate(DeviceServiceCallbacks *deviceService,
                                                                      const char *driverName,
                                                                      const char *deviceClass,
                                                                      uint8_t dcVersion)
{
    icLogDebug(driverName, "%s", __FUNCTION__);

    static const ZigbeeDriverCommonCallbacks myHooks = {
            .preStartup = preStartup,
            .devicesLoaded = devicesLoaded,
            .configureDevice = configureDevice,
            .postDeviceRemoved = postDeviceRemoved,
            .claimDevice = claimDevice,
            .fetchInitialResourceValues = fetchInitialResourceValues,
            .registerResources = registerResources,
            .mapDeviceIdToProfile = mapDeviceIdToProfile,
            .initiateFirmwareUpgrade = initiateFirmwareUpgrade,
            .postShutdown = postShutdown,
            .firmwareUpgradeFailed = firmwareUpgradeFailed
    };

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(driverName,
                                                                  deviceClass,
                                                                  dcVersion,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  deviceService,
                                                                  &myHooks);

    static const SecurityControllerCallbacks securityControllerCallbacks = {
            .handleKeypadMessage = handleKeypadMessage,
            .handleKeyfobMessage = handleKeyfobMessage
    };

    static const LegacySecurityClusterCallbacks legacySecurityClusterCallbacks = {
            .securityControllerCallbacks = &securityControllerCallbacks,
            .isGodparentPingSupported = isGodparentPingSupported,
            .upgradeInProgress = upgradeInProgress,
    };

    PrivateData *myData = malloc(sizeof(PrivateData));
    myData->legacySecurityCluster = legacySecurityClusterCreate(&legacySecurityClusterCallbacks, deviceService, myDriver);
    zigbeeDriverCommonAddCluster(myDriver, myData->legacySecurityCluster);

    zigbeeDriverCommonSetDriverPrivateData((ZigbeeDriverCommon *) myDriver, myData);

    // Disable standard HA configuration - not supported by legacy devices
    zigbeeDriverCommonSkipConfiguration(myDriver);

    return myDriver;
}

DeviceDriver *zigbeeLegacyKeypadDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    DeviceDriver *keypadDriver = zigbeeLegacySecurityControllerDeviceDriverCreate(deviceService,
                                                                                  KEYPAD_DRIVER_NAME,
                                                                                  KEYPAD_DC,
                                                                                  KEYPAD_DC_VERSION);
    return keypadDriver;
}

DeviceDriver *zigbeeLegacyKeyfobDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    DeviceDriver *keyfobDriver = zigbeeLegacySecurityControllerDeviceDriverCreate(deviceService,
                                                                                  KEYFOB_DRIVER_NAME,
                                                                                  KEYFOB_DC,
                                                                                  KEYFOB_DC_VERSION);

    return keyfobDriver;
}

static void preStartup(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds)
{
    const char *deviceClass = zigbeeDriverCommonGetDeviceClass(ctx);
    if (strcmp(deviceClass, KEYFOB_DC) == 0)
    {
        *commFailTimeoutSeconds = 0;
    }
}

static void postShutdown(ZigbeeDriverCommon *ctx)
{
    PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);
    free(myData);
}

static void firmwareUpgradeFailed(ZigbeeDriverCommon *ctx, uint64_t eui64)
{
    if (ctx != NULL)
    {
        const PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);
        if (myData != NULL)
        {
            legacySecurityClusterHandleFirmwareUpgradeFailed(myData->legacySecurityCluster, eui64);
        }
    }
}

static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *discoveredDeviceDetails)
{
    const DeviceDriver *_this = (DeviceDriver *) ctx;
    const PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);

    bool claimed = false;

    icLogDebug(_this->driverName, "%s", __FUNCTION__);

    const char *deviceClass = zigbeeDriverCommonGetDeviceClass(ctx);

    uint8_t type = LEGACY_DEVICE_TYPE_INVALID;

    if (strcmp(deviceClass, KEYPAD_DC) == 0)
    {
        type = KEYPAD_1;
    }
    else if (strcmp(deviceClass, KEYFOB_DC) == 0)
    {
        type = KEYFOB_1;
    }

    if (type != LEGACY_DEVICE_TYPE_INVALID)
    {
        icHashMap *includedDevices = hashMapCreate();

        hashMapPutCopy(includedDevices, &type, 1, NULL, 0);
        claimed = legacySecurityClusterClaimDevice(myData->legacySecurityCluster,
                                                   discoveredDeviceDetails,
                                                   includedDevices,
                                                   NULL);
        hashMapDestroy(includedDevices, NULL);
    }

    return claimed;
}

static void initiateFirmwareUpgrade(ZigbeeDriverCommon *ctx, const char *deviceUuid, const DeviceDescriptor *dd)
{
    const DeviceDriver *_this = (DeviceDriver *) ctx;
    const PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);

    icLogDebug(_this->driverName, "%s: deviceUuid=%s", __FUNCTION__, deviceUuid);
    uint64_t eui64 = zigbeeSubsystemIdToEui64(deviceUuid);
    //let the cluster know its ok to upgrade
    legacySecurityClusterUpgradeFirmware(myData->legacySecurityCluster, eui64, dd);
}

static void upgradeInProgress(uint64_t eui64,
                              bool inProgress,
                              const void *ctx)
{
    zigbeeDriverCommonSetBlockingUpgrade((ZigbeeDriverCommon *) ctx, eui64, inProgress);
}

static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices)
{
    const PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);
    const DeviceServiceCallbacks *deviceService = zigbeeDriverCommonGetDeviceService(ctx);

    legacySecurityClusterDevicesLoaded(myData->legacySecurityCluster, deviceService, devices);
}

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails)
{
    const DeviceDriver *_this = (DeviceDriver *) ctx;
    const PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);

    icLogDebug(_this->driverName, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    return legacySecurityClusterConfigureDevice(myData->legacySecurityCluster, eui64, device, descriptor);
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx,
                                       icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool ok = false;
    DeviceDriver *_this = (DeviceDriver *) ctx;
    const PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);

    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
    LegacyDeviceDetails *details = legacySecurityClusterGetDetailsCopy(myData->legacySecurityCluster,
                                                                       discoveredDeviceDetails->eui64);

    if (details)
    {
        const char *type = NULL;

        switch (details->devType)
        {
            case KEYPAD_1:
                type = SECURITY_CONTROLLER_PROFILE_KEYPAD_TYPE;
                break;
            case KEYFOB_1:
                type = SECURITY_CONTROLLER_PROFILE_KEYFOB_TYPE;
                break;
            default:
                icLogError(_this->driverName, "%s: unsupported device type [%d]", __FUNCTION__, details->devType);
                ok = false;
                break;
        }

        if (legacySecurityClusterFetchInitialResourceValues(myData->legacySecurityCluster,
                                                            discoveredDeviceDetails->eui64,
                                                            device,
                                                            discoveredDeviceDetails,
                                                            initialResourceValues) == true)
        {

            ok &= initialResourceValuesPutEndpointValue(initialResourceValues,
                                                        LEGACY_SECURITY_CONTROLLER_ENDPOINT_ID,
                                                        SECURITY_CONTROLLER_PROFILE_RESOURCE_TYPE,
                                                        type);

            ok &= initialResourceValuesPutDeviceValue(initialResourceValues,
                                                      COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE,
                                                      NULL);
        }
        else
        {
            icLogError(_this->driverName, "%s: %s failed to fetch legacy cluster initial resource values", __FUNCTION__,
                       device->uuid);
        }
    }

    return ok;

}

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues)
{
    bool ok = false;
    DeviceDriver *_this = (DeviceDriver *) ctx;
    const PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);

    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
        LegacyDeviceDetails *details = legacySecurityClusterGetDetailsCopy(myData->legacySecurityCluster,
                                                                           discoveredDeviceDetails->eui64);

    if (details)
    {
        icDeviceEndpoint *endpoint = NULL;
        uint8_t endpointId = (uint8_t) strtoul(LEGACY_SECURITY_CONTROLLER_ENDPOINT_ID, NULL, 10);

        ok = legacySecurityClusterRegisterResources(myData->legacySecurityCluster,
                                                    discoveredDeviceDetails->eui64,
                                                    device,
                                                    discoveredDeviceDetails,
                                                    initialResourceValues);

        endpoint = createEndpoint(device, LEGACY_SECURITY_CONTROLLER_ENDPOINT_ID, SECURITY_CONTROLLER_PROFILE, true);
        if (endpoint == NULL)
        {
            icLogError(_this->driverName, "%s: unable to create endpoint on device %s", __FUNCTION__, device->uuid);
            ok = false;
        }

        if (ok && createEndpointResourceIfAvailable(endpoint,
                                                    SECURITY_CONTROLLER_PROFILE_RESOURCE_TYPE,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_SECURITY_CONTROLLER_TYPE,
                                                    RESOURCE_MODE_READABLE,
                                                    CACHING_POLICY_ALWAYS) == NULL)
        {
            icLogError(_this->driverName,
                       "Unable to register resource %s on endpoint %s",
                       SECURITY_CONTROLLER_PROFILE_RESOURCE_TYPE,
                       LEGACY_SECURITY_CONTROLLER_ENDPOINT_ID);
            ok = false;
        }

        zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);

        // need to set the LPM flag for Keypads and Keyfobs
        // ZILKER-700
        //
        if (ok && createDeviceMetadata(device, LPM_POLICY_METADATA, lpmPolicyPriorityLabels[LPM_POLICY_ALWAYS]) == NULL)
        {
            icLogError(_this->driverName,
                       "Unable to register device metadata %s",
                       LPM_POLICY_METADATA);

            ok = false;
        }
    }

    return ok;
}

static bool isGodparentPingSupported(const LegacyDeviceDetails *details, const void *ctx)
{
    bool isSupported = false;
    uint32_t firmwareVer = convertLegacyFirmwareVersionToUint32(details->firmwareVer);

    /* Godparent ping is only supported on non-broken keypad firmware */
    if (details->devType == KEYPAD_1 && firmwareVer >= 0x00000309 && firmwareVer != 0x0000030E)
    {
        isSupported = true;
    }

    return isSupported;
}

void handleKeypadMessage(const uint64_t eui64,
                         const uint8_t endpointId,
                         const uCKeypadMessage *message,
                         const void *ctx)
{
    char accessCode[5];

    accessCode[4] = 0;
    memcpy(accessCode, message->code, 4);

    handleLegacySecurityControllerAction((ZigbeeDriverCommon *) ctx, eui64, endpointId, message->actionButton, accessCode);
}

static void handleKeyfobMessage(const uint64_t eui64,
                                const uint8_t endpointId,
                                const uCKeyfobMessage *message,
                                const void *ctx)
{
    handleLegacySecurityControllerAction((ZigbeeDriverCommon *) ctx, eui64, endpointId, message->buttons, NULL);
}

static void handleLegacySecurityControllerAction(ZigbeeDriverCommon *ctx,
                                                 const uint64_t eui64,
                                                 const uint8_t endpointId,
                                                 const LegacyActionButton button,
                                                 const char *accessCode)
{
    const DeviceDriver *_this = (DeviceDriver *) ctx;
    const DeviceServiceCallbacks *deviceServiceCallbacks = zigbeeDriverCommonGetDeviceService(ctx);

    icLogDebug(_this->driverName, "%s", __FUNCTION__);

    LegacyActionButton action = button;
    switch (button)
    {
        case LEGACY_ACTION_ARM_AWAY:
        case LEGACY_ACTION_ARM_STAY:
        case LEGACY_ACTION_PANIC:
        case LEGACY_ACTION_DISARM:
            if (!zigbeeDriverCommonIsDiscoveryActive(ctx))
            {
                if (button == LEGACY_ACTION_PANIC)
                {
                    AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
                    if (getBooleanMetadata(deviceUuid, NULL ,ALL_PANIC_DISABLED_META_DATA) == true)
                    {
                        icLogWarn(_this->driverName, "ignoring panic while all panics are disabled");
                        break;
                    }
                }
            }
            else
            {
                icLogInfo(_this->driverName, "ignoring arm/disarm/panic while discovery is active");
            }
            break;
        case LEGACY_ACTION_NONE:
            break;
        default:
            icLogWarn(_this->driverName, "%s: unsupported action button [%d]", __FUNCTION__, button);
            action = LEGACY_ACTION_NONE;
            break;
    }

    if (action != LEGACY_ACTION_NONE)
    {
        AUTO_CLEAN(free_generic__auto) const char *dateStr = stringBuilder("%"PRIu64, getCurrentUnixTimeMillis());
        AUTO_CLEAN(free_generic__auto) const char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

        deviceServiceCallbacks->updateResource(deviceUuid,
                                               NULL,
                                               COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE,
                                               dateStr,
                                               NULL);
    }
}

static void sendLEDCommand(const ZigbeeDriverCommon *ctx,
                           const uint64_t eui64,
                           const uint8_t endpointId,
                           const LEDMode ledMode,
                           const LEDColor color,
                           uint8_t duration)
{
    errno = 0;
    uint8_t payload[LED_COMMAND_LENGTH];
    sbZigbeeIOContext *zio = zigbeeIOInit(payload, ARRAY_LENGTH(payload), ZIO_WRITE);
    const DeviceDriver *_this = (DeviceDriver *) ctx;

    zigbeeIOPutUint8(zio, ledMode);
    zigbeeIOPutUint8(zio, duration);
    zigbeeIOPutUint8(zio, color);

    icLogDebug(_this->driverName, "%s: color: %d, mode: %d", __FUNCTION__, color, ledMode);
    if (errno)
    {
        AUTO_CLEAN(free_generic__auto) const char *errStr = strerrorSafe(errno);
        icLogWarn(_this->driverName, "%s: Unable to create LED command payload: %s", __FUNCTION__, errStr);
    }
    else if(zigbeeSubsystemSendMfgCommand(eui64,
                                          endpointId,
                                          IAS_ZONE_CLUSTER_ID,
                                          true,
                                          SET_LED,
                                          UC_MFG_ID_WRONG,
                                          payload,
                                          ARRAY_LENGTH(payload)) != 0)
    {
        icLogWarn(_this->driverName, "%s: Failed to send LED command", __FUNCTION__);
    }
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case LEGACY_ICONTROL_SENSOR_DEVICE_ID:
            profile = SECURITY_CONTROLLER_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}

static void postDeviceRemoved(ZigbeeDriverCommon *ctx, icDevice *device)
{
    const DeviceDriver *_this = (DeviceDriver *) ctx;
    const PrivateData *myData = zigbeeDriverCommonGetDriverPrivateData(ctx);

    icLogDebug(_this->driverName, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    legacySecurityClusterDeviceRemoved(myData->legacySecurityCluster, eui64);
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE
