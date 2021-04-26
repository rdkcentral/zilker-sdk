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
 * baseEvent.c
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

#include <string.h>
#include <sys/time.h>

#include <icBuildtime.h>
#include <icIpc/baseEvent.h>
#include <icIpc/eventIdSequence.h>
#include <icTime/timeUtils.h>

/*
 * Transfer the BaseEvent information to a JSON buffer
 * Used when encoding events to broadcast.  Caller needs
 * to release memory via cJSON_Delete()
 * @see cJSON_Delete()
 */
cJSON *baseEvent_toJSON(BaseEvent *event)
{
    // create JSON object
    //
    cJSON *root = cJSON_CreateObject();
    if (event != NULL)
    {
        // add each value from 'event'
        //
        cJSON_AddItemToObjectCS(root, EVENT_ID_JSON_KEY, cJSON_CreateNumber(event->eventId));
        cJSON_AddItemToObjectCS(root, EVENT_CODE_JSON_KEY, cJSON_CreateNumber(event->eventCode));
        cJSON_AddItemToObjectCS(root, EVENT_VALUE_JSON_KEY, cJSON_CreateNumber(event->eventValue));
        uint64_t millis = convertTimespecToUnixTimeMillis(&event->eventTime);
        cJSON_AddItemToObjectCS(root, EVENT_TIME_JSON_KEY, cJSON_CreateNumber(millis));
    }
    return root;
}

/*
 * Extract the BaseEvent information from a JSON 'buffer' and
 * place within 'event'.
 * Used when decoding received events.  Returns 0 on success.
 */
int baseEvent_fromJSON(BaseEvent *event, cJSON *buffer)
{
    // extract each value from 'buffer' for a BaseEvent
    //
    int rc = -1;
    cJSON *item = NULL;

    if ((item = cJSON_GetObjectItem(buffer, EVENT_ID_JSON_KEY)) != NULL)
    {
        event->eventId = (uint64_t)item->valuedouble;
        rc = 0;
    }
    if ((item = cJSON_GetObjectItem(buffer, EVENT_CODE_JSON_KEY)) != NULL)
    {
        event->eventCode = (int32_t)item->valueint;
        rc = 0;
    }
    if ((item = cJSON_GetObjectItem(buffer, EVENT_VALUE_JSON_KEY)) != NULL)
    {
        event->eventValue = (int32_t)item->valueint;
        rc = 0;
    }
    if ((item = cJSON_GetObjectItem(buffer, EVENT_TIME_JSON_KEY)) != NULL)
    {
        uint64_t millis = (uint64_t)item->valuedouble;
        convertUnixTimeMillisToTimespec(millis, &event->eventTime);
        rc = 0;
    }

    return rc;
}

/*
 * helper function to obtain a unique eventId
 */
void setEventId(BaseEvent *event)
{
    // This is a complex problem to solve...  We need a way
    // to create a unique identifier to prevent duplicate events
    // from the same CPE.  Due to the fact our server expects
    // these identifiers to be sequential, we cannot simply
    // grab a random number.
    //
    // The more complicated part is that we have several services
    // running as independent processes, and need to ensure they
    // don't have to synchronize with one-another OR create
    // duplicate identifiers.
    //
    // In the past, we resolved this by adding a kernel-mod,
    // however, that approach is not portable and cannot
    // be used anymore.
    //
    event->eventId = getNextEventId();
}

/*
 * helper function to set the time using the current time
 * via CLOCK_REALTIME.
 */
void setEventTimeToNow(BaseEvent *event)
{
    // ideally, we would use our timeUtils library function,
    // but we don't want to impose a dependency on libicUtil,
    // therefore, this is a copy/paste from "getCurrentTime()"
    //

#ifdef CONFIG_OS_DARWIN
    // do this the Mac way
    //
    struct timeval now;
    gettimeofday(&now, NULL);
    event->eventTime.tv_sec  = now.tv_sec;
    event->eventTime.tv_nsec = now.tv_usec * 1000;
#else

    // standard linux, use real (not monotonic) clock
    //
    clock_gettime(CLOCK_REALTIME, &(event->eventTime));
#endif

}

void baseEvent_copy(BaseEvent *dst, BaseEvent *src)
{
    if (dst != NULL && src != NULL)
    {
        dst->eventCode = src->eventCode;
        dst->eventId = src->eventId;
        dst->eventTime = src->eventTime;
        dst->eventValue = src->eventValue;
    }
}

