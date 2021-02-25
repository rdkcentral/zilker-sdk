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
#include <cslt/cslt.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <cjson/cJSON.h>
#include <stdlib.h>
#include <uuid/uuid.h>
#include <commonDeviceDefs.h>

#include "../cslt_internal.h"
#include "sheens_javascript.h"
#include "sheens_transcoders.h"
#include "sheens_json.h"
#include "sheens_request.h"
#include "js/schedulerActions.js.h"

static const char* actions_js = SCHEDULERACTIONS_JS_BLOB;

/** Create all the actions that will be emitted if a schedule
 * is set to fire. This action will be specific to the resource,
 * and will return a fresh array each time.
 *
 * @param iterator The list of thermostat IDs. Each ID will generate
 * a new emit message.
 * @param resource The resource that will be manipulated.
 * @param temperature The temperature to set the thermostat to.
 * @return An array of requests that may be emitted out to the system.
 */
static cJSON* create_actions(icLinkedListIterator* iterator, const char* resource, int temperature)
{
    cJSON* actions_json = cJSON_CreateArray();

    while (linkedListIteratorHasNext(iterator)) {
        const char* id = linkedListIteratorGetNext(iterator);
        cJSON* json;
        char buffer[128];

        /* All action values *must* be strings */
        sprintf(buffer, "%d", temperature);

        json = sheens_create_write_device_request(id, resource, THERMOSTAT_PROFILE_RESOURCE_HOLD_ON, buffer);
        if (json == NULL) {
            linkedListIteratorDestroy(iterator);

            return NULL;
        }

        cJSON_AddItemToArray(actions_json, json);
    }

    linkedListIteratorDestroy(iterator);


    return actions_json;
}

/** Create all of the JavaScript source to be used by an Action.
 *
 * @param iterator The list of schedules that are to be used by the
 * JavaScript logic.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @param start_branches The 'start' branches array for message pattern
 * matching.
 * @return A JSON string that may be used as the action 'source', or NULL
 * if there was an error.
 */
cJSON* sheens_schedules2javascript(icLinkedListIterator* iterator,
                                   cJSON* nodes_object,
                                   cJSON* start_branches)
{
    cJSON* json = NULL;
    cJSON* cool_list = cJSON_CreateArray();
    cJSON* heat_list = cJSON_CreateArray();

    char* js, * cool_js, * heat_js;

    while (linkedListIteratorHasNext(iterator)) {
        icrule_thermostat_schedule_t* schedule = linkedListIteratorGetNext(iterator);

        cJSON* schedule_json = sheens_create_time_object(&schedule->time);

        /* Each set of actions are unique to cool and heat. Thus
         * if the mode is BOTH then we must generate two schedules
         * each with their own set of actions. This is because the
         * 'resource' is specific to heat and cold.
         */
        switch (schedule->mode) {
            case TSTAT_MODE_INVALID:
                linkedListIteratorDestroy(iterator);

                cJSON_Delete(cool_list);
                cJSON_Delete(heat_list);
                cJSON_Delete(schedule_json);

                return NULL;
            case TSTAT_MODE_HEAT: {
                cJSON* action_array = create_actions(linkedListIteratorCreate(schedule->ids),
                                                     THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT,
                                                     schedule->temperature);

                if (action_array == NULL) {
                    linkedListIteratorDestroy(iterator);

                    cJSON_Delete(cool_list);
                    cJSON_Delete(heat_list);
                    cJSON_Delete(schedule_json);

                    return NULL;
                }

                cJSON_AddItemToObjectCS(schedule_json, sheens_actions_key, action_array);
                cJSON_AddItemToArray(heat_list, schedule_json);
                break;
            }
            case TSTAT_MODE_COOL: {
                cJSON* action_array = create_actions(linkedListIteratorCreate(schedule->ids),
                                                     THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT,
                                                     schedule->temperature);

                if (action_array == NULL) {
                    linkedListIteratorDestroy(iterator);

                    cJSON_Delete(cool_list);
                    cJSON_Delete(heat_list);
                    cJSON_Delete(schedule_json);

                    return NULL;
                }

                cJSON_AddItemToObjectCS(schedule_json, sheens_actions_key, action_array);
                cJSON_AddItemToArray(cool_list, schedule_json);
                break;
            }
            case TSTAT_MODE_BOTH: {
                // Start off by duplicating the schedule without any actions.
                cJSON* schedule_duplicate = cJSON_Duplicate(schedule_json, true);

                // Now create actions for Heat.
                cJSON* action_array = create_actions(linkedListIteratorCreate(schedule->ids),
                                                     THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT,
                                                     schedule->temperature);

                if (action_array == NULL) {
                    linkedListIteratorDestroy(iterator);

                    cJSON_Delete(cool_list);
                    cJSON_Delete(heat_list);
                    cJSON_Delete(schedule_json);

                    return NULL;
                }

                cJSON_AddItemToObjectCS(schedule_json, sheens_actions_key, action_array);
                cJSON_AddItemToArray(heat_list, schedule_json);

                // Now create actions for Cool.
                action_array = create_actions(linkedListIteratorCreate(schedule->ids),
                                              THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT,
                                              schedule->temperature);

                if (action_array == NULL) {
                    linkedListIteratorDestroy(iterator);

                    cJSON_Delete(cool_list);
                    cJSON_Delete(heat_list);
                    cJSON_Delete(schedule_json);

                    return NULL;
                }

                cJSON_AddItemToObjectCS(schedule_duplicate, sheens_actions_key, action_array);
                cJSON_AddItemToArray(cool_list, schedule_duplicate);
                break;
            }
        }
    }

    // This is the generated initializers for the Cool action list.
    cool_js = cJSON_PrintBuffered(cool_list, 4096, false);
    cJSON_Delete(cool_list);

    if (cool_js == NULL) {
        linkedListIteratorDestroy(iterator);
        cJSON_Delete(heat_list);

        errno = ENOMEM;
        return NULL;
    }

    // This is the generated initializers for the Heat action list.
    heat_js = cJSON_PrintBuffered(heat_list, 4096, false);
    cJSON_Delete(heat_list);

    if (heat_js == NULL) {
        free(cool_js);
        linkedListIteratorDestroy(iterator);

        errno = ENOMEM;
        return NULL;
    }

    /* Now determine how large the final buffer will have to be in
     * order to fit the final set of JavaScript. We add in one byte
     * for the NULL terminator.
     */
    js = malloc(strlen(cool_js) + strlen(heat_js) + strlen(actions_js) + 1);
    if (js == NULL) {
        free(cool_js);
        free(heat_js);

        linkedListIteratorDestroy(iterator);

        errno = ENOMEM;
        return NULL;
    }

    sprintf(js, actions_js, cool_js, heat_js);

    json = cJSON_CreateString(js);

    free(js);
    free(cool_js);
    free(heat_js);

    linkedListIteratorDestroy(iterator);

    return json;
}
