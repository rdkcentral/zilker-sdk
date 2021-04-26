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

#ifndef ZILKER_SHEENS_JSON_H
#define ZILKER_SHEENS_JSON_H

#include <stdbool.h>
#include <cjson/cJSON.h>
#include <icrule/icrule.h>

#define TIMER_TICK_EVENT_CODE 499

#define sheens_version_key "sheensVersion"

#define sheens_actions_key "actions"
#define sheens_constraints_key "constraints"

#define sheens_branching_key "branching"
#define sheens_branches_key "branches"
#define sheens_pattern_key "pattern"
#define sheens_target_key "target"
#define sheens_type_key "type"

#define sheens_action_key "action"
#define sheens_interpreter_key "interpreter"
#define sheens_source_key "source"

#define sheens_sunrise_key "_sunrise"
#define sheens_sunrise_bound_key "?_sunrise"
#define sheens_sunset_key "_sunset"
#define sheens_sunset_bound_key "?_sunset"
#define sheens_systemstatus_key "_systemStatus"
#define sheens_systemstatus_bound_key "?_systemStatus"

#define sheens_start_value "start"
#define sheens_reset_value "reset"
#define sheens_interpreter_value "ecmascript-5.1"
#define sheens_message_value "message"

#define sheens_event_code_key "_evCode"
#define sheens_event_code_bound_key "?_evCode"
#define sheens_event_time_key "_evTime"
#define sheens_event_time_bound_key "?_evTime"
#define sheens_event_id_key "_evId"
#define sheens_event_id_bound_key "??_evId"
#define sheens_event_value_key "_evVal"
#define sheens_event_value_bound_key "?_evVal"
#define sheens_event_orig_id_bound_key "??origEvId"
#define sheens_event_on_demand_required_key "_evOnDemandEventRequired"

#define sheens_allowed_key "allowed"
#define sheens_days_of_week_key "daysOfWeek"
#define sheens_seconds_key "seconds"

/** Create a new Sheens branch.
 *
 * @param pattern The pattern the branch should match in order to succeed. If NULL then only
 * the target will be poplulated.
 * @param target The target this branch should jump to.
 * @param is_reference If the 'target' is a known global constant then cJSON may use
 * less memory when allocating the JSON object. If unsure then always choose 'false'.
 * @return A new Sheens branch, or NULL on error.
 */
cJSON* sheens_create_branch(cJSON* pattern, const char* target, bool is_reference);

/** Create a new 'state' node for Sheens.
 *
 * Note: This function will always return a valid state JSON object.
 *
 * @param source The JavaScript JSON string used for the Sheen action. If NULL no action
 * function will be added.
 * @param branch_array The array of branches that this node may jump to. If NULL then
 * a new array will be generated for you and a default branch
 * will be generated jumping directly to the 'reset' state.
 * @param is_message If true this state node will only handle external messages.
 * Note: Having a non-NULL 'sources' will disable this flag.
 * @return A JSON object that holds the complete state node.
 */
cJSON* sheens_create_state_node(cJSON* source, cJSON* branch_array, bool is_message);

/**
 * Create a new reset node pointing to a specific
 * branch.
 *
 * All data will be removed from 'bindings' unless
 * it is within the object parameter <em>persist</em>.
 *
 * @param branch_node The name of the branch that will
 * be jumped to once the bindings reset is finished.
 * @return A JSON object that contains a complete
 * Sheens reset node.
 */
cJSON* sheens_create_reset_node(const char* branch_node);

/**
 * Create a JSON object representing the "time" object
 * for Sheens JavaScript. The time object is used by all
 * triggers, schedules, and constraints that must verify
 * time intervals.
 *
 * Object:
 * {
 *   daysOfWeek: <int> (bitmask of each day)
 *   seconds: <int> (hours_to_seconds() + minutes_to_seconds()
 * }
 * @param time The legacy iControl time used in rules.
 * @return JSON object representing the time.
 */
cJSON* sheens_create_time_object(const icrule_time_t* time);

/**
 * Add all required elements into a Sheens JSON pattern match.
 * The required items will be used by extra trigger nodes,
 * constraints, and actions.
 *
 * @param pattern The JSON object representing the pattern match.
 */
void sheens_pattern_add_constraints_required(cJSON* pattern);

/**
 * Convert a boolean into its respective string name.
 *
 * <true>: "true"
 * <false>: "false"
 *
 * @param value The boolean value to be converted.
 * @return The string representing the value "true" or "false".
 */
const char* bool2str(bool value);

#endif //ZILKER_SHEENS_JSON_H
