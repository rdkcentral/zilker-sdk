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
 * timeTracker.h
 *
 * Track the amount of time that is spent on an operation.
 * Handy for checking if operations are taking too long
 * or for simply gathering statistics.
 *
 * Author: jelderton - 9/1/15
 *-----------------------------------------------*/

#ifndef ZILKER_TIMETRACKER_H
#define ZILKER_TIMETRACKER_H

#include <stdint.h>
#include <stdbool.h>

/*
 * define object for tracking time
 */
typedef struct _timeTracker timeTracker;

/*
 * create a new time tracker.  needs to be released
 * via the timeTrackerDestroy() function
 */
timeTracker *timeTrackerCreate();

/*
 * free a time tracker
 */
void timeTrackerDestroy(timeTracker *tracker);

/*
 * starts the timer
 */
void timeTrackerStart(timeTracker *tracker, uint32_t timeoutSecs);

/*
 * stops the timer
 */
void timeTrackerStop(timeTracker *tracker);

/*
 * return is the timer is still going
 */
bool timeTrackerRunning(timeTracker *tracker);

/*
 * return if the amount of time setup for this tracker has expired
 */
bool timeTrackerExpired(timeTracker *tracker);

/*
 * if started, returns number of seconds remaining
 * before considering this timer 'expired'
 */
uint32_t timeTrackerSecondsUntilExpiration(timeTracker *tracker);

/*
 * return the number of seconds the timer ran (or is running)
 */
uint32_t timeTrackerElapsedSeconds(timeTracker *tracker);

void timeTrackerDebug(timeTracker *tracker);

#endif // ZILKER_TIMETRACKER_H
