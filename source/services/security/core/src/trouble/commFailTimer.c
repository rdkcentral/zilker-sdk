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
 * commFailTimer.c
 *
 * Facility to track devices that have reported comm failure, but
 * should not be considered a trouble until the time has surpassed
 * the TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY or
 * the TOUCHSCREEN_SENSOR_COMMFAIL_ALARM_DELAY property values.
 *
 * Author: jelderton -  6/20/19.
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <icLog/logging.h>
#include <icTypes/icLinkedList.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include <icConcurrent/repeatingTask.h>
#include <commonDeviceDefs.h>
#include <deviceHelper.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/propsService_eventAdapter.h>
#include "trouble/commFailTimer.h"
#include "securityProps.h"
#include "internal.h"
#include "common.h"

#define LOG_PREFIX          "commFailTimer:"
#define SECONDS_IN_A_MINUTE 60
#define FAST_COMM_FAIL_PROP "security.testing.fastCommFail.flag"

// object stored in our trackList
typedef struct {
    char *deviceId;             // device we're tracking
    commFailTimerType type;     // what we're tracking (trouble or alarm)
    commFailCallback callback;  // function to call
} trackDevice;

// private functions
static void deleteTrackDeviceFromList(void *item);
static void processCommFailTrackListTask(void *arg);
static uint64_t getLastContactedTimeMillis(const DSDevice *device);
static bool findDeviceFromTrackList(void *searchVal, void *item);
static const char *printCommFailTimerType(commFailTimerType type);
static void handlePropertyChangedEvent(cpePropertyEvent *event);
static void scheduleCommFailTask();

// private variables
static icLinkedList *trackList = NULL;      // list of trackDevice objects
static uint32_t trackTimerTickHandle = 0;   // repeating task to process trackList every minute
static bool fastCommFail = false;

/*
 * initialize the commFailTimer
 * assumes the global SECURITY_MTX is already held.
 */
void initCommFailTimer()
{
    // create our list and a recurring task
    //
    trackList = linkedListCreate();
    fastCommFail = getPropertyAsBool(FAST_COMM_FAIL_PROP, false);
    scheduleCommFailTask();
    register_cpePropertyEvent_eventListener(handlePropertyChangedEvent);
}

/*
 * shutdown and cleanup the commFailTimer
 * assumes the global SECURITY_MTX is already held.
 */
void shutdownCommFailTimer()
{
    // cancel the repeating task
    //
    if (trackTimerTickHandle > 0)
    {
        cancelRepeatingTask(trackTimerTickHandle);
        trackTimerTickHandle = 0;
    }

    // destroy our list
    //
    linkedListDestroy(trackList, deleteTrackDeviceFromList);
    trackList = NULL;
}

/*
 * add a device to the commFailTimer.  requires which "trouble"
 * we wish to track (TROUBLE_DELAY or ALARM_DELAY).  once the
 * device reaches the threshold, it will send a notification
 * to the supplied callback function.
 * assumes the global SECURITY_MTX is already held.
 */
bool startCommFailTimer(const char *deviceId, commFailTimerType type, commFailCallback callback)
{
    if (deviceId == NULL)
    {
        return false;
    }

    // see if we have this device in our list (for this type).
    //
    if (hasCommFailTimer(deviceId, type) == false)
    {
        // create the object to add to our trackList
        //
        icLogDebug(SECURITY_LOG, "%s adding device %s to commFailTimer for %s", LOG_PREFIX, deviceId, printCommFailTimerType(type));
        trackDevice *track = (trackDevice *)calloc(1, sizeof(trackDevice));
        track->deviceId = strdup(deviceId);
        track->type = type;
        track->callback = callback;
        linkedListAppend(trackList, track);
        return true;
    }
    return false;
}

/*
 * remove a device from the tracker.  this will occur naturally
 * when the device reaches the threshold and sends a notification.
 * primarily used when the device starts communicating or was
 * removed from the system.
 * assumes the global SECURITY_MTX is already held.
 */
void stopCommFailTimer(const char *deviceId, commFailTimerType type)
{
    if (deviceId == NULL)
    {
        return;
    }

    // find this device in our list (for thiss type) and remove it
    //
    trackDevice *search = (trackDevice *)calloc(1, sizeof(trackDevice));
    search->deviceId = (char *)deviceId;
    search->type = type;
    linkedListDelete(trackList, search, findDeviceFromTrackList, deleteTrackDeviceFromList);

    // cleanup our search object
    //
    search->deviceId = NULL;
    free(search);
}

/*
 * returns if this device is currently being tracked within the commFailTimer
 * assumes the global SECURITY_MTX is already held.
 */
bool hasCommFailTimer(const char *deviceId, commFailTimerType type)
{
    bool retVal = false;

    // sanity check
    //
    if (deviceId == NULL)
    {
        return retVal;
    }

    // find this device in our list (for either type).  if located, see if the
    // provided type matches the the boolean flag
    //
    trackDevice *search = (trackDevice *)calloc(1, sizeof(trackDevice));
    search->deviceId = (char *)deviceId;
    search->type = type;
    if (linkedListFind(trackList, search, findDeviceFromTrackList) != NULL)
    {
        retVal = true;
    }

    // cleanup our search object
    //
    search->deviceId = NULL;
    free(search);
    return retVal;
}

/*
 * uses the "last contacted time" and the associated _DELAY property
 * to see if this device is technically COMM_FAIL (from a trouble standpoint)
 *
 * NOTE: this makes IPC calls to deviceService so requires the global SECURITY_MTX to NOT be held.
 */
bool isDeviceConsideredCommFail(const DSDevice *device, commFailTimerType type)
{
    if (device == NULL || device->deviceClass == NULL)
    {
        return false;
    }

    // Camera and 4g adapter comm fails are different from other devices and are handled by the device driver.
    // If we get here and the device is a camera/m1lte, it's definitely in comm fail.
    //
    if (stringCompare(device->deviceClass, CAMERA_DC, false) == 0)
    {
        return true;
    }

    // get our "timeout" value to use (in minutes)
    //
    uint64_t timeoutMin = 0;
    if (type == TROUBLE_DELAY_TIMER)
    {
        timeoutMin = (uint64_t)getDeviceOfflineCommFailTroubleMinutesProp();
    }
    else
    {
        timeoutMin = (uint64_t)getDeviceOfflineCommFailAlarmTroubleMinutesProp();
    }

    // ask for the last time we contacted this device
    //
    uint64_t lastCommSuccessMillis = getLastContactedTimeMillis(device);

    // do the math on how long ago we communicated with this device
    //
    uint64_t troubleDurationMillis = (timeoutMin * SECONDS_IN_A_MINUTE * 1000);
    if (fastCommFail == true)
    {
        // For fast comm fail use minutes as millis
        troubleDurationMillis = timeoutMin;
    }
    uint64_t now = getCurrentUnixTimeMillis();
    icLogDebug(SECURITY_LOG, "%s check comm fail: using now=%"PRIu64" lastContactTime=%"PRIu64" troubleDurMillis=%"PRIu64,
               LOG_PREFIX, now, lastCommSuccessMillis, troubleDurationMillis);

    if ((now - lastCommSuccessMillis) > troubleDurationMillis)
    {
        // been in comm fail longer then the duration (default 30 min)
        //
        icLogDebug(SECURITY_LOG, "%s check comm fail: in %s!!!!", LOG_PREFIX, printCommFailTimerType(type));
        return true;
    }

    return false;
}

/*
 * ask the device "when" it was contacted last (in millis)
 *
 * NOTE: this makes IPC calls to deviceService so requires the global SECURITY_MTX to NOT be held.
 */
static uint64_t getLastContactedTimeMillis(const DSDevice *device)
{
    uint64_t retVal = 0;

    // ask the device for the 'dateLastContacted' value
    //
    char *lastSpokeStr = NULL;
    if (deviceHelperReadDeviceResource(device->id, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, &lastSpokeStr) == true)
    {
        if (lastSpokeStr != NULL)
        {
            // need to convert from string to long long
            retVal = strtoull(lastSpokeStr, NULL, 10);
        }
        else
        {
            icLogWarn(SECURITY_LOG, "%s got empty resource %s from %s", LOG_PREFIX, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, device->id);
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "%s error getting resource %s from %s", LOG_PREFIX, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, device->id);
    }

    // cleanup and return
    free(lastSpokeStr);
    return retVal;
}

/*
 * function used via 'linkedListFind' to find the trackDevice from
 * the trackList list with a matching deviceId and type.
 * called internally, should assumes the mutex is held.
 */
static bool findDeviceFromTrackList(void *searchVal, void *item)
{
    trackDevice *searchObj = (trackDevice *)searchVal;
    trackDevice *element = (trackDevice *)item;

    // see if the device id and type matches
    //
    if (strcmp(element->deviceId, searchObj->deviceId) == 0 && element->type == searchObj->type)
    {
        return  true;
    }
    return false;
}

/*
 * linkedListItemFreeFunc to destroy trackDevice objects from a linked list
 */
static void deleteTrackDeviceFromList(void *item)
{
    trackDevice *element = (trackDevice *)item;
    free(element->deviceId);
    element->deviceId = NULL;
    element->callback = NULL;
    free(element);
}

/*
 * linkedListCloneItemFunc for duplicating objects within the trackList
 */
static void *cloneTrackListItem(void *item, void *context)
{
    trackDevice *element = (trackDevice *)item;
    trackDevice *copy = (trackDevice *)calloc(1, sizeof(trackDevice));
    copy->deviceId = strdup(element->deviceId);
    copy->type = element->type;
    copy->callback = element->callback;
    return copy;
}

/*
 * taskCallbackFunc to process any devices that are in COMM FAIL,
 * but not long enough to be considered a trouble
 */
static void processCommFailTrackListTask(void *arg)
{
    // called from another thread, so get the global mutex
    // then clone our trackList so we can process each
    // deviceId without holding the lock....
    //
    icLinkedList *target = NULL;
    lockSecurityMutex();
    if (linkedListCount(trackList) > 0)
    {
        target = linkedListDeepClone(trackList, cloneTrackListItem, NULL);
    }
    unlockSecurityMutex();

    if (target == NULL)
    {
        return;
    }

    // got a list of deviceId strings to check for COMM FAIL
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(target);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // need to get the device so we can ask for the 'dateLastContacted'
        //
        trackDevice *curr = (trackDevice *)linkedListIteratorGetNext(loop);
        DSDevice *device = create_DSDevice();
        int rc = deviceService_request_GET_DEVICE_BY_ID(curr->deviceId, device);
        if (rc == IPC_SUCCESS)
        {
            // use the last time we contacted this device
            //
            if (isDeviceConsideredCommFail(device, curr->type) == true)
            {
                // notify our callback function
                //
                if (curr->callback != NULL)
                {
                    icLogDebug(SECURITY_LOG, "%s device %s is in %s; notifying callback", LOG_PREFIX, curr->deviceId, printCommFailTimerType(curr->type));
                    curr->callback((const DSDevice *)device, curr->type);
                }
                else
                {
                    icLogWarn(SECURITY_LOG, "%s device %s is in %s; however nothing to notify", LOG_PREFIX, curr->deviceId, printCommFailTimerType(curr->type));
                }
            }
            else
            {
                icLogDebug(SECURITY_LOG, "%s device %s is still not in %s; will check again later", LOG_PREFIX, curr->deviceId, printCommFailTimerType(curr->type));
            }
        }
        else
        {
            // error getting device
            icLogWarn(SECURITY_LOG, "%s error retrieving DSDevice for id %s; unable to determine %s", LOG_PREFIX, curr->deviceId, printCommFailTimerType(curr->type));
        }
        destroy_DSDevice(device);
    }

    // cleanup
    //
    linkedListIteratorDestroy(loop);
    linkedListDestroy(target, deleteTrackDeviceFromList);
}

/*
 * used for logging
 */
static const char *printCommFailTimerType(commFailTimerType type)
{
    if (type == TROUBLE_DELAY_TIMER)
    {
        return "COMM_FAIL";
    }
    else
    {
        return "COMM_FAIL_ALARM";
    }
}

/*
 * Schedule the comm fail task that checks the list.  Assumes the security mutex is held.  If the task is already
 * scheduled it will be updated to reflect any changes.
 */
static void scheduleCommFailTask()
{
    delayUnits delayUnit = DELAY_MINS;
    if (fastCommFail == true)
    {
        delayUnit = DELAY_SECS;
    }
    if (trackTimerTickHandle != 0)
    {
        changeRepeatingTask(trackTimerTickHandle, 1, delayUnit, true);
    }
    else
    {
        trackTimerTickHandle = createRepeatingTask(1, delayUnit, processCommFailTrackListTask, NULL);
    }
}

static void handlePropertyChangedEvent(cpePropertyEvent *event)
{
    if (strcmp(event->propKey, FAST_COMM_FAIL_PROP) == 0)
    {
        lockSecurityMutex();
        fastCommFail = getPropertyEventAsBool(event, false);
        scheduleCommFailTask();
        unlockSecurityMutex();
    }
}
