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
 * backupUtil.c
 *
 * Command line utility to communicate with the
 * backupRestoreService.  Initially, this allows someone
 * to 'force' a backup to the server.
 *
 * Author: jelderton - 10/19/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include <icLog/logging.h>
#include <icIpc/ipcMessage.h>
#include <backup/backupRestoreService_ipc.h>

/*
 * private function declarations
 */
void printUsage();

/*
 * main entry point
 */
int main(int argc, char *argv[])
{
    int c;
    bool waitForService = false;
    bool forceBackup = false;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    // parse CLI args
    //
    while ((c = getopt(argc,argv,"fwh")) != -1)
    {
        switch (c)
        {
            case 'f':       // start a backup now
            {
                forceBackup = true;
                break;
            }
            case 'w':
            {
                waitForService = true;
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

    // look to see that we have a mode set
    //
    if (forceBackup == false)
    {
        fprintf(stderr, "No mode defined.  Use -h option for usage\n");
        closeIcLogger();
        return EXIT_FAILURE;
    }

    // if told to wait, do that before we contact the service
    //
    if (waitForService == true)
    {
        waitForServiceAvailable(BACKUPRESTORESERVICE_IPC_PORT_NUM, 30);
    }

    // send the request to start the backup sequence
    //
    IPCCode rc = backupRestoreService_request_FORCE_BACKUP();
    if (rc != IPC_SUCCESS)
    {
        fprintf(stdout, "error asking backup service to initiate backup : %d - %s\n", rc, IPCCodeLabels[rc]);
        closeIcLogger();
        return EXIT_FAILURE;
    }

    fprintf(stdout, "successfully started backup\n");
    closeIcLogger();
    return EXIT_SUCCESS;
}

/*
 * show user available options
 */
void printUsage()
{
    fprintf(stderr, "iControl Backup Utility\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  backupUtil [-f [-w]\n");
    fprintf(stderr, "    -f - ask backupService to immedietly initiate a backup archive\n");
    fprintf(stderr, "    -w - if necessary, wait for service to be available\n");
    fprintf(stderr, "\n");
}
