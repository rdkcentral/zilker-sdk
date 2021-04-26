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
 * cameraSet.h
 *
 * Container of cameraDevice objects.
 *
 * Author: jelderton - 6/21/16
 *-----------------------------------------------*/

#ifndef ZILKER_CAMERASET_H
#define ZILKER_CAMERASET_H

#include <icTypes/icLinkedList.h>
#include "cameraDevice.h"

/*
 * extend the icLinkedList object
 */
typedef icLinkedList cameraSet;

typedef void (*cameraSetIterateFunc)(cameraDevice *item, void *arg);

/*
 * create a camera set
 */
cameraSet *createCameraSet();

/*
 * delete the camera set.  will also destroy
 * each cameraDevice object within the set.
 */
void destroyCameraSet(cameraSet *set);

/*
 * delete the contents within camera set, but
 * leave the container intact.  will destroy
 * each cameraDevice object within the set.
 */
void clearCameraSet(cameraSet *set);

/*
 * simple wrapper for appending a camera to a set. Enables locking for the operation.
 */
void appendCameraToSet(cameraSet *set, cameraDevice *item);

/*
 * iterates over a camera set and applies the function `callback` to each element.
 */
void cameraSetIterate(cameraSet *set, cameraSetIterateFunc callback, void *arg);

/*
 * find a camera from the set, using the uuid
 */
cameraDevice *findCameraByUuid(cameraSet *set, const char *uuid);

/*
 * find a camera from the set, using the ip address
 */
cameraDevice *findCameraByIpAddress(cameraSet *set, const char *ipAddress);

/*
 * destroy a single camera device in the set
 */
void destroyCameraDeviceFromSet(cameraSet *set, const char *uuid);

/*
 * move a single camera device from one set to another
 */
void moveCameraDeviceToSet(const char *uuid, cameraSet *srcSet, cameraSet *destSet);

/*
 * return the number of elements in the camera set
 */
uint16_t cameraSetCount(cameraSet *set);

#endif //ZILKER_CAMERASET_H
