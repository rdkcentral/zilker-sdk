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
 * securityZone.h
 *
 * collection of 'securityZone' objects, kept in memory.
 * during init, a call will be made to deviceService to get all
 * of the known sensors.  each sensor device that has a 'secZone'
 * attribute will be wrapped into a securityZone object and kept
 * in an ordered list.
 *
 * the 'secZone' attribute is actually a JSON object containing
 * the zone details for the sensor.  it's done this way so that
 * securityService can properly provide a layer above the 'sensor'
 * without easily exposing each of the attributes that make up a
 * logical 'security zone'.  for example, saving the 'displayIndex'
 * in a public attribute of a sensor could not prevent a service or
 * application from altering that value - which would have dire
 * consequences on the zone objects (dup numbers or out of sync with
 * the central station).
 *
 * several of the functions below will utilize the sharedTaskExecutor,
 * which is a queue of taks to be executed in FIFO order.  we added this
 * complexity because there are several situations where saving metadata
 * or sending events needs to occur outside of the SECURITY_MTX; and we
 * need to serialize those operations to handle quick succession of events
 * or requests.  For example, when a zone is added, it most likely will have
 * several update events immediately following the add.  we need the events
 * from those modifications to be sent in the order they were processed.
 *
 * Author: jelderton - 7/12/16
 *-----------------------------------------------*/

#ifndef ZILKER_SECURITYZONE_H
#define ZILKER_SECURITYZONE_H

#include <stdint.h>
#include <stdbool.h>

#include <icIpc/ipcStockMessagesPojo.h>
#include <icTypes/icLinkedList.h>
#include <securityService/securityService_pojo.h>

/*
 * one-time init to load the zones via deviceService.
 * since this makes requests to deviceService, should
 * not be called until all of the services are available
 */
void initSecurityZonesPublic();
void destroySecurityZonesPublic();

/*
 * used to get runtime status (via IPC)
 */
void getSecurityZoneStatusDetailsPublic(serviceStatusPojo *output);

/*
 * return a linked list of known securityZone objects (sorted by displayIndex).
 * caller is responsible for freeing the list and the elements
 *
 * @see zoneFreeFunc
 */
icLinkedList *getAllSecurityZonesPublic();

/*
 * populate a linked list with known securityZone objects
 * caller is responsible for freeing the elements
 *
 * @see zoneFreeFunc
 */
void extractAllSecurityZonesPublic(icLinkedList *targetList);

/*
 * return the zone with the supplied zoneNumber.
 * caller is responsible for freeing the returned object
 */
securityZone *getSecurityZoneForNumberPublic(uint32_t zoneNumber);

/*
 * locate the zone with the supplied zoneNumber, and copy
 * the information into the provided object.
 */
bool extractSecurityZoneForNumberPublic(uint32_t zoneNumber, securityZone *targetZone);

/*
 * quick search to see if any zones in the known set have a 'trouble'
 */
//bool isAnySecurityZonesTroubled();

/*
 * populate the linked list with cloned securityZone objects that
 * have a trouble.  caller is responsible for freeing the elements
 *
 * @see zoneFreeFunc
 */
//bool extractTroubledSecurityZones(icLinkedList *targetList);

/*
 * update the zone within the list using the information provided.  if
 * the 'requestId' is greater then 0, then it will be included with
 * the securityZoneEvent
 */
updateZoneResultCode updateSecurityZonePublic(securityZone *copy, uint64_t requestId);

#if 0    // DISABLED for now.  These were "nice to have" that are not getting utilized
/*
 * update the zone muted flag.
 */
bool updateSecurityZoneMutedPublic(uint32_t zoneNum, zoneMutedType muteState, uint64_t requestId);

/*
 * update the zone bypassed flag.
 */
bool updateSecurityZoneBypassedPublic(uint32_t zoneNum, bool bypassed, uint64_t requestId);
#endif

/*
 * removes the zone with this zoneNumber.  if the 'requestId' is
 * greater then 0, then it will be included with the securityZoneEvent
 */
bool removeSecurityZonePublic(uint32_t zoneNumber, uint64_t requestId);

/*
 * attempts a zone bypass toggle with a provided code.  The code must be valid
 * and authorized for bypassing.
 */
bool bypassToggleSecurityZonePublic(uint32_t zoneNumber, const char *code, armSourceType source, uint64_t requestId);

/**
 * Get the zone number based on a URI(could be resouce, endpoint, or device)
 *
 * @param uri the uri to use when searching
 * @return the zone number or 0 if not found
 */
uint32_t getZoneNumberForUriPublic(char *uri);

/*
 * returns true is security zone is a panic zone
 */
bool isSecurityZonePanic(securityZone *zone);

/*
 * returns true is security zone is a silent zone
 */
bool isSecurityZoneSilent(securityZone *zone);

/*
 * returns true is security zone is a life safety device
 */
bool isSecurityZoneLifeSafety(securityZone *zone);

/*
 * returns true is security zone is a panic zone or life safety device
 */
bool isSecurityZonePanicOrLifeSafety(securityZone *zone);

/*
 * returns true is security zone is a non-silent burglary zone
 */
bool isSecurityZoneBurglary(securityZone *zone);

/**
 * Determine if a zone is representative of a whole device or
 * is just one of many. Multi-zone devices have slightly different
 * trouble reporting and detection rules:
 * 1. Critical troubles for a zone are demoted to error (other zones on the device may continue to function).
 * 2. Multizone devices may have supervisory troubles (e.g., commfail), without necessarily having troubles on
 *    individual zones.
 *    On single-zone devices, supervisory troubles are naturally represented by the one zone.
 * @param zone
 * @return true when the zone _is_ the device, otherwise the device has other zones.
 */
bool isSecurityZoneSimpleDevice(const securityZone *zone);

/*
 * implementation of 'linkedListItemFreeFunc' to release
 * a securityZone object.
 */
void zoneFreeFunc(void *item);

#endif //ZILKER_SECURITYZONE_H
