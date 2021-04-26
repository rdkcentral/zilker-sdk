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

#ifndef ZILKER_SHEENS_REQUEST_H
#define ZILKER_SHEENS_REQUEST_H

#include <cjson/cJSON.h>
#include <stdint.h>

int sheens_parse_device_id(const char* src, char** uuid, char** endpoint);

/**
 *
 * @param src The incoming device ID in '<eui64>.<ep>' format
 * @param resource The resource to interact with in the device.
 * @return A REST URL path that will point to the device and resource
 * to manipulate.
 */
char* sheens_get_device_uri(const char* src, const char* resource);

/** Create a 'device' resource URI from the passed
 * in icrule ID given a resource name and mode
 * of operation.
 *
 * @param deviceId The ID passed in from the legacy rule.
 * @param resource The resource to interact with in the device.
 * @param actionSuppressResourceURI An optional resource ID that will be read before writing it. If the resource is "true"
 *                                  when the jsonrpc action executes, the write is not performed.
 * @return A JSON object that contains all the necessary
 * information to emit from the machine.
 */
cJSON* sheens_create_write_device_request(const char* deviceId,
                                          const char* resource,
                                          const char* actionSuppressResourceURI,
                                          const char* value);

cJSON* sheens_create_timer_fired_object(const char* uuid);

cJSON* sheens_create_timer_emit_object(uint32_t seconds,
                                       const char* uuid,
                                       cJSON* message);

/** Create a complete instance for a timer.
 * The routine will create the time, add
 * the pattern match into the start braches
 * pattern recognition array, and create the
 * new node that handles the timer being fired.
 *
 * The timer will then create the emit message
 * that sets up the timer, and pass that back to
 * the caller to be added into an action 'emit'.
 *
 * @param seconds The number of seconds the timer should
 * delay before firing.
 * @param action_js The JavaScript that will be placed
 * into the new timer node in order to handle the
 * timer being fired.
 * @param nodes_object The master object that contains
 * all 'nodes' that represent the state machine. The
 * new timer handling node will be placed in here.
 * @param start_branches The array of branches that
 * pattern match in start. The timer will place the
 * special 'timerFired' with new UUID in this field.
 * @return A JSON representation of the message to
 * be emitted to set up the timer.
 */
cJSON* sheens_create_timer_oneshot_request(uint32_t seconds,
                                           const char* action_js,
                                           cJSON* nodes_object,
                                           cJSON* start_branches);

#endif //ZILKER_SHEENS_REQUEST_H
