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
 * channelManager.c
 *
 * centralized point-of-contact for communicating with servers.
 * contains 1-or-more "channel" objects that know how to speak
 * a specific protocol to a particular server.
 *
 * The picture below shows the flow (goes from left to right)
 * of how internal requests & events are processed and eventually
 * make their way to a hypothetical channel:
 *
 *                              +---------+   +------------+      +----------+
 *                              | Channel |___| TCP / UDP  |<---->|          |
 * +----------+                 | One     |   | Connection |      |          |
 * | Requests |                /+---------+   +------------+      |          |
 * +----------+\   +---------+/ +---------+   +------------+      |          |
 *              \__| Channel |__| Channel |___| HTTPS      |<---->| Cloud    |
 *              /  | Manager |  | Two     |   | Connection |      | Servers  |
 *   +--------+/   +---------+\ +---------+   +------------+      |          |
 *   | Events |                \+---------+   +------------+      |          |
 *   +--------+                 | Channel |___| MQTT       |<---->|          |
 *                              | Three   |   | Connection |      |          |
 *                              +---------+   +------------+      +----------+
 *
 * Author: jelderton - 9/10/15
 *-----------------------------------------------*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icSystem/hardwareCapabilities.h>
#include <icSystem/runtimeAttributes.h>
#include <icIpc/eventIdSequence.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/delayedTask.h>
#include <icSystem/softwareCapabilities.h>
#include <icTypes/icHashMap.h>
#include <icUtil/stringUtils.h>
#include <commMgr/commService_eventAdapter.h>
#include <propsMgr/propsService_event.h>
#include <propsMgr/propsService_eventAdapter.h>
#include <propsMgr/propsHelper.h>
#include "channelManager.h"
#include "channel.h"
#include "commServiceEventBroadcaster.h"
#include "commServiceCommon.h"
#include "sample/sampleChannel.h"

#define CHANNEL_MAP_KEYLEN  sizeof(uint8_t)

static void startupWindowCallback(void *arg);

/*
 * private variables
 */

// NOTE:
// NOTE: no mutexes are used on our map because we don't allocate/deallocate the
// NOTE: channel objects on the fly.  they are only created during startup
// NOTE: and destroyed during shutdown
// NOTE:
static icHashMap *channelMap;            // hash of channel objects (indexed by uin84_t)

// set of variables for the 'startup window'.  primarily used to defer any
// communication loss events during service startup
static pthread_mutex_t STARTUP_WINDOW_MTX  = PTHREAD_MUTEX_INITIALIZER;
static bool      inStartupWindow = true;                // will remain true until our startup window expires
static uint32_t  startupWindowTask = 0;
static uint8_t   primaryChannelId = SAMPLE_CHANNEL_ID;  // FIXME: populate with a real channel

/*
 * one-time initialization that is generally called during startup
 */
void initChannelManager()
{
    // create the hashtable to hold our channel objects.  since each
    // has a uniqueue identifier, we'll use that as the index into the hash
    //
    channelMap = hashMapCreate();

    // start a timer to clear our 'startup window' flag.  used to prevent
    // sending out TOTAL_COMM_LOST events prematurely (being nice to the UI folks)
    //
    pthread_mutex_lock(&STARTUP_WINDOW_MTX);
    inStartupWindow = true;
    startupWindowTask = scheduleDelayTask(1, DELAY_MINS, startupWindowCallback, NULL);
    pthread_mutex_unlock(&STARTUP_WINDOW_MTX);

    // create the sampleChannel.  it can enable/disable itself based
    // on the settings provided from the cloud
    //
    channel *sample = createSampleChannel();
    hashMapPut(channelMap, &sample->id, CHANNEL_MAP_KEYLEN, sample);

    // TODO: create other channels
}

/*
 * called during shutdown
 */
void shutdownChannelManager()
{
    // stop startup window task if its still there
    //
    pthread_mutex_lock(&STARTUP_WINDOW_MTX);
    if (startupWindowTask != 0)
    {
        cancelDelayTask(startupWindowTask);
        inStartupWindow = false;
    }
    startupWindowTask = 0;
    pthread_mutex_unlock(&STARTUP_WINDOW_MTX);

    // loop through each channel and tell it to shutdown
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        uint16_t keyLen;
        void* key;
        void* value;

        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // pass along regardless if enabled or not
            channel *next = (channel *)value;
            if (next->shutdownFunc != NULL)
            {
                next->shutdownFunc();
            }
        }
    }
    hashMapIteratorDestroy(loop);

    // clear our hash.  we're assuming each channel did all
    // of it's own memory cleanup during the 'shutdown'
    //
    if (channelMap)
    {
        hashMapDestroy(channelMap, NULL);
    }
    channelMap = NULL;
}

/*
 * return true if we are within the startup window
 */
bool inChannelStartupWindow()
{
    pthread_mutex_lock(&STARTUP_WINDOW_MTX);
    bool retVal = inStartupWindow;
    pthread_mutex_unlock(&STARTUP_WINDOW_MTX);

    return retVal;
}

/*
 * return the channel with this unique identifier.
 * can be NULL if the channel is not supported or created
 */
channel *getChannelById(uint8_t channelId)
{
    return (channel *)hashMapGet(channelMap, &channelId, CHANNEL_MAP_KEYLEN);
}

/*
 * returns if any channel is connected to the server.
 * primarily used by "GET_ONLINE_STATUS" IPC handler
 */
bool channelIsAnythingOnline()
{
    bool retVal = false;

    // loop through each channel and get the detailed status
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        uint16_t keyLen;
        void* key;
        void* value;

        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // only pass along if enabled
            channel *next = (channel *) value;
            if (next->getStateFunc != NULL && next->isEnabledFunc() == true)
            {
                channelState state = next->getStateFunc();
                if (state == CHANNEL_STATE_COMPLETE)
                {
                    retVal = true;
                    break;
                }
            }
        }
    }
    hashMapIteratorDestroy(loop);

    return retVal;
}

/*
 * populates the 'output' list with commChannelStatus objects
 * that depict the detailed online status of each known channel.
 * primarily used by "GET_ONLINE_DETAILED_STATUS" IPC handler
 */
void channelGetOnlineDetailedStatus(commChannelStatusList *output)
{
    // loop through each channel and get the detailed status (regardless of enabled or not)
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        uint16_t keyLen;
        void* key;
        void* value;

        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // pass along regardless of enabled flag
            channel *next = (channel *) value;
            if (next->getStatusDetailsFunc != NULL)
            {
                commChannelStatus *obj = create_commChannelStatus();
                next->getStatusDetailsFunc(obj);
                linkedListAppend(output->commStatusList, obj);
            }
        }
    }
    hashMapIteratorDestroy(loop);
}

/*
 * create and broadcast the commOnlineChangedEvent.  it will collect
 * status from all known channels, then send the event with those details
 *
 * @param isHidden - set in the event
 */
void channelSendCommOnlineChangedEvent()
{
    // first, collect the detailed states from each allocated channel
    //
    commOnlineChangedEvent *event = create_commOnlineChangedEvent();
    channelGetOnlineDetailedStatus(event->channelStatusDetailedList);
    event->baseEvent.eventCode = COMM_ONLINE_CHANGED_EVENT;
    event->baseEvent.eventValue = TOTAL_COMM_DISCONNECTED_VALUE;    // assume offline
    event->isHidden = false;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // ask the primary channel for it's bband/cell states to place in the top-level of the event.
    // technically we have that info in the detailed-list from above, but no way to tie it to the primary
    //
    channel *primary = getChannelById(primaryChannelId);
    if (primary != NULL && primary->isEnabledFunc() == true)
    {
        // get the status
        //
        commChannelStatus *status = create_commChannelStatus();
        primary->getStatusDetailsFunc(status);
        event->bbandOnline = status->bbandOnline;
        event->cellOnline = status->cellOnline;

        // now see what to use for the event 'value'
        //
        if (status->cloudAssState == CLOUD_ACCOC_AUTHENTICATED &&
            (status->bbandOnline == true || status->cellOnline == true))
        {
            // one of the subchannels are online, so send "connected"
            //
            event->baseEvent.eventValue = TOTAL_COMM_CONNECTED_VALUE;
        }

        destroy_commChannelStatus(status);
    }

    // if claiming DOWN, suppress if we're within the startup window timeframe
    //
    if (event->baseEvent.eventValue == TOTAL_COMM_DISCONNECTED_VALUE && inChannelStartupWindow() == true)
    {
        icLogWarn(COMM_LOG, "channel: not sending commOnlineChangedEvent; still within the startup window");
    }
    else
    {
        // broadcast the event, then cleanup
        //
        icLogDebug(COMM_LOG, "channel: sending commOnlineChangedEvent; value=%" PRIi32 ", bband=%s, cell=%s",
                   event->baseEvent.eventValue,
                   stringValueOfBool(event->bbandOnline),
                   stringValueOfBool(event->cellOnline));
        broadcastCommOnlineChangedEvent(event);
    }
    destroy_commOnlineChangedEvent(event);
}

/*
 * obtain current status, and shove into the commStatusPojo
 * for external processes to gather details about our state.
 * supports the "GET_SERVICE_STATUS" IPC call to commService.
 */
void getChannelRuntimeStatusIPC(serviceStatusPojo *output)
{
    // loop through each channel and get the detailed status
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        uint16_t keyLen;
        void* key;
        void* value;

        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // only pass along if enabled
            channel *next = (channel *) value;
            if (next->getRuntimeStatusFunc != NULL && next->isEnabledFunc() == true)
            {
                next->getRuntimeStatusFunc(output);
            }
        }
    }
    hashMapIteratorDestroy(loop);
}

/*
 * collect statistics about the messages to/from the server,
 * and populate them into the supplied runtimeStatsPojo container.
 * supports the "GET_RUNTIME_STATS" IPC call to commService.
 */
void collectChannelMessageStatisticsIPC(runtimeStatsPojo *container, bool thenClear)
{
    // loop through each channel and get the statistics
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        uint16_t keyLen;
        void* key;
        void* value;

        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // only pass along if enabled
            channel *next = (channel *) value;
            if (next->getRuntimeStatisticsFunc != NULL && next->isEnabledFunc() == true)
            {
                next->getRuntimeStatisticsFunc(container, thenClear);
            }
        }
    }
    hashMapIteratorDestroy(loop);
}


/*
 * pass along "configuration restored" notification to each
 * of the allocated channels (regardless of enabled state).
 * supports the "CONFIG_RESTORED" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelConfigurationRestoredIPC(configRestoredInput *input)
{
    bool retVal = false;
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    // loop through each channel and call the function if defined
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // pass along regardless if enabled or not
            channel *next = (channel *) value;
            if (next->configRestoredIpcFunc != NULL)
            {
                if (next->configRestoredIpcFunc(input) == true)
                {
                    // at least one was successful
                    retVal = true;
                }
            }
        }
    }
    hashMapIteratorDestroy(loop);
    return retVal;
}

/**
 * Destructor for channelTestResult types
 * @param testResult
 */
void destroyChannelTestResult(channelTestResult *testResult)
{
    if (testResult != NULL)
    {
        free(testResult->channelId);
        free(testResult);
    }
}

/*
 * Requests each channel to perform a connection test and return the results in the form of a list of
 * channelTestResults. Caller is responsible for freeing the list and the channelTestResults inside.
 *
 * @see destroyChannelTestResult
 */
icLinkedList *channelPerformConnectionTests(bool useCell, bool primaryOnly)
{
    icLinkedList *retVal = linkedListCreate();
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    // loop through each channel and call the function if defined
    sbIcHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // perform channel test regardless if enabled or not
            channel *next = (channel *) value;
            if (primaryOnly == false || next->id == primaryChannelId)
            {
                if (next->performConnectionTestFunc != NULL)
                {
                    bool worked = next->performConnectionTestFunc(useCell);
                    channelTestResult *result = calloc(1, sizeof(channelTestResult));
                    result->succeeded = worked;
                    if (next->getStatusDetailsFunc != NULL)
                    {
                        AUTO_CLEAN(pojo_destroy__auto) commChannelStatus *status = create_commChannelStatus();
                        next->getStatusDetailsFunc(status);
                        if (status->channelId != NULL)
                        {
                            result->channelId = strdup(status->channelId);
                        }
                    }

                    linkedListAppend(retVal, result);
                }
            }
        }
    }
    return retVal;
}

/*
 * pass along "configuration reset" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "RESET_COMM_SETTINGS_TO_DEFAULT" IPC call to commService.
 */
void channelConfigurationResetToDefaultsIPC()
{
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    icLogDebug(COMM_LOG, "%s", __FUNCTION__);

    // loop through each channel and call the function if defined
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // pass along regardless if enabled or not
            channel *next = (channel *) value;
            if (next->configResetToDefaultsIpcFunc != NULL)
            {
                next->configResetToDefaultsIpcFunc();
            }
        }
    }
    hashMapIteratorDestroy(loop);
}


/*
 * pass along "get sunrise/sunset" request to each enabled channel.
 * supports the "GET_SUNRISE_SUNSET_TIME" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelGetSunriseSunsetTimeIPC(sunriseSunsetTimes *output)
{
    bool retVal = false;
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    // loop through each channel and call the function if defined
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // only pass along if enabled
            channel *next = (channel *) value;
            if (next->getSunriseSunsetTimeIpcFunc != NULL && next->isEnabledFunc() == true)
            {
                if (next->getSunriseSunsetTimeIpcFunc(output) == true)
                {
                    // at least one was successful
                    retVal = true;
                    break;
                }
            }
        }
    }
    hashMapIteratorDestroy(loop);
    return retVal;
}

/*
 * pass along "send message to subscriber" request to each enabled channel.
 * supports the "SEND_MESSAGE_TO_SUBSCRIBER" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelSendMessageToSubscriberIPC(ruleSendMessage *input)
{
    bool retVal = false;
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    // loop through each channel and call the function if defined
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // only pass along if enabled
            channel *next = (channel *) value;
            if (next->sendMessageToSubscriberIpcFunc != NULL && next->isEnabledFunc() == true)
            {
                if (next->sendMessageToSubscriberIpcFunc(input) == true)
                {
                    // at least one was successful
                    retVal = true;
                }
            }
        }
    }
    hashMapIteratorDestroy(loop);
    return retVal;
}

/*
 * pass along "get cloud association state" request to each enabled channel.
 * supports the "GET_CLOUD_ASSOCIATION_STATE" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 *
 * TODO: look into changing up how this the state is gathered
 */
