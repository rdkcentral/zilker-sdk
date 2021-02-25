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
// Created by tlea on 2/13/19.
//
#include <icBuildtime.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <memory.h>
#include <icLog/logging.h>
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <commonDeviceDefs.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icConcurrent/threadPool.h>
#include <deviceCommunicationWatchdog.h>
#include <icConcurrent/delayedTask.h>
#include <deviceModelHelper.h>
#include <resourceTypes.h>
#include <zigbeeClusters/diagnosticsCluster.h>
#include <zigbeeClusters/temperatureMeasurementCluster.h>
#include <zigbeeClusters/powerConfigurationCluster.h>
#include <versionUtils.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <propsMgr/sslVerify.h>
#include <urlHelper/urlHelper.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <deviceDescriptors.h>
#include <zigbeeClusters/helpers/comcastBatterySavingHelper.h>
#include <icConcurrent/threadUtils.h>
#include <zigbeeClusters/otaUpgradeCluster.h>
#include <deviceServicePrivate.h>
#include <deviceService.h>
#include "zigbeeDriverCommon.h"
#include "zigbeeClusters/pollControlCluster.h"
#include "zigbeeClusters/alarmsCluster.h"


#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#define LOG_TAG "zigbeeDriverCommon"

#define FIRMWARE_FORMAT_STRING "0x%08" PRIx32
#define DEFAULT_COMM_FAIL_SECONDS               (60*60)
#define DISCOVERED_DEVICE_DETAILS               "discoveredDetails"
#define ZIGBEE_ENDPOINT_ID_METADATA_NAME        "zigbee_epid"
#define FIRMWARE_UPGRADE_RETRYDELAYSECS         "firmware.upgrade.retryDelaySecs"
#define FIRMWARE_UPGRADE_RETRYDELAYSECS_DEFAULT (60*60)
#define FIRMWARE_UPGRADE_DELAYSECS              "firmware.upgrade.delaySecs"
#define FIRMWARE_UPGRADE_DELAYSECS_DEFAULT      (2*60*60)

// These properties and defaults are used for battery savings during poll control checkin processing
// to determine if/when we should go out to the device to read them.
#define DEFAULT_BATTERY_VOLTAGE_REFRESH_MIN_SECONDS (24 * 60 * 60)//1 day
#define BATTERY_VOLTAGE_REFRESH_MIN_SECS_PROP "BatteryVoltageRefreshMinSecs"
#define DEFAULT_RSSI_REFRESH_MIN_SECONDS (25 * 60) //25 minutes
#define RSSI_REFRESH_MIN_SECS_PROP "FeRssiRefreshMinSecs"
#define DEFAULT_LQI_REFRESH_MIN_SECONDS (25 * 60) //25 minutes
#define LQI_REFRESH_MIN_SECS_PROP "FeLqiRefreshMinSecs"
#define DEFAULT_TEMP_REFRESH_MIN_SECONDS (50 * 60) //50 minutes
#define TEMP_REFRESH_MIN_SECS_PROP "TempRefreshMinSecs"

//This is stored within the DeviceDriver's callbackContext
struct ZigbeeDriverCommon
{
    //the main driver structure at the top
    DeviceDriver baseDriver;

    char *deviceClass;
    uint8_t deviceClassVersion;
    uint16_t *deviceIds;
    uint8_t numDeviceIds;
    ZigbeeSubsystemDeviceDiscoveredHandler discoveredHandler;
    ZigbeeSubsystemDeviceCallbacks *deviceCallbacks;
    icHashMap *clusters; //clusterId to ZigbeeCluster
    const DeviceServiceCallbacks *deviceServiceCallbacks;
    bool discoveryActive;
    icHashMap *discoveredDeviceDetails; //maps eui64 to IcDiscoveredDeviceDetails
    pthread_mutex_t discoveredDeviceDetailsMtx; //lock for discoveredDeviceDetails
    const ZigbeeDriverCommonCallbacks *commonCallbacks;
    uint32_t commFailTimeoutSeconds; // number of seconds of no communication with a device before it comes in commfail
    bool skipConfiguration; // if true, the common driver will not perform any discovery/configuration during pairing
    bool batteryBackedUp; // if true, configure battery related resources
    icHashMap *pendingFirmwareUpgrades; //delayed task handle to FirmwareUpgradeContext
    pthread_mutex_t pendingFirmwareUpgradesMtx;

    void *driverPrivate; //Private data for the higher level device driver
};

static void startup(void *ctx);

static void driverShutdown(void *ctx);

static void deviceRemoved(void *ctx, icDevice *device);

static void communicationFailed(void *ctx, icDevice *device);

static void communicationRestored(void *ctx, icDevice *device);

static bool processDeviceDescriptor(void *ctx,
                                    icDevice *device,
                                    DeviceDescriptor *dd);

static bool discoverStart(void *ctx, const char *deviceClass);

static void discoverStop(void *ctx, const char *deviceClass);

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor);

static bool deviceNeedsReconfiguring(void *ctx, icDevice *device);

static bool fetchInitialResourceValues(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);

static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues);

static bool devicePersisted(void *ctx, icDevice *device);

static bool readResource(void *ctx, icDeviceResource *resource, char **value);

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);

static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response);

static void attributeReportReceived(void *ctx, ReceivedAttributeReport *report);

static void clusterCommandReceived(void *ctx, ReceivedClusterCommand *command);

static void firmwareVersionNotify(void *ctx, uint64_t eui64, uint32_t currentVersion);

static void firmwareUpdateStarted(void *ctx, uint64_t eui64);

static void firmwareUpdateCompleted(void *ctx, uint64_t eui64);

static void firmwareUpdateFailed(void *ctx, uint64_t eui64);

static void deviceRejoined(void *ctx,
                           uint64_t eui64,
                           bool isSecure);

static void deviceLeft(void *ctx,
                       uint64_t eui64);

static void synchronizeDevice(void *ctx, icDevice *device);

static void endpointDisabled(void *ctx, icDeviceEndpoint *endpoint);

static bool deviceDiscoveredCallback(void *ctx, IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator);

static void discoveredDeviceDetailsFreeFunc(void *key, void *value);

static bool findDeviceResource(void *searchVal, void *item);

static void updateNeRssiAndLqi(void *ctx, uint64_t eui64, int8_t rssi, uint8_t lqi);

static void handleAlarmCommand(uint64_t eui64,
                               uint8_t endpointId,
                               const ZigbeeAlarmTableEntry *entries,
                               uint8_t numEntries,
                               const void *ctx);

static void handleAlarmClearedCommand(uint64_t eui64,
                                      uint8_t endpointId,
                                      const ZigbeeAlarmTableEntry *entries,
                                      uint8_t numEntries,
                                      const void *ctx);

static void handlePollControlCheckin(uint64_t eui64,
                                     uint8_t endpointId,
                                     const ComcastBatterySavingData *batterySavingData,
                                     const void *ctx);

static void triggerBackgroundResetToFactory(uint8_t epid, uint64_t eui64);

static void handleRssiLqiUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, int8_t rssi, uint8_t lqi);

static void handleTemperatureMeasurementMeasuredValueUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, int16_t value);

static void handleBatteryVoltageUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t decivolts);

static void handleBatteryPercentageRemainingUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t percent);

static void handleBatteryChargeStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryLow);

static void handleBatteryBadStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryBad);

static void handleBatteryMissingStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryMissing);

static void handleBatteryTemperatureStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isHigh);

static void handleBatteryRechargeCyclesChanged(void *ctx, uint64_t eui64, uint16_t rechargeCycles);

static void handleAcMainsStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool acMainsConnected);

static void destroyMapCluster(void *key, void *value);

static IcDiscoveredDeviceDetails *getDiscoveredDeviceDetails(uint64_t eui64, ZigbeeDriverCommon *commonDriver);

static void systemPowerEvent(void *ctx, DeviceServiceSystemPowerEventType powerEvent);

static void propertyChanged(void *ctx, cpePropertyEvent *event);

static void fetchRuntimeStats(void *ctx,  icStringHashMap *output);

static bool getDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version);

static void handleSubsystemInitialized(void *ctx);

static void handleBatteryRechargeCyclesChanged(void *ctx, uint64_t eui64, uint16_t rechargeCycles);

typedef struct
{
    uint64_t eui64;
    uint8_t epid;
} resetToFactoryData;

typedef struct
{
    DeviceDescriptor *dd;
    struct ZigbeeDriverCommon *commonDriver;
    char *deviceUuid;
    char *endpointId;
    uint32_t *taskHandle;
} FirmwareUpgradeContext;

static icHashMap *blockingUpgrades = NULL; //eui64 to NULL (a set)

static pthread_mutex_t blockingUpgradesMtx = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t blockingUpgradesCond = PTHREAD_COND_INITIALIZER;

static void destroyFirmwareUpgradeContext(FirmwareUpgradeContext *ctx);

static void processDeviceDescriptorMetadata(ZigbeeDriverCommon *commonDriver,
                                            icDevice *device,
                                            icStringHashMap *metadata);

static void doFirmwareUpgrade(void *arg);

static void processDeviceDescriptorMetadata(ZigbeeDriverCommon *commonDriver,
                                            icDevice *device,
                                            icStringHashMap *metadata);


static void pendingFirmwareUpgradeFreeFunc(void *key, void *value);


static void waitForUpgradesToComplete();

static const PollControlClusterCallbacks pollControlClusterCallbacks =
        {
                .checkin = handlePollControlCheckin,
        };

static const AlarmsClusterCallbacks alarmsClusterCallbacks =
        {
                .alarmReceived = handleAlarmCommand,
                .alarmCleared = handleAlarmClearedCommand
        };

static const DiagnosticsClusterCallbacks diagnosticsClusterCallbacks =
        {
                .lastMessageRssiLqiUpdated = handleRssiLqiUpdated,
        };

static const TemperatureMeasurementClusterCallbacks temperatureMeasurementClusterCallbacks =
        {
                .measuredValueUpdated = handleTemperatureMeasurementMeasuredValueUpdated,
        };

static const PowerConfigurationClusterCallbacks powerConfigurationClusterCallbacks =
        {
                .batteryVoltageUpdated = handleBatteryVoltageUpdated,
                .batteryPercentageRemainingUpdated = handleBatteryPercentageRemainingUpdated,
                .batteryChargeStatusUpdated = handleBatteryChargeStatusUpdated,
                .batteryBadStatusUpdated = handleBatteryBadStatusUpdated,
                .batteryMissingStatusUpdated = handleBatteryMissingStatusUpdated,
                .acMainsStatusUpdated = handleAcMainsStatusUpdated,
                .batteryTemperatureStatusUpdated = handleBatteryTemperatureStatusUpdated,
                .batteryRechargeCyclesChanged = handleBatteryRechargeCyclesChanged,
        };

