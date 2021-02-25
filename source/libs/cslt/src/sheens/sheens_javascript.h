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
// Created by Boyd, Weston on 5/1/18.
//

#ifndef ZILKER_SHEENS_JAVASCRIPT_H
#define ZILKER_SHEENS_JAVASCRIPT_H

#include "sheens_json.h"

#ifndef sizeof_array
/**
 * Determine the number of items in an array.
 *
 * @param a The type of object. (i.e. <struct blah>)
 */
#define sizeof_array(a) (sizeof(a) / sizeof((a)[0]))
#endif

/**
 * Convert iControl legacy rule trigger
 * into Sheens based state machine node. Extra
 * state nodes may be generated in order to fulfill
 * the conversion.
 *
 * @param trigger The iControl legacy trigger
 * @param on_success_node The node to transition to on success.
 * @param on_failure_node The node to transition to on failure.
 * @param nodes_object The top-level JSON object that holds all nodes.
 * @param node_branches The JSON branch array for the current node. This
 * will only be used if multiple patterns need to be placed into the array.
 * @return A JSON pattern that may be placed into a branch array.
 */
cJSON* sheens_trigger2javascript(const icrule_trigger_t* trigger,
                                 const char* on_success_node,
                                 const char* on_failure_node,
                                 cJSON* nodes_object,
                                 cJSON* node_branches);

/**
 * Convert iControl legacy negative rules
 * into Sheens based state machine node. Extra
 * state nodes may be generated in order to fulfill
 * the conversion.
 *
 * @param triggers The iControl legacy trigger list
 * @param nodes_object The top-level JSON object that holds all nodes.
 * @return A JSON pattern that may be placed into a branch array.
 */
cJSON* sheens_negative2javascript(const icrule_trigger_list_t* triggers,
                                  const icrule_constraint_time_t* constraint,
                                  cJSON* nodes_object);

/**
 * Convert iControl legacy constraints
 * into Sheens based state machine node. Extra
 * state nodes may be generated in order to fulfill
 * the conversion.
 *
 * iControl legacy constraints all for nested constraints. Thus
 * this routine is recursive allowing the constraint children to
 * be processed. Once all constraints have been walked through the
 * RPN list will be utilized to construct the sheens JavaScript code
 * which will be placed into <em>js</em>.
 *
 * @param iterator Iterator containing the list of constraints.
 * @param js The buffer that generated JavaScript will be placed into.
 * Currently the size must be determined by the caller.
 * @param rpn The list of AND/OR nested constraints and how to replay
 * those into Sheens JavaScript logic.
 * @return Zero on success and -1 on failure. errno will be defined
 * on failure.
 */
int sheens_constraints2javascript(icLinkedListIterator* iterator, char* js, icLinkedList* rpn);

/**
 * Convert iControl legacy actions
 * into Sheens based state machine node. Extra
 * state nodes may be generated in order to fulfill
 * the conversion.
 *
 * @param iterator Iterator containing the list of actions. The iterator
 * will be destroyed for the caller.
 * @param nodes_object The top-level JSON object that holds all nodes.
 * @param start_branches The JSON branch array for the current node. This
 * will only be used if multiple patterns need to be placed into the array.
 * @return A JSON pattern that may be placed into a branch array.
 */
cJSON* sheens_actions2javascript(const uint64_t ruleId,
                                 icLinkedListIterator* iterator,
                                 cJSON* nodes_object,
                                 cJSON* start_branches);

/**
 * Convert iControl legacy thermostat schedules
 * into Sheens based state machine node. Extra
 * state nodes may be generated in order to fulfill
 * the conversion.
 *
 * @param iterator Iterator containing the list of schedules. The iterator
 * will be destroyed for the caller.
 * @param nodes_object The top-level JSON object that holds all nodes.
 * @param start_branches The JSON branch array for the current node. This
 * will only be used if multiple patterns need to be placed into the array.
 * @return A JSON pattern that may be placed into a branch array.
 */
cJSON* sheens_schedules2javascript(icLinkedListIterator* iterator,
                                   cJSON* nodes_object,
                                   cJSON* start_branches);

#endif //ZILKER_SHEENS_JAVASCRIPT_H
