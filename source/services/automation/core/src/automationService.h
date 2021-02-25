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
// Created by tlea on 3/29/18.
//

/**
 * The Automation Service hosts a collection of state machines.  Each machine is an instance
 * of an 'automation' that runs within the littlesheens runtime.
 *
 * A machine is an instance of a specification.  Machines take messages as input, optionally
 * change state, and optionally emit messages which can trigger further action.  A machine
 * performs each step atomically and in a non-blocking manner.
 *
 * All events within the platform are fed to all active machines.  Machine specifications can
 * either react or ignore these messages.
 *
 * Messages emitted by a machine are handed off to various message target handlers.  These
 * handlers process the message and optionally return some JSON response message that is fed
 * back into the state machine which can trigger further state transitions and additional
 * messages to be emitted.
 *
 * More details about littlesheens can be found here: https://github.com/Comcast/littlesheens
 */

#ifndef ZILKER_AUTOMATIONSERVICE_H
#define ZILKER_AUTOMATIONSERVICE_H

#include <stdbool.h>
#include <icIpc/ipcStockMessagesPojo.h>
#include <icTypes/icLinkedList.h>
#include <icLog/logging.h>
#include <cjson/cJSON.h>

#define LOG_TAG "automationService"

#ifndef unused
#define unused(v) ((void) (v))
#endif

#ifndef unlikely
#if defined(__GNUC__) || defined(__clang__)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define unlikely(x) (x)
#endif
#endif

/*
 * The verbosity level is similiar to a priority level.
 * The higher the level the more import the message
 * will be. Thus if the global verbosity level
 * is set to VERBOSITY_LEVEL_0 then everything will
 * be printed. If the global verbosity level
 * is VERBOSITY_LEVEL_2 then minimal items will be
 * printed.
 */
#define VERBOSITY_LEVEL_0 0
#define VERBOSITY_LEVEL_1 1
#define VERBOSITY_LEVEL_2 2

#ifdef CONFIG_DEBUG_AUTOMATIONS
#define VERBOSITY_LEVEL VERBOSITY_LEVEL_0
#else
#define VERBOSITY_LEVEL VERBOSITY_LEVEL_1
#endif //CONFIG_DEBUG_AUTOMATIONS

/**
 * Debug print a message with the function name and log tag only
 * if the statement is given verbosity is greater than, or equal to,
 * the global verbosity level.
 */
#define dbg(verbosity, format, ...) \
do {\
    if (verbosity >= VERBOSITY_LEVEL) {\
        icLogDebug(LOG_TAG, "%s: " format, __func__, ##__VA_ARGS__);\
    }\
} while (0)

#define EVENT_CODE_TIMER_TICK 499

/**
 * Information about automations in the service
 */
typedef struct
{
    char* id;
    bool enabled;
    uint32_t dateCreatedSecs; //the date and time in seconds when this automation was created
    uint32_t messagesConsumed;
    uint32_t messagesEmitted;
} MachineInfo;

/**
 * Phase 1 of initialization process.
 * All initialization that is required before IPC is
 * turned on must be placed here.
 *
 * @return true on success
 */
bool automationServiceInitPhase1(void);

/**
 * Phase 2 of the initialization process.
 * All initialization that must before automations
 * are executed must be placed here.
 */
void automationServiceInitPhase2(void);

/**
 * Clean up and terminate the Automation Service
 */
void automationServiceCleanup(void);

/**
 * Restore backed up automations during a "restore" process.
 *
 * All current automations will be destroyed and then replaced
 * by the restored automations.
 *
 * The automation service will be completely restarted internally.
 *
 * @param input The restore from/to directories.
 */
void automationServiceRestore(const configRestoredInput* input);

/**
 * Retrieve the current state of a machine
 *
 * @param machineId the unique name of the machine
 * @param state current state of the machine.  Caller frees.
 * @return true on success
 */
bool getMachineState(const char* machineId, char** state);

/**
 * Add a machine, which will be enabled and persisted.
 *
 * @param machineId the unique identifier of the machine instance
 * @param specification the specification of the machine
 * @param enabled whether the machine should be enabled
 * @return true on success
 */
bool automationServiceAddMachine(const char *machineId, char *specification, bool enabled);

/**
 * Remove an machine, destroying any related resources.
 *
 * @param machineId the unique identifier of the machine instance
 * @return true on success
 */
bool automationServiceRemoveMachine(const char* machineId);

/**
 * Enable or disable a machine.  A disabled machine does not receive nor generate messages.
 *
 * @param machineId the unique identifier of the machine instance
 * @param enabled true to enable the machine, false to disable
 * @return true on success
 */
bool automationServiceSetMachineEnabled(const char* machineId, bool enabled);

/**
 * Update a machine's specification.
 *
 * @param machineId the unique identifier of the machine instance
 * @param specification the JSON specification of the machine
 * @param originalSpecification the original specification of the machine
 * @param transcoderVersion the transcoder version used
 */
void automationServiceSetMachineSpecification(const char *machineId, const char *specification,
                                              const char *originalSpecification, int transcoderVersion);

/**
 * Retrieve a list of MachineInfos registered within the service.
 *
 * @return a list of MachineInfos.  Caller frees with linkedListDestroy(list, machineInfoFreeFunc)
 */
icLinkedList* automationServiceGetMachineInfos();

/**
 * The icLinkedList free function used to free the results of automationServiceGetMachineInfos()
 */
void machineInfoFreeFunc(void*);

#endif //ZILKER_AUTOMATIONSERVICE_H
