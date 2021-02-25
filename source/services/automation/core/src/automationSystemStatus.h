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
// Created by wboyd747 on 6/7/18.
//

#ifndef ZILKER_AUTOMATIONSYSTEMSTATUS_H
#define ZILKER_AUTOMATIONSYSTEMSTATUS_H

typedef enum {
    AUTOMATION_SYSTEMSTATUS_HOME = 0,
    AUTOMATION_SYSTEMSTATUS_AWAY,
    AUTOMATION_SYSTEMSTATUS_STAY,
    AUTOMATION_SYSTEMSTATUS_NIGHT,
    AUTOMATION_SYSTEMSTATUS_VACATION,
    AUTOMATION_SYSTEMSTATUS_ARMING,
    AUTOMATION_SYSTEMSTATUS_ALARM,
} automationSystemStatus;

void automationRegisterSystemStatus(void);
void automationUnregisterSystemStatus(void);

automationSystemStatus automationGetSystemStatus(void);
const char* automationGetSystemStatusLabel(void);

#endif //ZILKER_AUTOMATIONSYSTEMSTATUS_H
