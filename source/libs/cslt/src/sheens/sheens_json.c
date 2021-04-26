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
// Created by Boyd, Weston on 5/7/18.
//

#include <string.h>
#include <errno.h>

#include "sheens_json.h"

cJSON* sheens_create_branch(cJSON* pattern, const char* target, bool is_reference)
{
    cJSON *json[2];

    if ((target == NULL) || (strlen(target) == 0)) {
        errno = EINVAL;
        return NULL;
    }

    json[0] = cJSON_CreateObject();
    json[1] = (is_reference) ? cJSON_CreateStringReference(target) : cJSON_CreateString(target);

    if (pattern) {
        cJSON_AddItemToObjectCS(json[0], sheens_pattern_key, pattern);
    }

    cJSON_AddItemToObjectCS(json[0], sheens_target_key, json[1]);

    return json[0];
}

cJSON* sheens_create_state_node(cJSON* source, cJSON* branch_array, bool is_message)
{
    cJSON *root, *branches;

    root = cJSON_CreateObject();

    if (source) {
        cJSON* action = cJSON_CreateObject();

        cJSON_AddItemToObjectCS(action,
                                sheens_interpreter_key,
                                cJSON_CreateStringReference(sheens_interpreter_value));
        cJSON_AddItemToObjectCS(action, sheens_source_key, source);
        cJSON_AddItemToObjectCS(root, sheens_action_key, action);
    }

    if (branch_array == NULL) {
        branch_array = cJSON_CreateArray();

        cJSON_AddItemToArray(branch_array,
                             sheens_create_branch(NULL, sheens_reset_value, true));
    }

    branches = cJSON_CreateObject();

    if (is_message && (source == NULL)) {
        cJSON_AddItemToObjectCS(branches,
                                sheens_type_key,
                                cJSON_CreateStringReference(sheens_message_value));
    }

    cJSON_AddItemToObjectCS(branches, sheens_branches_key, branch_array);
    cJSON_AddItemToObjectCS(root, sheens_branching_key, branches);

    return root;
}

cJSON* sheens_create_reset_node(const char* branch_node)
{
    /* If the 'persist' binding is present then
 * preserve them in bindings. Clear out everything
 * else.
 */
    static const char* persist = "return ('persist' in _.bindings) ? "
                                 "{'persist': _.bindings['persist']} : {};\n";

    cJSON *branches = cJSON_CreateArray();

    /* Always force a default target that sends us back to
     * start so that we are at the beginning of time.
     */
    cJSON_AddItemToArray(branches, sheens_create_branch(NULL, branch_node, false));
    return sheens_create_state_node(cJSON_CreateStringReference(persist),
                                    branches,
                                    false);
}

cJSON* sheens_create_time_object(const icrule_time_t* time)
{
    cJSON* json;

    json = cJSON_CreateObject();

    /* Setup the JavaScript object for out actual schedule
     *
     * Format:
     * object {
     *   daysOfWeek: <number>
     *   seconds: <number> | <sunrise key> | <sunset key>
     * }
     */
    cJSON_AddItemToObjectCS(json,
                            sheens_days_of_week_key,
                            cJSON_CreateNumber(time->day_of_week));

    if (time->use_exact_time) {
        cJSON_AddItemToObjectCS(json,
                                sheens_seconds_key,
                                cJSON_CreateNumber(time->time.seconds));
    } else {
        cJSON* tmp_json;

        switch (time->time.sun_time) {
            case ICRULE_SUNTIME_SUNRISE:
                tmp_json = cJSON_CreateStringReference(sheens_sunrise_key);
                break;
            case ICRULE_SUNTIME_SUNSET:
                tmp_json = cJSON_CreateStringReference(sheens_sunset_key);
                break;
            default:
                cJSON_Delete(json);

                errno = EBADMSG;
                return NULL;
        }

        cJSON_AddItemToObjectCS(json, sheens_seconds_key, tmp_json);
    }

    return json;
}

void sheens_pattern_add_constraints_required(cJSON* pattern)
{
    if (!cJSON_HasObjectItem(pattern, sheens_event_id_key)) {
        cJSON_AddItemToObjectCS(pattern,
                                sheens_event_id_key,
                                cJSON_CreateStringReference(sheens_event_id_bound_key));
    }

    if (!cJSON_HasObjectItem(pattern, sheens_event_time_key)) {
        cJSON_AddItemToObjectCS(pattern,
                                sheens_event_time_key,
                                cJSON_CreateStringReference(sheens_event_time_bound_key));
    }

    if (!cJSON_HasObjectItem(pattern, sheens_sunrise_key)) {
        cJSON_AddItemToObjectCS(pattern,
                                sheens_sunrise_key,
                                cJSON_CreateStringReference(sheens_sunrise_bound_key));
    }

    if (!cJSON_HasObjectItem(pattern, sheens_sunset_key)) {
        cJSON_AddItemToObjectCS(pattern,
                                sheens_sunset_key,
                                cJSON_CreateStringReference(sheens_sunset_bound_key));
    }

    if (!cJSON_HasObjectItem(pattern, sheens_systemstatus_key)) {
        cJSON_AddItemToObjectCS(pattern,
                                sheens_systemstatus_key,
                                cJSON_CreateStringReference(sheens_systemstatus_bound_key));
    }
}

const char* bool2str(bool value)
{
    return (value) ? "true" : "false";
}

