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
// Created by tlea on 2/20/19.
//

#include <icBuildtime.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <stdio.h>
#include <resourceTypes.h>
#include <commonDeviceDefs.h>
#include <deviceModelHelper.h>
#include <memory.h>
#include <pthread.h>
#include <jsonHelper/jsonHelper.h>
#include <zigbeeClusters/legacySecurityCluster.h>
#include <pthread.h>
#include <unistd.h>
#include <deviceServicePrivate.h>
#include <sys/stat.h>
#include <zigbeeLegacySecurityCommon/uc_common.h>
#include <icUtil/fileUtils.h>
#include <string.h>
#include "zigbeeDriverCommon.h"
#include "zigbeeLegacySensorDeviceDriver.h"

#define LOG_TAG "zigbeeLegacySensorDD"
#define DRIVER_NAME "zigbeeLegacySensorDD"
#define MY_DC_VERSION 1
#define LEGACY_DEVICE_ENDPOINT_NUM 1
#define LEGACY_DEVICE_ENDPOINT_ID "1"

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

/* Intentionally empty to force claimDevice to claim devices based on deviceType */
static uint16_t myDeviceIds[] = {};

/* HashMap to keep track of uuid for which test fault has been triggered */
static icStringHashMap* testModeTriggeredSensorMap = NULL;
static pthread_mutex_t testModeTriggeredSensorMapMtx = PTHREAD_MUTEX_INITIALIZER;

static cJSON* getTestFaultedMetadata(bool test);
static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues);

static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices);

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails);

static const char *zigbeeSensorDeviceDriverGetTroubleResource(const char *sensorType);

static const char *getSensorTypeValue(uCLegacyDeviceClassification classification);

static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details);

static void postDeviceRemoved(ZigbeeDriverCommon *ctx, icDevice *device);

static void deviceStatusChanged(uint64_t eui64,
                                uint8_t endpointId,
                                const uCStatusMessage *status,
                                const void *ctx);

static void upgradeInProgress(uint64_t eui64,
                              bool inProgress,
                              const void *ctx);

static void initiateFirmwareUpgrade(ZigbeeDriverCommon *ctx, const char *deviceUuid, const DeviceDescriptor *dd);

static bool getDiscoveredDeviceMetadata(ZigbeeDriverCommon *ctx,
                                        IcDiscoveredDeviceDetails *details,
                                        icStringHashMap *metadata);

static void firmwareUpgradeFailed(ZigbeeDriverCommon *ctx, uint64_t eui64);

static const ZigbeeDriverCommonCallbacks commonCallbacks =
        {
                .devicesLoaded = devicesLoaded,
                .getDiscoveredDeviceMetadata = getDiscoveredDeviceMetadata,
                .configureDevice = configureDevice,
                .postDeviceRemoved = postDeviceRemoved,
                .claimDevice = claimDevice,
                .fetchInitialResourceValues = fetchInitialResourceValues,
                .registerResources = registerResources,
                .mapDeviceIdToProfile = mapDeviceIdToProfile,
                .initiateFirmwareUpgrade = initiateFirmwareUpgrade,
                .firmwareUpgradeFailed = firmwareUpgradeFailed
        };

static DeviceDriver *myDriver = NULL;

static DeviceServiceCallbacks *deviceServiceCallbacks = NULL;

static const LegacySecurityClusterCallbacks legacySecurityClusterCallbacks =
        {
                .deviceStatusChanged = deviceStatusChanged,
                .upgradeInProgress = upgradeInProgress,
        };

static ZigbeeCluster *legacySecurityCluster = NULL;

DeviceDriver *zigbeeLegacySensorDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    myDriver = zigbeeDriverCommonCreateDeviceDriver(DRIVER_NAME,
                                                    SENSOR_DC,
                                                    MY_DC_VERSION,
                                                    myDeviceIds,
                                                    sizeof(myDeviceIds) / sizeof(uint16_t),
                                                    deviceService,
                                                    &commonCallbacks);

    deviceServiceCallbacks = deviceService;

    legacySecurityCluster = legacySecurityClusterCreate(&legacySecurityClusterCallbacks, deviceService, myDriver);
    zigbeeDriverCommonAddCluster(myDriver, legacySecurityCluster);

    //we dont want the common driver to discover or configure stuff during pairing
    zigbeeDriverCommonSkipConfiguration(myDriver);

    return myDriver;
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
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    return legacySecurityClusterConfigureDevice(legacySecurityCluster, eui64, device, descriptor);
}

