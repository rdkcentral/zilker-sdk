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
 * broadcastEvent.c
 *
 * Responsible for generating events and broadcasting
 * them to the listening iControl processes (services & clients)
*
 * Author: jelderton - 7/9/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <time.h>

#include <icLog/logging.h>                    // logging subsystem
#include <icIpc/eventProducer.h>
#include <icIpc/eventIdSequence.h>
#include <icSystem/softwareCapabilities.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include <icTypes/icSortedLinkedList.h>
#include <securityService/troubleEventHelper.h>
#include "alarm/systemMode.h"
#include "alarm/alarmPanel.h"
#include "config/securityConfig.h"
#include "common.h"                           // common set of defines for CommService
#include "broadcastEvent.h"

/*
 * private functions
 */
static void addToAlarmEventMap(uint64_t eventId, char *rawEvent);

/*
 * local variables
 */
static pthread_mutex_t EVENT_MTX = PTHREAD_MUTEX_INITIALIZER;     // mutex for the event producer
static EventProducer producer = NULL;
static icHashMap *alarmEventMap = NULL;         // hash of eventId -> string (in json format)

/*
 * one-time initialization
 */
void startSecurityEventProducer()
{
    // call the EventProducer (from libicIpc) to
    // initialize our producer pointer
    //
    pthread_mutex_lock(&EVENT_MTX);
    if (producer == NULL)
    {
        icLogDebug(SECURITY_LOG, "starting event producer on port %d", SECURITYSERVICE_EVENT_PORT_NUM);
        producer = initEventProducer(SECURITYSERVICE_EVENT_PORT_NUM);
    }
    if (alarmEventMap == NULL)
    {
        // make the hash that will keep the unacknowledged events associated with alarms.
        // the concept here is that we will hold onto these events until told the message
        // was delivered (see receivedAlarmMessageDeliveryAcknowledgement).  at any time, a calling
        // process could ask what we have outstanding via provideAlarmMessagesNeedingAcknowledgement
        //
        alarmEventMap = hashMapCreate();
    }
    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * shutdown event producer
 */
void stopSecurityEventProducer()
{
    pthread_mutex_lock(&EVENT_MTX);

    if (producer != NULL)
    {
        shutdownEventProducer(producer);
        producer = NULL;
    }

    // use standard free() function as our hash items are simple objects (uint64, string)
    //
    hashMapDestroy(alarmEventMap, NULL);
    alarmEventMap = NULL;

    pthread_mutex_unlock(&EVENT_MTX);
}

static bool didInit()
{
    bool retVal = false;

    pthread_mutex_lock(&EVENT_MTX);
    if (producer != NULL)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&EVENT_MTX);

    return retVal;
}

/*
 * broadcast an "systemModeChangedEvent" with the eventCode
 * of SYSTEM_MODE_CHANGED_EVENT to any listeners
 *
 * @param oldMode - the string representation of the previous "systemMode"
 * @param newMode - the string representation of the current "systemMode"
 * @param version - the config file version
 * @param requestId - the identifier of the request that caused the mode change
 */
void broadcastSystemModeChangedEvent(char *oldMode, char *newMode, uint64_t version, uint64_t requestId)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast systemModeChangedEvent; producer not initialized");
        return;
    }
    icLogDebug(SECURITY_LOG, "broadcasting systemModeChanged event, old=%s new=%s", oldMode, newMode);

    // seems bizarre, but since broadcasting wants a JSON string,
    // create and populate an "upgradeStatusEvent" struct, then
    // convert it to JSON
    //
    systemModeChangedEvent *event = create_systemModeChangedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = SYSTEM_MODE_CHANGED_EVENT;
    event->baseEvent.eventValue = 0;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // add info specific to this event
    //
    event->previousSystemMode = strdup(oldMode);
    event->currentSystemMode = strdup(newMode);
    event->configVersion = version;
    event->requestId = requestId;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_systemModeChangedEvent_toJSON(event);

    // broadcast the encoded event
    //
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_systemModeChangedEvent(event);
}

/*
 * returns false if the trouble cannot be AUDIBLE right now
 * due to the "deferTroublesConfig" being active.
 *
 * this function is called from broadcastTroubleEvent()
 */
