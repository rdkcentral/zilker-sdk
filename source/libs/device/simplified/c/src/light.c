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
 * light.c
 *
 * simplified object to represent a light.
 * uses information from the deviceService DSDevice and
 * DSEndpoint to create the simplified object.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include <deviceHelper.h>
#include <commonDeviceDefs.h>
#include <light.h>
#include "helper.h"

/*
 * Create a basic Light object
 */
Light *createLight(void)
{
    return (Light *)calloc(1, sizeof(Light));
}

/*
 * Create a Light using 'endpoint resources' from deviceService.
 * Caller should check if 'label' is empty, and assign one as needed.
 */
Light *createLightFromEndpoint(DSEndpoint *endpoint)
{
    // assign info from the endpoint itself
    Light *retVal = createLight();
    retVal->deviceId = strdup(endpoint->ownerId);
    retVal->endpointId = strdup(endpoint->id);
    retVal->label = extractEndpointResource(endpoint, COMMON_ENDPOINT_RESOURCE_LABEL, NULL);

    // see if the light is 'on'
    retVal->isOn = extractEndpointResourceAsBool(endpoint, LIGHT_PROFILE_RESOURCE_IS_ON, false);

    // see if this support dim leval
    char *level = extractEndpointResource(endpoint, LIGHT_PROFILE_RESOURCE_CURRENT_LEVEL, NULL);
    if (level != NULL)
    {
        retVal->isDimable = true;
        stringToUint16(level, &retVal->dimLevel);
    }

    // get info saved in the device (parent owner)
    char *tmp = NULL;
    if (deviceHelperReadDeviceResource(retVal->deviceId, COMMON_DEVICE_RESOURCE_MANUFACTURER, &tmp) == true)
    {
        retVal->manufacturer = tmp;
        tmp = NULL;
    }
    if (deviceHelperReadDeviceResource(retVal->deviceId, COMMON_DEVICE_RESOURCE_MODEL, &tmp) == true)
    {
        retVal->model = tmp;
        tmp = NULL;
    }

    return retVal;
}

/*
 * Destroy a Light object
 */
void destroyLight(Light *light)
{
    if (light != NULL)
    {
        free(light->label);
        light->label = NULL;
        free(light->deviceId);
        light->deviceId = NULL;
        free(light->endpointId);
        light->endpointId = NULL;
        free(light->manufacturer);
        light->manufacturer = NULL;
        free(light->model);
        light->model = NULL;
        free(light);
        light = NULL;
    }
}
