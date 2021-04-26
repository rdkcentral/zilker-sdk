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
 * Entry point for the watchdogService process.
 * Utilizes IPC and Events (via libicIpc)
 * to launch and monitor iControl processes
 *
 * Author: jelderton - 6/23/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icIpc/eventIdSequence.h>
#include <icLog/logging.h>           // logging subsystem
#include <icTime/timeUtils.h>           // for geting the start time
#include <propsMgr/paths.h>             // for default path information
#include <propsMgr/timezone.h>
#include <propsMgr/logLevel.h>
#include <watchdog/serviceStatsHelper.h>
#include <icBuildtime.h>
#include <errno.h>
#include <signal.h>
#include "configParser.h"               // local config file
#include "ipcHandler.h"
#include "procMgr.h"                    // start/monitor processes
#include "common.h"
#include "broadcastEvent.h"

#define INIT_TRY_MAX 5

/**
 * Time to wait between attempts to initialize the event ID sequence
 * @note 100ms
 */
const struct timespec INIT_RETRY_WAIT = { .tv_sec = 0, .tv_nsec = 100 * 1000 * 1000 };

#ifdef CONFIG_DEBUG_BREAKPAD
#include <breakpadHelper.h>
#endif

/*
 * private function declarations
 */
static void printUsage();

/*
 * Program entry point
 */
#ifdef CONFIG_DEBUG_SINGLE_PROCESS
int watchdogService_main(int argc, char *const *argv)
#else
int main(int argc, char *argv[])
#endif
{
#ifdef CONFIG_DEBUG_BREAKPAD
    // Setup breakpad support
    breakpadHelperSetup();
#endif

    int statusCode = EXIT_SUCCESS;
    int c = 0;
    char *configDir = NULL;
    char *homeDir = NULL;

    // initialize logging
    //
    initIcLogger();

    // store the start time
    //
    storeWatchdogStartTime(getCurrentUnixTimeMillis());

    // process command line arguments
    //
    while ((c = getopt(argc, argv, "c:h:")) != -1)
    {
        switch(c)
        {
            case 'c':       // configuration dir
            {
                configDir = strdup(optarg);
                // There are places where we fall back to using the env variable, so make sure its set right
                setenv("IC_CONF", configDir, 1);
                icLogDebug(WDOG_LOG, "using supplied configDir %s", configDir);
                break;
            }

            case 'h':       // home dir
            {
                homeDir = strdup(optarg);
                // There are places where we fall back to using the env variable, so make sure its set right
                setenv("IC_HOME", homeDir, 1);
                icLogDebug(WDOG_LOG, "using supplied homeDir %s", homeDir);
                break;
            }

            default:
            {
                fprintf(stderr, "Unexpected option '%c'\n", c);
                free(configDir);
                free(homeDir);
                printUsage();
                closeIcLogger();
                return EXIT_FAILURE;
            }
        }
    }

    // use default paths if none were supplied
    //
    if (configDir == NULL)
    {
        configDir = strdup(DEFAULT_DYNAMIC_PATH);
        icLogDebug(WDOG_LOG, "using default configDir %s", configDir);
    }
    if (homeDir == NULL)
    {
        homeDir = strdup(DEFAULT_STATIC_PATH);
        icLogDebug(WDOG_LOG, "using default homeDir %s", homeDir);
    }

    // establish shared memory for eventId sequencing
    // we want this done here (before other processes start)
    // to ensure the semaphore and shared-memory are
    // established for use by all of our processes
    //
    uint64_t eventId = 0;
    uint8_t i = 0;
    while (eventId == 0 && i < INIT_TRY_MAX)
    {
        eventId = getNextEventId();

        if (eventId == 0)
        {
            icLogWarn(WDOG_LOG, "Failed to set up event counter: %s", strerror(errno));
            if (nanosleep(&INIT_RETRY_WAIT, NULL) == -1)
            {
                // Interrupted by a signal (e.g., terminated)
                icLogError(WDOG_LOG, "Event ID initialization cancelled: %s", strerror(errno));
                statusCode = EXIT_FAILURE;
                goto exit;
            }
        }

        i++;
    }

    if (eventId == 0)
    {
        icLogError(WDOG_LOG, "Failed to set up event counter!");
        goto exit;
    }

    // create an event producer
    //
    startWatchdogEventProducer();
    autoAdjustTimezone();
    autoAdjustCustomLogLevel(WATCH_DOG_SERVICE_NAME);

    // begin the 'service startup sequence', but do NOT block
    //
    startupService_watchdogService(NULL, NULL, NULL, IPC_DEFAULT_MIN_THREADS, IPC_DEFAULT_MAX_THREADS, 15, false);

    // finally, start all of the processes defined in our config
    // file, and monitor their lifecycle.  this will not return until
    // told to exit via IPC shutdown
    //
    startConfiguredProcessesAndWait((const char *)configDir, (const char *)homeDir);

    // cleanup
    disableAutoAdjustTimezone();

    exit:
    free(configDir);
    free(homeDir);

    closeIcLogger();

#ifdef CONFIG_DEBUG_BREAKPAD
    // Cleanup breakpad support
    breakpadHelperCleanup();
#endif

    return statusCode;
}

/*
 * show user available options
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  watchdog [-c config-dir] [-h home-dir]\n");
    fprintf(stderr, "    -c - set the 'configuration directory' (default: %s)\n", DEFAULT_DYNAMIC_PATH);
    fprintf(stderr, "    -h - set the 'home directory'          (default: %s)\n", DEFAULT_STATIC_PATH);
    fprintf(stderr, "\n");
}

