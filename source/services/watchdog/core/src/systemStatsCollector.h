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
// Created by jelder380 on 2/15/19.
//

#ifndef ZILKER_SYSTEMSTATSCOLLECTOR_H
#define ZILKER_SYSTEMSTATSCOLLECTOR_H

#include <icIpc/ipcStockMessagesPojo.h>

/**
 * Function for getting all of the system data
 * and adding it into our runtime stats
 *
 * NOTE: For IPC calls
 *
 * @param output - runtime stats hash map
 */
void collectSystemStats(runtimeStatsPojo *output);

/**
 * Function for getting reboot stats
 *
 * NOTE: For IPC calls
 *
 * @param output - runtime stats hash map
 */
void collectRebootStats(runtimeStatsPojo *output);

/**
 * Function for getting the stats for all services running
 * from Watchdog
 *
 * NOTE: For IPC calls
 *
 * @param output - runtime stats hash map
 */
void collectServiceListStats(runtimeStatsPojo *output);

#endif //ZILKER_SYSTEMSTATSCOLLECTOR_H
