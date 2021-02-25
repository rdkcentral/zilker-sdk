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

#ifndef ZILKER_DEVICEDESCRIPTOR_H
#define ZILKER_DEVICEDESCRIPTOR_H

#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>

typedef enum
{
    DEVICE_DESCRIPTOR_TYPE_UNKNOWN,
    DEVICE_DESCRIPTOR_TYPE_LEGACY_ZIGBEE,
    DEVICE_DESCRIPTOR_TYPE_ZIGBEE,
    DEVICE_DESCRIPTOR_TYPE_CAMERA
} DeviceDescriptorType;

typedef enum
{
    CAMERA_PROTOCOL_UNKNOWN,
    CAMERA_PROTOCOL_LEGACY,
    CAMERA_PROTOCOL_OPENHOME
} CameraProtocol;

typedef enum
{
    DEVICE_FIRMWARE_TYPE_UNKNOWN,
    DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA,
    DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY,
    DEVICE_FIRMWARE_TYPE_CAMERA
} DeviceFirmwareType;

typedef struct
{
    char *version;
    icLinkedList *filenames;
    char *checksum; //optional
    DeviceFirmwareType type;
} DeviceFirmware;

typedef enum
{
    DEVICE_VERSION_LIST_TYPE_UNKNOWN,
    DEVICE_VERSION_LIST_TYPE_LIST,
    DEVICE_VERSION_LIST_TYPE_WILDCARD,
    DEVICE_VERSION_LIST_TYPE_RANGE
} DeviceVersionListType;

typedef struct
{
    DeviceVersionListType listType;
    char *format; // optional
    union
    {
        icLinkedList *versionList;
        bool versionIsWildcarded;
        struct
        {
            char *from;
            char *to;
        } versionRange;
    } list;

} DeviceVersionList;

typedef struct
{
    DeviceDescriptorType deviceDescriptorType;
    char *uuid;
    char *description; //optional, could be NULL
    char *manufacturer;
    char *model;
    DeviceVersionList *hardwareVersions; //could be complex list, wildcard, or range.  Use version functions to access
    DeviceVersionList *firmwareVersions; //could be complex list, wildcard, or range.  Use version functions to access
    char *minSupportedFirmwareVersion; //optional, could be NULL
    icStringHashMap *metadata; //optional name/value pairs of strings
    DeviceFirmware *latestFirmware; //optional
} DeviceDescriptor;

typedef struct
{
    bool   enabled;

    struct
    {
        int low;
        int medium;
        int high;
    } sensitivity;

    struct
    {
        int low;
        int medium;
        int high;
    } detectionThreshold;

    struct
    {
        int width;
        int height;
        int bottom;
        int top;
        int left;
        int right;
    } regionOfInterest;
} CameraMotionSettings;

typedef struct
{
    DeviceDescriptor        baseDescriptor; //must be first to support casting
    CameraProtocol          protocol;
    CameraMotionSettings    defaultMotionSettings; //optional
} CameraDeviceDescriptor;

typedef struct
{
    DeviceDescriptor        baseDescriptor;

} ZigbeeDeviceDescriptor;

/*
 * Free any memory allocated for this device descriptor.  Caller may still need to free the DeviceDescriptor
 * pointer if it is in heap.
 */
void deviceDescriptorFree(DeviceDescriptor *dd);
inline void deviceDescriptorFree__auto(DeviceDescriptor **dd)
{
    deviceDescriptorFree(*dd);
}

/*
 * Display a device descriptor to the info log.  arg is provided for compatibility with the iterator callback func
 * and is ignored.
 */
bool deviceDescriptorPrint(DeviceDescriptor *dd, void *arg);

/*
 * Clone a device descriptor.
 */
DeviceDescriptor *deviceDescriptorClone(const DeviceDescriptor *dd);

#endif //ZILKER_DEVICEDESCRIPTOR_H
