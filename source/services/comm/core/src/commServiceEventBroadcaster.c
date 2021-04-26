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
 * commServiceEventBrodcaster.c
 *
 * Responsible for generating communication events
 * and broadcasting them to the listening iControl
 * processes (services & clients)
 *
 * Author: jelderton - 7/6/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <pthread.h>

#include <icLog/logging.h>                   // logging subsystem
#include <icIpc/eventProducer.h>
#include <icUtil/stringUtils.h>
#include "commServiceEventBroadcaster.h"
#include "commServiceCommon.h"

/*
 * local variables
 */
static EventProducer producer = NULL;
static pthread_mutex_t EVENT_MTX = PTHREAD_MUTEX_INITIALIZER;     // mutex for the event producer

/*
 * one-time initialization
 */
void startCommEventProducer()
{
    // call the EventProducer (from libicIpc) to
    // initialize our producer pointer
    //
    pthread_mutex_lock(&EVENT_MTX);
    if (producer == NULL)
    {
        icLogDebug(COMM_LOG, "starting event producer on port %d", COMMSERVICE_EVENT_PORT_NUM);
        producer = initEventProducer(COMMSERVICE_EVENT_PORT_NUM);
    }
    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * internal check to see that we have an event producer.
 * will grab the EVENT_MTX lock on it's own
 */
static bool haveProducer()
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
 * shutdown event producer
 */
void stopCommEventProducer()
{
    pthread_mutex_lock(&EVENT_MTX);

    if (producer != NULL)
    {
        shutdownEventProducer(producer);
        producer = NULL;
    }

    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * broadcast an "commOnlineChangedEvent" with the eventCode
 * of COMM_ONLINE_EVENT to any listeners
 */
void broadcastCommOnlineChangedEvent(commOnlineChangedEvent *event)
{
    if (haveProducer() == false)
    {
        icLogWarn(COMM_LOG, "unable to broadcast event, producer not initalized");
        return;
    }
    if (event == NULL)
    {
        return;
    }

    // convert to JSON object
    //
    cJSON *jsonNode = encode_commOnlineChangedEvent_toJSON(event);

    // broadcast the encoded event
    //
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
}

/*
 * broadcast an "activationStateChangedEvent" with the eventCode
 * of ACTIVATION_STATE_CHANGED_EVENT to any listeners
 *
 * @param eventValue - the event value to compliment the code of CLOUD_ASSOCIATION_STATE_CHANGED_EVENT
 */
void broadcastCloudAssociationEvent(int32_t eventValue, bool critical, bool wasCell, cloudAssociationState cloudAssState, uint64_t lastActiveMillis)
{
    // perform sanity checks
    //
    if (haveProducer() == false)
    {
        icLogWarn(COMM_LOG, "unable to broadcast event, producer not initalized");
        return;
    }
    if (eventValue > CLOUD_ASSOC_COMPLETED_VALUE)
    {
        icLogWarn(COMM_LOG, "unable to broadcast 'cloudAssociation' event, value %d is outside of bounds", eventValue);
        return;
    }
    icLogDebug(COMM_LOG, "broadcasting CLOUD_ASSOCIATION_STATE_CHANGED_EVENT event, code=%d value=%d state=%s",
               CLOUD_ASSOCIATION_STATE_CHANGED_EVENT, eventValue, cloudAssociationStateLabels[cloudAssState]);

    // physically broadcast the JSON string representation of the event, so let it do the marshaling
    //
    cloudAssociationStateChangedEvent *event = create_cloudAssociationStateChangedEvent();

    // first set normal 'baseEvent' crud.  note that we do NOT fill in the eventId
    // because we don't want this coming back to commService as something to forward
    // to the server.
    //
    event->baseEvent.eventCode = CLOUD_ASSOCIATION_STATE_CHANGED_EVENT;
    event->baseEvent.eventValue = eventValue;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);
    event->critical = critical;
    event->wasCell = wasCell;
    event->cloudAssState = cloudAssState;
    event->lastAssociationMillis = lastActiveMillis;

    // convert to JSON object, then send
    //
    cJSON *jsonNode = encode_cloudAssociationStateChangedEvent_toJSON(event);
    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_cloudAssociationStateChangedEvent(event);
}

/**
 * Broadcast a media uploaded event to the system.
 *
 * @param eventType The type of media broadcast.
 * @param ruleId The original rule ID (if any) associated with this event.
 * @param requestId The original event ID made during the request. May be zero.
 * @param uploadId If the original event ID is zero then a new unique event ID
 * will be created and used. Otherwise, the original event ID is placed here.
 */
void broadcastMediaUploadedEvent(mediaUploadEventType eventType, uint64_t ruleId, uint64_t requestId, uint64_t uploadId)
{
    cJSON* json;
    mediaUploadedEvent* event;

    // perform sanity checks
    //
    if (haveProducer() == false)
    {
        icLogWarn(COMM_LOG, "unable to broadcast event, producer not initalized");
        return;
    }

    icLogDebug(COMM_LOG, "broadcasting MEDIA_UPLOADED_EVENT event, code=%d id=%"PRIu64, MEDIA_UPLOADED_EVENT, requestId);
    event = create_mediaUploadedEvent();
    event->baseEvent.eventCode = MEDIA_UPLOADED_EVENT;

    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    event->mediaType = eventType;
    event->ruleId = ruleId;
    event->requestEventId = requestId;
    event->uploadEventId = uploadId;

    json = encode_mediaUploadedEvent_toJSON(event);

    pthread_mutex_lock(&EVENT_MTX);
    broadcastEvent(producer, json);
    pthread_mutex_unlock(&EVENT_MTX);

    cJSON_Delete(json);
    destroy_mediaUploadedEvent(event);
}

