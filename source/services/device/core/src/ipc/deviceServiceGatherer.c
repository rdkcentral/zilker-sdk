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
// Created by jelder380 on 1/17/19.
//

#include <icBuildtime.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include <string.h>
#include <stdio.h>

#include <icIpc/ipcStockMessagesPojo.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>
#include <icLog/logging.h>
#include <deviceService/deviceService_pojo.h>
#include <zhal/zhal.h>
#include <cjson/cJSON.h>
#include <icUtil/stringUtils.h>
#include <commonDeviceDefs.h>
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include "subsystems/zigbee/zigbeeEventTracker.h"
#include "deviceServiceIpcCommon.h"
#include "deviceServiceGatherer.h"
#include "deviceServicePrivate.h"
#include "deviceService.h"
#include <deviceDriverManager.h>
#include <icTypes/icStringBuffer.h>

#define LOG_TAG "deviceServiceGather"

// the device service / zigbee core keys
#define DEVICE_TITLE_KEY                    "Device"
#define PAN_ID_KEY                          "panid"
#define OPEN_FOR_JOIN_KEY                   "openForJoin"
#define IS_ZIGBEE_NET_CONFIGURED_KEY        "isConfigured"
#define IS_ZIGBEE_NET_AVAILABLE_KEY         "isAvailable"
#define IS_ZIGBEE_NETWORK_UP_KEY            "networkUp"
#define EUI_64_KEY                          "eui64"
#define CHANNEL_KEY                         "channel"
#define DEVICE_FW_UPGRADE_FAIL_CNT_KEY      "zigbeeDevFwUpgFails"
#define DEVICE_FW_UPGRADE_SUCCESS_CNT_KEY   "zigbeeDevFwUpgSuccesses"
#define DEVICE_FW_UPGRADE_FAILURE_KEY       "zigbeeDevFwUpgFail_"
#define CHANNEL_SCAN_MAX_KEY                "emaxc"
#define CHANNEL_SCAN_MIN_KEY                "eminc"
#define CHANNEL_SCAN_AVG_KEY                "eavgc"
#define CAMERA_TOTAL_DEVICE_LIST_KEY        "cameraDeviceList"
#define CAMERA_CONNECTED_LIST_KEY           "cameraConnectedList"
#define CAMERA_DISCONNECTED_LIST_KEY        "cameraDisconnectedList"

// device types to ignore
#define IGNORE_INTEGRATED_PIEZO_DEVICE "intPiezoDD"

// null value in device resource
#define DEVICE_NULL_VALUE "(null)"
#define VALUE_IS_EMPTY_STRING ""

// constants
#define INITIAL_CAMERA_BUFFER_SIZE 18 // size of MAC Address with collons and null char

/*
 * structs for all of the device information
 *
 * NOTE: only points to the strings, no memory is needed to be created
 */

typedef struct
{
    const char *manufacturer;
    const char *model;
    const char *firmVer;
    const char *hardVer;
    const char *nearLqi;
    const char *farLqi;
    const char *nearRssi;
    const char *farRssi;
    const char *temp;
    const char *batteryVolts;
    const char *lowBattery;
    const char *commFail;
    const char *troubled;
    const char *bypassed;
    const char *tampered;
    const char *faulted;
}basicDeviceInfo;

typedef struct
{
    const char *macAddress;
    const char *commFail;
}cameraDeviceInfo;

typedef struct
{
    const char *reportTime;
    const char *clusterId;
    const char *attributeId;
    const char *data;
}attributeDeviceInfo;

typedef struct
{
    const char *time;
    const char *isSecure;
}rejoinDeviceInfo;

typedef struct
{
    const char *time;
}checkInDeviceInfo;

// private functions
static const char *customStringToString(const char *src);
static void initDeviceInfo(basicDeviceInfo *deviceInfo);
static void initCameraDeviceInfo(cameraDeviceInfo *deviceInfo);
static void initAttDeviceInfo(attributeDeviceInfo *attDeviceInfo, int size);
static void initRejoinDeviceInfo(rejoinDeviceInfo *rejoinInfo, int size);
static void initCheckInDeviceInfo(checkInDeviceInfo *checkInInfo, int size);
static void collectResources(basicDeviceInfo *deviceInfo, icDevice *device);
static void collectEndpointResources(basicDeviceInfo *deviceInfo, icDevice *device);
static void collectCameraResources(cameraDeviceInfo *deviceInfo, icDevice *device);
static void convertAttributeReports(icLinkedList *attList, attributeDeviceInfo *deviceInfo);
static void convertRejoins(icLinkedList *rejoinList, rejoinDeviceInfo *deviceInfo);
static void convertCheckIns(icLinkedList *checkInList, checkInDeviceInfo *deviceInfo);
static char *createDeviceStringList(const char *uuid, basicDeviceInfo *deviceInfo, attributeDeviceInfo *attList,
                                    rejoinDeviceInfo *rejoinList, checkInDeviceInfo *checkInList,
                                    deviceEventCounterItem *deviceCounters, const char *deviceClass);

