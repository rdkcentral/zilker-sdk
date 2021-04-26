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
 * restoreJob.h
 *
 * Task to perform a 'restore' of our configuration.
 *
 * Similar to the Java counterpart, this will download our
 * configuration from the server into a temporary directory.
 * Once complete, notify all services so they can import the
 * settings they need from the old configuration files.
 * After all services have completed, message watchdog to restart
 * all of the services (soft boot).
 *
 * Author: jelderton - 4/6/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icConcurrent/delayedTask.h>
#include <icIpc/ipcReceiver.h>
#include <icIpc/ipcStockMessages.h>
#include <icConfig/storage.h>
#include <watchdog/watchdogService_pojo.h>
#include <watchdog/watchdogService_ipc.h>
#include <propsMgr/paths.h>
#include <commMgr/commService_ipc.h>
#include <backup/backupRestoreService_event.h>
#include <urlHelper/urlHelper.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/threadUtils.h>

#include "restoreJob.h"
#include "common.h"
#include "broadcastEvent.h"

#define TAG BACKUP_LOG "/restore"

// string representations of the restore event 'values'.
// NOTE: these strings get passed to the server, so they cannot change
//
#define RESTORE_STEP_BEGIN_STR    "RESTORE_BEGIN"
#define RESTORE_STEP_DOWNLOAD_STR "RESTORE_DOWNLOAD"
#define RESTORE_STEP_NETWORK_STR  "RESTORE_NETWORK"
#define RESTORE_STEP_CONFIG_STR   "RESTORE_CONFIG"
#define RESTORE_STEP_COMPLETE_STR "RESTORE_COMPLETE"

#define CONFIG_DOWNLOAD_TIMEOUT_SECONDS 300
#define SERVICE_STOP_TIMEOUT_SECONDS 20
#define SERVICE_RESTART_TIMEOUT_SECONDS 20

#ifdef CONFIG_DEBUG_SINGLE_PROCESS
// function from main.c
bool getServiceState(char *serviceName);
#endif

// local vars
//
static pthread_mutex_t RESTORE_MTX = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t SERVICE_RESTORE_COND;

static bool restoreRunning = false;

static icLinkedList* serviceCallbackList = NULL;
static icLinkedList* serviceRestartList = NULL;

// private functions
//
static void *restoreThread(void *arg);
static char *extractConfig();
static bool tellAllServices(char* restoreDir, char* configDir);
static bool waitForServiceCallbacks(void);
static bool waitForServiceRestarts(void);

/*
 * return if a restore is in progress.  called by IPC
 * handler to not allow backup schedules/runs while restoring
 */
bool isRestoreRunning()
{
    bool retVal;

    pthread_mutex_lock(&RESTORE_MTX);
    retVal = restoreRunning;
    pthread_mutex_unlock(&RESTORE_MTX);

    return retVal;
}

/*
 * starts the restore process.  will populate 'steps'
 * with the well-known strings of the steps this will
 * go through.  each step will have a corresponding
 * 'restoreStep' event signifying when the step is completed/failed.
 *
 * once complete (and successful), this will ask watchdog to restart all services.
 *
 * returns if the restore thread was able to begin.
 */
bool startRestoreProcess(icLinkedList *steps)
{
    bool ret = false;

    pthread_mutex_lock(&RESTORE_MTX);
    if (restoreRunning == false)
    {
        // set our flag
        //
        ret = restoreRunning = true;

        // fill in 'steps'
        //
        linkedListAppend(steps, strdup(RESTORE_STEP_BEGIN_STR));
        linkedListAppend(steps, strdup(RESTORE_STEP_DOWNLOAD_STR));
#ifdef CONFIG_SERVICE_NETWORK
        linkedListAppend(steps, strdup(RESTORE_STEP_NETWORK_STR));
#endif
        linkedListAppend(steps, strdup(RESTORE_STEP_CONFIG_STR));
        linkedListAppend(steps, strdup(RESTORE_STEP_COMPLETE_STR));

        if (serviceCallbackList)
        {
            linkedListClear(serviceCallbackList, NULL);
            linkedListClear(serviceRestartList, NULL);
        }
        else
        {
            initTimedWaitCond(&SERVICE_RESTORE_COND);

            serviceCallbackList = linkedListCreate();
            serviceRestartList = linkedListCreate();
        }

        // start the thread to do the restore
        //
        createDetachedThread(restoreThread, NULL, "restoreJob");
    }
    pthread_mutex_unlock(&RESTORE_MTX);

    return ret;
}

