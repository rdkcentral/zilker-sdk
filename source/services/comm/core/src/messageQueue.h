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
 * messageQueue.h
 *
 * FIFO queue used to cache message objects while they wait
 * for delivery to the server.  Similar to the Java "EventQueue"
 * object from the legacy implementation, but with some improvements.
 *
 * Supports the concept of "message filtering" so that items
 * in the queue can be targeted for use or for caching.  Internally,
 * there are three object sets:
 *   1 - 'all set'    - all messages
 *   2 - 'filter set' - messages successfully matching the 'filter'
 *   3 - 'sent set'   - messages sent, waiting on a response
 *
 * Processing of objects in this queue will pull from the 'filter set',
 * allowing messages that are not currently applicable to remain in the
 * 'all set' until conditions change.
 * For example: a SMAP Channel has messages that require broadband.  The
 * SMAP filter would exclude those messages when broadband is down; leaving
 * them in the 'all set' until the network state changes.
 *
 * This increases the speed of message processing and reduces the churn
 * and logic utilized in the legacy implementation.
 *
 * Message lifecycle:
 * - created
 * - queued (via messageQueueAppend)
 * - processed (queueDelegate->processFunc)
 *    - Reinserted into queue for message->retries additional attempts (return to queued)
 * - Notified XOR Completed:
 *    - Notified on total failure (transmit errors exceed retries or async timeouts exceed retries)
 *    - Notified on 'handled' success (no async reply expected)
 *    - Completed when a thread calls messageQueueCompleted with a payload (async reply received)
 * - Notify and complete each end the message lifecycle; These are mutually exclusive paths and
 *   cannot occur at the same time. Queue users are responsible for destroying messages once their
 *   lifecycle ends.
 *
 * Author: jelderton - 8/27/15
 *-----------------------------------------------*/

#ifndef ZILKER_MESSAGEQUEUE_H
#define ZILKER_MESSAGEQUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <icTypes/icQueue.h>
#include "message.h"

/*
 * the messageQueue object representation.
 */
typedef struct _messageQueue messageQueue;

// return code for message processing (via the delegate)
typedef enum {
    PROCESS_MSG_SUCCESS = 0,        // success processing the message
    PROCESS_MSG_INVALID,            // invalid message
    PROCESS_MSG_SEND_FAILURE,       // message failed to process/send
    PROCESS_MSG_DELAY_SEND,         // need to delay the processing due to a message-dependency
    PROCESS_MSG_SUCCESS_HANDLED     // for message that were successful and nothing else is needed
} processMessageCode;

// possible failure reasons
typedef enum {
    MESSAGE_FAIL_REASON_NONE = 0,
    MESSAGE_FAIL_REASON_INVALID,
    MESSAGE_FAIL_REASON_SEND,
    MESSAGE_FAIL_REASON_TIMEOUT,    // timeout waiting on a reply
    MESSAGE_FAIL_REASON_RETRY_MAX,  // max retry count exceeded
    MESSAGE_FAIL_REASON_REMOVE      // used when the message was removed from the queue (shutdown and clear)
} messageFailureReason;

typedef enum {
    QUEUE_SCOPE_ALL_SET,
    QUEUE_SCOPE_FILTER_SET,
    QUEUE_SCOPE_SENT_SET
} messageQueueScope;

/*
 * function prototype used for message filtering.
 * called by the messageQueue to determine if a message
 * meets the logical filter.  if this returns 'true',
 * the message will be added to the 'filter set'
 *
 * @param msg - the 'message' being examined
 *
 * @note This function should not call any messageQueue functions.
 */
typedef bool (*messageMeetsFilterFunc)(message *msg);

/*
 * function prototype used when marking a message
 * complete, but searching through the sent list via
 * a different match other then messageId.
 *
 * @see messageQueueCompletedCustomSearch
 *
 * @note This function should not call any messageQueue functions.
 */
typedef bool (*messageMatchSentSearchFunc)(message *msg, void *arg);

/*
 * function prototype for the delegate to process the
 * message at the top of the 'filter set'.  should return
 * if the message delivery was successful so the
 * message can be placed into the 'sent set', delayed for
 * a retry, or marked as failure.
 *
 * @note This function should not call any messageQueue functions.
 */
typedef processMessageCode (*messageProcessesFunc)(message *msg);

/*
 * function prototype for the delegate to perform notification
 * of the message success/failure.  at this point the delegate
 * can also destroy the message object
 *
 * @note This function should not call any messageQueue functions.
 */
typedef void (*messageNotifyFunc)(message *msg, bool success, messageFailureReason reason);

/*
 * define the 'delegate'.  this object is responsible for:
 * - filtering messages from 'all' to 'filtered' sets
 * - processing messages (i.e. delivering to a server)
 * - notification of message success or failure
 * - memory cleanup of the message
 *
 * @note Do not call messageQueue functions from within these.
 */
typedef struct _messageQueueDelegate {
    messageMeetsFilterFunc  filterFunc;     // function to filter messages from 'all' to 'filtered' sets
    messageProcessesFunc    processFunc;    // processing messages (i.e. delivering to a server)
    messageNotifyFunc       notifyFunc;     // notification of message success/failure AND mem cleanup of message
} messageQueueDelegate;


/*
 * create a new instance of a messageQueue using the
 * supplied functions for message filtering and processing.
 * requires a subsequent call to 'messageQueueStartThread()'
 * before it can be utilized.
 *
 * @param delegate - the delegate for this queue to interact with
 * @param maxProcessingMessageCount - positive number of concurrent 'processing' messages allowed ( >= 1 )
 * @param messageTimeoutSecs - positive number for timeout value to use for message delivery ( >= 1 )
 * @see messageQueueStartThread()
 */
messageQueue *messageQueueCreate(messageQueueDelegate *delegate,
                                 uint16_t maxProcessingMessageCount,
                                 uint32_t messageTimeoutSecs);

/*
 * destroys a message queue, including all messages currently
 * held by the queue.
 */
void messageQueueDestroy(messageQueue *queue);

/*
 * start the processing thread for the queue.  only
 * has an effect if the thread is not running.
 * returns of the thread successfully started.
 */
bool messageQueueStartThread(messageQueue *queue);

/*
 * halt the message queue thread.  if the flag is true,
 * this will block until the thread exits
 */
void messageQueueStopThread(messageQueue *queue, bool waitForExit);

/*
 * appends the message object to the queue.
 * if this message successfully matches the 'filter', it will
 * be placed into the 'filter set' for processing by the thread.
 */
void messageQueueAppend(messageQueue *queue, message *msg);

/*
 * removes the message with this id from the queue (all 3 sets).
 * will destroy the message object (if located)
 */
void messageQueueRemove(messageQueue *queue, uint64_t messageId);

/*
 * clears and destroys all messages within the queue (all 3 sets).
 */
void messageQueueClear(messageQueue *queue);

/*
 * iterator that loops though the 'all-set' or 'filter-set'
 * and runs the custom queueIterateFunc provided (with the arg).
 */
void messageQueueIterate(messageQueue *queue, messageQueueScope scope, queueIterateFunc iterFunc, void *arg);

/*
 * notify the message queue that a processed message is
 * complete and can be removed from the 'sent set'
 *
 * returns the message located in the sent list.  up to
 * the caller to destroy the message (allows post processing
 * to occur without complicating the queue).
 *
 * assumes the caller is the delegate and will handle delivery
 * of the success/failure (as well as perform cleanup of the message)
 *
 * Note: Use messageQueueContainsMessage before destroying the message if your processFunc transmits the same message
 *       more than once.
 */
message *messageQueueCompleted(messageQueue *queue, uint64_t messageId, void *payload);

/*
 * notify the message queue that a processed message is
 * complete and can be removed from the 'sent set'.  Similar to
 * messageQueueCompleted, but uses a custom function for locating the
 * message object (vs searching with messageId).
 *
 * returns the message located in the sent list.  up to
 * the caller to destroy the message (allows post processing
 * to occur without complicating the queue).
 *
 * assumes the caller is the delegate and will handle delivery
 * of the success/failure (as well as perform cleanup of the message)
 *
 * Note: Use messageQueueContainsMessage before destroying the message if your processFunc transmits the same message
 *       more than once.
 */
message *messageQueueCompletedCustomSearch(messageQueue *queue, messageMatchSentSearchFunc searchFunc, void *arg);

/*
 * return the number of items in the 'all set'
 */
uint16_t messageQueueAllSetCount(messageQueue *queue);

/*
 * return the number of items in the 'filter set'
 */
uint16_t messageQueueFilterSetCount(messageQueue *queue);

/*
 * return the number of items in the 'sent set'
 */
uint16_t messageQueueSentSetCount(messageQueue *queue);

/*
 * return the number of concurrent 'processing' messages allowed
 */
uint16_t messageQueueGetMaxProcessingMessageCount(messageQueue *queue);

/*
 * set the number of concurrent 'processing' messages allowed.
 * helps throttle the number of messages that are in-flight with
 * the server (prevent overloading the server)
 *
 * @param max - positive number ( >= 1 )
 */
void messageQueueSetMaxProcessingMessageCount(messageQueue *queue, uint16_t max);

/*
 * return the current message timeout (in seconds)
 */
uint32_t messageQueueGetMessageTimeoutSecs(messageQueue *queue);

/*
 * set the current message timeout value to use (in seconds)
 */
void messageQueueSetMessageTimeoutSecs(messageQueue *queue, uint32_t timeoutSecs);

/*
 * re-create the 'filter set' by running the filter against
 * every message in the 'all set'.  generally called when the
 * conditions of the filter have chaned (ex: broadband change)
 */
void messageQueueRunFilter(messageQueue *queue);

/**
 * Determine if the queue is actively processing a message.
 * @param queue A valid message queue
 * @return
 */
bool messageQueueIsBusy(messageQueue *queue);

/**
 * Determine if a given message object exists in the queue.
 * This will search the sent and 'all' hash sets for the message.
 * @return whether the message is in the queue
 */
bool messageQueueContainsMessage(messageQueue *queue, message *msg);

#endif // ZILKER_MESSAGEQUEUE_H

