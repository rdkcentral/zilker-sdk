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
 * IPCMessage.c
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

#include <stdlib.h>
#include <string.h>

#include <icIpc/ipcMessage.h>

#include "transport/transport.h"


/*
 * convenience function to create a new IPCMessage container.
 * needs to be freed when done (via freeIPCMessage)
 * @see freeIPCMessage()
 */
IPCMessage *createIPCMessage()
{
    // create and clear
    //
    IPCMessage *retVal = (IPCMessage *)malloc(sizeof(IPCMessage));
    memset(retVal, 0, sizeof(IPCMessage));
    return retVal;
}

/*
 * convenience function to free the IPCMessage container
 */
void freeIPCMessage(IPCMessage *msg)
{
    // only free if valid
    //
    if (msg != NULL)
    {
        // see if the payload needs to be released
        //
        if (msg->payload != NULL)
        {
            transport_free_msg(msg->payload);
            msg->payload = NULL;
        }

        // now free the container
        //
        free(msg);
    }
}

/*
 * convenience function to prime the IPCMessage container
 * with a JSON payload string.  Will allocate the 'payload'
 * using the strlen of 'jsonStr'
 *
 * @param msg - the message to populate
 * @parem jsonStr - the payload to set within the msg
 */
void populateIPCMessageWithJSON(IPCMessage *msg, const char *jsonStr)
{
    // set values appropriatly
    //
    if (jsonStr != NULL)
    {
        // first set length
        //
        size_t len = strlen(jsonStr);
        if (len > 0)
        {
            // add one to account for the NULL char
            //
            len++;
        }
        msg->payloadLen = (uint32_t)len;

        if (msg->payload != NULL)
        {
            // clear old mem in case this is being reused
            //
            transport_free_msg(msg->payload);
            msg->payload = NULL;
        }

        // apply string if applicable
        //
        if (len > 0)
        {
            // make the payload large enough for the string
            //
            msg->payload = transport_malloc_msg(sizeof(char) * len);
            memcpy(msg->payload, jsonStr, len);
        }
    }
}

