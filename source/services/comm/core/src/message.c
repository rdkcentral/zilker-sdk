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
 * message.c
 *
 * Define the base construct of information that can be sent/received
 * from an an iControl/Comcast Server.  The nature of messages are a
 * "command and response" methodology where the 'command' (or notification)
 * is sent to the server, and some form of a 'response' is expected.
 * The smallest response is a simple 'ack' to let us know the message was received.
 *
 * Serves as the basis for interaction with the servers, but intended to be
 * utilized within other structures such as messageQueue and channel.
 *
 * Author: jelderton - 8/27/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include "message.h"


/*
 * helper function to create and clear a message object
 */
message *createMessage(uint64_t id)
{
    // alloc then apply default values
    //
    message *retVal = (message *)calloc(1, sizeof(message));
    retVal->messageId = id;
    retVal->expectsReply = false;
    retVal->numRetries = DEFAULT_MAX_RETRIES;

    return retVal;
}

/*
 * helper function to free the memory associated with a message object
 */
void destroyMessage(message *msg)
{
    if (msg != NULL)
    {
        // release time tracker (if used)
        //
        if (msg->tracker != NULL)
        {
            timeTrackerDestroy(msg->tracker);
            msg->tracker = NULL;
        }

        // call the helper function (if defined) to release userData
        //
        if (msg->userData != NULL && msg->destroyUserDataFunc != NULL)
        {
            msg->destroyUserDataFunc(msg);
            msg->userData = NULL;
        }

        // now free the wrapper
        //
        free(msg);
    }
}



