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
 * sampleChMessage.h
 *
 * Extension of 'message' that is specific to the 'sample channel'.
 * Allows additional functinality such as "format the message payload"
 * to allow for multiple formats or protocols to the cloud.
 *
 * Should only be used via sampleChannel and stored within the
 * 'userData' section of the parent message object
 *
 * Author: jelderton - 4/14/21
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>

#include "sample/sampleChMessage.h"

// private functions
static void sampleMessageCleanup(message *msg);

/*
 * helper function to create a blank 'message', and
 * fill message->userData with a 'sampleMessage' object.
 * by default, will apply a function as the message->destroyUserDataFunc
 * to remove the userData, but can be replaced with a custom function;
 * just don't forget to call destroySampleMessage() from that custom function.
 *
 * @see destroySampleMessage()
 */
message *createSampleMessage(uint64_t id)
{
    // create the message container
    //
    message *retVal = createMessage(id);

    // apply default values
    //
    retVal->expectsReply = false;
    retVal->numRetries = DEFAULT_MAX_RETRIES;

    // now the 'simpleMessage', and assign to message->userData
    //
    sampleMessage *customMsg = calloc(1, sizeof(sampleMessage));
    retVal->userData = customMsg;

    // register default cleanup function
    //
    retVal->destroyUserDataFunc = sampleMessageCleanup;

    return retVal;
}

/*
 * helper function to free the memory associated with a sampleMessage object
 * (not the parent message container).  Intended to be called from a
 * 'messageDestroyFunc' implementation to release 'message->userData'.
 * assumes caller will release message->userData->messageData prior to calling
 * this function (which releases message->userData).
 */
void destroySampleMessage(sampleMessage *msg)
{
    if (msg != NULL)
    {
        // remove sampleMsgData, then the sampleMessage container (msg->userData)
        //
        free(msg->sampleMsgData);
        msg->sampleMsgData = NULL;
        free(msg);
    }
}

/*
 * extract the msg->userData and typecast to sampleMessage
 */
sampleMessage *extractSampleMessage(message *msg)
{
    return (sampleMessage *)msg->userData;
}

/*
 * default messageDestroyFunc implementation to auto-clean the userData
 * section of a message that was created via createSampleMessage()
 */
static void sampleMessageCleanup(message *msg)
{
    if (msg != NULL && msg->userData != NULL)
    {
        // cleanup the sampleMessage we added to the message->userData pointer
        //
        sampleMessage *sample = (sampleMessage *)msg->userData;
        destroySampleMessage(sample);
        msg->userData = NULL;
    }
}
