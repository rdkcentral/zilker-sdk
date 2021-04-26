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
 * backupJob.h
 *
 * Task to perform a 'backup' of our configuration.
 *
 * Similar to the Java counterpart, the idea is to
 * schedule a random time in the future to create a
 * backup of our configuration data, and send it to
 * the server.  Triggered when another Service changes
 * it's config data, this allows us to not perform a
 * backup every time something is changed - instead simply
 * start a timer to execute the backup after a period.
 * This allows lots of changes to occur in a window and
 * supply the server with a single backup to store.
 *
 * Once scheduled, the timer can be canceled in case
 * we need to reset or restart.
 *
 * Certain situations require a 'force backup', where
 * the timer is ignored and we immediately perform the
 * task.
 *
 * Author: jelderton - 7/1/15
 *-----------------------------------------------*/

#ifndef BACKUPJOB_H
#define BACKUPJOB_H

#include <stdint.h>
#include <stdbool.h>

typedef enum
{
    SCHEDULE_HOURS,
    SCHEDULE_MINS,
    SCHEDULE_SECONDS,
} scheduleUnits;

/*
 * Returns if a backup job is currently in progress
 */
bool isBackupRunning();

/*
 * Returns if a backup job is scheduled for later
 */
bool isBackupScheduled();

/*
 * Cancel a backup job that is scheduled
 */
void cancelScheduledBackup();

/*
 * schedule a backup job to run some time in the future
 * will fail if it is already scheduled or running
 */
bool scheduleBackup(uint16_t delay, scheduleUnits units);

/*
 * helper function to choose a random number (1-12) and
 * start the scheduled backup IF it's not already running
 * or scheduled.  More of an atomic operation then:
 *   if ( !isRunning() && !isScheduled() ) { scheduleBackup() }
 */
bool scheduleBackupIfPossible();

/*
 * perform an immediate backup (ignoring the timer).
 * this will block until the backup is complete.
 */
bool forceBackup();

#endif // BACKUPJOB_H
