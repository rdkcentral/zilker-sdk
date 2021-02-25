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

/*
 * procMgr.h
 *
 *  Created on: Apr 4, 2011
 *      Author: gfaulkner
 */

#ifndef PROCMGR_H_
#define PROCMGR_H_

#include "configParser.h"

// available options for the 'actionOnMaxRestarts' variable
typedef enum
{
    REBOOT_ACTION = 1,
    STOP_RESTARTING_ACTION,
} restartAction;

typedef enum
{
    START_PROCESS_ACTION,
    STOP_PROCESS_ACTION,
    RESTART_PROCESS_ACTION,
    RESTART_FOR_RECOVERY_PROCESS_ACTION
} operationAction;

/**
 * starts all configured processes and then waits for one of them to die, and handles that
 * this function DOES NOT return and is generally called from the main thread.
 */
void startConfiguredProcessesAndWait(const char *configDir, const char *homeDir);

/*
 * populates the supplied linked list with process info,
 * which are duplicates of the 'serviceDefinition'
 * within the set of known serviceDefinition objects.
 * caller must free the items added to the linked list.
 * Via destroy_processInfo
 */
void getAllServiceProcessInfo(icLinkedList *target);

/*
 * populates the supplied linked list with string values,
 * which are duplicates of the 'serviceName' value contained
 * within the set of known serviceDefinition objects.
 * caller must free the items added to the linked list.
 */
void getAllServiceNames(icLinkedList *target);

/*
 * locates the service with the serviceName matching 'proc->serviceName'.
 * if found, sets the "lastAckReceivedTime" to now and returns true.
 */
bool acknowledgeServiceStarted(ackServiceDef *proc);

/*
 * returns true if all services are started (and the WATCHDOG_INIT_COMPLETE event was sent)
 */
bool areAllServicesStarted();

/*
 * counts the number of services that have 'expectStartupAck' set to true,
 * but have not sent the ACK notification yet.  Used to help determine if
 * all critical services are initialized and ready.
 */
uint16_t countServicesToBeAcknowledged();

/*
 * counts the number of services that have 'expectStartupAck' and
 * 'singlePhaseStartup' set to true, but have not sent the ACK
 * notification yet.  Used to help determine if all singlePhase services
 * are initialized and ready.
 */
uint16_t countSinglePhaseServicesToBeAcknowledged();

/**
 * disable the 'restartOnFail' flag on the service with this name
 * returns false if the named process is not found
 */
bool stopMonitoringService(const char *serviceName);

/**
 * locates the service with this name.  if found returns
 * a clone of the definition. caller MUST free the returned object.
 *
 * @see destroyServiceDefinition
 */
serviceDefinition *getServiceForName(const char *serviceName);

/**
 * starts, stops, or restarts all known services
 */
void operationOnAllProcesses(operationAction action, bool forceKill);

/**
 * starts, stops, or restarts a single processes with matching 'serviceName'
 * returns true if the operation is executed successfully, false otherwise.
 */
bool operationOnSingleProcesses(operationAction action, char *serviceName);

/**
 * starts, stops, or restarts a group of processes with matching 'groupName'
 */
void operationOnGroupOfProcesses(operationAction action, char *groupName);

/*
 * called to launch a service.
 * Exposed for use by the "single main" binary
 */
void startProcess(serviceDefinition *procDef, bool restartAfterCrash);

/**
 * Notification that all 'singlePhaseStartup' services are complete.
 * Exposed for use by the "single main" binary
 */
//void notifySinglePhaseStartupComplete();

#endif /* PROCMGR_H_ */
