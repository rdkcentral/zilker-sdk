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
// Created by Thomas Lea on 7/20/15.
//

#include <icBuildtime.h>
#ifdef CONFIG_DEBUG_SINGLE_PROCESS

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/delayedTask.h>
#include <icIpc/ipcSender.h>
#include <icUtil/array.h>
#include <inttypes.h>
#include <icUtil/stringUtils.h>
#include <errno.h>
#include <icConcurrent/threadUtils.h>
#include <icLog/logging.h>
#include "../services/watchdog/core/src/procMgr.h"

// standard 'start service' function signature
typedef int (*service_main)(int argc, char ** argv);
// object used for the 'service start_main() thread'
typedef struct
{
    char *name;
    service_main main;
    int argc;
    char **argv;
    bool argv_alloced;
} ServiceStartInfo;

// externally defined service "main" functions for CONFIG_DEBUG_SINGLE_PROCESS
int watchdogService_main(int argc, char *argv[]);
#ifdef CONFIG_SERVICE_BACKUP_RESTORE
int backup_service_main(int argc, char *argv[]);
#endif
int comm_service_main(int argc, char *argv[]);
int props_service_main(int argc, char **argv);
int deviceService_main(int argc, char *argv[]);
#ifdef CONFIG_SERVICE_AUTOMATIONS
int automationService_main(int argc, char *argv[]);
#endif
#ifdef CONFIG_SERVICE_PKI
int pkiService_main(int argc, char *argv[]);
#endif

// forward declarations for internal functions
static void setServiceState(char *serviceName, bool isRunning);
static void freeServiceStartInfo(ServiceStartInfo *info);
bool getServiceState(char *serviceName);

// set of "service name" and "is running" (string, bool)
static icHashMap *serviceStateSet = NULL;
static icLinkedList *serviceThreads = NULL;
static pthread_mutex_t SERVICE_MTX = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t SINGLE_PHASE_START_COMPLETE_MTX = PTHREAD_COND_INITIALIZER;
//#define SINGLE_PHASE_STARTUP_WAIT_SECS 60

#define LOG_TAG "main"

/*
 *
 */
static char **getArgv(char *managerPath, int managerArgCount, char **managerArgs)
{
    char **result = NULL;

    if(managerArgCount == 0)
    {
        //watchdog wont set argv[0] if no args are provided in the conf file...
        result = (char**)calloc(1, sizeof(char*));
        result[0] = strdup(managerPath);
    }
    else
    {
        result = (char**)calloc((size_t) managerArgCount, sizeof(char*));

        for (int i = 0; i < managerArgCount; i++)
        {
            result[i] = strdup(managerArgs[i]);
        }
    }

    return result;
}

/*
 * thread to run a single service's 'main'
 */
static void* start_main(void *arg)
{
    ServiceStartInfo *info = (ServiceStartInfo*)arg;

    // show the input args...
    icLogDebug(LOG_TAG, "Calling main on %s", info->name);
    for(int i = 0; i < info->argc; i++)
    {
        icLogDebug(LOG_TAG, "\targ[%d] = %s", i, info->argv[i]);
    }

    // register state of this service in our serviceStateSet
    setServiceState(info->name, true);

    // call main
    info->main(info->argc, info->argv);

    // update state of this service in our serviceStateSet
    setServiceState(info->name, false);

    freeServiceStartInfo(info);

    return NULL;
}

/*
 * create thread to start a particular service
 */
static pthread_t* start_service(ServiceStartInfo *info)
{
    pthread_t *thread;

    thread = malloc(sizeof(pthread_t));
    createThread(thread, start_main, info, info->name);

    return thread;
}

/*
 * entry point for the "single process"
 */
int main(int argc, char ** argv)
{
    initTimedWaitCond(&SINGLE_PHASE_START_COMPLETE_MTX);
    pthread_t *watchdog_thread;

    // create hash of services and their state
    serviceStateSet = hashMapCreate();
    serviceThreads = linkedListCreate();

    ServiceStartInfo *watchdogInfo = (ServiceStartInfo*)calloc(1, sizeof(ServiceStartInfo));
    watchdogInfo->name = strdup("watchdog");
    watchdogInfo->main = watchdogService_main;
    watchdogInfo->argc = argc;
    watchdogInfo->argv = argv;
    watchdogInfo->argv_alloced = false;
    watchdog_thread = start_service(watchdogInfo);
    usleep(200000);

    // for now, just join on any of the services so main doesnt exit.
    pthread_join(*watchdog_thread, NULL);
    free(watchdog_thread);

    // wait for all threads to join (i.e. exit)
    icLinkedListIterator *iterator = linkedListIteratorCreate(serviceThreads);
    uint16_t remaining = linkedListCount(serviceThreads);
    while(linkedListIteratorHasNext(iterator))
    {
        pthread_t *thread = (pthread_t*)linkedListIteratorGetNext(iterator);
        char name[16] = { 0 };

#ifdef _GNU_SOURCE
        // Log any threads that are still running by now to help diagnose shutdown failures
        int err = pthread_getname_np(*thread, name, ARRAY_LENGTH(name));
        if (!err && *name != 0)
        {
            icLogDebug("main", "-----> WAITING for service %s to join", name);
        }
        else if (err != ENOENT)
        {
            // ENOENT OK for exited threads
            char *errStr = strerrorSafe(err);
            icLogDebug("main", "-----> ERROR getting service name: %s", errStr);
            free(errStr);
        }
#endif

        pthread_join(*thread, NULL);
        remaining--;

        if (*name != 0)
        {
            icLogDebug(LOG_TAG, "-----> COMPLETED service %s joined", name);
        }

        icLogDebug(LOG_TAG, "-----> %"PRIu16" services remaining", remaining);
    }
    linkedListIteratorDestroy(iterator);
    icLogDebug(LOG_TAG, "-----> COMPLETED all services joined");

    // cleanup
    hashMapDestroy(serviceStateSet, NULL);

    // cleanup any pending ipc messages
    ipcSenderShutdown();

    // Cleanup any delayed tasks that are left over
    finalizeAllDelayTasks();


    // pthread_exit(NULL) seems to cause address sanitizer to not find any symbols for the process...
    // We now seem to be shutting things down better so this it should not be necessary to wait on
    // other threads
    return 0;
}


