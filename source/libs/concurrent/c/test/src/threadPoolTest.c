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
 * threadPoolTest.c
 *
 * Unit test the icThreadPool object
 *
 * Author: jelderton - 2/12/16
 *-----------------------------------------------*/


// cmocka & it's dependencies
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <icLog/logging.h>
#include <icConcurrent/threadPool.h>
#include <icConcurrent/icBlockingQueue.h>
#include <pthread.h>
#include <icConcurrent/timedWait.h>
#include <inttypes.h>

#define LOG_CAT     "poolTEST"

/*
 * test to make sure the pool creation doesn't allow dump values
 */
static void test_doesPreventStupidity(void **state)
{
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = NULL;

    // create a pool with crazy values
    // and make sure we fail
    //
    pool = threadPoolCreate("test", 100,99,250);
    assert_true(pool == NULL);

    // cleanup
    threadPoolDestroy(pool);
}

typedef struct {
    char *str;
    icBlockingQueue *resultQueue;
} TaskArg;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static bool awoken = false;
static int counter = 0;

/*
 * simple task to push an item into a queue
 */
static void simpleTask(void *arg)
{
    TaskArg *taskArg = (TaskArg *)arg;
    pthread_mutex_lock(&mutex);
    ++counter;
    icLogDebug(LOG_CAT, "simple thread pool task '%s', incremented counter to %d", (taskArg != NULL) ? taskArg->str : "null", counter);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
    icLogDebug(LOG_CAT, "simple thread pool task '%s', pushing", (taskArg != NULL) ? taskArg->str : "null");
    if (blockingQueuePushTimeout(taskArg->resultQueue, taskArg->str, 10))
    {
        icLogDebug(LOG_CAT, "simple thread pool task '%s', pushing done", (taskArg != NULL) ? taskArg->str : "null");
    }
    else
    {
        icLogError(LOG_CAT, "simple thread pool task '%s', pushing failed!", (taskArg != NULL) ? taskArg->str : "null");
    }
}

static void doNotDestroyQueueItemFunc(void *item)
{

}

/*
 * simple test to create a pool, add some tasks, and tear it down
 */
static void test_canRunJobs(void **state)
{
    icBlockingQueue *blockingQueue = blockingQueueCreate(2);
    // make a small pool (3) and add 2 jobs
    // to make sure they run.
    //
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = threadPoolCreate("test", 3, 5, 10);

    // add 2 jobs (no arguments)
    TaskArg taskArgA = { .str = "a", .resultQueue = blockingQueue };
    TaskArg taskArgB = { .str = "b", .resultQueue = blockingQueue };
    threadPoolAddTask(pool, simpleTask, &taskArgA, threadPoolTaskArgDoNotFreeFunc);
    threadPoolAddTask(pool, simpleTask, &taskArgB, threadPoolTaskArgDoNotFreeFunc);

    // wait for the jobs to put their data in
    icLogDebug(LOG_CAT, "...waiting to allow threads to execute");
    blockingQueuePopTimeout(blockingQueue, 10);
    blockingQueuePopTimeout(blockingQueue, 10);

    // check that the pool size is 0
    //
    uint32_t size = threadPoolGetBacklogCount(pool);
    icLogDebug(LOG_CAT, "pool size is %d", size);
    assert_true(size == 0);

    // destroy the pool
    threadPoolDestroy(pool);

    // destory the queue
    blockingQueueDestroy(blockingQueue, doNotDestroyQueueItemFunc);
}

/*
 * test to create a pool, add overload with tasks to ensure
 * the pool properly queue's the tasks
 */
static void test_canQueueJobs(void **state)
{
    icBlockingQueue *blockingQueue = blockingQueueCreate(1);

    // make a small pool (3) and add 2 jobs
    // to make sure they run
    //
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = threadPoolCreate("test", 3, 3, 10);

    // add 5 jobs
    TaskArg taskArgA = { .str = "a", .resultQueue = blockingQueue };
    TaskArg taskArgB = { .str = "b", .resultQueue = blockingQueue };
    TaskArg taskArgC = { .str = "c", .resultQueue = blockingQueue };
    TaskArg taskArgD = { .str = "d", .resultQueue = blockingQueue };
    TaskArg taskArgE = { .str = "e", .resultQueue = blockingQueue };
    threadPoolAddTask(pool, simpleTask, &taskArgA, threadPoolTaskArgDoNotFreeFunc);
    threadPoolAddTask(pool, simpleTask, &taskArgB, threadPoolTaskArgDoNotFreeFunc);
    threadPoolAddTask(pool, simpleTask, &taskArgC, threadPoolTaskArgDoNotFreeFunc);   // at min
    threadPoolAddTask(pool, simpleTask, &taskArgD, threadPoolTaskArgDoNotFreeFunc);
    threadPoolAddTask(pool, simpleTask, &taskArgE, threadPoolTaskArgDoNotFreeFunc);  // at max

    // since the queue can only hold 3 some will be waiting still
    //
    uint32_t size = threadPoolGetBacklogCount(pool);
    icLogDebug(LOG_CAT, "pool size is %d", size);
    assert_true(size > 0);

    // let the tasks go
    for(int i = 0; i < 5; ++i)
    {
        assert_non_null(blockingQueuePopTimeout(blockingQueue,10));
    }

    // destroy the pool
    threadPoolDestroy(pool);

    blockingQueueDestroy(blockingQueue, doNotDestroyQueueItemFunc);
}

