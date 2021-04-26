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

#include <icUtil/stringUtils.h>
#include <icLog/logging.h>
#include <icIpc/eventConsumer.h>
#include <cslt/cslt.h>
#include <errno.h>
#include <cslt/sheens.h>
#include <propsMgr/paths.h>
#include <watchdog/serviceStatsHelper.h>
#include <inttypes.h>
#include <commMgr/commService_ipc.h>
#include <deviceHelper.h>
#include <commonDeviceDefs.h>
#include <icTime/timeUtils.h>

#include "automationService_ipc_handler.h"
#include "automationService.h"
#include "automationBroadcastEvent.h"
#include "automationServiceTranscoder.h"


/**
 * obtain the current runtime statistics of the service
 *   input - if true, reset stats after collecting them
 *   output - map of string/string to use for getting statistics
 **/
IPCCode handle_automationService_GET_RUNTIME_STATS_request(bool input, runtimeStatsPojo *output)
{
    // gather stats about Event and IPC handling
    //
    collectEventStatistics(output, input);
    collectIpcStatistics(get_automationService_ipc_receiver(), output, input);

    // memory process stats
    collectServiceStats(output);

    output->serviceName = strdup(AUTOMATION_SERVICE_NAME);
    output->collectionTime = getCurrentUnixTimeMillis();

    return IPC_SUCCESS;
}

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 **/
IPCCode handle_automationService_GET_SERVICE_STATUS_request(serviceStatusPojo *output)
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
IPCCode handle_automationService_CONFIG_RESTORED_request(configRestoredInput* input, configRestoredOutput* output)
{
    automationServiceRestore(input);
    output->action = CONFIG_RESTORED_COMPLETE;

    return IPC_SUCCESS;
}

/**
 * Create an automation
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_CREATE_AUTOMATION_request(AutomationRequest *input)
{
    bool value = false;
    IPCCode ret;

    char* specification = input->spec;

    if (specification && (strlen(specification) > 0))
    {
        value = automationServiceAddMachine(input->id, specification, input->enabled);
    }

    if (value) {
        broadcastAutomationCreatedEvent(input->id, input->requestId, input->enabled);
        ret = IPC_SUCCESS;
    } else {
        ret = IPC_GENERAL_ERROR;
    }

    return ret;
}

/**
 * Delete an automation
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_DELETE_AUTOMATION_request(DeleteAutomationRequest *input)

{
    bool value;
    IPCCode ret;

    value = automationServiceRemoveMachine(input->id);

    if (value) {
        broadcastAutomationDeletedEvent(input->id, input->requestId);
        ret = IPC_SUCCESS;
    } else {
        ret = IPC_GENERAL_ERROR;
    }

    return ret;
}

IPCCode handle_SET_AUTOMATION_request(AutomationRequest *input)
{
    bool value;
    IPCCode ret;

    value = automationServiceSetMachineEnabled(input->id, input->enabled);

    if (value) {
        char* specification = input->spec;

        if (specification && (strlen(specification) > 0)) {
            const cslt_transcoder_t* transcoder;
            char* transcodedSpec = NULL;
            int cslt_ret;

            transcoder = automationServiceGetTranscoder(specification);
            cslt_ret = cslt_transcode(transcoder, specification, &transcodedSpec);
            if (cslt_ret < 0) {
                icLogError(LOG_TAG, "Unable to transcode specification. [%s]", strerror(errno));
            } else {
                char *originalSpecification = NULL;
                if (transcodedSpec != specification) {
                    originalSpecification = specification;
                }
                automationServiceSetMachineSpecification(input->id, transcodedSpec, originalSpecification,
                                                         transcoder->transcoder_version);

                if (transcodedSpec != specification) {
                    free(transcodedSpec);
                }
            }
        }

        broadcastAutomationModifiedEvent(input->id, input->requestId, input->enabled);

        ret = IPC_SUCCESS;
    } else {
        ret = IPC_GENERAL_ERROR;
    }

    return ret;
}

/**
 * Get details on the available automations
 *   output - response from service
 **/
IPCCode handle_GET_AUTOMATIONS_request(AutomationDetailsList *output)
{
    icLinkedList* infos = automationServiceGetMachineInfos();

    icLinkedListIterator* it = linkedListIteratorCreate(infos);
    while(linkedListIteratorHasNext(it))
    {
        MachineInfo* info = (MachineInfo*)linkedListIteratorGetNext(it);

        AutomationDetails* details = create_AutomationDetails();

        details->id = info->id; //move the memory instead of copying
        info->id = NULL;

        details->enabled = info->enabled;
        details->dateCreatedSecs = info->dateCreatedSecs;
        details->messagesConsumed = info->messagesConsumed;
        details->messagesEmitted = info->messagesEmitted;

        linkedListAppend(output->automations, details);
    }

    linkedListDestroy(infos, machineInfoFreeFunc);
    return IPC_SUCCESS;
}

/**
 * Enable or disable an automation
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_SET_AUTOMATION_ENABLED_request(SetAutomationEnabledRequest *input)
{
    bool value;
    IPCCode ret;

    value = automationServiceSetMachineEnabled(input->id, input->enabled);

    if (value) {
        broadcastAutomationModifiedEvent(input->id, 0, input->enabled);
        ret = IPC_SUCCESS;
    } else {
        ret = IPC_GENERAL_ERROR;
    }

    return ret;
}

// As a short term measure, we are just validating the rule ID
// This may later be changed to do a real validation specific to each time the rule fires
//
IPCCode handle_IS_VALID_TOKEN_request(char *input, bool *output)
{
    *output = false;

    if (input == NULL)
    {
        icLogError(LOG_TAG, "%s: input is NULL", __FUNCTION__);
        return IPC_INVALID_ERROR;
    }

    icLinkedList *machines = automationServiceGetMachineInfos();
    icLinkedListIterator *machineIterator = linkedListIteratorCreate(machines);

    while(linkedListIteratorHasNext(machineIterator))
    {
        MachineInfo *machine = (MachineInfo*)linkedListIteratorGetNext(machineIterator);
        if(strcasecmp(machine->id,input) == 0)
        {
            *output= true;
            break;
        }
    }

    linkedListIteratorDestroy(machineIterator);
    linkedListDestroy(machines,machineInfoFreeFunc);

    return IPC_SUCCESS;
}
