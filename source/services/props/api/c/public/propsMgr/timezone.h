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
 * timezone.h
 *
 * Helper to register for a "property changed event"
 * and adjust the internal clock when the CPE_TZ
 * property changes.
 *
 * Author: jelderton - 2/4/16
 *-----------------------------------------------*/

#ifndef ZILKER_TIMEZONE_H
#define ZILKER_TIMEZONE_H

/*
 * add this process as a property change listener, specificly
 * looking for a change in the CPE_TZ property.  when that occurs,
 * reset the timezone for this process via tzset()
 */
void autoAdjustTimezone();

/**
 * Stop listening to CPE_TZ property changes
 */
void disableAutoAdjustTimezone();

#endif //ZILKER_TIMEZONE_H
