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
 * troubleLister.c
 *
 * Command line utility to list the troubles known
 * to securityService.
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include <securityService/securityService_ipc.h>
#include <securityService/securityService_event.h>
#include <securityService/troubleEventHelper.h>
#include <securityService/deviceTroubleEventHelper.h>

int main(int argc, char *argv[]) 
{
    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

	// get all of the system troubles
	//
    getTroublesInput *input = create_getTroublesInput();
    input->includeAck = true;
    input->sortAlgo = TROUBLE_SORT_BY_CREATE_DATE;
	troubleObjList *troubles = create_troubleObjList();
    IPCCode rc = securityService_request_GET_TROUBLE_LIST(input, troubles);
    destroy_getTroublesInput(input);
    if (rc != IPC_SUCCESS)
    {
        // error with IPC request
        //
        printf("unable to obtatin troubles\n\n");
        destroy_troubleObjList(troubles);
        closeIcLogger();
        return EXIT_FAILURE;
    }

    // success, so print each out
    //
    uint16_t count = linkedListCount(troubles->troubles);
    if (count == 0)
    {
        printf("No troubles found.\n\n");
        destroy_troubleObjList(troubles);
        closeIcLogger();
        return EXIT_SUCCESS;
    }

    // loop through the troubles
    //
    count = 0;
    icLinkedListIterator *loop = linkedListIteratorCreate(troubles->troubles);
    while (linkedListIteratorHasNext(loop) == true)
    {
        troubleObj *next = (troubleObj *)linkedListIteratorGetNext(loop);
        count++;

        char *eventTime = unixTimeMillisToISO8601(next->eventTime);

        char *payload = NULL;
        if (next->extra != NULL)
        {
            payload = cJSON_Print(next->extra);
        }

        printf("Trouble #%-2d: eventId=%" PRIu64 ", troubleId=%" PRIu64", payload=%s\n   type=%s, reason=%s, time=%s,\n   critical=%s, ack=%s\n\n",
               count,
               next->eventId,
               next->troubleId,
               (payload != NULL) ? payload : "N/A",
               troubleTypeLabels[next->type],
               troubleReasonLabels[next->reason],
               eventTime,
               troubleCriticalityTypeLabels[next->critical],
               (next->acknowledged == true) ? "YES" : "no ");

        // Cleanup
        free(payload);
    }

    // cleanup & exit
    //
    linkedListIteratorDestroy(loop);
    destroy_troubleObjList(troubles);

    closeIcLogger();
	return EXIT_SUCCESS;
}
