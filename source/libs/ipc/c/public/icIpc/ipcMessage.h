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
 * ipcMessage.h
 *
 * Container used for sending and receiving IPC Messages.
 * This supports messages to/from Native & Java Services.
 * 
 * By convention, the message payload should be a JSON
 * formatted string so that both Clients and Services
 * (regardless of Java/C) can decode the message content.
 *
 * Note: this correlates closely to IPCMessage.java
 *
 * Author: jelderton
 *-----------------------------------------------*/

#ifndef IPCMESSAGE_H
#define IPCMESSAGE_H

#include <stdlib.h>
#include <stdint.h>
#include <icBuildtime.h>        // possible 'verbose' flags set

/*
 * Possible return codes for sending or receiving IPC messages
 */
typedef enum {
    IPC_SUCCESS          = 0,
    IPC_CONNECT_ERROR    = 1,   // unable to establish connection with Service
    IPC_SEND_ERROR       = 2,
    IPC_READ_ERROR       = 3,
    IPC_INVALID_ERROR    = 4,   // usually used by Services when processing requests
    IPC_GENERAL_ERROR    = 5,   // usually used by Services when processing requests
    IPC_SERVICE_DISABLED = 6,   // service is disabled or not installed
    IPC_TIMEOUT          = 7,
    IPC_SHUT_DOWN        = 8,   // IPC should be shutdown after sending reply
} IPCCode;

/*
 * labels that correlate to IPCCode values (for debugging)
 */
static const char *IPCCodeLabels[] = {
    "IPC_SUCCESS",          // IPC_SUCCESS
    "IPC_CONNECT_ERROR",    // IPC_CONNECT_ERROR
    "IPC_SEND_ERROR",       // IPC_SEND_ERROR
    "IPC_READ_ERROR",       // IPC_READ_ERROR
    "IPC_INVALID_ERROR",    // IPC_INVALID_ERROR
    "IPC_GENERAL_ERROR",    // IPC_GENERAL_ERROR
    "IPC_SERVICE_DISABLED", // IPC_SERVICE_DISABLED
    "IPC_TIMEOUT",          // IPC_TIMEOUT
    "IPC_SHUT_DOWN",        // IPC_SHUT_DOWN
    NULL
};

/*
 * Message structure used to comprise the message.
 * Used as the payload when sending an IPC request
 * and as the container when reading an IPC reply.
 */
typedef struct _IPCMessage {
    int32_t   msgCode;      // the message code so the 'handler' knows how to interpret the request/response
    uint32_t  payloadLen;   // length of the payload.  if 0, the payload is ignored
    void      *payload;     // the JSON string that defines the message body
} IPCMessage;

/*
 * convenience function to create a new IPCMessage container.
 * needs to be freed when done (via freeIPCMessage)
 * @see freeIPCMessage()
 */
extern IPCMessage *createIPCMessage();

/*
 * convenience function to free the IPCMessage container
 */
extern void freeIPCMessage(IPCMessage *msg);

/*
 * convenience function to prime the IPCMessage container
 * with a JSON payload string.  Will allocate the 'payload'
 * (and set length) using the strlen of 'jsonStr'
 *
 * @param msg - the message to populate
 * @parem jsonStr - the payload to set within the msg
 */
extern void populateIPCMessageWithJSON(IPCMessage *msg, const char *jsonStr);

#endif // IPCMESSAGE_H
