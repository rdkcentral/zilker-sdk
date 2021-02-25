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
 * broadcastEvent.h
 *
 * Responsible for generating upgrade events and
 * broadcasting them to the listening iControl
 * processes (services & clients)
 *
 * Author: jelderton - 7/7/15
 *-----------------------------------------------*/

#ifndef IC_WDOG_BROADCAST_EVENTS_H
#define IC_WDOG_BROADCAST_EVENTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <watchdog/watchdogService_event.h>

/*
 * one-time initialization
 */
void startWatchdogEventProducer();

/*
 * shutdown event producer
 */
void stopWatchdogEventProducer();

/*
 * broadcast an "watchdogEvent" to any listeners
 *
 * @param eventCode - the event code; must be WATCHDOG_INIT_COMPLETE, WATCHDOG_SERVICE_STATE_CHANGED, or WATCHDOG_GROUP_STATE_CHANGED
 * @param eventValue - the event value to better describe the details of the eventCode (START, DEATH, RESTART)
 * @param name - the name of the SERVICE or GROUP the event is about
 */
void broadcastWatchdogEvent(int eventCode, int eventValue, const char *name);

#endif // IC_WDOG_BROADCAST_EVENTS_H