/*
 * make sure the pool can grow up to the max
 */
static void test_canGrowJobs(void **state)
{
    icBlockingQueue *blockingQueue = blockingQueueCreate(1);

    // make a pool with room to grow
    //
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = threadPoolCreate("test", 1, 5, 10);

    // intialize counter
    counter = 0;

    // add 6 jobs to see that the pool grows
    TaskArg taskArgs[6];
    char strs[6][2];
    for(int i = 0; i < 6; ++i)
    {
        strs[i][0] = 'a' + i;
        strs[i][1] = '\0';
        taskArgs[i].str = strs[i];
        taskArgs[i].resultQueue = blockingQueue;
        threadPoolAddTask(pool, simpleTask, &taskArgs[i], threadPoolTaskArgDoNotFreeFunc);
    }

    // Wait for counter to get to 6, means 1 completed, 5 active, 0 backlog
    pthread_mutex_lock(&mutex);
    while(counter < 6)
    {
        if (incrementalCondTimedWait(&cond, &mutex, 10) == ETIMEDOUT)
        {
            break;
        }

    }
    assert_true(counter == 6);
    pthread_mutex_unlock(&mutex);
    uint32_t back = threadPoolGetBacklogCount(pool);
    uint32_t active = threadPoolGetActiveCount(pool);
    icLogDebug(LOG_CAT, "pool active=%d backlog=%d", active, back);
    assert_true(active == 5 && back == 0);

    // let the tasks go
    for(int i = 0; i < 6; ++i)
    {
        assert_non_null(blockingQueuePopTimeout(blockingQueue,10));
    }

    // destroy the pool
    threadPoolDestroy(pool);

    blockingQueueDestroy(blockingQueue, doNotDestroyQueueItemFunc);
}

/*
 * make sure the pool can start with 0 threads
 */
static void test_canHaveZeroMinThreads(void **state)
{
    icBlockingQueue *blockingQueue = blockingQueueCreate(1);

    // make a pool with 0 min threads
    //
    icLogDebug(LOG_CAT, "running thread pool test '%s'", __FUNCTION__);
    icThreadPool *pool = threadPoolCreate("test", 0, 1, 10);

    // intialize counter
    counter = 0;

    // add 1 job to see that the pool grows
    TaskArg taskArgA = { .str = "a", .resultQueue = blockingQueue };
    TaskArg taskArgB = { .str = "b", .resultQueue = blockingQueue };
    threadPoolAddTask(pool, simpleTask, &taskArgA, threadPoolTaskArgDoNotFreeFunc);
    threadPoolAddTask(pool, simpleTask, &taskArgB, threadPoolTaskArgDoNotFreeFunc);

    // This waits for the thread to start
    // Wait for counter to get to 2, means 1 completed, 1 active, 0 backlog
    pthread_mutex_lock(&mutex);
    while(counter < 2)
    {
        if (incrementalCondTimedWait(&cond, &mutex, 10) == ETIMEDOUT)
        {
            break;
        }

    }
    assert_true(counter == 2);
    pthread_mutex_unlock(&mutex);
    uint32_t back = threadPoolGetBacklogCount(pool);
    uint32_t active = threadPoolGetActiveCount(pool);
    icLogDebug(LOG_CAT, "pool active=%d backlog=%d", active, back);

    // see that thread started as expected
    assert_true(active == 1 && back == 0);

    // let the tasks go
    assert_non_null(blockingQueuePopTimeout(blockingQueue,10));

    // destroy the pool, which will wait for everything to complete
    threadPoolDestroy(pool);

    blockingQueueDestroy(blockingQueue, doNotDestroyQueueItemFunc);
}

/*
 * Task designed to block until a condition is signaled. Allows control over number of threads in the pool for
 * counting purposes.
 */
