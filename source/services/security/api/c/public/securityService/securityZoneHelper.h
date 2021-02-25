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
 * securityZoneHelper.h
 *
 * Set of helper functions for securityZone validation
 * and manipulation.  Available for client-side processing
 * to reduce the need for un-necessary IPC calls.
 *
 * Author: jelderton -  9/27/16.
 *-----------------------------------------------*/

#ifndef ZILKER_SECURITYZONEHELPER_H
#define ZILKER_SECURITYZONEHELPER_H

#include <stdbool.h>
#include <securityService/securityService_pojo.h>

/*
 * validates that the security zone 'type' and 'function' are compatible.
 * for example, using a 'smoke' as an 'entry/exit' is invalid.
 */
bool validateSecurityZoneTypeAndFunction(securityZoneType type, securityZoneFunctionType func);

/*
 * returns the set of zone functions for a particular zone type as a
 * NULL-terminated array.  caller must release the returned array.
 */
securityZoneFunctionType *getSecurityZoneFunctionsForType(securityZoneType type);

/**
 * Indicate if a zone can ever prevent arming when faulted.
 * @param zone
 * @return
 */
bool securityZoneFaultPreventsArming(const securityZone *zone);

#endif // ZILKER_SECURITYZONEHELPER_H
