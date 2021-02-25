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
 * replayTracker.h
 *
 * Buckets of troubles that need to be re-broadcasted at certain
 * intervals to re-announce troubles that have not cleared.
 * The troubles are placed in buckets based on the source of the
 * trouble (life safety, burg, system or IoT).
 *
 * See https://etwiki.sys.comcast.net/display/CPE/Categorizing++Troubles for
 * details on bucket definitions, behavior, and properties available.
 *
 * Author: jelderton -  2/25/19.
 *-----------------------------------------------*/

#ifndef ZILKER_REPLAYTRACKER_H
#define ZILKER_REPLAYTRACKER_H

#include <stdlib.h>
#include <stdbool.h>
#include <securityService/securityService_event.h>

/*
 * initialize the replay trackers.
 * assumes the global SECURITY_MTX is already held.
 */
void initTroubleReplayTrackers();

/*
 * cleanup the replay trackers (during shutdown).
 * assumes the global SECURITY_MTX is already held.
 */
void shutdownTroubleReplayTrackers();

/*
 * called by troubleState to possibly begin replay tracking
 * because there is now a trouble in the system.
 * assumes the global SECURITY_MTX is already held.
 */
void startTroubleReplayTrackers(troubleEvent *event);

/*
 * called by troubleState to possibly cancel replay tracking
 * because there are NO troubles in the system.
 * assumes the global SECURITY_MTX is already held.
 */
void stopTroubleReplayTrackers();

/*
 * called by troubleState to reset the 'time' on a category/bucket
 * because there are NO troubles in the system for this category.
 * assumes the global SECURITY_MTX is already held.
 */
void resetCategoryReplayTracker(indicationCategory category);

#endif // ZILKER_REPLAYTRACKER_H
