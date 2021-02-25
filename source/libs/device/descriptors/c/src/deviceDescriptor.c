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
// Created by Thomas Lea on 7/31/15.
//

#include <deviceDescriptor.h>
#include <icLog/logging.h>
#include <stddef.h>
#include <icTypes/icLinkedList.h>
#include <stdlib.h>
#include <icTypes/icStringHashMap.h>
#include <string.h>

#define LOG_TAG "deviceDescriptor"

extern inline void deviceDescriptorFree__auto(DeviceDescriptor **dd);

static void freeDeviceVersionList(DeviceVersionList *list)
{
    if(list != NULL)
    {
        free(list->format);

        switch(list->listType)
        {
            case DEVICE_VERSION_LIST_TYPE_LIST:
                linkedListDestroy(list->list.versionList, NULL);
                break;

            case DEVICE_VERSION_LIST_TYPE_RANGE:
                free(list->list.versionRange.from);
                free(list->list.versionRange.to);
                break;

                //nothing to free for these
            case DEVICE_VERSION_LIST_TYPE_UNKNOWN:break;
            case DEVICE_VERSION_LIST_TYPE_WILDCARD:break;
        }

        free(list);
    }
}

static char * nullSafeStrdup(const char *str)
{
    if (str != NULL)
    {
        return strdup(str);
    }

    return NULL;
}

static void *cloneString(void *item, void *context)
{
    (void)(context); // prevent unused parameter warning
    return nullSafeStrdup((char *) item);
}

static DeviceVersionList *cloneDeviceVersionList(DeviceVersionList *list)
{
    DeviceVersionList *newList = NULL;
    if (list != NULL)
    {
        newList = (DeviceVersionList *)calloc(1, sizeof(DeviceVersionList));
        newList->format = nullSafeStrdup(list->format);
        newList->listType = list->listType;
        switch(list->listType)
        {
            case DEVICE_VERSION_LIST_TYPE_LIST:
                newList->list.versionList = linkedListDeepClone(list->list.versionList, cloneString, NULL);
                break;

            case DEVICE_VERSION_LIST_TYPE_RANGE:
                newList->list.versionRange.from = nullSafeStrdup(list->list.versionRange.from);
                newList->list.versionRange.to = nullSafeStrdup(list->list.versionRange.to);
                break;

                //nothing to clone for these
            case DEVICE_VERSION_LIST_TYPE_UNKNOWN:break;
            case DEVICE_VERSION_LIST_TYPE_WILDCARD:break;
        }
    }

    return newList;
}

static DeviceFirmware *cloneDeviceFirmware(DeviceFirmware *deviceFirmware)
{
    DeviceFirmware *newDeviceFirmware = NULL;
    if (deviceFirmware != NULL)
    {
        newDeviceFirmware = (DeviceFirmware *)calloc(1, sizeof(DeviceFirmware));
        newDeviceFirmware->type = deviceFirmware->type;
        newDeviceFirmware->version = strdup(deviceFirmware->version);
    }

    return newDeviceFirmware;
}

/*
 * Free any memory allocated for this device descriptor.  Caller may still need to free the DeviceDescriptor
 * pointer if it is in heap.
 */
void deviceDescriptorFree(DeviceDescriptor *dd)
{
    if(dd != NULL)
    {
        free(dd->uuid);
        free(dd->description);
        free(dd->manufacturer);
        free(dd->model);
        freeDeviceVersionList(dd->hardwareVersions);
        freeDeviceVersionList(dd->firmwareVersions);
        free(dd->minSupportedFirmwareVersion);
        stringHashMapDestroy(dd->metadata, NULL);
        if(dd->latestFirmware != NULL)
        {
            free(dd->latestFirmware->version);
            if (dd->latestFirmware->filenames != NULL)
            {
                linkedListDestroy(dd->latestFirmware->filenames, NULL);
            }
            free(dd->latestFirmware->checksum);
            free(dd->latestFirmware);
        }

        free(dd);
    }
}

