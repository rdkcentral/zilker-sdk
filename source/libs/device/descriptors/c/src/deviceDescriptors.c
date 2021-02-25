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

//
// Created by Thomas Lea on 7/29/15.
//

#include <stddef.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <libxml/tree.h>

#include <icLog/logging.h>
#include <icTypes/icLinkedList.h>
#include <xmlHelper/xmlHelper.h>
#include <deviceDescriptors.h>
#include <deviceDescriptor.h>
#include <icUtil/stringUtils.h>
#include "parser.h"

static bool parseFiles();

#define LOG_TAG "libdeviceDescriptors"

static pthread_mutex_t dataMutex = PTHREAD_MUTEX_INITIALIZER;

static char whiteListPath[PATH_MAX] = {'\0'};

static char blackListPath[PATH_MAX] = {'\0'};

static icLinkedList *deviceDescriptors = NULL;

static bool versionInRange(const char *versionInput, DeviceVersionList *allowedVersions);

void deviceDescriptorsInit(const char *wlPath, const char *blPath)
{
    icLogDebug(LOG_TAG, "deviceDescriptorsInit:  using WhiteList %s, and BlackList %s", wlPath, blPath);
    if (wlPath != NULL)
    {
        strncpy(whiteListPath, wlPath, PATH_MAX - 1);
    }

    if (blPath != NULL)
    {
        strncpy(blackListPath, blPath, PATH_MAX - 1);
    }
}

void deviceDescriptorsCleanup()
{
    icLogDebug(LOG_TAG, "deviceDescriptorsCleanup");
    pthread_mutex_lock(&dataMutex);

    if (deviceDescriptors != NULL)
    {
        linkedListDestroy(deviceDescriptors, (linkedListItemFreeFunc) deviceDescriptorFree);
        deviceDescriptors = NULL;
    }

    pthread_mutex_unlock(&dataMutex);
}

/*
 * Retrieve the matching DeviceDescriptor for the provided input or NULL if a matching one doesnt exist.
 */
DeviceDescriptor *deviceDescriptorsGet(const char *manufacturer,
                                       const char *model,
                                       const char *hardwareVersion,
                                       const char *firmwareVersion)
{
    DeviceDescriptor *result = NULL;

    icLogDebug(LOG_TAG, "deviceDescriptorsGet: manufacturer=%s, model=%s, hardwareVersion=%s, firmwareVersion=%s",
               manufacturer, model, hardwareVersion, firmwareVersion);

    //manufacturer and model are required
    if (manufacturer == NULL || model == NULL)
    {
        icLogError(LOG_TAG, "deviceDescriptorsGet: invalid arguments");
        return NULL;
    }

    pthread_mutex_lock(&dataMutex);

    if (deviceDescriptors == NULL)
    {
        icLogDebug(LOG_TAG, "no device descriptors loaded yet, attempting parse");
        parseFiles();
    }

    if (deviceDescriptors != NULL && linkedListCount(deviceDescriptors) > 0)
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(deviceDescriptors);
        while (linkedListIteratorHasNext(iterator))
        {
            DeviceDescriptor *dd = linkedListIteratorGetNext(iterator);

            if (strcmp(manufacturer, dd->manufacturer) != 0)
            {
                continue;
            }

            if (strcmp(model, dd->model) != 0)
            {
                continue;
            }

            if (versionInRange(hardwareVersion, dd->hardwareVersions) == false)
            {
                continue;
            }

            if (versionInRange(firmwareVersion, dd->firmwareVersions) == false)
            {
                continue;
            }

            //if we got here, we have a match!
            //to prevent use after free issues create a copy to hand back to the caller
            result = deviceDescriptorClone(dd);
            break;
        }

        linkedListIteratorDestroy(iterator);
    }
    else
    {
        icLogDebug(LOG_TAG, "no device descriptors available.");
    }

    pthread_mutex_unlock(&dataMutex);

    return result;
}

static bool parseFiles()
{
    bool result = true;

    if (strlen(whiteListPath) == 0)
    {
        icLogError(LOG_TAG, "parseFiles: no WhiteList path set!");
        return false;
    }

    deviceDescriptors = parseDeviceDescriptors(whiteListPath, blackListPath);

    return result;
}

/*
 * return true if the provided versionInput is in the allowedVersions data structure
 */
static bool versionInRange(const char *versionInput, DeviceVersionList *allowedVersions)
{
    bool result = false;

    if (allowedVersions == NULL)
    {
        //fast fail
        return false;
    }

    switch (allowedVersions->listType)
    {
        case DEVICE_VERSION_LIST_TYPE_WILDCARD:
            result = true;
            break;

        case DEVICE_VERSION_LIST_TYPE_LIST:
            //check to see if the versionInput exactly matches a version in the allowed list (ignoring case)
            if (allowedVersions->list.versionList != NULL)
            {
                icLinkedListIterator *iterator = linkedListIteratorCreate(allowedVersions->list.versionList);
                while (linkedListIteratorHasNext(iterator))
                {
                    char *allowedVersion = linkedListIteratorGetNext(iterator);
                    if (allowedVersion != NULL && stringCompare(allowedVersion, versionInput, true) == 0)
                    {
                        result = true;
                        break;
                    }
                }
                linkedListIteratorDestroy(iterator);
            }
            break;

        case DEVICE_VERSION_LIST_TYPE_RANGE:
            if (allowedVersions->list.versionRange.from != NULL &&
                allowedVersions->list.versionRange.to != NULL)
            {
                if (stringCompare(versionInput, allowedVersions->list.versionRange.from, true) >= 0 &&
                    stringCompare(versionInput, allowedVersions->list.versionRange.to, true) <= 0)
                {
                    result = true;
                }
            }
            break;

        default:
            result = false;
            break;
    }

    return result;
}

char *getWhiteListPath()
{
    if(strlen(whiteListPath) > 0)
    {
        return strdup(whiteListPath);
    }
    else
    {
        return NULL;
    }
}

char *getBlackListPath()
{
    if(strlen(blackListPath) > 0)
    {
        return strdup(blackListPath);
    }
    else
    {
        return NULL;
    }
}

/*
 * Check whether a given white list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param whiteListPath the white list file to check
 * @return true if valid, false otherwise
 */
bool checkWhiteListValid(const char *wlPath)
{
    bool valid = false;
    icLinkedList *descriptors = parseDeviceDescriptors(wlPath, NULL);
    if (descriptors != NULL)
    {
        valid = true;
        linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);
    }

    return valid;
}

/*
 * Check whether a given black list is valid/parsable.  This function does NOT require deviceDescriptorsInit to be
 * called
 * @param blackListPath the black list file to check
 * @return true if valid, false otherwise
 */
bool checkBlackListValid(const char *blPath)
{
    bool valid = false;

    icStringHashMap *blacklistUuids = getBlacklistedUuids(blPath);
    if (blacklistUuids != NULL)
    {
        valid = true;
        stringHashMapDestroy(blacklistUuids, NULL);
    }

    return valid;
}