bool canHaveTroubleIndication(troubleEvent *event)
{
    bool retVal = true;

    // The only troubles eligible for deferment are low battery and commfail
    // All other troubles must be indicated
    //
    if (event->trouble->reason != TROUBLE_REASON_BATTERY_LOW &&
        event->trouble->reason != TROUBLE_REASON_COMM_FAIL)
    {
        return retVal;
    }

    // the check for 'defer' status only matters if the trouble has AUDIBLE or VISUAL indications
    //
    if (event->trouble->indication == INDICATION_BOTH ||
        event->trouble->indication == INDICATION_AUDIBLE)
    {
        // see if defer is enabled
        //
        if (isDeferTroublesEnabled() == true)
        {
            // turned on, so check our time window
            //
            deferTroublesConfig *cfg = create_deferTroublesConfig();
            getDeferTroublesConfiguration(cfg);

            // calculate the 'start' time using the hour/min values in the config
            //
            time_t now = getCurrentTime_t(false);
            struct tm start;
            localtime_r((const time_t *)&now, &start);
            start.tm_hour = cfg->deferTroublesStartHour;
            start.tm_min = cfg->deferTroublesStartMinute;
            //For accuracy, set the second value as 0  for start time, while calculating defer trouble start time.
            start.tm_sec = 0;
            time_t startTime = mktime(&start);

            // calculate the 'end' time by adding duration
            //
            time_t endTime = startTime + (60 * 60 * cfg->durationInHours);

            // calculate the previous 'end' time by subtracting 24 hours from the end time
            // this is used for the case where deferred hours passes midnight and the event
            // can fall into the previous deferred hours time window but in the same day
            //
            time_t previousEndTime = endTime - (60 * 60 * 24);

            // see if 'now' is between the start and end time OR if its before the previous end time
            //
            if ((now >= startTime && now <= endTime) || now <= previousEndTime)
            {
                // enabled and within the window.  do not send
                //
                if (isIcLogPriorityDebug() == true)
                {
                    struct tm endObj;
                    localtime_r((const time_t *)&endTime, &endObj);
                    struct tm nowObj;
                    localtime_r((const time_t *)&now, &nowObj);
                    char startStr[64];
                    char endStr[64];
                    char nowStr[64];

                    // print the 3 dates to our log using "mm/dd/yy HH:MM" format
                    //
                    strftime(startStr, 63, "%D %R", &start);
                    strftime(endStr, 63, "%D %R", &endObj);
                    strftime(nowStr, 63, "%D %R", &nowObj);
                    icLogWarn(SECURITY_LOG, "unable to broadcast troubleEvent with an AUDIBLE indication; in 'defer troubles' window; start=%s end=%s now=%s", startStr, endStr, nowStr);
                }

                retVal = false;
            }

            // cleanup
            destroy_deferTroublesConfig(cfg);
            cfg = NULL;
        }
    }

    return retVal;
}

/*
 * broadcast an "troubleEvent" with the eventCode of
 * TROUBLE_OCCURED_EVENT or TROUBLE_CLEARED_EVENT
 *
 * @param event - the troubleEvent to broadcast
 * @param eventCode - TROUBLE_OCCURED_EVENT or TROUBLE_CLEARED_EVENT
 * @param eventValue - the event "value" to include
 *
 * @return indicationType the trouble was broadcast with, returns -1 if the event was not broadcast
 */
