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
 * serviceUtil.c
 *
 * Command line utility to stop, start, and restart
 * iControl services that are managed via the
 * watchdogService.
 *
 * Author: jelderton - 7/9/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icIpc/ipcMessage.h>
#include <icIpc/ipcStockMessages.h>
#include <icUtil/stringUtils.h>
#include <icTypes/icSortedLinkedList.h>
#include <icSystem/runtimeAttributes.h>
#include <watchdog/watchdogService_ipc.h>
#include <commMgr/commService_ipc.h>
#include <propsMgr/propsService_ipc.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsHelper.h>
#include <icTime/timeUtils.h>

typedef enum
{
    NO_ACTION,
    LIST_ACTION,
    SUMMARY_LIST_ACTION,
    START_ACTION,       // cliTarget required
    STOP_ACTION,        // cliTarget required
    RESTART_ACTION      // cliTarget required
} cliAction;

typedef enum
{
    NO_TARGET,
    SERVICE_TARGET,
    GROUP_TARGET,
    ALL_TARGET
} cliTarget;

typedef enum
{
    PRINT_SUMMARY,
    PRINT_NORMAL,
    PRINT_VERBOSE
} outputFormat;

/*
 * private function declarations
 */
static void printUsage();
static void printList(cliTarget filter, char *serviceName, outputFormat format);
static void printService(processInfo *info, outputFormat format);
static void printBool(short flag);
static void printDate(uint64_t time);
static void doCleanup(char *value);
static bool findPrimaryCommHostConfig(void *searchVal, void *item);

/*
 * main entry point
 */
