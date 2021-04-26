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
 * messageQueue.c
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
 * Author: jelderton - 8/27/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/errno.h>

#include <icLog/logging.h>
#include <icTypes/icQueue.h>
#include <icTypes/icHashMap.h>
#include <icUtil/stringUtils.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/threadUtils.h>
#include "channelManager.h"
#include "messageQueue.h"
#include "commServiceCommon.h"

#define SENT_HASH_KEY_SIZE  sizeof(uint64_t)

#define MSG_PROCESS_WAIT_INTERVAL_S 5

/*
 * possible states of the messageQueue
 */
typedef enum {
    MQ_NOT_RUNNING,
    MQ_RUNNING,
    MQ_PROCESSING,
    MQ_CANCELING
} mqState;

/*
 * definition of the messageQueue object
 */
struct _messageQueue {
    icQueue     *allSet;            // every message in the queue
    icQueue     *filterSet;         // copy of pointers in 'allSet' that match our 'filter'
    icHashMap   *sentHash;          // hash of msgId / message
    uint16_t    maxProcessedCount;  // max number of messages in the 'sentHash'
    uint32_t    messageTimeoutSecs; // number of seconds to use for message timeouts
    mqState     threadState;        // state of the thread

    pthread_mutex_t queueMtx;       // thread safety
    pthread_cond_t  queueCond;      // trigger cancel

    messageQueueDelegate *delegate; // delegate for filtering, processing, notifications, etc
};

/*
 * private functions
 */
static void *queueThread(void *arg);
static bool findMessageInQueue(void *searchVal, void *item);
static bool printMessageInQueue(void *item, void *arg);
static void doNotFreeMessageQueueFunc(void *item);
static bool iterateQueueAndApplyFilter(void *item, void *arg);
static void checkSentHashForTimeouts(messageQueue *queue);
static void waitOnProcessingLocked(messageQueue *queue);
static bool queueIsBusyLocked(messageQueue *queue);

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
                                 uint32_t messageTimeoutSecs)
{
    // sanity check
    //
    if (delegate == NULL)
    {
        icLogError(COMM_LOG, "queue: unable to create queue with missing 'delegate'");
        return NULL;
    }
    if (maxProcessingMessageCount == 0)
    {
        maxProcessingMessageCount = 1;
    }
    if (messageTimeoutSecs == 0)
    {
        messageTimeoutSecs = 1;
    }

    // allocate the mem
    //
    messageQueue *retVal = (messageQueue *)malloc(sizeof(messageQueue));
    memset(retVal, 0, sizeof(messageQueue));

    // create the queues & hash
    //
    retVal->allSet = queueCreate();
    retVal->filterSet = queueCreate();
    retVal->sentHash = hashMapCreate();

    // setup the mutex objects
    //
    pthread_mutex_init(&retVal->queueMtx, NULL);
    initTimedWaitCond(&retVal->queueCond);

    // save delegate and initial flags
    //
    retVal->delegate = delegate;
    retVal->threadState = MQ_NOT_RUNNING;
    retVal->maxProcessedCount = maxProcessingMessageCount;
    retVal->messageTimeoutSecs = messageTimeoutSecs;

    return retVal;
}

/*
 * destroys a message queue, including all messages currently
 * held by the queue.
 */
void messageQueueDestroy(messageQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    // stop the thread
    //
    messageQueueStopThread(queue, true);

    // clear memory
    //
    messageQueueClear(queue);
    pthread_mutex_lock(&queue->queueMtx);
    queueDestroy(queue->allSet, standardDoNotFreeFunc);
    queue->allSet = NULL;
    queueDestroy(queue->filterSet, standardDoNotFreeFunc);
    queue->filterSet = NULL;
    hashMapDestroy(queue->sentHash, standardDoNotFreeHashMapFunc);
    queue->sentHash = NULL;
    pthread_mutex_unlock(&queue->queueMtx);

    queue->delegate = NULL;     // not our memory to cleanup
    pthread_mutex_destroy(&queue->queueMtx);
    pthread_cond_destroy(&queue->queueCond);
    free(queue);
}

/*
 * start the processing thread for the queue.  only
 * has an effect if the thread is not running.
 * returns of the thread successfully started.
 */
bool messageQueueStartThread(messageQueue *queue)
{
    bool retVal = false;
    if (queue == NULL)
    {
        return retVal;
    }

    pthread_mutex_lock(&queue->queueMtx);
    if (queue->threadState == MQ_NOT_RUNNING)
    {
        // set flags so we know the thread is running
        //
        queue->threadState = MQ_RUNNING;

        // create the thread to process incoming messages
        //
        retVal = createDetachedThread(queueThread, queue, "messageQueue");
    }
    pthread_mutex_unlock(&queue->queueMtx);
    return retVal;
}


/*
 * check the state to see if it's MQ_NOT_RUNNING
 * will lock the mutex to make this get/check somwehat
 * of an atomic operation
 */
