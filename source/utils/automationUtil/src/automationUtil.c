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
 * automationTool.c
 *
 * Command line utility to list, and enable/disable
 * automations within the service
 *
 * Author: jelderton and tlea
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <icLog/logging.h>
#include <automationService/automationService_ipc.h>
#include <automationService/automationService_event.h>
#include <icUtil/fileUtils.h>
#include <icUtil/stringUtils.h>
#include <icIpc/eventProducer.h>
#include <icTime/timeUtils.h>

#define TICK_EVENT_ID 499

/*
 * private function declarations
 */
void printUsage();

/*
 * internal data types
 */
typedef enum
{
    NO_ACTION,
    LIST_ACTION,        // list out rule meta-data
    ENABLE_ACTION,      // enable a rule
    DISABLE_ACTION,     // disable a rule
    CREATE_ACTION,      // create a new rule
    UPDATE_ACTION,      // update an existing rule
    DELETE_ACTION,      // delete an existing rule
    TIMER_TICK_ACTION,  // simulate a timer tick event
} actionEnum;

/*
 * main entry point
 */
int main(int argc, char *argv[])
{
    int c;
    char* id = NULL;
    uint64_t tickTimeMillis = 0;
    actionEnum mode = NO_ACTION;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    // parse CLI args
    //
    while ((c = getopt(argc,argv,"le:d:c:u:x:t:h")) != -1)
    {
        switch (c)
        {
            case 'l':       // list automations and their meta-data information
            {
                mode = LIST_ACTION;
                break;
            }
            case 'e':       // enable an automation
            {
                id = strdup(optarg);
                mode = ENABLE_ACTION;
                break;
            }
            case 'd':       // disable an automation
            {
                id = strdup(optarg);
                mode = DISABLE_ACTION;
                break;
            }

            case 'c':       // create automation
            {
                id = strdup(optarg);
                mode = CREATE_ACTION;
                break;
            }

            case 'u':
            {
                id = strdup(optarg);
                mode = UPDATE_ACTION;
                break;
            }

            case 'x':       // delete automation
            {
                id = strdup(optarg);
                mode = DELETE_ACTION;
                break;
            }

            case 't':
            {
                if (stringToUint64(optarg, &tickTimeMillis) == true)
                {
                    mode = TIMER_TICK_ACTION;
                    break;
                }
                else
                {
                    fprintf(stderr, "Invalid tick time specified\n");
                    closeIcLogger();
                    return EXIT_FAILURE;
                }
            }

            case 'h':       // help option
            {
                printUsage();
                closeIcLogger();
                return EXIT_SUCCESS;
            }

            default:
            {
                fprintf(stderr,"Unknown option '%c'\n",c);
                closeIcLogger();
                return EXIT_FAILURE;
            }
        }
    }

    // look to see that we have a mode set
    //
    if (mode == NO_ACTION)
    {
        fprintf(stderr, "No action defined.  Use -h option for usage\n");
        closeIcLogger();
        return EXIT_FAILURE;
    }
    else if (mode == LIST_ACTION)
    {
        // create the automation details for the IPC to use as output
        //
        AutomationDetailsList* detailsList = create_AutomationDetailsList();
        IPCCode rc = automationService_request_GET_AUTOMATIONS(detailsList);
        if (rc == IPC_SUCCESS && linkedListCount(detailsList->automations) > 0)
        {
            // call worked, print the automation details for each
            //
            icLinkedListIterator *loop = linkedListIteratorCreate(detailsList->automations);
            while (linkedListIteratorHasNext(loop) == true)
            {
                AutomationDetails *next = (AutomationDetails *)linkedListIteratorGetNext(loop);
                printf("Automation %s:\n", next->id);
                time_t dateCreatedSecs = (time_t)next->dateCreatedSecs;
                char buf[50]; //manpages say 26 chars.
                printf("  dateCreated          = %s", ctime_r(&dateCreatedSecs, buf));
                printf("  messagesConsumed     = %" PRId64 "\n", next->messagesConsumed);
                printf("  messagesEmitted      = %" PRId64 "\n", next->messagesEmitted);
                printf("  enabled = %s\n", (next->enabled) == true ? "yes" : "no");
            }
            linkedListIteratorDestroy(loop);
        }

        // cleanup
        //
        destroy_AutomationDetailsList(detailsList);
    }
    else if ((mode == ENABLE_ACTION || mode == DISABLE_ACTION) && id != NULL)
    {
        // enable automation
        //
        SetAutomationEnabledRequest* msg = create_SetAutomationEnabledRequest();
        msg->id = id;
        msg->enabled = mode == ENABLE_ACTION;

        // forward to automationService
        //
        IPCCode rc = automationService_request_SET_AUTOMATION_ENABLED(msg);
        if (rc == IPC_SUCCESS)
        {
            // rule successfully updated
            //
            printf("successfully %s automation %s\n", (mode == ENABLE_ACTION) ? "enabled" : "disabled", id);
        }
        else
        {
            fprintf(stderr, "error while %s automation : %d - %s\n", (mode == ENABLE_ACTION) ? "enabling" : "disabling",
                    rc, IPCCodeLabels[rc]);
            closeIcLogger();
            destroy_SetAutomationEnabledRequest(msg);
            free(id);
            return EXIT_FAILURE;
        }
        destroy_SetAutomationEnabledRequest(msg);
        id = NULL; //freed by above line
    }
    else if (mode == CREATE_ACTION || mode == UPDATE_ACTION)
    {
        const char *modeStr = mode == CREATE_ACTION ? "creating" : "updating";

        // should be an extra arg for the 'filename'
        //
        if (argc <= optind)
        {
            // missing filename parameter
            //
            fprintf(stderr, "error while %s automation, missing 'filename' argument\n", modeStr);
            free(id);
            return EXIT_FAILURE;
        }

        // read the file
        //
        char *contents = readFileContents(argv[optind]);
        if (contents == NULL)
        {
            fprintf(stderr, "error while %s automation, problems reading '%s'\n", modeStr, argv[optind]);
            free(id);
            return EXIT_FAILURE;
        }

        AutomationRequest* msg = create_AutomationRequest();
        msg->id = id;
        msg->enabled = true;
        msg->spec = contents;     // gets released during destroy_CreateAutomationRequest()

        // forward to automationSerice
        //
        IPCCode rc;
        if (mode == CREATE_ACTION)
        {
            rc = automationService_request_CREATE_AUTOMATION(msg);
        }
        else
        {
            rc = automationService_request_SET_AUTOMATION(msg);
        }

        if (rc == IPC_SUCCESS)
        {
            // rule successfully created
            //
            printf("%s automation %s was successful\n", modeStr, id);
        }
        else
        {
            fprintf(stderr, "error while %s automation : %d - %s\n", modeStr, rc, IPCCodeLabels[rc]);
            closeIcLogger();
            destroy_AutomationRequest(msg);
            return EXIT_FAILURE;
        }
        destroy_AutomationRequest(msg);
        id = NULL; //freed by above line
    }
    else if (mode == DELETE_ACTION && id > 0)
    {
        DeleteAutomationRequest* msg = create_DeleteAutomationRequest();
        msg->id = id;

        // delete automation
        //
        IPCCode rc = automationService_request_DELETE_AUTOMATION(msg);
        if (rc == IPC_SUCCESS)
        {
            // automation successfully deleted
            //
            printf("successfully deleted automation %s\n", id);
        }
        else
        {
            fprintf(stderr, "error while deleting automation : %d - %s\n", rc, IPCCodeLabels[rc]);
            closeIcLogger();
            destroy_DeleteAutomationRequest(msg);
            return EXIT_FAILURE;
        }
        destroy_DeleteAutomationRequest(msg);
        id = NULL; //free by above line
    }
    else if (mode == TIMER_TICK_ACTION && tickTimeMillis > 0)
    {
        EventProducer producer = initEventProducer(AUTOMATIONSERVICE_EVENT_PORT_NUM);

        automationEvent *event = create_automationEvent();

        // first set normal 'baseEvent' crud
        //
        event->baseEvent.eventCode = TICK_EVENT_ID;
        convertUnixTimeMillisToTimespec(tickTimeMillis, &event->baseEvent.eventTime);

        // convert to JSON objet
        //
        cJSON *jsonNode = encode_automationEvent_toJSON(event);

        // Clear out the eventId, so that it properly does onDemand events for any actions
        cJSON_DeleteItemFromObject(jsonNode, EVENT_ID_JSON_KEY);

        // broadcast the encoded event
        //
        broadcastEvent(producer, jsonNode);

        // cleanup
        destroy_automationEvent(event);
        cJSON_Delete(jsonNode);
        shutdownEventProducer(producer);
    }

    closeIcLogger();
    free(id);
    return EXIT_SUCCESS;
}

/*
 * show user available options
 */
void printUsage()
{
    fprintf(stderr, "Comcast Automation Utility\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  automationTool [-l] [-e id] [-d id] [-c id filename] [-u id filename] [-x id] [-t timestamp millis]\n");
    fprintf(stderr, "    -l : list automations\n");
    fprintf(stderr, "    -c - create new automation from file\n");
    fprintf(stderr, "    -u - update existing automation from file\n");
    fprintf(stderr, "    -x - delete 'id'\n");
    fprintf(stderr, "    -e - enable 'id'\n");
    fprintf(stderr, "    -d - disable 'id'\n");
    fprintf(stderr, "    -t - simulate timer tick event");
    fprintf(stderr, "\n");
}
