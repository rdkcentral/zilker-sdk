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

#include <string.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>
#include <time.h>
#include <pthread.h>

#include <icLog/logging.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/threadUtils.h>
#include "messageQueue.h"
#include "message.h"

#define LOG_CAT "messageQueueTest"

static pthread_mutex_t completeMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t completeCond;

static pthread_cond_t invertedProcessCond;
static pthread_mutex_t invertedProcessMtx = PTHREAD_MUTEX_INITIALIZER;

static bool messageMeetsFilter(message *msg)
{
    return true;
}

static processMessageCode messageProcess(message *msg)
{
    icLogDebug(LOG_CAT, "fake processing message %"PRIu64, msg->messageId);

    pthread_mutex_lock(&invertedProcessMtx);

    pthread_cond_signal(&invertedProcessCond);

    incrementalCondTimedWait(&invertedProcessCond, &invertedProcessMtx, 1);

    pthread_mutex_unlock(&invertedProcessMtx);

    processMessageCode msgCode = (processMessageCode) msg->userData;

    icLogDebug(LOG_CAT, "fake processing message %"PRIu64 " completed with code [%d]", msg->messageId, msgCode);

    return msgCode;
}

static void messageNotify(message *msg, bool success, messageFailureReason reason)
{
    icLogDebug(LOG_CAT, "message %"PRIu64 " disposition code: [%d]", msg->messageId, reason);

    destroyMessage(msg);
}

static messageQueueDelegate testDelegate = {
        .filterFunc = messageMeetsFilter,
        .processFunc = messageProcess,
        .notifyFunc = messageNotify
};

static messageQueue *testQueue;

/**
 * Ensure that messageQueueClear cannot destroy an in-use message (via notifyFunc) before processFunc can finish
 * (use after free)
 * @param state
 */
void test_queue_clear_during_process(void **state)
{
    message *msg = createMessage(1);

    msg->expectsReply = true;
    msg->numRetries = 0;

    /* Faking a send failure will guarantee msg is referred to after notify potentially frees it */
    msg->userData = (void *) PROCESS_MSG_SEND_FAILURE;

    pthread_mutex_lock(&invertedProcessMtx);

    messageQueueAppend(testQueue, msg);
    incrementalCondTimedWait(&invertedProcessCond, &invertedProcessMtx, 1);
    pthread_mutex_unlock(&invertedProcessMtx);

    assert_true(messageQueueIsBusy(testQueue));

    messageQueueClear(testQueue);

    pthread_mutex_lock(&invertedProcessMtx);
    pthread_cond_signal(&invertedProcessCond);
    pthread_mutex_unlock(&invertedProcessMtx);

    assert_false(messageQueueIsBusy(testQueue));
    /* Nothing to assert; sanitizer will detect use-after-free */
}

void *simulateReplyThread(message *msg)
{
    message *done = messageQueueCompleted(testQueue, msg->messageId, "");

    assert_non_null(done);

    /*
     * This thread was started just in time to grab the message from sentHash
     * just before processing finished and the queue can see the failure.
     * This simulates a race between async replies and the queue's internal
     * split critical sections.
     */
    pthread_mutex_lock(&completeMtx);
    pthread_cond_signal(&completeCond);
    pthread_mutex_unlock(&completeMtx);
    /* Message reply is typically processed here (decoding, etc) */

    icLogDebug(LOG_CAT, "message %d reply received", done->messageId);
    if (messageQueueContainsMessage(testQueue, done) == false)
    {
        icLogDebug(LOG_CAT, "Done with message %d, destroying it", done->messageId);
        destroyMessage(done);
    }
    else
    {
        icLogDebug(LOG_CAT, "message %d is still in queue, not destroying it", done->messageId);
    }

    return NULL;
}

/**
 * Test that a message failure that races with an async reply can't
 * destroy a message before its life is over
 * @param state not used
 */
void test_queue_busy_complete(void **state)
{
    message *msg = createMessage(2);
    msg->expectsReply = true;
    msg->numRetries = 2;
    msg->userData = (void *) PROCESS_MSG_SEND_FAILURE;
    pthread_t tid;

    /*
     * Try to process a doomed message while receiving a reply for a simulated previously successful transmission.
     * The failed message will remain in-queue and must not be destroyed.
     * This simulates a successful SMAP transmission and reply with a concurrent failed csmap transmission.
     */
    pthread_mutex_lock(&invertedProcessMtx);
    messageQueueAppend(testQueue, msg);
    incrementalCondTimedWait(&invertedProcessCond, &invertedProcessMtx, 1);
    pthread_cond_signal(&invertedProcessCond);

    incrementalCondTimedWait(&invertedProcessCond, &invertedProcessMtx, 1);

    /*
     * Message has failed and is now back in the queue at queueDelegate->processFunc()
     * Receive a reply to test that msg, which is in use, is not released
     */
    pthread_mutex_lock(&completeMtx);
    createThread(&tid, (taskFunc) simulateReplyThread, msg, "simReply");
    incrementalCondTimedWait(&completeCond, &completeMtx, 1);
    pthread_mutex_unlock(&completeMtx);

    /* Now let processing fail once more to test that the queue reports it contains msg */
    pthread_cond_signal(&invertedProcessCond);
    pthread_mutex_unlock(&invertedProcessMtx);

    pthread_join(tid, NULL);

    pthread_mutex_lock(&invertedProcessMtx);
    incrementalCondTimedWait(&invertedProcessCond, &invertedProcessMtx, 1);
    msg->userData = (void *) PROCESS_MSG_SUCCESS;
    pthread_cond_signal(&invertedProcessCond);
    pthread_mutex_unlock(&invertedProcessMtx);

    simulateReplyThread(msg);

    assert_int_equal(messageQueueAllSetCount(testQueue), 0);
}

int setup(void **state)
{
    messageQueueClear(testQueue);
    messageQueueStartThread(testQueue);

    return 0;
}

int teardown(void **state)
{
    messageQueueClear(testQueue);
    messageQueueStopThread(testQueue, true);

    return  0;
}

int main(int argc, const char ** argv)
{
    initIcLogger();

    initTimedWaitCond(&invertedProcessCond);
    initTimedWaitCond(&completeCond);
    testQueue = messageQueueCreate(&testDelegate, 1, 1);  // one message at a time

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test_setup_teardown(test_queue_clear_during_process, setup, teardown),
                    cmocka_unit_test_setup_teardown(test_queue_busy_complete, setup, teardown)
            };

    int rc = cmocka_run_group_tests(tests, NULL, NULL);
    closeIcLogger();

    messageQueueDestroy(testQueue);

    return rc;
}