indicationType broadcastTroubleEvent(troubleEvent *event, int32_t eventCode, int32_t eventValue)
{
    indicationType retVal = -1;

    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast troubleEvent; producer not initialized");
        return retVal;
    }

    // before sending any trouble, check if our "defer troubles" is turned on and outside of the time window.
    // note that it only matters if the trouble has AUDIBLE or VISUAL indications
    //
    indicationType origIndication = event->trouble->indication;
    if (canHaveTroubleIndication(event) == false)
    {
        // remove the AUDIBLE portion of the indication
        //
        if (event->trouble->indication == INDICATION_BOTH)
        {
            event->trouble->indication = INDICATION_VISUAL;
        }
        else if (event->trouble->indication == INDICATION_AUDIBLE)
        {
            event->trouble->indication = INDICATION_NONE;
        }
    }

    // set the return value now that we know what indication type will be used
    //
    retVal = event->trouble->indication;

    // log what kind of trouble this is
    //
    bool saveInMap = false;
    const char *kind = "unknown";
    switch(eventCode)
    {
        case TROUBLE_OCCURED_EVENT:
            kind = "occured";
            break;

        case TROUBLE_CLEARED_EVENT:
            kind = "cleared";
            event->trouble->restored = true;
            break;

        case TROUBLE_ACKNOWLEDGED_EVENT:
            kind = "ack";
            break;

        case TROUBLE_UNACKNOWLEDGED_EVENT:
            kind = "unack";
            break;

        case TROUBLE_ALARM_SESSION_CODE:
            kind = "alarm session";
            if (event->panelStatus->testModeSecsRemaining == 0)
            {
                // not an alarm test
                saveInMap = true;
            }
            break;
    }
    if (isIcLogPriorityDebug() == true)
    {
        icLogDebug(SECURITY_LOG, "broadcasting troubleEvent (%s); code=%"PRIi32", val=%"PRIi32", eventId=%"PRIu64
                ", troubleId=%"PRIu64", type=%s, critical=%s ind=%s cat=%s alarmSes=%"PRIu64" cid=%s ",
                   kind,
                   eventCode, eventValue,
                   event->baseEvent.eventId,
                   event->trouble->troubleId,
                   troubleTypeLabels[event->trouble->type],
                   troubleCriticalityTypeLabels[event->trouble->critical],
                   indicationTypeLabels[event->trouble->indication],
                   indicationCategoryLabels[event->trouble->indicationGroup],
                   event->alarm->alarmSessionId,
                   stringCoalesce(event->alarm->contactId));
    }

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = eventCode;
    event->baseEvent.eventValue = eventValue;
    if (event->baseEvent.eventId == 0)
    {
        // assign an event id
        setEventId(&event->baseEvent);
    }
    if (event->baseEvent.eventTime.tv_sec == 0)
    {
        // assign to 'now'
        setEventTimeToNow(&event->baseEvent);
    }

    // convert to JSON object
    //
    cJSON *jsonNode = encode_troubleEvent_toJSON(event);

    // broadcast the encoded event
    //
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);

    if (saveInMap == true)
    {
        // save a copy of this event in the hash
        //
        icLogTrace(SECURITY_LOG, "adding trouble event to alarmEventMap; id=%"PRIu64" code=%"PRIi32" val=%"PRIi32,
                event->baseEvent.eventId, event->baseEvent.eventCode, event->baseEvent.eventValue);
        addToAlarmEventMap(event->baseEvent.eventId, cJSON_PrintUnformatted(jsonNode));
    }
    pthread_mutex_unlock(&EVENT_MTX);

    // restore original indication, then cleanup memory
    //
    event->trouble->indication = origIndication;
    cJSON_Delete(jsonNode);

    return retVal;
}

/*
 * broadcast a "securityZoneEvent"
 * @param event - the zone event to send
 * @param eventCode - the event "code" to include
 * @param eventValue - the event "value" to include
 * @param requestId - the identifier of the request that caused the change
 */