extern icLinkedList *managerList;       // stolen from watchdogService

/*
 * overload function in watchdog.  instead of launching the
 * service process, we're going to start a thread for that service
 */
void startProcess(serviceDefinition *procDef, bool restartAfterCrash)
{
    useconds_t sleepTime = 0;
    pthread_t *thread = NULL;
    service_main svc_main = NULL;

    icLogDebug(LOG_TAG, "Starting %s", procDef->serviceName);

    if(strcmp(procDef->serviceName, "propsService") == 0)
    {
        svc_main = props_service_main;
    }
#ifdef CONFIG_SERVICE_BACKUP_RESTORE
    else if(strcmp(procDef->serviceName, "backupRestoreService") == 0)
    {
        svc_main = backup_service_main;
    }
#endif
    else if(strcmp(procDef->serviceName, "commService") == 0)
    {
        svc_main = comm_service_main;
    }
    else if(strcmp(procDef->serviceName, "deviceService") == 0)
    {
        svc_main = deviceService_main;
    }
#ifdef CONFIG_SERVICE_AUTOMATIONS
    else if(strcmp(procDef->serviceName, "automationService") == 0)
    {
        svc_main = automationService_main;
    }
#endif
#ifdef CONFIG_SERVICE_PKI
    else if (strcmp(procDef->serviceName, "pkiService") == 0)
    {
        svc_main = pkiService_main;
    }
#endif
    else
    {
        fprintf(stderr, "Unexpected service referenced in watchdog.conf: %s  Exiting\n", procDef->serviceName);
    }

    // sanity check that we have a service to run
    //
    if (svc_main == NULL)
    {
        // set the service to NOT expect an acknowledgement
        //
        procDef->expectStartupAck = false;
        procDef->autoStart = false;
        fprintf(stderr, "Unable to start service %s; missing 'main function'\n", procDef->serviceName);
        return;
    }

    //gonna leak this stuff
    ServiceStartInfo *info = (ServiceStartInfo*)calloc(1, sizeof(ServiceStartInfo));
    info->name = strdup(procDef->serviceName);
    info->main = svc_main;
    info->argc = procDef->execArgCount;
    info->argv = getArgv(procDef->execPath, procDef->execArgCount, procDef->execArgs);
    info->argv_alloced = true;
    if(info->argc == 0)
    {
        info->argc = 1;
    }

    // before we begin, reset the "received ack" time
    // so that we can easily detect if/when the process
    // sends us the acknowledgement.
    //
    procDef->lastActReceivedTime = 0;

    thread = start_service(info);

    if(sleepTime > 0)
    {
        usleep(sleepTime);
    }

    if(thread != NULL)
    {
        linkedListAppend(serviceThreads, thread);
    }
}

/*
 * set 'serviceName' to be running or not
 */
static void setServiceState(char *serviceName, bool isRunning)
{
    pthread_mutex_lock(&SERVICE_MTX);

    // see if we have an entry for this service name
    //
    uint16_t keyLen = (uint16_t)strlen(serviceName);
    bool *state = hashMapGet(serviceStateSet, serviceName, keyLen);
    if (state != NULL)
    {
        // already there, so update it
        //
        *state = isRunning;
        icLogDebug(LOG_TAG, "-----> UPDATING service %s state=%s", serviceName, (isRunning == true) ? "true" : "false");
    }
    else
    {
        // need to create the hash entry
        //
        bool *value = (bool *)malloc(sizeof(bool));
        *value = isRunning;
        hashMapPut(serviceStateSet, strdup(serviceName), keyLen, value);
        icLogDebug(LOG_TAG, "-----> SETTING service %s state=%s", serviceName, (isRunning == true) ? "true" : "false");
    }

    pthread_mutex_unlock(&SERVICE_MTX);
}

static void freeServiceStartInfo(ServiceStartInfo *info)
{
    if (!info)
    {
        return;
    }

    free(info->name);
    info->name = NULL;

    if (info->argv && info->argv_alloced)
    {
        for (int i = 0; i < info->argc; i++)
        {
            free(info->argv[i]);
        }
        free(info->argv);
        info->argv = NULL;
    }

    free(info);
}

/*
 * return if 'serviceName' is running or not.  not defined as 'static'
 * since this will be called by watchdog for the "waitForDeath()" implementation
 */
bool getServiceState(char *serviceName)
{
    bool retVal = false;

    // find this entry in our hash
    //
    pthread_mutex_lock(&SERVICE_MTX);
    bool *state = hashMapGet(serviceStateSet, serviceName, (uint16_t)strlen(serviceName));
    if (state != NULL)
    {
        retVal = *state;
    }
    pthread_mutex_unlock(&SERVICE_MTX);
    icLogDebug(LOG_TAG, "-----> GETTING service %s state=%s", serviceName, (retVal == true) ? "true" : "false");

    return retVal;
}

#endif

