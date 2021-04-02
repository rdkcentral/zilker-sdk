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
 * sensorHelper.h
 *
 * set of helper functions to aid with the "business logic" needs
 * that many services require when interacting with sensor endpoints.
 * allows the lower layers of deviceService to remain generic and
 * not have to be concerned with specifics of sensors.
 *
 * Author: jelderton - 7/19/16
 *-----------------------------------------------*/

#ifndef ZILKER_SENSORHELPER_H
#define ZILKER_SENSORHELPER_H

#include <stdbool.h>
#include <deviceService/deviceService_pojo.h>
#include <deviceService/deviceService_event.h>

/*
 * examine the DSEndpoint 'status' attribute to see if faulted or restored.
 */
bool isEndpointFaulted(DSEndpoint *endpoint);

/*
 * similar to 'isEndpointFaulted', however uses the information
 * from a DeviceServiceResourceUpdatedEvent.
 * note that this can only extract the faulted state when the
 * event->resource->id == SENSOR_PROFILE_RESOURCE_STATUS
 */
bool isEndpointFaultedViaEvent(DeviceServiceResourceUpdatedEvent *event);

/*
 * extract the DSEndpoint 'bypassed' attribute.
 */
bool isEndpointBypassed(DSEndpoint *endpoint);

/*
 * similar to 'isEndpointBypassed', however uses the information
 * from a DeviceServiceResourceUpdatedEvent.
 * note that this can only extract the bypassed state when the
 * event->resource->id == SENSOR_PROFILE_RESOURCE_BYPASSED
 */
bool isEndpointBypassedViaEvent(DeviceServiceResourceUpdatedEvent *event);


// overall type/class of a Sensor
typedef enum {
    SENSOR_TYPE_FIRST_AND_INVALID  = -1,  // For bounds checking and iteration
    SENSOR_TYPE_UNKNOWN            = 0,
    SENSOR_TYPE_DOOR               = 1,
    SENSOR_TYPE_WINDOW             = 2,
    SENSOR_TYPE_MOTION             = 3,
    SENSOR_TYPE_GLASS_BREAK        = 4,
    SENSOR_TYPE_SMOKE              = 5,
    SENSOR_TYPE_CO                 = 6,
    SENSOR_TYPE_ENVIRONMENTAL      = 7,
    SENSOR_TYPE_WATER              = 8,
    SENSOR_TYPE_MEDICAL            = 9,
    SENSOR_TYPE_LAST_AND_INVALID   = 10   // For bounds checking and iteration
} SensorType;

// NULL terminated array that should correlate to the SensorType enum
static const char *sensorTypeLabels[] = {
        "SENSOR_TYPE_UNKNOWN",       // SENSOR_TYPE_UNKNOWN
        "SENSOR_TYPE_DOOR",          // SENSOR_TYPE_DOOR
        "SENSOR_TYPE_WINDOW",        // SENSOR_TYPE_WINDOW
        "SENSOR_TYPE_MOTION",        // SENSOR_TYPE_MOTION
        "SENSOR_TYPE_GLASS_BREAK",   // SENSOR_TYPE_GLASS_BREAK
        "SENSOR_TYPE_SMOKE",         // SENSOR_TYPE_SMOKE
        "SENSOR_TYPE_CO",            // SENSOR_TYPE_CO
        "SENSOR_TYPE_ENVIRONMENTAL", // SENSOR_TYPE_ENVIRONMENTAL
        "SENSOR_TYPE_WATER",         // SENSOR_TYPE_WATER
        "SENSOR_TYPE_MEDICAL",       // SENSOR_TYPE_MEDICAL
        NULL
};

// define a 'sensor'
typedef struct {
    char       *label;                 // friendly label/name of the Sensor (what is displayed to the user)
    char       *deviceId;              // deviceId of the sensor
    char       *endpointId;            // endpointId of the device
    bool       isFaulted;              // if the sensor is faulted (open) or not
    bool       isTroubled;             // if the sensor reported a trouble (ex: low battery)
    SensorType type;                   // kind of sensor
} Sensor;

/*
 * Create a basic Sensor object
 */
Sensor *createSensor(void);

/*
 * Create a Sensor using 'endpoint resources' from deviceService.
 * Caller should check if 'label' is empty, and assign one as needed.
 */
Sensor *createSensorFromEndpoint(DSEndpoint *endpoint);

/*
 * Destroy a Sensor object
 */
void destroySensor(Sensor *sensor);


#endif // ZILKER_SENSORHELPER_H