static bool getDiscoveredDeviceMetadata(ZigbeeDriverCommon *ctx,
                                        IcDiscoveredDeviceDetails *details,
                                        icStringHashMap *metadata)
{
    stringHashMapPut(metadata, strdup(SENSOR_PROFILE_RESOURCE_QUALIFIED), strdup("true"));

    cJSON *endpoints = cJSON_CreateArray();
    cJSON_AddItemToArray(endpoints, cJSON_CreateNumber(LEGACY_DEVICE_ENDPOINT_NUM));
    stringHashMapPut(metadata, strdup(SENSOR_PROFILE_ENDPOINT_ID_LIST), cJSON_PrintUnformatted(endpoints));
    cJSON_Delete(endpoints);

    return true;
}

static void firmwareUpgradeFailed(ZigbeeDriverCommon *ctx, uint64_t eui64)
{
    (void) ctx;

    legacySecurityClusterHandleFirmwareUpgradeFailed(legacySecurityCluster, eui64);
}

static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    //set up our list of device types that this driver should NOT handle
    icHashMap *excludedDevices = hashMapCreate();
    uint8_t type = KEYPAD_1;
    hashMapPutCopy(excludedDevices, &type, 1, NULL, 0);
    type = KEYFOB_1;
    hashMapPutCopy(excludedDevices, &type, 1, NULL, 0);
    type = REPEATER_SIREN_1;
    hashMapPutCopy(excludedDevices, &type, 1, NULL, 0);
    type = MTL_REPEATER_SIREN;
    hashMapPutCopy(excludedDevices, &type, 1, NULL, 0);
    type = TAKEOVER_1;
    hashMapPutCopy(excludedDevices, &type, 1, NULL, 0);

    bool result = legacySecurityClusterClaimDevice(legacySecurityCluster, details, NULL, excludedDevices);

    hashMapDestroy(excludedDevices, NULL);

    return result;
}

static void postDeviceRemoved(ZigbeeDriverCommon *ctx, icDevice *device)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);
    legacySecurityClusterDeviceRemoved(legacySecurityCluster, eui64);
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    bool result;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    //get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
    LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterGetDetailsCopy(legacySecurityCluster, eui64);

    if (legacyDeviceDetails != NULL)
    {
        const char *sensorType = getSensorTypeValue(legacyDeviceDetails->classification);

        // Populate some common resource values with NULL.  We wont know their real values until we get a
        // status/checkin message
        result = legacySecurityClusterFetchInitialResourceValues(legacySecurityCluster, eui64, device,
                                                                 discoveredDeviceDetails, initialResourceValues);

        //bypassed
        initialResourceValuesPutEndpointValue(initialResourceValues, LEGACY_DEVICE_ENDPOINT_ID,
                                              SENSOR_PROFILE_RESOURCE_BYPASSED, "false");

        //faulted
        initialResourceValuesPutEndpointValue(initialResourceValues, LEGACY_DEVICE_ENDPOINT_ID,
                                              SENSOR_PROFILE_RESOURCE_FAULTED,
                                              legacyDeviceDetails->isFaulted ? "true" : "false");

        //tampered
        initialResourceValuesPutEndpointValue(initialResourceValues, LEGACY_DEVICE_ENDPOINT_ID,
                                              SENSOR_PROFILE_RESOURCE_TAMPERED,
                                              legacyDeviceDetails->isTampered ? "true" : "false");

        //trouble
        const char *troubleResourceId = zigbeeSensorDeviceDriverGetTroubleResource(sensorType);
        if (troubleResourceId != NULL)
        {
            initialResourceValuesPutEndpointValue(initialResourceValues, LEGACY_DEVICE_ENDPOINT_ID,
                                                  troubleResourceId,
                                                  legacyDeviceDetails->isTroubled ? "true" : "false");
        }

        //sensor type
        initialResourceValuesPutEndpointValue(initialResourceValues, LEGACY_DEVICE_ENDPOINT_ID,
                                              SENSOR_PROFILE_RESOURCE_TYPE,
                                              sensorType);

        //qualified
        initialResourceValuesPutEndpointValue(initialResourceValues, LEGACY_DEVICE_ENDPOINT_ID,
                                              SENSOR_PROFILE_RESOURCE_QUALIFIED,
                                              "true");
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to retrieve legacy device details", __FUNCTION__);
        result = false;
    }

    return result;
}

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues)
{
    bool result;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    //get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    AUTO_CLEAN(legacyDeviceDetailsDestroy__auto)
        LegacyDeviceDetails *legacyDeviceDetails = legacySecurityClusterGetDetailsCopy(legacySecurityCluster, eui64);

    if (legacyDeviceDetails != NULL)
    {
        const char *sensorType = getSensorTypeValue(legacyDeviceDetails->classification);

        icDeviceEndpoint *endpoint = createEndpoint(device, LEGACY_DEVICE_ENDPOINT_ID, SENSOR_PROFILE, true);

        // All this is really doing now is setting up some metadata we use for legacy devices.  The common driver
        // takes care of creating common resources
        result = legacySecurityClusterRegisterResources(legacySecurityCluster, eui64, device, discoveredDeviceDetails,
                                                        initialResourceValues);

        //bypassed
        result &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_BYPASSED,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        //faulted
        result &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_FAULTED,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        //tampered
        result &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_TAMPERED,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                    RESOURCE_MODE_EMIT_EVENTS,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        //trouble
        const char *troubleResourceId = zigbeeSensorDeviceDriverGetTroubleResource(sensorType);
        if (troubleResourceId != NULL)
        {
            result &= createEndpointResourceIfAvailable(endpoint,
                                                        troubleResourceId,
                                                        initialResourceValues,
                                                        RESOURCE_TYPE_SENSOR_TROUBLE,
                                                        RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC |
                                                        RESOURCE_MODE_EMIT_EVENTS,
                                                        CACHING_POLICY_ALWAYS) != NULL;
        }

        //sensor type
        result &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_TYPE,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_SENSOR_TYPE,
                                                    RESOURCE_MODE_READABLE,
                                                    CACHING_POLICY_ALWAYS) != NULL;

        //qualified
        result &= createEndpointResourceIfAvailable(endpoint,
                                                    SENSOR_PROFILE_RESOURCE_QUALIFIED,
                                                    initialResourceValues,
                                                    RESOURCE_TYPE_BOOLEAN,
                                                    RESOURCE_MODE_READABLE,
                                                    CACHING_POLICY_ALWAYS) != NULL;
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to retrieve legacy device details", __FUNCTION__);
        result = false;
    }

    return result;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case LEGACY_ICONTROL_SENSOR_DEVICE_ID:
            profile = SENSOR_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}


static cJSON* getTestFaultedMetadata(bool test)
{
    cJSON *metadata = cJSON_CreateObject();
    cJSON_AddBoolToObject(metadata, SENSOR_PROFILE_METADATA_TEST, test);
    return metadata;
}

static void deviceStatusChanged(uint64_t eui64,
                                uint8_t endpointId,
                                const uCStatusMessage *status,
                                const void *ctx)
{
    icLogDebug(LOG_TAG, "%s: status byte1: 0x%02x, byte2: 0x%02x", __FUNCTION__, status->status.byte1, status->status.byte2);

    char *uuid = zigbeeSubsystemEui64ToId(eui64);
    cJSON *resourceFaultedMetadataJson = NULL;
    bool isTestFaultedRestoreWithAlarm = false;

    // check if we need to handle the test fault or test fault restore.
    // fill the metadata with required value and see if we need to send
    // test fault restore specifically first.
    pthread_mutex_lock(&testModeTriggeredSensorMapMtx);

    if (status->status.fields2.test == 1)
    {
        // handle test fault.
        resourceFaultedMetadataJson = getTestFaultedMetadata(status->status.fields2.test);

        if (testModeTriggeredSensorMap == NULL)
        {
            testModeTriggeredSensorMap = stringHashMapCreate();
        }
        stringHashMapPut(testModeTriggeredSensorMap, strdup(uuid), NULL);
    }
    else if (stringHashMapContains(testModeTriggeredSensorMap, uuid) == true)
    {
        // handle test fault restore.
        resourceFaultedMetadataJson = getTestFaultedMetadata(status->status.fields2.test);

        if (status->status.fields1.primaryAlarm == 1 ||
            status->status.fields1.secondaryAlarm == 1)
        {
            isTestFaultedRestoreWithAlarm = true;
        }

        stringHashMapDelete(testModeTriggeredSensorMap, uuid, NULL);

        if (stringHashMapCount(testModeTriggeredSensorMap) == 0)
        {
            stringHashMapDestroy(testModeTriggeredSensorMap, NULL);
            testModeTriggeredSensorMap = NULL;
        }
    }

    pthread_mutex_unlock(&testModeTriggeredSensorMapMtx);

    // if it is test fault restore then we must send the resource value as false first indicating that
    // test fault is restored and then send another with alarm.
    if (isTestFaultedRestoreWithAlarm == true)
    {
        deviceServiceCallbacks->updateResource(uuid,
                                               LEGACY_DEVICE_ENDPOINT_ID,
                                               SENSOR_PROFILE_RESOURCE_FAULTED,
                                               "false",
                                               resourceFaultedMetadataJson);

        // delete the json since we don't want any metadata on following fault true.
        cJSON_Delete(resourceFaultedMetadataJson);
        resourceFaultedMetadataJson = NULL;
    }

    //Endpoint resources
    deviceServiceCallbacks->updateResource(uuid,
                                           LEGACY_DEVICE_ENDPOINT_ID,
                                           SENSOR_PROFILE_RESOURCE_FAULTED,
                                           (status->status.fields1.primaryAlarm == 1 ||
                                           status->status.fields1.secondaryAlarm == 1 ||
                                           status->status.fields2.test == 1) ? "true" : "false",
                                           resourceFaultedMetadataJson);

    if (resourceFaultedMetadataJson != NULL)
    {
        cJSON_Delete(resourceFaultedMetadataJson);
    }

    deviceServiceCallbacks->updateResource(uuid,
                                           LEGACY_DEVICE_ENDPOINT_ID,
                                           SENSOR_PROFILE_RESOURCE_TAMPERED,
                                           status->status.fields1.tamper ? "true" : "false",
                                           NULL);

    // Cleanup
    free(uuid);
}

static void initiateFirmwareUpgrade(ZigbeeDriverCommon *ctx, const char *deviceUuid, const DeviceDescriptor *dd)
{
    icLogDebug(LOG_TAG, "%s: deviceUuid=%s", __FUNCTION__, deviceUuid);
    uint64_t eui64 = zigbeeSubsystemIdToEui64(deviceUuid);
    //let the cluster know its ok to upgrade
    legacySecurityClusterUpgradeFirmware(legacySecurityCluster, eui64, dd);
}

static void upgradeInProgress(uint64_t eui64,
                              bool inProgress,
                              const void *ctx)
{
    zigbeeDriverCommonSetBlockingUpgrade((ZigbeeDriverCommon *) myDriver, eui64, inProgress);
}

static const char *getSensorTypeValue(uCLegacyDeviceClassification classification)
{
    char *result = NULL;

    switch (classification)
    {
        case UC_DEVICE_CLASS_CONTACT_SWITCH:
            result = SENSOR_PROFILE_CONTACT_SWITCH_TYPE;
            break;

        case UC_DEVICE_CLASS_SMOKE:
            result = SENSOR_PROFILE_SMOKE;
            break;

        case UC_DEVICE_CLASS_CO:
            result = SENSOR_PROFILE_CO;
            break;

        case UC_DEVICE_CLASS_MOTION:
            result = SENSOR_PROFILE_MOTION;
            break;

        case UC_DEVICE_CLASS_GLASS_BREAK:
            result = SENSOR_PROFILE_GLASS_BREAK;
            break;

        case UC_DEVICE_CLASS_WATER:
            result = SENSOR_PROFILE_WATER;
            break;

        case UC_DEVICE_CLASS_VIBRATION:
            result = SENSOR_PROFILE_VIBRATION;
            break;

        case UC_DEVICE_CLASS_SIREN:
            result = SENSOR_PROFILE_SIREN;
            break;

        case UC_DEVICE_CLASS_KEYFOB:
            result = SENSOR_PROFILE_KEYFOB;
            break;

        case UC_DEVICE_CLASS_KEYPAD:
            result = SENSOR_PROFILE_KEYPAD;
            break;

        case UC_DEVICE_CLASS_PERSONAL_EMERGENCY:
            result = SENSOR_PROFILE_PERSONAL_EMERGENCY;
            break;

        case UC_DEVICE_CLASS_REMOTE_CONTROL:
            result = SENSOR_PROFILE_REMOTE_CONTROL;
            break;

        default:
            icLogWarn(LOG_TAG, "%s: unsupported classification %d", __FUNCTION__, classification);
            break;
    }

    return result;
}

static const char *zigbeeSensorDeviceDriverGetTroubleResource(const char *sensorType)
{
    const char *resourceId = NULL;
    if (sensorType != NULL)
    {
        if (strcmp(sensorType, SENSOR_PROFILE_SMOKE) == 0)
        {
            resourceId = SENSOR_PROFILE_RESOURCE_DIRTY;
        }
        else if (strcmp(sensorType, SENSOR_PROFILE_CO) == 0)
        {
            resourceId = SENSOR_PROFILE_RESOURCE_END_OF_LIFE;
        }
    }

    return resourceId;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE



