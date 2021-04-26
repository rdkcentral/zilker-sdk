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

//
// Created by John Elderton on 6/18/15.
//


#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdbool.h>

//#include <zlog.h>
#include <icLog/logging.h>
#include "parsePropTest.h"
#include "encodeTest.h"
#include "macAddrTest.h"
#include "ipAddrTest.h"
#include "versionTest.h"

/*
 *
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  utilTest <-a|-t>\n");
    fprintf(stderr, "    -a : run all tests\n");
    fprintf(stderr, "    -p : run propFile test\n");
    fprintf(stderr, "    -e : run encode/decode tests\n");
    fprintf(stderr, "    -m : run mac address tests\n");
    fprintf(stderr, "    -i : run ip address tests\n");
    fprintf(stderr, "    -v : run version tests\n");
    fprintf(stderr, "\n");
}

/*
 * main
 */
int main(int argc, char *argv[])
{
    int c;
    bool doPropFile = false;
    bool doEncode = false;
    bool doMacAddr = false;
    bool doIpAddr = false;
    bool doVersion = false;

    initIcLogger();

    while ((c = getopt(argc, argv, "aehpmiv")) != -1)
    {
        switch (c)
        {
            case 'a':
                doPropFile = true;
                doEncode = true;
                doMacAddr = true;
                doIpAddr = true;
                doVersion = true;
                break;

            case 'p':
                doPropFile = true;
                break;

            case 'e':
                doEncode = true;
                break;

            case 'm':
                doMacAddr = true;
                break;

            case 'i':
                doIpAddr = true;
                break;

            case 'v':
                doVersion = true;
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
    if (doPropFile == true)
    {
        printf("\n\nRunning PropFile Test:\n");
        if (runParsePropFileTests() == false)
        {
            printf("PropFile Test FAILED!\n");
            closeIcLogger();
            return 1;
        }
        printf("PropFile Test SUCCESS!\n");
        didSomething = true;
    }
    if (doEncode == true)
    {
        printf("\n\nRunning Encode/Decode Test:\n");
        if (runEncodeTests() == false)
        {
            printf("Encode/Decode Test FAILED!\n");
            closeIcLogger();
            return 1;
        }
        printf("Encode/Decode Test SUCCESS!\n");
        didSomething = true;
    }
    if (doMacAddr == true)
    {
        printf("\n\nRunning MAC Address Test:\n");
        if (runMacAddrTests() == false)
        {
            printf("MAC Address Test FAILED!\n");
            closeIcLogger();
            return 1;
        }
        printf("MAC Address Test SUCCESS!\n");
        didSomething = true;
    }
    if (doIpAddr == true)
    {
        printf("\n\nRunning IP Address Test:\n");
        if (runIpAddrTests() == false)
        {
            printf("IP Address Test FAILED!\n");
            closeIcLogger();
            return 1;
        }
        printf("IP Address Test SUCCESS!\n");
        didSomething = true;
    }
    if (doVersion == true)
    {
        printf("\n\nRunning Version Compare Test:\n");
        if (runVersionTests() == false)
        {
            printf("Version Compare Test FAILED!\n");
            closeIcLogger();
            return 1;
        }
        printf("Version Compare Test SUCCESS!\n");
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


