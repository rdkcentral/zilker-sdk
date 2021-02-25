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
 * ipcHandler.c
 *
 * Implement functions that were stubbed from the
 * generated IPC Handler.  Each will be called when
 * IPC requests are made from various clients.
 *
 * Author: jelderton - 7/7/15
 *-----------------------------------------------*/

#include <stdio.h>

#include <icLog/logging.h>                      // logging subsystem
#include <icIpc/ipcMessage.h>
#include <icIpc/eventConsumer.h>
#include <icUtil/stringUtils.h>
#include <icSystem/runtimeAttributes.h>
#include <watchdog/serviceStatsHelper.h>
#include <icTime/timeUtils.h>
#include "backupRestoreService_ipc_handler.h"
#include "common.h"                             // common set of defines for backup/restore service
#include "backupJob.h"
#include "restoreJob.h"

/**
 * obtain the current runtime statistics of the service
 *   input - if true, reset stats after collecting them
 *   output - map of string/string to use for getting statistics
 **/
IPCCode handle_backupRestoreService_GET_RUNTIME_STATS_request(bool input, runtimeStatsPojo *output)
{
    // gather stats about Event and IPC handling
    //
    collectEventStatistics(output, input);
    collectIpcStatistics(get_backupRestoreService_ipc_receiver(), output, input);

    // memory process stats
    collectServiceStats(output);

    output->serviceName = strdup(BACKUP_RESTORE_SERVICE_NAME);
    output->collectionTime = getCurrentUnixTimeMillis();

    return IPC_SUCCESS;
}

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 **/
IPCCode handle_backupRestoreService_GET_SERVICE_STATUS_request(serviceStatusPojo *output)
{
    // TODO: return status
    return IPC_SUCCESS;
}

/**
 * inform a service that the configuration data was restored, into 'restoreDir'.
 * allows the service an opportunity to import files from the restore dir into the
 * normal storage area.  only happens during RMA situations.
 *   details - both the 'temp dir' the config was extracted to, and the 'target dir' of where to store
 */
IPCCode handle_backupRestoreService_CONFIG_RESTORED_request(configRestoredInput *input, configRestoredOutput* output)

{
    // nothing to do, we don't have configuration to restore
    //
    output->action = CONFIG_RESTORED_COMPLETE;
    return IPC_SUCCESS;
}

/**
 * Notification that a configuration file has been altered, meaning a new backup to the server is required
 **/
IPCCode handle_CONFIG_UPDATED_request()
{
    if (isBackupRunning() == false)
    {
        // start our timer (if not already running)
        //
        scheduleBackupIfPossible();
    }
    return IPC_SUCCESS;
}

/**
 * Same as CONFIG_UPDATED, however force it to occur now instead of waiting
 **/
IPCCode handle_FORCE_BACKUP_request()
{
    // only allowed if activated and not restoring
    //
    if (isBackupRunning() == false)
    {
        icLogInfo(BACKUP_LOG, "forcing backup due to IPC request");
        forceBackup();
    }
    else
    {
        icLogWarn(BACKUP_LOG, "ignoring request to start backup, activation not complete or restore in progress");
    }
    return IPC_SUCCESS;
}

/**
 * Start the restore process (during RMA).  returns a list of strings, representing the steps this will go through.
                Each step will be part of the 'restoreStep' event, signaling when each is complete (or failed).
 *   output - response from service
 **/
IPCCode handle_START_RESTORE_PROCESS_request(restoreStepResults *output)
{
    // prevent other backup requests from coming in while the restore is in progress
    //
    if (isRestoreRunning() == true)
    {
        icLogDebug(BACKUP_LOG, "ignoring request to 'restore'; a restore is already in progress.");
        return IPC_GENERAL_ERROR;
    }

    // cancel the current scheduled backup (if one exists)
    //
    cancelScheduledBackup();

    // start the restore, filling in the 'steps' this will
    // go through so the caller can show progress (if desired)
    //
    output->success = startRestoreProcess(output->results);

    return IPC_SUCCESS;
}

IPCCode handle_RESTORE_CALLBACK_request(restoreCallbackInfo *input)
{
    notifyRestoreCallback(input);

    return IPC_SUCCESS;
}
