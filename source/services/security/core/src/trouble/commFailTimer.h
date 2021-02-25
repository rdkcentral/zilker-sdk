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
 * commFailTimer.h
 *
 * Facility to track devices that have reported comm failure, but
 * should not be considered a trouble until the time has surpassed
 * the TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY or
 * the TOUCHSCREEN_SENSOR_COMMFAIL_ALARM_DELAY property values.
 *
 * Author: jelderton -  6/20/19.
 *-----------------------------------------------*/

#ifndef ZILKER_COMMFAILTIMER_H
#define ZILKER_COMMFAILTIMER_H

#include <stdbool.h>
#include <string.h>
#include <deviceService/deviceService_pojo.h>

typedef enum {
    TROUBLE_DELAY_TIMER,    // if timer is meant to track TOUCHSCREEN_SENSOR_COMMFAIL_TROUBLE_DELAY
    ALARM_DELAY_TIMER,      // if timer is meant to track TOUCHSCREEN_SENSOR_COMMFAIL_ALARM_DELAY
} commFailTimerType;

/*
 * notification callback function signature.  this is called
 * when a tracked device reaches the threshold
 */
typedef void (*commFailCallback)(const DSDevice *device, commFailTimerType type);

/*
 * initialize the commFailTimer
 * assumes the global SECURITY_MTX is already held.
 */
void initCommFailTimer();

/*
 * shutdown and cleanup the commFailTimer
 * assumes the global SECURITY_MTX is already held.
 */
void shutdownCommFailTimer();

/*
 * add a device to the commFailTimer.  requires which "trouble"
 * we wish to track (TROUBLE_DELAY or ALARM_DELAY).  once the
 * device reaches the threshold, it will send a notification
 * to the supplied callback function.
 * assumes the global SECURITY_MTX is already held.
 */
bool startCommFailTimer(const char *deviceId, commFailTimerType type, commFailCallback callback);

/*
 * remove a device from the tracker.  this will occur naturally
 * when the device reaches the threshold and sends a notification.
 * primarily used when the device starts communicating or was
 * removed from the system.
 * assumes the global SECURITY_MTX is already held.
 */
void stopCommFailTimer(const char *deviceId, commFailTimerType type);

/*
 * returns if this device is currently being tracked within the commFailTimer
 * assumes the global SECURITY_MTX is already held.
 */
bool hasCommFailTimer(const char *deviceId, commFailTimerType type);

/*
 * uses the "last contacted time" and the associated _DELAY property
 * to see if this device is technically COMM_FAIL (from a trouble standpoint)
 *
 * NOTE: this makes IPC calls to deviceService so requires the global SECURITY_MTX to NOT be held.
 */
bool isDeviceConsideredCommFail(const DSDevice *device, commFailTimerType type);

#endif //ZILKER_COMMFAILTIMER_H
