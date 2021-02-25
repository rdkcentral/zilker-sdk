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
 * ipcHandler.h
 *
 * Implement functions that were stubbed from the
 * generated IPC Handler.  Each will be called when
 * IPC requests are made from various clients.
 *
 * Author: jelderton - 7/7/15
 *-----------------------------------------------*/

#ifndef WATCHDOG_HANDLER_H
#define WATCHDOG_HANDLER_H


#include "watchdogService_ipc_handler.h"   // local IPC handler
#include "configParser.h"
#include <time.h>

/*
 * Stores the start time for watchdog
 */
void storeWatchdogStartTime(uint64_t startTime);

/*
 * copy from 'serviceDefinition' to 'processInfo' pojo
 */
void transferServiceDefinitionToProcessInfo(serviceDefinition *def, processInfo *output);

#endif // WATCHDOG_HANDLER_H