void notifyRestoreCallback(restoreCallbackInfo *input)
{
    pthread_mutex_lock(&RESTORE_MTX);
    if (serviceCallbackList)
    {
        if (linkedListDelete(serviceCallbackList, input->serviceName, linkedListStringCompareFunc, NULL))
        {
            switch (input->status)
            {
                case RESTORED_CALLBACK_COMPLETE:
                    icLogDebug(TAG, "Successfully restored service [%s] via callback", input->serviceName);
                    break;
                case RESTORED_CALLBACK_RESTART:
                    icLogInfo(TAG, "restore: successfully informed service [%s] of the 'restore dir', requested to restart service.", input->serviceName);
                    linkedListAppend(serviceRestartList, strdup(input->serviceName));
                    break;
                case RESTORED_CALLBACK_FAILED:
                    icLogDebug(TAG, "Failed to restore service [%s] via callback", input->serviceName);
                    break;
            }
        }

        pthread_cond_signal(&SERVICE_RESTORE_COND);
    }
    pthread_mutex_unlock(&RESTORE_MTX);
}

/*
 * execute the 'xhRestore.sh' and unpack into a temp dir.
 * caller must free there returned string (the temp dir)
 */
static char *extractConfig()
{
    // create a temp dir to extract to
    //
    char path[128];
    sprintf(path, "/tmp/restXXXXXX");
    if (mkdtemp(path) == NULL)
    {
        // failed to create temp folder, just use /tmp/rest
        //
        strcpy(path, "/tmp/rest");
        if (mkdir(path, 0777) != 0)
        {
            char *error = strerrorSafe(errno);
            icLogWarn(TAG, "error creating directory %s - %d %s", path, errno, error);
            free(error);
        }
    }
    else
    {
        // since tmp file was created, change the permissions to 777,
        // this allows Java Services to access files (aka NetworkMgr)
        // see ZILKER-1877, for more details
        //
        // mode is not automatically assumed to be an octal value (decimal; probably incorrect),
        // so to ensure the expected operation, need to prefix mode with a zero (0).
        if (chmod(path, 0777) != 0)
        {
            char *error = strerrorSafe(errno);
            icLogWarn(TAG, "error setting permissions for directory %s - %d %s", path, errno, error);
            free(error);
        }
    }

    // get the location of our 'xhRestore.sh' script
    //
    char script[FILENAME_MAX];
    char *homeDir = getStaticPath();

    // get the information the script will need
    // TODO: Needs implementation
    char *serverUrl = NULL;
    char *username = NULL;
    char *password = NULL;
    uint64_t identifier = 0;

    long httpCode;
    const char* restoreFile = "/tmp/restore.tgz.pgp";
    size_t bytesWritten;

    bytesWritten = urlHelperDownloadFile(serverUrl,
                                         &httpCode,
                                         username,
                                         password,
                                         CONFIG_DOWNLOAD_TIMEOUT_SECONDS,
                                         getSslVerifyProperty(SSL_VERIFY_HTTP_FOR_SERVER),
                                         true,
                                         restoreFile);

    if ((bytesWritten > 0) && (httpCode == 200))
    {
        // put it together and pass all of this information as args to the script
        //
        snprintf(script,
            FILENAME_MAX,
            "%s/bin/xhRestore.sh %s %s %"PRIu64,
            homeDir,
            path,
            restoreFile,
            identifier);
    }
    else
    {
        free(homeDir);

        icLogWarn(TAG, "Failed to download restore config. [%ld][%ld]", bytesWritten, httpCode);

        return NULL;
    }

    free(homeDir);

    // run the xhRestore.sh script
    //
    int rc = system(script);
    if (rc != 0)
    {
        icLogWarn(TAG, "restore: script command failed with rc %d", rc);
#ifdef CONFIG_DEBUG_RMA
        // show what command we ran
        //
        icLogDebug(TAG, "restore: failed running script '%s'", script);
#endif
        return NULL;
    }
    else
    {
        icLogDebug(TAG, "restore: script command success");
    }

    // return the temp dir
    //
    return strdup(path);
}

/*
 * thread to perform the restore steps
 */
