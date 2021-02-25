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
 * securityZonePrivate.h
 *
 * set of 'internal' private functions for use where the
 * SECURITY_MTX is already held.  placed into a separate header
 * to help reduce confusion.
 *
 * Author: jelderton - 11/13/18
 *-----------------------------------------------*/

#ifndef ZILKER_SECURITYZONEPRIVATE_H
#define ZILKER_SECURITYZONEPRIVATE_H

#include <stdint.h>
#include <stdbool.h>

#include <icIpc/ipcStockMessagesPojo.h>
#include <icTypes/icLinkedList.h>
#include <securityService/securityService_pojo.h>
#include "zone/securityZone.h"

/*
 * return a pointer to the securityZone with this zoneNumber
 * this is internal and should therefore be treated as READ-ONLY
 */
securityZone *findSecurityZoneForNumberPrivate(uint32_t zoneNumber);

/*
 * return a pointer to the securityZone with this displayIndex
 * this is internal and should therefore be treated as READ-ONLY
 */
securityZone *findSecurityZoneForDisplayIndexPrivate(uint32_t displayIndex);

/*
 * returns if there are any life-safety zones in the list.
 * primarily used by trouble 'replay' when determining if a system
 * trouble needs to be treated as life-safety because we have one
 */
bool haveLifeSafetyZonePrivate();

/**
 * Get the zones based on the deviceId
 * this is internal and the zones in the list should therefore be treated as READ-ONLY
 * Caller must free the returned list but should not free the zones contained within
 */
icLinkedList *getZonesForDeviceIdPrivate(char *deviceId);

/**
 * Get the whether any zones are bypassed
 * securityMutex must be locked when calling this.
 * @return
 */
bool isZoneBypassActivePrivate();

/*
 * take a specific zone in/out of "swinger shutdown" state.
 * done as a specific modification because this is tied directly
 * to the TROUBLE_REASON_SWINGER trouble reason.
 */
void setZoneSwingerShutdownStatePrivate(securityZone *zone, bool isSwingerFlag, uint64_t alarmSessionId);

#endif // ZILKER_SECURITYZONEPRIVATE_H
