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
 * main.c
 *
 * Entry point for the upgradeMgr process.  Utilizes
 * IPC and Events (via libicIpc) to perform
 * upgrades of iControl (and possibly the OS).
 *
 * Author: jelderton - 7/1/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#include <icBuildtime.h>
#include <icLog/logging.h>                      // logging subsystem
#include <propsMgr/timezone.h>                  // keep timezone up-to-date
#include <propsMgr/logLevel.h>                  // adjust to log level changes
#include <commMgr/commService_eventAdapter.h>   // for 'activation' event
#include "backupRestoreService_ipc_handler.h"   // local IPC handler
#include "broadcastEvent.h"                     // local Event producer
#include "backupJob.h"
#include "common.h"

#ifdef CONFIG_DEBUG_BREAKPAD
#include <breakpadHelper.h>
#endif

/*
 * implementation of handleEvent_cloudAssociationStateChangedEvent
 */
static void cloudAssociationStateChangedNotif(cloudAssociationStateChangedEvent *event)
{
    if (event->baseEvent.eventValue == CLOUD_ASSOC_COMPLETED_VALUE)
    {
        // activation state changed, start the backup timer
        //
        icLogInfo(BACKUP_LOG, "activation state changed, potentially starting backup timer");
        scheduleBackupIfPossible();
    }
}

/*
 * step 2 of the startup sequence:
 * optional callback notification that occurs when
 * all services are initialized and ready for use.
 * this is triggered by the WATCHDOG_INIT_COMPLETE
 * event.
 */
static void allServicesAvailableNotify(void)
{
    icLogDebug(BACKUP_LOG, "got watchdog event that all services are running");

    // register for 'cloud association' event (old "activation")
    //
    register_cloudAssociationStateChangedEvent_eventListener(cloudAssociationStateChangedNotif);
}

/*
 * Program entry point
 */
#ifdef CONFIG_DEBUG_SINGLE_PROCESS
int backup_service_main(int argc, const char *argv[])
#else
int main(int argc, const char *argv[])
#endif
{
#ifdef CONFIG_DEBUG_BREAKPAD
    // Setup breakpad support
    breakpadHelperSetup();
#endif

    // initialize logging
    //
    initIcLogger();
    autoAdjustCustomLogLevel(BACKUP_RESTORE_SERVICE_NAME);
    autoAdjustTimezone();

    // setup event producer for broadcasting Upgrade events
    //
    startBackupRestoreEventProducer();

    // begin the 'service startup sequence', and block until the IPC receiver exits
    //
    startupService_backupRestoreService(NULL, allServicesAvailableNotify, NULL, IPC_DEFAULT_MIN_THREADS, IPC_DEFAULT_MAX_THREADS, 15, true);

    // cleanup
    //
    unregister_cloudAssociationStateChangedEvent_eventListener(cloudAssociationStateChangedNotif);
    disableAutoAdjustTimezone();
    cancelScheduledBackup();
    stopBackupRestoreEventProducer();
    closeIcLogger();

#ifdef CONFIG_DEBUG_BREAKPAD
    // Cleanup breakpad support
    breakpadHelperCleanup();
#endif

    return 0;
}