static void *restoreThread(void *arg)
{
    bool allConfigRestored = false;
    bool allCallbacksOK = false;
    bool allRestartsOK = false;

    // wait a few seconds so that whatever process started this
    // can get the 'steps' as part of a response
    //
    sleep(5);
    icLogDebug(TAG, "restore: starting restore config thread...");
    broadcastRestoreEvent(RESTORE_STEP_BEGIN, RESTORE_STEP_BEGIN_STR, true);

    // download the config file.  we want this done before starting the thread so we can return failure
    //
    icLogDebug(TAG, "restore: downloading config from server...");
    char *tempDir = extractConfig();
    if (tempDir == NULL)
    {
        // failed to download config from the server
        //
        broadcastRestoreEvent(RESTORE_STEP_DOWNLOAD, RESTORE_STEP_DOWNLOAD_STR, false);
        icLogError(TAG, "restore: unable to restore RMA configuration from server.");
        pthread_mutex_lock(&RESTORE_MTX);
        restoreRunning = false;
        pthread_mutex_unlock(&RESTORE_MTX);
        return false;
    }
    broadcastRestoreEvent(RESTORE_STEP_DOWNLOAD, RESTORE_STEP_DOWNLOAD_STR, true);

    // For pre-zilker the backup has full file paths, e.g. files would be at /tmp/restX/opt/etc/communication.conf, etc.
    // For zilker the backup is done from inside /opt/etc, so the files would be at /tmp/restX/communication.conf, etc.
    // Hide this detail from our consumers by checking for the pre-zilker directory structure and passing that out as
    // the restore dir if it exists
    char *restoreDir = tempDir;
    char *preZilkerPath = stringBuilder("%s/opt/etc", tempDir);
    if (doesDirExist(preZilkerPath) == true)
    {
        restoreDir = preZilkerPath;
        icLogDebug(TAG, "restore: pre-zilker backup found, using restore dir %s", restoreDir);
    }
    else
    {
        free(preZilkerPath);
    }

    // get directories we need
    //
    char *configDir = getDynamicConfigPath();

#ifdef CONFIG_SERVICE_NETWORK
    /*
     * Lie here as we will restore network as part of the "all services".
     */
    broadcastRestoreEvent(RESTORE_STEP_NETWORK, RESTORE_STEP_NETWORK_STR, true);
#endif

    // notify all services where the RMA files are located, allowing
    // each the opportunity to parse or copy them into 'configDir'
    //
    icLogDebug(TAG, "restore: informing services of downloaded config (stored in %s)", restoreDir);
    allConfigRestored = tellAllServices(restoreDir, configDir);
    broadcastRestoreEvent(RESTORE_STEP_CONFIG, RESTORE_STEP_CONFIG_STR, allConfigRestored);

    /*
     * Wait for any services that needed to perform the restore
     * asynchronously. Even if the previous step failed, allow any callbacks/restarts to complete to
     * avoid an inconsistent state as often as possible.
     */
    icLogDebug(TAG, "Waiting for service callbacks...");
    allCallbacksOK = waitForServiceCallbacks();

    icLogDebug(TAG, "Waiting for service restarts...");
    allRestartsOK = waitForServiceRestarts();

    // cleanup temp files
    //
    icLogDebug(TAG, "restore: clearing temp dir %s", tempDir);
    deleteDirectory(tempDir);

    if (restoreDir != tempDir)
    {
        free(restoreDir);
    }
    free(tempDir);
    free(configDir);

    // send the 'complete' event
    //
    icLogDebug(TAG, "restore: sending complete event");
    broadcastRestoreEvent(RESTORE_STEP_COMPLETE,
                          RESTORE_STEP_COMPLETE_STR,
                          allConfigRestored == true && allCallbacksOK == true && allRestartsOK == true);

    // reset flag
    //
    pthread_mutex_lock(&RESTORE_MTX);
    linkedListClear(serviceCallbackList, NULL);
    linkedListClear(serviceRestartList, NULL);

    restoreRunning = false;
    pthread_mutex_unlock(&RESTORE_MTX);

    return NULL;
}

/**
 * Wait for all services that told us they would call back
 * when done. If a call never comes in then timeout and
 * remove things from the queue.
 * @return whether the entire serviceCallbackList's set called back with their notifications
 */
