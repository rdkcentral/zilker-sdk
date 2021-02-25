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
 * securityProps.h
 *
 * common location for properties that pertain to the
 * security service.  it will initially load the values
 * and keep the cached values up-to-date via property change events.
 * this is intended for on-demand query use.  if the code needs
 * to react to the property change event, it is recommended that it
 * also register itself for those updates.
 *
 * Author: jelderton -  3/23/19.
 *-----------------------------------------------*/

#ifndef ZILKER_SECURITYPROPS_H
#define ZILKER_SECURITYPROPS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <propsMgr/commonProperties.h>

#define DEFAULT_TAMPER_ENABLED              true
#define DEFAULT_BATTERY_ENABLED             true

/*
 * load initial property values and register for changes
 */
void initSecurityProps();

/*
 * cleanup during shutdown
 */
void cleanupSecurityProps();

/*
 * return the minute duration of a sensor in comm fail
 * before declaring it a "comm fail" trouble
 */
uint32_t getDeviceOfflineCommFailTroubleMinutesProp();

/*
 * return the minute duration of a sensor in comm fail
 * before declaring it in "comm fail alarm"
 */
uint32_t getDeviceOfflineCommFailAlarmTroubleMinutesProp();

/*
 * return the cached value for NO_ALARM_ON_COMM_FAILURE
 */
bool getNoAlarmOnCommFailProp();

/*
 * return the cached value for TAMPER_ENABLED_BOOL_PROP
 */
bool getSystemTamperEnabledProp();

/*
 * return the cached (negated) value for IGNORE_BATTERY_BOOL_PROPERTY
 */
bool getSystemBatteryEnabledProp();

/*
 * return the cached value for DURESSCODE_DISABLED
 */
bool getDuressCodeDisabledProp();

/*
 * return the cached value for ALARM_CANCEL_CONTACT_ID
 * caller should free the returned value (if not NULL)
 */
char *getAlarmCancelCustomContactIdProp();

/*
 * return the "setup wizard" (activation) state
 */
setupWizardState getSetupWizardStateProp();

#endif // ZILKER_SECURITYPROPS_H
