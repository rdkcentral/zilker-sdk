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
 * securitySystemTracker
 * 
 * Collects events that need to be added into our statistics gathering.
 *
 * Author: jelder380 - 5/6/19.
 *-----------------------------------------------*/

#ifndef ZILKER_SECURITYSYSTEMTRACKER_H
#define ZILKER_SECURITYSYSTEMTRACKER_H

#include <icIpc/ipcStockMessagesPojo.h>
#include "securityService/securityService_pojo.h"

/**
 * Checks and adds arm failure reasons, will only
 * keep track of the events if arm reason is not successful
 *
 * @param armResult - the result of trying to arm the system
 */
void addArmFailureEventToTracker(armResultType armResult);

/**
 * Checks and adds disarm failure reasons, will only
 * keep track of the events if disarm reason is not successful
 *
 * @param disarmResult - the result of trying to disarm the system
 */
void addDisarmFailureEventToTracker(disarmResultType disarmResult);

/**
 * Collects the events and adds them into the runtime
 * stats hash map.
 *
 * NOTE: will remove rest the events once collected.
 *
 * @param output - the stats hash map
 */
void collectArmDisarmFailureEvents(runtimeStatsPojo *output);

/**
 * Initializes the security system tracker
 */
void initSecuritySystemTracker();

/**
 * Cleans up the security system tracker
 */
void destroySecuritySystemTracker();

#endif //ZILKER_SECURITYSYSTEMTRACKER_H

//+++++++++++
//EOF
//+++++++++++
