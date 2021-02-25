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
// Created by wboyd747 on 6/7/18.
//

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <commMgr/commService_pojo.h>
#include <icIpc/ipcMessage.h>
#include <commMgr/commService_ipc.h>
#include <icLog/logging.h>
#include <icConcurrent/delayedTask.h>
#include <icTime/timeUtils.h>

#include "automationService.h"
#include "automationSunTime.h"

#define DEFAULT_SUNRISE_HOUR 7
#define DEFAULT_SUNRISE_MIN 0
#define DEFAULT_SUNSET_HOUR 19
#define DEFAULT_SUNSET_MIN 0

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t refreshSunriseTask = 0;
static time_t sunrise = 0;
static time_t sunset = 0;

static bool running = false;

static void refreshSunriseSunsetCallback(void *arg)
{
    IPCCode rc;

    unsigned long entropy = (unsigned long) arg;
    uint8_t min = (uint8_t) (random() % entropy);

    time_t _sunrise, _sunset;

    sunriseSunsetTimes *resp = create_sunriseSunsetTimes();

    _sunrise = _sunset = 0;

    //request sunrise/sunset from comm
    //
    rc = commService_request_GET_SUNRISE_SUNSET_TIME(resp);
    if (rc == IPC_SUCCESS) {
        _sunrise = convertUnixTimeMillisToTime_t(resp->sunrise);
        _sunset = convertUnixTimeMillisToTime_t(resp->sunset);

        dbg(VERBOSITY_LEVEL_0, "Sunrise: [%s], Sunset [%s]", ctime(&_sunrise), ctime(&_sunset));
    }

    destroy_sunriseSunsetTimes(resp);

    // ensure that the returned sunrise time is greater than 0
    //
    pthread_mutex_lock(&mutex);
    if (_sunrise > 0) {
        sunrise = _sunrise;
    } else {

        //sunrise time is not greater than 0, so we either
        // 1. Use the previous hour and minute from last calculated time
        // 2. Use a default defined as DEFAULT_SUNRISE_HOUR and DEFAULT_SUNRISE_MIN
        //
        time_t now = getCurrentTime_t(false);
        struct tm today;
        localtime_r(&now, &today);
        today.tm_sec = 0;

        // if sunrise is not 0, then we have a previous value
        //
        if(sunrise != 0) {
            dbg(VERBOSITY_LEVEL_2, "Failed to get Sunrise, using previous hour and minute");

            // We have a previous sunrise time, so carry forward the hour and minute
            //
            struct tm sunriseTm;
            localtime_r(&sunrise, &sunriseTm);
            today.tm_hour = sunriseTm.tm_hour;
            today.tm_min = sunriseTm.tm_min;

            sunrise = timelocal(&today);
        } else {

            // no previous value for sunrise, using defaults
            //
            dbg(VERBOSITY_LEVEL_2, "Failed to get Sunrise, using defaults");

            today.tm_hour = DEFAULT_SUNRISE_HOUR;
            today.tm_min = DEFAULT_SUNRISE_MIN;

            sunrise = timelocal(&today);
        }
    }

    // ensure the returned value for sunset is greater than 0
    //
    if (_sunset > 0) {
        sunset = _sunset;
    } else {

        // sunset time was not greater than 0, so we
        // 1.  Use the previous sunset hour and minute from the last calculated time
        // 2.  Use a default defined as DEFAULT_SUNSET_HOUR and DEFAULT_SUNSET_MIN
        //
        time_t now = getCurrentTime_t(false);
        struct tm today;
        localtime_r(&now, &today);
        today.tm_sec = 0;

        // if the current sunset time is greater than 0, then we had a previous value
        //
        if(sunset != 0) {
            dbg(VERBOSITY_LEVEL_2, "Failed to get Sunset, using previous hour and minute");

            // We have a previous sunset time, so carry forward the hour and minute
            //
            struct tm sunsetTm;
            localtime_r(&sunset, &sunsetTm);
            today.tm_hour = sunsetTm.tm_hour;
            today.tm_min = sunsetTm.tm_min;

            sunset = timelocal(&today);
        } else {

            // sunset time was 0, so using the defaults
            //
            dbg(VERBOSITY_LEVEL_2, "Failed to get Sunset, using defaults");

            today.tm_hour = DEFAULT_SUNSET_HOUR;
            today.tm_min = DEFAULT_SUNSET_MIN;

            sunset = timelocal(&today);
        }
    }
    
    // ensure that this was logged at least once, otherwise with lower verbosity, we can't tell when sunrise/sunset is
    // ctime_r requires at least 26 bytes
    // using %.24s leaves off the new line at the end of ctime_r
    //

    char sunriseString[27];
    ctime_r(&sunrise,sunriseString);

    char sunsetString[27];
    ctime_r(&sunset,sunsetString);

    dbg(VERBOSITY_LEVEL_2, "Sunrise: [%.24s], Sunset [%.24s]", sunriseString, sunsetString);


    /* If we do not get a valid time then something is wrong with
     * comm service. Thus we need to try again after a bit, but
     * we want to make sure we are randomized.
     */
    if ((sunrise == 0) || (sunset == 0)) {
        // We don't want to handle zero minutes here.
        refreshSunriseTask = scheduleDelayTask(min + 1, DELAY_MINS, refreshSunriseSunsetCallback, (void*) entropy);
    } else {
        refreshSunriseTask = scheduleTimeOfDayTask(0, min, refreshSunriseSunsetCallback, (void*) entropy);
    }
    pthread_mutex_unlock(&mutex);
}

void automationStartSunMonitor(uint8_t midnightEntropy)
{
    // This is silly, but required to pass warning issues.
    unsigned long entropy = midnightEntropy;

    pthread_mutex_lock(&mutex);
    if (!running) {
        running = true;
        refreshSunriseTask = scheduleDelayTask(30, DELAY_SECS, refreshSunriseSunsetCallback, (void*) entropy);
    }
    pthread_mutex_unlock(&mutex);
}

void automationStopSunMonitor(void)
{
    pthread_mutex_lock(&mutex);
    if (running) {
        running = false;
        cancelDelayTask(refreshSunriseTask);
    }
    pthread_mutex_unlock(&mutex);
}

void automationGetSunTimes(time_t* sunriseTime, time_t* sunsetTime)
{
    pthread_mutex_lock(&mutex);
    *sunriseTime = sunrise;
    *sunsetTime = sunset;
    pthread_mutex_unlock(&mutex);
}