static bool waitForServiceCallbacks(void)
{
    bool ok = true;
    pthread_mutex_lock(&RESTORE_MTX);
    if (serviceCallbackList)
    {
        while (linkedListCount(serviceCallbackList) != 0)
        {
            if (incrementalCondTimedWait(&SERVICE_RESTORE_COND, &RESTORE_MTX, 15 * 60) == ETIMEDOUT)
            {
                icLinkedListIterator* iterator = linkedListIteratorCreate(serviceCallbackList);

                icLogInfo(TAG, "Restore timed out waiting for services...");

                while (linkedListIteratorHasNext(iterator))
                {
                    icLogInfo(TAG, "[%s] failed to report config restore status!",
                              (char *) linkedListIteratorGetNext(iterator));
                }

                linkedListIteratorDestroy(iterator);

                /*
                 * This is an error condition so go ahead and clear the list.
                 */
                linkedListClear(serviceCallbackList, NULL);

                ok = false;
            }
        }
    }
    pthread_mutex_unlock(&RESTORE_MTX);

    return ok;
}

/**
 * Wait for all services that told us they need to be
 * restarted.
 * @return whether the entire servficeRestartList successfully restarted
 */
static bool waitForServiceRestarts(void)
{
    bool ok = true;
    pthread_mutex_lock(&RESTORE_MTX);
    if (serviceRestartList)
    {
        icLinkedListIterator* iterator;

        /* TODO: This should probably be moved down into the watchdog
         * This is so that the services will get the proper start
         * phase system.
         */

        /*
         * Stop all the services so that they do not get bad information
         * on the restart.
         */
        iterator = linkedListIteratorCreate(serviceRestartList);
        while (linkedListIteratorHasNext(iterator))
        {
            char*  serviceName = linkedListIteratorGetNext(iterator);
            bool stopService = false;

            icLogInfo(TAG,
                      "Stopping service [%s] per restart request...",
                      serviceName);

            if ((watchdogService_request_STOP_SERVICE_timeout(serviceName, &stopService, SERVICE_STOP_TIMEOUT_SECONDS) == IPC_SUCCESS) &&
                    stopService == true)
            {
                icLogDebug(TAG, "Successfully stopped service [%s]", serviceName);
            }
            else
            {
                icLogError(TAG, "Failed to stop service [%s]", serviceName);
                ok = false;
            }

#ifdef CONFIG_DEBUG_SINGLE_PROCESS
            int checkCount = 0;
            while (getServiceState(serviceName) == true && checkCount < 5)
            {
                sleep(1);
                checkCount++;
            }
#endif

        }
        linkedListIteratorDestroy(iterator);

        /*
         * Now start all the services in order.
         */
        iterator = linkedListIteratorCreate(serviceRestartList);
        while (linkedListIteratorHasNext(iterator))
        {
            char*  serviceName = linkedListIteratorGetNext(iterator);
            bool startResponse = false;

            icLogInfo(TAG,
                      "Restarting service [%s]...",
                      serviceName);

            if ((watchdogService_request_START_SERVICE_timeout(serviceName, &startResponse, SERVICE_RESTART_TIMEOUT_SECONDS) == IPC_SUCCESS) &&
                    startResponse == true)
            {
                icLogDebug(TAG, "Successfully restarted service [%s]", serviceName);
            }
            else
            {
                icLogError(TAG, "Failed to restart service [%s]", serviceName);
                ok = false;
            }
        }
        linkedListIteratorDestroy(iterator);
    }
    pthread_mutex_unlock(&RESTORE_MTX);

    return ok;
}

/*
 * tell all services where the restore directory is at
 * IPC failures here are considered recoverable and will be reported upwards as a step failure
 */