void broadcastZoneEvent(securityZoneEvent *event, int32_t eventCode, int32_t eventValue, uint64_t requestId)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast securityZoneEvent; producer not initialized");
        return;
    }

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = eventCode;
    event->baseEvent.eventValue = eventValue;
    event->requestId = requestId;
    if (event->baseEvent.eventId == 0)
    {
        // assign an event id
        setEventId(&event->baseEvent);
    }
    if (event->baseEvent.eventTime.tv_sec == 0)
    {
        // assign to 'now'
        setEventTimeToNow(&event->baseEvent);
    }

    icLogDebug(SECURITY_LOG, "broadcasting securityZoneEvent, eventId=%"PRIu64", zoneNum=%"PRIu32", zoneLabel=%s, "
                             "code=%"PRIi32", value=%"PRIi32", alarmSes=%"PRIu64" cid=%s ind=%s",
               event->baseEvent.eventId,
               event->zone->zoneNumber,
               (event->zone->label != NULL) ? event->zone->label : "N/A",
               event->baseEvent.eventCode, event->baseEvent.eventValue,
               event->alarm->alarmSessionId,
               stringCoalesce(event->alarm->contactId),
               indicationTypeLabels[event->indication]);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_securityZoneEvent_toJSON(event);

    // broadcast the encoded event
    //
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);

    if (eventCode == ZONE_EVENT_ALARM_SESSION_CODE && event->panelStatus->testModeSecsRemaining == 0)
    {
        // not a test alarm, so save a copy of this event in the hash
        //
        icLogTrace(SECURITY_LOG, "adding zone event to alarmEventMap; id=%"PRIu64" code=%"PRIi32" val=%"PRIi32,
                   event->baseEvent.eventId, event->baseEvent.eventCode, event->baseEvent.eventValue);
        addToAlarmEventMap(event->baseEvent.eventId, cJSON_PrintUnformatted(jsonNode));
    }
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
}

/*
 * broadcast a "zoneDiscoveredEvent"
 * @param zoneNumber - the zone number for the discovered zone
 * @param details - the device discovery details to put into the event
 */
void broadcastZoneDiscoveredEvent(uint32_t zoneNumber, DSEarlyDeviceDiscoveryDetails *details)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast zoneDiscoveredEvent; producer not initialized");
        return;
    }
    if (details == NULL)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast zoneDiscoveredEvent; device discovery details is NULL");
        return;
    }

    securityZoneDiscoveredEvent *event = create_securityZoneDiscoveredEvent();
    event->baseEvent.eventCode = ZONE_EVENT_DISCOVERED;
    setEventId(&(event->baseEvent));
    setEventTimeToNow(&(event->baseEvent));
    // Set zone number
    event->discoverDetails->zoneNumber = zoneNumber;
    // Populate the rest of the details from the source details
    event->discoverDetails->deviceId = strdup(details->id);
    event->discoverDetails->deviceClass = strdup(details->deviceClass);
    event->discoverDetails->manufacturer = strdup(details->manufacturer);
    event->discoverDetails->model = strdup(details->model);
    event->discoverDetails->firmwareVersion = strdup(details->firmwareVersion);
    event->discoverDetails->hardwareVersion = strdup(details->hardwareVersion);
    icHashMapIterator *iter = hashMapIteratorCreate(details->metadataValuesMap);
    while(hashMapIteratorHasNext(iter))
    {
        char *key;
        uint16_t keyLen;
        char *value;
        hashMapIteratorGetNext(iter, (void**)&key, &keyLen, (void**)&value);
        // This takes care of populating both values and types map
        put_item_in_securityZoneDiscoveryDetails_metadata(event->discoverDetails, key, value);
    }
    hashMapIteratorDestroy(iter);

    icLogDebug(SECURITY_LOG, "broadcasting zoneDiscoveredEvent, eventId=%"PRIu64", zoneNum=%"PRIu32", deviceId=%s",
               event->baseEvent.eventId,
               zoneNumber,
               event->discoverDetails->deviceId);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_securityZoneDiscoveredEvent_toJSON(event);

    // broadcast the encoded event
    //
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_securityZoneDiscoveredEvent(event);
}

/*
 * broadcast a zoneReorderedEvent
 * @param zones - zones that have been reordered
 */
void broadcastZoneReorderedEvent(icLinkedList *zones)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast securityZoneEvent; producer not initialized");
        return;
    }

    // create the event object
    //
    securityZoneReorderEvent *event = create_securityZoneReorderEvent();
    event->baseEvent.eventCode = ZONE_EVENT_REORDER_CODE;
    event->baseEvent.eventValue = 0;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);
    linkedListDestroy(event->zoneList->zoneArray, (linkedListItemFreeFunc)destroy_securityZone);
    event->zoneList->zoneArray = zones;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_securityZoneReorderEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(SECURITY_LOG, "broadcasting zoneReorderEvent, eventId=%"PRIu64, event->baseEvent.eventId);
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    event->zoneList->zoneArray = NULL;
    destroy_securityZoneReorderEvent(event);
}

