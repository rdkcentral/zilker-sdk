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
 * Author: jelderton 7/7/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <icBuildtime.h>
#include <icLog/logging.h>                      // logging subsystem
#include <icIpc/ipcMessage.h>           // IPC library
#include <icIpc/ipcReceiver.h>
#include <icIpc/ipcStockMessages.h>
#include <icIpc/eventConsumer.h>
#include <icReset/factoryReset.h>
#include <icTime/timeUtils.h>
#include <watchdog/watchdogService_event.h>
#include <watchdog/serviceStatsHelper.h>
#include "watchdogService_ipc_handler.h"       // local IPC handler
#include "common.h"
#include "configParser.h"
#include "procMgr.h"
#include "systemStatsCollector.h"
#include "broadcastEvent.h"                     // local event producer
#include <commMgr/commService_ipc.h>
#include <icReset/shutdown.h>

#ifdef CONFIG_SERVICE_POWER
#include <xRebootMem/xRebootMem.h>
#include <powerService/powerService_event.h>
#endif


/*
 * private variables
 */
static uint64_t watchdogStartTime = 0;

/*
 * Populates watchdog (itself process info)
 */
static void createWatchdogProcessInfo(processInfo *info)
{
    info->serviceName = strdup(WATCH_DOG_SERVICE_NAME);
    info->ipcPortNum = (uint32_t) WATCHDOGSERVICE_IPC_PORT_NUM;
    info->running = true;
    info->deathCount = 0;     // Since watchdog does not count its own death, just use this for filler
    info->processId = (uint16_t ) getpid();
    info->runStartTime = watchdogStartTime;
}

/*
 * Stores the start time for watchdog
 */
void storeWatchdogStartTime(uint64_t startTime)
{
    watchdogStartTime = startTime;
}

/*
 * copy from 'serviceDefinition' to 'processInfo' pojo
 */
void transferServiceDefinitionToProcessInfo(serviceDefinition *def, processInfo *output)
{
    // copy information into 'output'
    //
    if (def->serviceName != NULL)
    {
        output->serviceName = strdup(def->serviceName);
    }
    if (def->currentPid <= 0)
    {
        // don't typecast negative numbers as positive integers
        output->processId = 0;
    }
    else
    {
        output->processId = (uint64_t)def->currentPid;
    }
    if (def->currentPid > 0)
    {
        output->running = true;
    }
    output->runStartTime = convertTime_tToUnixTimeMillis(def->lastRestartTime);
    output->autoStart = def->autoStart;
    output->restartOnFail = def->restartOnFail;
    output->expectsAck = def->expectStartupAck;
    output->ackReceivedTime = convertTime_tToUnixTimeMillis(def->lastActReceivedTime);
    output->ipcPortNum = def->serviceIpcPort;
    output->deathCount = def->deathCount;
    output->isJava = def->isJavaService;
}

/**
 * obtain the current runtime statistics of the service
 *   input - if true, reset stats after collecting them
 *   output - map of string/string to use for getting statistics
 **/
IPCCode handle_watchdogService_GET_RUNTIME_STATS_request(bool input, runtimeStatsPojo *output)
{
    // gather stats about Event and IPC handling
    //
    collectEventStatistics(output, input);
    collectIpcStatistics(get_watchdogService_ipc_receiver(), output, input);

    // memory process stats
    collectServiceStats(output);

    // gather Reboot stats
    collectRebootStats(output);

    // gather system memory stats
    collectSystemStats(output);

    // gather service stats
    collectServiceListStats(output);

    output->serviceName = strdup(WATCH_DOG_SERVICE_NAME);
    output->collectionTime = getCurrentUnixTimeMillis();

    return IPC_SUCCESS;
}

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 **/
IPCCode handle_watchdogService_GET_SERVICE_STATUS_request(serviceStatusPojo *output)
{
    // nothing really to report
    //
    return IPC_SUCCESS;
}

/**
 * inform a service that the configuration data was restored, into 'restoreDir'.
 * allows the service an opportunity to import files from the restore dir into the
 * normal storage area.  only happens during RMA situations.
 *   details - both the 'temp dir' the config was extracted to, and the 'target dir' of where to store
 */
IPCCode handle_watchdogService_CONFIG_RESTORED_request(configRestoredInput *input, configRestoredOutput* output)
{
    // nothing to do, we don't have configuration to restore
    //
    output->action = CONFIG_RESTORED_COMPLETE;
    return IPC_SUCCESS;
}

/**
 * return a service by the name
 *   input - name of the service
 *   output - response from service
 **/
IPCCode handle_GET_SERVICE_BY_NAME_request(char *input, processInfo *output)
{
    if (input == NULL)
    {
        icLogError(WDOG_LOG, "%s: service name is NULL", __FUNCTION__);
        return IPC_INVALID_ERROR;
    }

    // grab itself's process data
    if (strcmp(input, WATCH_DOG_SERVICE_NAME) == 0)
    {
        createWatchdogProcessInfo(output);
        return IPC_SUCCESS;
    }

    // ask our config for this definition
    // and if found, populate the output
    //
    serviceDefinition *def = getServiceForName((const char *)input);
    if (def != NULL)
    {
        // copy information into 'output'
        //
        transferServiceDefinitionToProcessInfo(def, output);
        destroyServiceDefinition(def);
        return IPC_SUCCESS;
    }

    // couldn't find that process, so return an error
    //
    icLogDebug(WDOG_LOG, "unable to find process with name %s", input);
    return IPC_INVALID_ERROR;
}