DeviceDriver *zigbeeDriverCommonCreateDeviceDriver(const char *driverName,
                                                   const char *deviceClass,
                                                   uint8_t deviceClassVersion,
                                                   const uint16_t *deviceIds,
                                                   uint8_t numDeviceIds,
                                                   const DeviceServiceCallbacks *deviceServiceCallbacks,
                                                   const ZigbeeDriverCommonCallbacks *commonCallbacks)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) calloc(1, sizeof(ZigbeeDriverCommon));
    commonDriver->commonCallbacks = commonCallbacks;

    //standard DeviceDriver stuff
    commonDriver->baseDriver.driverName = strdup(driverName);
    commonDriver->baseDriver.subsystemName = strdup(ZIGBEE_SUBSYSTEM_NAME);
    commonDriver->baseDriver.startup = startup;
    commonDriver->baseDriver.shutdown = driverShutdown;
    commonDriver->baseDriver.discoverDevices = discoverStart;
    commonDriver->baseDriver.recoverDevices = discoverStart;
    commonDriver->baseDriver.stopDiscoveringDevices = discoverStop;
    commonDriver->baseDriver.configureDevice = configureDevice;
    commonDriver->baseDriver.deviceNeedsReconfiguring = deviceNeedsReconfiguring;
    commonDriver->baseDriver.fetchInitialResourceValues = fetchInitialResourceValues;
    commonDriver->baseDriver.registerResources = registerResources;
    commonDriver->baseDriver.devicePersisted = devicePersisted;
    commonDriver->baseDriver.readResource = readResource;
    commonDriver->baseDriver.writeResource = writeResource;
    commonDriver->baseDriver.executeResource = executeResource;
    commonDriver->baseDriver.deviceRemoved = deviceRemoved;
    commonDriver->baseDriver.communicationFailed = communicationFailed;
    commonDriver->baseDriver.communicationRestored = communicationRestored;
    commonDriver->baseDriver.supportedDeviceClasses = linkedListCreate();
    linkedListAppend(commonDriver->baseDriver.supportedDeviceClasses, strdup(deviceClass));
    commonDriver->baseDriver.processDeviceDescriptor = processDeviceDescriptor;
    commonDriver->baseDriver.synchronizeDevice = synchronizeDevice;
    commonDriver->baseDriver.endpointDisabled = endpointDisabled;
    commonDriver->baseDriver.systemPowerEvent = systemPowerEvent;
    commonDriver->baseDriver.propertyChanged = propertyChanged;
    commonDriver->baseDriver.fetchRuntimeStats = fetchRuntimeStats;
    commonDriver->baseDriver.getDeviceClassVersion = getDeviceClassVersion;
    commonDriver->baseDriver.subsystemInitialized = handleSubsystemInitialized;
    commonDriver->baseDriver.callbackContext = commonDriver;
    commonDriver->baseDriver.neverReject = false;

    commonDriver->deviceIds = (uint16_t *) calloc(numDeviceIds, sizeof(uint16_t));
    memcpy(commonDriver->deviceIds, deviceIds, numDeviceIds * sizeof(uint16_t));
    commonDriver->numDeviceIds = numDeviceIds;
    commonDriver->clusters = hashMapCreate();

    //Device discovered handler
    commonDriver->discoveredHandler.callback = deviceDiscoveredCallback;
    commonDriver->discoveredHandler.driverName = strdup(driverName);
    commonDriver->discoveredHandler.callbackContext = commonDriver;

    commonDriver->deviceServiceCallbacks = deviceServiceCallbacks;
    commonDriver->discoveryActive = false;
    commonDriver->discoveredDeviceDetails = hashMapCreate();
    pthread_mutex_init(&commonDriver->discoveredDeviceDetailsMtx, NULL);
    commonDriver->deviceClass = strdup(deviceClass);
    commonDriver->deviceClassVersion = deviceClassVersion;

    //ZigbeeSubsystem callbacks
    commonDriver->deviceCallbacks = (ZigbeeSubsystemDeviceCallbacks *) calloc(1,
                                                                              sizeof(ZigbeeSubsystemDeviceCallbacks));
    commonDriver->deviceCallbacks->callbackContext = commonDriver;
    commonDriver->deviceCallbacks->attributeReportReceived = attributeReportReceived;
    commonDriver->deviceCallbacks->clusterCommandReceived = clusterCommandReceived;
    commonDriver->deviceCallbacks->firmwareVersionNotify = firmwareVersionNotify;
    commonDriver->deviceCallbacks->firmwareUpdateStarted = firmwareUpdateStarted;
    commonDriver->deviceCallbacks->firmwareUpdateCompleted = firmwareUpdateCompleted;
    commonDriver->deviceCallbacks->firmwareUpdateFailed = firmwareUpdateFailed;
    commonDriver->deviceCallbacks->deviceRejoined = deviceRejoined;
    commonDriver->deviceCallbacks->deviceLeft = deviceLeft;

    commonDriver->commFailTimeoutSeconds = DEFAULT_COMM_FAIL_SECONDS; //can be overwritten by higher level driver

    commonDriver->batteryBackedUp = false; // can be overwritten by higher level driver

    //Add clusters that are common across all/most devices.

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 pollControlClusterCreate(&pollControlClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 alarmsClusterCreate(&alarmsClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 diagnosticsClusterCreate(&diagnosticsClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 temperatureMeasurementClusterCreate(&temperatureMeasurementClusterCallbacks,
                                                                     commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 powerConfigurationClusterCreate(&powerConfigurationClusterCallbacks, commonDriver));

    zigbeeDriverCommonAddCluster((DeviceDriver *) commonDriver,
                                 otaUpgradeClusterCreate());

    return (DeviceDriver *) commonDriver;
}

void zigbeeDriverCommonAddCluster(DeviceDriver *driver, ZigbeeCluster *cluster)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    uint16_t *clusterId = (uint16_t *) malloc(sizeof(uint16_t));
    *clusterId = cluster->clusterId;
    hashMapPut(commonDriver->clusters, clusterId, sizeof(cluster->clusterId), cluster);
}

void zigbeeDriverCommonSetEndpointNumber(icDeviceEndpoint *endpoint, uint8_t endpointNumber)
{
    if (endpoint != NULL)
    {
        char epid[4]; //max uint8_t in decimal plus null
        sprintf(epid, "%" PRIu8, endpointNumber);
        createEndpointMetadata(endpoint, ZIGBEE_ENDPOINT_ID_METADATA_NAME, epid);
    }
}

uint8_t zigbeeDriverCommonGetEndpointNumber(ZigbeeDriverCommon *ctx, icDeviceEndpoint *endpoint)
{
    uint8_t endpointId = 0;
    if (endpoint != NULL)
    {
        AUTO_CLEAN(free_generic__auto) const char *zigbeeEpId = ctx->deviceServiceCallbacks->getMetadata(
                   endpoint->deviceUuid,
                   endpoint->id,
                   ZIGBEE_ENDPOINT_ID_METADATA_NAME);

        if (zigbeeEpId != NULL)
        {
            // TODO: This may be a useful stringutils function
            errno = 0;
            char *bad = NULL;
            unsigned long tmp = strtoul(zigbeeEpId, &bad, 10);

            if (!errno)
            {
                if (*bad)
                {
                    errno = EINVAL;
                }
                else if (tmp > UINT8_MAX)
                {
                    errno = ERANGE;
                }
            }

            if (!errno)
            {
                endpointId = (uint8_t) tmp;
            }
            else
            {
                AUTO_CLEAN(free_generic__auto) const char *errStr = strerrorSafe(errno);
                icLogError(LOG_TAG, "Unable to convert %s to a Zigbee endpoint id: %s", endpoint->id, errStr);
            }
        }
        else
        {
            icLogError(LOG_TAG, "Unable to read endpoint metadata for %s on %s", ZIGBEE_ENDPOINT_ID_METADATA_NAME,
                       endpoint->uri);
        }
    }

    return endpointId;
}

DeviceDescriptor *zigbeeDriverCommonGetDeviceDescriptor(const char *manufacturer,
                                                        const char *model,
                                                        uint64_t hardwareVersion,
                                                        uint64_t firmwareVersion)
{
    char hw[21]; //max uint64_t plus null
    // Convert to decimal string, as that's what we expect everywhere
    sprintf(hw, "%" PRIu64, hardwareVersion);

    AUTO_CLEAN(free_generic__auto) char *fw = getZigbeeVersionString((uint32_t) (firmwareVersion));

    return deviceDescriptorsGet(manufacturer, model, hw, fw);
}

const DeviceServiceCallbacks *zigbeeDriverCommonGetDeviceService(ZigbeeDriverCommon *ctx)
{
    return ctx->deviceServiceCallbacks;
}

const char *zigbeeDriverCommonGetDeviceClass(ZigbeeDriverCommon *ctx)
{
    return ctx->deviceClass;
}

uint32_t zigbeeDriverCommonGetDeviceCommFailTimeout(DeviceDriver *driver)
{
    uint32_t retVal = DEFAULT_COMM_FAIL_SECONDS;

    if (driver != NULL)
    {
        ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;

        if (commonDriver->commFailTimeoutSeconds != 0)
        {
            retVal = commonDriver->commFailTimeoutSeconds;
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: unable to get commFailTimeoutSeconds for driver %s", __FUNCTION__, driver->driverName);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: invalid device driver", __FUNCTION__);
    }

    return retVal;
}

void zigbeeDriverCommonSetDeviceCommFailTimeout(DeviceDriver *driver, uint32_t commFailSeconds)
{
    if (driver != NULL)
    {
        ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
        commonDriver->commFailTimeoutSeconds = commFailSeconds;
    }
}

void zigbeeDriverCommonSkipConfiguration(DeviceDriver *driver)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    commonDriver->skipConfiguration = true;
}

static void startup(void *ctx)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *driverName = commonDriver->baseDriver.driverName;

    icLogDebug(LOG_TAG, "startup %s", driverName);

    commonDriver->pendingFirmwareUpgrades = hashMapCreate();
    pthread_mutex_init(&commonDriver->pendingFirmwareUpgradesMtx, NULL);

    if (commonDriver->commonCallbacks->preStartup != NULL)
    {
        commonDriver->commonCallbacks->preStartup(ctx, &commonDriver->commFailTimeoutSeconds);
    }

    icLinkedList *devices = commonDriver->deviceServiceCallbacks->getDevicesByDeviceDriver(driverName);
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
        zigbeeSubsystemRegisterDeviceListener(zigbeeSubsystemIdToEui64(device->uuid), commonDriver->deviceCallbacks);

        bool inCommFail = false;
        icDeviceResource *commFailureResource = (icDeviceResource *) linkedListFind(
                device->resources, COMMON_DEVICE_RESOURCE_COMM_FAIL, findDeviceResource);
        if (commFailureResource != NULL)
        {
            if (commFailureResource->value != NULL && strcasecmp(commFailureResource->value, "true") == 0)
            {
                inCommFail = true;
            }
        }

        if (commonDriver->commFailTimeoutSeconds != 0)
        {
            //start the comm fail watchdog for this device
            deviceCommunicationWatchdogMonitorDevice(device->uuid, commonDriver->commFailTimeoutSeconds, inCommFail);
        }
        else
        {
            icLogInfo(LOG_TAG, "Device communication watchdog disabled for %s %s", device->deviceClass, device->uuid);
        }
    }
    linkedListIteratorDestroy(iterator);

    if (commonDriver->commonCallbacks->devicesLoaded != NULL)
    {
        commonDriver->commonCallbacks->devicesLoaded(ctx, devices);
    }

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    zigbeeSubsystemRegisterDiscoveryHandler(commonDriver->baseDriver.driverName, &commonDriver->discoveredHandler);

    if (commonDriver->commonCallbacks->postStartup != NULL)
    {
        commonDriver->commonCallbacks->postStartup(ctx);
    }
}

static void driverShutdown(void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *driverName = commonDriver->baseDriver.driverName;

    icLogDebug(LOG_TAG, "shutdown %s", driverName);

    zigbeeSubsystemUnregisterDiscoveryHandler(&commonDriver->discoveredHandler);

    if (commonDriver->commonCallbacks->preShutdown != NULL)
    {
        commonDriver->commonCallbacks->preShutdown(ctx);
    }

    zigbeeSubsystemUnregisterDiscoveryHandler(&commonDriver->discoveredHandler);
    free(commonDriver->discoveredHandler.driverName);

    icLinkedList *devices = commonDriver->deviceServiceCallbacks->getDevicesByDeviceDriver(driverName);
    if (devices != NULL)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
        while (linkedListIteratorHasNext(iterator))
        {
            icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
            zigbeeSubsystemUnregisterDeviceListener(zigbeeSubsystemIdToEui64(device->uuid));

            //stop monitoring this device
            deviceCommunicationWatchdogStopMonitoringDevice(device->uuid);
        }
        linkedListIteratorDestroy(iterator);
        linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
    }
    free(commonDriver->deviceCallbacks);

    free(commonDriver->baseDriver.driverName);
    free(commonDriver->baseDriver.subsystemName);
    linkedListDestroy(commonDriver->baseDriver.supportedDeviceClasses, NULL);
    free(commonDriver->deviceClass);
    commonDriver->deviceServiceCallbacks = NULL;

    //Cancel all pending upgrade tasks and wait for any that are running to complete
    zigbeeDriverCommonCancelPendingUpgrades(commonDriver);
    waitForUpgradesToComplete();

    hashMapDestroy(commonDriver->pendingFirmwareUpgrades, pendingFirmwareUpgradeFreeFunc);

    free(commonDriver->deviceIds);
    hashMapDestroy(commonDriver->discoveredDeviceDetails, discoveredDeviceDetailsFreeFunc);
    pthread_mutex_destroy(&commonDriver->discoveredDeviceDetailsMtx);
    hashMapDestroy(commonDriver->clusters, destroyMapCluster);

    if (commonDriver->commonCallbacks->postShutdown != NULL)
    {
        commonDriver->commonCallbacks->postShutdown(ctx);
    }

    commonDriver->commonCallbacks = NULL;

    pthread_mutex_destroy(&commonDriver->pendingFirmwareUpgradesMtx);

    free(commonDriver);

    hashMapDestroy(blockingUpgrades, NULL);
    blockingUpgrades = NULL;
}

static uint8_t getEndpointNumber(void *ctx, const char *deviceUuid, const char *endpointId)
{
    uint8_t result = 0; // an invalid endpoint

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver != NULL)
    {
        char *epid = commonDriver->deviceServiceCallbacks->getMetadata(deviceUuid, endpointId,
                                                                       ZIGBEE_ENDPOINT_ID_METADATA_NAME);
        if (epid != NULL)
        {
            result = (uint8_t) atoi(epid);
            free(epid);
        }
    }

    return result;
}

