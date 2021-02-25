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
 * restoreJob.h
 *
 * Task to perform a 'restore' of our configuration.
 *
 * Similar to the Java counterpart, this will download our
 * configuration from the server into a temporary directory.
 * Once complete, notify all services so they can import the
 * settings they need from the old configuration files.
 * After all services have completed, message watchdog to restart
 * all of the services (soft boot).
 *
 * Author: jelderton - 4/6/16
 *-----------------------------------------------*/

#ifndef ZILKER_RESTOREJOB_H
#define ZILKER_RESTOREJOB_H

#include <stdbool.h>
#include <icTypes/icLinkedList.h>
#include <backup/backupRestoreService_pojo.h>

/*
 * return if a restore is in progress.  called by IPC
 * handler to not allow backup schedules/runs while restoring
 */
bool isRestoreRunning();

/*
 * starts the restore process.  will populate 'steps'
 * with the well-known strings of the steps this will
 * go through.  each step will have a corresponding
 * 'restoreStep' event signifying when the step is completed/failed.
 *
 * once complete (and successful), this will ask watchdog to restart all services.
 *
 * returns if the restore thread was able to begin.
 */
bool startRestoreProcess(icLinkedList *steps);

/**
 * Tell the restore process that a service has
 * finished its RMA process.
 *
 * @param input The service name and status.
 */
void notifyRestoreCallback(restoreCallbackInfo *input);

#endif // ZILKER_RESTOREJOB_H
