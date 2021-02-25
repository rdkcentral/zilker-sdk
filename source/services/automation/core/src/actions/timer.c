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
// Created by tlea on 4/26/18.
//

/*
 * This message target enables automations to send arbitrary ipc requests and events to the local platform.
 */

#include <string.h>
#include <icLog/logging.h>
#include <stdbool.h>
#include <icTypes/icHashMap.h>
#include <pthread.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <ctype.h>
#include <stdio.h>
#include <automationService.h>
#include <ccronexpr.h>
#include <automationAction.h>
#include <automationEngine.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/propsService_eventAdapter.h>
#include "timer.h"

#define IN_PARAM "in"
#define CRON_PARAM "cron"
#define JSON_TIMERID_KEY "timerId"
#define JSON_PRIVATE_KEY "private"

#define FAST_TIMER_ACTION_PROP "automation.testing.fastTimerAction.flag"

typedef struct
{
    cJSON* id;
    cJSON* message;
    timer_t timerId;
} TimerEntry;

static icHashMap* timers = NULL;
static bool fastTimer = false;
static pthread_mutex_t timersMtx = PTHREAD_MUTEX_INITIALIZER;

static void handlePropertyChangedEvent(cpePropertyEvent *event);

static void freeTimerEntry(TimerEntry* timer)
{
    if (timer != NULL) {
        timer_delete(timer->timerId);

        cJSON_Delete(timer->message);
        cJSON_Delete(timer->id);

        free(timer);
    }
}

static void freeTimerEntryFromHash(void* key, void* value)
{
    freeTimerEntry((TimerEntry*) value);
    free(key);
}

static void timerExpiredThreadProc(union sigval arg)
{
    cJSON* params = cJSON_CreateObject();
    cJSON* notification;

    TimerEntry* timer = (TimerEntry*) arg.sival_ptr;

    char* timerId = timer->id->valuestring;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    cJSON_AddItemToObjectCS(params, JSON_TIMERID_KEY, timer->id);

    if (timer->message) {
        cJSON_AddItemToObjectCS(params, JSON_PRIVATE_KEY, timer->message);
    }

    notification = jsonrpc_create_notification("timerFired", params);

    //destroy this timer since it fired
    pthread_mutex_lock(&timersMtx);
    /*
     * The ID and Message JSON objects have been added into the parameters.
     * Thus they will be deleted by either the failure to create the
     * notification or through the normal "post" process.
     *
     * We set the values to NULL so that the "free" routines will
     * not attempt to actually free them.
     */
    timer->id = NULL;
    timer->message = NULL;

    hashMapDelete(timers, timerId, (uint16_t) strlen(timerId), freeTimerEntryFromHash);

    if (notification == NULL) {
        cJSON_Delete(params);
    } else {
        automationEnginePost(notification);
        // The engine post above will copy this json, so delete our copy.
        cJSON_Delete(notification);
    }
    pthread_mutex_unlock(&timersMtx);
}

/* Returning a value of -1 means error */
static time_t getTimeFromCron(const char* cron)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    time_t result = -1;

    // smallest spec is something like 2s.  must start with a digit
    if (cron != NULL) {
        cron_expr expr;
        const char* error = NULL; //do not free any value returned from cron_parse_expr
        cron_parse_expr(cron, &expr, &error);

        if (error == NULL) //success
        {
            time_t now = time(NULL);
            result = cron_next(&expr, now);

            char buf[50]; //manpage says 26 max
            icLogDebug(LOG_TAG, "%s: cron '%s' set to fire on %s", __FUNCTION__, cron, ctime_r(&result, buf));
        } else {
            icLogError(LOG_TAG, "%s: failed to parse cron spec %s", __FUNCTION__, cron);
        }
    }

    return result;
}