int main(int argc, char *argv[])
{
    int c;
    char *name = NULL;
    cliAction action = NO_ACTION;
    cliTarget target = NO_TARGET;
    bool waitForService = false;
    bool verboseMode = false;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    // parse CLI args
    //
    while ((c = getopt(argc,argv,"lmkrbs:g:awhv")) != -1)
    {
        switch (c)
        {
            case 'l':       // list processes
            {
                action = LIST_ACTION;
                break;
            }
            case 'm':       // summary of processes
            {
                action = SUMMARY_LIST_ACTION;
                break;
            }
            case 'k':       // kill action
            {
                action = STOP_ACTION;
                break;
            }
            case 'r':       // start action
            {
                action = START_ACTION;
                break;
            }
            case 'b':       // restart action
            {
                action = RESTART_ACTION;
                break;
            }

            case 's':       // service target
            {
                if (name != NULL || target != NO_TARGET)
                {
                    fprintf(stderr,"Can only specify one service (-s) or group (-g)\n  Use -h option for usage\n");
                    doCleanup(name);
                    closeIcLogger();
                    return EXIT_FAILURE;
                }
                name = strdup(optarg);
                target = SERVICE_TARGET;
                break;
            }
            case 'g':       // group target
            {
                if (name != NULL || target != NO_TARGET)
                {
                    fprintf(stderr,"Can only specify one service (-s) or group (-g)\n  Use -h option for usage\n");
                    doCleanup(name);
                    closeIcLogger();
                    return EXIT_FAILURE;
                }
                name = strdup(optarg);
                target = GROUP_TARGET;
                break;
            }
            case 'a':       // all target
            {
                target = ALL_TARGET;
                break;
            }
            case 'w':
            {
                waitForService = true;
                break;
            }
            case 'v':
            {
                verboseMode = true;
                break;
            }
            case 'h':       // help option
            {
                printUsage();
                doCleanup(name);
                closeIcLogger();
                return EXIT_SUCCESS;
            }
                        
            default:
            {
                fprintf(stderr,"Unknown option '%c'\n",c);
                doCleanup(name);
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
        doCleanup(name);
        closeIcLogger();
        return EXIT_FAILURE;
    }

    // most modes require a service or group name
    //
    if (action == LIST_ACTION || action == SUMMARY_LIST_ACTION)
    {
        // good to go
    }
    else if (name == NULL && target != ALL_TARGET)
    {
        // need a service or group
        fprintf(stderr,"Must specify a service (-s) or group (-g)\n  Use -h option for usage\n");
        doCleanup(name);
        closeIcLogger();
        return EXIT_FAILURE;
    }

    // if told to wait, do that before we contact the service
    //
    if (waitForService == true)
    {
        waitForServiceAvailable(WATCHDOGSERVICE_IPC_PORT_NUM, 30);
    }

    // handle each action
    //
    IPCCode ipcRc;
    bool rc = false;
    switch (action)
    {
        case LIST_ACTION:
        case SUMMARY_LIST_ACTION:
        {
            // call our 'printList', but need to setup the inputs
            //
            cliTarget filter = NO_TARGET;
            char *serviceName = NULL;
            outputFormat format = PRINT_NORMAL;
            if (target == SERVICE_TARGET && name != NULL)
            {
                filter = target;
                serviceName = name;
            }
            if (action == SUMMARY_LIST_ACTION)
            {
                format = PRINT_SUMMARY;
            }
            else if (verboseMode == true)
            {
                format = PRINT_VERBOSE;
            }

            // do the print
            //
            printList(filter, serviceName, format);
            rc = true;
            break;
        }

        case START_ACTION:
        {
            if (target == SERVICE_TARGET)
            {
                if ((ipcRc = watchdogService_request_START_SERVICE(name, &rc)) != IPC_SUCCESS)
                {
                    fprintf(stderr, "Unable to communicate with watchdog : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                }
                else if (rc != true)
                {
                    fprintf(stderr, "Failed to start %s, which could be due to invalid service or not allowed\n", name);
                }
                else
                {
                    fprintf(stdout, "Successfully started service %s via watchdog\n", name);
                }
            }
            else if (target == GROUP_TARGET)
            {
                if ((ipcRc = watchdogService_request_START_GROUP(name, &rc)) != IPC_SUCCESS)
                {
                    fprintf(stderr, "Unable to communicate with watchdog : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                }
                else if (rc != true)
                {
                    fprintf(stderr, "Unable to start group %s via watchdog\n", name);
                }
                else
                {
                    fprintf(stdout, "Successfully started group %s via watchdog\n", name);
                }
            }
            else
            {
                fprintf(stderr, "Unable to start services, missing 'target'\n");
            }
            break;
        }

        case STOP_ACTION:
        {
            if (target == SERVICE_TARGET)
            {
                if ((ipcRc = watchdogService_request_STOP_SERVICE_timeout(name, &rc, 0)) != IPC_SUCCESS)
                {
                    fprintf(stderr, "Unable to communicate with watchdog : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                }
                else if (rc != true)
                {
                    fprintf(stderr, "Failed to stop %s, which could be due to invalid service or not allowed\n", name);
                }
                else
                {
                    fprintf(stdout, "Successfully stopped service %s via watchdog\n", name);
                }
            }
            else if (target == GROUP_TARGET)
            {
                if ((ipcRc = watchdogService_request_STOP_GROUP_timeout(name, &rc, 0)) != IPC_SUCCESS)
                {
                    fprintf(stderr, "Unable to communicate with watchdog : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                }
                else if (rc != true)
                {
                    fprintf(stderr, "Unable to stop group %s via watchdog\n", name);
                }
                else
                {
                    fprintf(stdout, "Successfully stopped group %s via watchdog\n", name);
                }
            }
            else if (target == ALL_TARGET)
            {
                // note: use default input parms
                //
                shutdownOptions *opt = create_shutdownOptions();
#if defined(CONFIG_OS_LINUX) || defined(CONFIG_OS_DARWIN)
                opt->exit = true;       // force exit when running from Linux/Mac
#else
                opt->exit = false;
#endif
                opt->forReset = false;

                // because some may be busy for a long time, do this without a timeout on the IPC
                if ((ipcRc = watchdogService_request_SHUTDOWN_ALL_SERVICES_timeout(opt, 0)) != IPC_SUCCESS)
                {
                    fprintf(stderr, "Unable to communicate with watchdog : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                }
                else
                {
                    fprintf(stdout, "Successfully stopped ALL SERVICES via watchdog\n");
                }
                destroy_shutdownOptions(opt);
            }
            else
            {
                fprintf(stderr, "Unable to 'stop' services, missing 'target'\n");
            }
            break;
        }

        case RESTART_ACTION:
        {
            if (target == SERVICE_TARGET)
            {
                if ((ipcRc = watchdogService_request_RESTART_SERVICE(name, &rc)) != IPC_SUCCESS)
                {
                    fprintf(stderr, "Unable to communicate with watchdog : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                }
                else if (rc != true)
                {
                    fprintf(stderr, "Failed to restart %s, which could be due to invalid service or not allowed\n", name);
                }
                else
                {
                    fprintf(stdout, "Successfully restarted service %s via watchdog\n", name);
                }
            }
            else if (target == GROUP_TARGET)
            {
                if ((ipcRc = watchdogService_request_RESTART_GROUP(name, &rc)) != IPC_SUCCESS)
                {
                    fprintf(stderr, "Unable to communicate with watchdog\n");
                }
                else if (rc != true)
                {
                    fprintf(stderr, "Unable to restart group %s via watchdog\n", name);
                }
                else
                {
                    fprintf(stdout, "Successfully restarted group %s via watchdog\n", name);
                }
            }
            else if (target == ALL_TARGET)
            {
                // note: use default input parms
                //
                shutdownOptions *opt = create_shutdownOptions();
                if ((ipcRc = watchdogService_request_RESTART_ALL_SERVICES(opt)) != IPC_SUCCESS)
                {
                    fprintf(stderr, "Unable to communicate with watchdog : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
                }
                else
                {
                    fprintf(stdout, "Successfully restarted ALL SERVICES via watchdog\n");
                }
                destroy_shutdownOptions(opt);
            }
            else
            {
                fprintf(stderr, "Unable to 'restart' services, missing 'target'\n");
            }
            break;
        }

        default:
            // This will only be displayed if an option is parsed but isn't defined
            fprintf(stderr, "Unsupported operation [%d]\n", action);
            break;
    }

    // memory cleanup
    //
    doCleanup(name);
    closeIcLogger();
    if (rc != true)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/*
 * mem cleanup
 */
static void doCleanup(char *value)
{
    if (value != NULL)
    {
        free(value);
    }
}

/*
 * show user available options
 */
static void printUsage()
{
    fprintf(stderr, "iControl Service Utility (via watchdog)\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  serviceUtil <-l|-k|-r|-b> <-s name|-g name> [-w]\n");
    fprintf(stderr, "    -l : list all known services\n");
    fprintf(stderr, "    -m : summary of known services (name, pid, deaths)\n");
    fprintf(stderr, "    -k : kill a service, group, or all\n");
    fprintf(stderr, "    -r : run a service or group\n");
    fprintf(stderr, "    -b : bounce a service, group, or all\n");
    fprintf(stderr, "    -s - target service 'name'\n");
    fprintf(stderr, "    -g - target group 'name'\n");
    fprintf(stderr, "    -a - target all services (kill or bounce)\n");
    fprintf(stderr, "    -w : if necessary, wait for watchdog to be available\n");
    fprintf(stderr, "    -v : verbose mode for -l (ask service for status fields)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  # list all services and their status\n");
    fprintf(stderr, "  serviceUtil -l\n\n");
    fprintf(stderr, "  # list detailed status for one service\n");
    fprintf(stderr, "  serviceUtil -l -v -s commService\n\n");
    fprintf(stderr, "  # kill service 'commService'\n");
    fprintf(stderr, "  serviceUtil -k -s commService\n\n");
    fprintf(stderr, "  # kill all services\n");
    fprintf(stderr, "  serviceUtil -k -a\n\n");
    fprintf(stderr, "  # bounce (restart) group 'zigbee'\n");
    fprintf(stderr, "  serviceUtil -b -g commService\n\n");
}

/*
 *
 */
static void printList(cliTarget filter, char *serviceName, outputFormat format)
{
    if (format == PRINT_SUMMARY)
    {
        // print headers.  use same formatting as printService()
        //
        printf("%-30s PID    Deaths\n", "Service Name");
    }

    // see if single service or all of them
    //
    IPCCode ipcRc;
    if (filter != NO_TARGET && serviceName != NULL)
    {
        // get details on the 'one' service
        //
        processInfo *proc = create_processInfo();
        if ((ipcRc = watchdogService_request_GET_SERVICE_BY_NAME(serviceName, proc)) == IPC_SUCCESS)
        {
            // good to go, print it
            //
            printService(proc, format);
        }
        else
        {
            fprintf(stderr, "Unable to get information about service %s : %d - %s\n", serviceName, ipcRc, IPCCodeLabels[ipcRc]);
        }
        destroy_processInfo(proc);
        return;
    }

    // get the list of service names
    //
    allServiceNames *all = create_allServiceNames();
    if ((ipcRc = watchdogService_request_GET_ALL_SERVICE_NAMES(all)) != IPC_SUCCESS)
    {
        fprintf(stderr, "Unable to get list of service names within watchdog : %d - %s\n", ipcRc, IPCCodeLabels[ipcRc]);
        destroy_allServiceNames(all);
        return;
    }

    // get each one, and print it's information
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(all->list);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // get information for this service name
        //
        char *svcName = (char *) linkedListIteratorGetNext(loop);
        processInfo *proc = create_processInfo();
        if ((ipcRc = watchdogService_request_GET_SERVICE_BY_NAME(svcName, proc)) == IPC_SUCCESS)
        {
            // print the service information
            //
            printService(proc, format);
        }
        else
        {
            fprintf(stderr, "Unable to get information about service %s : %d - %s\n", svcName, ipcRc, IPCCodeLabels[ipcRc]);
        }
        destroy_processInfo(proc);
    }
    linkedListIteratorDestroy(loop);
    destroy_allServiceNames(all);
}

/*
 * caller needs to free returned struct
 */
static icHashMap *getProcessStatus(processInfo *info)
{
    icHashMap *retVal = NULL;

    // use the helper functions in ipcStockMessages.h to send the IPC
    //
    serviceStatusPojo *holder = create_serviceStatusPojo();
    IPCCode rc = getServiceStatus((uint16_t)info->ipcPortNum, holder, 10);
    if (rc == IPC_SUCCESS)
    {
        // just pull the 'values' map from our container, then set it's
        // internal variable to NULL so that we don't free it in the 'destroy' below
        //
        retVal = holder->statusMap;
        holder->statusMap = NULL;
    }

    // cleanup
    //
    destroy_serviceStatusPojo(holder);
    return retVal;
}

/*
 * used to sort the status keys (alpha sort)
 */
static int8_t sortStatusMapKeys(void *left, void *right)
{
    int diff = strcmp((char *)left, (char *)right);

    if (diff < 0)
    {
        return -1;
    }
    else if (diff > 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*
 * print the detailed status received from the process
 */
static void printStatusHashMap(icHashMap *map)
{
    if (map == NULL)
    {
        return;
    }

    // get an iterator to go through the keys, and sort them alphabetically
    //
    icSortedLinkedList *sorted = sortedLinkedListCreate();
    icHashMapIterator *loop = hashMapIteratorCreate(map);
    while (hashMapIteratorHasNext(loop) == true)
    {
        void *mapKey;
        void *mapValue;
        uint16_t mapKeyLen = 0;

        // get the next status key/value string from the iterator
        //
        hashMapIteratorGetNext(loop, &mapKey, &mapKeyLen, &mapValue);
        if (mapKey != NULL && mapValue != NULL)
        {
            sortedLinkedListAdd(sorted, mapKey, sortStatusMapKeys);
//            printf("  %-15s = %s\n", (char *)mapKey, (char *)mapValue);
        }
    }
    hashMapIteratorDestroy(loop);

    // now loop through the sorted keys, extracting the values
    icLinkedListIterator *sortLoop = linkedListIteratorCreate(sorted);
    while (linkedListIteratorHasNext(sortLoop) == true)
    {
        char *mapKey = (char *)linkedListIteratorGetNext(sortLoop);
        char *mapValue = (char *)hashMapGet(map, mapKey, (uint16_t)strlen(mapKey));
        if (mapValue != NULL)
        {
            printf("  %-18s = %s\n", (char *)mapKey, (char *)mapValue);
        }
    }
    linkedListIteratorDestroy(sortLoop);
    linkedListDestroy(sorted, standardDoNotFreeFunc);
}

static void printService(processInfo *info, outputFormat format)
{
    if (format == PRINT_SUMMARY)
    {
        // just name, pid, crash-count
        //
        printf("%-30s %-6"PRIu64" %"PRIu64"\n", info->serviceName, info->processId, info->deathCount);
        return;
    }

    // print the details
    //
    printf("--------------------------------------------------\n");
    printf("Service         : %s\n", info->serviceName);
    if (info->running > 0)
    {
        printf("PID             : %" PRIu64 "\n", info->processId);
    }
    printf("Running         : ");
    printBool(info->running);
    printf("\nDeath Count     : %" PRIu64 , info->deathCount);
    if (info->runStartTime > 0)
    {
        printf("\nStarted at      : ");
        printDate(info->runStartTime);
    }
    printf("\nAutostart       : ");
    printBool(info->autoStart);
    printf("\nRestart on fail : ");
    printBool(info->restartOnFail);
    printf("\nSend ack @ start: ");
    printBool(info->expectsAck);
    if (info->expectsAck > 0)
    {
        printf("\nReceived ack at : ");
        if (info->ackReceivedTime > 0)
        {
            printDate(info->ackReceivedTime);
        }
        else
        {
            printf("NOT RECEIVED");
        }
    }
    printf("\nJava Service    : ");
    printBool(info->isJava);
    printf("\n\n");

    // if include status, try to ask this service
    // for it's status information.
    //
    if (format == PRINT_VERBOSE && info->ipcPortNum > 0)
    {
        icHashMap *status = getProcessStatus(info);
        if (status != NULL && hashMapCount(status) > 0)
        {
            printf("Status details  : \n");
            printStatusHashMap(status);
        }
        hashMapDestroy(status, NULL);
    }
    printf("\n");   // blank line before next service
}

static void printBool(short flag)
{
    if (flag == 0)
    {
        printf("NO");
    }
    else
    {
        printf("yes");
    }
}

static void printDate(uint64_t time)
{
    char buff[32];
    struct tm ptr;

    time_t timet = convertUnixTimeMillisToTime_t(time);

    localtime_r(&timet, &ptr);
    strftime(buff, 31, "%Y-%m-%d %H:%M:%S", &ptr);
    printf("%s", buff);
//    printf("%ld", time);
}
