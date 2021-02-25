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
 * timeTracker.c
 *
 * Track the amount of time that is spent on an operation.
 * Handy for checking if operations are taking too long
 * or for simply gathering statistics.
 *
 * Author: jelderton - 9/1/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icTime/timeTracker.h>

#define LOG_TAG "TIME_TRACKER"
/*
 * define object for tracking time
 */
struct _timeTracker
{
    time_t    startTime;
    uint32_t  timeoutSecs;
    bool running;
};

/*
 * private functions
 */
static time_t currentTime();

/*
 * create a new time tracker.  needs to be released
 * via the timeTrackerDestroy() function
 */
timeTracker *timeTrackerCreate()
{
    // create basic struct
    //
    timeTracker *retVal = (timeTracker *)malloc(sizeof(timeTracker));
    if (retVal != NULL)
    {
        memset(retVal, 0, sizeof(timeTracker));
        retVal->running = false;
    }
    return retVal;
}

/*
 * free a time tracker
 */
void timeTrackerDestroy(timeTracker *tracker)
{
    if (tracker != NULL)
    {
        free(tracker);
        tracker = NULL;
    }
}

/*
 * starts the timer
 */
void timeTrackerStart(timeTracker *tracker, uint32_t timeoutSecs)
{
    if (tracker != NULL)
    {
        tracker->timeoutSecs = timeoutSecs;
        tracker->startTime = currentTime();
        tracker->running = true;
    }
}

/*
 * stops the timer
 */
void timeTrackerStop(timeTracker *tracker)
{
    if (tracker != NULL)
    {
        // get the elapsed time since start, and save as 'start'
        // in case something comes back around and asks for the
        // elapsed time (after it was stopped)
        //
        tracker->startTime = timeTrackerElapsedSeconds(tracker);

        // now set the flag
        //
        tracker->running = false;
    }
}

/*
 * return is the timer is still going
 */
bool timeTrackerRunning(timeTracker *tracker)
{
    if (tracker != NULL)
    {
        return tracker->running;
    }
    return false;
}

/*
 * return if the amount of time setup for this tracker has expired
 */
bool timeTrackerExpired(timeTracker *tracker)
{
    if (tracker != NULL && tracker->running == true)
    {
        // get the elapsed seconds, then compare to our timeout
        //
        uint32_t elapsed = timeTrackerElapsedSeconds(tracker);
        if (elapsed >= tracker->timeoutSecs)
        {
            return true;
        }
    }

    return false;
}

/*
 * if started, returns number of seconds remaining
 * before considering this timer 'expired'
 */
uint32_t timeTrackerSecondsUntilExpiration(timeTracker *tracker)
{
    if (tracker != NULL && tracker->running == true)
    {
        // get the elapsed seconds, then subtract from the timeout
        //
        uint32_t elapsed = timeTrackerElapsedSeconds(tracker);
        if (elapsed < tracker->timeoutSecs)
        {
            return (tracker->timeoutSecs - elapsed);
        }

        // already expired, so fall through to return 0
        //
    }
    return 0;
}

/*
 * return the number of seconds the timer ran (or is running)
 */
uint32_t timeTrackerElapsedSeconds(timeTracker *tracker)
{
    uint32_t retVal = 0;
    if (tracker != NULL)
    {
        // if running, diff start from now
        //
        if (tracker->running == true)
        {
            // get the number of seconds between start time and now
            //
            time_t now = currentTime();
            double diff = difftime(now, tracker->startTime);
            retVal = (uint32_t)diff;
        }
        else
        {
            // not running, so just return the 'startTime' as that
            // should be 0 or was set to the duration of the stop - start
            //
            retVal = (uint32_t)tracker->startTime;
        }
    }

    return retVal;
}

void timeTrackerDebug(timeTracker *tracker)
{
    if (tracker == NULL)
    {
        icLogDebug(LOG_TAG, "tracker-dump: tracker is NULL");
        return;
    }

    icLogDebug(LOG_TAG, "tracker-dump: tracker run=%s timeout=%d start=%ld now=%ld elapsed=%d remain=%d",
               (tracker->running == true) ? "true" : "false",
               tracker->timeoutSecs,
               tracker->startTime,
               currentTime(),
               timeTrackerElapsedSeconds(tracker),
               timeTrackerSecondsUntilExpiration(tracker));
}

static time_t currentTime()
{
    struct timespec local;
    getCurrentTime(&local, true);

    return local.tv_sec;
}
