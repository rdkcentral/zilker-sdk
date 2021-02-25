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
 * troubleContainer.h
 *
 * container used internally by troubleState to hold
 * all of the known information about an individual trouble
 *
 * Author: jelderton -  3/3/19.
 *-----------------------------------------------*/

#ifndef ZILKER_TROUBLECONTAINER_H
#define ZILKER_TROUBLECONTAINER_H

#include <securityService/securityService_event.h>
#include <securityService/sensorTroubleEventHelper.h>
#include <securityService/cameraTroubleEventHelper.h>

/*
 * used/stored in the trouble container to define what
 * the trouble "extra" is defining (camera, zone, etc)
 */
typedef enum {
    TROUBLE_DEVICE_TYPE_NONE = 0,   // no 'extra' section for the trouble
    TROUBLE_DEVICE_TYPE_ZONE,
    TROUBLE_DEVICE_TYPE_CAMERA,
    TROUBLE_DEVICE_TYPE_IOT,
    // add more here for each encode/decode to stuff in 'extra'
} troublePayloadType;

/*
 * the container used to internally store the troubleEvent objects
 * within the 'troubleList', as-well-as some transient information
 * that stays local to securityService.
 */
typedef struct _troubleContainer {
    // the event that is publicly exposed
    troubleEvent *event;

    // define what 'payload' is stored in event->trouble->extra
    troublePayloadType payloadType;

    // the "decoded" payload object that is stored in event->trouble->extra
    // helps to eliminate unnecessary decoding of the trouble->extra JSON.
    // use the 'payloadType' variable to determine which of these to use
    union {
        SensorTroublePayload *zone;     // TROUBLE_DEVICE_TYPE_ZONE
        CameraTroublePayload *camera;   // TROUBLE_DEVICE_TYPE_CAMERA
        DeviceTroublePayload *device;   // TROUBLE_DEVICE_TYPE_IOT  (light, thermostat, door-lock, etc)
    } extraPayload;

    // if true, this trouble will be persisted to storage.  Default is true.
    bool persist;

} troubleContainer;


/*
 * create an empty troubleContainer
 */
troubleContainer *createTroubleContainer();

/*
 * deep clone a troubleContainer
 */
troubleContainer *cloneTroubleContainer(troubleContainer *container);

/*
 * destroy a troubleConteiner
 */
void destroyTroubleContainer(troubleContainer *container);

/*
 * 'linkedListItemFreeFunc' implementation used when deleting
 * troubleContainer objects from the linked list.
 */
void destroyTroubleContainerFromList(void *item);

#endif // ZILKER_TROUBLECONTAINER_H