bool channelGetCloudAssociationStateIPC(cloudAssociationValue *output)
{
    bool retVal = false;
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    // loop through each channel and call the function if defined
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // only pass along if enabled
            channel *next = (channel *) value;
            if (next->getCloudAssociationStateIpcFunc != NULL && next->isEnabledFunc() == true)
            {
                if (next->getCloudAssociationStateIpcFunc(output) == true)
                {
                    // at least one was successful
                    retVal = true;
                    break;
                }
            }
        }
    }
    hashMapIteratorDestroy(loop);
    return retVal;
}

/*
 * pass along "manually start cloud association" request to each enabled channel.
 * supports the "INITIATE_MANUAL_CLOUD_ASSOCIATION" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelStartManualCloudAssociationIPC(cloudAssociationParms *input, cloudAssociationResponse *output)
{
    bool retVal = false;
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    // loop through each channel and call the function if defined
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // only pass along if enabled
            channel *next = (channel *) value;
            if (next->startManualCloudAssociationIpcFunc != NULL && next->isEnabledFunc() == true)
            {
                icLogDebug(COMM_LOG, "asking channel %d to start manual cloud association", next->id);
                if (next->startManualCloudAssociationIpcFunc(input, output) == true)
                {
                    // at least one was successful
                    icLogInfo(COMM_LOG, "channel %d appears to have successfully performed a manual cloud association", next->id);
                    retVal = true;
                    break;
                }
            }
        }
    }
    hashMapIteratorDestroy(loop);
    if (retVal == false)
    {
        icLogWarn(COMM_LOG, "failed to perform manual cloud association");
    }
    return retVal;
}

/*
 * pass along "get hostname configuration" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "GET_HOSTNAME_CONFIG_LIST" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelGetHostnameConfigurationListIPC(commHostConfigList *output)
{
    bool retVal = false;
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    // loop through each channel and call the function if defined
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // pass along regardless if enabled or not
            channel *next = (channel *) value;
            if (next->getHostnameConfigurationListIpcFunc != NULL)
            {
                if (next->getHostnameConfigurationListIpcFunc(output) == true)
                {
                    // at least one was successful
                    retVal = true;
                }
            }
        }
    }
    hashMapIteratorDestroy(loop);
    return retVal;
}

/*
 * pass along "set hostname configuration" request to each
 * of the allocated channels (regardless of enabled state).
 * supports the "SET_HOSTNAME_CONFIG_LIST" IPC call to commService.
 * returns true if at least one channel was able to process the request.
 */
bool channelSetHostnameConfigurationListIPC(commHostConfigList *input)
{
    bool retVal = false;
    uint16_t keyLen = 0;
    void *key = NULL;
    void *value = NULL;

    // loop through each channel and call the function if defined
    //
    icHashMapIterator *loop = hashMapIteratorCreate(channelMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        if (hashMapIteratorGetNext(loop, &key, &keyLen, &value) == true)
        {
            // pass along regardless if enabled or not
            channel *next = (channel *) value;
            if (next->setHostnameConfigurationListIpcFunc != NULL)
            {
                if (next->setHostnameConfigurationListIpcFunc(input) == true)
                {
                    // at least one was successful
                    retVal = true;
                }
            }
        }
    }
    hashMapIteratorDestroy(loop);
    return retVal;
}

/*
 * delayed task function called when our startup window
 * timer has expired.  at a min, need to disable our flag.
 */
static void startupWindowCallback(void *arg)
{
    // turn off the flag and clear the timer handle
    //
    icLogDebug(COMM_LOG, "channel: startup window complete");
    pthread_mutex_lock(&STARTUP_WINDOW_MTX);
    inStartupWindow = false;
    startupWindowTask = 0;
    pthread_mutex_unlock(&STARTUP_WINDOW_MTX);

    // collect the status of our channels and send the initial event
    //
    icLogDebug(COMM_LOG, "channel: checking status of primary channel at end of startup window");
    channelSendCommOnlineChangedEvent();
}


