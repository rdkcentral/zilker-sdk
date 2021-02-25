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
 *
 * Created by Thomas Lea on 7/22/15.
 */

#ifndef ZILKER_ICDEVICERESOURCE_H
#define ZILKER_ICDEVICERESOURCE_H

#include <stdint.h>
#include <stdbool.h>
#include <cjson/cJSON.h>
#include <icTypes/icLinkedList.h>
#include <serial/icSerDesContext.h>
#include <deviceService/resourceModes.h>

typedef enum
{
    CACHING_POLICY_NEVER, //Never cache this attribute and always call the device driver to retrieve the value
    CACHING_POLICY_ALWAYS, //Always cache this attribute and never call the device driver to retrieve the value
} ResourceCachingPolicy;

typedef struct
{
    char *id;
    char *uri;
    char *endpointId; // NULL if on the root device
    char *deviceUuid;
    char *value;
    char *type;
    uint8_t mode; //bitmask of RESOURCE_MODE_*
    ResourceCachingPolicy cachingPolicy;
    uint64_t dateOfLastSyncMillis; //GMT epoch based time when this resource was last read from or written to the device.  0 for never.

} icDeviceResource;

void resourceDestroy(icDeviceResource *resource);
inline void resourceDestroy__auto(icDeviceResource **resource)
{
    resourceDestroy(*resource);
}

void resourcePrint(icDeviceResource *resource, const char *prefix);

/**
 * Clone a device resource object
 *
 * @param resource the resource to clone
 * @return the cloned resource object
 */
icDeviceResource *resourceClone(const icDeviceResource *resource);

/**
 * Convert resource object to JSON
 *
 * @param resource the resource to convert
 * @return the JSON object
 */
cJSON *resourceToJSON(const icDeviceResource *resource, const icSerDesContext *context);

/**
 * Convert a list of resources objects to a JSON object with id as key
 *
 * @param resources the list of resources
 * @return the JSON object
 */
cJSON *resourcesToJSON(icLinkedList *resources, const icSerDesContext *context);

/**
* Load a device resource into memory from JSON
*
* @param deviceUUID the deviceUUID for which we are loading the metadata
* @param endpointId the endpointId for which we are loading the metdata
* @param resourceJSON the JSON to load
* @return the resource object or NULL if there is an error
*/
icDeviceResource *resourceFromJSON(const char *deviceUUID, const char *endpointId, cJSON *resourceJSON,
                                   const icSerDesContext *context);

/**
 * Load the resources for a device and endpoint from JSON
 *
 * @param device  the device
 * @param endpoint the endpoint
 * @param resourcesJson the JSON to load
 * @return linked list of resource structures, caller is responsible for destroying result
 *
 * @see linkedListDestroy
 * @see resourceDestroy
 */
icLinkedList *resourcesFromJSON(const char *deviceUUID, const char *endpointId, cJSON *resourcesJSON,
                                const icSerDesContext *context);

#endif //ZILKER_ICDEVICERESOURCE_H
