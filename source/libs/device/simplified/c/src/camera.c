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
 * camera.c
 *
 * simplified object to represent a camera.
 * uses information from the deviceService DSDevice and
 * DSEndpoint to create the simplified object.
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <deviceHelper.h>
#include <commonDeviceDefs.h>
#include <camera.h>
#include "helper.h"

/*
 * Create a basic Camera object
 */
Camera *createCamera(void)
{
    return (Camera *)calloc(1, sizeof(Camera));
}

/*
 * Create a Camera using 'device resources' from deviceService.
 *  If 'label' is not defined in the device, assigns it to "My Camera"
 */
Camera *createCameraFromDevice(DSDevice *device)
{
    // assign info from the device object
    //
    Camera *retVal = createCamera();
    retVal->deviceId = strdup(device->id);

    // fill in rest using resources from the device
    retVal->label = extractDeviceResource(device, COMMON_ENDPOINT_RESOURCE_LABEL, "My Camera");
    retVal->manufacturer = extractDeviceResource(device, COMMON_DEVICE_RESOURCE_MANUFACTURER, NULL);
    retVal->model = extractDeviceResource(device, COMMON_DEVICE_RESOURCE_MODEL, NULL);
    retVal->macAddress = extractDeviceResource(device, COMMON_DEVICE_RESOURCE_MAC_ADDRESS, NULL);
    retVal->ipAddress = extractDeviceResource(device, COMMON_DEVICE_RESOURCE_IP_ADDRESS, NULL);
    retVal->serialNumber = extractDeviceResource(device, COMMON_DEVICE_RESOURCE_SERIAL_NUMBER, NULL);

    return retVal;
}

/*
 * Destroy a Camera object
 */
void destroyCamera(Camera *camera)
{
    if (camera != NULL)
    {
        free(camera->label);
        camera->label = NULL;
        free(camera->deviceId);
        camera->deviceId = NULL;
        free(camera->manufacturer);
        camera->manufacturer = NULL;
        free(camera->model);
        camera->model = NULL;
        free(camera->macAddress);
        camera->macAddress = NULL;
        free(camera->ipAddress);
        camera->ipAddress = NULL;
        free(camera->serialNumber);
        camera->serialNumber = NULL;
        free(camera);
        camera = NULL;
    }
}

