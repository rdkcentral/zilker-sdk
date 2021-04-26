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
 * sensor.c
 *
 * simplified object to represent a sensor.
 * uses information from the deviceService DSDevice and
 * DSEndpoint to create the simplified object.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <deviceHelper.h>
#include <sensorHelper.h>
#include <commonDeviceDefs.h>
#include <sensor.h>
#include "helper.h"

/* forward dec */
static SensorType getSensorTypeFromResourceString(const char *sensorType);

/*
 * Create a basic Sensor object
 */
Sensor *createSensor(void)
{
    return (Sensor *)calloc(1, sizeof(Sensor));
}

/*
 * Create a Sensor using 'endpoint resources' from deviceService.
 * Caller should check if 'label' is empty, and assign one as needed.
 */
Sensor *createSensorFromEndpoint(DSEndpoint *endpoint)
{
    // assign info from the endpoint itself
    Sensor *retVal = createSensor();
    retVal->deviceId = strdup(endpoint->ownerId);
    retVal->endpointId = strdup(endpoint->id);
    retVal->isFaulted = isEndpointFaulted(endpoint);

    // get info saved in the endpoint
    retVal->label = extractEndpointResource(endpoint, COMMON_ENDPOINT_RESOURCE_LABEL, NULL);
    char *sensorType = extractEndpointResource(endpoint, SENSOR_PROFILE_RESOURCE_TYPE, NULL);
    retVal->type = getSensorTypeFromResourceString(sensorType);
    free(sensorType);

    // get info saved in the device (parent owner)
    char *tmp = NULL;
    if (deviceHelperReadDeviceResource(retVal->deviceId, COMMON_DEVICE_RESOURCE_SERIAL_NUMBER, &tmp) == true)
    {
        retVal->serialNumber = tmp;
        tmp = NULL;
    }
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
 * Destroy a Sensor object
 */
void destroySensor(Sensor *sensor)
{
    if (sensor != NULL)
    {
        free(sensor->label);
        sensor->label = NULL;
        free(sensor->deviceId);
        sensor->deviceId = NULL;
        free(sensor->endpointId);
        sensor->endpointId = NULL;
        free(sensor->manufacturer);
        sensor->manufacturer = NULL;
        free(sensor->model);
        sensor->model = NULL;
        free(sensor->serialNumber);
        sensor->serialNumber = NULL;
        free(sensor);
        sensor = NULL;
    }
}

/*
 * find enum value for the sensorType string extracted from the SENSOR_PROFILE_RESOURCE_TYPE resource
 */
static SensorType getSensorTypeFromResourceString(const char *sensorType)
{
    if (sensorType != NULL)
    {
        if (strcmp(sensorType, SENSOR_PROFILE_CONTACT_SWITCH_TYPE) == 0)
        {
            return SENSOR_TYPE_DOOR;
        }
        else if (strcmp(sensorType, SENSOR_PROFILE_MOTION_TYPE) == 0)
        {
            return SENSOR_TYPE_MOTION;
        }
        else if(strcmp(sensorType, SENSOR_PROFILE_CO) == 0)
        {
            return SENSOR_TYPE_CO;
        }
        else if(strcmp(sensorType, SENSOR_PROFILE_WATER) == 0)
        {
            return SENSOR_TYPE_WATER;
        }
        else if(strcmp(sensorType, SENSOR_PROFILE_SMOKE) == 0)
        {
            return SENSOR_TYPE_SMOKE;
        }
        else if(strcmp(sensorType, SENSOR_PROFILE_GLASS_BREAK) == 0)
        {
            return SENSOR_TYPE_GLASS_BREAK;
        }
        else
        {
            // TODO: more sensor type mappings?
            // assume door/window for now
            //
            return SENSOR_TYPE_DOOR;
        }
    }

    return SENSOR_TYPE_UNKNOWN;
}
