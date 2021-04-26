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
// Created by Boyd, Weston on 5/8/18.
//

#include <icrule/icrule.h>
#include <errno.h>
#include <string.h>

#include "icrule_schedule.h"
#include "icrule_xml.h"
#include "icrule_internal.h"

#define ELEMENT_SCHEDULE_TIMESLOT "timeSlot"
#define ELEMENT_SCHEDULE_MODE "mode"
#define ELEMENT_SCHEDULE_TEMPERATURE "temperature"

#define ATTRIBUTE_SCHEDULE_THERMOSTATID "thermostatID"

static const char* schedule_enum2str[] = {
    "heat",
    "cool",
    "heatAndCool"
};

static icrule_thermostat_schedule_t* contians_schedule(icrule_thermostat_schedule_t* schedule, icLinkedListIterator* iterator)
{
    while(linkedListIteratorHasNext(iterator)) {
        icrule_thermostat_schedule_t* item = linkedListIteratorGetNext(iterator);

        if ((schedule->mode == item->mode) &&
            (schedule->temperature == item->temperature) &&
            (schedule->time.use_exact_time == item->time.use_exact_time) &&
            (schedule->time.time.seconds == item->time.time.seconds)) {
            if (linkedListCount(item->ids) == 0) {
                linkedListIteratorDestroy(iterator);
                return item;
            } else {
                bool rejected = false;
                icLinkedListIterator* __iterator = linkedListIteratorCreate(schedule->ids);

                /* Each tstat ID that is in this schedule *must* also be in
                 * the item ID list. Thus if the item contains each
                 * and every ID that is in the schedule then we are equal.
                 * It doesn't matter if the item contains *more* IDs than
                 * the schedule. Either way the rule will fire and tell the
                 * tstat to update.
                 */
                while (!rejected && linkedListIteratorHasNext(__iterator)) {
                    icLinkedListIterator* __iter = linkedListIteratorCreate(item->ids);
                    const char* id = linkedListIteratorGetNext(__iterator);
                    bool found = false;

                    while (!found && linkedListIteratorHasNext(__iter)) {
                        if (strcmp(id, linkedListIteratorGetNext(__iter)) == 0) {
                            found = true;
                        }
                    }

                    linkedListIteratorDestroy(__iter);

                    if (!found) {
                        rejected = true;
                    }
                }

                linkedListIteratorDestroy(__iterator);

                if (!rejected) {
                    linkedListIteratorDestroy(iterator);
                    return item;
                }
            }
        }
    }

    linkedListIteratorDestroy(iterator);

    return NULL;
}

void icrule_free_schedule(void* alloc)
{
    if (alloc) {
        icrule_thermostat_schedule_t* schedule = alloc;

        linkedListDestroy(schedule->ids, NULL);

        free(alloc);
    }
}
int icrule_parse_schedule(xmlNodePtr parent, icLinkedList* schedule_list)
{
    icrule_thermostat_schedule_t* schedule;
    xmlNodePtr node;
    char* thermostat_ids;

    schedule = malloc(sizeof(icrule_thermostat_schedule_t));
    if (schedule == NULL) {
        errno = ENOMEM;
        return -1;
    }

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, ELEMENT_SCHEDULE_TIMESLOT) == 0) {
                if (icrule_parse_time_slot(node, &schedule->time) < 0) {
                    free(schedule);

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, ELEMENT_SCHEDULE_MODE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    int i;

                    for (i = 0; i < 3; i++) {
                        if (strcmp((const char*) value, schedule_enum2str[i]) == 0) {
                            schedule->mode = (icrule_thermostat_mode_t) i;
                            break;
                        }
                    }

                    xmlFree(value);
                }
            } else if (strcmp((const char*) node->name, ELEMENT_SCHEDULE_TEMPERATURE) == 0) {
                schedule->temperature = icrule_get_xml_int(node, NULL, 75);
            }
        }
    }

    schedule->ids = linkedListCreate();
    thermostat_ids = icrule_get_xml_string(parent, ATTRIBUTE_SCHEDULE_THERMOSTATID, NULL);
    if (thermostat_ids) {
        char *token, *saveptr;

        token = strtok_r(thermostat_ids, ",", &saveptr);
        while (token != NULL) {
            linkedListAppend(schedule->ids, strdup(token));

            token = strtok_r(NULL, ",", &saveptr);
        }

        free(thermostat_ids);
    }

    /* If the schedule is equal in every way, except day of week, then
     * there is no point in having this new schedule as the day should
     * just be added in with the other existing schedule days. This
     * solves are silly issue with the way the UI is generating the
     * rules with one new schedule entry for each day instead of just
     * making them as a list.
     */
    {
        icrule_thermostat_schedule_t* old_schedule = contians_schedule(schedule,
                                                                       linkedListIteratorCreate(schedule_list));

        if (old_schedule) {
            old_schedule->time.day_of_week |= schedule->time.day_of_week;
            icrule_free_schedule(schedule);
        } else {
            linkedListAppend(schedule_list, schedule);
        }
    }

    return 0;
}
