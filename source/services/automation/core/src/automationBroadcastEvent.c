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
// Created by wboyd747 on 6/3/18.
//

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <icIpc/eventProducer.h>
#include <automationService/automationService_event.h>
#include <icLog/logging.h>
#include <inttypes.h>
#include <memory.h>

#include "automationBroadcastEvent.h"
#include "automationService.h"

static EventProducer producer = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void startAutomationEventProducer()
{
    pthread_mutex_lock(&mutex);
    if (producer == NULL) {
        producer = initEventProducer(AUTOMATIONSERVICE_EVENT_PORT_NUM);
    }
    pthread_mutex_unlock(&mutex);
}

void stopAutomationEventProducer()
{
    pthread_mutex_lock(&mutex);
    if (producer != NULL) {
        shutdownEventProducer(producer);
        producer = NULL;
    }
    pthread_mutex_unlock(&mutex);
}

static void sendAutomationEvent(int32_t code, const char* id, uint64_t requestId, bool enabled)
{
    pthread_mutex_lock(&mutex);
    if (producer != NULL) {
        cJSON* json;

        icLogDebug(LOG_TAG, "broadcasting automation event; code=%d rule=%s", code, id);

        automationEvent* event = create_automationEvent();

        if (event) {
            event->baseEvent.eventCode = code;
            event->baseEvent.eventValue = 0;

            setEventId(&event->baseEvent);
            setEventTimeToNow(&event->baseEvent);

            event->ruleId = strdup(id);
            event->requestId = requestId;
            event->enabled = enabled;

            json = encode_automationEvent_toJSON(event);

            if (json) {
                broadcastEvent(producer, json);

                cJSON_Delete(json);
            }

            destroy_automationEvent(event);
        }
    }
    pthread_mutex_unlock(&mutex);
}

void broadcastAutomationCreatedEvent(const char* id, uint64_t requestId, bool enabled)
{
    sendAutomationEvent(AUTOMATION_CREATED_EVENT, id, requestId, enabled);
}

void broadcastAutomationDeletedEvent(const char* id, uint64_t requestId)
{
    sendAutomationEvent(AUTOMATION_DELETED_EVENT, id, requestId, false);
}

void broadcastAutomationModifiedEvent(const char* id, uint64_t requestId, bool enabled)
{
    sendAutomationEvent(AUTOMATION_MODIFIED_EVENT, id, requestId, enabled);
}