/**
 * Collects all information about each device
 * on the system, which is added to the
 * runtimeStats output per device.
 *
 * Each list added contains the following:
 *
 * The 5 most recent rejoin info,
 * The 5 most recent check-in info,
 * The 8 most recent attribute reports,
 * All of the devices endpoints and resources,
 * The device counters:
 *      Total rejoins for device,
 *      Total Secure rejoins for device,
 *      Total Un-secure rejoins for device,
 *      Total Duplicate Sequence Numbers for device,
 *      Total Aps Ack Failures for device
 *
 * NOTE: each device list has to be in the format:
 *
 * "uuid,manufacturer,model,firmwareVersion,
 * AttributeReportTime1,ClusterId1,AttributeId1,Data1,
 * AttributeReportTime2,ClusterId2,AttributeId2,Data2,
 * AttributeReportTime3,ClusterId3,AttributeId3,Data3,
 * AttributeReportTime4,ClusterId4,AttributeId4,Data4,
 * AttributeReportTime5,ClusterId5,AttributeId5,Data5,
 * AttributeReportTime6,ClusterId6,AttributeId6,Data6,
 * AttributeReportTime7,ClusterId7,AttributeId7,Data7,
 * AttributeReportTime8,ClusterId8,AttributeId8,Data8,
 * rejoinTime1,isSecure1,
 * rejoinTime2,isSecure2,
 * rejoinTime3,isSecure3,
 * rejoinTime4,isSecure4,
 * rejoinTime5,isSecure5,
 * checkInTime1,
 * checkInTime2,
 * checkInTime3,
 * checkInTime4,
 * checkInTime5,
 * type,hardwareVersion,lqi(ne/fe),
 * rssi(ne/fe),temperature,batteryVoltage,
 * lowBattery,commFailure,troubled,
 * bypassed,tampered,faulted,
 * totalRejoinCounter,
 * totalSecureRejoinCounter,
 * totalUnSecureRejoinCounter,
 * totalDuplicateSequenceNumberCounter,
 * totalApsAckFailureCounter"
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectAllDeviceStatistics(runtimeStatsPojo *output)
{
    // ----------------------------------------------------------------
    // get list of devices and loop though them
    // ----------------------------------------------------------------

    icLinkedList *deviceList = deviceServiceGetAllDevices();
    icLinkedListIterator *deviceIter = linkedListIteratorCreate(deviceList);
    int deviceCount = 0;
    while(linkedListIteratorHasNext(deviceIter))
    {
        // ----------------------------------------------------------------
        // gather all device information
        // ----------------------------------------------------------------

        // init strings
        //
        basicDeviceInfo statList;
        initDeviceInfo(&statList);

        attributeDeviceInfo attDeviceInfo[MAX_NUMBER_OF_ATTRIBUTE_REPORTS];
        initAttDeviceInfo(attDeviceInfo, MAX_NUMBER_OF_ATTRIBUTE_REPORTS);

        rejoinDeviceInfo rejoinInfo[MAX_NUMBER_OF_REJOINS];
        initRejoinDeviceInfo(rejoinInfo, MAX_NUMBER_OF_REJOINS);

        checkInDeviceInfo checkInInfo[MAX_NUMBER_OF_CHECK_INS];
        initCheckInDeviceInfo(checkInInfo, MAX_NUMBER_OF_CHECK_INS);

        // get device
        //
        icDevice *device = (icDevice *) linkedListIteratorGetNext(deviceIter);

        if (device->uuid == NULL || device->deviceClass == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to use device %d ID and/or deviceClass are empty", __FUNCTION__, deviceCount);
            continue;
        }

        deviceCount ++;   // used for which device is being looked at

        // get the basic device resources
        //
        collectResources(&statList, device);

        // now get the endpoint resources
        //
        collectEndpointResources(&statList, device);

        // get the counters for device
        //
        deviceEventCounterItem *deviceEventCounters = zigbeeEventTrackerCollectEventCountersForDevice(device->uuid);

        // get attribute report list, rejoin list, and check-in list
        //
        icLinkedList *attList = zigbeeEventTrackerCollectAttributeReportEventsForDevice(device->uuid);
        convertAttributeReports(attList, attDeviceInfo);

        icLinkedList *rejoinList = zigbeeEventTrackerCollectRejoinEventsForDevice(device->uuid);
        convertRejoins(rejoinList, rejoinInfo);

        icLinkedList *checkInList = zigbeeEventTrackerCollectCheckInEventsForDevice(device->uuid);
        convertCheckIns(checkInList, checkInInfo);

        // ----------------------------------------------------------------
        // create the device information list and add to output
        // ----------------------------------------------------------------

        // convert these strings into detailed report string list
        //
        char *deviceInfoList = createDeviceStringList(device->uuid, &statList, attDeviceInfo,
                rejoinInfo, checkInInfo, deviceEventCounters, device->deviceClass);

        // sanity check
        //
        if (deviceInfoList != NULL)
        {
            // create the detail report list key
            //
            char *listTag = stringBuilder("%s %d", DEVICE_TITLE_KEY, deviceCount);

            // now add to output
            //
            put_string_in_runtimeStatsPojo(output, listTag, deviceInfoList);

            // cleanup ... no longer needed
            free(listTag);
            free(deviceInfoList);
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: was unable to create device information string list for device %s", __FUNCTION__, device->uuid);
        }

        // cleanup ... no longer needed
        linkedListDestroy(attList, destroyDeviceAttributeItem);
        linkedListDestroy(rejoinList, destroyDeviceRejoinItem);
        linkedListDestroy(checkInList, NULL);
        free(deviceEventCounters);
    }

    linkedListIteratorDestroy(deviceIter);
    linkedListDestroy(deviceList, (linkedListItemFreeFunc) deviceDestroy);

    // finally, give each driver a chance to add to/update the runtime statistics
    //
    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    icLinkedListIterator *driverIter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(driverIter))
    {
        DeviceDriver *driver = (DeviceDriver *)linkedListIteratorGetNext(driverIter);
        if (driver->fetchRuntimeStats != NULL)
        {
            icStringHashMap *stats = stringHashMapCreate();
            driver->fetchRuntimeStats(driver->callbackContext, stats);

            icStringHashMapIterator *statsIter = stringHashMapIteratorCreate(stats);
            while (stringHashMapIteratorHasNext(statsIter))
            {
                char *key, *value;
                stringHashMapIteratorGetNext(statsIter, &key, &value);
                put_string_in_runtimeStatsPojo(output, key, value);
            }
            stringHashMapIteratorDestroy(statsIter);
            stringHashMapDestroy(stats, NULL);
        }
    }
    linkedListIteratorDestroy(driverIter);
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);

}

/**
 * Collect all of the Zigbee counters from Zigbee core
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectZigbeeNetworkCounters(runtimeStatsPojo *output)
{
    // get the counters
    cJSON *counters = zigbeeSubsystemGetAndClearCounters();
    if (counters != NULL)
    {
        // now sort through them
        //
        cJSON *counterIter = counters->child;
        while (counterIter != NULL)
        {
            // make sure it is integer values
            //
            if (cJSON_IsNumber(counterIter))
            {
                // only add values if they are not 0
                //
                if (counterIter->valueint != 0)
                {
                    put_int_in_runtimeStatsPojo(output, counterIter->string, counterIter->valueint);
                }
            }

            // iterate to the next one
            //
            counterIter = counterIter->next;
        }

        // clean up
        //
        cJSON_Delete(counters);
    }
}

/**
 * Collect all of the Zigbee Core Network status
 * includes panID, channel, openForJoin, network up,
 * and eui64
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectZigbeeCoreNetworkStatistics(runtimeStatsPojo *output)
{
    zhalSystemStatus zigbeeNetworkStatus;
    memset(&zigbeeNetworkStatus, 0, sizeof(zhalSystemStatus));

    // make the call to zigbee core
    int rc = zigbeeSubsystemGetSystemStatus(&zigbeeNetworkStatus);

    // if it gathered stats
    if (rc == 0)
    {
        // since we got a response we can assume that zigbee core has been configured and network is up.
        put_string_in_runtimeStatsPojo(output, IS_ZIGBEE_NET_AVAILABLE_KEY, "true");
        put_string_in_runtimeStatsPojo(output, IS_ZIGBEE_NET_CONFIGURED_KEY, "true");

        // store the states and stats
        put_string_in_runtimeStatsPojo(output, IS_ZIGBEE_NETWORK_UP_KEY, zigbeeNetworkStatus.networkIsUp ? "true" : "false");
        put_string_in_runtimeStatsPojo(output, OPEN_FOR_JOIN_KEY, zigbeeNetworkStatus.networkIsOpenForJoin ? "true" : "false");

        char *id = zigbeeSubsystemEui64ToId(zigbeeNetworkStatus.eui64);
        if (id != NULL)
        {
            put_string_in_runtimeStatsPojo(output, EUI_64_KEY, id);
            free(id);
        }
        else
        {
            put_string_in_runtimeStatsPojo(output, EUI_64_KEY, "N/A");
        }

        put_long_in_runtimeStatsPojo(output, CHANNEL_KEY, zigbeeNetworkStatus.channel);
        put_long_in_runtimeStatsPojo(output, PAN_ID_KEY, zigbeeNetworkStatus.panId);
    }
    else
    {
        // since we did not get a response we can assume network is unavailable
        // and we can not confirm any information or assume any state Zigbee core is in
        put_string_in_runtimeStatsPojo(output, IS_ZIGBEE_NET_AVAILABLE_KEY, "false");
    }
}

/**
 * Collect all of the Zigbee device firmware failures/success
 *
 * @param output - the runtimeStatistics hashMap
 */
