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
 * commServiceMain.c
 *
 * Entry point for the commService process.  Utilizes
 * IPC and Events (via libicIpc) to perform
 * communication between the CPE and the Server
 *
 * Author: jelderton - 7/6/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <curl/curl.h>

#include <icBuildtime.h>
#include <icLog/logging.h>              // logging subsystem
#include <propsMgr/logLevel.h>
#include <propsMgr/timezone.h>
#include <commMgr/commService_ipc_codes.h>
#include "commServiceEventBroadcaster.h"
#include "channelManager.h"
#include "commServiceCommon.h"
#include "commService_ipc_handler.h"       // local IPC handler

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
    icLogDebug(COMM_LOG, "got watchdog IPC to finalize initialization");

    // start our channels and connections
    //
    initChannelManager();
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
    icLogDebug(COMM_LOG, "got watchdog event that all services are running");
}

/*
 * optional callback notification that gets called
 * when the service is requested to shutdown via IPC
 * (not when we internally call shutdown_commService_ipc_handler)
 *
 * @see handle_commService_SHUTDOWN_SERVICE_request
 */
static void shutdownServiceCallback(void)
{
    // TODO: for comm it may be prudent to wait for un-delivered
    //       alarms to finish before shutting down

}

/*
 * Program entry point
 */
#ifdef CONFIG_DEBUG_SINGLE_PROCESS
int comm_service_main(int argc, const char *argv[])
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
    autoAdjustCustomLogLevel(COMM_SERVICE_NAME);
    autoAdjustTimezone();

    // ignore SIGPIPE; can happen at odd times (like forcing a connection closed,
    // or loosing the upstream connection due to network outage)
    //
    struct sigaction pipeAction;
    sigemptyset(&pipeAction.sa_mask);
    pipeAction.sa_handler = SIG_IGN;
    pipeAction.sa_flags = 0;
    sigaction(SIGPIPE, &pipeAction, NULL);

    // setup event producer for broadcasting communication events
    //
    startCommEventProducer();

    // begin the 'service startup sequence'.  we're going to give the threads/pool
    // a decent amount of room since many clients/services hit commService.
    // this will block until the IPC receiver exits.
    //
    startupService_commService(serviceInitNotify, allServicesAvailableNotify, shutdownServiceCallback,
                               IPC_DEFAULT_MIN_THREADS, IPC_DEFAULT_MAX_THREADS * 2, 50, true);

    // cleanup
    //
    disableAutoAdjustTimezone();
    shutdownChannelManager();
    stopCommEventProducer();
    closeIcLogger();

#ifdef CONFIG_DEBUG_BREAKPAD
    // Cleanup breakpad support
    breakpadHelperCleanup();
#endif

    return 0;
}