static bool tellAllServices(char* restoreDir, char* configDir)
{
    // first get the list of service names
    //
    allServiceNames *all = create_allServiceNames();
    bool ok = true;
    IPCCode rc;

    if ((rc = watchdogService_request_GET_ALL_SERVICE_NAMES(all)) == IPC_SUCCESS)
    {
        // create the 'details' of the restore to pass along to all services
        //
        configRestoredInput *restoreDetails = create_configRestoredInput();

        restoreDetails->tempRestoreDir = strdup(restoreDir);
        restoreDetails->dynamicConfigPath = strdup(configDir);

        // TODO: Lock down while we inform all the services so we can populate the list.
        // loop through each one, informing it where the "temp restore dir" is.
        //
        icLinkedListIterator *loop = linkedListIteratorCreate(all->list);

        while (linkedListIteratorHasNext(loop) == true && ok == true)
        {
            configRestoredOutput *restoredOutput = create_configRestoredOutput();
            char *svcName = (char *) linkedListIteratorGetNext(loop);

            // get information for this service name
            //
            processInfo *info = create_processInfo();

            if ((rc = watchdogService_request_GET_SERVICE_BY_NAME(svcName, info)) == IPC_SUCCESS)
            {

                // make sure this service has IPC capabilities
                //
                if (info->ipcPortNum == 0)
                {
                    // service doesn't have IPC, so it doesn't do RMA restores
                    icLogInfo(TAG,"restore: skipping service [%s] because it has no IPC port configured",svcName);
                    destroy_processInfo(info);
                    destroy_configRestoredOutput(restoredOutput);
                    continue;
                }
                bool removeFromList = true;

                /*
                 * We must add to the list before calling the config restore because
                 * we could get the IPC callback _before_ we are finished handling the
                 * return type.
                 */
                pthread_mutex_lock(&RESTORE_MTX);
                linkedListAppend(serviceCallbackList, strdup(svcName));
                pthread_mutex_unlock(&RESTORE_MTX);

                if ((rc = configRestored((uint16_t)info->ipcPortNum, restoreDetails, restoredOutput, 120)) == IPC_SUCCESS)
                {
                    switch (restoredOutput->action)
                    {
                        case CONFIG_RESTORED_CALLBACK:
                            icLogInfo(TAG, "restore: successfully informed service [%s] of the 'restore dir', waiting for service to indicate status of restore.", svcName);

                            /*
                             * Going to leave this entry in the queue since we are waiting for the callback.
                             */
                            removeFromList = false;
                            break;
                        case CONFIG_RESTORED_COMPLETE:
                            icLogInfo(TAG, "restore: successfully informed service [%s] of the 'restore dir'", svcName);
                            break;
                        case CONFIG_RESTORED_RESTART:
                            icLogInfo(TAG, "restore: successfully informed service [%s] of the 'restore dir', requested to restart service. (Ignored)", svcName);

//                            // TODO: uncomment this, whenever UI decides a reboot is no longer needed during Activation/RMA
//                            // XHFW-635 : Problem with rebooting once RMA is complete
//                            //
//                            icLogInfo(TAG, "restore: successfully informed service [%s] of the 'restore dir', requested to restart service.", svcName);
//                            pthread_mutex_lock(&RESTORE_MTX);
//                            linkedListAppend(serviceRestartList, strdup(svcName));
//                            pthread_mutex_unlock(&RESTORE_MTX);

                            break;
                        case CONFIG_RESTORED_FAILED:
                            /*
                             * The service tried but failed to restore the old configuration. This is considered
                             * an unrecoverable error, but should not fail the RMA. The CPE is now in an inconsistent
                             * state, but would otherwise have to be reset and activated, so offer a chance at recovery
                             * by suppressing the failure.
                             */
                            icLogWarn(TAG, "restore: service [%s] failed to perform the restore", svcName);
                            break;
                    }
                }
                else
                {
                    icLogError(TAG, "restore: unable to inform service [%s] of the 'restore dir': %d - %s\n", svcName, rc, IPCCodeLabels[rc]);
                    ok = false;
                }

                if (removeFromList)
                {
                    /*
                     * Only the callback will keep an entry in the list. Thus
                     * we need to remove the last added service.
                     */
                    pthread_mutex_lock(&RESTORE_MTX);
                    linkedListDelete(serviceCallbackList, svcName, linkedListStringCompareFunc, NULL);
                    pthread_mutex_unlock(&RESTORE_MTX);
                }
            }
            else
            {
                icLogError(TAG, "restore: unable to get information about service %s : %d - %s\n", svcName, rc, IPCCodeLabels[rc]);
                ok = false;
            }

            destroy_processInfo(info);
            destroy_configRestoredOutput(restoredOutput);
        }

        // cleanup
        //
        linkedListIteratorDestroy(loop);
        destroy_configRestoredInput(restoreDetails);
    }
    else
    {
        icLogWarn(TAG, "restore: unable to get list of service names from watchdog : %d - %s\n", rc, IPCCodeLabels[rc]);
        ok = false;
    }

    destroy_allServiceNames(all);

    return ok;
}
