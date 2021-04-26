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

#ifndef ZILKER_SDK_SAMPLECHMESSAGE_H
#define ZILKER_SDK_SAMPLECHMESSAGE_H

#include <icTypes/icStringBuffer.h>
#include "message.h"

typedef enum {
    PAYLOAD_FORMAT_XML,
    PAYLOAD_FORMAT_JSON,
} samplePayloadFormat;

/*
 * function prototype to create the payload for delivery.  for simplicity,
 * the formatting is a stringBuffer.
 *
 * @param msg - the 'message' being sent
 * @param format - the format to translate the message into
 * @return formatted message adhering to 'format'
 */
typedef icStringBuffer *(*formatSampleMessageFunc)(message *msg, samplePayloadFormat format);

/*
 * extend the 'message' object by adding additional
 * functions/metadata about smap/csmap specific messages.
 * this will be stored in message->userData.
 */
typedef struct
{
    // function to call to 'encode' the message into the required format during delivery
    //
    formatSampleMessageFunc encodeMessageFunc;

    // pointer to the event or request this message is intended for.
    // should be released via the message->destroyUserDataFunc call
    //
    void *sampleMsgData;
} sampleMessage;

/*
 * helper function to create a blank 'message', and
 * fill message->userData with a 'sampleMessage' object.
 * by default, will apply a function as the message->destroyUserDataFunc
 * to remove the userData, but can be replaced with a custom function;
 * just don't forget to call destroySampleMessage() from that custom function.
 *
 * @see destroySampleMessage()
 */
message *createSampleMessage(uint64_t id);

/*
 * helper function to free the memory associated with a sampleMessage object
 * (not the parent message container).  Intended to be called from a
 * 'messageDestroyFunc' implementation to release 'message->userData'.
 * assumes caller will release message->userData->sampleMsgData prior to calling
 * this function (which releases message->userData).
 */
void destroySampleMessage(sampleMessage *msg);

/*
 * extract the msg->userData and typecast to sampleMessage
 */
sampleMessage *extractSampleMessage(message *msg);

#endif //ZILKER_SDK_SAMPLECHMESSAGE_H