void collectAllDeviceFirmwareEvents(runtimeStatsPojo *output)
{
    // get the failure events
    //
    icLinkedList *failureEvents = zigbeeEventTrackerCollectFirmwareUpgradeFailureEvents();
    if (failureEvents != NULL)
    {
        // loop through the events
        //
        icLinkedListIterator *listIter = linkedListIteratorCreate(failureEvents);
        while(linkedListIteratorHasNext(listIter))
        {
            deviceUpgFailureItem *item = (deviceUpgFailureItem *) linkedListIteratorGetNext(listIter);
            if (item->deviceId != NULL)
            {
                // create the key
                //
                char *key = stringBuilder("%s%s", DEVICE_FW_UPGRADE_FAILURE_KEY, item->deviceId);
                if (key != NULL)
                {
                    // add failure event info to runtime stats
                    //
                    put_long_in_runtimeStatsPojo(output, key, item->failureTime);

                    // cleanup
                    //
                    free(key);
                }
            }
        }

        // add the upgrade failure count
        //
        int upgradeFailureCount = linkedListCount(failureEvents);
        if (upgradeFailureCount != 0)
        {
            put_int_in_runtimeStatsPojo(output, DEVICE_FW_UPGRADE_FAIL_CNT_KEY, upgradeFailureCount);
        }

        // cleanup
        //
        linkedListIteratorDestroy(listIter);
        linkedListDestroy(failureEvents, destroyDeviceUpgFailureItem);
    }

    // add the upgrade success count
    //
    int upgradeSuccess = zigbeeEventTrackerCollectFirmwareUpgradeSuccessEvents();
    if (upgradeSuccess != 0)
    {
        put_int_in_runtimeStatsPojo(output, DEVICE_FW_UPGRADE_SUCCESS_CNT_KEY, upgradeSuccess);
    }
}