/*
 * broadcast a zoneRemovedEvent
 * @param event - the populated event to broadcast
 */
void broadcastZonesRemovedEvent(securityZonesRemovedEvent *event)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast securityZoneEvent; producer not initialized");
        return;
    }

    // finish setting things on the event object
    //
    event->baseEvent.eventValue = 0;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_securityZonesRemovedEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(SECURITY_LOG, "broadcasting zonesRemovedEvent, eventId=%"PRIu64, event->baseEvent.eventId);
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
}

/*
 * send "arming" event
 */
void broadcastArmingEvent(uint32_t eventCode, uint32_t eventValue,
                          systemPanelStatus *status,     // required
                          armSourceType source,          // required
                          const char *userCode,
                          uint32_t remainSecs,
                          bool anyZonesFaulted,
                          indicationType eventIndication)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast armingEvent; producer not initialized");
        return;
    }

    // create the event object
    //
    armingEvent *event = create_armingEvent();
    event->baseEvent.eventCode = eventCode;
    event->baseEvent.eventValue = eventValue;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // fill in what we were provided
    //
    bool freeStatus = true;
    if (status != NULL)
    {
        destroy_systemPanelStatus(event->panelStatus);
        event->panelStatus = status;
        freeStatus = false;
    }
    event->armSource = source;
    event->indication = eventIndication;
    event->exitDelay = remainSecs;
    event->userCode = (char *)userCode;
    event->isZonesFaulted = anyZonesFaulted;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_armingEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(SECURITY_LOG, "broadcasting armingEvent; eventId=%"PRIu64" status=%s arm=%s remain=%"PRIu16,
               event->baseEvent.eventId,
               (status != NULL ? alarmStatusTypeLabels[status->alarmStatus] : "unknown"),
               (status != NULL ? armModeTypeLabels[status->armMode] : "unknown"), remainSecs);
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    if (freeStatus == false)
    {
        event->panelStatus = NULL;
    }
    event->userCode = NULL;     // not our mem to delete
    destroy_armingEvent(event);
}

/*
 * send "armed" event
 */
void broadcastArmedEvent(uint32_t eventValue,
                         systemPanelStatus *status,     // required
                         armSourceType source,          // required
                         armModeType requestedArmMode,  // original arm mode request
                         const char *userCode,
                         bool isRearmed,
                         bool didZonesFault,
                         indicationType eventIndication)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast armedEvent; producer not initialized");
        return;
    }

    // create the event object
    //
    armedEvent *event = create_armedEvent();
    event->baseEvent.eventCode = ALARM_EVENT_ARMED;
    event->baseEvent.eventValue = eventValue;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // fill in what we were provided
    //
    bool freeStatus = true;
    if (status != NULL)
    {
        destroy_systemPanelStatus(event->panelStatus);
        event->panelStatus = status;
        freeStatus = false;
    }
    event->armSource = source;
    event->requestedArmMode = requestedArmMode;
    event->indication = eventIndication;
    event->didZonesFaulted = didZonesFault;
    event->isReArmed = isRearmed;
    event->userCode = (char *)userCode;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_armedEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(SECURITY_LOG, "broadcasting armedEvent; eventId=%"PRIu64" status=%s arm=%s",
               event->baseEvent.eventId,
               (status != NULL ? alarmStatusTypeLabels[status->alarmStatus] : "unknown"),
               (status != NULL ? armModeTypeLabels[status->armMode] : "unknown"));
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    if (freeStatus == false)
    {
        event->panelStatus = NULL;
    }
    event->userCode = NULL;     // not our mem to delete
    destroy_armedEvent(event);
}

/*
 * send "entry delay" event
 */
