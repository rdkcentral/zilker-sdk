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
// Created by mkoch201 on 2/8/19.
//

#include <unistd.h>
#include <inttypes.h>
#include <icConcurrent/repeatingTask.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>

#define LOG_TAG "repeatingTaskTest"

static void taskCallback(void * arg)
{
    const char *str = (char *)arg;
    struct timespec now;
    getCurrentTime(&now, true);
    icLogDebug(LOG_TAG, "Task callback called: %s at sec: %lld, nsec: %lld", str, now.tv_sec, now.tv_nsec);
    // sleep for 50 milliseconds
    usleep(1000*50);
}

static int i = 0;
static bool backOffTaskCallback(void *arg)
{
    const char *str = (char *)arg;
    struct timespec now;
    getCurrentTime(&now, true);
    icLogDebug(LOG_TAG, "Back Off Task callback called: %s on iteration %d at sec: %lld, nsec: %lld",
            str, i + 1, now.tv_sec, now.tv_nsec);

    if (i < 7)
    {
        i++;
        return false;
    }
    else
    {
        i = 0;
        return true;
    }
}

static void successCallback(void *arg)
{
    const char *str = (char *)arg;
    icLogDebug(LOG_TAG, "Back off Success Callback called: for task %s", str);
}

static uint32_t selfChangingHandle = 0;
static void selfChangingCallback(void *arg)
{
    // First run our usual taskCallback
    taskCallback(arg);
    // Now try to change ourselves. If this is working, we should run 10 seconds from now.
    uint32_t newDelaySecs = 10;
    icLogDebug(LOG_TAG, "Changing task to run every %"PRIu32" seconds", newDelaySecs);
    changeRepeatingTask(selfChangingHandle, newDelaySecs, DELAY_SECS, true);
}

/*
 * main
 */
int main(int argc, const char **argv)
{
    icLogDebug(LOG_TAG, "Scheduling start...");
    createRepeatingTask(100, DELAY_MILLIS, taskCallback, "100 Millisecond delay");
    createFixedRateRepeatingTask(100, DELAY_MILLIS, taskCallback, "100 Millisecond fixed");
    createRepeatingTask(1, DELAY_SECS, taskCallback, "1 second delay");
    createRepeatingTask(5, DELAY_SECS, taskCallback, "5 second delay");
    createRepeatingTask(10, DELAY_SECS, taskCallback, "10 second delay");
    createBackOffRepeatingTask(2, 10, 2, DELAY_SECS, backOffTaskCallback, successCallback, "2 sec init, 10 sec max, and 2 sec interval back off");
    selfChangingHandle = createRepeatingTask(5, DELAY_SECS, selfChangingCallback, "5 second delay");

    sleep(25);
}