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
 * thermostat.h
 *
 * simplified object to represent a thermostat.
 * uses information from the deviceService DSDevice and
 * DSEndpoint to create the simplified object.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#ifndef ZILKER_SDK_THERMOSTAT_H
#define ZILKER_SDK_THERMOSTAT_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <deviceService/deviceService_pojo.h>

typedef enum {
    THERMOSTAT_OFF,
    THERMOSTAT_HEAT,
    THERMOSTAT_COOL,
    THERMOSTAT_AUTO,
    THERMOSTAT_FAN_ONLY
} ThermostatState;

// define a simple 'thermostat'
typedef struct {
    char       *label;                 // friendly label/name of the Thermostat (what is displayed to the user)
    char       *deviceId;              // deviceId of the sensor
    char       *endpointId;            // endpointId of the device
    bool       systemOn;
    bool       fanOn;
    float      currentTemperature;     // current temperature reading (in Celsius)
    ThermostatState state;
    float      coolSetpoint;           // desired temp when state is COOL
    float      heatSetpoint;           // desired temp when state is HEAT
    char       *manufacturer;
    char       *model;
} Thermostat;

/*
 * Create a basic Thermostat object
 */
Thermostat *createThermostat(void);

/*
 * Create a Thermostat using 'endpoint resources' from deviceService.
 * Caller should check if 'label' is empty, and assign one as needed.
 */
Thermostat *createThermostatFromEndpoint(DSEndpoint *endpoint);

/*
 * Destroy a Thermostat object
 */
void destroyThermostat(Thermostat *thermostat);

#endif //ZILKER_SDK_THERMOSTAT_H