static bool isThreadDead(messageQueue *queue)
{
    bool retVal = false;

    if (queue != NULL)
    {
        pthread_mutex_lock(&queue->queueMtx);
        if (queue->threadState == MQ_NOT_RUNNING)
        {
            retVal = true;
        }
        pthread_mutex_unlock(&queue->queueMtx);
    }

    return retVal;
}

/*
 * check the state to see if it's MQ_CANCELING
 * will lock the mutex to make this get/check somwehat
 * of an atomic operation
 */
static bool isThreadCanceled(messageQueue *queue)
{
    bool retVal = false;

    if (queue != NULL)
    {
        pthread_mutex_lock(&queue->queueMtx);
        if (queue->threadState == MQ_CANCELING)
        {
            retVal = true;
        }
        pthread_mutex_unlock(&queue->queueMtx);
    }

    return retVal;
}

/*
 * halt the message queue thread.  if the flag is true,
 * this will block until the thread exits
 */
void messageQueueStopThread(messageQueue *queue, bool waitForExit)
{
    if (queue == NULL)
    {
        return;
    }

    // set the state to have the thread exit
    //
    pthread_mutex_lock(&queue->queueMtx);

    waitOnProcessingLocked(queue);

    if (queue->threadState == MQ_RUNNING)
    {
        // set then broadcast to stop the 'pause'
        //
        queue->threadState = MQ_CANCELING;
        pthread_cond_broadcast(&queue->queueCond);
    }
    else
    {
        // nothing to wait on, so clear the flag so we don't get stuck below
        //
        waitForExit = false;
    }
    pthread_mutex_unlock(&queue->queueMtx);

    // see if we hang around for the thread to die...
    //
    if (waitForExit == true)
    {
        while (isThreadDead(queue) == false)
        {
            // wait a second then check again
            //
            pthread_mutex_lock(&queue->queueMtx);
            incrementalCondTimedWait(&queue->queueCond, &queue->queueMtx, 1);
            pthread_mutex_unlock(&queue->queueMtx);
        }
    }
}

/*
 * appends the message object to the queue.
 * if this message successfully matches the 'filter', it will
 * be placed into the 'filter set' for processing by the thread.
 */
void messageQueueAppend(messageQueue *queue, message *msg)
{
    if (queue == NULL || msg == NULL)
    {
        return;
    }

    // append to the 'all set'
    //
    pthread_mutex_lock(&queue->queueMtx);
    queuePush(queue->allSet, msg);
    icLogTrace(COMM_LOG, "queue: added message id=%" PRIu64 ", total-queue count=%d", msg->messageId, queueCount(queue->allSet));

    // see if this message meets our current 'filter'.
    // if so, also add it to our filter queue.
    //
    if (queue->delegate->filterFunc(msg) == true)
    {
        queuePush(queue->filterSet, msg);
        icLogTrace(COMM_LOG, "queue: added message id=%" PRIu64 ", filtered-queue count=%d", msg->messageId, queueCount(queue->filterSet));

        // send notification so thread starts to
        // process the message (via filter queue)
        //
        if (queue->threadState == MQ_RUNNING)
        {
            pthread_cond_broadcast(&queue->queueCond);
        }
    }
    else
    {
        icLogTrace(COMM_LOG, "queue: skipped message id=%" PRIu64 ", filtered-queue count=%d", msg->messageId, queueCount(queue->filterSet));
    }
    pthread_mutex_unlock(&queue->queueMtx);
}

/*
 * implementation of 'queueCompareFunc' when removing a msg
 * via the 'queueDelete' function.
 */
static bool findMessageIdInQueue(void *searchVal, void *item)
{
    message *msg = (message *)item;
    uint64_t *messageId = (uint64_t *)searchVal;

    if (msg->messageId == *messageId)
    {
        return true;
    }
    return false;
}

/*
 * removes the message with this id from the queue (all 3 sets).
 * will destroy the message object (if located)
 */
