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
// Created by tlea on 2/4/19.
//

#include <deviceService.h>
#include <deviceServicePrivate.h>
#include <icLog/logging.h>
#include <inttypes.h>
#include <string.h>
#include <pthread.h>
#include <deviceModelHelper.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include "deviceNumberAllocator.h"

#define LOG_TAG "deviceService"
#define MAX_LEGACY_DEVICE_NUM 64
#define DEVICE_NUMBER_METADATA "legacyDevNum"

static pthread_mutex_t allocatorMtx = PTHREAD_MUTEX_INITIALIZER;

//stores device numbers that have been allocated 'recently'.  This covers the situation whereby a device
// number was allocated, but not yet persisted to the database since the device is still undergoing
// discovery.
//TODO find a good way to clear this without a restart
static uint64_t tempDevNums = 0;

uint8_t allocateDeviceNumber(icDevice *device)
{
    pthread_mutex_lock(&allocatorMtx);

    uint8_t result = 1;
    uint64_t allocated = 0;

    // Loop through all legacy devices in the system and track which device numbers are in use
    icLinkedList *devices = deviceServiceGetDevicesByMetadata(DEVICE_NUMBER_METADATA, NULL);

    if (devices != NULL)
    {
        icLinkedListIterator *it = linkedListIteratorCreate(devices);
        while (linkedListIteratorHasNext(it))
        {
            icDevice *tmp = linkedListIteratorGetNext(it);

            uint8_t devNum = getDeviceNumberForDevice(tmp->uuid);
            if(devNum > 0)
            {
                icLogDebug(LOG_TAG, "%s: device %s has devNum %u", __FUNCTION__, tmp->uuid, devNum);
                allocated |= 0x1ULL << devNum;
            }
        }
        linkedListIteratorDestroy(it);
    }

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    //minor cleanup to the tempDevNums: if it was found in the allocated bitfield, clear it out
    // from the tempDevNums so we can support pairing then deleting and pairing again reuse.
    tempDevNums &= ~allocated;

    //scan through the bitfield to find an available slot
    while (((allocated & (0x1ULL << result)) > 0 || (tempDevNums & (0x1ULL << result)) > 0) &&
            result <= MAX_LEGACY_DEVICE_NUM)
    {
        result++;
    }

    if (result >= MAX_LEGACY_DEVICE_NUM)
    {
        icLogError(LOG_TAG, "%s: all device numbers have been allocated!", __FUNCTION__);
        result = 0;
    }

    if(setDeviceNumberForDevice(device, result) == false)
    {
        result = 0;
    }

    //save off this dev number in our list of temporary ones
    tempDevNums |= 0x1ULL << result;

    pthread_mutex_unlock(&allocatorMtx);

    icLogDebug(LOG_TAG, "%s: allocated=%"PRIx64", tempDevNums=%"PRIx64", devNum=%d", __FUNCTION__, allocated, tempDevNums, result);

    return result;
}

uint8_t getDeviceNumberForDevice(const char *uuid)
{
    uint8_t result = 0;

    char *uri = getMetadataUri(uuid, NULL, DEVICE_NUMBER_METADATA);
    if(uri != NULL)
    {
        char *devNumStr = NULL;
        if(deviceServiceGetMetadata(uri, &devNumStr) && devNumStr != NULL)
        {
            unsigned long devNum = strtoul(devNumStr, NULL, 10);
            if(devNum == 0 || devNum >= MAX_LEGACY_DEVICE_NUM)
            {
                icLogError(LOG_TAG, "%s: invalid device number read from %s (value = %lu)", __FUNCTION__, uri, devNum);
            }
            else
            {
                result = (uint8_t) devNum;
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: failed to read device number metadata (%s)!", __FUNCTION__, uri);
        }

        free(devNumStr);
        free(uri);
    }

    return result;
}

bool getEui64ForDeviceNumber(uint8_t devNum, uint64_t *eui64)
{
    bool result = false;

    if(eui64 == NULL)
    {
        return false;
    }

    *eui64 = 0;

    if(devNum == 0)
    {
        //this is 'us'
        result = true;
    }
    else
    {
        char devNumStr[4]; //255 \0 worst case
        snprintf(devNumStr, 4, "%"PRIu8, devNum);
        icLinkedList *devices = deviceServiceGetDevicesByMetadata(DEVICE_NUMBER_METADATA, devNumStr);
        if (devices != NULL)
        {
            if (linkedListCount(devices) != 1)
            {
                icLogError(LOG_TAG, "%s: zero or more than one device found matching metadata "
                        DEVICE_NUMBER_METADATA
                        " with value %s!", __FUNCTION__, devNumStr);
            }
            else
            {
                icDevice *device = linkedListGetElementAt(devices, 0);
                *eui64 = zigbeeSubsystemIdToEui64(device->uuid);
                result = true;
            }

            linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
        }
    }

    return result;
}

void clearTemporaryDeviceNumbers()
{
    pthread_mutex_lock(&allocatorMtx);
    tempDevNums = 0;
    pthread_mutex_unlock(&allocatorMtx);
}

bool setDeviceNumberForDevice(icDevice *device, uint8_t deviceNumber)
{
    bool result = true;

    if(device == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments (NULL device)", __FUNCTION__);
        return false;
    }

    if(deviceNumber == 0 || deviceNumber >= MAX_LEGACY_DEVICE_NUM)
    {
        icLogError(LOG_TAG, "%s: invalid arguments (deviceNumber=%u)", __FUNCTION__, deviceNumber);
        return false;
    }

    char devNumStr[3]; //xx + \0
    snprintf(devNumStr, 3, "%u", deviceNumber);
    if(createDeviceMetadata(device, DEVICE_NUMBER_METADATA, devNumStr) == NULL)
    {
        icLogError(LOG_TAG, "%s: failed to save device number to metadata", __FUNCTION__);
        result = false;
    }

    return result;
}