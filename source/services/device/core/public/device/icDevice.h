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

/*
 * Created by Thomas Lea on 7/22/15.
 */

#ifndef ZILKER_ICDEVICE_H
#define ZILKER_ICDEVICE_H

#include <device/icDeviceResource.h>
#include <device/icDeviceEndpoint.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>
#include <stddef.h>
#include <cjson/cJSON.h>
#include <serial/icSerDesContext.h>

//NOTE: the contents of icDevice is all exposed here for now until we are confident in the data model.
// Later we may want to hide behind an opaque type.
typedef struct
{
    char    *uuid;
    char    *deviceClass;
    uint8_t deviceClassVersion;
    char    *uri; // this is likely just "/[device class]/[uuid]"
    char    *managingDeviceDriver;
    icLinkedList *endpoints;
    icLinkedList *resources;
    icLinkedList *metadata;
} icDevice;

void deviceDestroy(icDevice *device);
inline void deviceDestroy__auto(icDevice **device)
{
    deviceDestroy(*device);
}

void devicePrint(icDevice *device, const char *prefix);

/**
 * Clone a device
 *
 * @param device the device to clone
 * @return the cloned device object
 */
icDevice *deviceClone(const icDevice *device);

/**
 * Convert device object to JSON
 *
 * @param device the device to convert
 * @return the JSON object
 */
cJSON *deviceToJSON(const icDevice *device, const icSerDesContext *context);

/**
 * Load a device into memory from JSON
 *
 * @param json the JSON to load
 * @return the device object or NULL if there is an error
 */
icDevice *deviceFromJSON(cJSON *json, const icSerDesContext *context);

/**
 * Retrieve a metadata item from the provided device, if it exists.
 *
 * @param device
 * @param key
 * @return the metadata value or NULL if not found
 */
const char *deviceGetMetadata(const icDevice *device, const char *key);

#endif //ZILKER_ICDEVICE_H