static void printVersionList(const char *prefix, DeviceVersionList *versionList)
{
    switch(versionList->listType)
    {
        case DEVICE_VERSION_LIST_TYPE_LIST:
        {
            icLogInfo(LOG_TAG, "%s: (format=%s) version list:", prefix, versionList->format);
            icLinkedListIterator *it = linkedListIteratorCreate(versionList->list.versionList);
            while (linkedListIteratorHasNext(it) == true)
            {
                char *next = (char *) linkedListIteratorGetNext(it);
                icLogInfo(LOG_TAG, "\t\t%s", next);
            }
            linkedListIteratorDestroy(it);
        }
            break;

        case DEVICE_VERSION_LIST_TYPE_WILDCARD:
            icLogInfo(LOG_TAG, "%s: any", prefix);
            break;

        case DEVICE_VERSION_LIST_TYPE_RANGE:
            icLogInfo(LOG_TAG, "%s: from=%s, to=%s",
                      prefix,
                      versionList->list.versionRange.from,
                      versionList->list.versionRange.to);
            break;

        default:
            icLogInfo(LOG_TAG, "%s: unsupported version list type", prefix);
            break;
    }
}

/*
 * Display a device descriptor to the info log.  arg is provided for compatibility with the interator callback func
 * and is ignored.
 */
