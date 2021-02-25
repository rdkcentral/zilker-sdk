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

//
// Created by mkoch201 on 11/15/18.
//

#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "icConcurrent/icBlockingQueue.h"
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <pthread.h>
#include <icTypes/sbrm.h>

#define LOG_TAG "blockingQueueTest"

static void test_canCreate(void **state)
{
    icBlockingQueue *queue = blockingQueueCreate(10);
    assert_non_null(queue);
    blockingQueueDestroy(queue, NULL);
}

static void blockingQueueDoNotFreeFunc(void *item)
{

}

static void test_canPushAndPop(void **state)
{
    icBlockingQueue *queue = blockingQueueCreate(10);
    assert_int_equal(blockingQueueCount(queue), 0);
    assert_true(blockingQueuePush(queue, "a"));
    assert_int_equal(blockingQueueCount(queue), 1);
    char *res = (char *)blockingQueuePop(queue);
    assert_int_equal(blockingQueueCount(queue), 0);
    assert_string_equal(res, "a");
    blockingQueueDestroy(queue, blockingQueueDoNotFreeFunc);
}

static void test_canClear(void **state)
{
    icBlockingQueue *queue = blockingQueueCreate(10);
    assert_int_equal(blockingQueueCount(queue), 0);
    blockingQueuePush(queue, "a");
    blockingQueuePush(queue, "b");
    blockingQueuePush(queue, "c");
    assert_int_equal(blockingQueueCount(queue), 3);
    blockingQueueClear(queue, blockingQueueDoNotFreeFunc);
    assert_int_equal(blockingQueueCount(queue), 0);
    blockingQueueDestroy(queue, blockingQueueDoNotFreeFunc);
}


static bool iterateFunc(void *item, void *arg)
{
    int *iterateCount = (int *)arg;
    *iterateCount = *iterateCount + 1;
    if (strcmp(item,"b") == 0)
    {
        return true;
    }
    return false;
}

static void test_canIterate(void **state)
{
    int iterateCount = 0;
    icBlockingQueue *queue = blockingQueueCreate(10);
    blockingQueuePush(queue, "a");
    blockingQueuePush(queue, "b");
    blockingQueuePush(queue, "c");
    blockingQueueIterate(queue, iterateFunc, &iterateCount);
    assert_int_equal(iterateCount, 1);

    blockingQueueDestroy(queue, blockingQueueDoNotFreeFunc);
}

static bool searchFunc(void *searchVal, void *item)
{
    char *searchValCh = (char *)searchVal;
    char *itemCh = (char *)item;
    return strcmp(searchValCh, itemCh) == 0;
}

static void test_canDelete(void **state)
{
    icBlockingQueue *queue = blockingQueueCreate(10);
    blockingQueuePush(queue, "a");
    blockingQueuePush(queue, "b");
    blockingQueuePush(queue, "c");
    blockingQueueDelete(queue, "b", searchFunc, blockingQueueDoNotFreeFunc);
    assert_int_equal(blockingQueueCount(queue), 2);
    blockingQueueDestroy(queue, blockingQueueDoNotFreeFunc);
}

typedef struct {
   icBlockingQueue *queue;
   char *arg;
   int *waitCount;
   int totalWaitCount;
} TaskArg;

static TaskArg *taskArgs;
static icBlockingQueue *taskQueue;
static pthread_t *taskThreads;
static int taskWaitCount;

static pthread_mutex_t mutex;
static pthread_cond_t cond;

static void *pushTask(void *arg)
{
    TaskArg *taskArg = (TaskArg*)arg;
    pthread_mutex_lock(&mutex);
    int *waitCount = taskArg->waitCount;
    *waitCount += 1;
    if (*waitCount == taskArg->totalWaitCount)
    {
        pthread_cond_broadcast(&cond);
    }
    else
    {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    icLogDebug(LOG_TAG, "Pushing %s", taskArg->arg);

    bool *isSuccess = (bool *)malloc(sizeof(bool));
    errno = 0;
    *isSuccess = blockingQueuePushTimeout(taskArg->queue, taskArg->arg, 10);
    if (*isSuccess == false)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "Failed pushing %s with errno %s", taskArg->arg, errStr);
    }
    else
    {
        icLogDebug(LOG_TAG, "Finished pushing %s", taskArg->arg);
    }
    return isSuccess;
}

#define CONCURRENT_COUNT 1000

