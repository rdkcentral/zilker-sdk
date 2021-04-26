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
 * resetToFactoryDefaults.c
 *
 * Command line utility to reset 'most' of settings back
 * to the factory default - and perform a reboot.
 *
 * We say 'most' because some configuration files are
 * kept intact, or only partially cleared out (ex: communication.conf)
 *
 * Author: gfaulkner, jelderton
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

#include <icLog/logging.h>
#include <icReset/factoryReset.h>

static void printUsage();

/*
 * main entry point of the utility
 */
int main(int argc, char *argv[])
{
    // make sure the user running this is "root"
    // (note: look at effective uid and real uid)
    //
    if (geteuid() != 0 && getuid() != 0)
    {
        fprintf(stderr, "Unable to perform 'reset to factory'.  This requires execution as 'root'.  Use -h for options.\n");
        return EXIT_FAILURE;
    }

    int c;
    bool resetAll = false;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    // parse CLI args
    //
    while ((c = getopt(argc,argv,"Rh")) != -1)
    {
        switch (c)
        {
            case 'R':
            {
                resetAll = true;
                break;
            }
            case 'h':       // help option
            {
                printUsage();
                closeIcLogger();
                return EXIT_SUCCESS;
            }

            default:
            {
                fprintf(stderr,"Unknown option '%c'\n",c);
                closeIcLogger();
                return EXIT_FAILURE;
            }
        }
    }

    printf("Resetting to factory defaults, please wait...\n");
    fflush(stdout);
    if (resetAll == true)
    {
        // remove all files
        //
        resetForRebranding();
    }
    else
    {
        // remove standard files
        //
        resetToFactory();
    }
    printf("Done\n");

    closeIcLogger();
    return EXIT_SUCCESS;
}

/*
 * show user available options
 */
static void printUsage()
{
    fprintf(stderr, "iControl resetToFactory Utility\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  resetToFactoryDefaults [-R] [-h]\n");
    fprintf(stderr, "    -R : remove all files (essentially reset-for-rebranding)\n");
    fprintf(stderr, "    -h : show this usage\n\n");
}