void broadcastEntryDelayEvent(uint32_t eventCode, uint32_t eventValue,
                              systemPanelStatus *status,
                              armSourceType source,          // required
                              const char *userCode,
                              uint16_t entryDelaySecs,
                              indicationType eventIndication,
                              bool anyZonesFaulted,
                              bool isExitError)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast entryDelay; producer not initialized");
        return;
    }

    // create the event object
    //
    entryDelayEvent *event = create_entryDelayEvent();
    event->baseEvent.eventCode = eventCode;
    event->baseEvent.eventValue = eventValue;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // fill in what we were provided
    //
    bool freeStatus = true;
    if (status != NULL)
    {
        destroy_systemPanelStatus(event->panelStatus);
        event->panelStatus = status;
        freeStatus = false;
    }
    event->armSource = source;
    event->indication = eventIndication;
    event->isExitError = isExitError;
    event->userCode = (char *)userCode;
    event->entryDelay = entryDelaySecs;
    event->isZonesFaulted = anyZonesFaulted;

    // TODO: fill in the zone number that caused entry delay to begin

    // convert to JSON object
    //
    cJSON *jsonNode = encode_entryDelayEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(SECURITY_LOG, "broadcasting entryDelayEvent; eventId=%"PRIu64" status=%s arm=%s",
               event->baseEvent.eventId,
               (status != NULL ? alarmStatusTypeLabels[status->alarmStatus] : "unknown"),
               (status != NULL ? armModeTypeLabels[status->armMode] : "unknown"));
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    if (freeStatus == false)
    {
        event->panelStatus = NULL;
    }
    event->userCode = NULL;     // not our mem to delete
    destroy_entryDelayEvent(event);
}

/*
 * send "disarmed" event
 */
void broadcastDisarmedEvent(systemPanelStatus *status,
                            const char *userCode,
                            armSourceType disarmSource,
                            bool anyZonesFaulted,
                            indicationType eventIndication)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast alarmEvent; producer not initialized");
        return;
    }

    // create the event object
    //
    disarmEvent *event = create_disarmEvent();
    event->baseEvent.eventCode = ALARM_EVENT_DISARMED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // fill in what we were provided
    //
    event->disarmSource = disarmSource;
    //event->previousArmMode = who-knows?  TODO: previous arm mode
    event->userCode = (char *)userCode;
    destroy_systemPanelStatus(event->panelStatus);
    event->panelStatus = status;
    event->isZonesFaulted = anyZonesFaulted;
    event->indication = eventIndication;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_disarmEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(SECURITY_LOG, "broadcasting disarmEvent; eventId=%"PRIu64" status=%s arm=%s",
               event->baseEvent.eventId,
               (status != NULL ? alarmStatusTypeLabels[status->alarmStatus] : "unknown"),
               (status != NULL ? armModeTypeLabels[status->armMode] : "unknown"));
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    event->panelStatus = NULL;
    event->userCode = NULL;
    destroy_disarmEvent(event);
}

/*
 * send "alarm" event.  used for a variety of codes:
 *  ALARM_EVENT_STATE_READY
 *  ALARM_EVENT_STATE_NOT_READY
 *  ALARM_EVENT_ALARM
 *  ALARM_EVENT_ALARM_CLEAR
 *  ALARM_EVENT_ALARM_CANCELLED
 *  ALARM_EVENT_ALARM_RESET
 *  ALARM_EVENT_PANIC
 *  ALARM_EVENT_TEST_MODE
 *  ALARM_EVENT_ACKNOWLEDGED
 *  ALARM_EVENT_SEND_ALARM
 */
