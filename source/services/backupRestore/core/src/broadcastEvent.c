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

#include <icLog/logging.h>                   // logging subsystem
#include <icIpc/eventProducer.h>
#include <backup/backupRestoreService_event.h>
#include "broadcastEvent.h"                     // local header
#include "common.h"                             // common set of defines for UpgradeService

/*
 * local variables
 */
static EventProducer producer = NULL;
static pthread_mutex_t EVENT_MTX = PTHREAD_MUTEX_INITIALIZER;     // mutex for the event producer

/*
 * one-time initialization
 */
void startBackupRestoreEventProducer()
{
    // call the EventProducer (from libicIpc) to
    // initialize our producer pointer
    //
    pthread_mutex_lock(&EVENT_MTX);
    if (producer == NULL)
    {
        icLogDebug(BACKUP_LOG, "starting event producer on port %d", BACKUPRESTORESERVICE_EVENT_PORT_NUM);
        producer = initEventProducer(BACKUPRESTORESERVICE_EVENT_PORT_NUM);
    }
    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * shutdown event producer
 */
void stopBackupRestoreEventProducer()
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
 * broadcast a "restoreStep" event to any listeners
 *
 * @param stepNun  - 'eventValue' to use
 * @param stepName - 'key' of the step we're on (so it can be translated and presented)
 * @param stepSuccess - if the step we're on is successful
 */
void broadcastRestoreEvent(uint32_t stepNum, const char *stepName, bool stepSuccess)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(BACKUP_LOG, "unable to broadcast event, producer not initalized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }
    icLogDebug(BACKUP_LOG, "broadcasting restore event, code=%d value=%d", RESTORE_STEP_CODE, stepNum);

    // seems bizarre, but since broadcasting wants a JSON string,
    // create and populate an "upgradeStatusEvent" struct, then
    // convert it to JSON
    //
    restoreStepEvent *event = create_restoreStepEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = RESTORE_STEP_CODE;
    event->baseEvent.eventValue = stepNum;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the restore specific information
    //
    event->restoreStepKey = strdup(stepName);
    event->restoreStepWorked = stepSuccess;

    // convert to JSON
    //
    cJSON *jsonNode = encode_restoreStepEvent_toJSON(event);

    // broadcast the JSON object
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_restoreStepEvent(event);
}