static void deviceRemoved(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver != NULL && device != NULL && device->uuid != NULL)
    {
        //stop monitoring this device
        deviceCommunicationWatchdogStopMonitoringDevice(device->uuid);

        if (commonDriver->commonCallbacks->preDeviceRemoved != NULL)
        {
            commonDriver->commonCallbacks->preDeviceRemoved(ctx, device);
        }

        uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

        zigbeeSubsystemUnregisterDeviceListener(eui64);
        zigbeeSubsystemRemoveDeviceAddress(eui64);

        //just tell the first endpoint to reset to factory and leave... that should get the whole device
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListGetElementAt(device->endpoints, 0);
        bool willReset = false;
        if (endpoint != NULL)
        {
            sbIcLinkedListIterator *it = linkedListIteratorCreate(endpoint->metadata);
            while(linkedListIteratorHasNext(it))
            {
                icDeviceMetadata *metadata = linkedListIteratorGetNext(it);
                if (strcmp(metadata->id, ZIGBEE_ENDPOINT_ID_METADATA_NAME) == 0)
                {
                    const char *epName = metadata->value;
                    if (epName != NULL)
                    {
                        errno = 0;
                        char *bad = NULL;
                        long epid = strtoul(epName, &bad, 10);

                        if (errno == 0 && *bad == 0 && epid <= UINT8_MAX)
                        {
                            // Kick this off in the background so we don't block for devices which are sleepy
                            triggerBackgroundResetToFactory((uint8_t) epid, eui64);
                            willReset = true;
                            break;
                        }
                    }
                }
            }
        }

        if (!willReset)
        {
            icLogWarn(LOG_TAG, "Removed device was not told to reset to factory");
        }

        if (commonDriver->commonCallbacks->postDeviceRemoved != NULL)
        {
            commonDriver->commonCallbacks->postDeviceRemoved(ctx, device);
        }
    }

    // Go through and remove and unused firmware files now that something has been removed
    // This will do an overall scan, which is more work than is needed, but this event
    // is rare so this overhead is minimal
    zigbeeSubsystemCleanupFirmwareFiles();
}

static void communicationFailed(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver != NULL && device != NULL && device->uuid != NULL)
    {
        if (commonDriver->commonCallbacks->communicationFailed != NULL)
        {
            commonDriver->commonCallbacks->communicationFailed(ctx, device);
        }

        commonDriver->deviceServiceCallbacks->updateResource(device->uuid,
                                                             NULL,
                                                             COMMON_DEVICE_RESOURCE_COMM_FAIL,
                                                             "true",
                                                             NULL);
    }
}

static void communicationRestored(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver != NULL && device != NULL && device->uuid != NULL)
    {
        if (commonDriver->commonCallbacks->communicationRestored != NULL)
        {
            commonDriver->commonCallbacks->communicationRestored(ctx, device);
        }

        commonDriver->deviceServiceCallbacks->updateResource(device->uuid,
                                                             NULL,
                                                             COMMON_DEVICE_RESOURCE_COMM_FAIL,
                                                             "false",
                                                             NULL);
    }
}

static void destroyFirmwareUpgradeContext(FirmwareUpgradeContext *ctx)
{
    deviceDescriptorFree(ctx->dd);
    free(ctx->deviceUuid);
    free(ctx->endpointId);
    free(ctx->taskHandle);
    free(ctx);
}

static bool processDeviceDescriptor(void *ctx,
                                    icDevice *device,
                                    DeviceDescriptor *dd)
{
    bool result = true;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (dd == NULL)
    {
        icLogWarn(LOG_TAG, "processDeviceDescriptor: NULL dd argument; ignoring");
        return true;
    }

    if (device == NULL)
    {
        icLogWarn(LOG_TAG, "processDeviceDescriptor: NULL device argument; ignoring");
        return false;
    }

    if (dd->latestFirmware == NULL)
    {
        icLogWarn(LOG_TAG, "processDeviceDescriptor: No latest firmware for dd uuid: %s; ignoring", dd->uuid);
        return true;
    }

    icLogDebug(LOG_TAG, "processDeviceDescriptor: %s", device->uuid);

    // Get the device's current firmware version
    icDeviceResource *firmwareVersionResource = (icDeviceResource *) linkedListFind(
            device->resources, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, findDeviceResource);

    if (firmwareVersionResource != NULL)
    {
        //Get a handle to the provided device's firmware update status resource so we can
        // update it as well as attempting to update in the database since during pairing
        // it isnt persisted yet.
        icDeviceResource *firmwareUpdateStatusResource = (icDeviceResource *) linkedListFind(
                device->resources, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, findDeviceResource);

        bool firmwareUpgradeRequired = false;
        if(commonDriver->commonCallbacks->firmwareUpgradeRequired != NULL)
        {
            firmwareUpgradeRequired = commonDriver->commonCallbacks->firmwareUpgradeRequired(ctx,
                    device->uuid,
                    dd->latestFirmware->version,
                    firmwareVersionResource->value);
        }
        else
        {
            int versionComparison = compareVersionStrings(dd->latestFirmware->version, firmwareVersionResource->value);
            firmwareUpgradeRequired = versionComparison == -1;
        }

        if (firmwareUpgradeRequired)
        {
            icLogDebug(LOG_TAG, "processDeviceDescriptor: New firmware for device %s, at version %s, latest version %s",
                       device->uuid, firmwareVersionResource->value, dd->latestFirmware->version);

            commonDriver->deviceServiceCallbacks->updateResource(device->uuid,
                                                                 NULL,
                                                                 COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                                                                 FIRMWARE_UPDATE_STATUS_PENDING,
                                                                 NULL);
            if(firmwareUpdateStatusResource != NULL)
            {
                free(firmwareUpdateStatusResource->value);
                firmwareUpdateStatusResource->value = strdup(FIRMWARE_UPDATE_STATUS_PENDING);
            }

            if (dd->latestFirmware->filenames != NULL)
            {
                DeviceDescriptor *ddCopy = deviceDescriptorClone(dd);
                if (ddCopy == NULL)
                {
                    icLogError(LOG_TAG, "Failed to clone device descriptor for firmware download");
                    result = false;
                }
                else
                {
                    FirmwareUpgradeContext *firmwareUpgradeContext = (FirmwareUpgradeContext *) calloc(1,
                                                                                                          sizeof(FirmwareUpgradeContext));
                    firmwareUpgradeContext->dd = ddCopy;
                    firmwareUpgradeContext->deviceUuid = strdup(device->uuid);

                    icDeviceEndpoint *firstEndpoint = linkedListGetElementAt(device->endpoints, 0);
                    if (firstEndpoint != NULL)
                    {
                        firmwareUpgradeContext->endpointId = strdup(firstEndpoint->id);
                    }

                    firmwareUpgradeContext->taskHandle = malloc(sizeof(uint32_t));
                    firmwareUpgradeContext->commonDriver = commonDriver;

                    uint32_t delaySeconds;

                    if(getPropertyAsBool(ZIGBEE_FW_UPGRADE_NO_DELAY_BOOL_PROPERTY, false) == false)
                    {
                        delaySeconds = getPropertyAsUInt32(FIRMWARE_UPGRADE_DELAYSECS,
                                                           FIRMWARE_UPGRADE_DELAYSECS_DEFAULT);
                    }
                    else
                    {
                        delaySeconds = 1;
                    }

                    pthread_mutex_lock(&commonDriver->pendingFirmwareUpgradesMtx);

                    icLogInfo(LOG_TAG, "%s: scheduling firmware upgrade to start in %"PRIu32 " seconds.",
                            __FUNCTION__, delaySeconds);

                    *firmwareUpgradeContext->taskHandle = scheduleDelayTask(delaySeconds,
                                                                             DELAY_SECS,
                                                                             doFirmwareUpgrade,
                                                                             firmwareUpgradeContext);

                    if (*firmwareUpgradeContext->taskHandle == 0)
                    {
                        icLogError(LOG_TAG, "Failed to add task for firmware upgrade");
                        destroyFirmwareUpgradeContext(firmwareUpgradeContext);
                        result = false;
                    }
                    else
                    {
                        hashMapPut(commonDriver->pendingFirmwareUpgrades,
                                   firmwareUpgradeContext->taskHandle,
                                   sizeof(uint32_t),
                                   firmwareUpgradeContext);
                    }

                    pthread_mutex_unlock(&commonDriver->pendingFirmwareUpgradesMtx);
                }
            }
            else
            {
                icLogWarn(LOG_TAG, "No filenames in DD for uuid: %s", dd->uuid);
            }
        }
        else
        {
            icLogDebug(LOG_TAG, "Device %s does not need a firmware upgrade, skipping download", device->uuid);

            commonDriver->deviceServiceCallbacks->updateResource(device->uuid,
                                                                 NULL,
                                                                 COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                                                                 FIRMWARE_UPDATE_STATUS_UP_TO_DATE,
                                                                 NULL);

            if(firmwareUpdateStatusResource != NULL)
            {
                free(firmwareUpdateStatusResource->value);
                firmwareUpdateStatusResource->value = strdup(FIRMWARE_UPDATE_STATUS_UP_TO_DATE);
            }
        }

        if (dd->metadata != NULL)
        {
            processDeviceDescriptorMetadata(commonDriver, device, dd->metadata);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Unable to find firmware resource for device %s", device->uuid);
    }

    return result;
}

static bool discoverStart(void *ctx, const char *deviceClass)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(LOG_TAG, "%s: %s deviceClass=%s", __FUNCTION__, commonDriver->baseDriver.driverName, deviceClass);

    commonDriver->discoveryActive = true;

    zigbeeSubsystemStartDiscoveringDevices();

    return true;
}

static void discoverStop(void *ctx, const char *deviceClass)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(LOG_TAG, "%s: %s stopping discovery of %s", __FUNCTION__, commonDriver->baseDriver.driverName,
               deviceClass);

    commonDriver->discoveryActive = false;

    zigbeeSubsystemStopDiscoveringDevices();

    if (commonDriver->commonCallbacks->postDiscoverStop != NULL)
    {
        commonDriver->commonCallbacks->postDiscoverStop(ctx);
    }

}

/*
 * For the provided clusterIds, get the attribute infos and store them in the pre-allocated clusterDetails list
 */
