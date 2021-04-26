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
 * message.h
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

#ifndef ZILKER_MESSAGE_H
#define ZILKER_MESSAGE_H

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include <icTime/timeTracker.h>

#define NO_MAX_RETRIES_LIMIT 99      // use as 'numRetries' on a message that should never be pitched due to too many errors (i.e. alarms)
#define NO_RETRIES           0       // no retries at all; a fire and forget style message
#define DEFAULT_MAX_RETRIES  3       // set by default as part of createMessage()

/*
 * Custom mask type so we can update in the future
 * if necessary.
 * @note use MASK_PRI or MASK_SCN for format conversions.
 */
typedef uint32_t mask_t;

#define MASK_PRI PRIu32
#define MASK_SCN SCNu32

/*
 * forward declare the object type
 */
typedef struct _message message;

/*
 * optional callback function to invoke after the message has been declared a success or failure.
 *
 * if set as the 'successCallback', called when successfully sent and received as-well-as parsed
 * the response.
 *
 * if set as the 'failureCallback', called when the message 'errorCount' >= 'numRetries' (meaning
 * the message failed to get a response too many times) OR if the 'processMessageResponseFunc'
 * returns false; passing along the parsed output.
 */
typedef void (*messageResponseCallback)(message *msg);

/*
 * Optional function to run when destroying the message
 * so that the 'userData' variable can properly be released.
 */
typedef void (*messageDestroyFunc)(message *msg);

/*
 * options callback function after a response has been received to determine if the message can
 * be removed from the sent queue or not.
 *
 * if set, and returns true, the message can be removed from the sent queue.  If set and
 * returns false, the message should stay in the sent queue.  If it is not set, then
 * the message is handled normally (i.e. removed from the queue).
 */
typedef bool (*msgCanBeRemovedFromSentQueue)(message *msg, void *payload);

/*
 * object representation of messages.
 * added to the 'messageQueue'.  each should define
 * how to encode the message into one or more of the
 * supported formats. (ex: SMAP, CSMAP, REST, etc)
 */
struct _message
{
    // unique identifier for the message that can
    // be used for correlating responses.  generally
    // equal to the eventId
    //
    uint64_t messageId;

    // if non-zero, represents the id of the server request this message is
    // associated with.  most instances will not have this set.
    //
    uint64_t requestId;

    // optional bit-mask of all supported formats and/or network interfaces this
    // message can use during delivery.  specific to the channel this message will be
    // utilized in, which is why this is very opaque.
    //
    mask_t deliveryMask;

    // optional callback when the message is done (success or fail).
    // used by the messageQueue delegate (object supplying the messageProcesses function)
    //
    messageResponseCallback successCallback;
    messageResponseCallback failureCallback;

    // optional callback when the message response has been received
    msgCanBeRemovedFromSentQueue okToRemoveFromSentQueueCallback;

    // generic "user data" - i.e. object this message is representing.
    //
    void *userData;

    // function to call when freeing the message
    // so that 'userData' can be properly released
    //
    messageDestroyFunc destroyUserDataFunc;

    // track duration of 'sent and waiting for reply'.  primarily used
    // to determined if the message timed out.  applicable when
    // 'expectsReply' is set to true.
    //
    timeTracker *tracker;

    // flags updated while this message is sitting in the queue
    //
    bool     expectsReply;      // true if this message should wait for a response from the server
    bool     sentOnceFlag;      // true if this message was sent at least one time to the server
    uint16_t errorCount;        // number of times this was sent, but did not receive a response
    uint16_t numRetries;        // number of times this should be attempted before giving up.
                                // use NO_MAX_RETRIES to remove the limitation
};

/*
 * helper function to create and clear a message object
 */
message *createMessage(uint64_t id);

/*
 * helper function to free the memory associated with a message object
 */
void destroyMessage(message *msg);

#endif // ZILKER_MESSAGE_H