/**
 * Collect zigbee channel status and add them into
 * the runtime stats hash map
 *
 * @param output
 */
void collectChannelScanStats(runtimeStatsPojo *output)
{
    // get the channel scan data stats
    //
    icLinkedList *channelStats = zigbeeEventTrackerCollectChannelEnergyScanStats();
    if (channelStats != NULL)
    {
        // loop through the list
        //
        icLinkedListIterator *listIterator = linkedListIteratorCreate(channelStats);
        while(linkedListIteratorHasNext(listIterator))
        {
            channelEnergyScanDataItem *item = (channelEnergyScanDataItem *) linkedListIteratorGetNext(listIterator);
            if (item != NULL)
            {
                // create the keys
                //
                char *maxKey = stringBuilder("%s%"PRIu8, CHANNEL_SCAN_MAX_KEY, item->channel);
                char *minKey = stringBuilder("%s%"PRIu8, CHANNEL_SCAN_MIN_KEY, item->channel);
                char *avgKey = stringBuilder("%s%"PRIu8, CHANNEL_SCAN_AVG_KEY, item->channel);

                // and add to output
                //
                put_int_in_runtimeStatsPojo(output, maxKey, item->max);
                put_int_in_runtimeStatsPojo(output, minKey, item->min);
                put_int_in_runtimeStatsPojo(output, avgKey, item->average);

                // cleanup
                free(maxKey);
                free(minKey);
                free(avgKey);
            }
        }

        // cleanup
        linkedListIteratorDestroy(listIterator);
        linkedListDestroy(channelStats, NULL);
    }
}

/**
 * Collects stats about Cameras and add them into
 * the runtime stats hash map
 *
 * @param output - the hash map container
 */
void collectCameraDeviceStats(runtimeStatsPojo *output)
{
    // get all cameras on the system
    //
    icLinkedList *cameraList = deviceServiceGetDevicesByDeviceClass(CAMERA_DC);
    if (linkedListCount(cameraList) > 0)
    {
        // create our string buffers
        //
        icStringBuffer *allCameras = stringBufferCreate(INITIAL_CAMERA_BUFFER_SIZE);
        icStringBuffer *connectedCameras = stringBufferCreate(INITIAL_CAMERA_BUFFER_SIZE);
        icStringBuffer *disconnectedCameras = stringBufferCreate(INITIAL_CAMERA_BUFFER_SIZE);

        // loop through the list
        //
        icLinkedListIterator *listIterator = linkedListIteratorCreate(cameraList);
        while (linkedListIteratorHasNext(listIterator) == true)
        {
            icDevice *camera = (icDevice *) linkedListIteratorGetNext(listIterator);
            if (camera == NULL || camera->uuid == NULL)
            {
                icLogError(LOG_TAG, "%s: got an unknown camera", __FUNCTION__);
                continue;
            }

            // init strings
            //
            cameraDeviceInfo cameraInfo;
            initCameraDeviceInfo(&cameraInfo);

            // collect attributes for camera
            //
            collectCameraResources(&cameraInfo, camera);

            // determine what should be done
            //
            if (stringIsEmpty(cameraInfo.macAddress) == false)
            {
                // figure out which bucket camera goes in
                //
                if (stringIsEmpty(cameraInfo.commFail) == false)
                {
                    if (stringToBool(cameraInfo.commFail) == true)
                    {
                        // since camera is in comm failure
                        // add to disconnected bucket
                        //
                        stringBufferAppendWithComma(disconnectedCameras, cameraInfo.macAddress, true);
                    }
                    else
                    {
                        // since camera is not in comm failure
                        // it goes into the connected bucket
                        //
                        stringBufferAppendWithComma(connectedCameras, cameraInfo.macAddress, true);
                    }
                }
                else
                {
                    icLogError(LOG_TAG, "%s: unable to determine Comm Failure for camera %s", __FUNCTION__, camera->uuid);
                }

                // always add to all camera's bucket
                //
                stringBufferAppendWithComma(allCameras, cameraInfo.macAddress, true);
            }
            else
            {
                icLogError(LOG_TAG, "%s: unable to locate MAC Address for camera %s", __FUNCTION__, camera->uuid);
            }

            // TODO: anything else needed?
        }

        // cleanup
        linkedListIteratorDestroy(listIterator);

        // add the all cameras list to stats
        //
        char *value = stringBufferToString(allCameras);
        if (stringIsEmpty(value) == false)
        {
            put_string_in_runtimeStatsPojo(output, CAMERA_TOTAL_DEVICE_LIST_KEY, value);
        }

        // cleanup
        free(value);
        stringBufferDestroy(allCameras);

        // add the connected camera list to stats
        //
        value = stringBufferToString(connectedCameras);
        if (stringIsEmpty(value) == false)
        {
            put_string_in_runtimeStatsPojo(output, CAMERA_CONNECTED_LIST_KEY, value);
        }

        // cleanup
        free(value);
        stringBufferDestroy(connectedCameras);

        // add the disconnected camera list to stats
        //
        value = stringBufferToString(disconnectedCameras);
        if (stringIsEmpty(value) == false)
        {
            put_string_in_runtimeStatsPojo(output, CAMERA_DISCONNECTED_LIST_KEY, value);
        }

        // cleanup
        free(value);
        stringBufferDestroy(disconnectedCameras);
    }

    // cleanup
    linkedListDestroy(cameraList, (linkedListItemFreeFunc) deviceDestroy);
}