static bool getAttributeInfos(uint64_t eui64,
                              uint8_t endpointId,
                              IcDiscoveredClusterDetails *clusterDetails,
                              uint8_t numClusterDetails)
{
    bool result = true;

    for (uint8_t i = 0; i < numClusterDetails; i++)
    {
        zhalAttributeInfo *attributeInfos = NULL;
        uint16_t numAttributeInfos = 0;

        if (zhalGetAttributeInfos(eui64,
                                  endpointId,
                                  clusterDetails[i].clusterId,
                                  clusterDetails[i].isServer,
                                  &attributeInfos,
                                  &numAttributeInfos) == 0)
        {
            clusterDetails[i].numAttributeIds = numAttributeInfos;
            // Cleanup the old stuff before allocating something new
            free(clusterDetails[i].attributeIds);
            clusterDetails[i].attributeIds = (uint16_t *) calloc(numAttributeInfos, sizeof(uint16_t));
            for (uint16_t j = 0; j < numAttributeInfos; j++)
            {
                clusterDetails[i].attributeIds[j] = attributeInfos[j].id;
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: failed to get attribute infos", __FUNCTION__);
            result = false;
        }

        free(attributeInfos);
    }

    return result;
}

static bool getDeviceAttributeInfos(uint64_t eui64,
                                      IcDiscoveredDeviceDetails *deviceDetails)
{
    bool result = true;

    for (int i = 0; i < deviceDetails->numEndpoints; i++)
    {
        //get server cluster attributes
        result = getAttributeInfos(deviceDetails->eui64,
                                   deviceDetails->endpointDetails[i].endpointId,
                                   deviceDetails->endpointDetails[i].serverClusterDetails,
                                   deviceDetails->endpointDetails[i].numServerClusterDetails);

        if (!result)
        {
            icLogError(LOG_TAG, "%s: failed to discover server cluster attributes", __FUNCTION__);
            break;
        }

        //get client cluster attributes
        result = getAttributeInfos(deviceDetails->eui64,
                                   deviceDetails->endpointDetails[i].endpointId,
                                   deviceDetails->endpointDetails[i].clientClusterDetails,
                                   deviceDetails->endpointDetails[i].numClientClusterDetails);

        if (!result)
        {
            icLogError(LOG_TAG, "%s: failed to discover client cluster attributes", __FUNCTION__);
            break;
        }
    }

    return result;
}

/**
 * Compute the cluster configuration order. Clusters with the same priority will be in no particular order within the
 * priority band.
 */
static icLinkedList *createClusterOrder(ZigbeeDriverCommon *commonDriver)
{
    icLinkedList *orderedClusters = linkedListCreate();

    icHashMapIterator *it = hashMapIteratorCreate(commonDriver->clusters);
    while (hashMapIteratorHasNext(it))
    {
        uint16_t *clusterId;
        uint16_t keyLen;
        ZigbeeCluster *cluster;

        hashMapIteratorGetNext(it, (void **) &clusterId, &keyLen, (void **) &cluster);

        //TODO: support insertAfter(void *) in icLinkedList to build a nicely ordered list
        switch (cluster->priority)
        {
            case CLUSTER_PRIORITY_DEFAULT:
                linkedListAppend(orderedClusters, cluster);
                break;
            case CLUSTER_PRIORITY_HIGHEST:
                linkedListPrepend(orderedClusters, cluster);
                break;
            default:
                icLogWarn(LOG_TAG, "Cluster priority [%d] not supported, assigning default priority", cluster->priority);
                break;
        }
    }
    hashMapIteratorDestroy(it);

    return orderedClusters;
}

bool zigbeeDriverCommonConfigureEndpointClusters(uint64_t eui64,
                                                 uint8_t endpointId,
                                                 ZigbeeDriverCommon *commonDriver,
                                                 IcDiscoveredDeviceDetails *deviceDetails,
                                                 DeviceDescriptor *descriptor)
{
    bool result = true;

    DeviceConfigurationContext deviceConfigContext;
    memset(&deviceConfigContext, 0, sizeof(DeviceConfigurationContext));
    deviceConfigContext.eui64 = eui64;
    deviceConfigContext.endpointId = endpointId;
    deviceConfigContext.deviceDescriptor = descriptor;
    deviceConfigContext.configurationMetadata = stringHashMapCreate();
    deviceConfigContext.discoveredDeviceDetails = deviceDetails;

    //allow each cluster to perform its configuration
    icLinkedList *orderedClusters = createClusterOrder(commonDriver);
    icLinkedListIterator *it = linkedListIteratorCreate(orderedClusters);
    while (linkedListIteratorHasNext(it))
    {
        ZigbeeCluster *cluster = linkedListIteratorGetNext(it);
        if (cluster->configureCluster != NULL)
        {
            //if this endpoint has this cluster, let the cluster configure it
            bool endpointHasCluster = icDiscoveredDeviceDetailsEndpointHasCluster(deviceDetails,
                                                                                  endpointId,
                                                                                  cluster->clusterId,
                                                                                  true);

            //if this cluster wasnt in the server clusters, maybe its in the client
            if (endpointHasCluster == false)
            {
                endpointHasCluster = icDiscoveredDeviceDetailsEndpointHasCluster(deviceDetails,
                                                                                 endpointId,
                                                                                 cluster->clusterId,
                                                                                 false);
            }

            if (endpointHasCluster)
            {
                bool doConfigure = true;

                if (commonDriver->commonCallbacks->preConfigureCluster != NULL)
                {
                    // Let the driver do any preconfiguration of the cluster and tell us whether it wants a cluster
                    // configured
                    doConfigure = commonDriver->commonCallbacks->preConfigureCluster(commonDriver,
                            cluster, &deviceConfigContext);
                }
                if (doConfigure)
                {
                    if(cluster->configureCluster(cluster, &deviceConfigContext) == false)
                    {
                        icLogError(LOG_TAG, "%s: cluster 0x%04x failed to configure", __FUNCTION__, cluster->clusterId);
                        result = false;
                        break;
                    }
                }
            }
        }
    }
    linkedListIteratorDestroy(it);
    linkedListDestroy(orderedClusters, standardDoNotFreeFunc);

    stringHashMapDestroy(deviceConfigContext.configurationMetadata, NULL);

    return result;
}

static bool configureClusters(uint64_t eui64,
                              ZigbeeDriverCommon *commonDriver,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              DeviceDescriptor *descriptor)
{
    bool result = true;

    for (int i = 0; result && i < discoveredDeviceDetails->numEndpoints; i++)
    {
        IcDiscoveredEndpointDetails *endpointDetails = &discoveredDeviceDetails->endpointDetails[i];
        result = zigbeeDriverCommonConfigureEndpointClusters(eui64,
                                                             endpointDetails->endpointId,commonDriver,
                                                             discoveredDeviceDetails,
                                                             descriptor);
    }

    return result;
}

static void registerNewDevice(icDevice *device, ZigbeeSubsystemDeviceCallbacks *deviceCallbacks, uint32_t commFailTimeoutSeconds)
{
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    zigbeeSubsystemRegisterDeviceListener(eui64, deviceCallbacks);

    if (commFailTimeoutSeconds != 0)
    {
        //start the comm fail watchdog for this device
        deviceCommunicationWatchdogMonitorDevice(device->uuid, commFailTimeoutSeconds, false);
    }
    else
    {
        icLogInfo(LOG_TAG, "Device communication watchdog disabled for %s %s", device->deviceClass, device->uuid);
    }
}

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor)
{
    bool result = true;

    if (ctx == NULL || device == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid args", __FUNCTION__);
        return false;
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;
    icLogDebug(LOG_TAG, "%s: %s configuring %s", __FUNCTION__, commonDriver->baseDriver.driverName, device->uuid);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    IcDiscoveredDeviceDetails *discoveredDeviceDetails = getDiscoveredDeviceDetails(eui64, commonDriver);

    if (discoveredDeviceDetails == NULL)
    {
        icLogError(LOG_TAG, "%s: discovered device details not found", __FUNCTION__);
        return false;
    }

    if (commonDriver->skipConfiguration == false)
    {
        //Before we configure the device, perform the detailed discovery of its attributes so we know what type of
        // capabilities we should configure
        result = getDeviceAttributeInfos(eui64, discoveredDeviceDetails);

        if (!result)
        {
            goto exit;
        }

        //allow each cluster to perform its configuration
        result = configureClusters(eui64, commonDriver, discoveredDeviceDetails, descriptor);

        if (!result)
        {
            goto exit;
        }
    }

    if (commonDriver->commonCallbacks->configureDevice != NULL)
    {
        if (commonDriver->commonCallbacks->configureDevice(ctx, device, descriptor, discoveredDeviceDetails) == false)
        {
            icLogError(LOG_TAG, "%s: higher level driver failed to configure device", __FUNCTION__);
            result = false;
            goto exit;
        }
    }

    exit:
    return result;
}

static bool deviceNeedsReconfiguring(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = false;

    if (ctx == NULL || device == NULL)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __func__);
        return false;
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->deviceNeedsReconfiguring != NULL)
    {
        result = commonDriver->commonCallbacks->deviceNeedsReconfiguring(ctx, device);
    }

    return result;
}

static char *getDefaultLabel(icInitialResourceValues *initialResourceValues, const char *uuid)
{
    char *label = NULL;

    const char *manufacturer = initialResourceValuesGetDeviceValue(initialResourceValues,
                                                                   COMMON_DEVICE_RESOURCE_MANUFACTURER);

    if (manufacturer != NULL && strlen(uuid) >= 4)
    {
        label = stringBuilder("%s%s", manufacturer, uuid + strlen(uuid) - 4);
    }

    return label;
}

// register resources that all zigbee devices would have.  It could determine which resources to add based
//  on information in the device (if it has a battery, etc)
static bool registerCommonZigbeeResources(ZigbeeDriverCommon *commonDriver,
                                          icDevice *device,
                                          IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                          icInitialResourceValues *initialResourceValues)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;

    if (device == NULL || discoveredDeviceDetails == NULL || initialResourceValues == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLinkedListIterator *it = NULL;

    // first add the discoveredDetails metadata
    cJSON *deviceDetailsJson = icDiscoveredDeviceDetailsToJson(discoveredDeviceDetails);
    char *detailsStr = cJSON_PrintUnformatted(deviceDetailsJson);
    cJSON_Delete(deviceDetailsJson);
    if (createDeviceMetadata(device, DISCOVERED_DEVICE_DETAILS, detailsStr) == NULL)
    {
        icLogError(LOG_TAG, "%s: failed to create discovered details metadata", __FUNCTION__);
        result = false;
        free(detailsStr);
        goto exit;
    }
    free(detailsStr);


    //create resources common for all endpoints created by the concrete driver
    it = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(it))
    {
        icDeviceEndpoint *endpoint = linkedListIteratorGetNext(it);

        // Create this regardless if there is a value there or not
        const char *label = initialResourceValuesGetEndpointValue(initialResourceValues, endpoint->id,
                                                                  COMMON_ENDPOINT_RESOURCE_LABEL);

        AUTO_CLEAN(free_generic__auto) char *defaultLabel = NULL;

#ifdef CONFIG_SERVICE_DEVICE_GENERATE_DEFAULT_LABELS
        // if there was no label in the initialResourceValues, create a default one
        if (label == NULL)
        {
            defaultLabel = getDefaultLabel(initialResourceValues, device->uuid);
        }
#endif

        result &= createEndpointResource(endpoint,
                                         COMMON_ENDPOINT_RESOURCE_LABEL,
                                         label == NULL ? defaultLabel : label,
                                         RESOURCE_TYPE_LABEL,
                                         RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS |
                                         RESOURCE_MODE_DYNAMIC,
                                         CACHING_POLICY_ALWAYS) != NULL;
    }
    linkedListIteratorDestroy(it);

    //fe rssi
    result &= createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_FERSSI,
                                              initialResourceValues,
                                              RESOURCE_TYPE_RSSI,
                                              RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                              CACHING_POLICY_ALWAYS) != NULL;

    //fe lqi
    result &= createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_FELQI,
                                              initialResourceValues,
                                              RESOURCE_TYPE_LQI,
                                              RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                              CACHING_POLICY_ALWAYS) != NULL;

    //ne rssi
    result &= createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_NERSSI,
                                              initialResourceValues,
                                              RESOURCE_TYPE_RSSI,
                                              RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                              CACHING_POLICY_ALWAYS) != NULL;

    //ne lqi
    result &= createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_NELQI,
                                              initialResourceValues,
                                              RESOURCE_TYPE_LQI,
                                              RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                              CACHING_POLICY_ALWAYS) != NULL;

    //temperature: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_TEMPERATURE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_TEMPERATURE,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                    CACHING_POLICY_ALWAYS);

    //highTemperature: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_HIGH_TEMPERATURE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                    CACHING_POLICY_ALWAYS);

    //battery low: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_LOW,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    //battery voltage: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BATTERY_VOLTAGE,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                    CACHING_POLICY_ALWAYS);

    //ac mains connected: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    //battery bad: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_BAD,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    //battery missing: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_MISSING,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    //battery high temperature: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_HIGH_TEMPERATURE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_BOOLEAN,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    //battery percentage remaining: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_BATTERY_PERCENTAGE_REMAINING,
                                    initialResourceValues,
                                    RESOURCE_TYPE_PERCENTAGE,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                    CACHING_POLICY_ALWAYS);

    // last user interaction date: optional
    createDeviceResourceIfAvailable(device,
                                    COMMON_DEVICE_RESOURCE_LAST_USER_INTERACTION_DATE,
                                    initialResourceValues,
                                    RESOURCE_TYPE_DATETIME,
                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                    CACHING_POLICY_ALWAYS);

    exit:

    return result;
}

// fetch resources values that all zigbee devices would have.  It could determine which resources to populate based
//  on information in the device (if it has a battery, etc)
static bool fetchCommonZigbeeResourceValues(ZigbeeDriverCommon *commonDriver,
                                            icDevice *device,
                                            IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                            icInitialResourceValues *initialResourceValues)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;

    if (device == NULL || discoveredDeviceDetails == NULL || initialResourceValues == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint8_t epid;
    if (icDiscoveredDeviceDetailsGetAttributeEndpoint(discoveredDeviceDetails,
                                                      DIAGNOSTICS_CLUSTER_ID,
                                                      DIAGNOSTICS_LAST_MESSAGE_RSSI_ATTRIBUTE_ID, //if it has either...
                                                      &epid))
    {
        char tmp[5]; //-127 + \0 worst case

        //fe rssi
        int8_t rssi;
        if (diagnosticsClusterGetLastMessageRssi(discoveredDeviceDetails->eui64, epid, &rssi))
        {
            snprintf(tmp, 5, "%"PRId8, rssi);
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FERSSI, tmp);
        }
        else
        {
            result = false;
            goto exit;
        }

        //fe lqi
        uint8_t lqi;
        if (diagnosticsClusterGetLastMessageLqi(discoveredDeviceDetails->eui64, epid, &lqi))
        {
            snprintf(tmp, 4, "%"PRIu8, lqi);
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FELQI, tmp);
        }
        else
        {
            result = false;
            goto exit;
        }
    }
    else
    {
        // Just provide NULL defaults if the driver didn't provide anything, since these are resources we always create
        initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_FERSSI, NULL);
        initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_FELQI, NULL);
    }

    // Just default these with NULL, they don't get populated until later
    initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_NERSSI, NULL);
    initialResourceValuesPutDeviceValueIfNotExists(initialResourceValues, COMMON_DEVICE_RESOURCE_NELQI, NULL);

    if (icDiscoveredDeviceDetailsGetAttributeEndpoint(discoveredDeviceDetails,
                                                      TEMPERATURE_MEASUREMENT_CLUSTER_ID,
                                                      TEMP_MEASURED_VALUE_ATTRIBUTE_ID,
                                                      &epid))
    {
        //temperature
        int16_t value;

        if (temperatureMeasurementClusterGetMeasuredValue(discoveredDeviceDetails->eui64, epid, &value))
        {
            char tmp[7]; //-32767 + \0 worst case
            snprintf(tmp, 7, "%"PRId16, value);
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_TEMPERATURE, tmp);
        }
        else
        {
            result = false;
            goto exit;
        }
    }

    icLogDebug(LOG_TAG, "Is battery powered=%s, is battery backed up=%s",
               discoveredDeviceDetails->powerSource == powerSourceBattery ? "true" : "false",
               commonDriver->batteryBackedUp ? "true" : "false");

    if (discoveredDeviceDetails->powerSource == powerSourceBattery || commonDriver->batteryBackedUp)
    {
        //battery low
        if (!initialResourceValuesHasDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_LOW))
        {
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_LOW, "false");
        }

        if (icDiscoveredDeviceDetailsGetAttributeEndpoint(discoveredDeviceDetails,
                                                          POWER_CONFIGURATION_CLUSTER_ID,
                                                          BATTERY_VOLTAGE_ATTRIBUTE_ID,
                                                          &epid))
        {
            uint8_t value;

            //battery voltage
            if (powerConfigurationClusterGetBatteryVoltage(discoveredDeviceDetails->eui64, epid, &value))
            {
                char tmp[6]; //25500 + \0 worst case
                snprintf(tmp, 6, "%u", (unsigned int) value * 100);
                initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE, tmp);
            }
        }

    }

    if (commonDriver->batteryBackedUp)
    {
        //ac mains connected
        if (!initialResourceValuesHasDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED))
        {
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED,
                                                "false");
        }

        //battery bad
        if (!initialResourceValuesHasDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_BAD))
        {
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_BAD, "false");
        }

        //battery missing
        if (!initialResourceValuesHasDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_MISSING))
        {
            initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_MISSING, "false");
        }

        //battery percentage remaining
        if (icDiscoveredDeviceDetailsGetAttributeEndpoint(discoveredDeviceDetails,
                                                          POWER_CONFIGURATION_CLUSTER_ID,
                                                          BATTERY_PERCENTAGE_REMAINING_ATTRIBUTE_ID,
                                                          &epid))
        {
            uint8_t value;

            if (powerConfigurationClusterGetBatteryPercentageRemaining(discoveredDeviceDetails->eui64, epid, &value))
            {
                char tmp[4]; //100 + \0 worst case
                snprintf(tmp, 4, "%u", (unsigned int) value);
                initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_BATTERY_PERCENTAGE_REMAINING, tmp);
            }
        }

    }

    exit:
    return result;
}

