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
 * camera.h
 *
 * simplified object to represent a camera.
 * uses information from the deviceService DSDevice and
 * DSEndpoint to create the simplified object.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#ifndef ZILKER_SDK_CAMERA_H
#define ZILKER_SDK_CAMERA_H

#include <stdlib.h>
#include <deviceService/deviceService_pojo.h>

// define a 'camera'
typedef struct {
    char    *label;                 // friendly label/name of the Camera (what is displayed to the user)
    char    *deviceId;              // deviceId of the Camera
    char    *manufacturer;
    char    *model;
    char    *serialNumber;
    char    *macAddress;
    char    *ipAddress;
} Camera;

/*
 * Create a basic Camera object
 */
Camera *createCamera(void);

/*
 * Create a Camera using 'device resources' from deviceService.
 * If 'label' is not defined in the device, assigns it to "My Camera"
 */
Camera *createCameraFromDevice(DSDevice *device);

/*
 * Destroy a Camera object
 */
void destroyCamera(Camera *camera);

#endif //ZILKER_SDK_CAMERA_H
