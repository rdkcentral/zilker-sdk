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
 * Entry point for the propsService process.
 * Utilizes IPC and Events (via libicIpc)
 * to track and report on CPE properties
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icIpc/ipcReceiver.h>
#include <propsMgr/paths.h>
#include <propsMgr/timezone.h>
#include <propsMgr/logLevel.h>
#include "propsService_ipc_handler.h"
#include "broadcastEvent.h"
#include "properties.h"
#include "common.h"
#include "propertyTypeDefinitions.h"

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
int props_service_main(int argc, char *const *argv)
#else
int main(int argc, char *argv[])
#endif
{
#ifdef CONFIG_DEBUG_BREAKPAD
    // Setup breakpad support
    breakpadHelperSetup();
#endif

    int c = 0;
    char *configDir = NULL;
    char *homeDir = NULL;

    // initialize logging
    //
    initIcLogger();

    // process command line arguments
    //
#ifdef CONFIG_DEBUG_SINGLE_PROCESS
    //if we are running as a single process then watchdog already used getopt... reset the optind to the beginning
    optind = 1;
#endif
    while ((c = getopt(argc, argv, "c:h:")) != -1)
    {
        switch(c)
        {
            case 'c':       // configuration dir
            {
                configDir = strdup(optarg);
                icLogDebug(PROP_LOG, "using supplied conf dir '%s'", configDir);
                break;
            }

            case 'h':       // home dir
            {
                homeDir = strdup(optarg);
                icLogDebug(PROP_LOG, "using supplied home dir '%s'", homeDir);
                break;
            }

            default:
                fprintf(stderr,"Unexpected option '%c'\n",c);
                if (configDir != NULL)
                {
                    free(configDir);
                }
                if (homeDir != NULL)
                {
                    free(homeDir);
                }
                printUsage();
                return EXIT_FAILURE;
                break;
        }
    }

    // use default paths if none were supplied
    //
    if (configDir == NULL)
    {
        configDir = strdup(DEFAULT_DYNAMIC_PATH);
        icLogDebug(PROP_LOG, "using default conf dir '%s'", configDir);
    }
    if (homeDir == NULL)
    {
        homeDir = strdup(DEFAULT_STATIC_PATH);
        icLogDebug(PROP_LOG, "using default home dir '%s'", homeDir);
    }

    // one time init of the property type checking config
    initPropertyTypeDefs();

    // setup event producer for broadcasting property add/del/update events
    //
    startPropsEventProducer();

    // one-time setup of internal structures
    //
    initProperties(configDir, homeDir);
    free(homeDir);
    free(configDir);

    // load 'branding' global properties, adding any we don't already have
    //
    loadGlobalDefaults();

    autoAdjustTimezone();
    autoAdjustCustomLogLevel(PROPS_SERVICE_NAME);

    // begin the 'service startup sequence', and block until the IPC receiver exits
    //
    startupService_propsService(NULL, NULL, NULL, IPC_DEFAULT_MIN_THREADS, IPC_DEFAULT_MAX_THREADS, IPC_DEFAULT_MAX_QUEUE_SIZE, true);

    // cleanup
    //
    destroyPropertyTypeDefs();
    destroyProperties();
    disableAutoAdjustTimezone();
    stopPropsEventProducer();
    closeIcLogger();

#ifdef CONFIG_DEBUG_BREAKPAD
    // Cleanup breakpad support
    breakpadHelperCleanup();
#endif

    return EXIT_SUCCESS;
}


/*
 * show user available options
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  propsService [-c config-dir] [-h home-dir]\n");
    fprintf(stderr, "    -c - set the 'configuration directory' (default: %s)\n", DEFAULT_DYNAMIC_PATH);
    fprintf(stderr, "    -h - set the 'home directory'          (default: %s)\n", DEFAULT_STATIC_PATH);
    fprintf(stderr, "\n");
}