static cJSON* makeTimerActionHandler(const cJSON* id, const cJSON* params)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool success = false;
    cJSON* response = NULL;

    unused(id);

    if (params != NULL) {
        cJSON* timerId = cJSON_GetObjectItem(params, JSON_TIMERID_KEY);

        if (timerId != NULL) {
            cJSON* message = cJSON_GetObjectItem(params, JSON_PRIVATE_KEY);
            cJSON* in = cJSON_GetObjectItem(params, IN_PARAM);
            cJSON* cron = cJSON_GetObjectItem(params, CRON_PARAM);

            time_t timerTime = -1;

            if (in != NULL) {
                timerTime = in->valueint;
            } else if (cron != NULL) {
                time_t now = time(NULL);

                timerTime = getTimeFromCron(cron->valuestring);
                if (timerTime != -1) {
                    //if we parsed it correctly, subtract 'now' since the timer is relative
                    timerTime -= now;
                }
            }

            if (timerTime != -1) {
                pthread_mutex_lock(&timersMtx);

                //ensure there isnt already an entry with this id
                size_t idLen = strlen(timerId->valuestring);
                if (hashMapGet(timers, timerId->valuestring, (uint16_t) idLen) == NULL) {
                    struct sigevent se;
                    struct itimerspec ts;

                    TimerEntry* timer = (TimerEntry*) calloc(1, sizeof(TimerEntry));
                    timer->id = cJSON_Duplicate(timerId, true);
                    timer->message = cJSON_Duplicate(message, true);

                    memset(&se, 0, sizeof(se));
                    memset(&ts, 0, sizeof(ts));

                    //we want to be notified from a new thread
                    se.sigev_notify = SIGEV_THREAD;
                    se.sigev_value.sival_ptr = timer;
                    se.sigev_notify_function = timerExpiredThreadProc;
                    se.sigev_notify_attributes = NULL;

                    int rc = timer_create(CLOCK_MONOTONIC, &se, &timer->timerId);
                    if (rc == 0) {
                        // Determine if we should use fast time or not
                        if (fastTimer == true)
                        {
                            // Ludicrous speed!
                            time_t timerTimeMillis = timerTime; // Treat the timer as if it was milliseconds instead of seconds.
                            if (timerTimeMillis < timerTime)
                            {
                                // We overflowed, dangit! I guess just do normal speed?
                                icLogWarn(LOG_TAG, "Unable to use fast timer for duration %ld", timerTime);
                                ts.it_value.tv_sec = timerTime;
                            }
                            else
                            {
                                ts.it_value.tv_sec = timerTimeMillis / 1000;
                                ts.it_value.tv_nsec = (timerTimeMillis % 1000) * 1000 * 1000;
                                icLogDebug(LOG_TAG, "Using LUDICROUS SPEED - sec: %ld, nsec: %ld",
                                        ts.it_value.tv_sec, ts.it_value.tv_nsec);
                            }
                        }
                        else
                        {
                            // Normal speed
                            ts.it_value.tv_sec = timerTime;
                        }

                        success = timer_settime(timer->timerId, 0, &ts, 0) == 0;

                        if (success) {
                            hashMapPut(timers, strdup(timerId->valuestring), (uint16_t) idLen, timer);
                        }
                    }

                    if (!success) {
                        freeTimerEntry(timer);
                    }
                } else {
                    icLogWarn(LOG_TAG, "%s: timer %s already exists", __FUNCTION__, timerId->valuestring);
                }

                pthread_mutex_unlock(&timersMtx);
            }
        }
    } else {
        icLogError(LOG_TAG, "%s: invalid message", __FUNCTION__);
    }

    if (id) {
        if (success) {
            response = jsonrpc_create_response_success(id, NULL);
        } else {
            response = jsonrpc_create_response_error(id,
                                                     -1,
                                                     "Failure to handle make timer action.",
                                                     NULL);
        }
    }

    return response;
}

static cJSON* deleteTimerActionHandler(const cJSON* id, const cJSON* params)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool success = false;
    cJSON* response = NULL;

    unused(id);

    if (params != NULL) {
        cJSON* timerId = cJSON_GetObjectItem(params, JSON_TIMERID_KEY);

        if (timerId != NULL) {
            pthread_mutex_lock(&timersMtx);
            success = hashMapDelete(timers, timerId->valuestring,
                                    (uint16_t) strlen(timerId->valuestring),
                                    freeTimerEntryFromHash);
            pthread_mutex_unlock(&timersMtx);
        }
    } else {
        icLogError(LOG_TAG, "%s: invalid message", __FUNCTION__);
    }

    if (id) {
        if (success) {
            response = jsonrpc_create_response_success(id, NULL);
        } else {
            response = jsonrpc_create_response_error(id,
                                                     -1,
                                                     "Failure to handle delete timer action.",
                                                     NULL);
        }
    }

    return response;
}

static void handlePropertyChangedEvent(cpePropertyEvent *event)
{
    if (strcmp(event->propKey, FAST_TIMER_ACTION_PROP) == 0)
    {
        pthread_mutex_lock(&timersMtx);
        fastTimer = getPropertyEventAsBool(event, false);
        pthread_mutex_unlock(&timersMtx);
    }
}

void timersMessageTargetInit(void)
{
    fastTimer = getPropertyAsBool(FAST_TIMER_ACTION_PROP, false);
    register_cpePropertyEvent_eventListener(handlePropertyChangedEvent);
    timers = hashMapCreate();

    automationActionRegisterOps("makeTimerAction", makeTimerActionHandler);
    automationActionRegisterOps("deleteTimerAction", deleteTimerActionHandler);
}

void timersMessageTargetDestroy(void)
{
    hashMapDestroy(timers, freeTimerEntryFromHash);
    unregister_cpePropertyEvent_eventListener(handlePropertyChangedEvent);
}
