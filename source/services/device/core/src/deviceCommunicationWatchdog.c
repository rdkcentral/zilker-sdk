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

/*
 * Created by Thomas Lea on 9/19/16.
 */

#include "deviceCommunicationWatchdog.h"

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include <icTypes/icHashMap.h>
#include <pthread.h>
#include <icLog/logging.h>
#include <string.h>
#include <icTime/timeUtils.h>
#include <icTypes/icLinkedList.h>
#include <icConcurrent/timedWait.h>
#include <propsMgr/propsService_eventAdapter.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <icConcurrent/threadUtils.h>
#include <icUtil/stringUtils.h>

#define LOG_TAG "deviceCommunicationWatchdog"

#define FAST_COMM_FAIL_PROP "zigbee.testing.fastCommFail.flag"

#define MONITOR_THREAD_SLEEP_SECS 60

typedef struct
{
    char* uuid;
    uint32_t commFailTimeoutSeconds;
    uint64_t lastSuccessfulCommunicationMillis;
    bool inCommFail;
} MonitoredDeviceInfo;

static pthread_mutex_t monitoredDevicesMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t monitorThreadId;
static icHashMap* monitoredDevices = NULL;
static void monitoredDevicesEntryDestroy(void* key, void* value);
static void handlePropertyChangedEvent(cpePropertyEvent *event);

static deviceCommunicationWatchdogCommFailedCallback failedCallback = NULL;
static deviceCommunicationWatchdogCommRestoredCallback restoredCallback = NULL;

static pthread_mutex_t controlMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t controlCond = PTHREAD_COND_INITIALIZER;

static void* commFailWatchdogThreadProc(void* arg);
static bool running = false;
static uint32_t monitorThreadSleepSeconds = MONITOR_THREAD_SLEEP_SECS;
static bool fastCommFailTimer = false;

void deviceCommunicationWatchdogInit(deviceCommunicationWatchdogCommFailedCallback failedcb,
                                     deviceCommunicationWatchdogCommRestoredCallback restoredcb)
{
    if (failedcb == NULL || restoredcb == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    pthread_mutex_lock(&controlMutex);

    if (failedCallback != NULL || restoredCallback != NULL)
    {
        icLogError(LOG_TAG, "%s: already initialized", __FUNCTION__);
        pthread_mutex_unlock(&controlMutex);
        return;
    }

    running = true;

    initTimedWaitCond(&controlCond);
    createThread(&monitorThreadId, commFailWatchdogThreadProc, NULL, "commFailWD");

    monitoredDevices = hashMapCreate();

    failedCallback = failedcb;
    restoredCallback = restoredcb;
    fastCommFailTimer = getPropertyAsBool(FAST_COMM_FAIL_PROP, false);

    pthread_mutex_unlock(&controlMutex);

    register_cpePropertyEvent_eventListener(handlePropertyChangedEvent);
}

void deviceCommunicationWatchdogSetMonitorInterval(uint32_t seconds)
{
    monitorThreadSleepSeconds = seconds;
}

void deviceCommunicationWatchdogTerm()
{
    unregister_cpePropertyEvent_eventListener(handlePropertyChangedEvent);

    pthread_mutex_lock(&controlMutex);

    running = false;

    failedCallback = NULL;
    restoredCallback = NULL;

    pthread_mutex_lock(&monitoredDevicesMutex);
    hashMapDestroy(monitoredDevices, monitoredDevicesEntryDestroy);
    monitoredDevices = NULL;
    pthread_mutex_unlock(&monitoredDevicesMutex);

    pthread_cond_broadcast(&controlCond);

    pthread_mutex_unlock(&controlMutex);

    pthread_join(monitorThreadId, NULL);
}

void deviceCommunicationWatchdogMonitorDevice(const char* uuid, const uint32_t commFailTimeoutSeconds, bool inCommFail)
{
    icLogDebug(LOG_TAG,
               "%s: start monitoring %s with commFailTimeoutSeconds %d, inCommFail %s",
               __FUNCTION__,
               uuid,
               commFailTimeoutSeconds,
               inCommFail ? "true" : "false");

    if (uuid == NULL || commFailTimeoutSeconds == 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    pthread_mutex_lock(&monitoredDevicesMutex);

    if (running == true)
    {
        MonitoredDeviceInfo* info = (MonitoredDeviceInfo*) calloc(1, sizeof(MonitoredDeviceInfo));
        info->uuid = strdup(uuid);
        info->commFailTimeoutSeconds = commFailTimeoutSeconds;
        info->inCommFail = inCommFail;
        info->lastSuccessfulCommunicationMillis = getMonotonicMillis(); //assume ok initially

        // Defensive in case for some reason the device already exists(can happen for device recovery)
        if (hashMapPut(monitoredDevices, info->uuid, (uint16_t) (strlen(info->uuid) + 1), info) == false)
        {
            monitoredDevicesEntryDestroy(info->uuid, info);
            info = NULL;
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: Ignoring monitoring of %s, we aren't running", __FUNCTION__, uuid);
    }

    pthread_mutex_unlock(&monitoredDevicesMutex);
}

void deviceCommunicationWatchdogStopMonitoringDevice(const char* uuid)
{
    icLogDebug(LOG_TAG, "%s: stop monitoring %s", __FUNCTION__, uuid);

    pthread_mutex_lock(&monitoredDevicesMutex);
    hashMapDelete(monitoredDevices, (void*) uuid, (uint16_t) (strlen(uuid) + 1), monitoredDevicesEntryDestroy);
    pthread_mutex_unlock(&monitoredDevicesMutex);
}

void deviceCommunicationWatchdogPetDevice(const char* uuid)
{
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid argument", __FUNCTION__);
        return;
    }

    icLogDebug(LOG_TAG, "%s: petting %s", __FUNCTION__, uuid);

    bool doNotify = false;

    pthread_mutex_lock(&monitoredDevicesMutex);
    MonitoredDeviceInfo* info = (MonitoredDeviceInfo*) hashMapGet(monitoredDevices,
                                                                  (void*) uuid,
                                                                  (uint16_t) (strlen(uuid) + 1));
    if (info != NULL)
    {
        info->lastSuccessfulCommunicationMillis = getMonotonicMillis();
        if (info->inCommFail == true)
        {
            icLogInfo(LOG_TAG, "%s is no longer in comm fail", uuid);
            info->inCommFail = false;
            doNotify = true;
        }
    }
    pthread_mutex_unlock(&monitoredDevicesMutex);

    if (doNotify == true)
    {
        restoredCallback(uuid);
    }
}

void deviceCommunicationWatchdogForceDeviceInCommFail(const char *uuid)
{
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    icLogDebug(LOG_TAG, "%s: forcing device %s to be in comm fail", __FUNCTION__, uuid);

    bool doNotify = false;

    pthread_mutex_lock(&monitoredDevicesMutex);
    MonitoredDeviceInfo *info = (MonitoredDeviceInfo*) hashMapGet(monitoredDevices,
                                                                  (void *) uuid,
                                                                  (uint16_t) (strlen(uuid) + 1));
    if (info != NULL)
    {
        // if device is not in comm fail
        //
        if (info->inCommFail == false)
        {
            // force the device to be in comm fail
            //
            info->inCommFail = true;

            // notify
            //
            doNotify = true;
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: device %s already in comm failure, ignoring", __FUNCTION__, uuid);
        }
    }
    pthread_mutex_unlock(&monitoredDevicesMutex);

    if (doNotify == true)
    {
        failedCallback(uuid);
    }
}

int32_t deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(const char *uuid, uint32_t commFailDelaySeconds)
{
    int32_t retVal = -1;
    if (uuid == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid argument", __FUNCTION__);
        return retVal;
    }

    icLogDebug(LOG_TAG, "%s: getting timeout left for device %s", __FUNCTION__, uuid);

    pthread_mutex_lock(&monitoredDevicesMutex);

    MonitoredDeviceInfo *info = (MonitoredDeviceInfo*) hashMapGet(monitoredDevices,
                                                                  (void *) uuid,
                                                                  (uint16_t) (strlen(uuid) + 1));
    if (info != NULL && info->inCommFail == false)
    {
        // determine how much time is remaining for this device to go into comm fail.
        // this is the amount of time since we last heard from it subtracted from the total
        // comm fail timeout.  If it is already past comm fail time, just fall
        // back to -1 (no more monitoring of it for comm fail).
        //
        uint64_t currentTime = getMonotonicMillis();
        uint64_t diffInSeconds = (currentTime - info->lastSuccessfulCommunicationMillis) / 1000;

        // need to get the time remain before comm fail trouble/alarm should occur
        //
        if (diffInSeconds <= commFailDelaySeconds)
        {
            retVal = (int32_t) (commFailDelaySeconds - diffInSeconds);
        }

        icLogTrace(LOG_TAG, "%s: for device %s the currentTime=%"
                PRIu64" with lastSuccessfulCommunicationTime=%"
                PRIu64", so differenceInSeconds=%"
                PRIu64" and device inCommFail=%s"
                " with commFailDelaySeconds=%"
                PRIu32"; meaning retVal=%"PRId32,
                __FUNCTION__, uuid,
                currentTime,
                info->lastSuccessfulCommunicationMillis,
                diffInSeconds,
                stringValueOfBool(info->inCommFail),
                commFailDelaySeconds,
                retVal);
    }

    pthread_mutex_unlock(&monitoredDevicesMutex);

    return retVal;
}

void deviceCommunicationWatchdogResetTimeoutForDevice(const char *uuid, uint32_t commFailTimeoutSeconds)
{
    if (uuid == NULL || commFailTimeoutSeconds == 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    icLogDebug(LOG_TAG, "%s: setting new timeout %d for device %s", __FUNCTION__, commFailTimeoutSeconds, uuid);

    pthread_mutex_lock(&monitoredDevicesMutex);

    MonitoredDeviceInfo *info = (MonitoredDeviceInfo*) hashMapGet(monitoredDevices,
                                                                  (void *) uuid,
                                                                  (uint16_t) (strlen(uuid) + 1));
    if (info != NULL)
    {
        // set the comm fail timeout seconds
        //
        info->commFailTimeoutSeconds = commFailTimeoutSeconds;

        // if device is not in comm fail
        //
        if (info->inCommFail == false)
        {
            info->lastSuccessfulCommunicationMillis = getMonotonicMillis();  // make the assumption that we just heard from the device
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: device %s already in comm failure, ignoring", __FUNCTION__, uuid);
        }
    }

    pthread_mutex_unlock(&monitoredDevicesMutex);
}

static void monitoredDevicesEntryDestroy(void* key, void* value)
{
    MonitoredDeviceInfo* info = (MonitoredDeviceInfo*) value;
    free(info->uuid);
    free(info);
}

static void* commFailWatchdogThreadProc(void* arg)
{
    icLogDebug(LOG_TAG, "%s: starting up", __FUNCTION__);

    while (true)
    {
        pthread_mutex_lock(&controlMutex);
        if (running == false)
        {
            icLogInfo(LOG_TAG, "%s exiting", __FUNCTION__);
            pthread_mutex_unlock(&controlMutex);
            break;
        }
        if (fastCommFailTimer == false)
        {
            incrementalCondTimedWait(&controlCond, &controlMutex, monitorThreadSleepSeconds);
        }
        else
        {
            incrementalCondTimedWaitMillis(&controlCond, &controlMutex, monitorThreadSleepSeconds);
        }
        pthread_mutex_unlock(&controlMutex);

        icLogDebug(LOG_TAG, "%s: looking for comm failed devices", __FUNCTION__);

        icLinkedList* uuidsInCommFail = linkedListCreate();

        //iterate over all monitored devices and check to see if any have hit comm fail
        pthread_mutex_lock(&monitoredDevicesMutex);
        icHashMapIterator* it = hashMapIteratorCreate(monitoredDevices);
        while (hashMapIteratorHasNext(it))
        {
            uint64_t currentTimeMillis = getMonotonicMillis();
            char* uuid;
            uint16_t uuidLen;
            MonitoredDeviceInfo* info;
            if (hashMapIteratorGetNext(it, (void**) &uuid, &uuidLen, (void**) &info))
            {
                icLogTrace(LOG_TAG, "%s: checking on %s", __FUNCTION__, uuid);
                uint64_t timeoutMillis = ((uint64_t)info->commFailTimeoutSeconds) * 1000;
                if (fastCommFailTimer == true)
                {
                    timeoutMillis = info->commFailTimeoutSeconds / 100;
                }
                if (((currentTimeMillis - info->lastSuccessfulCommunicationMillis) > timeoutMillis)
                    && info->inCommFail == false)
                {
                    icLogWarn(LOG_TAG, "%s: %s is in comm fail", __FUNCTION__, uuid);
                    info->inCommFail = true;
                    linkedListAppend(uuidsInCommFail, strdup(uuid));
                }

                icLogTrace(LOG_TAG, "%s: for device %s the currentTime=%"
                           PRIu64" with lastSuccessfulCommunicationTime=%"
                           PRIu64" and having timeout=%"
                           PRIu64"; meaning device inCommFail=%s",
                           __FUNCTION__, uuid,
                           currentTimeMillis,
                           info->lastSuccessfulCommunicationMillis,
                           timeoutMillis,
                           stringValueOfBool(info->inCommFail));
            }
        }
        hashMapIteratorDestroy(it);
        pthread_mutex_unlock(&monitoredDevicesMutex);

        icLinkedListIterator* iterator = linkedListIteratorCreate(uuidsInCommFail);
        while (linkedListIteratorHasNext(iterator))
        {
            char* uuid = (char*) linkedListIteratorGetNext(iterator);
            icLogDebug(LOG_TAG, "%s: notifying callback of comm fail on %s", __FUNCTION__, uuid);
            failedCallback(uuid);
        }
        linkedListIteratorDestroy(iterator);
        linkedListDestroy(uuidsInCommFail, NULL);
    }

    return NULL;
}

static void handlePropertyChangedEvent(cpePropertyEvent *event)
{
    if (strcmp(event->propKey, FAST_COMM_FAIL_PROP) == 0)
    {

        if (event->baseEvent.eventValue != GENERIC_PROP_DELETED)
        {
            fastCommFailTimer = getPropertyEventAsBool(event, false);
        }
        else
        {
            fastCommFailTimer = false;
        }
        pthread_mutex_lock(&controlMutex);
        pthread_cond_broadcast(&controlCond);
        pthread_mutex_unlock(&controlMutex);
    }
}