/**
 * Collect the device service status
 * TODO: determine what to gather
 *
 * @param output - the serviceStatusHasMap
 */
void collectAllDeviceStatus(serviceStatusPojo *output)
{
    // TODO: finish me
}

/**
 *  Helper function for handling the logic or adding
 *  a value to output if it has not been created yet.
 *  And determine if input is NULL or the special char
 *  NULL.
 *
 * @param input - the value to be copied
 * @return - either the input value or a custom NULL value
 */
static const char *customStringToString(const char *src)
{
    const char *dest;

    if ((src == NULL) || strcmp(src, DEVICE_NULL_VALUE) == 0)
    {
        dest = VALUE_IS_EMPTY_STRING;
    }
    else
    {
        dest = src;
    }

    return dest;
}

/**
 * Helper function to init basicDeviceInfo struct
 * by making all of the char points set to our
 * custom NULL char which is ""
 *
 * @param statList - the list of stats
 */
static void initDeviceInfo(basicDeviceInfo *deviceInfo)
{
    memset(deviceInfo, 0, sizeof(basicDeviceInfo));

    deviceInfo->manufacturer = customStringToString(NULL);
    deviceInfo->model = customStringToString(NULL);
    deviceInfo->firmVer = customStringToString(NULL);
    deviceInfo->hardVer = customStringToString(NULL);
    deviceInfo->farLqi = customStringToString(NULL);
    deviceInfo->nearLqi = customStringToString(NULL);
    deviceInfo->nearRssi = customStringToString(NULL);
    deviceInfo->farRssi = customStringToString(NULL);
    deviceInfo->temp = customStringToString(NULL);
    deviceInfo->batteryVolts = customStringToString(NULL);
    deviceInfo->lowBattery = customStringToString(NULL);
    deviceInfo->commFail = customStringToString(NULL);
    deviceInfo->troubled = customStringToString(NULL);
    deviceInfo->bypassed = customStringToString(NULL);
    deviceInfo->tampered = customStringToString(NULL);
    deviceInfo->faulted = customStringToString(NULL);
}

/**
 * Helper function to init cameraDeviceInfo struct
 * by making all of the char points set to our
 * custom NULL char which is ""
 *
 * @param deviceInfo - the list of stats
 */
static void initCameraDeviceInfo(cameraDeviceInfo *deviceInfo)
{
    deviceInfo->macAddress = customStringToString(NULL);
    deviceInfo->commFail = customStringToString(NULL);
}

/**
 * Helper function to init attributeDeviceInfo struct
 * by making all of the char points set to our
 * custom NULL char which is ""
 *
 * @param attDeviceInfo - the list of attribute values
 * @param size - the number of attribute reports
 */
static void initAttDeviceInfo(attributeDeviceInfo *attDeviceInfo, int size)
{
    memset(attDeviceInfo, 0, size * sizeof(attributeDeviceInfo));

    for(int i = 0; i < size; i++)
    {
        attDeviceInfo[i].reportTime = customStringToString(NULL);
        attDeviceInfo[i].clusterId = customStringToString(NULL);
        attDeviceInfo[i].attributeId = customStringToString(NULL);
        attDeviceInfo[i].data = customStringToString(NULL);
    }
}

/**
 * Helper function to init rejoinDeviceInfo struct
 * by making all of the char points set to our
 * custom NULL char which is ""
 *
 * @param rejoinInfo - the list of rejoin values
 * @param size - the number of rejoins
 */
static void initRejoinDeviceInfo(rejoinDeviceInfo *rejoinInfo, int size)
{
    memset(rejoinInfo, 0, size * sizeof(rejoinDeviceInfo));

    for(int i = 0; i < size; i++)
    {
        rejoinInfo[i].time = customStringToString(NULL);
        rejoinInfo[i].isSecure = customStringToString(NULL);
    }
}

/**
 * Helper function to create checkInInfo struct
 * by making all elements in list point to our
 * the custom NULL char which is ""
 *
 * @param rejoinInfo - the list of check-in values
 * @param size - the number of check ins
 */
static void initCheckInDeviceInfo(checkInDeviceInfo *checkInInfo, int size)
{
    memset(checkInInfo, 0, size * sizeof(checkInDeviceInfo));

    for (int i = 0; i < size; i++)
    {
        checkInInfo[i].time = customStringToString(NULL);
    }
}

/**
 * Helper function to sort through the resource list
 * and find the ones we want to gather
 *
 * @param statList - out stats collected list
 * @param device - the physical device
 */
static void collectResources(basicDeviceInfo *deviceInfo, icDevice *device)
{
    if (device->resources == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to use resource list for device %s",__FUNCTION__, device->uuid);
        return;
    }

    // ----------------------------------------------------------------
    // loop though the resource ids
    // ----------------------------------------------------------------

    icLinkedListIterator *resourceIter = linkedListIteratorCreate(device->resources);
    while(linkedListIteratorHasNext(resourceIter))
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(resourceIter);
        if (resource->id == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to use resource %s", __FUNCTION__, resource->uri);
            continue;
        }

        // ----------------------------------------------------------------
        // now compare all of the resource and find the ones we want
        // ----------------------------------------------------------------

        // the resources to gather
        //
        if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_MANUFACTURER) == 0)
        {
            deviceInfo->manufacturer = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_MODEL) == 0)
        {
            deviceInfo->model = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION) == 0)
        {
            deviceInfo->firmVer = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION) == 0)
        {
            deviceInfo->hardVer = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_NERSSI) == 0)
        {
            deviceInfo->nearLqi = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_FERSSI) == 0)
        {
            deviceInfo->farLqi = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_NELQI) == 0)
        {
            deviceInfo->nearRssi = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_FELQI) == 0)
        {
            deviceInfo->farRssi = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_TEMPERATURE) == 0)
        {
            deviceInfo->temp = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE) == 0)
        {
            deviceInfo->batteryVolts = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_BATTERY_LOW) == 0)
        {
            deviceInfo->lowBattery = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_COMM_FAIL) == 0)
        {
            deviceInfo->commFail = customStringToString(resource->value);
        }
    }

    // ----------------------------------------------------------------
    // cleanup resource loop
    // ----------------------------------------------------------------

    linkedListIteratorDestroy(resourceIter);
}

/**
 * Helper function to find and gather the resources in the endpoints
 *
 * @param statList - our stats collected list
 * @param device - the physical device
 */
static void collectEndpointResources(basicDeviceInfo *deviceInfo, icDevice *device)
{
    // ----------------------------------------------------------------
    // loop though the endpoint ids
    // ----------------------------------------------------------------

    icLinkedListIterator *endpointIter = linkedListIteratorCreate(device->endpoints);
    while(linkedListIteratorHasNext(endpointIter))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(endpointIter);
        if (endpoint == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to find endpoint for device %s", __FUNCTION__, device->uuid);
            continue;
        }

        // ----------------------------------------------------------------
        // loop though the resource ids
        // ----------------------------------------------------------------

        icLinkedListIterator *endpointResourceIter = linkedListIteratorCreate(endpoint->resources);
        while (linkedListIteratorHasNext(endpointResourceIter))
        {
            icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(endpointResourceIter);
            if (resource == NULL)
            {
                icLogError(LOG_TAG, "%s: unable to find resource for device %s on endpoint %s", __FUNCTION__, device->uuid, endpoint->uri);
                continue;
            }

            // ----------------------------------------------------------------
            // now compare all of the resource and find the ones we want
            // ----------------------------------------------------------------

            // the endpoint resources to gather
            //
            if (strcmp(resource->id, COMMON_ENDPOINT_RESOURCE_TROUBLE) == 0)
            {
                deviceInfo->troubled = customStringToString(resource->value);
            }
            else if (strcmp(resource->id, SENSOR_PROFILE_RESOURCE_BYPASSED) == 0)
            {
                deviceInfo->bypassed = customStringToString(resource->value);
            }
            else if (strcmp(resource->id, COMMON_ENDPOINT_RESOURCE_TAMPERED) == 0)
            {
                deviceInfo->tampered = customStringToString(resource->value);
            }
            else if (strcmp(resource->id, SENSOR_PROFILE_RESOURCE_FAULTED) == 0)
            {
                deviceInfo->faulted = customStringToString(resource->value);
            }
        }

        // ----------------------------------------------------------------
        // cleanup endpoint resource loop
        // ----------------------------------------------------------------

        linkedListIteratorDestroy(endpointResourceIter);
    }

    // ----------------------------------------------------------------
    // cleanup endpoint loop
    // ----------------------------------------------------------------

    linkedListIteratorDestroy(endpointIter);
}

/**
 * Helper function to sort through the resource list
 * for Cameras and find the ones we want to gather
 *
 * @param deviceInfo - our stats to collect
 * @param device - the physical device
 */
static void collectCameraResources(cameraDeviceInfo *deviceInfo, icDevice *device)
{
    if (device->resources == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to use resource list for device %s",__FUNCTION__, device->uuid);
        return;
    }

    // ----------------------------------------------------------------
    // loop though the resource ids
    // ----------------------------------------------------------------

    icLinkedListIterator *resourceIter = linkedListIteratorCreate(device->resources);
    while(linkedListIteratorHasNext(resourceIter))
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(resourceIter);
        if (resource->id == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to use resource %s", __FUNCTION__, resource->uri);
            continue;
        }

        // ----------------------------------------------------------------
        // now compare all of the resource and find the ones we want
        // ----------------------------------------------------------------

        // the resources to gather
        //
        if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_MAC_ADDRESS) == 0)
        {
            deviceInfo->macAddress = customStringToString(resource->value);
        }
        else if (strcmp(resource->id, COMMON_DEVICE_RESOURCE_COMM_FAIL) == 0)
        {
            deviceInfo->commFail = customStringToString(resource->value);
        }

        // TODO: collect more resources??
    }

    // ----------------------------------------------------------------
    // cleanup resource loop
    // ----------------------------------------------------------------

    linkedListIteratorDestroy(resourceIter);
}

/**
 * Helper function to collect the values
 * found in the attribute report list if
 * its not NULL and stores the values in
 * device Info
 *
 * @param attList - the list to go through
 * @param deviceInfo - the list of attributeDeviceInfo to hold the values
 */
static void convertAttributeReports(icLinkedList *attList, attributeDeviceInfo *deviceInfo)
{
    // only do this if attribute list and device info list exist
    if (attList != NULL && deviceInfo !=NULL)
    {
        // ----------------------------------------------------------------
        // loop though the attribute report list
        // ----------------------------------------------------------------

        int counter = 0;
        icLinkedListIterator *listIter = linkedListIteratorCreate(attList);
        while(linkedListIteratorHasNext(listIter))
        {
            deviceAttributeItem *currentItem = (deviceAttributeItem *) linkedListIteratorGetNext(listIter);
            if (currentItem == NULL)
            {
                icLogWarn(LOG_TAG, "%s: unable to find attribute item in attribute list", __FUNCTION__);
                continue;
            }

            // ----------------------------------------------------------------
            // now add values based on the counter
            // ----------------------------------------------------------------

            if (counter < MAX_NUMBER_OF_ATTRIBUTE_REPORTS)
            {
                deviceInfo[counter].reportTime = customStringToString(currentItem->reportTime);
                deviceInfo[counter].clusterId = customStringToString(currentItem->clusterId);
                deviceInfo[counter].attributeId = customStringToString(currentItem->attributeId);
                deviceInfo[counter].data = customStringToString(currentItem->data);

                // increase counter
                counter ++;
            }
            else
            {
                break;  // no point in continuing the loop
            }
        }

        // ----------------------------------------------------------------
        // clean up loop
        // ----------------------------------------------------------------

        linkedListIteratorDestroy(listIter);
    }
}

/**
 * Helper function to collect the values
 * found in the rejoin list if its not NULL
 * and stores the values in device Info
 *
 * @param rejoinList - the list to go though
 * @param deviceInfo - the list of rejoinDeviceInfo to hold the values
 */
static void convertRejoins(icLinkedList *rejoinList, rejoinDeviceInfo *deviceInfo)
{
    // only do this if rejoin list and device info list exist
    if (rejoinList != NULL && deviceInfo != NULL)
    {
        // ----------------------------------------------------------------
        // loop though the rejoin check-in event list
        // ----------------------------------------------------------------

        int counter = 0;
        icLinkedListIterator *listIter = linkedListIteratorCreate(rejoinList);
        while (linkedListIteratorHasNext(listIter))
        {
            deviceRejoinItem *currItem = (deviceRejoinItem *) linkedListIteratorGetNext(listIter);
            if (currItem == NULL)
            {
                icLogWarn(LOG_TAG, "%s: unable to find rejoin item in rejoin event list", __FUNCTION__);
                continue;
            }

            // ----------------------------------------------------------------
            // now add values based on the counter
            // ----------------------------------------------------------------

            if (counter < MAX_NUMBER_OF_REJOINS)
            {
                deviceInfo[counter].time = customStringToString(currItem->rejoinTime);
                deviceInfo[counter].isSecure = customStringToString(currItem->isSecure);

                // increase counter
                counter ++;
            }
            else
            {
                break;    // no point in continuing the loop
            }
        }

        // ----------------------------------------------------------------
        // clean up
        // ----------------------------------------------------------------

        linkedListIteratorDestroy(listIter);
    }
}

/**
 * Helper function to collect the values
 * found in the check-in list if its not NULL
 * and stores the values in device Info
 *
 * @param checkInList - the list to go though
 * @param deviceInfo - the list of check-ins to hold the values
 */
static void convertCheckIns(icLinkedList *checkInList, checkInDeviceInfo *deviceInfo)
{
    // only do this if check-in list and device info list exist
    if (checkInList != NULL && deviceInfo != NULL)
    {
        // ----------------------------------------------------------------
        // loop though the rejoin check-in event list
        // ----------------------------------------------------------------

        int counter = 0;
        icLinkedListIterator *listIter = linkedListIteratorCreate(checkInList);
        while (linkedListIteratorHasNext(listIter))
        {
            char *currItem = (char *) linkedListIteratorGetNext(listIter);

            // ----------------------------------------------------------------
            // now add values based on the counter
            // ----------------------------------------------------------------

            if (counter < MAX_NUMBER_OF_CHECK_INS)
            {
                deviceInfo[counter].time = customStringToString(currItem);

                // increase counter
                counter ++;
            }
            else
            {
                break;    // no point in continuing the loop
            }
        }

        // ----------------------------------------------------------------
        // clean up
        // ----------------------------------------------------------------

        linkedListIteratorDestroy(listIter);
    }
}

/**
 * Helper function to create the string list
 * for all of the device's information and events
 *
 * NOTE: each device list has to be in the format:
 *
 * "uuid,manufacturer,model,firmwareVersion,
 * AttributeReportTime1,ClusterId1,AttributeId1,Data1,
 * AttributeReportTime2,ClusterId2,AttributeId2,Data2,
 * AttributeReportTime3,ClusterId3,AttributeId3,Data3,
 * AttributeReportTime4,ClusterId4,AttributeId4,Data4,
 * AttributeReportTime5,ClusterId5,AttributeId5,Data5,
 * AttributeReportTime6,ClusterId6,AttributeId6,Data6,
 * AttributeReportTime7,ClusterId7,AttributeId7,Data7,
 * AttributeReportTime8,ClusterId8,AttributeId8,Data8,
 * rejoinTime1,isSecure1,
 * rejoinTime2,isSecure2,
 * rejoinTime3,isSecure3,
 * rejoinTime4,isSecure4,
 * rejoinTime5,isSecure5,
 * checkInTime1,
 * checkInTime2,
 * checkInTime3,
 * checkInTime4,
 * checkInTime5,
 * type,hardwareVersion,lqi(ne/fe),
 * rssi(ne/fe),temperature,batteryVoltage,
 * lowBattery,commFailure,troubled,
 * bypassed,tampered,faulted,
 * totalRejoinCounter,
 * totalSecureRejoinCounter,
 * totalUnSecureRejoinCounter,
 * totalDuplicateSequenceNumberCounter,
 * totalApsAckFailureCounter"
 *
 * NOTE: caller must free return object if not null
 *
 * @param uuid - the deivce id
 * @param deviceInfo - all of the basic resource and enpoint information
 * @param attList - the attribute reports list
 * @param rejoinList - the rejoin event list
 * @param checkInList - the check-in event list
 * @param deviceCounters - the device counters for device
 * @param deviceClass - the device type
 * @return - a massive string list containing all of the information above or NULL
 */
static char *createDeviceStringList(const char *uuid, basicDeviceInfo *deviceInfo, attributeDeviceInfo *attList,
                                    rejoinDeviceInfo *rejoinList, checkInDeviceInfo *checkInList,
                                    deviceEventCounterItem *deviceCounters, const char *deviceClass)
{
    // sanity check
    //
    if (uuid == NULL || deviceInfo == NULL || attList == NULL || rejoinList == NULL
    || checkInList == NULL || deviceCounters == NULL || deviceClass == NULL)
    {
        return NULL;
    }

    char *listOfValues = stringBuilder(
            "%s,%s,%s,%s,"
            "%s,%s,%s,%s,"
            "%s,%s,%s,%s,"
            "%s,%s,%s,%s,"
            "%s,%s,%s,%s,"
            "%s,%s,%s,%s,"
            "%s,%s,%s,%s,"
            "%s,%s,%s,%s,"
            "%s,%s,%s,%s,"
            "%s,%s,"
            "%s,%s,"
            "%s,%s,"
            "%s,%s,"
            "%s,%s,"
            "%s,"
            "%s,"
            "%s,"
            "%s,"
            "%s,"
            "%s,%s,%s/%s,"
            "%s/%s,%s,%s,"
            "%s,%s,%s,"
            "%s,%s,%s,"
            "%d,"
            "%d,"
            "%d,"
            "%d,"
            "%d",
            uuid, deviceInfo->manufacturer, deviceInfo->model, deviceInfo->firmVer,
            attList[0].reportTime, attList[0].clusterId, attList[0].attributeId, attList[0].data,
            attList[1].reportTime, attList[1].clusterId, attList[1].attributeId, attList[1].data,
            attList[2].reportTime, attList[2].clusterId, attList[2].attributeId, attList[2].data,
            attList[3].reportTime, attList[3].clusterId, attList[3].attributeId, attList[3].data,
            attList[4].reportTime, attList[4].clusterId, attList[4].attributeId, attList[4].data,
            attList[5].reportTime, attList[5].clusterId, attList[5].attributeId, attList[5].data,
            attList[6].reportTime, attList[6].clusterId, attList[6].attributeId, attList[6].data,
            attList[7].reportTime, attList[7].clusterId, attList[7].attributeId, attList[7].data,
            rejoinList[0].time, rejoinList[0].isSecure,
            rejoinList[1].time, rejoinList[1].isSecure,
            rejoinList[2].time, rejoinList[2].isSecure,
            rejoinList[3].time, rejoinList[3].isSecure,
            rejoinList[4].time, rejoinList[4].isSecure,
            checkInList[0].time,
            checkInList[1].time,
            checkInList[2].time,
            checkInList[3].time,
            checkInList[4].time,
            deviceClass, deviceInfo->hardVer, deviceInfo->nearLqi, deviceInfo->farLqi,
            deviceInfo->nearRssi, deviceInfo->farRssi, deviceInfo->temp, deviceInfo->batteryVolts,
            deviceInfo->lowBattery, deviceInfo->commFail, deviceInfo->troubled,
            deviceInfo->bypassed, deviceInfo->tampered, deviceInfo->faulted,
            deviceCounters->totalRejoinEvents,
            deviceCounters->totalSecureRejoinEvents,
            deviceCounters->totalUnSecureRejoinEvents,
            deviceCounters->totalDuplicateSeqNumEvents,
            deviceCounters->totalDuplicateSeqNumEvents
            );

    return listOfValues;
}

#endif // CONFIG_SERVICE_DEVICE_ZIGBEE
