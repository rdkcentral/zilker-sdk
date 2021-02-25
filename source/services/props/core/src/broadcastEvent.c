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
 * broadcastEvent.h
 *
 * Responsible for generating property set/del events
 * and broadcasting them to the listening iControl
 * processes (services & clients)
 *
 * Author: jelderton - 7/7/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#include <icIpc/eventProducer.h>
#include <propsMgr/propsService_event.h>
#include <icLog/logging.h>
#include "broadcastEvent.h"
#include "common.h"

/*
 * local variables
 */
static EventProducer producer = NULL;
static pthread_mutex_t EVENT_MTX = PTHREAD_MUTEX_INITIALIZER;     // mutex for the event producer

/*
 * one-time initialization
 */
void startPropsEventProducer()
{
    // call the EventProducer (from libicIpc) to
    // initialize our producer pointer
    //
    pthread_mutex_lock(&EVENT_MTX);
    if (producer == NULL)
    {
        icLogDebug(PROP_LOG, "starting event producer on port %d", PROPSSERVICE_EVENT_PORT_NUM);
        producer = initEventProducer(PROPSSERVICE_EVENT_PORT_NUM);
    }
    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * shutdown event producer
 */
void stopPropsEventProducer()
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
 * broadcast an "cpePropertyEvent" to any listeners
 *
 * @param eventValue - the event value; should be GENERIC_PROP_ADDED, GENERIC_PROP_DELETED, or GENERIC_PROP_UPDATED
 * @param key - the property name that was added, deleted, or updated
 * @param value - the property value that was added or updated
 * @param version - the internal version - done this way so event generation doesn't have to ask and potentially cause a deadlock
 * @param source - where this change originated from
 */
void broadcastPropertyEvent(int eventValue, const char *key, const char *value, uint64_t version, propSource source)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(PROP_LOG, "unable to broadcast event, producer not initalized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }
    if (eventValue < GENERIC_PROP_ADDED || eventValue > GENERIC_PROP_UPDATED)
    {
        icLogWarn(PROP_LOG, "unable to broadcast event, value %d is outside of min/max bounds", eventValue);
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    // seems bizarre, but since broadcasting wants a JSON string,
    // create and populate an "cpePropertyEvent" struct, then
    // convert it to JSON
    //
    cpePropertyEvent *event = create_cpePropertyEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = GENERIC_PROP_EVENT;
    event->baseEvent.eventValue = eventValue;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the property specific information
    //
    if (key != NULL)
    {
        event->propKey = strdup(key);
    }
    if (value != NULL)
    {
        event->propValue = strdup(value);
    }
    event->source = source;
    event->overallPropsVersion = version;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_cpePropertyEvent_toJSON(event);

    // broadcast the encoded event
    //
    icLogDebug(PROP_LOG, "broadcasting prop event, code=%d value=%d eventId=%" PRIu64, GENERIC_PROP_EVENT, eventValue, event->baseEvent.eventId);
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_cpePropertyEvent(event);
}

