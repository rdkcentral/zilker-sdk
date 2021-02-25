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
 * Responsible for generating upgrade events and
 * broadcasting them to the listening iControl
 * processes (services & clients)
 *
 * Author: jelderton - 7/7/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <icLog/logging.h>                    // logging subsystem
#include <icIpc/eventProducer.h>
#include "broadcastEvent.h"                      // local header
#include "common.h"                              // common set of defines for UpgradeService

/*
 * local variables
 */
static EventProducer producer = NULL;
static pthread_mutex_t EVENT_MTX = PTHREAD_MUTEX_INITIALIZER;     // mutex for the event producer

/*
 * one-time initialization
 */
void startWatchdogEventProducer()
{
    // call the EventProducer (from libicIpc) to
    // initialize our producer pointer
    //
    pthread_mutex_lock(&EVENT_MTX);
    if (producer == NULL)
    {
        icLogDebug(WDOG_LOG, "starting event producer on port %d", WATCHDOGSERVICE_EVENT_PORT_NUM);
        producer = initEventProducer(WATCHDOGSERVICE_EVENT_PORT_NUM);
    }
    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * shutdown event producer
 */
void stopWatchdogEventProducer()
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
 * broadcast an "watchdogEvent" to any listeners
 *
 * @param eventCode - the event code; must be WATCHDOG_INIT_COMPLETE, WATCHDOG_SERVICE_STATE_CHANGED, or WATCHDOG_GROUP_STATE_CHANGED
 * @param eventValue - the event value to better describe the details of the eventCode (START, DEATH, RESTART)
 * @param name - the name of the SERVICE or GROUP the event is about
 */
void broadcastWatchdogEvent(int eventCode, int eventValue, const char *name)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(WDOG_LOG, "unable to broadcast event, producer not initalized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }
    if (eventCode < WATCHDOG_INIT_COMPLETE || eventCode > WATCHDOG_GROUP_STATE_CHANGED)
    {
        icLogWarn(WDOG_LOG, "unable to broadcast event, code %d is outside of min/max bounds", eventCode);
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }
    icLogDebug(WDOG_LOG, "broadcasting event, code=%d value=%d", eventCode, eventValue);

    // seems bizarre, but since broadcasting wants a JSON string,
    // create and populate an "upgradeStatusEvent" struct, then
    // convert it to JSON
    //
    watchdogEvent *event = create_watchdogEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = eventCode;
    event->baseEvent.eventValue = eventValue;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the process specific information
    //
    if (name != NULL)
    {
        event->name = strdup(name);
    }

    // convert to JSON object
    //
    cJSON *jsonNode = encode_watchdogEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanu
    //
    cJSON_Delete(jsonNode);
    destroy_watchdogEvent(event);
}


