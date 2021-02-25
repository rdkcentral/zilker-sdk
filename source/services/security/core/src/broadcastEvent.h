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
 * broadcastEvent.h
 *
 * Responsible for generating events and broadcasting
 * them to the listening iControl processes (services & clients)
 *
 * Author: jelderton - 7/9/15
 *-----------------------------------------------*/

#ifndef IC_SECURITY_BROADCAST_EVENTS_H
#define IC_SECURITY_BROADCAST_EVENTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <securityService/securityService_event.h>
#include <deviceService/deviceService_pojo.h>

/*
 * one-time initialization
 */
void startSecurityEventProducer();

/*
 * shutdown event producer
 */
void stopSecurityEventProducer();

/*
 * broadcast an "systemModeChangedEvent" with the eventCode
 * of SYSTEM_MODE_CHANGED_EVENT to any listeners
 *
 * @param oldMode - the string representation of the previous "systemMode"
 * @param newMode - the string representation of the current "systemMode"
 * @param version - the config file version
 * @param requestId - the identifier of the request that caused the mode change
 */
void broadcastSystemModeChangedEvent(char *oldMode, char *newMode, uint64_t version, uint64_t requestId);

/*
 * returns false if the trouble cannot be AUDIBLE right now
 * due to the "deferTroublesConfig" being active.
 *
 * this function is called from broadcastTroubleEvent()
 */
bool canHaveTroubleIndication(troubleEvent *event);

/*
 * broadcast an "troubleEvent" with the eventCode of
 * TROUBLE_OCCURED_EVENT or TROUBLE_CLEARED_EVENT
 *
 * @param event - the troubleEvent to broadcast
 * @param eventCode - TROUBLE_OCCURED_EVENT or TROUBLE_CLEARED_EVENT
 * @param eventValue - the event "value" to include
 *
 * @return indicationType the trouble was broadcast with, returns -1 if the event was not broadcast
 */
indicationType broadcastTroubleEvent(troubleEvent *event, int32_t eventCode, int32_t eventValue);

/*
 * broadcast a "securityZoneEvent"
 * @param event - the zone event to send
 * @param eventCode - the event "code" to include
 * @param eventValue - the event "value" to include
 * @param requestId - the identifier of the request that caused the change
 */
void broadcastZoneEvent(securityZoneEvent *event, int32_t eventCode, int32_t eventValue, uint64_t requestId);

/*
 * broadcast a "zoneDiscoveredEvent"
 * @param zoneNumber - the zone number for the discovered zone
 * @param details - the device discovery details to put into the event
 */
void broadcastZoneDiscoveredEvent(uint32_t zoneNumber, DSEarlyDeviceDiscoveryDetails *details);

/*
 * broadcast a zoneReorderedEvent
 * @param zones - zones that have been reordered
 */
void broadcastZoneReorderedEvent(icLinkedList *zones);

/*
 * broadcast a zoneRemovedEvent
 * @param event - the populated event to broadcast
 */
void broadcastZonesRemovedEvent(securityZonesRemovedEvent *event);

/*
 * send "arming" event
 */
void broadcastArmingEvent(uint32_t eventCode, uint32_t eventValue,
                          systemPanelStatus *status,     // required
                          armSourceType source,          // required
                          const char *userCode,
                          uint32_t remainSecs,
                          bool anyZonesFaulted,
                          indicationType eventIndication);

/*
 * send "armed" event
 */
void broadcastArmedEvent(uint32_t eventValue,
                         systemPanelStatus *status,     // required
                         armSourceType source,          // required
                         armModeType requestedArmMode,  // original arm mode request
                         const char *userCode,
                         bool isRearmed,
                         bool didZonesFault,
                         indicationType eventIndication);

/*
 * send "entry delay" event
 */
void broadcastEntryDelayEvent(uint32_t eventCode, uint32_t eventValue,
                              systemPanelStatus *status,
                              armSourceType source,          // required
                              const char *userCode,
                              uint16_t entryDelaySecs,
                              indicationType eventIndication,
                              bool anyZonesFaulted,
                              bool isExitError);

/*
 * send "disarmed" event
 */
void broadcastDisarmedEvent(systemPanelStatus *status,
                            const char *userCode,
                            armSourceType disarmSource,
                            bool anyZonesFaulted,
                            indicationType eventIndication);

/*
 * send "alarm" event.  used for a variety of codes:
 *  ALARM_EVENT_STATE_READY
 *  ALARM_EVENT_STATE_NOT_READY
 *  ALARM_EVENT_ALARM
 *  ALARM_EVENT_ALARM_CLEAR
 *  ALARM_EVENT_ALARM_CANCELLED
 *  ALARM_EVENT_ALARM_RESET
 *  ALARM_EVENT_PANIC
 *  ALARM_EVENT_TEST_MODE
 *  ALARM_EVENT_ACKNOWLEDGED
 *  ALARM_EVENT_SEND_ALARM
 */
void broadcastAlarmEvent(uint32_t eventCode, uint32_t eventValue,
                         systemPanelStatus *status,     // required
                         armSourceType source,          // required
                         alarmDetails *alarmInfo,
                         securityZone *zone,            // supplied when a zone caused the alarm
                         alarmPanicType panicType,
                         armSourceType panicSource,
                         indicationType alarmIndication);

/*
 * send user add/update/delete event.  eventCode should be one of:
 * ALARM_EVENT_USER_CODE_ADDED, ALARM_EVENT_USER_CODE_MOD, or ALARM_EVENT_USER_CODE_DEL
 */
void broadcastUserCodeEvent(uint32_t eventCode, uint64_t configVersion, keypadUserCode *user, armSourceType source);

/*
 * called when commService has delivered an alarm event for us.
 * this will clear that cached alarm from memory, preventing it from
 * being returned via provideAlarmMessagesNeedingAcknowledgement()
 */
void receivedAlarmMessageDeliveryAcknowledgement(uint64_t alarmEventId);

/*
 * populates the supplied list with RAW JSON of each alarm event broadcasted
 * that has NOT been acknowledged via receivedAlarmMessageDeliveryAcknowledgement()
 *
 * the list will sort these by eventId so caller can replay them in the order they were generated
 */
void provideAlarmMessagesNeedingAcknowledgement(icLinkedList *targetList);

/*
 * used to report the number of ourstanding alarm messages we have that have
 * not been acknowledged yet.
 */
uint32_t getAlarmMessagesNeedingAcknowledgementCount();

#endif // IC_SECURITY_BROADCAST_EVENTS_H
