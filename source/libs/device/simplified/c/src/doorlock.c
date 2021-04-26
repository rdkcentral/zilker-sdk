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
 * doorlock.c
 *
 * simplified object to represent a door-lock.
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
#include <doorlock.h>
#include "helper.h"

/*
 * Create a basic Doorlock object
 */
DoorLock *createDoorLock(void)
{
    return (DoorLock *)calloc(1, sizeof(DoorLock));
}

/*
 * Create a Doorlock using 'endpoint resources' from deviceService.
 * Caller should check if 'label' is empty, and assign one as needed.
 */
DoorLock *createDoorLockFromEndpoint(DSEndpoint *endpoint)
{
    // assign info from the endpoint itself
    DoorLock *retVal = createDoorLock();
    retVal->deviceId = strdup(endpoint->ownerId);
    retVal->endpointId = strdup(endpoint->id);
    retVal->label = extractEndpointResource(endpoint, COMMON_ENDPOINT_RESOURCE_LABEL, NULL);

    // see if the bolt is 'locked'
    retVal->isLocked = extractEndpointResourceAsBool(endpoint, DOORLOCK_PROFILE_RESOURCE_LOCKED, false);

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
 * Destroy a Doorlock object
 */
void destroyDoorLock(DoorLock *doorlock)
{
    if (doorlock != NULL)
    {
        free(doorlock->label);
        doorlock->label = NULL;
        free(doorlock->deviceId);
        doorlock->deviceId = NULL;
        free(doorlock->endpointId);
        doorlock->endpointId = NULL;
        free(doorlock->manufacturer);
        doorlock->manufacturer = NULL;
        free(doorlock->model);
        doorlock->model = NULL;
        free(doorlock);
        doorlock = NULL;
    }
}
