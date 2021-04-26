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
 * cameraSet.c
 *
 * Container of cameraDevice objects.
 *
 * Author: jelderton - 6/21/16
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <icUtil/stringUtils.h>
#include "cameraDevice.h"
#include "cameraSet.h"

//A mutex to be used for set operations for all camera sets.
static pthread_mutex_t camSetsMutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * internal functions
 */
static bool searchCameraForMatchingUUID(void *searchVal, void *item);
static bool searchCameraForMatchingIpAddress(void *searchVal, void *item);
static void internalDestroyCameraDeviceFromSet(void *item);

/*
 * create a camera set
 */
cameraSet *createCameraSet()
{
    return (cameraSet *)linkedListCreate();
}

/*
 * delete the camera set.  will also destroy
 * each cameraDevice object within the set.
 */
void destroyCameraSet(cameraSet *set)
{
    pthread_mutex_lock(&camSetsMutex);
    linkedListDestroy(set, internalDestroyCameraDeviceFromSet);
    pthread_mutex_unlock(&camSetsMutex);
}

/*
 * delete the contents within camera set, destroying
 * each cameraDevice object within the set.
 */
void clearCameraSet(cameraSet *set)
{
    pthread_mutex_lock(&camSetsMutex);
    linkedListClear(set, internalDestroyCameraDeviceFromSet);
    pthread_mutex_unlock(&camSetsMutex);
}

/*
 * simple wrapper for appending a camera to a set. Enables locking for the operation.
 */
void appendCameraToSet(cameraSet *set, cameraDevice *item)
{
    pthread_mutex_lock(&camSetsMutex);
    linkedListAppend(set, item);
    pthread_mutex_unlock(&camSetsMutex);
}

/*
 * iterates over a camera set and applies the function `callback` to each element.
 */
void cameraSetIterate(cameraSet *set, cameraSetIterateFunc callback, void *arg)
{
    pthread_mutex_lock(&camSetsMutex);
    icLinkedListIterator *camIter = linkedListIteratorCreate(set);
    while (linkedListIteratorHasNext(camIter))
    {
        cameraDevice *cam = linkedListIteratorGetNext(camIter);
        callback(cam, arg);
    }
    linkedListIteratorDestroy(camIter);
    pthread_mutex_unlock(&camSetsMutex);
}

/*
 * find a camera from the set, using the uuid
 */
cameraDevice *findCameraByUuid(cameraSet *set, const char *uuid)
{
    pthread_mutex_lock(&camSetsMutex);
    cameraDevice *result = (cameraDevice *)linkedListFind(set, (void *)uuid, searchCameraForMatchingUUID);
    pthread_mutex_unlock(&camSetsMutex);
    return result;
}

/*
 * find a camera from the set, using the ip address
 */
cameraDevice *findCameraByIpAddress(cameraSet *set, const char *ipAddress)
{
    pthread_mutex_lock(&camSetsMutex);
    cameraDevice *result = (cameraDevice *)linkedListFind(set, (void *)ipAddress, searchCameraForMatchingIpAddress);
    pthread_mutex_unlock(&camSetsMutex);
    return result;
}

/*
 * destroy a single camera device in the set
 */
void destroyCameraDeviceFromSet(cameraSet *set, const char *uuid)
{
    pthread_mutex_lock(&camSetsMutex);
    linkedListDelete(set, (void *)uuid, searchCameraForMatchingUUID, internalDestroyCameraDeviceFromSet);
    pthread_mutex_unlock(&camSetsMutex);
}


/*
 * move a single camera device from one set to another
 */
void moveCameraDeviceToSet(const char *uuid, cameraSet *srcSet, cameraSet *destSet)
{
    // get the object in 'srcSet'
    //
    pthread_mutex_lock(&camSetsMutex);
    cameraDevice *obj = (cameraDevice *)linkedListFind(srcSet, (void *)uuid, searchCameraForMatchingUUID);
    if (obj != NULL)
    {
        // add to the new set
        //
        linkedListAppend(destSet, obj);

        // now remove from src, but don't free
        //
        linkedListDelete(srcSet, (void *)uuid, searchCameraForMatchingUUID, standardDoNotFreeFunc);
    }
    pthread_mutex_unlock(&camSetsMutex);
}

/*
 * return the number of elements in the camera set
 */
uint16_t cameraSetCount(cameraSet *set)
{
    uint16_t retVal = 0;
    pthread_mutex_lock(&camSetsMutex);
    retVal = linkedListCount(set);
    pthread_mutex_unlock(&camSetsMutex);
    return retVal;
}

/*
 * 'linkedListCompareFunc' implementation for searching
 * the linked list for a cameraDevice with a matching 'uuid'
 */
static bool searchCameraForMatchingUUID(void *searchVal, void *item)
{
    char *uuid = (char *)searchVal;
    cameraDevice *next = (cameraDevice *)item;

    // compare the two strings
    //
    if (stringCompare(uuid, next->uuid, false) == 0)
    {
        return true;
    }
    return false;
}

/*
 * 'linkedListCompareFunc' implementation for searching
 * the linked list for a cameraDevice with a matching 'ip address'
 */
static bool searchCameraForMatchingIpAddress(void *searchVal, void *item)
{
    char *uuid = (char *)searchVal;
    cameraDevice *next = (cameraDevice *)item;

    // compare the two strings
    //
    if (stringCompare(uuid, next->ipAddress, false) == 0)
    {
        return true;
    }
    return false;
}

/*
 * 'linkedListItemFreeFunc' implementation to destroy cameraDevice objects
 */
static void internalDestroyCameraDeviceFromSet(void *item)
{
    if (item != NULL)
    {
        // see if the camera monitor thread is still running
        //
        cameraDevice *cam = (cameraDevice *)item;
        if (cam->monitorRunning == true)
        {
            // cannot destroy this while the monitor thread is running.  our choices are to stop
            // that thread (and wait for it to die) or just mark this as CAMERA_OP_STATE_DESTROY
            // and let that thread do the hard part.
            //
            pthread_mutex_lock(&cam->mutex);
            cam->opState = CAMERA_OP_STATE_DESTROY;
            pthread_cond_broadcast(&cam->cond);
            pthread_mutex_unlock(&cam->mutex);
            pthread_join(cam->monitorThread, NULL);

            // return so the object is removed from the set, but not released
            //
        }
        else
        {
            // safe to destroy
            //
            destroyCameraDevice((cameraDevice *)item);
        }
    }
}