static void *popTask(void *arg)
{
    TaskArg *taskArg = (TaskArg*)arg;
    pthread_mutex_lock(&mutex);
    int *waitCount = taskArg->waitCount;
    *waitCount += 1;
    if (*waitCount == taskArg->totalWaitCount)
    {
        pthread_cond_broadcast(&cond);
    }
    else
    {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);

    icLogDebug(LOG_TAG, "Popping");

    bool *isSuccess = (bool *)malloc(sizeof(bool));
    errno = 0;
    char *res = (char *)blockingQueuePopTimeout(taskArg->queue, 10);
    if (res == NULL)
    {
        *isSuccess = false;
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogError(LOG_TAG, "Failed popping with errno %s", errStr);
    }
    else
    {
        *isSuccess = true;
        icLogDebug(LOG_TAG, "Finished popping %s", taskArg->arg);
    }
    return isSuccess;
}

static int createQueueTasks(void **state, void *taskFunc(void *))
{
    taskQueue = blockingQueueCreate(1);
    taskArgs = calloc(CONCURRENT_COUNT, sizeof(TaskArg));
    taskThreads = calloc(CONCURRENT_COUNT, sizeof(pthread_t));
    taskWaitCount = 0;

    for(int i = 0; i < CONCURRENT_COUNT; ++i)
    {
        taskArgs[i].queue = taskQueue;
        taskArgs[i].arg = stringBuilder("%d", i);
        taskArgs[i].waitCount = &taskWaitCount;
        taskArgs[i].totalWaitCount = CONCURRENT_COUNT;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
        pthread_create(&taskThreads[i], &attr, taskFunc, &taskArgs[i]);
        pthread_attr_destroy(&attr);
    }

    return 0;
}

static int setup_popWaitsForPush(void **state)
{
    return createQueueTasks(state, popTask);
}

static int setup_pushWaitsForPop(void **state)
{
    return createQueueTasks(state, pushTask);
}

static int teardown_queueTasks(void **state)
{
    bool *success;
    bool allWorked = true;

    for (int i = 0; i < CONCURRENT_COUNT; ++i)
    {
        pthread_join(taskThreads[i], (void **) &success);

        if (success != NULL && success != PTHREAD_CANCELED)
        {
            allWorked &= *success;
            free(success);
        }

        free(taskArgs[i].arg);
    }

    blockingQueueDestroy(taskQueue, blockingQueueDoNotFreeFunc);
    taskQueue = NULL;
    free(taskArgs);
    taskArgs = NULL;
    free(taskThreads);
    taskThreads = NULL;

    assert_true(allWorked);

    return 0;
}

static void test_popWaitsForPush(void **state)
{
    for(int i = 0; i < CONCURRENT_COUNT; ++i)
    {
        assert_true(blockingQueuePushTimeout(taskQueue, taskArgs[i].arg, 10));
        icLogDebug(LOG_TAG, "Pushed %s", taskArgs[i].arg);
    }
}

static void test_pushWaitsForPop(void **state)
{
    for(int i = 0; i < CONCURRENT_COUNT; ++i)
    {
        char *res = blockingQueuePopTimeout(taskQueue, 10);
        if (res != NULL)
        {
            icLogDebug(LOG_TAG, "Popped %s", res);
        }
        assert_non_null(res);
    }
}

static void test_zeroTimeout(void **state)
{
    icBlockingQueue *queue = blockingQueueCreate(1);
    assert_true(blockingQueuePush(queue, "1"));
    assert_false(blockingQueuePushTimeout(queue, "2", 0));
    assert_int_equal(errno, ETIMEDOUT);
    blockingQueueDestroy(queue, blockingQueueDoNotFreeFunc);
}

/*
 * main
 */
int main(int argc, const char ** argv)
{
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_ERROR);

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_canCreate),
                    cmocka_unit_test(test_canPushAndPop),
                    cmocka_unit_test(test_canClear),
                    cmocka_unit_test_setup_teardown(test_pushWaitsForPop, setup_pushWaitsForPop, teardown_queueTasks),
                    cmocka_unit_test_setup_teardown(test_popWaitsForPush, setup_popWaitsForPush, teardown_queueTasks),
                    cmocka_unit_test(test_canIterate),
                    cmocka_unit_test(test_canDelete),
                    cmocka_unit_test(test_zeroTimeout)
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    closeIcLogger();

    return retval;
}
