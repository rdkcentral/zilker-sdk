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
// Created by wboyd747 on 6/4/18.
//

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <icConcurrent/timedWait.h>
#include <icIpc/baseEvent.h>
#include <icConcurrent/threadUtils.h>

#include "automationTimerTick.h"
#include "automationService.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition;

static pthread_t tickThread;

static bool running;
static automationTimerTickHandler timerFiredHandler;

static void* timerTickThread(void* args)
{
    unsigned long seconds = (unsigned long) args;

    pthread_mutex_lock(&mutex);
    while (running) {
        time_t t;
        int _ret;

        // Get number of seconds remaining in the current minute.
        t = seconds - (time(NULL) % seconds);

        _ret = incrementalCondTimedWait(&condition, &mutex, (int) t);
        if (running && (_ret == ETIMEDOUT)) {
            timerFiredHandler();
        }
    }

    running = false;

    pthread_cond_broadcast(&condition);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void automationStartTimerTick(unsigned long seconds, automationTimerTickHandler handler)
{
    if ((seconds > 0) && handler) {
        pthread_mutex_lock(&mutex);
        if (!running) {
            initTimedWaitCond(&condition);
            timerFiredHandler = handler;

            running = true;

            createThread(&tickThread, timerTickThread, (void *) seconds, "timerTicker");
        }
        pthread_mutex_unlock(&mutex);
    }
}

void automationStopTimerTick(void)
{
    pthread_mutex_lock(&mutex);
    if (running) {
        running = false;

        // Notify the timer tick thread
        pthread_cond_broadcast(&condition);

        // Wait for thread to exit.
        pthread_cond_wait(&condition, &mutex);
    }
    pthread_mutex_unlock(&mutex);
}