static void blockingTask(void *arg)
{
    (void) arg;

    pthread_mutex_lock(&mutex);
    while (awoken == false)
    {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
}

/*
 * Wait until all the threads are "active" in the pool. This is a silly sleep, but I don't want to add anymore
 * threads and we should hit this active count fairly quickly.
 */
static bool waitForThreadCount(icThreadPool *pool, int amount, uint16_t maxTimeSeconds)
{
    uint32_t threadCount = threadPoolGetThreadCount(pool);
    uint8_t sleepIncr = 1; //1 second
    uint16_t totalTime = 0;
    while (threadCount != amount && totalTime < maxTimeSeconds)
    {
        sleep(sleepIncr);
        totalTime += sleepIncr;
        threadCount = threadPoolGetThreadCount(pool);
    }

    return threadCount == amount;
}

/*
 * Verifies that threads used in a threadpool are properly joined and cleaned up after they finish running.
 */
static void test_threadsAreCleanedUp(void **state)
{
    icLogDebug(LOG_CAT, "Starting test %s", __FUNCTION__);

    //Initialize variables
    uint16_t maxThreads = 10;
    uint16_t minThreads = 0;
    uint16_t numThreadsToAdd = 1;
    icThreadPool *pool = threadPoolCreate("test", minThreads, maxThreads, 15);
    counter = 0;
    awoken = false;

    icLogDebug(LOG_CAT, "Starting number of threads in pool = %"PRIu16, threadPoolGetThreadCount(pool));

    //Populate our pool with some number of blocking threads
    for (counter = 0; counter < numThreadsToAdd; counter++)
    {
        threadPoolAddTask(pool, blockingTask, NULL, threadPoolTaskArgDoNotFreeFunc);
    }

    //Wait until all threads are running
    bool success = waitForThreadCount(pool, counter+minThreads, 3);

    //If we didn't hit the active count, then this test is a failure.
    assert_true(success);

    icLogDebug(LOG_CAT, "Running number of threads in pool = %"PRIu16, threadPoolGetThreadCount(pool));

    //Now lets signal our threads so they finish.
    pthread_mutex_lock(&mutex);
    awoken = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

    //Give the thread pool threads time to realize there's nothing to do and end (may take up to numThreads * 10 seconds)
    success = waitForThreadCount(pool, minThreads, (counter+minThreads) * 10);

    //If we didn't hit the thread count, then this test is a failure.
    assert_true(success);

    icLogDebug(LOG_CAT, "Final number of threads in pool = %"PRIu16, threadPoolGetThreadCount(pool));

    //Cleanup
    threadPoolDestroy(pool);
    icLogDebug(LOG_CAT, "Ending test %s", __FUNCTION__);
    (void) state;
}

static void doNotFreeArgFunc(void *arg)
{
    // Nothing to do
    (void)arg;
}

static void selfDestroyTask(void *arg)
{
    icThreadPool *pool = (icThreadPool *)arg;
    threadPoolDestroy(pool);
    pthread_mutex_lock(&mutex);
    awoken = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);

}

static void test_taskCanDestroyPool(void **state)
{
    // Reset just in case
    awoken = false;
    icThreadPool *pool = threadPoolCreate("test", 1, 1, 15);

    threadPoolAddTask(pool, selfDestroyTask, pool, doNotFreeArgFunc);
    // Need to make sure the task actually started and did the destroy
    pthread_mutex_lock(&mutex);
    while(awoken == false)
    {
        if (incrementalCondTimedWait(&cond, &mutex, 1) == ETIMEDOUT)
        {
            fail_msg("self destroy task should have started");
            break;
        }
    }

    assert_true(awoken);
}

/*
 * main
 */
int main(int argc, const char **argv)
{
    // init logging and set to ERROR so we don't output a ton of log lines
    // NOTE: feel free to make this INFO or DEBUG when debugging
    initIcLogger();
    //setIcLogPriorityFilter(IC_LOG_ERROR);
    initTimedWaitCond(&cond);

    // make our array of tests to run
    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_doesPreventStupidity),
                    cmocka_unit_test(test_canRunJobs),
                    cmocka_unit_test(test_canQueueJobs),
                    cmocka_unit_test(test_canGrowJobs),
                    cmocka_unit_test(test_canHaveZeroMinThreads),
                    cmocka_unit_test(test_threadsAreCleanedUp),
                    cmocka_unit_test(test_taskCanDestroyPool)
            };

    // fire off the suite of tests
    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    // shutdown
    closeIcLogger();
    return retval;
}

