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
 * configTest.c
 *
 * test utility for libicConfig.  also serves as an
 * example for protecting portions of the file.
 *
 * Author: jelderton -  8/30/16.
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include <icLog/logging.h>
#include "tests.h"

/*
 *
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  configTest <-a|-t>\n");
    fprintf(stderr, "    -a : run all tests\n");
    fprintf(stderr, "    -c : run config test\n");
    fprintf(stderr, "    -s : run storage test\n");
    fprintf(stderr, "\n");
}

/*
 * main
 */
int main(int argc, char *argv[])
{
    int c;
    bool doConfig = false;
    bool doStorage = false;

    initIcLogger();

    while ((c = getopt(argc, argv, "acsh")) != -1)
    {
        switch (c)
        {
            case 'a':
                doConfig = true;
                doStorage = true;
                break;

            case 'c':
                doConfig = true;
                break;

            case 's':
                doStorage = true;
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
    if (doConfig == true)
    {
        printf("\n\nRunning Config Test:\n");
        if (testProtectConfig() == false)
        {
            printf("  Config Test FAILED\n");
            closeIcLogger();
            return 1;
        }
        printf("  Config Test SUCCESS!\n");
        didSomething = true;
    }
    if (doStorage == true)
    {
        printf("\n\nRunning Storage Test:\n");
        if (runStorageTest() != 0)
        {
            printf("  Storage Test FAILED\n");
            closeIcLogger();
            return 1;
        }
        printf("  Storage Test SUCCESS!\n");
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

