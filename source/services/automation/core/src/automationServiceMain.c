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
 * automationServiceMain.c
 *
 * Entry point for the automationService process.
 *
 * automationService is responsible for managing and running automation specifications
 *
 * Author: tlea - 03/29/2018
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icBuildtime.h>
#include <icLog/logging.h>           // logging subsystem
#include <icIpc/ipcReceiver.h>
#include <propsMgr/logLevel.h>
#include <propsMgr/timezone.h>
#include "automationService.h"
#include "automationService_ipc_handler.h"
#include "automationBroadcastEvent.h"

#ifdef CONFIG_DEBUG_BREAKPAD
#include <breakpadHelper.h>
#endif

#define AUTOMATION_SERVICE_NAME "automationService"

/*
 * step 1 of the startup sequence:
 * optional callback notification that occurs when
 * it is safe to interact with dependent services.
 * this is triggered by watchdogService directly.
 */
static void serviceInitNotify(void)
{
    icLogDebug(LOG_TAG, "got watchdog call that required services are running");
    automationServiceInitPhase2();
}

/*
 * Program entry point
 */
#ifdef CONFIG_DEBUG_SINGLE_PROCESS
int automationService_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
#ifdef CONFIG_DEBUG_BREAKPAD
    // Setup breakpad support
    breakpadHelperSetup();
#endif

    // init the basics (log, internal structs, event producer)
    initIcLogger();
    autoAdjustCustomLogLevel(AUTOMATION_SERVICE_NAME);
    autoAdjustTimezone();

    // init stuff prior to IPC/Service
    automationServiceInitPhase1();

    // begin the 'service startup sequence', and block until the IPC receiver exits
    startupService_automationService(serviceInitNotify, NULL, NULL, 3, IPC_DEFAULT_MAX_THREADS, IPC_DEFAULT_MAX_QUEUE_SIZE, true);

    // cleanup
    disableAutoAdjustTimezone();
    automationServiceCleanup();
    closeIcLogger();

#ifdef CONFIG_DEBUG_BREAKPAD
    // Cleanup breakpad support
    breakpadHelperCleanup();
#endif

    return EXIT_SUCCESS;
}
