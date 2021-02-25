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
 * eventProducer.c
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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <arpa/inet.h>

#include <icLog/logging.h>
#include <icIpc/eventProducer.h>
#include <icIpc/baseEvent.h>
#include <icIpc/ipcMessage.h>
#include "ipcCommon.h"
#include "transport/transport.h"

/*
 * Container of each 'producer' we've allocated
 */
typedef struct _eventProducer
{
    int sockfd;
    uint16_t  serviceIdNum;
} eventProducer;

/*
 * initialize the event producer system for a particular group of events.
 *
 * @param serviceIdNum - the service id to inject into broadcasted events.
 *                       used by the consumer to filter interested events.
 * @return the EventProducer that should be used when sending events
 */
EventProducer initEventProducer(uint16_t serviceIdNum)
{
    // allocate an EventProducer to contain this serviceId.  in the past
    // we kept these in a list, but that is not necessary
    //
    eventProducer *retVal = (eventProducer *)malloc(sizeof(eventProducer));
    if (retVal == NULL) {
        icLogError(API_LOG_CAT, "failed to allocate producer memory: %s", strerror(errno));
        return NULL;
    }

    retVal->serviceIdNum = serviceIdNum;

    if ((retVal->sockfd = transport_pub_register(TRANSPORT_DEFAULT_PUBSUB)) < 0) {
        icLogError(API_LOG_CAT, "failed to create event producer socket: %s", strerror(errno));
        free(retVal);
        return NULL;
    }

    // return the eventProducer as EventProducer
    //
    return (EventProducer)retVal;
}

/*
 * broadcast an event using the serviceId defined by the EventProducer
 *
 * @param handle - the EventProducer created during initEventProducer()
 * @param json - the event, modeled in a JSON object
 */
void broadcastEvent(EventProducer producer, cJSON *json)
{
    eventProducer* session = (eventProducer*) producer;

    // check input parms
    //
    if (session == NULL)
    {
        icLogWarn(API_LOG_CAT, "unable to broadcast event, NULL producer");
        return;
    }
    if (json == NULL)
    {
        icLogWarn(API_LOG_CAT, "unable to broadcast event, NULL json object");
        return;
    }

    insertServiceIdToRawEvent(json, session->serviceIdNum);

#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "broadcasting event; port=%" PRIu16, session->serviceIdNum);
#endif

    transport_publish(session->sockfd, json);
}

/*
 * Closes the sockets created during initEventProducer()
 * Generally called during shutdown.
 */
void shutdownEventProducer(EventProducer producer)
{
    eventProducer* session = (eventProducer*) producer;

    if (session && session->sockfd != -1) {
        transport_close(session->sockfd);
        session->sockfd = -1;

        icLogInfo(API_LOG_CAT, "shutdown event producer");
    }

    // Cleanup producer
    free(producer);
}


