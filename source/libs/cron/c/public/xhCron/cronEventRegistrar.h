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
// Created by mkoch201 on 2/20/19.
//

#ifndef ZILKER_CRONEVENTREGISTRAR_H
#define ZILKER_CRONEVENTREGISTRAR_H

#include <stdbool.h>

/*
 * the definifition of a cron "schedule" (as defined in crontab man page)
 *    field          allowed values
 *    -----          --------------
 *    minute         0-59
 *    hour           0-23
 *    day of month   1-31
 *    month          1-12 (or names, see below)
 *    day of week    0-7 (0 or 7 is Sun, or use names)
 */

/**
 * Callback function for when a cron event occurs
 * Return true if it needs to unregister for the event and remove the cron schedule
 * That can be used for one-time scheduled events
 */
typedef bool (*cronEventHandler)(const char *name);

/**
 * Register to receive a callback when a cron event happens
 *
 * @param name the globally unique name for this entry, use serviceName to help with uniqueness
 * @param schedule the cron schedule, can be NULL to try and just listen for an existing cron
 * tab entry, though this is atypical.  Most services should be passing the schedule they want
 * so if its not there yet it will be created.
 * @param callback the callback for when the cron entry fires
 * @return true if successfully registered
 */
bool registerForCronEvent(const char *name, const char *schedule, cronEventHandler callback);

/**
 * Update the schedule for a cron event
 *
 * @param name the unique name
 * @param schedule the new cron schedule
 * @return true if updated, false if not found
 */
bool updateCronEventSchedule(const char *name, const char *schedule);

/**
 * Unregister to receive events for the given name
 *
 * @param name the name originally registered under
 * @param clearCronEntry if true the cron entry will be removed from the cron tab
 */
void unregisterForCronEvent(const char *name, bool clearCronEntry);

#endif //ZILKER_CRONEVENTREGISTRAR_H
