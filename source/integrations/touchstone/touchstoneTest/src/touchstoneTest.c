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
 * touchstoneTest.c
 *
 * simple command line utility to validate the
 * libtouchstone.so integration library.
 *
 * Author: jelderton - 7/5/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>

#include <icLog/logging.h>
#include <touchstone.h>

/*
 * show CLI options
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  touchstoneTest <-a|-r|-g|-s hostname|-A hostname|-R>\n");
    fprintf(stderr, "    -a : print result of 'touchstoneIsActivated'\n");
    fprintf(stderr, "    -r : print result of 'touchstoneIsRunning'\n");
    fprintf(stderr, "    -g : print result of 'touchstoneGetServerHostname'\n");
    fprintf(stderr, "    -s - set the host via 'touchstoneSetServerHostname'\n");
    fprintf(stderr, "    -T - activate timeout for 'touchstoneStartActivation' (requires -A)\n");
    fprintf(stderr, "    -R : reset via 'touchstoneResetToFactory'\n");
    fprintf(stderr, "    -b : restart touchstone processes\n");
    fprintf(stderr, "\n");
}

/*
 * main
 */
int main(int argc, char *argv[])
{
    int c;
    bool doActivation = false;
    bool doRunning = false;
    bool doGetServer = false;
    bool doRestart = false;
    char *doSetServer = NULL;
    uint32_t activationTimeout = 10;
    bool doReset = false;

    initIcLogger();

    while ((c = getopt(argc, argv, "args:T:Rbh")) != -1)
    {
        switch (c)
        {
            case 'a':
                doActivation = true;
                break;

            case 'r':
                doRunning = true;
                break;

            case 'g':
                doGetServer = true;
                break;

            case 'b':
                doRestart = true;
                break;

            case 's':
                doSetServer = strdup(optarg);
                break;

            case 'T':
                activationTimeout = (uint32_t)strtoul(optarg, 0, 10);
                break;

            case 'R':
                doReset = true;
                break;

            case 'h':
            default:
            {
                printUsage();
                closeIcLogger();
                return 1;
            }
        }
    }

    bool didSomething = false;
    if (doActivation == true)
    {
        printf("\n\nRunning 'is activated' Test:\n");
        bool flag = touchstoneIsActivated();
        printf("touchstoneIsActivated = %s\n", (flag == true) ? "true" : "false");
        didSomething = true;
    }
    if (doRunning == true)
    {
        printf("\n\nRunning 'is running' Test:\n");
        bool flag = touchstoneIsRunning();
        printf("touchstoneIsRunning = %s\n", (flag == true) ? "true" : "false");
        didSomething = true;
    }
    if (doGetServer == true)
    {
        printf("\n\nRunning 'get server' Test:\n");
        char *hostname = touchstoneGetServerHostname();
        printf("touchstoneGetServerHostneme = %s\n", (hostname != NULL) ? hostname : "NULL");
        didSomething = true;
    }
    if (doSetServer != NULL)
    {
        printf("\n\nRunning 'set server' Test:\n");
        bool flag = touchstoneSetServerHostname(doSetServer);
        printf("touchstoneSetServerHostname(%s) = %s\n", doSetServer, (flag == true) ? "true" : "false");
        didSomething = true;
    }
    if (doReset == true)
    {
        printf("\n\nRunning 'reset to factory' Test:\n");
        bool flag = touchstoneResetToFactory();
        printf("touchstoneResetToFactory = %s\n", (flag == true) ? "true" : "false");
        didSomething = true;
    }
    if (doRestart == true)
    {
        printf("\n\nRunning 'restart touchstone' Test:\n");
        bool flag = touchstoneRestart();
        printf("touchstoneRestart = %s\n", (flag == true) ? "true" : "false");
        didSomething = true;
    }

    if (didSomething == false)
    {
        fprintf(stderr, "no options provided, use -h option for help\n");
        closeIcLogger();
        return 1;
    }

    // no errors?
    //
    closeIcLogger();
    return 0;
}
