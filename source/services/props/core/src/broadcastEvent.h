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
 * Responsible for generating property set/del events
 * and broadcasting them to the listening iControl
 * processes (services & clients)
 *
 * Author: jelderton - 7/7/15
 *-----------------------------------------------*/

#ifndef IC_PROP_BROADCAST_EVENTS_H
#define IC_PROP_BROADCAST_EVENTS_H

#include <stdint.h>
#include <propsMgr/propsService_pojo.h>
#include <propsMgr/propsService_event.h>

/*
 * one-time initialization
 */
void startPropsEventProducer();

/*
 * shutdown event producer
 */
void stopPropsEventProducer();

/*
 * broadcast an "cpePropertyEvent" to any listeners
 *
 * @param eventValue - the event value; should be GENERIC_PROP_ADDED, GENERIC_PROP_DELETED, or GENERIC_PROP_UPDATED
 * @param key - the property name that was added, deleted, or updated
 * @param value - the property value that was added or updated
 * @param version - the internal version - done this way so event generation doesn't have to ask and potentially cause a deadlock
 * @param source - where this change originated from
 */
void broadcastPropertyEvent(int eventValue, const char *key, const char *value, uint64_t version, propSource source);

#endif // IC_PROP_BROADCAST_EVENTS_H