static bool fetchInitialResourceValues(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    bool result = true;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
    IcDiscoveredDeviceDetails *discoveredDeviceDetails = hashMapGet(commonDriver->discoveredDeviceDetails,
                                                                    &eui64, sizeof(uint64_t));
    pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

    if (commonDriver->commonCallbacks->fetchInitialResourceValues != NULL)
    {
        result = commonDriver->commonCallbacks->fetchInitialResourceValues(ctx,
                                                                           device,
                                                                           discoveredDeviceDetails,
                                                                           initialResourceValues);
    }

    if (result)
    {
        result = fetchCommonZigbeeResourceValues(commonDriver, device, discoveredDeviceDetails, initialResourceValues);
    }
    else
    {
        icLogError(LOG_TAG, "%s: %s driver failed to fetch initial resource values", __FUNCTION__, device->uuid);
    }

    return result;
}

static bool registerResources(void *ctx, icDevice *device, icInitialResourceValues *initialResourceValues)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    bool result = true;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
    IcDiscoveredDeviceDetails *discoveredDeviceDetails = hashMapGet(commonDriver->discoveredDeviceDetails,
                                                                    &eui64, sizeof(uint64_t));
    pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

    if (commonDriver->commonCallbacks->registerResources != NULL)
    {
        result = commonDriver->commonCallbacks->registerResources(ctx, device, discoveredDeviceDetails, initialResourceValues);
    }

    if (result)
    {
        result = registerCommonZigbeeResources(commonDriver, device, discoveredDeviceDetails, initialResourceValues);
    }

    return result;
}

static bool devicePersisted(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    bool result = true;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // Finish registering the new device
    registerNewDevice(device, commonDriver->deviceCallbacks, commonDriver->commFailTimeoutSeconds);

    if (commonDriver->commonCallbacks->devicePersisted != NULL)
    {
        result = commonDriver->commonCallbacks->devicePersisted(ctx, device);
    }

    //update the addresses and flags
    zigbeeSubsystemSetAddresses();

    return result;
}

static bool readResource(void *ctx, icDeviceResource *resource, char **value)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = false;

    if (ctx == NULL || resource == NULL || value == NULL)
    {
        return false;
    }

    icLogDebug(LOG_TAG, "readResource %s", resource->id);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (resource->endpointId == NULL)
    {
        if (commonDriver->commonCallbacks->readDeviceResource != NULL)
        {
            return commonDriver->commonCallbacks->readDeviceResource(ctx, resource, value);
        }
    }
    else
    {
        if (commonDriver->commonCallbacks->readEndpointResource != NULL)
        {
            uint8_t epid = getEndpointNumber(ctx, resource->deviceUuid, resource->endpointId);
            return commonDriver->commonCallbacks->readEndpointResource(ctx, epid, resource, value);
        }
    }

    return result;
}

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;
    bool updateResource = true;

    if (ctx == NULL || resource == NULL)
    {
        icLogDebug(LOG_TAG, "writeResource: invalid arguments");
        return false;
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (resource->endpointId == NULL)
    {
        if (commonDriver->commonCallbacks->writeDeviceResource != NULL)
        {
            result = commonDriver->commonCallbacks->writeDeviceResource(ctx, resource, previousValue, newValue);
        }
    }
    else
    {
        // dont pass resource writes that we manage to the owning driver
        if (strcmp(resource->id, COMMON_ENDPOINT_RESOURCE_LABEL) != 0 &&
            commonDriver->commonCallbacks->writeEndpointResource != NULL)
        {
            uint8_t epid = getEndpointNumber(ctx, resource->deviceUuid, resource->endpointId);
            result = commonDriver->commonCallbacks->writeEndpointResource(ctx, epid, resource, previousValue, newValue,
                                                                          &updateResource);
        }
    }

    if (result && updateResource)
    {
        commonDriver->deviceServiceCallbacks->updateResource(resource->deviceUuid,
                                                             resource->endpointId,
                                                             resource->id,
                                                             newValue,
                                                             NULL);
    }

    return result;
}

static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;

    if (ctx == NULL || resource == NULL)
    {
        icLogDebug(LOG_TAG, "executeResource: invalid arguments");
        return false;
    }

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (resource->endpointId == NULL)
    {
        if (commonDriver->commonCallbacks->executeDeviceResource != NULL)
        {
            result = commonDriver->commonCallbacks->executeDeviceResource(ctx, resource, arg, response);
        }
    }
    else
    {
        if (commonDriver->commonCallbacks->executeEndpointResource != NULL)
        {
            uint8_t epid = getEndpointNumber(ctx, resource->deviceUuid, resource->endpointId);
            result = commonDriver->commonCallbacks->executeEndpointResource(ctx, epid, resource, arg, response);
        }
    }

    return result;
}

static void attributeReportReceived(void *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    //update ne rssi and lqi
    updateNeRssiAndLqi(ctx, report->eui64, report->rssi, report->lqi);

    //forward to the owning cluster
    ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &report->clusterId, sizeof(report->clusterId));
    if (cluster != NULL)
    {
        if (cluster->handleAttributeReport != NULL)
        {
            cluster->handleAttributeReport(cluster, report);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: no cluster registered to handle the command", __FUNCTION__);
    }

    //always let the actual driver have a crack at it too
    if (commonDriver->commonCallbacks->handleAttributeReport != NULL)
    {
        commonDriver->commonCallbacks->handleAttributeReport(ctx, report);
    }
}

static void clusterCommandReceived(void *ctx, ReceivedClusterCommand *command)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    //update ne rssi and lqi
    updateNeRssiAndLqi(ctx, command->eui64, command->rssi, command->lqi);

    //forward to the owning cluster
    ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &command->clusterId, sizeof(command->clusterId));
    if (cluster != NULL)
    {
        if (cluster->handleClusterCommand != NULL)
        {
            cluster->handleClusterCommand(cluster, command);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: no cluster registered to handle the command", __FUNCTION__);
    }

    //always let the actual driver have a crack at it too
    if (commonDriver->commonCallbacks->handleClusterCommand != NULL)
    {
        commonDriver->commonCallbacks->handleClusterCommand(ctx, command);
    }
}

static void firmwareVersionNotify(void *ctx, uint64_t eui64, uint32_t currentVersion)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    AUTO_CLEAN(free_generic__auto) char *fw = getZigbeeVersionString(currentVersion);

    AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    //Read the currently known firmware version and compare with what we just got sent to determine if a firmware
    // update just completed.
    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *fwRes =
            commonDriver->deviceServiceCallbacks->getResource(deviceUuid,
                                                              NULL,
                                                              COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    if(fwRes != NULL)
    {
        if(strcmp(fw, fwRes->value) != 0)
        {
            firmwareUpdateCompleted(ctx, eui64);
        }

        commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                             NULL,
                                                             COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                                                             fw,
                                                             NULL);
    }

    // Go through and remove and unused firmware files now that something has upgraded
    // This will do an overall scan, which is more work than is needed, but this event
    // is rare so this overhead is minimal
    zigbeeSubsystemCleanupFirmwareFiles();
}

static void firmwareUpdateStarted(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         NULL,
                                                         COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                                                         FIRMWARE_UPDATE_STATUS_STARTED,
                                                         NULL);

    // Cleanup
    free(deviceUuid);
}

static void firmwareUpdateCompleted(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         NULL,
                                                         COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                                                         FIRMWARE_UPDATE_STATUS_COMPLETED,
                                                         NULL);

    // Cleanup
    free(deviceUuid);
}

static void firmwareUpdateFailed(void *ctx, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    // Forward to subscribing drivers
    if (commonDriver->commonCallbacks->firmwareUpgradeFailed != NULL)
    {
        commonDriver->commonCallbacks->firmwareUpgradeFailed(commonDriver, eui64);
    }

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         NULL,
                                                         COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                                                         FIRMWARE_UPDATE_STATUS_FAILED,
                                                         NULL);

    // Cleanup
    free(deviceUuid);
}

static void deviceRejoined(void *ctx,
                           uint64_t eui64,
                           bool isSecure)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(LOG_TAG, "%s: driver %s", __FUNCTION__, commonDriver->baseDriver.driverName);

    if (commonDriver->commonCallbacks->deviceRejoined != NULL)
    {
        IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, commonDriver);
        commonDriver->commonCallbacks->deviceRejoined(ctx, eui64, isSecure, details);
    }
}

static void deviceLeft(void *ctx,
                       uint64_t eui64)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    icLogDebug(LOG_TAG, "%s: driver %s", __FUNCTION__, commonDriver->baseDriver.driverName);

    if (commonDriver->commonCallbacks->deviceLeft != NULL)
    {
        IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, commonDriver);
        commonDriver->commonCallbacks->deviceLeft(ctx, eui64, details);
    }
}

