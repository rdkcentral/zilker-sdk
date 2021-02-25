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
 * An endpoint is a logical device encapsulated in the physical icDevice.  Examples of endpoints
 * on an open home camera would be 'camera' and 'sensor'.
 *
 * Created by Thomas Lea on 8/11/15.
 */

#ifndef ZILKER_ICDEVICEENDPOINT_H
#define ZILKER_ICDEVICEENDPOINT_H

#include <icTypes/icLinkedList.h>
#include <icTypes/icStringHashMap.h>
#include <cjson/cJSON.h>
#include <serial/icSerDesContext.h>

typedef struct
{
    char            *id;
    char            *uri;
    char            *profile;
    uint8_t         profileVersion;
    char            *deviceUuid;
    bool            enabled;
    icLinkedList    *resources;
    icLinkedList    *metadata;
} icDeviceEndpoint;

void endpointDestroy(icDeviceEndpoint *endpoint);
inline void endpointDestroy__auto(icDeviceEndpoint **endpoint)
{
    endpointDestroy(*endpoint);
}

void endpointPrint(icDeviceEndpoint *endpoint, const char *prefix);

/**
 * Clone a endpoint
 *
 * @param endpoint the endpoint to clone
 * @return the cloned endpoint object
 */
icDeviceEndpoint *endpointClone(const icDeviceEndpoint *endpoint);

/**
 * Convert endpoint object to JSON
 *
 * @param endpoint the endpoint to convert
 * @return the JSON object
 */
cJSON *endpointToJSON(const icDeviceEndpoint *endpoint, const icSerDesContext *context);

/**
 * Convert a list of endpoint objects to a JSON object with id as key
 *
 * @param endpoints the list of endpoints
 * @return the JSON object
 */
cJSON *endpointsToJSON(icLinkedList *endpoints, const icSerDesContext *context );

/**
 * Load a device endpoint into memory from JSON
 *
 * @param deviceUUID the device UUID for which we are loading the metadata
 * @param endpointJSON the JSON to load
 * @return the endpoint object or NULL if there is an error
 */
icDeviceEndpoint *endpointFromJSON(const char *deviceUUID, cJSON *endpointJSON, const icSerDesContext *context);

/**
 * Load the endpoints for a device from JSON
 *
 * @param deviceUUID  the device UUID
 * @param endpointsJSON the JSON to load
 * @return linked list of endpoint structures, caller is responsible for destroying result
 *
 * @see linkedListDestroy
 * @see endpointDestroy
 */
icLinkedList *endpointsFromJSON(const char *deviceUUID, cJSON *endpointsJSON, const icSerDesContext *context);

#endif //ZILKER_ICDEVICEENDPOINT_H
