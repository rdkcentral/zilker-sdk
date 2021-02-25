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
// Created by tlea on 4/16/18.
//

#ifndef ZILKER_EMITTEDMESSAGEHANDLER_H
#define ZILKER_EMITTEDMESSAGEHANDLER_H

#include <cjson/cJSON.h>
#include <stdbool.h>

#include <jsonrpc/jsonrpc.h>

typedef jsonrpc_method_t automationActionHandler_t;

/**
 * Initialize the automation action handling sub-system.
 *
 * @return True if emit handling has been fully initialized.
 */
bool automationActionInit(void);

/**
 * Terminate and cleanup the automation action handlers.
 */
void automationActionDestroy(void);

/**
 * Custom action message RPC handler.
 *
 * Any handler that wish to receive targeted actions
 * must register their operations handler with the system.
 *
 * @param name The name of the action that will be processed.
 * @param handler The callback routine that will be notified
 * when an instance of the action "name" has been called.
 */
void automationActionRegisterOps(const char* name, automationActionHandler_t handler);

/**
 * Post a new action message(s) in JSON format to be handled
 * asynchronously for a container.
 *
 * The JSON action will be fully handled (i.e. deallocated)
 * by the action sub-system.
 *
 * @param id The ID of the container to run the actions within.
 * @param action An array, or single instance, of JSONRPC objects that
 * represent the action to call and their parameter data.
 * @return True if the action(s) were successfully
 * posted for a container. On failure to post the action
 * it is up to the caller how to proceed.
 */
bool automationActionPost(const char* id, cJSON* action);

#endif //ZILKER_EMITTEDMESSAGEHANDLER_H