void messageQueueRemove(messageQueue *queue, uint64_t messageId)
{
    if (queue == NULL)
    {
        return;
    }
    pthread_mutex_lock(&queue->queueMtx);

    // clear from filterSet (no free, it's a pointer)
    //
    if (queueDelete(queue->filterSet, &messageId, findMessageIdInQueue, doNotFreeMessageQueueFunc) == false)
    {
        icLogTrace(COMM_LOG, "%s: message not deleted from filter set", __FUNCTION__);
    }

    // find the message in the 'all' set
    //
    message *msg = queueFind(queue->allSet, &messageId, findMessageIdInQueue);
    if (msg != NULL)
    {
        // appears to be in 'all'.  remove from the queue (no free) then ask
        // the delegate to clear the mem
        //
        if (queueDelete(queue->allSet, &messageId, findMessageIdInQueue, doNotFreeMessageQueueFunc) == true)
        {
            // safe to release mem
            //

            // TODO: when processing is 'slow' this can waste time. Add ability to short circuit if messageId == currentMsgId
            //
            waitOnProcessingLocked(queue);
            queue->delegate->notifyFunc(msg, false, MESSAGE_FAIL_REASON_REMOVE);
        }
//      else
//      {
//          nothing to do here.  just noting that we are purposefully not destroying this
//          since it was not removed from the queue.
//      }
    }
    else
    {
        // was not in the 'all' set, so see if it's in the 'sent' hash
        //
        msg = (message *)hashMapGet(queue->sentHash, &messageId, SENT_HASH_KEY_SIZE);
        if (msg != NULL)
        {
            // remove from the sent hash, then ask the delegate to destroy the memory
            //
            if (hashMapDelete(queue->sentHash, &messageId, SENT_HASH_KEY_SIZE, standardDoNotFreeHashMapFunc) == true)
            {
                // safe to release mem
                //

                // TODO: when processing is 'slow' this can waste time. Add ability to short circuit if messageId == currentMsgId
                //
                waitOnProcessingLocked(queue);
                queue->delegate->notifyFunc(msg, false, MESSAGE_FAIL_REASON_REMOVE);

                // broadcast since this altered our 'sent set' count
                //
                if (queue->threadState == MQ_RUNNING)
                {
                    pthread_cond_broadcast(&queue->queueCond);
                }
            }
        }
    }
    pthread_mutex_unlock(&queue->queueMtx);
}

/*
 * clears and destroys all messages within the queue (all 3 sets).
 */
void messageQueueClear(messageQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }
    pthread_mutex_lock(&queue->queueMtx);

    waitOnProcessingLocked(queue);

    // wipe everything from filter (no free, these are pointers)
    //
    queueClear(queue->filterSet, doNotFreeMessageQueueFunc);
    message *msg = NULL;

    // iterate through all elements in the sent hash and forward to the delegate
    // with a reason of MESSAGE_FAIL_REASON_REMOVE.  once that's complete, we can
    // clear the hash (no free).
    //
    icHashMapIterator *hashLoop = hashMapIteratorCreate(queue->sentHash);
    while (hashMapIteratorHasNext(hashLoop) == true)
    {
        uint16_t keyLen;
        void* key;
        void* value;

        if (hashMapIteratorGetNext(hashLoop, &key, &keyLen, &value) == true)
        {
            // pass to delegate
            //
            msg = (message *) value;
            if (msg != NULL)
            {
                queue->delegate->notifyFunc(msg, false, MESSAGE_FAIL_REASON_REMOVE);
            }
        }
    }
    hashMapIteratorDestroy(hashLoop);
    hashMapClear(queue->sentHash, standardDoNotFreeHashMapFunc);

    // pull all elements from the all queue and forward to the delegate
    // with a reason of MESSAGE_FAIL_REASON_REMOVE.
    //
    while ((msg = queuePop(queue->allSet)) != NULL)
    {
        // pass to delegate for mem cleanup
        //
        queue->delegate->notifyFunc(msg, false, MESSAGE_FAIL_REASON_REMOVE);
    }

    pthread_mutex_unlock(&queue->queueMtx);
}

/*
 * iterator that loops though the 'all-set' or 'filter-set'
 * and runs the custom queueIterateFunc provided (with the arg).
 */
void messageQueueIterate(messageQueue *queue, messageQueueScope scope, queueIterateFunc iterFunc, void *arg)
{
    if (queue != NULL)
    {
        // loop through the requested 'set'
        //
        pthread_mutex_lock(&queue->queueMtx);
        if (scope == QUEUE_SCOPE_FILTER_SET)
        {
            queueIterate(queue->filterSet, iterFunc, arg);
        }
        else if (scope == QUEUE_SCOPE_ALL_SET)
        {
            queueIterate(queue->allSet, iterFunc, arg);
        }
        else if (scope == QUEUE_SCOPE_SENT_SET)
        {
            // loop through the sent hash map
            //
            icHashMapIterator *sentHashLoop = hashMapIteratorCreate(queue->sentHash);
            while (hashMapIteratorHasNext(sentHashLoop) == true)
            {
                uint16_t keyLen;
                void* key;
                void* value;
                if (hashMapIteratorGetNext(sentHashLoop, &key, &keyLen, &value) == true)
                {
                    message *msg = (message *) value;
                    if (msg != NULL)
                    {
                        // pass item to iterFunc
                        //
                        if (iterFunc(msg, arg) == false)
                        {
                            break;
                        }
                    }
                }
            }
            hashMapIteratorDestroy(sentHashLoop);
        }
        pthread_mutex_unlock(&queue->queueMtx);
    }
}

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
 */