/**
 * return a list of all known serviceNames
 *   output - list of the names of all known services
 **/
IPCCode handle_GET_ALL_SERVICE_NAMES_request(allServiceNames *output)
{
    // get all of the service names, and save them into the output->list
    //
    getAllServiceNames(output->list);

    // add watchdog to the service list
    //
    put_serviceName_in_allServiceNames_list(output, strdup(WATCH_DOG_SERVICE_NAME));

    return IPC_SUCCESS;
}

/**
 * return a list of the process info for all known services
 *   output - list of the process info for all known services
 **/
IPCCode handle_GET_ALL_SERVICES_request(allServices *output)
{
    // get all of the services, and save them into the out->list
    //
    getAllServiceProcessInfo(output->services);

    // add watchdog to the service list
    //
    processInfo *watchdogService = calloc(1, sizeof(processInfo));
    createWatchdogProcessInfo(watchdogService);
    put_processInfo_in_allServices_services(output, watchdogService);

    return IPC_SUCCESS;
}

/**
 * shutdown all services without forcing a reboot (NOTE: on JVM devices,
                does not exit the JVM - leaving the WatchdogService running)
 *   input - value to send to the service
 **/
IPCCode handle_SHUTDOWN_ALL_SERVICES_request(shutdownOptions *input)
{
    // stop all of the services
    //
    operationOnAllProcesses(STOP_PROCESS_ACTION, input->forReset);

    // if input options have 'exit == true', then kill ourself as well
    //
    if (input->exit == true)
    {
#ifdef CONFIG_DEBUG_SINGLE_PROCESS
        // Shutdown ipc sender to ensure any hanging on a receive times out immediately
        ipcSenderShutdown();
#endif
    }
    return IPC_SHUT_DOWN;
}

/**
 * shutdown all services then perform a reset to factory.  If the platform
                supports reboot AND the 'exit' option is true, this will reboot the device
                after the reset to factory.
 *   input - value to send to the service
 **/
IPCCode handle_SHUTDOWN_AND_RESET_TO_FACTORY_request(shutdownOptions *input)
{
    // perform reset to factory and kill all processes.  note that if the platform supports
    // reboot, that will be done as part of this function call
    //
    resetToFactory();

    // if we get here, then this platform DOES NOT supports reboot.
    // see if we kill ourself
    //
    if (input->exit == true)
    {
#ifdef CONFIG_DEBUG_SINGLE_PROCESS
        // Shutdown ipc sender to ensure any hanging on a receive times out immediately
        ipcSenderShutdown();
#endif
    }
    return IPC_SHUT_DOWN;
}

/**
 * shutdown then startup all services without forcing a reboot
                (NOTE: on JVM devices, does not exit the JVM - leaving the
                WatchdogService running)
 *   input - value to send to the service
 **/
IPCCode handle_RESTART_ALL_SERVICES_request(shutdownOptions *input)
{
    // TODO: potentially trim out these 'restart' args since we're not in the JRE
    //
    operationOnAllProcesses(RESTART_PROCESS_ACTION, input->forReset);
    return IPC_SUCCESS;
}

/**
 * start a single service (by name) and NOT restart it
 *   input - name of the service
 *   output - response from service (boolean)
 **/
IPCCode handle_STOP_SERVICE_request(char *input, bool *output)
{
    // stop the single service (no need for the 'start' function)
    //
    *output = operationOnSingleProcesses(STOP_PROCESS_ACTION, input);
    return IPC_SUCCESS;
}

/**
 * starts a single service by name, if it's not already running
 *   input - name of the service
 *   output - response from service (boolean)
 **/
IPCCode handle_START_SERVICE_request(char *input, bool *output)
{
    // start the single service
    //
    *output = operationOnSingleProcesses(START_PROCESS_ACTION, input);
    return IPC_SUCCESS;
}

/**
 * stop then start a single service by name
 *   input - name of the service
 *   output - response from service (boolean)
 **/
IPCCode handle_RESTART_SERVICE_request(char *input, bool *output)
{
    // restart the single service
    //
    *output = operationOnSingleProcesses(RESTART_PROCESS_ACTION, input);
    return IPC_SUCCESS;
}

