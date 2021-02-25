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
// Created by wboyd747 on 6/3/18.
//

#ifndef ZILKER_AUTOMATIONBROADCASTEVENT_H
#define ZILKER_AUTOMATIONBROADCASTEVENT_H

/**
 * Start the event producer enabling the Automation Service to emit events
 *
 * @return true on success
 */
void startAutomationEventProducer();

/**
 * Stop the event producer.
 */
void stopAutomationEventProducer();

void broadcastAutomationCreatedEvent(const char* id, uint64_t requestId, bool enabled);
void broadcastAutomationDeletedEvent(const char* id, uint64_t requestId);
void broadcastAutomationModifiedEvent(const char* id, uint64_t requestId, bool enabled);

#endif //ZILKER_AUTOMATIONBROADCASTEVENT_H
