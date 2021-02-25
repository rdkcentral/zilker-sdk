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
// Created by tlea on 4/11/19.
//

#include <icBuildtime.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <stdio.h>
#include <resourceTypes.h>
#include <commonDeviceDefs.h>
#include <deviceModelHelper.h>
#include <memory.h>
#include <zigbeeClusters/alarmsCluster.h>
#include <icConcurrent/delayedTask.h>
#include <zigbeeClusters/powerConfigurationCluster.h>
#include <deviceCommunicationWatchdog.h>
#include <asm/errno.h>
#include <sys/time.h>
#include <pthread.h>
#include <icUtil/stringUtils.h>
#include "zigbeeDriverCommon.h"
#include "xbbDeviceDriver.h"

#define LOG_TAG "xbbDD"
#define DRIVER_NAME "xbb"
#define DEVICE_CLASS_NAME "xbb"
#define DEVICE_PROFILE_NAME "xbb"
#define MY_DC_VERSION 1

#define MY_MANUFACTURER_NAME "ARRIS"
#define MY_MODEL_NAME "XBB1"
#define MY_MODEL_NAME_24 "XBB24"
#define ARRIS_MFG_ID 0x1195
#define COMCAST_ALT_MFG_ID 0x111D

#define XBB_RESOURCE_STATUS                         "status"
#define XBB_RESOURCE_CONFIG                         "config"
#define XBB_RESOURCE_ALARMS                         "alarms"
#define XBB_RESOURCE_SIREN_MAX_DURATION             "sirenMaxDuration"
#define XBB_RESOURCE_SIREN_START                    "sirenStart"
#define XBB_RESOURCE_SIREN_STOP                     "sirenStop"
#define XBB_RESOURCE_SIREN_MUTE                     "sirenMute"

#define STATUS_ATTRIBUTE_ID                         0
#define HEALTH_ATTRIBUTE_ID                         1
#define CHARGING_STATUS_ATTRIBUTE_ID                2
#define TESTING_STATUS_ATTRIBUTE_ID                 3
#define TESTING_STATE_ATTRIBUTE_ID                  4
#define CHARGING_SYSTEM_HEALTH_ATTRIBUTE_ID         5
#define POWERED_DEVICE_IDLE_POWER1_ATTRIBUTE_ID     6
#define POWERED_DEVICE_IDLE_POWER2_ATTRIBUTE_ID     7
#define SECONDS_ON_BATTERY_ATTRIBUTE_ID             8
#define ESTIMATED_MINUTES_REMAINING_ATTRIBUTE_ID    9
#define ESTIMATED_CHARGE_REMAINING_ATTRIBUTE_ID     10
#define CONFIG_LOW_BATTERY_TIME_ATTRIBUTE_ID        11

#define XBB_SIREN_CLUSTER_ID                        0xfd01
#define SIREN_MAX_DURATION_ATTRIBUTE_ID             0
#define SIREN_START_COMMAND_ID                      0
#define SIREN_STOP_COMMAND_ID                       1
#define SIREN_MUTE_COMMAND_ID                       2

#define ARRIS_DIAGNOSTIC_CLUSTER_ID                 0xFCA0
#define ARRIS_DVT_TELEMETRY_ATTRIBUTE_ID            5

#define DISCOVERY_TIMEOUT_SECONDS                   300

#define ALARMS_READ_TIMEOUT_SECONDS                 5

//firmware versions earlier than this could automatically leave if they get upset
#define MIN_FIRMWARE_VERSION_NO_AUTOLEAVE           0x39000000

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

// since we are hooking the claimDevice function, the device id matching done by ZigbeeDriverCommon is skipped
// and this is ignored
static uint16_t myDeviceIds[] = {0};

static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices);

static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details);

static void preDeviceRemoved(ZigbeeDriverCommon *ctx, icDevice *device);

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails);

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
                              IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                              icInitialResourceValues *initialResourceValues);

static bool readEndpointResource(ZigbeeDriverCommon *ctx,
                                 uint32_t endpointNumber,
                                 icDeviceResource *resource,
                                 char **value);

static bool writeEndpointResource(ZigbeeDriverCommon *ctx,
                                  uint32_t endpointNumber,
                                  icDeviceResource *resource,
                                  const char *previousValue,
                                  const char *newValue,
                                  bool *baseDriverUpdatesResource);

static bool executeEndpointResource(ZigbeeDriverCommon *ctx,
                                    uint32_t endpointNumber,
                                    icDeviceResource *resource,
                                    const char *arg,
                                    char **response);

static void handleClusterCommand(ZigbeeDriverCommon *ctx, ReceivedClusterCommand *command);

static void communicationFailed(ZigbeeDriverCommon *ctx, icDevice *device);

static bool readStatus(uint64_t eui64, char **value);

static bool readConfig(uint64_t eui64, char **value);

static bool readSirenMaxDuration(uint64_t eui64, char **value);

static bool readAlarms(uint64_t eui64, char **value);

static void locateBattery();

static const ZigbeeDriverCommonCallbacks commonCallbacks =
        {
                .devicesLoaded = devicesLoaded,
                .claimDevice = claimDevice,
                .preDeviceRemoved = preDeviceRemoved,
                .configureDevice = configureDevice,
                .registerResources = registerResources,
                .readEndpointResource = readEndpointResource,
                .writeEndpointResource = writeEndpointResource,
                .executeEndpointResource = executeEndpointResource,
                .handleClusterCommand = handleClusterCommand,
                .communicationFailed = communicationFailed,
        };

static DeviceServiceCallbacks *deviceServiceCallbacks = NULL;

//The EUI64 of our battery
static uint64_t batteryEui64 = 0;

static pthread_cond_t alarmsReadCond = PTHREAD_COND_INITIALIZER;

static pthread_mutex_t alarmsReadMtx = PTHREAD_MUTEX_INITIALIZER;

static icLinkedList *alarms = NULL; //list of ZigbeeAlarmTableEntrys

DeviceDriver *xbbDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DRIVER_NAME,
                                                                  DEVICE_CLASS_NAME,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  sizeof(myDeviceIds) / sizeof(uint16_t),
                                                                  deviceService,
                                                                  &commonCallbacks);

    deviceServiceCallbacks = deviceService;
    myDriver->neverReject = true;

    return myDriver;
}

// if this battery has firmware older than 0x39000000 then it can automatically leave if it gets upset
static bool batteryCanAutoLeave(const char *uuid)
{
    bool result = true;

    AUTO_CLEAN(free_generic__auto) char *fwVer = deviceServiceGetDeviceFirmwareVersion(uuid);
    uint32_t ver = 0;
    if (fwVer != NULL && stringToUint32(fwVer, &ver) == true)
    {
        result = ver < MIN_FIRMWARE_VERSION_NO_AUTOLEAVE;
    }

    return result;
}

static void devicesLoaded(ZigbeeDriverCommon *ctx, icLinkedList *devices)
{
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE_XBB_AUTO_DISCOVERY
    batteryEui64 = 0;

    if(devices != NULL && linkedListCount(devices) > 0)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
        while (linkedListIteratorHasNext(iterator))
        {
            // if there is more than one battery, remove them.  Also remove batteries that are in comm fail
            // and are running firmware older than when the auto leave feature was removed.
            icDevice *item = (icDevice*)linkedListIteratorGetNext(iterator);

            if (deviceServiceIsDeviceInCommFail(item->uuid) == true && batteryCanAutoLeave(item->uuid) == true)
            {
                icLogError(LOG_TAG, "Old firmware on XBB in comm fail, removing %s", item->uuid);
                deviceServiceCallbacks->removeDevice(item->uuid);
            }
            else if (batteryEui64 != 0)
            {
                icLogError(LOG_TAG, "Duplicate XBB in database, removing %s", item->uuid);
                deviceServiceCallbacks->removeDevice(item->uuid);
            }
            else
            {
                batteryEui64 = zigbeeSubsystemIdToEui64(item->uuid);
                icLogDebug(LOG_TAG, "Loaded XBB %s", item->uuid);
            }
        }
        linkedListIteratorDestroy(iterator);
    }

    // if no valid battery loaded, start discovery
    if (batteryEui64 == 0)
    {
        locateBattery();
    }
#endif
}

//handle identification of the battery differently since we cannot just look at the device id (7 -- combined
// interface) cuz its too generic return true of the device discovered is our battery.
static bool claimDevice(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = false;

    if (details->manufacturer != NULL &&
        details->model != NULL &&
        strcmp(details->manufacturer, MY_MANUFACTURER_NAME) == 0 &&
        (strcmp(details->model, MY_MODEL_NAME) == 0 || strcmp(details->model, MY_MODEL_NAME_24) == 0) &&
        batteryEui64 == 0)
    {
        result = true;

        batteryEui64 = details->eui64;

        //we found our battery
        icLinkedList *deviceClasses = linkedListCreate();
        linkedListAppend(deviceClasses, strdup(DEVICE_CLASS_NAME));
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE_XBB_AUTO_DISCOVERY
        deviceServiceCallbacks->discoverStop(deviceClasses);
#endif
        linkedListDestroy(deviceClasses, NULL);
    }

    return result;
}

static void preDeviceRemoved(ZigbeeDriverCommon *ctx, icDevice *device)
{
    if (zigbeeSubsystemIdToEui64(device->uuid) == batteryEui64)
    {
        batteryEui64 = 0;

        deviceCommunicationWatchdogStopMonitoringDevice(device->uuid);

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE_XBB_AUTO_DISCOVERY
        locateBattery();
#endif
    }
}

static bool configureDevice(ZigbeeDriverCommon *ctx,
                            icDevice *device,
                            DeviceDescriptor *descriptor,
                            IcDiscoveredDeviceDetails *discoveredDeviceDetails)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    //get the eui64 for the device, which is the uuid
    uint64_t eui64 = zigbeeSubsystemIdToEui64(device->uuid);

    //enable high and low temp alarms
    if (zigbeeSubsystemWriteNumber(eui64,
                                   1,
                                   DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                                   true,
                                   DEVICE_TEMPERATURE_ALARM_MASK_ATTRIBUTE_ID,
                                   ZCL_BITMAP8_ATTRIBUTE_TYPE,
                                   3,
                                   1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set temperature alarm mask", __FUNCTION__);
        result = false;
    }

    return result;
}

static bool registerResources(ZigbeeDriverCommon *ctx,
                              icDevice *device,
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

        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        icDeviceEndpoint *endpoint = createEndpoint(device, epName, DEVICE_PROFILE_NAME, true);

        char *tmpValue = NULL;

        //Basic cluster (0): DateCode (note that its not really a date code, but instead a serial number)
        zigbeeSubsystemReadString(eui64, 1, BASIC_CLUSTER_ID, true, DATE_CODE_ATTRIBUTE_ID, &tmpValue);
        createEndpointResource(endpoint,
                               COMMON_DEVICE_RESOURCE_SERIAL_NUMBER,
                               tmpValue,
                               RESOURCE_TYPE_SERIAL_NUMBER,
                               RESOURCE_MODE_READABLE,
                               CACHING_POLICY_ALWAYS);
        free(tmpValue);
        tmpValue = NULL;

        readStatus(eui64, &tmpValue);
        createEndpointResource(endpoint,
                               XBB_RESOURCE_STATUS,
                               tmpValue,
                               RESOURCE_TYPE_XBB_STATUS,
                               RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC,
                               CACHING_POLICY_NEVER);
        free(tmpValue);
        tmpValue = NULL;

        readConfig(eui64, &tmpValue);
        createEndpointResource(endpoint,
                               XBB_RESOURCE_CONFIG,
                               tmpValue,
                               RESOURCE_TYPE_XBB_CONFIG,
                               RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC,
                               CACHING_POLICY_NEVER);
        free(tmpValue);
        tmpValue = NULL;

        readSirenMaxDuration(eui64, &tmpValue);
        createEndpointResource(endpoint,
                               XBB_RESOURCE_SIREN_MAX_DURATION,
                               tmpValue,
                               RESOURCE_TYPE_SECONDS,
                               RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC,
                               CACHING_POLICY_NEVER);

        free(tmpValue);
        tmpValue = NULL;

        //TODO do something with these resource types.
        createEndpointResource(endpoint,
                               XBB_RESOURCE_SIREN_START,
                               tmpValue,
                               RESOURCE_TYPE_XBB_SIRENSTART,
                               RESOURCE_MODE_EXECUTABLE,
                               CACHING_POLICY_NEVER);

        createEndpointResource(endpoint,
                               XBB_RESOURCE_SIREN_STOP,
                               tmpValue,
                               RESOURCE_TYPE_XBB_SIRENSTOP,
                               RESOURCE_MODE_EXECUTABLE,
                               CACHING_POLICY_NEVER);

        createEndpointResource(endpoint,
                               XBB_RESOURCE_SIREN_MUTE,
                               tmpValue,
                               RESOURCE_TYPE_XBB_SIRENMUTE,
                               RESOURCE_MODE_EXECUTABLE,
                               CACHING_POLICY_NEVER);

        createEndpointResource(endpoint,
                               XBB_RESOURCE_ALARMS,
                               NULL,
                               RESOURCE_TYPE_XBB_ALARMS,
                               RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC,
                               CACHING_POLICY_NEVER);

        //TODO represent the current alarms somehow

        zigbeeDriverCommonSetEndpointNumber(endpoint, endpointId);
    }

    return result;
}

static bool
readEndpointResource(ZigbeeDriverCommon *ctx, uint32_t endpointNumber, icDeviceResource *resource, char **value)
{
    bool result = false;

    if (resource == NULL || value == NULL || endpointNumber == 0)
    {
        return false;
    }

    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, resource->id);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);

    if (strcmp(resource->id, XBB_RESOURCE_STATUS) == 0)
    {
        result = readStatus(eui64, value);
    }
    else if (strcmp(resource->id, XBB_RESOURCE_CONFIG) == 0)
    {
        result = readConfig(eui64, value);
    }
    else if (strcmp(resource->id, XBB_RESOURCE_ALARMS) == 0)
    {
        result = readAlarms(eui64, value);
    }
    else if (strcmp(resource->id, XBB_RESOURCE_SIREN_MAX_DURATION) == 0)
    {
        result = readSirenMaxDuration(eui64, value);
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
        icLogDebug(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icLogDebug(LOG_TAG, "%s: endpoint %s: id=%s, previousValue=%s, newValue=%s",
               __FUNCTION__,
               resource->endpointId,
               resource->id,
               previousValue,
               newValue);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);

    if (strcmp(resource->id, XBB_RESOURCE_CONFIG) == 0)
    {
        int rc = 0;
        cJSON *inputObject = cJSON_Parse(newValue);

        if (inputObject != NULL)
        {
            cJSON *attributeObject = NULL;

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "PoweredDeviceIdlePower1")) != NULL)
            {
                rc = zigbeeSubsystemWriteNumberMfgSpecific(eui64,
                                                           1,
                                                           POWER_CONFIGURATION_CLUSTER_ID,
                                                           COMCAST_ALT_MFG_ID,
                                                           true,
                                                           POWERED_DEVICE_IDLE_POWER1_ATTRIBUTE_ID,
                                                           ZCL_INT32U_ATTRIBUTE_TYPE,
                                                           ((uint64_t) attributeObject->valueint) & 0xFFFFFFFF,
                                                           4);
                if (rc != 0)
                {
                    result = false;
                }
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "PoweredDeviceIdlePower2")) != NULL)
            {
                rc = zigbeeSubsystemWriteNumberMfgSpecific(eui64,
                                                           1,
                                                           POWER_CONFIGURATION_CLUSTER_ID,
                                                           COMCAST_ALT_MFG_ID,
                                                           true,
                                                           POWERED_DEVICE_IDLE_POWER2_ATTRIBUTE_ID,
                                                           ZCL_INT32U_ATTRIBUTE_TYPE,
                                                           ((uint64_t) attributeObject->valueint) & 0xFFFFFFFF,
                                                           4);
                if (rc != 0)
                {
                    result = false;
                }
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "ConfigLowBatteryTime")) != NULL)
            {
                rc = zigbeeSubsystemWriteNumberMfgSpecific(eui64,
                                                           1,
                                                           POWER_CONFIGURATION_CLUSTER_ID,
                                                           COMCAST_ALT_MFG_ID,
                                                           true,
                                                           CONFIG_LOW_BATTERY_TIME_ATTRIBUTE_ID,
                                                           ZCL_INT32U_ATTRIBUTE_TYPE,
                                                           ((uint64_t) attributeObject->valueint) & 0xFFFFFFFF,
                                                           4);
                if (rc != 0)
                {
                    result = false;
                }
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "LowTempThreshold")) != NULL)
            {
                rc = zigbeeSubsystemWriteNumber(eui64,
                                                1,
                                                DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                                                true,
                                                LOW_TEMPERATURE_THRESHOLD_ATTRIBUTE_ID,
                                                ZCL_INT16S_ATTRIBUTE_TYPE,
                                                ((uint64_t) attributeObject->valueint) & 0xFFFFu,
                                                2);
                if (rc != 0)
                {
                    result = false;
                }
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "HighTempThreshold")) != NULL)
            {
                rc = zigbeeSubsystemWriteNumber(eui64,
                                                1,
                                                DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                                                true,
                                                HIGH_TEMPERATURE_THRESHOLD_ATTRIBUTE_ID,
                                                ZCL_INT16S_ATTRIBUTE_TYPE,
                                                ((uint64_t) attributeObject->valueint) & 0xFFFFu,
                                                2);
                if (rc != 0)
                {
                    result = false;
                }
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "LowTempDwellTripPoint")) != NULL)
            {
                rc = zigbeeSubsystemWriteNumber(eui64,
                                                1,
                                                DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                                                true,
                                                LOW_TEMPERATURE_DWELL_TRIP_POINT_ATTRIBUTE_ID,
                                                ZCL_INT24U_ATTRIBUTE_TYPE,
                                                ((uint64_t) attributeObject->valueint) & 0xFFFFFFu,
                                                3);
                if (rc != 0)
                {
                    result = false;
                }
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "HighTempDwellTripPoint")) != NULL)
            {
                rc = zigbeeSubsystemWriteNumber(eui64,
                                                1,
                                                DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                                                true,
                                                HIGH_TEMPERATURE_DWELL_TRIP_POINT_ATTRIBUTE_ID,
                                                ZCL_INT24U_ATTRIBUTE_TYPE,
                                                ((uint64_t) attributeObject->valueint) & 0xFFFFFFu,
                                                3);
                if (rc != 0)
                {
                    result = false;
                }
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "DeviceTempAlarmMask")) != NULL)
            {
                rc = zigbeeSubsystemWriteNumber(eui64,
                                                1,
                                                DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                                                true,
                                                DEVICE_TEMPERATURE_ALARM_MASK_ATTRIBUTE_ID,
                                                ZCL_BITMAP8_ATTRIBUTE_TYPE,
                                                ((uint64_t) attributeObject->valueint) & 0x0000FFu,
                                                1);
                if (rc != 0)
                {
                    result = false;
                }
            }

            cJSON_Delete(inputObject);
        }
        else
        {
            icLogError(LOG_TAG, "Invalid config JSON");
            result = false;
        }
    }
    else if (strcmp(resource->id, XBB_RESOURCE_SIREN_MAX_DURATION) == 0)
    {
        uint16_t val = 0;

        if (sscanf(newValue, "%" SCNd16, &val) == 1 &&
            zigbeeSubsystemWriteNumberMfgSpecific(eui64,
                                                  1,
                                                  XBB_SIREN_CLUSTER_ID,
                                                  COMCAST_ALT_MFG_ID,
                                                  true,
                                                  SIREN_MAX_DURATION_ATTRIBUTE_ID,
                                                  ZCL_INT16U_ATTRIBUTE_TYPE,
                                                  ((uint64_t) val) & 0xFFFFu,
                                                  2) == true)
        {
            result = true;
        }
        else
        {
            icLogError(LOG_TAG, "Failed to set siren max duration to %s", newValue);
            result = false;
        }
    }

    if (result)
    {
        deviceServiceCallbacks->updateResource(resource->deviceUuid,
                                               resource->endpointId,
                                               resource->id,
                                               newValue,
                                               NULL);
    }

    return result;
}

static bool executeEndpointResource(ZigbeeDriverCommon *ctx,
                                    uint32_t endpointNumber,
                                    icDeviceResource *resource,
                                    const char *arg,
                                    char **response)
{
    bool result = true;

    if (resource == NULL || endpointNumber != 1 ||
        (strcmp(resource->id, XBB_RESOURCE_SIREN_START) == 0 && arg == NULL))
    {
        icLogDebug(LOG_TAG, "executeEndpointResource: invalid arguments");
        return false;
    }

    icLogDebug(LOG_TAG,
               "executeEndpointResource on endpoint %s: id=%s, arg=%s",
               resource->endpointId,
               resource->id,
               arg);

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);

    if (strcmp(resource->id, XBB_RESOURCE_SIREN_START) == 0)
    {
        cJSON *inputObject = cJSON_Parse(arg);

        uint16_t *frequency = NULL;
        uint8_t *volume = NULL;
        uint16_t *duration = NULL;
        uint8_t *temporalPattern = NULL;
        uint8_t *numPulses = NULL;
        uint16_t *onPhaseDuration = NULL;
        uint16_t *offPhaseDuration = NULL;
        uint16_t *pauseDuration = NULL;

        if (inputObject != NULL)
        {
            cJSON *attributeObject = NULL;

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "Frequency")) != NULL)
            {
                frequency = (uint16_t *) malloc(2);
                *frequency = (uint16_t) (attributeObject->valueint & 0xFFFF);
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "Volume")) != NULL)
            {
                volume = (uint8_t *) malloc(1);
                *volume = (uint8_t) (attributeObject->valueint & 0xFF);
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "Duration")) != NULL)
            {
                duration = (uint16_t *) malloc(2);
                *duration = (uint16_t) (attributeObject->valueint & 0xFFFF);
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "TemporalPattern")) != NULL)
            {
                temporalPattern = (uint8_t *) malloc(1);

                if (strcmp(attributeObject->valuestring, "none") == 0)
                {
                    *temporalPattern = (uint8_t) 0;
                }
                else if (strcmp(attributeObject->valuestring, "3") == 0)
                {
                    *temporalPattern = (uint8_t) 1;
                }
                else if (strcmp(attributeObject->valuestring, "4") == 0)
                {
                    *temporalPattern = (uint8_t) 2;
                }
                else if (strcmp(attributeObject->valuestring, "user") == 0)
                {
                    *temporalPattern = (uint8_t) 3;
                }
                else
                {
                    icLogWarn(LOG_TAG,
                              "Unexpected temporal pattern %s, using 'none' (0) instead",
                              attributeObject->valuestring);
                    *temporalPattern = (uint8_t) 0;
                }
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "NumPulses")) != NULL)
            {
                numPulses = (uint8_t *) malloc(1);
                *numPulses = (uint8_t) (attributeObject->valueint & 0xFF);
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "OnPhaseDuration")) != NULL)
            {
                onPhaseDuration = (uint16_t *) malloc(2);
                *onPhaseDuration = (uint16_t) (attributeObject->valueint & 0xFFFF);
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "OffPhaseDuration")) != NULL)
            {
                offPhaseDuration = (uint16_t *) malloc(2);
                *offPhaseDuration = (uint16_t) (attributeObject->valueint & 0xFFFF);
            }

            if ((attributeObject = cJSON_GetObjectItem(inputObject, "PauseDuration")) != NULL)
            {
                pauseDuration = (uint16_t *) malloc(2);
                *pauseDuration = (uint16_t) (attributeObject->valueint & 0xFFFF);
            }

            //sanity check.... if temporal pattern is user (3), then we validate the numPulses, on/off phase durations which cannot be zero)
            if (frequency == NULL ||
                volume == NULL ||
                duration == NULL ||
                temporalPattern == NULL ||
                numPulses == NULL ||
                onPhaseDuration == NULL ||
                offPhaseDuration == NULL ||
                pauseDuration == NULL ||
                (*temporalPattern == 3 && (*numPulses == 0 || *onPhaseDuration == 0 || *offPhaseDuration == 0)))
            {
                icLogDebug(LOG_TAG, "executeEndpointResource: invalid arguments");
                result = false;
            }
            else
            {
                uint16_t i = 0;
                uint8_t *cmdBuf = (uint8_t *) calloc(1, 13);
                cmdBuf[i++] = (uint8_t) (*frequency & 0xff);
                cmdBuf[i++] = (uint8_t) (*frequency >> 8 & 0xff);
                cmdBuf[i++] = *volume;
                cmdBuf[i++] = (uint8_t) (*duration & 0xff);
                cmdBuf[i++] = (uint8_t) (*duration >> 8 & 0xff);
                cmdBuf[i++] = *temporalPattern;
                cmdBuf[i++] = *numPulses;
                cmdBuf[i++] = (uint8_t) (*onPhaseDuration & 0xff);
                cmdBuf[i++] = (uint8_t) (*onPhaseDuration >> 8 & 0xff);
                cmdBuf[i++] = (uint8_t) (*offPhaseDuration & 0xff);
                cmdBuf[i++] = (uint8_t) (*offPhaseDuration >> 8 & 0xff);
                cmdBuf[i++] = (uint8_t) (*pauseDuration & 0xff);
                cmdBuf[i++] = (uint8_t) (*pauseDuration >> 8 & 0xff);

                if (zigbeeSubsystemSendMfgCommand(eui64,
                                                  1,
                                                  XBB_SIREN_CLUSTER_ID,
                                                  true,
                                                  SIREN_START_COMMAND_ID,
                                                  COMCAST_ALT_MFG_ID,
                                                  cmdBuf,
                                                  i) != 0)
                {
                    icLogError(LOG_TAG, "Failed to send siren start command");
                    result = false;
                }
            }

            free(frequency);
            free(volume);
            free(duration);
            free(temporalPattern);
            free(numPulses);
            free(onPhaseDuration);
            free(offPhaseDuration);
            free(pauseDuration);
        }
        else
        {
            icLogError(LOG_TAG, "Invalid siren start JSON");
            result = false;
        }
    }
    else if (strcmp(resource->id, XBB_RESOURCE_SIREN_STOP) == 0)
    {
        if (zigbeeSubsystemSendMfgCommand(eui64,
                                          1,
                                          XBB_SIREN_CLUSTER_ID,
                                          true,
                                          SIREN_STOP_COMMAND_ID,
                                          COMCAST_ALT_MFG_ID,
                                          NULL,
                                          0) != 0)
        {
            icLogError(LOG_TAG, "Failed to send siren stop command");
            result = false;
        }
    }
    else if (strcmp(resource->id, XBB_RESOURCE_SIREN_MUTE) == 0)
    {
        if (zigbeeSubsystemSendMfgCommand(eui64,
                                          1,
                                          XBB_SIREN_CLUSTER_ID,
                                          true,
                                          SIREN_MUTE_COMMAND_ID,
                                          COMCAST_ALT_MFG_ID,
                                          NULL,
                                          0) != 0)
        {
            icLogError(LOG_TAG, "Failed to send siren mute command");
            result = false;
        }
    }

    return result;
}

static void handleAlarmsClusterCommand(uint8_t* commandData, uint32_t commandDataLen)
{
    pthread_mutex_lock(&alarmsReadMtx);

    //first check the status byte (byte 0).  If it is success (0) then we can process the rest, otherwise we are done
    if (commandData[0] == 0)
    {
        if (commandDataLen != 8) //the payload of an alarm record should be 8 bytes
        {
            icLogDebug(LOG_TAG, "handleAlarmsClusterCommand: unexpected payload length 0x08"
            PRIu32, commandDataLen);
            pthread_mutex_unlock(&alarmsReadMtx);
            return;
        }

        ZigbeeAlarmTableEntry* entry = (ZigbeeAlarmTableEntry*) calloc(1, sizeof(ZigbeeAlarmTableEntry));

        entry->alarmCode = commandData[1];
        entry->clusterId = commandData[2] + (commandData[3] << 8);
        entry->timeStamp = commandData[4] + ((uint32_t) commandData[5] << 8) + ((uint32_t) commandData[6] << 16) +
                           ((uint32_t) commandData[7] << 24);

        //zigbee epoch is 1/1/2000 00:00 GMT.  To convert to POSIX time (ISO 8601), add 946684800 (the difference between the two epochs)
        entry->timeStamp += 946684800;

        icLogDebug(LOG_TAG, "handleAlarmsClusterCommand: got alarm:  code=0x%02x, clusterId=0x%04x, timeStamp=%"
        PRIu32, entry->alarmCode, entry->clusterId, entry->timeStamp);

        linkedListAppend(alarms, entry);

        //Trigger the next read
        zigbeeSubsystemSendCommand(batteryEui64, 1, ALARMS_CLUSTER_ID, true, ALARMS_GET_ALARM_COMMAND_ID, NULL, 0);
    }
    else
    {
        if (alarms == NULL)
        {
            icLogDebug(LOG_TAG, "handleAlarmsClusterCommand: got an alarm event (ignored)");
        }
        else
        {
            icLogDebug(LOG_TAG, "handleAlarmsClusterCommand: done retrieving alarms");
            pthread_cond_signal(&alarmsReadCond);
        }
    }

    pthread_mutex_unlock(&alarmsReadMtx);
}

static void handleClusterCommand(ZigbeeDriverCommon *ctx, ReceivedClusterCommand *command)
{
    if (command->eui64 != batteryEui64)
    {
        icLogDebug(LOG_TAG, "handleClusterCommand: ignoring command from unexpected device 0x016"
        PRIx64, command->eui64);
        return;
    }

    if (command->profileId != HA_PROFILE_ID)
    {
        icLogDebug(LOG_TAG, "handleClusterCommand: ignoring command from non HA profile 0x%04x", command->profileId);
        return;
    }

    if (command->sourceEndpoint != 1)
    {
        icLogDebug(LOG_TAG,
                   "handleClusterCommand: ignoring command from unexpected endpoint 0x%02x",
                   command->sourceEndpoint);
        return;
    }

    if (command->mfgSpecific)
    {
        icLogDebug(LOG_TAG, "handleClusterCommand: ignoring manufacturer specific command");
        return;
    }

    switch (command->clusterId)
    {
        case ALARMS_CLUSTER_ID:
            handleAlarmsClusterCommand(command->commandData, command->commandDataLen);
            break;

        default:
            icLogDebug(LOG_TAG,
                       "handleClusterCommand: ignoring unexpected command for cluster 0x%04x",
                       command->clusterId);
            break;
    }
}

static void communicationFailed(ZigbeeDriverCommon *ctx, icDevice *device)
{
    // if this battery went into comm failure and it is running firmware that can auto leave, we have to discover
    // it again
    if (batteryCanAutoLeave(device->uuid) == true)
    {
        icLogError(LOG_TAG,
                   "communicationFailed and battery has old firmware.  Removing device and starting discovery");
        deviceServiceCallbacks->removeDevice(device->uuid);
    }
}

static bool readMfgNumberIntoJson(uint64_t eui64,
                                  uint16_t mfgId,
                                  uint16_t clusterId,
                                  uint16_t attributeId,
                                  cJSON *json,
                                  const char *jsonFieldName)
{
    bool result = false;

    uint64_t tmp;
    if (zigbeeSubsystemReadNumberMfgSpecific(eui64, 1, clusterId, mfgId, true, attributeId, &tmp) == 0)
    {
        cJSON_AddNumberToObject(json, jsonFieldName, tmp);
        result = true;
    }

    return result;
}

static bool
readNumberIntoJson(uint64_t eui64, uint16_t clusterId, uint16_t attributeId, cJSON *json, const char *jsonFieldName)
{
    bool result = false;

    uint64_t tmp;
    if (zigbeeSubsystemReadNumber(eui64, 1, clusterId, true, attributeId, &tmp) == 0)
    {
        cJSON_AddNumberToObject(json, jsonFieldName, tmp);
        result = true;
    }

    return result;
}

// build up a JSON object representing the complete battery status
static bool readStatus(uint64_t eui64, char **value)
{
    cJSON *status = cJSON_CreateObject();

    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          STATUS_ATTRIBUTE_ID,
                          status,
                          "BatteryStatus");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          HEALTH_ATTRIBUTE_ID,
                          status,
                          "BatteryHealth");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          CHARGING_STATUS_ATTRIBUTE_ID,
                          status,
                          "ChargingStatus");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          TESTING_STATUS_ATTRIBUTE_ID,
                          status,
                          "TestingStatus");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          TESTING_STATE_ATTRIBUTE_ID,
                          status,
                          "TestingState");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          CHARGING_SYSTEM_HEALTH_ATTRIBUTE_ID,
                          status,
                          "ChargingSystemHealth");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          SECONDS_ON_BATTERY_ATTRIBUTE_ID,
                          status,
                          "SecondsOnBattery");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          ESTIMATED_MINUTES_REMAINING_ATTRIBUTE_ID,
                          status,
                          "EstimatedMinutesRemaining");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          ESTIMATED_CHARGE_REMAINING_ATTRIBUTE_ID,
                          status,
                          "EstimatedChargeRemaining");
    readNumberIntoJson(eui64,
                       DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                       CURRENT_TEMPERATURE_ATTRIBUTE_ID,
                       status,
                       "CurrentTemperature");
    readNumberIntoJson(eui64,
                       DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                       MIN_TEMPERATURE_EXPERIENCED_ATTRIBUTE_ID,
                       status,
                       "MinTempExperienced");
    readNumberIntoJson(eui64,
                       DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                       MAX_TEMPERATURE_EXPERIENCED_ATTRIBUTE_ID,
                       status,
                       "MaxTempExperienced");
    readNumberIntoJson(eui64, ALARMS_CLUSTER_ID, ALARMS_ALARM_COUNT_ATTRIBUTE_ID, status, "AlarmCount");

    char *vendorBuff = NULL;
    if (zigbeeSubsystemReadStringMfgSpecific(eui64,
                                             1,
                                             ARRIS_DIAGNOSTIC_CLUSTER_ID,
                                             ARRIS_MFG_ID,
                                             true,
                                             ARRIS_DVT_TELEMETRY_ATTRIBUTE_ID,
                                             &vendorBuff) == 0)
    {
        // This is ugly... ARRIS declares this attribute as a string, but it just an array of 32 bytes.  Cant use strlen on the result.
        char hexBuff[65]; // 32 bytes in hex plus null
        for (int i = 0; i < 32; i++)
        {
            sprintf(hexBuff + (i * 2), "%02X", ((0xff) & vendorBuff[i]));
        }
        cJSON_AddStringToObject(status, "VendorSpecific", hexBuff);
    }
    free(vendorBuff);

    //TODO get the actual alarms (if any) and add as an array of sub-objects

    *value = cJSON_Print(status);
    cJSON_Delete(status);
    return true;
}

// build up a JSON object representing the complete battery config
static bool readConfig(uint64_t eui64, char **value)
{
    cJSON *config = cJSON_CreateObject();

    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          POWERED_DEVICE_IDLE_POWER1_ATTRIBUTE_ID,
                          config,
                          "PoweredDeviceIdlePower1");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          POWERED_DEVICE_IDLE_POWER2_ATTRIBUTE_ID,
                          config,
                          "PoweredDeviceIdlePower2");
    readMfgNumberIntoJson(eui64,
                          COMCAST_ALT_MFG_ID,
                          POWER_CONFIGURATION_CLUSTER_ID,
                          CONFIG_LOW_BATTERY_TIME_ATTRIBUTE_ID,
                          config,
                          "ConfigLowBatteryTime");
    readNumberIntoJson(eui64,
                       DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                       LOW_TEMPERATURE_THRESHOLD_ATTRIBUTE_ID,
                       config,
                       "LowTempThreshold");
    readNumberIntoJson(eui64,
                       DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                       HIGH_TEMPERATURE_THRESHOLD_ATTRIBUTE_ID,
                       config,
                       "HighTempThreshold");
    readNumberIntoJson(eui64,
                       DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                       LOW_TEMPERATURE_DWELL_TRIP_POINT_ATTRIBUTE_ID,
                       config,
                       "LowTempDwellTripPoint");
    readNumberIntoJson(eui64,
                       DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                       HIGH_TEMPERATURE_DWELL_TRIP_POINT_ATTRIBUTE_ID,
                       config,
                       "HighTempDwellTripPoint");
    readNumberIntoJson(eui64,
                       DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID,
                       DEVICE_TEMPERATURE_ALARM_MASK_ATTRIBUTE_ID,
                       config,
                       "DeviceTempAlarmMask");

    *value = cJSON_Print(config);
    cJSON_Delete(config);
    return true;
}

static bool readSirenMaxDuration(uint64_t eui64, char **value)
{
    bool result = false;

    uint64_t val = 0;
    if (zigbeeSubsystemReadNumberMfgSpecific(eui64,
                                             1,
                                             XBB_SIREN_CLUSTER_ID,
                                             COMCAST_ALT_MFG_ID,
                                             true,
                                             SIREN_MAX_DURATION_ATTRIBUTE_ID,
                                             &val) == 0)
    {
        *value = (char *) calloc(1, 6); //max unsigned 16 bit 65535 + \0
        snprintf(*value, 6, "%" PRIu16, (uint16_t) val);
        result = true;
    }
    else
    {
        *value = NULL;
    }

    return result;
}

static bool readAlarms(uint64_t eui64, char **value)
{
    bool result = false;
    cJSON *alarmsJsonArray = cJSON_CreateArray();

    pthread_mutex_lock(&alarmsReadMtx);

    if (alarms != NULL)
    {
        icLogWarn(LOG_TAG, "readAlarms: alarm retrieval already in progress");
        pthread_mutex_unlock(&alarmsReadMtx);
        return false;
    }

    alarms = linkedListCreate();

    zigbeeSubsystemSendCommand(eui64, 1, ALARMS_CLUSTER_ID, true, ALARMS_GET_ALARM_COMMAND_ID, NULL, 0);

    struct timespec ts;
#ifdef CONFIG_OS_DARWIN
    struct timeval now;
    gettimeofday(&now, NULL);
    ts.tv_sec = now.tv_sec;
    ts.tv_nsec = now.tv_usec * 1000;
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    ts.tv_sec += ALARMS_READ_TIMEOUT_SECONDS;

    if (ETIMEDOUT == pthread_cond_timedwait(&alarmsReadCond, &alarmsReadMtx, &ts))
    {
        icLogWarn(LOG_TAG, "readAlarms: request timed out, aborting");
    }
    else
    {
        icLogDebug(LOG_TAG, "readAlarms: got %d alarms.", linkedListCount(alarms));
        icLinkedListIterator *iterator = linkedListIteratorCreate(alarms);
        while (linkedListIteratorHasNext(iterator))
        {
            ZigbeeAlarmTableEntry *item = (ZigbeeAlarmTableEntry *) linkedListIteratorGetNext(iterator);

            cJSON *itemJson = cJSON_CreateObject();
            switch (item->clusterId)
            {
                case POWER_CONFIGURATION_CLUSTER_ID:
                {
                    switch (item->alarmCode)
                    {
                        case 0xc0: //bad battery
                            cJSON_AddStringToObject(itemJson, "type", "badBattery");
                            break;
                        case 0xc1: //low battery
                            cJSON_AddStringToObject(itemJson, "type", "lowBattery");
                            break;
                        case 0xc2: //charging system bad
                            cJSON_AddStringToObject(itemJson, "type", "chargingSystemBad");
                            break;
                        case 0xc3: //missing battery
                            cJSON_AddStringToObject(itemJson, "type", "missingBattery");
                            break;

                        default:
                            cJSON_AddStringToObject(itemJson, "type", "unknown");
                            break;
                    }

                    break;
                }

                case DEVICE_TEMPERATURE_CONFIGURATION_CLUSTER_ID:
                {
                    switch (item->alarmCode)
                    {
                        case 0x00: //low temp
                            cJSON_AddStringToObject(itemJson, "type", "lowTemp");
                            break;
                        case 0x01: //high temp
                            cJSON_AddStringToObject(itemJson, "type", "highTemp");
                            break;

                        default:
                            cJSON_AddStringToObject(itemJson, "type", "unknown");
                            break;
                    }

                    break;
                }

                default:
                    cJSON_AddStringToObject(itemJson, "type", "unknown");
                    break;
            }
            cJSON_AddNumberToObject(itemJson, "timestamp", item->timeStamp);

            cJSON_AddItemToArray(alarmsJsonArray, itemJson);
        }
        linkedListIteratorDestroy(iterator);

        result = true;
    }

    if (alarms != NULL)
    {
        linkedListDestroy(alarms, NULL);
        alarms = NULL;
    }

    pthread_mutex_unlock(&alarmsReadMtx);

    *value = cJSON_Print(alarmsJsonArray);
    cJSON_Delete(alarmsJsonArray);
    return result;
}

//locate a single battery through a limited time discovery window
static void locateBattery()
{
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, strdup(DEVICE_CLASS_NAME));
    deviceServiceCallbacks->discoverStart(deviceClasses, DISCOVERY_TIMEOUT_SECONDS, false);
    linkedListDestroy(deviceClasses, NULL);
}

#endif // CONFIG_SERVICE_DEVICE_ZIGBEE