// Called by zigbeeSubsystem.
// return true if we own this device.  The provided details are fully filled out with the exception
// of information about attributes since that discovery takes a while to perform and we are trying
// to get device discovery events out as quickly as possible.
static bool deviceDiscoveredCallback(void *ctx, IcDiscoveredDeviceDetails *details, DeviceMigrator *deviceMigrator)
{
    bool result = false;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    //silently ignore if this driver instance is not discovering and we aren't migrating
    if (commonDriver->discoveryActive == false && deviceMigrator == NULL)
    {
        return false;
    }

    icLogDebug(LOG_TAG, "%s: driver %s", __FUNCTION__, commonDriver->baseDriver.driverName);

    if (commonDriver->commonCallbacks->claimDevice != NULL)
    {
        result = commonDriver->commonCallbacks->claimDevice(ctx, details);
    }

    //we may have had a customClaimCallback registered, but want to let regular deviceid matching work
    if (!result)
    {
        if (details->numEndpoints == 0)
        {
            return false;
        }

        //we expect the first endpoint to match one of our device ids
        for (int i = 0; i < commonDriver->numDeviceIds; i++)
        {
            if (details->endpointDetails[0].appDeviceId == commonDriver->deviceIds[i])
            {
                result = true;
                break;
            }
        }

        if (!result)
        {
            icLogDebug(LOG_TAG,
                       "%s: deviceId %04x does not match any of our (%s) device ids",
                       __FUNCTION__,
                       details->endpointDetails[0].appDeviceId,
                       commonDriver->baseDriver.driverName);
        }
    }

    if (result)
    {
        //save off a copy of the discovered device details for use later if the device is kept.  Delete any previous
        // entry
        pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
        IcDiscoveredDeviceDetails *detailsCopy = cloneIcDiscoveredDeviceDetails(details);
        hashMapDelete(commonDriver->discoveredDeviceDetails,
                      &details->eui64,
                      sizeof(uint64_t),
                      discoveredDeviceDetailsFreeFunc);
        hashMapPut(commonDriver->discoveredDeviceDetails, &detailsCopy->eui64, sizeof(uint64_t), detailsCopy);
        pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

        //if we got here either the higher level device driver claimed it or it matched one of the device ids
        char *uuid = zigbeeSubsystemEui64ToId(details->eui64);

        char hw[21]; //max uint64_t plus null
        // Convert to decimal string, as that's what we expect everywhere
        sprintf(hw, "%" PRIu64, details->hardwareVersion);

        char *fw = getZigbeeVersionString((uint32_t) (details->firmwareVersion));

        icStringHashMap *metadata = NULL;

        if(commonDriver->commonCallbacks->getDiscoveredDeviceMetadata != NULL)
        {
            metadata = stringHashMapCreate();
            commonDriver->commonCallbacks->getDiscoveredDeviceMetadata(ctx, details, metadata);
        }

        // Provide some more information in the form of a mapping of endpoint id to its profile
        icStringHashMap *endpointProfileMap = stringHashMapCreate();
        if (commonDriver->commonCallbacks->mapDeviceIdToProfile != NULL)
        {
            for (int i = 0; i < details->numEndpoints; ++i)
            {
                const char *profile = commonDriver->commonCallbacks->mapDeviceIdToProfile(ctx,
                        details->endpointDetails[i].appDeviceId);
                if (profile != NULL)
                {
                    char *key = stringBuilder("%d", details->endpointDetails[i].endpointId);
                    stringHashMapPut(endpointProfileMap, key, strdup(profile));
                }
            }
        }

        DeviceFoundDetails deviceFoundDetails = {
                .deviceDriver = (DeviceDriver *) commonDriver,
                .deviceMigrator = deviceMigrator,
                .subsystem = ZIGBEE_SUBSYSTEM_NAME,
                .deviceClass = commonDriver->deviceClass,
                .deviceClassVersion = commonDriver->deviceClassVersion,
                .deviceUuid = uuid,
                .manufacturer = details->manufacturer,
                .model = details->model,
                .hardwareVersion = hw,
                .firmwareVersion = fw,
                .metadata = metadata,
                .endpointProfileMap = endpointProfileMap
        };

        if (commonDriver->deviceServiceCallbacks->deviceFound(&deviceFoundDetails,
                                                              commonDriver->baseDriver.neverReject) == false)
        {
            //device service did not like something about this device and it was not successfully
            // added.  We cannot keep anything about it around, so clean up, reset it, and tell it
            // to leave
            //
            // Note: This log line is used for telemetry, please DO NOT modify or remove it
            //
            icLogWarn(LOG_TAG,
                      "%s: device service rejected the device of type %s and id %s",
                      __FUNCTION__,
                      commonDriver->deviceClass,
                      uuid);

            pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
            hashMapDelete(commonDriver->discoveredDeviceDetails,
                          &details->eui64,
                          sizeof(uint64_t),
                          discoveredDeviceDetailsFreeFunc);
            pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

            // Don't reset if we failed migration
            if (deviceMigrator == NULL)
            {
                // reset and kick it out in the background so we dont block.  We just send to the
                // first endpoint since its a global operation on the device
                triggerBackgroundResetToFactory(details->endpointDetails[0].endpointId, details->eui64);
            }

            result = false;
        }

        // Cleanup endpointProfileMap
        stringHashMapDestroy(endpointProfileMap, NULL);

        // Cleanup metadata
        stringHashMapDestroy(metadata, NULL);
        free(uuid);
        free(fw);
    }

    return result;
}

static void synchronizeDevice(void *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->synchronizeDevice != NULL)
    {
        uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
        IcDiscoveredDeviceDetails *details = getDiscoveredDeviceDetails(eui64, commonDriver);
        commonDriver->commonCallbacks->synchronizeDevice(ctx, device, details);
    }
}

static void endpointDisabled(void *ctx, icDeviceEndpoint *endpoint)
{
    icLogDebug(LOG_TAG, "%s: uuid=%s, endpointId=%s", __FUNCTION__, endpoint->deviceUuid, endpoint->id);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->endpointDisabled != NULL)
    {
        commonDriver->commonCallbacks->endpointDisabled(ctx, endpoint);
    }
}

static void handleAlarmCommand(uint64_t eui64,
                               uint8_t endpointId,
                               const ZigbeeAlarmTableEntry *entries,
                               uint8_t numEntries,
                               const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    for (uint8_t i = 0; i < numEntries; i++)
    {
        //forward to the owning cluster
        ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, (void *) &entries[i].clusterId,
                                            sizeof(entries[i].clusterId));
        if (cluster != NULL)
        {
            if (cluster->handleAlarm != NULL)
            {
                cluster->handleAlarm(cluster, eui64, endpointId, &entries[i]);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: no cluster registered to handle the command: cluster 0x%.2"PRIx16" ep %"PRIu8" alarmCode 0x%.2"PRIx8,
                    __FUNCTION__, entries[i].clusterId, endpointId, entries[i].alarmCode);
        }
    }

    if (commonDriver->commonCallbacks->handleAlarms != NULL)
    {
        commonDriver->commonCallbacks->handleAlarms(commonDriver, eui64, endpointId, entries, numEntries);
    }
}

static void handleAlarmClearedCommand(uint64_t eui64,
                                      uint8_t endpointId,
                                      const ZigbeeAlarmTableEntry *entries,
                                      uint8_t numEntries,
                                      const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    for (uint8_t i = 0; i < numEntries; i++)
    {
        //forward to the owning cluster
        ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, (void *) &entries[i].clusterId,
                                            sizeof(entries[i].clusterId));
        if (cluster != NULL)
        {
            if (cluster->handleAlarmCleared != NULL)
            {
                cluster->handleAlarmCleared(cluster, eui64, endpointId, &entries[i]);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: no cluster registered to handle the command", __FUNCTION__);
        }
    }

    if (commonDriver->commonCallbacks->handleAlarmsCleared != NULL)
    {
        commonDriver->commonCallbacks->handleAlarmsCleared(commonDriver, eui64, endpointId, entries, numEntries);
    }
}

static IcDiscoveredDeviceDetails *getDiscoveredDeviceDetails(uint64_t eui64, ZigbeeDriverCommon *commonDriver)
{
    IcDiscoveredDeviceDetails *result = NULL;

    char *uuid = zigbeeSubsystemEui64ToId(eui64);

    //try to get the details from our cached map.  If it isnt in there, load from metadata JSON and cache for next time
    pthread_mutex_lock(&commonDriver->discoveredDeviceDetailsMtx);
    result = hashMapGet(commonDriver->discoveredDeviceDetails, &eui64, sizeof(uint64_t));

    if (result == NULL)
    {
        char *detailsStr = commonDriver->deviceServiceCallbacks->getMetadata(uuid, NULL, DISCOVERED_DEVICE_DETAILS);

        if (detailsStr == NULL)
        {
            icLogError(LOG_TAG, "%s: missing %s metadata!", __FUNCTION__, DISCOVERED_DEVICE_DETAILS);
            pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);
            free(uuid);
            return NULL;
        }

        cJSON *detailsJson = cJSON_Parse(detailsStr);
        result = icDiscoveredDeviceDetailsFromJson(detailsJson);
        cJSON_Delete(detailsJson);
        free(detailsStr);

        if (result == NULL)
        {
            icLogError(LOG_TAG, "%s: failed to parse %s metadata!", __FUNCTION__, DISCOVERED_DEVICE_DETAILS);
            pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);
            free(uuid);
            return NULL;
        }

        //cache it
        if(hashMapPut(commonDriver->discoveredDeviceDetails, &result->eui64, sizeof(uint64_t), result) == false)
        {
            icLogWarn(LOG_TAG, "%s: Failed to cache discovered device details for %s", __func__, uuid);
            freeIcDiscoveredDeviceDetails(result);
            result = NULL;
        }
    }
    pthread_mutex_unlock(&commonDriver->discoveredDeviceDetailsMtx);

    free(uuid);

    return result;
}

static bool resourceNeedsRefreshing(const char *deviceUuid,
                                    const char *resourceId,
                                    const char *metadataPropName,
                                    uint32_t defaultRefreshIntervalSecs)
{
    bool result = true; //default is to go ahead and refresh

    uint64_t resourceAgeMillis;
    if (deviceServiceGetResourceAgeMillis(deviceUuid,
                                          NULL,
                                          resourceId,
                                          &resourceAgeMillis) == true)
    {
        // if there is a metadata entry for this interval, use that.  Otherwise fall back to default.
        uint32_t refreshIntervalSecs = defaultRefreshIntervalSecs;

        AUTO_CLEAN(free_generic__auto) char *uri = getMetadataUri(deviceUuid, NULL, metadataPropName);

        char *value = NULL;
        if (deviceServiceGetMetadata(uri, &value) == true && value != NULL)
        {
            uint32_t temp = 0;
            if (stringToUint32(value, &temp) == true)
            {
                refreshIntervalSecs = temp;
            }
        }
        free(value);

        // if the resource is not old enough, lets not do anything now
        if (resourceAgeMillis < ((uint64_t)refreshIntervalSecs * 1000))
        {
            icLogDebug(LOG_TAG, "%s: resource %s does not need refreshing yet", __func__, resourceId);
            result = false;
        }
    }

    return result;
}

/*
 * Caller must NOT destroy the contents of the returned list
 */
static icLinkedList *getClustersNeedingPollControlRefresh(uint64_t eui64,
                                                          uint8_t endpointId,
                                                          const ZigbeeDriverCommon *commonDriver)
{
    icLinkedList *result = linkedListCreate();

    /*
     * Currently only Temperature Measurement, Diagnostics, Power Configuration, and OTA clusters do anything
     * during poll control checkin.  Given that these are all common clusters controlled by this common driver,
     * we handle the checkin here by only including these clusters in the checkin processing if our logic
     * determines there is something to be done.  This allows us to let the device go right back to sleep
     * when it does a checkin if nothing needs to be done.
     *
     * In the future we may want lower level drivers and/or other clusters to be able to do their processing
     * during checkin.  If/when that time comes, we will likely need to extend this and check some new callback
     * on the lower level device driver or cluster.
     */

    AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    if (resourceNeedsRefreshing(deviceUuid,
                                COMMON_DEVICE_RESOURCE_TEMPERATURE,
                                TEMP_REFRESH_MIN_SECS_PROP,
                                DEFAULT_TEMP_REFRESH_MIN_SECONDS) == true)
    {
        // add the temperature measurement cluster to our result set
        uint16_t clusterId = TEMPERATURE_MEASUREMENT_CLUSTER_ID;
        ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &clusterId, sizeof(clusterId));
        if (cluster != NULL)
        {
            icLogDebug(LOG_TAG, "%s: going to refresh temperature measurement", __func__);
            linkedListAppend(result, cluster);
        }
    }

    if (resourceNeedsRefreshing(deviceUuid,
                                COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE,
                                BATTERY_VOLTAGE_REFRESH_MIN_SECS_PROP,
                                DEFAULT_BATTERY_VOLTAGE_REFRESH_MIN_SECONDS) == true)
    {
        // add the power configuration cluster to our result set
        uint16_t clusterId = POWER_CONFIGURATION_CLUSTER_ID;
        ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &clusterId, sizeof(clusterId));
        if (cluster != NULL)
        {
            icLogDebug(LOG_TAG, "%s: going to refresh power configuration", __func__);
            linkedListAppend(result, cluster);
        }
    }

    // if either far end rssi or lqi need updating, add the diagnostics cluster
    if (resourceNeedsRefreshing(deviceUuid,
                                COMMON_DEVICE_RESOURCE_FERSSI,
                                RSSI_REFRESH_MIN_SECS_PROP,
                                DEFAULT_RSSI_REFRESH_MIN_SECONDS) == true ||
       resourceNeedsRefreshing(deviceUuid,
                                COMMON_DEVICE_RESOURCE_FELQI,
                                LQI_REFRESH_MIN_SECS_PROP,
                                DEFAULT_LQI_REFRESH_MIN_SECONDS) == true)
    {
        uint16_t clusterId = DIAGNOSTICS_CLUSTER_ID;
        ZigbeeCluster *cluster = hashMapGet(commonDriver->clusters, &clusterId, sizeof(clusterId));
        if (cluster != NULL)
        {
            icLogDebug(LOG_TAG, "%s: going to refresh diagnostics", __func__);
            linkedListAppend(result, cluster);
        }
    }

    return result;
}

static void handlePollControlCheckin(uint64_t eui64,
                                     uint8_t endpointId,
                                     const ComcastBatterySavingData *batterySavingData,
                                     const void *ctx)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    //TODO: allow each cluster to perform some action on poll control check-in...even on a Comcast Custom check-in.
    // For now, if it's a Comcast Custom Message, skip fast poll and don't allow clusters to handle the checkin.
    if(batterySavingData != NULL)
    {
        comcastBatterySavingHelperUpdateResources(eui64, batterySavingData, commonDriver);

        if (!pollControlClusterSendCustomCheckInResponse(eui64, endpointId))
        {
            icLogError(LOG_TAG, "%s: failed to send custom poll control checkin response!", __FUNCTION__);
        }
    }
    else
    {
        //first check to see if any cluster had any work to do during this checkin.  If not, no need to start fast
        // polling
        icLinkedList *clustersNeedingPollControlRefresh = getClustersNeedingPollControlRefresh(eui64,
                                                                                               endpointId,
                                                                                               commonDriver);

        if (clustersNeedingPollControlRefresh != NULL && linkedListCount(clustersNeedingPollControlRefresh) > 0)
        {
            //request fast poll while we do the refresh
            if (pollControlClusterSendCheckInResponse(eui64, endpointId, true))
            {
                //Allow each cluster that needs refresh to run
                sbIcLinkedListIterator *it = linkedListIteratorCreate(clustersNeedingPollControlRefresh);
                while (linkedListIteratorHasNext(it) == true)
                {
                    ZigbeeCluster *cluster = linkedListIteratorGetNext(it);

                    if (cluster->handlePollControlCheckin != NULL)
                    {
                        icLogDebug(LOG_TAG, "%s: notifying cluster 0x%04x that it can do poll control checkin work",
                                   __FUNCTION__, cluster->clusterId);

                        cluster->handlePollControlCheckin(cluster, eui64, endpointId);
                    }
                }
            }
            else
            {
                icLogError(LOG_TAG, "%s: failed to enter fast poll!", __FUNCTION__);
            }

            //Stop the fast polling
            pollControlClusterStopFastPoll(eui64, endpointId);
        }
        else
        {
            // no work to do. send checkin response indicating no fast polling
            icLogDebug(LOG_TAG, "%s: no work to do, not fast polling", __func__);
            pollControlClusterSendCheckInResponse(eui64, endpointId, false);
        }

        // dont destroy the clusters in the list
        linkedListDestroy(clustersNeedingPollControlRefresh, standardDoNotFreeFunc);
    }
}

char *getZigbeeVersionString(uint32_t version)
{
    char *result = (char *) malloc(11); //max 32 bit hex string plus 0x and null
    snprintf(result, 11, FIRMWARE_FORMAT_STRING, version);
    return result;
}

uint32_t getZigbeeVersionFromString(const char *version)
{
    uint32_t result = 0;
    stringToUint32(version, &result);
    return result;
}

void *zigbeeDriverCommonGetDriverPrivateData(ZigbeeDriverCommon *ctx)
{
    return ctx->driverPrivate;
}

void zigbeeDriverCommonSetDriverPrivateData(ZigbeeDriverCommon *ctx, void *privateData)
{
    ctx->driverPrivate = privateData;
}

void zigbeeDriverCommonSetBatteryBackedUp(DeviceDriver *driver)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) driver;
    commonDriver->batteryBackedUp = true;
}

static void discoveredDeviceDetailsFreeFunc(void *key, void *value)
{
    (void) key;
    freeIcDiscoveredDeviceDetails((IcDiscoveredDeviceDetails *) value);
}

static void *resetToFactoryTask(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    resetToFactoryData *resetData = (resetToFactoryData *) arg;

    zigbeeSubsystemSendCommand(resetData->eui64, resetData->epid, BASIC_CLUSTER_ID, true,
                               BASIC_RESET_TO_FACTORY_COMMAND_ID, NULL, 0);
    zhalRequestLeave(resetData->eui64, false, false);

    // Cleanup
    free(resetData);

    return NULL;
}

static bool findDeviceResource(void *searchVal, void *item)
{
    icDeviceResource *resourceItem = (icDeviceResource *) item;
    return strcmp(searchVal, resourceItem->id) == 0;
}

static void updateNeRssiAndLqi(void *ctx, uint64_t eui64, int8_t rssi, uint8_t lqi)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    char rssiStr[5]; //-127 + \0
    snprintf(rssiStr, 5, "%d", rssi);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_NERSSI,
                                                         rssiStr,
                                                         NULL);

    char lqiStr[4]; //255 + \0
    snprintf(lqiStr, 4, "%u", lqi);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_NELQI,
                                                         lqiStr,
                                                         NULL);

    free(deviceUuid);
}

static void handleRssiLqiUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, int8_t rssi, uint8_t lqi)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    char rssiStr[5]; //-127 + \0
    snprintf(rssiStr, 5, "%d", rssi);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_FERSSI,
                                                         rssiStr,
                                                         NULL);

    char lqiStr[4]; //255 + \0
    snprintf(lqiStr, 4, "%u", lqi);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_FELQI,
                                                         lqiStr,
                                                         NULL);

    free(deviceUuid);
}

static void handleTemperatureMeasurementMeasuredValueUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, int16_t value)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    char tmp[7]; //-32767 + \0 worst case
    snprintf(tmp, 7, "%"PRId16, value);

    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_TEMPERATURE,
                                                         tmp,
                                                         NULL);
    free(deviceUuid);
}

static void handleBatteryVoltageUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t decivolts)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    char tmp[6]; //25500 + \0 worst case
    snprintf(tmp, 6, "%u", (unsigned int) decivolts * 100);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE,
                                                         tmp,
                                                         NULL);
    free(deviceUuid);
}

static void handleBatteryPercentageRemainingUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, uint8_t percent)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    char tmp[4]; //100 + \0 worst case
    snprintf(tmp, 4, "%u", (unsigned int) percent);
    commonDriver->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_BATTERY_PERCENTAGE_REMAINING,
                                                         tmp,
                                                         NULL);
    free(deviceUuid);
}

void zigbeeDriverCommonUpdateBatteryChargeStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryLow)
{
    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    ((ZigbeeDriverCommon*)driver)->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_BATTERY_LOW,
                                                         isBatteryLow ? "true" : "false",
                                                         NULL);
    free(deviceUuid);
}

static void handleBatteryChargeStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryLow)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;

    zigbeeDriverCommonUpdateBatteryChargeStatus(commonDriver, eui64, isBatteryLow);
}

void zigbeeDriverCommonUpdateBatteryBadStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryBad)
{
    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    ((ZigbeeDriverCommon*)driver)->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_BATTERY_BAD,
                                                         isBatteryBad ? "true" : "false",
                                                         NULL);
    free(deviceUuid);
}

static void handleBatteryBadStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryBad)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;

    zigbeeDriverCommonUpdateBatteryBadStatus(commonDriver, eui64, isBatteryBad);
}

void zigbeeDriverCommonUpdateBatteryMissingStatus(DeviceDriver *driver, uint64_t eui64, bool isBatteryMissing)
{
    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    ((ZigbeeDriverCommon*)driver)->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_BATTERY_MISSING,
                                                         isBatteryMissing ? "true" : "false",
                                                         NULL);
    free(deviceUuid);
}

static void handleBatteryMissingStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isBatteryMissing)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;

    zigbeeDriverCommonUpdateBatteryMissingStatus(commonDriver, eui64, isBatteryMissing);
}

void zigbeeDriverCommonUpdateBatteryTemperatureStatus(DeviceDriver *driver, uint64_t eui64, bool isHigh)
{
    AUTO_CLEAN(free_generic__auto) char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    ((ZigbeeDriverCommon*)driver)->deviceServiceCallbacks->updateResource(deviceUuid,
                                                                          0,
                                                                          COMMON_DEVICE_RESOURCE_BATTERY_HIGH_TEMPERATURE,
                                                                          stringValueOfBool(isHigh),
                                                                          NULL);
}

static void handleBatteryTemperatureStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isHigh)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;
    zigbeeDriverCommonUpdateBatteryTemperatureStatus(commonDriver, eui64, isHigh);
}

void zigbeeDriverCommonUpdateAcMainsStatus(DeviceDriver *driver, uint64_t eui64, bool isAcMainsConnected)
{
    char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);
    ((ZigbeeDriverCommon*)driver)->deviceServiceCallbacks->updateResource(deviceUuid,
                                                         0,
                                                         COMMON_DEVICE_RESOURCE_AC_MAINS_DISCONNECTED,
                                                         isAcMainsConnected ? "false" : "true",
                                                         NULL);
    free(deviceUuid);
}

static void handleAcMainsStatusUpdated(void *ctx, uint64_t eui64, uint8_t endpointId, bool isAcMainsConnected)
{
    DeviceDriver *commonDriver = (DeviceDriver *) ctx;

    zigbeeDriverCommonUpdateAcMainsStatus(commonDriver, eui64, isAcMainsConnected);
}

/**
 * Reprocess any commands received before the device was persisted, e.g. pass them to their appropriate
 * cluster/driver to be handled.   Typically this would be called from a devicePersisted callback.
 * @param driver the calling driver
 * @param eui64 the eui64 of the device
 * @param filter a function that will return true for the commands to be processed, or NULL can be passed to process
 * them all.
 */
void zigbeeDriverCommonProcessPrematureClusterCommands(ZigbeeDriverCommon *driver,
                                                       uint64_t eui64,
                                                       receivedClusterCommandFilter filter)
{
    icLinkedList *commands = zigbeeSubsystemGetPrematureClusterCommands(eui64);
    icLinkedListIterator *iter = linkedListIteratorCreate((icLinkedList *)commands);
    while(linkedListIteratorHasNext(iter))
    {
        ReceivedClusterCommand *item = (ReceivedClusterCommand *)linkedListIteratorGetNext(iter);
        if (filter == NULL || filter(item))
        {
            clusterCommandReceived(driver, item);
        }
    }
    linkedListIteratorDestroy(iter);

    linkedListDestroy(commands, (linkedListItemFreeFunc) freeReceivedClusterCommand);
}

void zigbeeDriverCommonRegisterNewDevice(ZigbeeDriverCommon *driver, icDevice *device)
{
    // Pass through to our common registration function
    registerNewDevice(device, driver->deviceCallbacks, driver->commFailTimeoutSeconds);
}

void zigbeeDriverCommonSetBlockingUpgrade(ZigbeeDriverCommon *driver, uint64_t eui64, bool inProgress)
{
    icLogDebug(LOG_TAG, "%s: %016"PRIx64" upgrade %s", __FUNCTION__, eui64, inProgress ? "in progress" : "complete");

    pthread_mutex_lock(&blockingUpgradesMtx);
    if(inProgress)
    {
        if(blockingUpgrades == NULL)
        {
            blockingUpgrades = hashMapCreate();
        }

        uint64_t *key = malloc(sizeof(uint64_t));
        *key = eui64;
        hashMapPut(blockingUpgrades, key, sizeof(uint64_t), NULL);
    }
    else
    {
        if(blockingUpgrades != NULL)
        {
            if(hashMapDelete(blockingUpgrades, &eui64, sizeof(uint64_t), NULL) == false)
            {
                icLogError(LOG_TAG, "%s: device not found in blockingUpgrades set", __FUNCTION__);
            }
            else
            {
                //notify anyone that might be waiting that our map of blocking upgrades has shrunk
                pthread_cond_broadcast(&blockingUpgradesCond);
            }
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: no blockingUpgrades set", __FUNCTION__);
        }
    }
    pthread_mutex_unlock(&blockingUpgradesMtx);
}

bool zigbeeDriverCommonIsDiscoveryActive(ZigbeeDriverCommon *driver)
{
    return driver->discoveryActive;
}

static void triggerBackgroundResetToFactory(uint8_t epid, uint64_t eui64)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    resetToFactoryData *resetData = (resetToFactoryData *) calloc(1, sizeof(resetToFactoryData));
    resetData->epid = epid;
    resetData->eui64 = eui64;

    createDetachedThread(resetToFactoryTask, resetData, "zbDrvDefaultDev");
}

static bool firmwareFileExists(const char *firmwareDirectory, const char *firmwareFileName)
{
    char *path = (char *) malloc(sizeof(char) * (strlen(firmwareDirectory) + strlen(firmwareFileName) + 2));
    sprintf(path, "%s/%s", firmwareDirectory, firmwareFileName);

    struct stat buffer;
    bool retval = stat(path, &buffer) == 0;
    free(path);
    return retval;
}

/*
 * curl callback to store URL contents into a file
 */
static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    size_t written;
    written = fwrite(ptr, size, nmemb, stream);
    return written;
}

/*
 * Do the firmware file download
 */