message *messageQueueCompleted(messageQueue *queue, uint64_t messageId, void *payload)
{
    if (queue == NULL)
    {
        return NULL;
    }

    // locate this message from sent hash
    //
    pthread_mutex_lock(&queue->queueMtx);
    message *msg = (message *)hashMapGet(queue->sentHash, &messageId, SENT_HASH_KEY_SIZE);
    if (msg != NULL)
    {
        bool canDelete = true;
        if ((payload != NULL) && (msg->okToRemoveFromSentQueueCallback != NULL))
        {
            canDelete = msg->okToRemoveFromSentQueueCallback(msg, payload);
            icLogTrace(COMM_LOG,"%s: message specific check before removing from sent queue returned %s",__FUNCTION__,stringValueOfBool(canDelete));
        }
        // found a matching message.  remove from the hash so we can remove the lock
        // (but don't release the object yet)
        //
        if (canDelete)
        {
            hashMapDelete(queue->sentHash, &messageId, SENT_HASH_KEY_SIZE, standardDoNotFreeHashMapFunc);

            // send notification so our queue thread can send the next message (in case it was
            // waiting on the sent size to get below concurrentMessageCountMax)
            //
            if (queue->threadState == MQ_RUNNING)
            {
                pthread_cond_broadcast(&queue->queueCond);
            }
        }
        else
        {
            // we set the message return value to NULL, so that upstream from this call no additional
            // processing is done on the message.  We want it to stay in the sent queue and be naturally
            // re-sent after its timeout.
            icLogInfo(COMM_LOG,"queue: message prevented deletion from sent queue for message id %"PRIu64 ", ignoring response",msg->messageId);
            msg = NULL;
        }
    }
    else
    {
        icLogWarn(COMM_LOG, "queue: got response for unknown messageId %" PRIu64, messageId);
    }
    pthread_mutex_unlock(&queue->queueMtx);

    // return object from hash
    //
    return msg;
}

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
 */
message *messageQueueCompletedCustomSearch(messageQueue *queue, messageMatchSentSearchFunc searchFunc, void *arg)
{
    if (queue == NULL || searchFunc == NULL)
    {
        return NULL;
    }

    // would be nice to just use this searchFunc in our hashMap, but no such luck...
    // therefore will iterate through the 'sent set' and call the searchFunc for each
    //
    message *retVal = NULL;
    pthread_mutex_lock(&queue->queueMtx);
    icHashMapIterator *loop = hashMapIteratorCreate(queue->sentHash);
    while (hashMapIteratorHasNext(loop) == true)
    {
        void *mapKey;
        void *mapValue;
        uint16_t mapKeyLen = 0;
        message *next = NULL;

        // get the next message from the iterator
        //
        if (hashMapIteratorGetNext(loop, &mapKey, &mapKeyLen, &mapValue) == false)
        {
            // failed, so go to next
            //
            continue;
        }
        next = (message *) mapValue;

        // ask search function if this is our match
        //
        if (searchFunc(next, arg) == true)
        {
            // found a match, so remove from the iterator and bail
            //
            retVal = next;
            hashMapIteratorDeleteCurrent(loop, standardDoNotFreeHashMapFunc);
            break;
        }
    }

    // cleanup & return
    //
    hashMapIteratorDestroy(loop);
    pthread_mutex_unlock(&queue->queueMtx);
    return retVal;
}


/*
 * return the number of items in the 'all set'
 */
uint16_t messageQueueAllSetCount(messageQueue *queue)
{
    uint16_t retVal = 0;

    // get number of items in the 'all set'
    //
    if (queue != NULL)
    {
        pthread_mutex_lock(&queue->queueMtx);
        retVal = queueCount(queue->allSet);
        pthread_mutex_unlock(&queue->queueMtx);
    }

    return retVal;
}

/*
 * return the number of items in the 'filter set'
 */
uint16_t messageQueueFilterSetCount(messageQueue *queue)
{
    uint16_t retVal = 0;

    // get number of items in the 'filter set'
    //
    if (queue != NULL)
    {
        pthread_mutex_lock(&queue->queueMtx);
        retVal = queueCount(queue->filterSet);
        pthread_mutex_unlock(&queue->queueMtx);
    }

    return retVal;
}

/*
 * return the number of items in the 'sent set'
 */
uint16_t messageQueueSentSetCount(messageQueue *queue)
{
    uint16_t retVal = 0;

    // get number of items in the 'sent set'
    //
    if (queue != NULL)
    {
        pthread_mutex_lock(&queue->queueMtx);
        retVal = hashMapCount(queue->sentHash);
        pthread_mutex_unlock(&queue->queueMtx);
    }

    return retVal;
}

/*
 * return the number of concurrent 'processing' messages allowed
 */
uint16_t messageQueueGetMaxProcessingMessageCount(messageQueue *queue)
{
    uint16_t retVal = 0;

    if (queue != NULL)
    {
        pthread_mutex_lock(&queue->queueMtx);
        retVal = queue->maxProcessedCount;
        pthread_mutex_unlock(&queue->queueMtx);
    }

    return retVal;
}

/*
 * set the number of concurrent 'processing' messages allowed.
 * helps throttle the number of messages that are in-flight with
 * the server (prevent overloading the server)
 *
 * @param max - positive number ( >= 1 )
 */
void messageQueueSetMaxProcessingMessageCount(messageQueue *queue, uint16_t max)
{
    if (queue != NULL && max != 0)
    {
        pthread_mutex_lock(&queue->queueMtx);
        queue->maxProcessedCount = max;
        pthread_mutex_unlock(&queue->queueMtx);
    }
}

/*
 * return the current message timeout (in seconds)
 */
uint32_t messageQueueGetMessageTimeoutSecs(messageQueue *queue)
{
    uint32_t retVal = 0;
    if (queue != NULL)
    {
        pthread_mutex_lock(&queue->queueMtx);
        retVal = queue->messageTimeoutSecs;
        pthread_mutex_unlock(&queue->queueMtx);
    }

    return retVal;
}

/*
 * set the current message timeout value to use (in seconds)
 */
void messageQueueSetMessageTimeoutSecs(messageQueue *queue, uint32_t timeoutSecs)
{
    if (queue != NULL && timeoutSecs != 0)
    {
        pthread_mutex_lock(&queue->queueMtx);
        queue->messageTimeoutSecs = timeoutSecs;
        pthread_mutex_unlock(&queue->queueMtx);
    }
}

/*
 * re-create the 'filter set' by running the filter against
 * every message in the 'all set'.  generally called when the
 * conditions of the filter have chaned (ex: broadband change)
 */
void messageQueueRunFilter(messageQueue *queue)
{
    if (queue == NULL)
    {
        return;
    }
    pthread_mutex_lock(&queue->queueMtx);

    // empty the 'filter set'
    //
    icLogDebug(COMM_LOG, "queue: rebuilding message queue with new filter...");
    queueClear(queue->filterSet, doNotFreeMessageQueueFunc);

    // loop through all elements of our 'queue' and
    // add each that matches our new 'filter' into the filterQueue.
    // we'll utilize the 'iterator' functionality of the icQueue
    // for the looping and filtration tests.
    //
    queueIterate(queue->allSet, iterateQueueAndApplyFilter, queue);

    // send notification so thread starts to
    // process new messages (via filter queue)
    //
    if (queue->threadState == MQ_RUNNING)
    {
        pthread_cond_broadcast(&queue->queueCond);
    }
    pthread_mutex_unlock(&queue->queueMtx);
}

bool messageQueueIsBusy(messageQueue *queue)
{
    bool isBusy = false;
    if (queue == NULL)
    {
        return false;
    }

    pthread_mutex_lock(&queue->queueMtx);

    isBusy = queueIsBusyLocked(queue);

    pthread_mutex_unlock(&queue->queueMtx);

    return isBusy;
}

bool messageQueueContainsMessage(messageQueue *queue, message *msg)
{
    bool hasMsg = false;
    if (queue == NULL)
    {
        return hasMsg;
    }

    pthread_mutex_lock(&queue->queueMtx);

    /*
     * Let any message processing finish: msg may be current and
     * re-inserted into the all and filter sets.
     * Until then, msg, if current, is invisible to the queue and
     * in danger of being destroyed at the wrong time.
     */
    waitOnProcessingLocked(queue);

    hasMsg = hashMapContains(queue->sentHash, &msg->messageId, SENT_HASH_KEY_SIZE);

    if (hasMsg == false)
    {
        hasMsg = queueFind(queue->allSet, msg, findMessageInQueue) != NULL;
    }

    pthread_mutex_unlock(&queue->queueMtx);

    return hasMsg;
}

/*
 * queueIterateFunc implementation to see which items in 'all set'
 * should be added to the 'filter set'.  called via applyMessageFilter()
 * and assumes the QUEUE_MTX is locked.
 */
static bool iterateQueueAndApplyFilter(void *item, void *arg)
{
    message *msg = (message *)item;
    messageQueue *queue = (messageQueue *)arg;

    // add to the filtered queue if this message meets the filter
    //
    if (queue->delegate->filterFunc(msg) == true)
    {
        queuePush(queue->filterSet, msg);
    }

    // always return true so the iterator keeps going
    //
    return true;
}

/*
 * loop until told to cancel...processing messages in the queue as they arrive.
 */
