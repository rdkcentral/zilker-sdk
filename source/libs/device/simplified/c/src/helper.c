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
 * helper.h
 *
 * helper functions used by the simplified object creation functions
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <icUtil/stringUtils.h>
#include <deviceHelper.h>
#include "helper.h"

/*
 * extract the desired resource from the device object.
 * returns a copy of the resource value, or a copy of the defaultValue
 * caller is responsible for releasing the returned string
 */
char *extractDeviceResource(DSDevice *device, const char *attribName, const char *defaultValue)
{
    // bail if bad input
    //
    if (device == NULL || attribName == NULL)
    {
        return NULL;
    }

    // determine the resource path (uri) to read from
    char *uri = createResourceUri(device->uri, attribName);
    if (uri == NULL)
    {
        return NULL;
    }

    // extract the resource from the device
    char *value = NULL;
    DSResource *resource = get_DSResource_from_DSDevice_resources(device, uri);
    if (resource == NULL)
    {
        // ask deviceService for the value
        deviceHelperReadResourceByUri(uri, &value);
    }
    else
    {
        // pull the value from the resource
        if (resource->value != NULL)
        {
            value = strdup(resource->value);
        }
    }
    free(uri);

    // use default if no value obtained
    if (value == NULL && defaultValue != NULL)
    {
        value = strdup(defaultValue);
    }
    return value;
}

/*
 * extract the desired resource from the endpoint object.
 * returns a copy of the resource value, or a copy of the defaultValue
 * caller is responsible for releasing the returned string
 */
char *extractEndpointResource(DSEndpoint *endpoint, const char *attribName, const char *defaultValue)
{
    // bail if bad input
    //
    if (endpoint == NULL || attribName == NULL)
    {
        return NULL;
    }

    // determine the resource path (uri) to read from
    char *uri = createResourceUri(endpoint->uri, attribName);
    if (uri == NULL)
    {
        return NULL;
    }

    // extract the resource from the device
    char *value = NULL;
    DSResource *resource = get_DSResource_from_DSEndpoint_resources(endpoint, uri);
    if (resource == NULL)
    {
        // ask deviceService for the value
        deviceHelperReadResourceByUri(uri, &value);
    }
    else
    {
        // pull the value from the resource
        if (resource->value != NULL)
        {
            value = strdup(resource->value);
        }
    }
    free(uri);

    // use default if no value obtained
    if (value == NULL && defaultValue != NULL)
    {
        value = strdup(defaultValue);
    }
    return value;
}

/*
 * extract the desired resource from the endpoint object.
 */
bool extractEndpointResourceAsBool(DSEndpoint *endpoint, const char *attribName, bool defaultValue)
{
    // get the string value
    char *value = extractEndpointResource(endpoint, attribName, NULL);
    if (value == NULL)
    {
        return defaultValue;
    }

    // convert to boolean
    bool retVal = stringToBool(value);
    free(value);
    return retVal;
}

/*
 * extract the desired resource from the endpoint object.
 */
float extractEndpointResourceAsFloat(DSEndpoint *endpoint, const char *attribName, float defaultValue)
{
    // get the string value
    char *value = extractEndpointResource(endpoint, attribName, NULL);
    if (value == NULL)
    {
        return defaultValue;
    }

    // convert to float
    char *bad = NULL;
    errno = 0;
    float result = strtof(value, &bad);
    if (errno || *bad)
    {
        result = defaultValue;
    }
    return result;
}