static bool
downloadFirmwareFile(const char *firmwareBaseUrl, const char *firmwareDirectory, const char *firmwareFileName)
{
    bool result = false;
    char *url = (char *) malloc(sizeof(char) * (strlen(firmwareBaseUrl) + strlen(firmwareFileName) + 2));
    sprintf(url, "%s/%s", firmwareBaseUrl, firmwareFileName);

    icLogDebug(LOG_TAG, "downloadFirmwareFile: attempting to download firmware from %s", url);

    char *outfilename = stringBuilder("%s/%s", firmwareDirectory, firmwareFileName);

    //download the new list
    CURL *curl;

    CURLcode res;
    curl = curl_easy_init();
    if (curl)
    {
        // Write to a temp file in case the transfer dies in the middle so we don't leave a partial firmware
        // file sitting around
        FILE *fp = tmpfile();
        if (fp != NULL)
        {
            // set standard curl options
            sslVerify verifyFlag = getSslVerifyProperty(SSL_VERIFY_HTTP_FOR_SERVER);
            applyStandardCurlOptions(curl, url, 60, verifyFlag, false);
            if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_URL, url) failed at %s(%d)",__FILE__,__LINE__);
            }
            if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data) failed at %s(%d)",__FILE__,__LINE__);
            }
            if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp) failed at %s(%d)",__FILE__,__LINE__);
            }
            res = curl_easy_perform(curl);
            if (res != CURLE_OK)
            {
                icLogError(LOG_TAG, "curl_easy_perform() failed in %s : %s\n", __func__, curl_easy_strerror(res));
                fclose(fp);
            }
            else
            {
                icLogDebug(LOG_TAG,
                           "downloadFirmwareFile: firmware file download finished, moving into place at %s",
                           outfilename);
                // Copy from the temp file to out output file
                rewind(fp);
                FILE *outFp = fopen(outfilename, "wb");
                // This will *always* close both files, which will delete 'fp' per tmpfile() API
                if (!copyFile(fp, outFp))
                {
                    icLogError(LOG_TAG, "Failed to copy firmware temp file to firmware directory: %d", errno);
                    // Cleanup the file in case it got partially there
                    if(remove(outfilename))
                    {
                        icLogError(LOG_TAG, "Failed remove %s", outfilename);
                    }
                }
                else
                {
                    /* fp already closed by copyFile */
                    fp = NULL;
                    icLogInfo(LOG_TAG, "downloadFirmwareFile: firmware file %s successfully downloaded!",
                              outfilename);
                    // Set permissions
                    if(chmod(outfilename, 0777))
                    {
                        icLogError(LOG_TAG, "Failed set permissions on %s", outfilename);
                    }
                    // Download was successful
                    result = true;
                }
            }
        }
        else
        {
            icLogError(LOG_TAG, "failed to open output file for writing: %s", outfilename);
        }
        // Cleanup curl
        curl_easy_cleanup(curl);
    }

    free(outfilename);
    free(url);

    return result;
}

/*
 * Download the firmware files related to this device descriptor.
 *
 * Return true on success, false on failure.
 */
bool zigbeeDriverCommonDownloadFirmwareFiles(const DeviceDescriptor *dd)
{
    bool result = false;
    int filesRequired = 0;
    int filesAvailable = 0;
    AUTO_CLEAN(free_generic__auto) const char *firmwareDirectory =
            zigbeeSubsystemGetAndCreateFirmwareFileDirectory(dd->latestFirmware->type);

    if (firmwareDirectory != NULL)
    {
        // This property gets straight mapped to a CPE property
        AUTO_CLEAN(free_generic__auto) const char *firmwareBaseUrl =
                getPropertyAsString(DEVICE_FIRMWARE_URL_NODE, NULL);

        if (firmwareBaseUrl == NULL)
        {
            icLogError(LOG_TAG, "Device Firmware Base URL was empty, cannot download firmware");
        }
        else
        {
            icLinkedListIterator *iterator = linkedListIteratorCreate(dd->latestFirmware->filenames);
            while (linkedListIteratorHasNext(iterator))
            {
                filesRequired++;

                char *filename = (char *) linkedListIteratorGetNext(iterator);

                if (!firmwareFileExists(firmwareDirectory, filename))
                {
                    icLogDebug(LOG_TAG, "%s: did not find %s in %s: downloading", __FUNCTION__, filename, firmwareDirectory);
                    if (downloadFirmwareFile(firmwareBaseUrl, firmwareDirectory, filename))
                    {
                        filesAvailable++;
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Firmware file %s failed to download", filename);
                    }
                }
                else
                {
                    icLogDebug(LOG_TAG, "Firmware file %s already exists in directory %s", filename, firmwareDirectory);
                    filesAvailable++;
                }
            }
            linkedListIteratorDestroy(iterator);

            if(filesRequired == filesAvailable)
            {
                result = true;
            }
        }

        // we can do this part regardless of whether or not we got all the files
        if (filesAvailable > 0)
        {
            // Inform zigbee that there are new OTA files for devices
            zhalRefreshOtaFiles();
        }
    }
    else
    {
        icLogError(LOG_TAG, "Could not get/create firmware directory for dd uuid: %s", dd->uuid);
    }

    return result;
}

/*
 * For a device, download all firmware files and apply the upgrade.
 */
static void doFirmwareUpgrade(void *arg)
{
    FirmwareUpgradeContext *ctx = (FirmwareUpgradeContext *) arg;

    //The delayed task handle is no longer valid.  Remove it from our map.
    pthread_mutex_lock(&ctx->commonDriver->pendingFirmwareUpgradesMtx);
    bool found = hashMapDelete(ctx->commonDriver->pendingFirmwareUpgrades, ctx->taskHandle, sizeof(uint32_t), standardDoNotFreeHashMapFunc);
    pthread_mutex_unlock(&ctx->commonDriver->pendingFirmwareUpgradesMtx);

    if(!found)
    {
        icLogInfo(LOG_TAG, "%s: exiting since this upgrade was not found in pendingFirmwareUpgrades", __FUNCTION__);
        destroyFirmwareUpgradeContext(ctx);
        return;
    }

    DeviceDescriptor *dd = ctx->dd;

    bool willRetry = false;
    if(zigbeeDriverCommonDownloadFirmwareFiles(dd) == false)
    {
        icLogError(LOG_TAG, "%s: failed to download firmware files", __FUNCTION__);

        uint32_t retrySeconds = getPropertyAsUInt32(FIRMWARE_UPGRADE_RETRYDELAYSECS,
                                                     FIRMWARE_UPGRADE_RETRYDELAYSECS_DEFAULT);

        icLogInfo(LOG_TAG, "%s: rescheduling for %" PRIu32 " seconds", __FUNCTION__, retrySeconds);

        pthread_mutex_lock(&ctx->commonDriver->pendingFirmwareUpgradesMtx);

        // Reschedule for a retry
        *ctx->taskHandle = scheduleDelayTask(retrySeconds,
                                             DELAY_SECS,
                                             doFirmwareUpgrade,
                                             ctx);
        if(*ctx->taskHandle > 0)
        {
            hashMapPut(ctx->commonDriver->pendingFirmwareUpgrades,
                       ctx->taskHandle,
                       sizeof(uint32_t),
                       ctx);

            // Make sure we don't cleanup the context since we still need it
            willRetry = true;
        }

        pthread_mutex_unlock(&ctx->commonDriver->pendingFirmwareUpgradesMtx);
    }

    if (!willRetry)
    {
        if (ctx->commonDriver->commonCallbacks->initiateFirmwareUpgrade != NULL)
        {
            ctx->commonDriver->commonCallbacks->initiateFirmwareUpgrade(ctx->commonDriver, ctx->deviceUuid, dd);
        }
        else
        {
            //we completed the download and we dont have a custom initiate firmware upgrade callback.
            // Attempt a standard OTA Upgrade cluster image notify command.  That will
            // harmlessly fail on very sleepy devices and/or legacy iControl security devices.
            // If that doesnt work, the notify will be sent if the device supports the poll control checkin
            // cluster and checks in with a pending firmware upgrade set.
            uint64_t eui64 = zigbeeSubsystemIdToEui64(ctx->deviceUuid);

            //if for whatever reason we didnt get an endpoint number, fall back to the most common value of 1
            uint8_t epid = ctx->endpointId == NULL ? 1 : getEndpointNumber(ctx->commonDriver,
                                                                           ctx->deviceUuid,
                                                                           ctx->endpointId);
            otaUpgradeClusterImageNotify(eui64, epid);
        }

        destroyFirmwareUpgradeContext(ctx);
    }
}

// set the provided metadata on this device
static void
processDeviceDescriptorMetadata(ZigbeeDriverCommon *commonDriver, icDevice *device, icStringHashMap *metadata)
{
    //If this device is not yet in our database, then it is a newly pairing device which already processed the metadata
    if (device->uri == NULL)
    {
        icLogDebug(LOG_TAG, "%s: skipping metadata processing for newly paired device", __FUNCTION__);
        return;
    }

    icStringHashMapIterator *it = stringHashMapIteratorCreate(metadata);
    while (stringHashMapIteratorHasNext(it))
    {
        char *key;
        char *value;
        if (stringHashMapIteratorGetNext(it, &key, &value))
        {
            icLogInfo(LOG_TAG, "%s: setting metadata (%s=%s) on device %s", __FUNCTION__, key, value, device->uuid);

            commonDriver->deviceServiceCallbacks->setMetadata(device->uuid, NULL, key, value);
        }
    }
    stringHashMapIteratorDestroy(it);

    //now let the higher level device driver at it, if it cares
    if (commonDriver->commonCallbacks->processDeviceDescriptorMetadata != NULL)
    {
        commonDriver->commonCallbacks->processDeviceDescriptorMetadata(commonDriver, device, metadata);
    }
}

void zigbeeDriverCommonCancelPendingUpgrades(ZigbeeDriverCommon *commonDriver)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&commonDriver->pendingFirmwareUpgradesMtx);
    icHashMapIterator *pendingIt = hashMapIteratorCreate(commonDriver->pendingFirmwareUpgrades);
    while(hashMapIteratorHasNext(pendingIt))
    {
        void *taskHandle;
        uint16_t keyLen;
        FirmwareUpgradeContext *fwupgctx;
        hashMapIteratorGetNext(pendingIt, &taskHandle, &keyLen, (void **) &fwupgctx);
        cancelDelayTask(*((uint32_t*)taskHandle));
        hashMapIteratorDeleteCurrent(pendingIt, pendingFirmwareUpgradeFreeFunc);
    }
    hashMapIteratorDestroy(pendingIt);
    pthread_mutex_unlock(&commonDriver->pendingFirmwareUpgradesMtx);
}

/**
 * Check if this driver is for battery backed up devices
 * @param commonDriver
 * @return true if is battery backed up
 */
bool zigbeeDriverCommonIsBatteryBackedUp(const ZigbeeDriverCommon *commonDriver)
{
    return commonDriver->batteryBackedUp;
}

/*
 * This can wait forever in a stuck scenario. Device service on shutdown will allow up to some max time
 * (historically 31 minutes) before existing the process.
 */
static void waitForUpgradesToComplete()
{
    while(true)
    {
        pthread_mutex_lock(&blockingUpgradesMtx);
        uint16_t count = 0;
        if(blockingUpgrades != NULL)
        {
            count = hashMapCount(blockingUpgrades);
        }

        if(count == 0)
        {
            pthread_mutex_unlock(&blockingUpgradesMtx);
            break;
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: %"PRIu16" upgrades are blocking", __FUNCTION__, count);
            pthread_cond_wait(&blockingUpgradesCond, &blockingUpgradesMtx);
        }

        pthread_mutex_unlock(&blockingUpgradesMtx);
    }
}

static void destroyMapCluster(void *key, void *value)
{
    ZigbeeCluster *cluster = (ZigbeeCluster *) value;
    if (cluster && cluster->destroy)
    {
        cluster->destroy(cluster);
    }
    free(cluster);
    free(key);
}

static void pendingFirmwareUpgradeFreeFunc(void *key, void *value)
{
    (void)key;
    destroyFirmwareUpgradeContext(value);
}

static void systemPowerEvent(void *ctx, DeviceServiceSystemPowerEventType powerEvent)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->systemPowerEvent != NULL)
    {
        commonDriver->commonCallbacks->systemPowerEvent(commonDriver, powerEvent);
    }
}

static void propertyChanged(void *ctx, cpePropertyEvent *event)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->handlePropertyChanged != NULL)
    {
        commonDriver->commonCallbacks->handlePropertyChanged(commonDriver, event);
    }
}

static void fetchRuntimeStats(void *ctx,  icStringHashMap *output)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->fetchRuntimeStats != NULL)
    {
        commonDriver->commonCallbacks->fetchRuntimeStats(commonDriver, output);
    }
}

static bool getDeviceClassVersion(void *ctx, const char *deviceClass, uint8_t *version)
{
    bool result = false;

    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;
    if (version != NULL && stringCompare(commonDriver->deviceClass, deviceClass, false) == 0)
    {
        *version = commonDriver->deviceClassVersion;
        result = true;
    }

    return result;
}

static void handleSubsystemInitialized(void *ctx)
{
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->subsystemInitialized != NULL)
    {
        commonDriver->commonCallbacks->subsystemInitialized(commonDriver);
    }
}

static void handleBatteryRechargeCyclesChanged(void *ctx, uint64_t eui64, uint16_t rechargeCycles)
{
    icLogTrace(LOG_TAG, "%s ", __FUNCTION__);
    ZigbeeDriverCommon *commonDriver = (ZigbeeDriverCommon *) ctx;

    if (commonDriver->commonCallbacks->updateBatteryRechargeCycles != NULL)
    {
        commonDriver->commonCallbacks->updateBatteryRechargeCycles(commonDriver, eui64, rechargeCycles);
    }
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE
