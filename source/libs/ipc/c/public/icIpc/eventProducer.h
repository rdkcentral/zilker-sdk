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
 * eventProducer.h
 *
 * Set of functions to broadcast an encoded BaseEvent
 * (or variant) to any listeners.  Uses multicast UDP
 * sockets as the mechanism to allow for minimal delay
 * and not deal with synchronous message handshaking with
 * each potential listening process.
 *
 * Closely mirrors the JavaEventProducer.java so that events
 * can be propagated regardless of Native or Java implementation.
 *
 * Receivers of the event should use the 'eventCode' and
 * 'eventValue' to determine how to decipher the 'payload'.
 *
 * Note: this correlates to JavaEventProducer.java
 *
 * Authors: jelderton, gfaulkner, tlea
 *-----------------------------------------------*/

#ifndef EVENTPRODUCER_H
#define EVENTPRODUCER_H

#include <stdint.h>
#include <cjson/cJSON.h>

/*
 * define the instance that should be created and used
 */
#define EventProducer void*

/*
 * initialize the event producer system for a particular group of events.
 *
 * @param serviceIdNum - the service id to inject into broadcasted events.
 *                       used by the consumer to filter interested events.
 * @return the EventProducer that should be used when sending events
 */
EventProducer initEventProducer(uint16_t serviceIdNum);

/*
 * broadcast an event using the serviceId defined by the EventProducer
 *
 * @param handle - the EventProducer created during initEventProducer()
 * @param json - the event, modeled in a JSON object
 */
extern void broadcastEvent(EventProducer producer, cJSON *json);

/*
 * Closes the sockets created during initEventProducer()
 * Generally called during shutdown.
 */
extern void shutdownEventProducer(EventProducer producer);

#endif // EVENTPRODUCER_H