/**
 * stop then start a single service by name because there was some problem with it
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_RESTART_SERVICE_FOR_RECOVERY_request(char *input, bool *output)
{
    // restart the single service
    //
    *output = operationOnSingleProcesses(RESTART_FOR_RECOVERY_PROCESS_ACTION, input);
    return IPC_SUCCESS;
}

/**
 * start a group of services
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_START_GROUP_request(char *input, bool *output)
{
    // start the service group
    //
    operationOnGroupOfProcesses(START_PROCESS_ACTION, input);
    *output = true;
    return IPC_SUCCESS;
}

/**
 * stop a group of services
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_STOP_GROUP_request(char *input, bool *output)
{
    // stop the service group (no need for the 'start' function)
    //
    operationOnGroupOfProcesses(STOP_PROCESS_ACTION, input);
    *output = true;
    return IPC_SUCCESS;
}

/**
 * restart a group of services
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_RESTART_GROUP_request(char *input, bool *output)
{
    // restart the service group
    //
    operationOnGroupOfProcesses(RESTART_PROCESS_ACTION, input);
    *output = true;
    return IPC_SUCCESS;
}

/**
 * stops monitoring of a particular service (meaning if it dies we will not restart it)
 *   input - name of the service
 *   output - response from service (boolean)
 **/
IPCCode handle_STOP_MONITORING_request(char *input, bool *output)
{
    if (input != NULL && output != NULL)
    {
        bool worked = stopMonitoringService((const char *)input);
        *output = worked;
        return IPC_SUCCESS;
    }

    // bad input/output variables
    //
    return IPC_INVALID_ERROR;
}

/**
 * Called by a single service once it has completed initialization.
                Causes the WATCHDOG_INIT_COMPLETE once all services have reported in.
                If the optional port/msg numbers are larger then 0, they will be
                used to ask the service for "status".  That message signature needs
                to be string/string hash as the output so it can be displayed.
 *   input - value to send to the service
 **/
IPCCode handle_ACK_SERVICE_STARTUP_request(ackServiceDef *input)
{
    if (input == NULL)
    {
        return IPC_INVALID_ERROR;
    }
    if (input->serviceName != NULL)
    {
        icLogDebug(WDOG_LOG, "got acknowledgement from service '%s'; it must be ready for business", input->serviceName);
    }

    // first set the 'acknowledged' flag on this service
    //
    if (acknowledgeServiceStarted(input) == false)
    {
        icLogWarn(WDOG_LOG, "received ack from unknown service '%s'", (input->serviceName != NULL) ? input->serviceName : "NULL");
        return IPC_GENERAL_ERROR;
    }

    return IPC_SUCCESS;
}

/**
 * returns true if all services are started up.  necessary in case something missed the WATCHDOG_INIT_COMPLETE event
 *   output - response from service (boolean)
 **/
IPCCode handle_ARE_ALL_SERVICES_STARTUP_request(bool *output)
{
    *output = areAllServicesStarted();
    return IPC_SUCCESS;
}

/**
 * re-extract assets now that Activation is done (touchstone only for now)
 **/
IPCCode handle_ACTIVATION_COMPLETED_request()
{
    // TODO: handle ACTIVATION_COMPLETE IPC request
    //
    return IPC_GENERAL_ERROR;
}


/**
 * Called by powerService as we go into/out-of low power modes.  Note the input value can be typecast to 'lowPowerLevel'
 *   input - value to send to the service
 **/
IPCCode handle_LOW_POWER_MODE_CHANGED_WATCHDOG_request(int32_t input)
{

    // set LPM mode into xRebootMemHAL.  Note this is only supported on
    // devices that have the powerService (and xRebootMem HAL)
    //
#ifdef CONFIG_SERVICE_POWER
    // convert integer to lowPowerLevel, then pass along to xRebootMemHAL
    //
    lowPowerLevel level = (lowPowerLevel)input;
    switch (level)
    {
        case LOW_POWER_LEVEL_NORMAL:
        default:
            hal_xRebootMem_setACPowerConnected();
            break;

        case LOW_POWER_LEVEL_PREP:
            hal_xRebootMem_setLPMModePrepState();
            break;

        case LOW_POWER_LEVEL_BEGIN:
            hal_xRebootMem_setLPMModeBeginState();
            break;

        case LOW_POWER_LEVEL_TEARDOWN:
            hal_xRebootMem_setLPMModeTearddownState();
            break;

        case LOW_POWER_LEVEL_STANDBY:
            hal_xRebootMem_setLPMModeStandbyState();
            break;

        case LOW_POWER_LEVEL_SUSPEND:
            hal_xRebootMem_setLPMModeSuspendState();
            break;
    }
#endif

    return IPC_SUCCESS;
}

/**
 * Schedule a system reboot after a configurable delay
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_REBOOT_SYSTEM_request(rebootRequest *input, bool *output)
{
    // convert 'input' to the reboot reason
    //
    shutdownReason reason = SHUTDOWN_UNKNOWN;
    IPCCode rc = IPC_GENERAL_ERROR;

    if (input != NULL)
    {
        if (input->shutdownReason != NULL)
        {
            int i = 0;
            for (i = 0; shutdownReasonNames[i] != NULL; i++)
            {
                if (strcasecmp(shutdownReasonNames[i], input->shutdownReason) == 0)
                {
                    // found a match
                    reason = (shutdownReason) i;
                    break;
                }
            }
        }

        if (input->delaySeconds > 0)
        {
            icDelayedShutdown(reason, input->delaySeconds);
        }
        else
        {
            icShutdown(reason);
        }

        *output = true;
        rc = IPC_SUCCESS;
    }
    else
    {
        *output = false;
        rc = IPC_INVALID_ERROR;
    }

    return rc;
}