static void *queueThread(void *arg)
{
    messageQueue *queue = (messageQueue *)arg;
    bool keepGoing = true;
    uint16_t count = 0;
    icLogInfo(COMM_LOG, "queue: start of messageQueue loop");

    uint64_t delayMillis = 0;
    while (keepGoing == true)
    {
        // see if told to cancel
        //
        pthread_mutex_lock(&queue->queueMtx);
        if (queue->threadState == MQ_CANCELING)
        {
            keepGoing = false;
            icLogInfo(COMM_LOG, "queue: canceling messageQueue...");
            pthread_mutex_unlock(&queue->queueMtx);
            continue;
        }

        // examine the number of outstanding messages (ones sent we're waiting
        // on a response from the server).  if we're at (or above) our "threshold"
        // then just wait for something to complete before moving forward.
        // similar to a fixed-size thread pool where we need to wait for an
        // available thread to continue...
        //
        count = hashMapCount(queue->sentHash);
        if (count >= queue->maxProcessedCount)
        {
            // wait for the queue size to change
            // or a response gets sent to us
            //
            icLogDebug(COMM_LOG, "queue: pausing msg processing for %" PRIu32 " seconds since 'sent list' has %" PRIu16 " items in it...", queue->messageTimeoutSecs, count);
            incrementalCondTimedWait(&queue->queueCond, &queue->queueMtx, (int)(queue->messageTimeoutSecs));
            // regardless of whether we timeout waiting or not, check our sent list so that if we are at max processed
            // count we eventually clear out expired messages.
            checkSentHashForTimeouts(queue);
            pthread_mutex_unlock(&queue->queueMtx);
            continue;
        }

        // see if there is a message in the queue to process
        //
        if (queueCount(queue->filterSet) == 0)
        {
            // wait up to 30 seconds for something to appear in the queue
            //
            int rc = incrementalCondTimedWait(&queue->queueCond, &queue->queueMtx, 30);
            if (rc == ETIMEDOUT)
            {
                // nothing to do for the last 30 seconds.  check to see
                // if any messages should be marked as "timed out"
                //
                checkSentHashForTimeouts(queue);
            }

#ifdef DEBUG_COMM_VERBOSE
            // show contents of the main queue (items not ever sent until filter changes)
            //
            icLogTrace(COMM_LOG, "queue: dumping full-queue <***** START *****>");
            icLogTrace(COMM_LOG, "queue: dumping full-queue count=%d", queueCount(queue->allSet));
            queueIterate(queue->allSet, printMessageInQueue, NULL);
            icLogTrace(COMM_LOG, "queue: dumping full-queue <***** END *****>");
#endif

            // loop back around to either process the newly added message - or wait again
            //
            pthread_mutex_unlock(&queue->queueMtx);
            continue;
        }

        // pull next message from the 'filter set'.  we're assuming the owner
        // has the filter properly configured such that this message is able
        // to be delivered.
        //
        delayMillis = 0;
        icLogDebug(COMM_LOG, "queue: safe to proceed with msg processing since 'sent list' has %" PRIu16 " items in it", count);
        message *msg = (message *)queuePop(queue->filterSet);
        if (msg != NULL)
        {
            // reflect the removal from 'filter' in the 'all' queue
            //
            if (queueDelete(queue->allSet, msg, findMessageInQueue, doNotFreeMessageQueueFunc) == false)
            {
                icLogError(COMM_LOG, "queue: message id=%" PRIu64 " was NOT removed from total-queue, meaning this will be duplicated to the server!!!!", msg->messageId);
            }

            if (msg->expectsReply == true)
            {
                // add message to 'sent hash' and start the 'send timeout'
                //
                if (!hashMapPut(queue->sentHash, &msg->messageId, SENT_HASH_KEY_SIZE, msg))
                {
                    icLogError(COMM_LOG,
                               "queue: duplicate message ID %" PRIu64 " at %s, considering it invalid!",
                               msg->messageId,
                               __FUNCTION__);
                    queue->delegate->notifyFunc(msg, false, MESSAGE_FAIL_REASON_INVALID);
                    pthread_mutex_unlock(&queue->queueMtx);
                    continue;
                }
                if (msg->tracker == NULL)
                {
                    msg->tracker = timeTrackerCreate();
                }
                timeTrackerStart(msg->tracker, queue->messageTimeoutSecs);
            }

            // Save this off so we can avoid using the message after we send as otherwise it might get free'd
            // from underneath us when the reply comes in
            bool expectsReply = msg->expectsReply;

            // now that the message has been deleted from both queues AND added to the sent hash,
            // we can release the lock to prevent holding it while the channel manager sends it
            // Note that this is still a critical section, and queueDelegate cannot be notified until after
            // processFunc completes.
            //
            queue->threadState = MQ_PROCESSING;
            pthread_mutex_unlock(&queue->queueMtx);

            // process the message
            //
            icLogTrace(COMM_LOG, "queue: dispatching msgId=%" PRIu64, msg->messageId);
            processMessageCode rc = queue->delegate->processFunc(msg);

            pthread_mutex_lock(&queue->queueMtx);

            if (queue->threadState == MQ_PROCESSING)
            {
                queue->threadState = MQ_RUNNING;
                pthread_cond_broadcast(&queue->queueCond);
            }
            else
            {
                icLogError(COMM_LOG, "queue state changed to [%d] during message processing ", queue->threadState);
            }

            // react to return code from channel manager
            //
            if (rc == PROCESS_MSG_SUCCESS)
            {
                // Handle success case
                if (expectsReply == false)
                {
                    // We won't get anything back, so notify of success so it gets cleaned up
                    queue->delegate->notifyFunc(msg, true, MESSAGE_FAIL_REASON_NONE);
                }
            }
            else if (rc == PROCESS_MSG_SUCCESS_HANDLED)
            {
                // handle success case and already handled on the server side
                //
                // nothing more to do, so remove from sent-hash
                //
                hashMapDelete(queue->sentHash, &msg->messageId, SENT_HASH_KEY_SIZE, standardDoNotFreeHashMapFunc);

                // now let the message get cleaned up
                //
                queue->delegate->notifyFunc(msg, true, MESSAGE_FAIL_REASON_NONE);
            }
            else
            {
                // stop timer (if set)
                //
                if (msg->tracker != NULL)
                {
                    timeTrackerStop(msg->tracker);
                }

                // message wan't sent, remove from sent-hash
                //
                if (msg->expectsReply == true)
                {
                    hashMapDelete(queue->sentHash, &msg->messageId, SENT_HASH_KEY_SIZE, standardDoNotFreeHashMapFunc);
                }

                if (rc == PROCESS_MSG_INVALID)
                {
                    // bad message, so send to delegate so it can decide what to do
                    // (release mem or add back to the queue for processing later).
                    //
                    icLogError(COMM_LOG, "queue: unable to process messageId=%" PRIu64 " (maybe failure in translation).  pitching message as it is BAD", msg->messageId);
                    queue->delegate->notifyFunc(msg, false, MESSAGE_FAIL_REASON_INVALID);
                    msg = NULL;
                }
                else  // DELAY_SEND or SEND_FAILURE
                {
                    if (rc == PROCESS_MSG_DELAY_SEND)
                    {
                        // sleep a few milliseconds to allow the CPU
                        // some cycles to process whatever we're dependent on.
                        // otherwise this tight-spins for a while
                        //
                        icLogWarn(COMM_LOG, "queue: unable to process messageId=%" PRIu64 ", as it depends on another message=%"PRIu64".  placing back into queue", msg->messageId, msg->requestId);
                        delayMillis = 250;
                    }
                    else
                    {
                        // rc is PROCESS_MSG_SEND_FAILURE.  before pushing back in the queue, see if this
                        // has hit (or exceeded) the max failure count.
                        //
                        msg->errorCount++;
                        if (msg->errorCount > msg->numRetries)
                        {
                            // send to the delegate for reporting and mem cleanup
                            //
                            icLogWarn(COMM_LOG, "queue: message %" PRIu64 " failed to send; NOT re-adding to queue since errors exceeds retries of %d", msg->messageId, msg->numRetries);
                            queue->delegate->notifyFunc(msg, false, MESSAGE_FAIL_REASON_RETRY_MAX);
                            msg = NULL;
                        }
                        else
                        {
                            icLogWarn(COMM_LOG, "queue: unable to process messageId=%" PRIu64 "; placing back into queue", msg->messageId);
                            delayMillis = 250;
                        }
                    }

                    if (msg != NULL)
                    {
                        // message wan't sent, so put back into queue
                        //
                        queuePush(queue->allSet, msg);

                        // if this message still meets the criteria, put it directly into the filtered queue
                        // so we can try to send it again.
                        //
                        if (queue->delegate->filterFunc(msg) == true)
                        {
                            queuePush(queue->filterSet, msg);
                        }
                    }
                }
            }
        }

        // if we need to pause before looping around, do that here
        // now that we've released the mutex lock
        //
        if (delayMillis > 0)
        {
            // quick check to see if told to cancel.  no point in waiting
            // if we should be on the way out of the thread
            //
            if (queue->threadState != MQ_CANCELING)
            {
                incrementalCondTimedWaitMillis(&queue->queueCond, &queue->queueMtx, delayMillis);
            }
        }
        pthread_mutex_unlock(&queue->queueMtx);
    }

    // update our state, then exit
    //
    pthread_mutex_lock(&queue->queueMtx);
    queue->threadState = MQ_NOT_RUNNING;
    pthread_cond_broadcast(&queue->queueCond);
    pthread_mutex_unlock(&queue->queueMtx);

    icLogInfo(COMM_LOG, "queue: end of messageQueue loop");
    return NULL;
}

