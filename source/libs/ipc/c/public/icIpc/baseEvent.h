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
 * baseEvent.h
 *
 * Base container to represent an Event within the iControl system.
 * Closely mirrors the BaseEvent.java so that events can be propagated
 * regardless of Native or Java implementation.  Any custom events
 * sould have this as part of their definition so that all events
 * have the common elements (id, code, value, time)
 *
 * During broadcast, the event will be encoded into a JSON string
 * that contains the base information along with
 * and a JSON string as the payload.  This allows receivers
 * to decode the content and potentially place into another container.
 *
 * Receivers of the event should use the 'eventCode' and 'eventValue'
 * to determine how to decipher the 'payload'.
 *
 * Note: this correlates to BaseEvent.java
 *
 * Authors: jelderton, gfaulkner, tlea
 *-----------------------------------------------*/

#ifndef BASE_EVENT_H
#define BASE_EVENT_H

#include <time.h>
#include <stdint.h>
#include <cjson/cJSON.h>
#include "pojo.h"

/*
 * Define the multicast group for events to be sent over.
 * This is the same multicast group as the Java code.
 * NOTE: this is only used if the IC_USE_MULTICAST is defined
 */
#define IC_EVENT_MULTICAST_GROUP    "225.0.0.50"

/*
 * Set of JSON keys that are commonly used when encoding/decoding
 * the BaseEvent to/from JSON.  These should correlate to BaseEvent.java
 * to ensure transition between Native and Java works the same.
 */
#define EVENT_ID_JSON_KEY       "_evId"
#define EVENT_CODE_JSON_KEY     "_evCode"
#define EVENT_VALUE_JSON_KEY    "_evVal"
#define EVENT_TIME_JSON_KEY     "_evTime"

/*
 * abstract event object
 */
typedef struct _BaseEvent
{
    Pojo base;                  // Base object; use pojo.h interface
    uint64_t eventId;           // unique event identifier
    int32_t  eventCode;         // code used to describe the event so receivers know how to decode (ex: ARM_EVENT_CODE)
    int32_t  eventValue;        // auxiliary value to augment the eventCode however the event deems necessary (ex: percent complete of an UPGRADE_DOWNLOAD_EVENT)
    struct timespec eventTime;  // when the event occurred
} BaseEvent;

/*
 * overlay that can be used on any generated event
 * to extract the base event data so the appropriate
 * typecast can be used.  For example:
 *
 *   void handleGenericEvent(void *event)
 *   {
 *       BaseEventOverlay *ptr = (BaseEventOverlay *)event;
 *       if (ptr->baseEvent.eventCode == RULE_ADDED_EVENT)
 *       {
 *           ruleEvent *ruleEvent = (ruleEvent *)event;
 *       }
 *   }
 */
typedef struct
{
    BaseEvent baseEvent;         // event id, code, val, time
    // all other generated info would follow
} BaseEventOverlay;

/*
 * Transfer the BaseEvent information to a JSON buffer
 * Used when encoding events to broadcast.  Caller needs
 * to release memory via cJSON_Delete()
 * @see cJSON_Delete()
 */
cJSON *baseEvent_toJSON(BaseEvent *event);

/*
 * Extract the BaseEvent information from a JSON 'buffer' and
 * place within 'event'.
 * Used when decoding received events.  Returns 0 on success.
 */
int baseEvent_fromJSON(BaseEvent *event, cJSON *buffer);

/*
 * helper function to obtain a unique eventId
 */
void setEventId(BaseEvent *event);

/*
 * helper function to set the time using the current time
 * via CLOCK_REALTIME.
 */
void setEventTimeToNow(BaseEvent *event);

/**
 * Safely copy the details of a BaseEvent.
 * @note Attempting to memcpy a baseEvent into another will overwrite important private information.
 */
void baseEvent_copy(BaseEvent *dst, BaseEvent *src);


#endif // BASE_EVENT_H
