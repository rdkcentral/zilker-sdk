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

/*
 * Created by Thomas Lea on 9/19/16.
 */

#ifndef ZILKER_DEVICECOMMUNICATIONWATCHDOG_H
#define ZILKER_DEVICECOMMUNICATIONWATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*deviceCommunicationWatchdogCommFailedCallback)(const char *uuid);
typedef void (*deviceCommunicationWatchdogCommRestoredCallback)(const char *uuid);

void deviceCommunicationWatchdogInit(deviceCommunicationWatchdogCommFailedCallback failedCallback, deviceCommunicationWatchdogCommRestoredCallback restoredCallback);
void deviceCommunicationWatchdogTerm();

/*
 * Adjust the monitor interval.  This is mostly useful for unit tests where the default interval is too long.
 */
void deviceCommunicationWatchdogSetMonitorInterval(uint32_t seconds);

void deviceCommunicationWatchdogMonitorDevice(const char *uuid, const uint32_t commFailTimeoutSeconds, bool inCommFail);
void deviceCommunicationWatchdogStopMonitoringDevice(const char *uuid);

void deviceCommunicationWatchdogPetDevice(const char *uuid);
void deviceCommunicationWatchdogForceDeviceInCommFail(const char *uuid);

int32_t deviceCommunicationWatchdogGetRemainingCommFailTimeoutForLPM(const char *uuid, uint32_t commFailDelaySeconds);
void deviceCommunicationWatchdogResetTimeoutForDevice(const char *uuid, uint32_t commFailTimeoutSeconds);

#endif //ZILKER_DEVICECOMMUNICATIONWATCHDOG_H
