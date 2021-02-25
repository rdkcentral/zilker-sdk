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
// Created by mkoch201 on 2/21/19.
//

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <icLog/logging.h>
#include <icIpc/eventProducer.h>
#include <xhCron/cron_event.h>
#include <xhCron/cronEventRegistrar.h>

/*
 * private function declarations
 */
static void printUsage();

static bool eventHandler(const char *name)
{
    printf("**** Got cron event %s\n", name);
    return false;
}

static void broadcastCronEvent(const char *name)
{
    EventProducer producer = initEventProducer(CRON_EVENT_PORT_NUM);

    cronEvent *event = create_cronEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = CRON_EVENT;
    setEventId(&(event->baseEvent));
    setEventTimeToNow(&(event->baseEvent));
    event->name = strdup(name);
    name = NULL;

    // convert to JSON objet
    //
    cJSON *jsonNode = encode_cronEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);

    // cleanup
    destroy_cronEvent(event);
    cJSON_Delete(jsonNode);
    shutdownEventProducer(producer);
}


/*
 * main entry point
 */
int main(int argc, char *argv[])
{
    int retVal = EXIT_FAILURE;
    char *name = NULL;
    char *schedule = NULL;
    int c = 0;
    bool doBroadcastEvent = true;
    bool doRegister = false;
    bool doUnregister = false;
    bool doWait = false;
    long waitTimeSec = 0;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    while ((c = getopt(argc, argv, "n:s:bruw:h")) != -1)
    {
        switch(c)
        {
            case 'n':       // name
            {
                name = strdup(optarg);
                break;
            }

            case 's':
            {
                schedule = strdup(optarg);
                break;
            }

            case 'b':
            {
                doBroadcastEvent = true;
                break;
            }

            case 'r':
            {
                doRegister = true;
                break;
            }

            case 'u':
            {
                doUnregister = true;
                break;
            }

            case 'w':
            {
                doWait = true;
                waitTimeSec = strtol(optarg, NULL, 10);
                break;
            }

            case 'h':       // help option
                printUsage();
                closeIcLogger();
                return EXIT_SUCCESS;

            default:
                fprintf(stderr,"Unexpected option '%c' given\n  Use -h option for usage\n",c);
                closeIcLogger();
                return EXIT_FAILURE;
                break;
        }

    }

    if (name == NULL)
    {
        fprintf(stderr,"Must specify name.  Use -h option for usage\n\n");
        retVal = EXIT_FAILURE;
        goto cleanup;
    }

    if (doRegister)
    {
        if (schedule == NULL)
        {
            fprintf(stderr,"Must specify schedule.  Use -h option for usage\n\n");
            retVal = EXIT_FAILURE;
            goto cleanup;
        }

        if (registerForCronEvent(name, schedule, eventHandler) == false)
        {
            fprintf(stderr,"Failed to register for cron event\n\n");
            retVal =  EXIT_FAILURE;
            goto cleanup;
        }

        printf("Registered for cron event %s", name);
    }
    if (doBroadcastEvent)
    {
        printf("Broadcasting cron event %s\n", name);
        broadcastCronEvent(name);
    }
    if (doWait)
    {
        if (registerForCronEvent(name, NULL, eventHandler))
        {
            if (waitTimeSec > 0)
            {
                printf("Sleeping for %ld seconds...\n", waitTimeSec);
                sleep(waitTimeSec);
            }
        }
        else
        {
            fprintf(stderr, "Did not find existing entry %s to wait on\n\n", name);
        }
    }
    if (doUnregister)
    {
        unregisterForCronEvent(name, true);
        printf("Unregistered cron event %s", name);
    }
cleanup:
    free(name);
    free(schedule);

    closeIcLogger();
    return retVal;
}

/*
 * show user available options
 */
static void printUsage()
{
    fprintf(stderr, "Cron Event Utility\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  xhCronEventUtil -n name\n");
    fprintf(stderr, "    -n - name of entry\n");
    fprintf(stderr, "    -s - schedule of entry\n");
    fprintf(stderr, "    -b : broadcast event mode, will cause an cron event with the given name to be sent\n");
    fprintf(stderr, "    -r : register mode, will register to get events with the given name and schedule\n");
    fprintf(stderr, "    -u : unregister mode, will unregister and remove cron tab entry for given name\n");
    fprintf(stderr, "    -w - wait time in seconds, will wait for events with the given name\n");
    fprintf(stderr, "\n");
}