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
 * commServiceEventBrodcaster.h
 *
 * Responsible for generating communication events
 * and broadcasting them to the listening iControl
 * processes (services & clients)
 *
 * Author: jelderton - 7/6/15
 *-----------------------------------------------*/

#ifndef IC_COMM_BROADCAST_EVENTS_H
#define IC_COMM_BROADCAST_EVENTS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <commMgr/commService_event.h>

/*
 * one-time initialization
 */
void startCommEventProducer();

/*
 * shutdown event producer
 */
void stopCommEventProducer();

/*
 * broadcast an "commOnlineChangedEvent" with the eventCode
 * of COMM_ONLINE_EVENT to any listeners
 */
void broadcastCommOnlineChangedEvent(commOnlineChangedEvent *event);

/*
 * broadcast an "activationStateChangedEvent" with the eventCode
 * of ACTIVATION_STATE_CHANGED_EVENT to any listeners
 *
 * @param eventValue - the event value to compliment the code of CLOUD_ASSOCIATION_STATE_CHANGED_EVENT
 */
void broadcastCloudAssociationEvent(int32_t eventValue, bool critical, bool wasCell,
                                    cloudAssociationState cloudAssState, uint64_t lastActiveMillis);

/**
 * Broadcast a media uploaded event to the system.
 *
 * @param eventType The type of media broadcast.
 * @param ruleId The original rule ID (if any) associated with this event.
 * @param requestId The original event ID made during the request. May be zero.
 * @param uploadId If the original event ID is zero then a new unique event ID
 * will be created and used. Otherwise, the original event ID is placed here.
 */
void broadcastMediaUploadedEvent(mediaUploadEventType eventType,
                                 uint64_t ruleId,
                                 uint64_t requestId,
                                 uint64_t uploadId);

#endif // IC_COMM_BROADCAST_EVENTS_H
