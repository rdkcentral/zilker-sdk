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
// Created by wboyd747 on 7/2/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cjson/cJSON.h>
#include <icrule/icrule.h>

#include "sheens_json.h"
#include "sheens_javascript.h"
#include "js/timeFunctions.js.h"

#define START_TIME_NODE "start_time"
#define END_TIME_NODE "end_time"
#define TRIGGER_WINDOW_NODE "trigger_window"
#define RESET_FOR_TRIGGER_WINDOW_NODE "reset_for_trigger_window"

/* Required variables in order:
 *
 * <Time Object>
 */
static const char* time_check_js = TIMEFUNCTIONS_JS_BLOB
                                   "\n"
                                   "_.bindings['" sheens_allowed_key "'] = isTimeMatch("
                                   "new WeekTime(_.bindings['" sheens_event_time_bound_key "']), "
                                   "%s, _.bindings['" sheens_sunrise_bound_key "'], "
                                   "_.bindings['" sheens_sunset_bound_key "']);\n"
                                   "return _.bindings;\n";

/**
 * Build the start/end time verification nodes.
 * The start time node will be stimulated by the global timer
 * tick. Each tick it will check to see if we have a time match.
 * (based on day of the week and hour:min) If we do then the state
 * will transition to the success node, otherwise the state will
 * transition to the failure node.
 *
 * @param constraint The legacy iControl rule time based
 * start/end constraint.
 * @param node_name The name of this time node.
 * @param on_success_node The node to transition to on success.
 * @param on_failure_node The node to transition to on failure.
 * @param nodes_object The top-level JSON object that holds all nodes.
 * @return True on successfully building the node.
 */
static bool build_time_node(const icrule_time_t* constraint,
                            const char* node_name,
                            const char* on_success_node,
                            const char* on_failure_node,
                            cJSON* nodes_object)
{
    cJSON* json;
    cJSON* branch_array;

    char* js, * time_json;

    /* First generate the "time" object that will be used to
     * compare the "now" time.
     */
    json = sheens_create_time_object(constraint);
    if (json == NULL) {
        errno = EINVAL;
        return false;
    }

    time_json = cJSON_PrintBuffered(json, 256, false);
    cJSON_Delete(json);

    if (time_json == NULL) {
        errno = ENOMEM;
        return false;
    }

    /* Now create the Javascript that will be used to set the allowed
     * flag.
     */
    js = malloc(strlen(time_check_js) +
                strlen(time_json) +
                1);
    if (js == NULL) {
        free(time_json);

        errno = ENOMEM;
        return false;
    }

    sprintf(js, time_check_js, time_json);

    /* Build the new Start Time node */
    branch_array = cJSON_CreateArray();

    json = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(json, sheens_allowed_key, cJSON_CreateBool(true));

    cJSON_AddItemToArray(branch_array,
                         sheens_create_branch(json,
                                              on_success_node,
                                              true));
    cJSON_AddItemToArray(branch_array,
                         sheens_create_branch(NULL,
                                              on_failure_node,
                                              true));

    /* Create the state node JSON and add it to the
     * top-level nodes object.
     */
    cJSON_AddItemToObjectCS(nodes_object,
                            node_name,
                            sheens_create_state_node(cJSON_CreateString(js),
                                                     branch_array,
                                                     false));

    free(time_json);
    free(js);

    return true;
}

/**
 * Build the nodes for each trigger in the trigger list.
 * The Trigger Window node will be generated (as a message type)
 * and all patterns for trigger matching will be placed in it.
 *
 * @param triggers The list of iControl triggers.
 * @param nodes_object The top-level JSON object that holds all nodes.
 * @return True on successfully building the node.
 */
static bool build_triggers(const icrule_trigger_list_t* triggers,
                           cJSON* nodes_object)
{
    cJSON *json, *branch_array;
    icLinkedListIterator* iterator;

    branch_array = cJSON_CreateArray();

    /* Walk through each trigger and generate its patterns
     * and nodes. Add the patterns into the trigger windows
     * branch array.
     */
    iterator = linkedListIteratorCreate(triggers->triggers);
    while (linkedListIteratorHasNext(iterator)) {
        icrule_trigger_t* trigger = linkedListIteratorGetNext(iterator);

        json = sheens_trigger2javascript(trigger,
                                         sheens_reset_value,
                                         TRIGGER_WINDOW_NODE,
                                         nodes_object,
                                         branch_array);
        if (json == NULL) {
            cJSON_Delete(branch_array);
            linkedListIteratorDestroy(iterator);

            return false;
        }

        cJSON_AddItemToArray(branch_array, json);
    }
    linkedListIteratorDestroy(iterator);

    // Create schedule timer tick pattern for end.
    json = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(json,
                            sheens_event_code_key,
                            cJSON_CreateNumber(TIMER_TICK_EVENT_CODE));

    sheens_pattern_add_constraints_required(json);

    cJSON_AddItemToArray(branch_array,
                         sheens_create_branch(json,
                                              END_TIME_NODE,
                                              true));

    /* Add the trigger window to the top-level
     * node state machine.
     */
    cJSON_AddItemToObjectCS(nodes_object,
                            TRIGGER_WINDOW_NODE,
                            sheens_create_state_node(NULL,
                                                     branch_array,
                                                     true));

    return true;
}

cJSON* sheens_negative2javascript(const icrule_trigger_list_t* triggers,
                                  const icrule_constraint_time_t* constraint,
                                  cJSON* nodes_object)
{
    cJSON* json;

    if (!build_time_node(&constraint->start,
                         START_TIME_NODE,
                         RESET_FOR_TRIGGER_WINDOW_NODE,
                         sheens_reset_value,
                         nodes_object)) {
        errno = EINVAL;
        return NULL;
    }

    if (!build_time_node(&constraint->end,
                         END_TIME_NODE,
                         sheens_constraints_key,
                         RESET_FOR_TRIGGER_WINDOW_NODE,
                         nodes_object)) {
        errno = EINVAL;
        return NULL;
    }

    if (!build_triggers(triggers, nodes_object)) {
        errno = EINVAL;
        return NULL;
    }

    cJSON_AddItemToObjectCS(nodes_object,
                            RESET_FOR_TRIGGER_WINDOW_NODE,
                            sheens_create_reset_node(TRIGGER_WINDOW_NODE));

    // Create schedule timer tick pattern for start.
    json = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(json,
                            sheens_event_code_key,
                            cJSON_CreateNumber(TIMER_TICK_EVENT_CODE));

    sheens_pattern_add_constraints_required(json);

    return sheens_create_branch(json, START_TIME_NODE, true);
}

