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
 * systemMode.c
 *
 * Command line utility to get or set the "system mode"
 * via the securityService
 *
 * Author: jelderton - 7/9/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <icLog/logging.h>
#include <icIpc/ipcMessage.h>
#include <securityService/securityService_ipc.h>

/*
 * private function declarations
 */
void printUsage();
void doCleanup(char *value);

typedef enum
{
    NO_ACTION,
    GET_ACTION,
    SET_ACTION,
    LIST_ACTION
} actionMode;

/*
 * main entry point
 */
int main(int argc, char *argv[])
{
    int c;
    actionMode action = NO_ACTION;
    char *mode = NULL;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    // parse CLI args
    //
    while ((c = getopt(argc,argv,"lgs:h")) != -1)
    {
        switch (c)
        {
            case 'l':       // list
            {
                action = LIST_ACTION;
                break;
            }
            case 'g':       // get
            {
                action = GET_ACTION;
                break;
            }
            case 's':       // set
            {
                if (mode != NULL)
                {
                    fprintf(stderr,"Can only specify one systemMode (-s)\n  Use -h option for usage\n");
                    doCleanup(mode);
                    closeIcLogger();
                    return EXIT_FAILURE;
                }
                mode = strdup(optarg);
                action = SET_ACTION;
                break;
            }
            case 'h':       // help option
            {
                printUsage();
                doCleanup(mode);
                closeIcLogger();
                return EXIT_SUCCESS;
            }
                        
            default:
            {
                fprintf(stderr,"Unknown option '%c'\n",c);
                doCleanup(mode);
                closeIcLogger();
                return EXIT_FAILURE;
            }
        }
    }

    // look to see that we have a mode set
    //
    if (action == NO_ACTION)
    {
        fprintf(stderr, "No operation defined.  Use -h option for usage\n");
        doCleanup(mode);
        closeIcLogger();
        return EXIT_FAILURE;
    }

    // most modes require a service or group name
    //
    if (action == SET_ACTION && mode == NULL)
    {
        fprintf(stderr,"Must specify a systemMode (-s)\n  Use -h option for usage\n");
        doCleanup(mode);
        closeIcLogger();
        return EXIT_FAILURE;
    }

    // handle each action
    //
    IPCCode ipcRc = IPC_SUCCESS;
    switch (action)
    {
        case LIST_ACTION:
        {
            // prep the request, then ask securityService
            //
            systemModeList *output = create_systemModeList();

            if ((ipcRc = securityService_request_GET_ALL_SYSTEM_MODES(output)) == IPC_SUCCESS)
            {
                // loop through the list, printing each one
                //
                icLinkedListIterator *loop = linkedListIteratorCreate(output->list);
                while (linkedListIteratorHasNext(loop) == true)
                {
                    char *mode = (char *)linkedListIteratorGetNext(loop);
                    printf("%s\n", mode);
                }

                // mem cleanup
                //
                linkedListIteratorDestroy(loop);
                destroy_systemModeList(output);
                output = NULL;
            }
            else
            {
                // error
                //
                fprintf(stderr, "Unable to get system modes : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                doCleanup(mode);
                destroy_systemModeList(output);
                closeIcLogger();
                return EXIT_FAILURE;
            }
            break;
        }

        case GET_ACTION:
        {
            // get the mode from securityService
            //
            char *output = NULL;
            if ((ipcRc = securityService_request_GET_CURRENT_SYSTEM_MODE(&output)) == IPC_SUCCESS)
            {
                // print the mode
                //
                if (output != NULL)
                {
                    printf("Current system mode is: %s\n", output);
                }
                else
                {
                    printf("system mode is not set\n");
                }

                // cleanup
                if (output != NULL)
                {
                    free(output);
                }
            }
            else
            {
                // error
                //
                fprintf(stderr, "Unable to get system mode : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                doCleanup(mode);
                if (output != NULL)
                {
                    free(output);
                }
                closeIcLogger();
                return EXIT_FAILURE;
            }
            break;
        }

        case SET_ACTION:
        {
            // build up the request
            //
            bool flag = false;
            systemModeRequest *req = create_systemModeRequest();
            req->systemMode = strdup(mode);
            req->requestId = 0;

            // ask securityService to perform the 'set'
            //
            ipcRc = securityService_request_SET_CURRENT_SYSTEM_MODE(req, &flag);
            destroy_systemModeRequest(req);
            if (ipcRc != IPC_SUCCESS || flag != true)
            {
                // error
                //
                fprintf(stderr, "Unable to set system mode to '%s' : %d - %s\n", mode, ipcRc, IPCCodeLabels[ipcRc]);
                doCleanup(mode);
                closeIcLogger();
                return EXIT_FAILURE;
            }
            break;
        }

        case NO_ACTION:
        default:
            // do nothing, just here to satisfy the compiler
            break;
    }

    // memory cleanup
    //
    doCleanup(mode);
    closeIcLogger();
    return EXIT_SUCCESS;
}

/*
 * mem cleanup
 */
void doCleanup(char *value)
{
    if (value != NULL)
    {
        free(value);
    }
}

/*
 * show user available options
 */
void printUsage()
{
//    while ((c = getopt(argc,argv,"lkrbs:g:w")) != -1)
    fprintf(stderr, "iControl SystemMode Utility\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  systemMode [-l] [-g] [-s mode]\n");
    fprintf(stderr, "    -l : list all system modes\n");
    fprintf(stderr, "    -g : get current system mode\n");
    fprintf(stderr, "    -s - set the system mode to 'name'\n\n");
}

