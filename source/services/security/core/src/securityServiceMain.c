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
 * Entry point for the securityService process.  Utilizes
 * IPC and Events (via libicIpc) to perform
 * basic operations for the CPE (i.e. track scene,
 * troubles, etc).
 *
 * Unlike the Java counterpart, this does not currently
 * support alarms or zones.  That is an exercise for
 * the future.
 *
 * Author: jelderton - 7/9/15
 *-----------------------------------------------*/

#include <string.h>
#include <curl/curl.h>

#include <icBuildtime.h>
#include <icLog/logging.h>                // logging subsystem
#include <icIpc/ipcSender.h>
#include <icIpc/ipcReceiver.h>
#include <icSystem/softwareCapabilities.h>
#include <propsMgr/logLevel.h>
#include <propsMgr/timezone.h>
#include "securityService_ipc_handler.h"    // local IPC handler
#include "broadcastEvent.h"
#include "eventListener.h"
#include "alarm/alarmPanel.h"
#include "alarm/systemMode.h"
#include "trouble/troubleState.h"
#include "zone/securityZone.h"
#include "common.h"
#include "internal.h"
#include "securityProps.h"
#include "securitySystemTracker.h"

#ifdef CONFIG_DEBUG_BREAKPAD
#include <breakpadHelper.h>
#endif

/*
 * step 1 of the startup sequence:
 * optional callback notification that occurs when
 * it is safe to interact with dependent services.
 * this is triggered by watchdogService directly.
 */
static void serviceInitNotify(void)
{
    icLogDebug(SECURITY_LOG, "got watchdog IPC to finalize initialization");

    // load zones
    //
    initSecurityZonesPublic();

    // load initial troubles
    //
    loadInitialTroublesPublic();
    setupSecurityServiceEventListeners();

    // initialize (load) our alarm-state-machine and/or systemMode
    //
    if (supportAlarms() == true)
    {
        initAlarmPanelPublic();
    }
    if (supportSystemMode() == true)
    {
        initSystemMode();
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
    if (supportAlarms() == true)
    {
        // inform alarm state machine that all services are available
        //
        finishInitAlarmPanelPublic();
    }
}

/*
 * Program entry point
 */
#ifdef CONFIG_DEBUG_SINGLE_PROCESS
int securityService_main(int argc, const char *argv[])
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
    autoAdjustCustomLogLevel(SECURITY_SERVICE_NAME);
    autoAdjustTimezone();

    // create our shared task executor
    //
    initSecurityTask();

    // setup event producer for broadcasting events
    //
    startSecurityEventProducer();

    // one-time setup of internal trouble structures
    //
    initSecurityProps();
    initTroubleStatePublic();
    initSecuritySystemTracker();

    // begin the 'service startup sequence', and block until the IPC receiver exits
    //
    startupService_securityService(serviceInitNotify, allServicesAvailableNotify, NULL,
                                   IPC_DEFAULT_MIN_THREADS, IPC_DEFAULT_MAX_THREADS, IPC_DEFAULT_MAX_QUEUE_SIZE, true);

    // cleanup
    //
    if (supportAlarms() == true)
    {
        shutdownAlarmPanelPublic();
    }
    destroySecurityZonesPublic();
    disableAutoAdjustTimezone();
    removeSecurityServiceEventListeners();
    stopSecurityEventProducer();
    destroySecurityTask();
    destroyTroubleStatePublic();
    destroySecuritySystemTracker();
    cleanupSecurityProps();
    closeIcLogger();

#ifdef CONFIG_DEBUG_BREAKPAD
    // Cleanup breakpad support
    breakpadHelperCleanup();
#endif

    return 0;
}