bool deviceDescriptorPrint(DeviceDescriptor *dd, void *arg)
{
    if(dd == NULL)
    {
        icLogInfo(LOG_TAG, "NULL DeviceDescriptor");
        return true;
    }

    switch(dd->deviceDescriptorType)
    {
        case DEVICE_DESCRIPTOR_TYPE_CAMERA:
            icLogInfo(LOG_TAG, "DeviceDescriptor (Camera)");
            break;

        case DEVICE_DESCRIPTOR_TYPE_ZIGBEE:
            icLogInfo(LOG_TAG, "DeviceDescriptor (ZigBee)");
            break;

        case DEVICE_DESCRIPTOR_TYPE_LEGACY_ZIGBEE:
            icLogInfo(LOG_TAG, "DeviceDescriptor (Legacy ZigBee)");
            break;

        default:
            icLogInfo(LOG_TAG, "Unsupported DeviceDescriptor type (%d)!", dd->deviceDescriptorType);
            return true;
    }

    icLogInfo(LOG_TAG, "\tuuid: %s", dd->uuid);
    icLogInfo(LOG_TAG, "\tdescription: %s", dd->description);
    icLogInfo(LOG_TAG, "\tmanufacturer: %s", dd->manufacturer);
    icLogInfo(LOG_TAG, "\tmodel: %s", dd->model);
    printVersionList("\thardwareVersions", dd->hardwareVersions);
    printVersionList("\tfirmwareVersions", dd->firmwareVersions);
    icLogInfo(LOG_TAG, "\tminSupportedFirmwareVersion: %s", dd->minSupportedFirmwareVersion);

    if(dd->metadata != NULL)
    {
        icLogInfo(LOG_TAG, "\tmetadata:");
        icStringHashMapIterator *it = stringHashMapIteratorCreate(dd->metadata);
        while(stringHashMapIteratorHasNext(it))
        {
            char *name;
            char *value;

            stringHashMapIteratorGetNext(it, &name, &value);
            icLogInfo(LOG_TAG, "\t\t%s = %s", name, value);
        }

        stringHashMapIteratorDestroy(it);
    }

    if (dd->latestFirmware != NULL)
    {
        icLogInfo(LOG_TAG, "\tlatestFirmware: version %s, checksum %s, filenames:", dd->latestFirmware->version,
                  dd->latestFirmware->checksum);
        if (dd->latestFirmware->filenames != NULL)
        {
            icLinkedListIterator *it = linkedListIteratorCreate(dd->latestFirmware->filenames);
            while (linkedListIteratorHasNext(it) == true)
            {
                char *next = (char *)linkedListIteratorGetNext(it);
                icLogInfo(LOG_TAG, "\t\t%s", next);
            }
            linkedListIteratorDestroy(it);
        }
    }

    if(dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
    {
        CameraDeviceDescriptor *cdd = (CameraDeviceDescriptor*)dd;
        switch(cdd->protocol)
        {
            case CAMERA_PROTOCOL_LEGACY:
                icLogInfo(LOG_TAG, "\tprotocol: legacy");
                break;

            case CAMERA_PROTOCOL_OPENHOME:
                icLogInfo(LOG_TAG, "\tprotocol: openHome");
                break;

            default:
                icLogInfo(LOG_TAG, "\tprotocol: UNKNOWN!");
                break;
        }

        if(cdd->defaultMotionSettings.enabled == true)
        {
            icLogInfo(LOG_TAG, "\tmotion enabled:");
            icLogInfo(LOG_TAG, "\t\tsensitivity (low,medium,high): %d,%d,%d",
                      cdd->defaultMotionSettings.sensitivity.low,
                      cdd->defaultMotionSettings.sensitivity.medium,
                      cdd->defaultMotionSettings.sensitivity.high);

            icLogInfo(LOG_TAG, "\t\tdetectionThreshold (low,medium,high): %d,%d,%d",
                      cdd->defaultMotionSettings.detectionThreshold.low,
                      cdd->defaultMotionSettings.detectionThreshold.medium,
                      cdd->defaultMotionSettings.detectionThreshold.high);

            icLogInfo(LOG_TAG, "\t\tregionOfInterest (width, height, bottom, top, left, right): %d,%d,%d,%d,%d,%d",
                      cdd->defaultMotionSettings.regionOfInterest.width,
                      cdd->defaultMotionSettings.regionOfInterest.height,
                      cdd->defaultMotionSettings.regionOfInterest.bottom,
                      cdd->defaultMotionSettings.regionOfInterest.top,
                      cdd->defaultMotionSettings.regionOfInterest.left,
                      cdd->defaultMotionSettings.regionOfInterest.right);
        }
        else
        {
            icLogInfo(LOG_TAG, "\tmotion disabled");
        }
    }

    return true;
}


/*
 * Clone a device descriptor.
 */
DeviceDescriptor *deviceDescriptorClone(const DeviceDescriptor *dd)
{
    DeviceDescriptor *newDD = NULL;
    if(dd != NULL)
    {
        // Create it of the right size based on the type
        if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
        {
            newDD = (DeviceDescriptor *) calloc(1, sizeof(CameraDeviceDescriptor));
        }
        else
        {
            newDD = (DeviceDescriptor *) calloc(1, sizeof(ZigbeeDeviceDescriptor));
        }
        newDD->uuid = nullSafeStrdup(dd->uuid);
        newDD->deviceDescriptorType = dd->deviceDescriptorType;
        newDD->description = nullSafeStrdup(dd->description);
        newDD->manufacturer = nullSafeStrdup(dd->manufacturer);
        newDD->model = nullSafeStrdup(dd->model);
        // Deep clone device version list
        newDD->hardwareVersions = cloneDeviceVersionList(dd->hardwareVersions);
        newDD->firmwareVersions = cloneDeviceVersionList(dd->firmwareVersions);
        newDD->minSupportedFirmwareVersion = nullSafeStrdup(dd->minSupportedFirmwareVersion);
        if (dd->metadata != NULL)
        {
            newDD->metadata = stringHashMapDeepClone(dd->metadata);
        }
        if(dd->latestFirmware != NULL)
        {
            // Deep clone latestFirmware
            newDD->latestFirmware = (DeviceFirmware *)calloc(1, sizeof(DeviceFirmware));
            newDD->latestFirmware->version = nullSafeStrdup(dd->latestFirmware->version);
            if (dd->latestFirmware->filenames != NULL)
            {
                newDD->latestFirmware->filenames = linkedListDeepClone(dd->latestFirmware->filenames, cloneString, NULL);
            }
            newDD->latestFirmware->checksum = nullSafeStrdup(dd->latestFirmware->checksum);
            newDD->latestFirmware->type = dd->latestFirmware->type;
        }

        // Make sure to do the extra camera bits if necessary
        if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
        {
            CameraDeviceDescriptor *cameraDD = (CameraDeviceDescriptor *)dd;
            CameraDeviceDescriptor *newCameraDD = (CameraDeviceDescriptor *)newDD;
            newCameraDD->protocol = cameraDD->protocol;
            newCameraDD->defaultMotionSettings = cameraDD->defaultMotionSettings;
        }
    }

    return newDD;
}