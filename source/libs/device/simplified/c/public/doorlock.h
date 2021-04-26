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
 * doorlock.h
 *
 * simplified object to represent a door-lock.
 * uses information from the deviceService DSDevice and
 * DSEndpoint to create the simplified object.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#ifndef ZILKER_SDK_DOORLOCK_H
#define ZILKER_SDK_DOORLOCK_H

#include <stdbool.h>
#include <deviceService/deviceService_pojo.h>

// define a 'door-lock'
typedef struct {
    char       *label;                 // friendly label/name of the Doorlock (what is displayed to the user)
    char       *deviceId;              // deviceId of the sensor
    char       *endpointId;            // endpointId of the device
    bool       isLocked;
    char       *manufacturer;
    char       *model;
} DoorLock;

/*
 * Create a basic Doorlock object
 */
DoorLock *createDoorLock(void);

/*
 * Create a Doorlock using 'endpoint resources' from deviceService.
 * Caller should check if 'label' is empty, and assign one as needed.
 */
DoorLock *createDoorLockFromEndpoint(DSEndpoint *endpoint);

/*
 * Destroy a Doorlock object
 */
void destroyDoorLock(DoorLock *doorlock);

#endif //ZILKER_SDK_DOORLOCK_H
