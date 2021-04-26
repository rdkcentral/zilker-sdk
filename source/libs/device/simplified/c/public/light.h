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
 * light.h
 *
 * simplified object to represent a light.
 * uses information from the deviceService DSDevice and
 * DSEndpoint to create the simplified object.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#ifndef ZILKER_SDK_LIGHT_H
#define ZILKER_SDK_LIGHT_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <deviceService/deviceService_pojo.h>

// define a 'light'
typedef struct {
    char       *label;                 // friendly label/name of the Light (what is displayed to the user)
    char       *deviceId;              // deviceId of the sensor
    char       *endpointId;            // endpointId of the device
    bool       isOn;
    bool       isDimable;              // if reports the dimLevel
    uint16_t   dimLevel;               // only if isDimable == true. range: 0 - 100
    char       *manufacturer;
    char       *model;
} Light;

/*
 * Create a basic Light object
 */
Light *createLight(void);

/*
 * Create a Light using 'endpoint resources' from deviceService.
 * Caller should check if 'label' is empty, and assign one as needed.
 */
Light *createLightFromEndpoint(DSEndpoint *endpoint);

/*
 * Destroy a Light object
 */
void destroyLight(Light *light);

#endif //ZILKER_SDK_LIGHT_H