void broadcastAlarmEvent(uint32_t eventCode, uint32_t eventValue,
                         systemPanelStatus *status,     // required
                         armSourceType source,          // required
                         alarmDetails *alarmInfo,
                         securityZone *zone,            // supplied when a zone caused the alarm
                         alarmPanicType panicType,
                         armSourceType panicSource,
                         indicationType alarmIndication)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast alarmEvent; producer not initialized");
        return;
    }

    // create the event object
    //
    alarmEvent *event = create_alarmEvent();
    event->baseEvent.eventCode = eventCode;
    event->baseEvent.eventValue = eventValue;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // fill in what we were provided
    //
    bool freeStatus = true;
    if (status != NULL)
    {
        // use supplied panel status
        destroy_systemPanelStatus(event->panelStatus);
        event->panelStatus = status;
        freeStatus = false;
    }
    event->armSource = source;
    event->panicType = panicType;
    event->panicSource = panicSource;
    event->indication = alarmIndication;

    bool freeInfo = true;
    alarmReasonType printReason = ALARM_REASON_NONE;
    if (alarmInfo != NULL)
    {
        // use supplied alarm details
        printReason = alarmInfo->alarmReason;
        destroy_alarmDetails(event->alarm);
        event->alarm = alarmInfo;
        freeInfo = false;
    }

    // since the zone is totally optional, just clear out the
    // stub that is assigned  when the event was allocated
    //
    destroy_securityZone(event->zone);
    event->zone = zone;     // ok if NULL or not-NULL

    // convert to JSON object
    //
    cJSON *jsonNode = encode_alarmEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(SECURITY_LOG, "broadcasting alarmEvent; eventId=%"PRIu64" code=%"PRIi32" value=%"PRIi32
                             " status=%s arm=%s reason=%s immediate=%s alarmSes=%"PRIu64" cid=%s",
            event->baseEvent.eventId, eventCode, eventValue,
            (status != NULL ? alarmStatusTypeLabels[status->alarmStatus] : "unknown"),
            (status != NULL ? armModeTypeLabels[status->armMode] : "unknown"),
            alarmReasonTypeLabels[printReason],
            stringValueOfBool(event->alarm->sendImmediately),
            event->alarm->alarmSessionId,
            stringCoalesce(event->alarm->contactId));
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);

    bool saveInMap = false;
    if (event->panelStatus->testModeSecsRemaining == 0)
    {
        // not in test mode, so look at the event code to see if we save this or not
        //
        switch(event->baseEvent.eventCode)
        {
            case ALARM_EVENT_ALARM:
            case ALARM_EVENT_ALARM_CLEAR:
            case ALARM_EVENT_ALARM_CANCELLED:
            case ALARM_EVENT_ALARM_RESET:
            case ALARM_EVENT_PANIC:
            case ALARM_EVENT_SEND_ALARM:
                saveInMap = true;
                break;

            case ALARM_EVENT_TEST_MODE:
            case ALARM_EVENT_STATE_READY:
            case ALARM_EVENT_STATE_NOT_READY:
            case ALARM_EVENT_ACKNOWLEDGED:
            default:
                break;
        }
    }

    if (saveInMap == true)
    {
        icLogTrace(SECURITY_LOG, "adding alarm event to alarmEventMap; id=%" PRIu64 " code=%" PRIi32 " val=%" PRIi32,
                   event->baseEvent.eventId, event->baseEvent.eventCode, event->baseEvent.eventValue);
        addToAlarmEventMap(event->baseEvent.eventId, cJSON_PrintUnformatted(jsonNode));
    }
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    if (freeStatus == false)
    {
        event->panelStatus = NULL;
    }
    if (freeInfo == false)
    {
        event->alarm = NULL;
    }

    // already freed zone, so make it NULL so we don't free the supplied one
    //
    event->zone = NULL;

    destroy_alarmEvent(event);
}

/*
 * send user add/update/delete event.  eventCode should be one of:
 * ALARM_EVENT_USER_CODE_ADDED, ALARM_EVENT_USER_CODE_MOD, or ALARM_EVENT_USER_CODE_DEL
 */
void broadcastUserCodeEvent(uint32_t eventCode, uint64_t configVersion, keypadUserCode *user, armSourceType source)
{
    // perform sanity checks
    //
    if (didInit() == false)
    {
        icLogWarn(SECURITY_LOG, "unable to broadcast userCodeChangedEvent; producer not initialized");
        return;
    }

    // create the event object
    //
    userCodeChangedEvent *event = create_userCodeChangedEvent();
    event->baseEvent.eventCode = eventCode;
    event->baseEvent.eventValue = 0;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // destroy the pre-allocated userCode we're about to set
    //
    destroy_keypadUserCode(event->userCode);
    event->userCode = user;

    event->source = source;
    event->version = configVersion;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_userCodeChangedEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(SECURITY_LOG, "broadcasting userCodeChangedEvent, eventId=%"PRIu64, event->baseEvent.eventId);
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    event->userCode = NULL; // not our memory to free
    destroy_userCodeChangedEvent(event);
}

/*
 * called when commService has delivered an alarm event for us.
 * this will clear that cached alarm from memory, preventing it from
 * being returned via provideAlarmMessagesNeedingAcknowledgement()
 */