/*
 * find and messages in the sentHash that have
 * been there too long.  each could be re-added
 * to the queue, or tossed out (based on the message)
 *
 * assumes QUEUE_MTX is locked
 */
static void checkSentHashForTimeouts(messageQueue *queue)
{
    if (hashMapCount(queue->sentHash) == 0)
    {
        return;
    }

    // internal call, assume the mutex is already held
    // try to be quick about it, but the painful truth is that
    // this has to iterate through everything in the hash
    //
    icLogTrace(COMM_LOG, "queue: checking sent list for expired messages!!!! count = %d", hashMapCount(queue->sentHash));
    icHashMapIterator *loop = hashMapIteratorCreate(queue->sentHash);
    while (hashMapIteratorHasNext(loop) == true)
    {
        void *mapKey;
        void *mapValue;
        uint16_t mapKeyLen = 0;
        message *next = NULL;

        // get the next message from the iterator
        //
        if (hashMapIteratorGetNext(loop, &mapKey, &mapKeyLen, &mapValue) == false)
        {
            // failed, so go to next
            //
            continue;
        }
        next = (message *) mapValue;

        // see if this expired
        //
        if (next->tracker != NULL && timeTrackerExpired(next->tracker) == true)
        {
            // increment the error count and tag for removal from the hash
            //
            next->errorCount++;
            // Make sure we know it has been sent before.  We do this lazily because there are timing issues with
            // trying to set this at the time we send.
            next->sentOnceFlag = true;
            timeTrackerStop(next->tracker);

            // see if we can try to resend this again
            //
            if (next->errorCount <= next->numRetries)
            {
                // can be resent, put back in queue
                //
                icLogInfo(COMM_LOG, "queue: message %" PRIu64 " expired waiting on reply, re-adding to queue. attempt %d of %d", next->messageId, next->errorCount, next->numRetries);
                queuePush(queue->allSet, next);

                // since this message still meets the criteria, put it directly into the filtered queue
                //
                if (queue->delegate->filterFunc(next) == true)
                {
                    queuePush(queue->filterSet, next);
                }

                // clear from sent set
                //
                hashMapIteratorDeleteCurrent(loop, standardDoNotFreeHashMapFunc);
            }
            else
            {
                // too many failures, need to pitch this message.  first, remove from our
                // sent hash (before delegate totally kills it)
                //
                hashMapIteratorDeleteCurrent(loop, standardDoNotFreeHashMapFunc);

                // send to the delegate for reporting and mem cleanup
                //
                icLogWarn(COMM_LOG, "queue: message %" PRIu64 " expired waiting on reply, NOT re-adding to queue since errors exceeds retries of %d", next->messageId, next->numRetries);
                queue->delegate->notifyFunc(next, false, MESSAGE_FAIL_REASON_RETRY_MAX);
            }
        }
        else
        {
            if (next->tracker == NULL)
            {
                // something horribly wrong.  we have a message in the sent hash, meaning it
                // expects a reply - and somehow it does not have a timeout mechanism.
                // log the error, then shitcan the object.  this used to be a problem that
                // was since-resolved...however keeping the check just to be safe.
                //
                icLogError(COMM_LOG, "queue: message id=%" PRIu64 " does not have a timer, but is stuck in the sent list! pitching as this message is probably corrupt!", next->messageId);
                queue->delegate->notifyFunc(next, false, MESSAGE_FAIL_REASON_INVALID);
                hashMapIteratorDeleteCurrent(loop, standardDoNotFreeHashMapFunc);
            }
            else
            {
                icLogTrace(COMM_LOG, "queue: message id=%" PRIu64 " has not expired yet while waiting for a reply.", next->messageId);
                timeTrackerDebug(next->tracker);
            }
        }
    }
    hashMapIteratorDestroy(loop);
}

