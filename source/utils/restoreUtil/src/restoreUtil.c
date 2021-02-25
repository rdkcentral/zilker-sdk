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
 * restoreUtil.c
 *
 * Command line utility to notify all services
 * managed by watchdog to perform a configuration
 * restore.
 *
 * Author: wboyd747 - 04/16/2019
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <icLog/logging.h>
#include <icIpc/ipcMessage.h>

#include <backup/backupRestoreService_ipc.h>
#include <watchdog/watchdogService_pojo.h>
#include <watchdog/watchdogService_ipc.h>
#include <icIpc/ipcStockMessagesPojo.h>
#include <icIpc/ipcStockMessages.h>
#include <propsMgr/paths.h>

#define TAG "restoreUtil"

static void restore_exit(void)
{
    closeIcLogger();
}

/*
 * show user available options
 */
static void printUsage()
{
    fprintf(stdout, "iControl Backup Utility\n");
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "  restoreUtil -i <dir|tgz> [-w]\n");
    fprintf(stdout, "    -i - Location of backup directory, or archive.\n");
    fprintf(stdout, "    -o - Restore backup directory, or archive, to output location. "
                    "[Default: Dynamic Config Path\n");
    fprintf(stdout, "    -s - Restore only the specified service. [Default: Restore all services.]\n");
    fprintf(stdout, "    -w - if necessary, wait for service to be available\n");
    fprintf(stdout, "\n");
}

static void restoreService(const char* service, const char *restoreDir, const char *configDir)
{
    IPCCode rc;

    // create the 'details' of the restore to pass along to all services
    //
    configRestoredInput *restoreDetails = create_configRestoredInput();

    restoreDetails->tempRestoreDir = strdup(restoreDir);
    restoreDetails->dynamicConfigPath = strdup(configDir);

    // get information for this service name
    //
    processInfo *info = create_processInfo();
    if ((rc = watchdogService_request_GET_SERVICE_BY_NAME((char*) service, info)) != IPC_SUCCESS)
    {
        icLogWarn(TAG, "restore: unable to get information about service %s : %d - %s\n", service, rc, IPCCodeLabels[rc]);
    }
    else
    {
        if ((rc = configRestored((uint16_t)info->ipcPortNum, restoreDetails, 10)) != IPC_SUCCESS)
        {
            icLogWarn(TAG, "restore: unable to inform service %s of the 'restore dir': %d - %s\n", service, rc, IPCCodeLabels[rc]);
        }
        else
        {
            icLogInfo(TAG, "restore: successfully informed service %s of the 'restore dir'", service);
        }
    }

    destroy_processInfo(info);
    destroy_configRestoredInput(restoreDetails);
}

static void tellAllServices(const char* restoreDir, const char* configDir)
{
    // first get the list of service names
    //
    IPCCode rc;
    allServiceNames* all = create_allServiceNames();

    if ((rc = watchdogService_request_GET_ALL_SERVICE_NAMES(all)) != IPC_SUCCESS) {
        icLogWarn(TAG, "restore: unable to get list of service names from watchdog : %d - %s\n", rc, IPCCodeLabels[rc]);
        destroy_allServiceNames(all);
        return;
    }

    // loop through each one, informing it where the "temp restore dir" is.
    //
    icLinkedListIterator* loop = linkedListIteratorCreate(all->list);
    while (linkedListIteratorHasNext(loop) == true) {
        restoreService(linkedListIteratorGetNext(loop), restoreDir, configDir);
    }

    // cleanup
    //
    linkedListIteratorDestroy(loop);
    destroy_allServiceNames(all);
}

/*
 * main entry point
 */
int main(int argc, char* argv[])
{
    int c;

    bool waitForService = false;
    char* restorePath = NULL;
    char* configPath = NULL;
    char* singleService = NULL;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    atexit(restore_exit);

    // parse CLI args
    //
    while ((c = getopt(argc, argv, "i:o:s:wh")) != -1) {
        switch (c) {
            case 'i':
                restorePath = strdup(optarg);
                break;
            case 'o':
                configPath = strdup(optarg);
                break;
            case 's':
                singleService = strdup(optarg);
                break;
            case 'w':
                waitForService = true;
                break;
            case 'h':       // help option
                printUsage();

                return EXIT_SUCCESS;
            default:
                fprintf(stderr, "Unknown option '%c'\n", c);

                printUsage();

                return EXIT_FAILURE;
        }
    }

    if ((restorePath == NULL) || (restorePath[0] =='\0')) {
        fprintf(stderr, "Error: No configuration directory, or archive, provided.\n");
        return EXIT_FAILURE;
    }

    if (strstr(restorePath, ".tgz") == NULL) {
        /*
         * This is _not_ an archive. Check to see if it is a directory.
         */

        struct stat sinfo;
        int ret;

        ret = stat(restorePath, &sinfo);
        if (ret != 0) {
            fprintf(stderr, "Error: Failed to verify configuration path: [%s]\n", strerror(errno));
            return EXIT_FAILURE;
        } else if (!(sinfo.st_mode & S_IFDIR)) {
            fprintf(stderr, "Error: Unknown configuration parameter.\n");
            return EXIT_FAILURE;
        }

        fprintf(stdout, "Restoring from: [%s]\n", restorePath);
    } else {
        /*
         * This is an archive of type tar-gzip (tgz)
         *
         * TODO: Actually extract this tarball and replace config path.
         */
        fprintf(stderr, "Error: Archive support not implemented.\n");
        return EXIT_FAILURE;
    }

    // if told to wait, do that before we contact the service
    //
    if (waitForService == true) {
        waitForServiceAvailable(BACKUPRESTORESERVICE_IPC_PORT_NUM, 30);
    }

    if (configPath == NULL) {
        configPath = getDynamicConfigPath();
    }

    if (singleService == NULL) {
        tellAllServices(restorePath, configPath);
    } else {
        restoreService(singleService, restorePath, configPath);
    }

    free(restorePath);
    free(configPath);
    free(singleService);

    return EXIT_SUCCESS;
}