void receivedAlarmMessageDeliveryAcknowledgement(uint64_t alarmEventId)
{
    pthread_mutex_lock(&EVENT_MTX);
    if (alarmEventMap != NULL)
    {
        // remove (and destroy) the hash entry for this eventId.
        // no need for a custom free function, as our values are strings
        //
        hashMapDelete(alarmEventMap, &alarmEventId, sizeof(uint64_t), NULL);
    }
    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * linkedListSortCompareFunc to sort uint64_t items ascending
 */
static int8_t sortEventsById(void *newItem, void *element)
{
    uint64_t *newValue = (uint64_t *)newItem;
    uint64_t *currValue = (uint64_t *)element;

    if (*newValue > *currValue)
    {
        return 1;
    }
    else if (*newValue < *currValue)
    {
        return -1;
    }
    return 0;
}

/*
 * populates the supplied list with RAW JSON of each alarm event broadcasted
 * that has NOT been acknowledged via receivedAlarmMessageDeliveryAcknowledgement()
 *
 * the list will sort these by eventId so caller can replay them in the order they were generated
 */
void provideAlarmMessagesNeedingAcknowledgement(icLinkedList *targetList)
{
    pthread_mutex_lock(&EVENT_MTX);
    if (hashMapCount(alarmEventMap) > 0)
    {
        // iterate through the hash, adding each eventId to a sorted list, then
        // use that to populate the targetList in the proper order
        //
        icSortedLinkedList *sortedList = sortedLinkedListCreate();

        // loop through the hash, adding each 'key' into the sorted list
        //
        icHashMapIterator *hashLoop = hashMapIteratorCreate(alarmEventMap);
        while (hashMapIteratorHasNext(hashLoop) == true)
        {
            void *key;
            uint16_t keyLen;
            void *val;

            if (hashMapIteratorGetNext(hashLoop, &key, &keyLen, &val) == true)
            {
                // add the key to our sorted list
                //
                uint64_t *eventId = (uint64_t *)key;
                sortedLinkedListAdd(sortedList, eventId, sortEventsById);
            }
        }
        hashMapIteratorDestroy(hashLoop);

        // now loop through the sorted list, using the key to extract the raw event
        //
        icLinkedListIterator *listLoop = linkedListIteratorCreate(sortedList);
        while (linkedListIteratorHasNext(listLoop) == true)
        {
            // get the event, then find the correlating json
            //
            uint64_t *nextEventId = (uint64_t *)linkedListIteratorGetNext(listLoop);
            char *json = hashMapGet(alarmEventMap, nextEventId, sizeof(uint64_t));
            if (json != NULL)
            {
                // add the raw event to the output list
                //
                linkedListAppend(targetList, strdup(json));
            }
        }
        linkedListIteratorDestroy(listLoop);

        // delete our sorted list, but not the elements as they belong to the hash
        //
        linkedListDestroy(sortedList, standardDoNotFreeFunc);
    }
    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * used to report the number of ourstanding alarm messages we have that have
 * not been acknowledged yet.
 */
uint32_t getAlarmMessagesNeedingAcknowledgementCount()
{
    pthread_mutex_lock(&EVENT_MTX);
    uint32_t retVal = hashMapCount(alarmEventMap);
    pthread_mutex_unlock(&EVENT_MTX);

    return retVal;
}

/*
 * inserts the supplied string into the alarmEventMap hash, using
 * the eventId as the index
 * assumes caller holds the EVENT_MTX
 */
static void addToAlarmEventMap(uint64_t eventId, char *rawEvent)
{
    if (alarmEventMap != NULL)
    {
        // create mem for the key
        //
        uint64_t *key = malloc(sizeof(uint64_t));
        *key = eventId;

        // add cloned key and the supplied string to the hash
        //
        if (hashMapPut(alarmEventMap, key, sizeof(uint64_t), rawEvent) == false)
        {
            // failed to add, cleanup mem
            free(key);
            free(rawEvent);
            rawEvent = NULL;
        }
    }
    else
    {
        // failed to add, cleanup
        //
        free(rawEvent);
        rawEvent = NULL;
    }
}

