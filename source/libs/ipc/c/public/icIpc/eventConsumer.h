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
 * eventConsumer.h
 *
 * Set of functions to listen for multicast events
 * and forward to a listener to be decoded and processed.
 *
 * Closely mirrors the BaseJavaEventAdapter.java so that events
 * are process the same regardless of Native or Java implementation.
 *
 * Note: this correlates to BaseJavaEventAdapter.java
 *
 * Authors: jelderton, gfaulkner, tlea
 *-----------------------------------------------*/

#ifndef EVENTCONSUMER_H
#define EVENTCONSUMER_H

#include <time.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <cjson/cJSON.h>
#include <icIpc/ipcStockMessagesPojo.h>
#include <icConcurrent/threadPool.h>

#define EVENTCONSUMER_SUBSCRIBE_ALL UINT16_MAX

/*
 * function signature of the 'adapter' to decode the incoming
 * event messages as needed.  This adapter should also filter
 * on the eventCode to ignore events that are not desired
 *
 * @parm eventCode - the eventCode extracted from the incoming event.  used to determine how (or if) to decode the payload
 * @parm eventValue - the eventValue extracted from the incoming event.
 * @parm jsonPayload - the JSON string that contains all of the information about the event
 */
typedef void (*eventListenerAdapter)(int32_t eventCode, int32_t eventValue, cJSON *jsonPayload);

/*
 * register the supplied 'handler' to be notified when an event comes in from
 * the service with a matching 'serviceIdNum'.  the 'handler' receives raw events
 * that need to be decoded and internally broadcasted.
 *
 * @param serviceIdNum - identifier of the service we want events from
 * @param handler - function to call as events from the service are received
 * @return false if there is a problem
 */
extern bool startEventListener(uint16_t serviceIdNum, eventListenerAdapter handler);

/*
 * un-register the handlers associated with 'serviceIdNum'.  assume that this is called
 * by the handler because it has no listeners to inform.  if this is the last handler,
 * will cleanup and close the event socket.
 *
 * @param serviceIdNum - identifier of the service we want events from
 */
extern void stopEventListener(uint16_t serviceIdNum);

/*
 * force-close the event listener thread and socket.
 * Generally called during shutdown.
 */
extern void shutdownEventListener();

/*
 * collect statistics about the event listeners, and populate
 * them into the supplied runtimeStatsPojo container
 */
extern void collectEventStatistics(runtimeStatsPojo *container, bool thenClear);

/*
 * Register a specific thread pool to handle events from a service
 */
bool registerServiceSpecificEventHandlerThreadPool(uint16_t serviceIdNum, icThreadPool *threadPool);

/*
 * Unregister a specific thread pool to handle events from a service
 */
bool unregisterServiceSpecificEventHandlerThreadPool(uint16_t serviceIdNum);

/*
 * mechanism to direct-inject events (in raw json) through the reader so they
 * can be delivered to listeners as-if they arrived over the socket.
 */
void directlyProcessRawEvent(const char *eventJsonString);

#endif // EVENTCONSUMER_H