/**
 * Check the queue for being in a busy state.
 * @param queue
 * @return
 * @note the queue->queueMtx must be locked before calling this.
 */
static bool queueIsBusyLocked(messageQueue *queue)
{
    return queue->threadState == MQ_PROCESSING;
}

/**
 * Wait for the queue to finish processing the current message
 * @note the queue->queueMtx must be locked before calling this.
 * @param queue
 */
static void waitOnProcessingLocked(messageQueue *queue)
{
    while (queueIsBusyLocked(queue) == true)
    {
        /* Signalled by queueThread after queue->delegate->processFunc(msg) returns */
        if (incrementalCondTimedWait(&queue->queueCond, &queue->queueMtx, MSG_PROCESS_WAIT_INTERVAL_S) == ETIMEDOUT)
        {
            icLogWarn(COMM_LOG, "Waiting for queue to finish processing a message");
        }
    }
}

/*
 * implementation of 'queueCompareFunc' when removing a msg
 * via the 'queueDelete' function.
 */
static bool findMessageInQueue(void *searchVal, void *item)
{
    // just do a pointer comparison
    //
    if (searchVal == item)
    {
        return true;
    }
    return false;
}

/*
 * called by queue during the "queueIterate()" function.
 * used for debugging, try to print as much as we know
 */
static bool printMessageInQueue(void *item, void *arg)
{
    // print as much as we can without doing something expensive
    // such as format the message and print to the log.
    // TODO: use the 'arg' someday to dicate something such as source queue, more details, etc
    //
    message *msg = (message *)item;
    icLogDebug(COMM_LOG,
               "queue-dump: message id=%" PRIu64 " mask=%x reply=%s sent=%s error=%d retries=%d timer_running=%d timer_ran_for=%d secs",
               msg->messageId,
               msg->deliveryMask,
               (msg->expectsReply == true) ? "true" : "false",
               (msg->sentOnceFlag == true) ? "true" : "false",
               msg->errorCount,
               msg->numRetries,
               (msg->tracker != NULL) ? timeTrackerRunning(msg->tracker) : 0,
               (msg->tracker != NULL) ? timeTrackerElapsedSeconds(msg->tracker) : 0);

    // don't cancel the loop
    //
    return true;
}

/*
 * implementation of 'queueItemFreeFunc' to not
 * actually free the data.  used internally when
 * clearing the 'filter queue'
 */
static void doNotFreeMessageQueueFunc(void *item)
{
    // do nothing.  the icQueue didn't supply a default
    // "do not free" function, so we needed one...
}





