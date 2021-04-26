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

/*-----------------------------------------------
 * sensorHelper.c
 *
 * set of helper functions to aid with the "business logic" needs
 * that many services require when interacting with sensor endpoints.
 * allows the lower layers of deviceService to remain generic and
 * not have to be concerned with specifics of sensors.
 *
 * Author: jelderton - 7/19/16
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <deviceHelper.h>
#include <sensorHelper.h>
#include <commonDeviceDefs.h>

/*
 * extract a resource from the values map.  the map-key will be in a
 * uri format (ex: /000d6f000c25c74c/ep/1/r/qualified)
 */
static const char *extractResource(icHashMap *map, const char *ownerUri, const char *resourceName)
{
    // build the URI using the device/endpoint id and the resource name
    //
    char *retVal = NULL;
    char *uri = createResourceUri(ownerUri, resourceName);

    // find in the hash
    //
    DSResource *resource = (DSResource *)hashMapGet(map, (void *)uri, (uint16_t)strlen(uri));
    if (resource != NULL)
    {
        retVal = resource->value;
    }

    // cleanup and return
    //
    free(uri);
    return (const char *)retVal;
}

/*
 * examine the DSEndpoint 'status' attribute to see if faulted or restored.
 */
bool isEndpointFaulted(DSEndpoint *endpoint)
{
    // get the 'SENSOR_PROFILE_RESOURCE_STATUS' attribute, then parse to
    // see if this particular Endpoint is 'faulted' or not
    //
    const char *resource = extractResource(endpoint->resourcesValuesMap, endpoint->uri, SENSOR_PROFILE_RESOURCE_FAULTED);
    return (strcmp("true", resource) == 0);
}

/*
 * similar to 'isEndpointFaulted', however uses the information
 * from a DeviceServiceResourceUpdatedEvent.
 * note that this can only extract the faulted state when the
 * event->resource->id == SENSOR_PROFILE_RESOURCE_FAULTED
 */
bool isEndpointFaultedViaEvent(DeviceServiceResourceUpdatedEvent *event)
{
    bool retVal = false;

    // sanity check
    //
    if (event == NULL || event->resource == NULL || event->resource->id == NULL)
    {
        // unable to check the supplied event
        //
        return false;
    }

    // see if this event depicts a resource change
    //
    if(strcmp(event->resource->id, SENSOR_PROFILE_RESOURCE_FAULTED) == 0 && event->resource->value != NULL)
    {
        retVal = strcmp("true", event->resource->value) == 0;
    }

    return retVal;
}

/*
 * extract the DSEndpoint 'bypassed' attribute.
 */
bool isEndpointBypassed(DSEndpoint *endpoint)
{
    bool retVal = false;

    // get the 'SENSOR_PROFILE_RESOURCE_BYPASSED' attribute, then parse to
    // see if this particular Endpoint is 'bypassed' or not
    //
    const char *statusAttrib = extractResource(endpoint->resourcesValuesMap, endpoint->uri, SENSOR_PROFILE_RESOURCE_BYPASSED);
    if (statusAttrib != NULL && strcmp("true", statusAttrib) == 0)
    {
        retVal = true;
    }

    return retVal;
}

/*
 * similar to 'isEndpointBypassed', however uses the information
 * from a DeviceServiceResourceUpdatedEvent.
 * note that this can only extract the bypassed state when the
 * event->resource->id == SENSOR_PROFILE_RESOURCE_BYPASSED
 */
bool isEndpointBypassedViaEvent(DeviceServiceResourceUpdatedEvent *event)
{
    bool retVal = false;

    // sanity check
    //
    if (event == NULL || event->resource == NULL || event->resource->id == NULL)
    {
        // unable to check the supplied event
        //
        return false;
    }

    // see if this event depicts a resource change
    //
    if(strcmp(event->resource->id, SENSOR_PROFILE_RESOURCE_BYPASSED) == 0 && event->resource->value != NULL)
    {
        // see if this attribute is 'true' or not
        //
        if (strcmp("true", event->resource->value) == 0)
        {
            retVal = true;
        }
    }

    return retVal;
}
