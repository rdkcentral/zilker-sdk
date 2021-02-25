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
// Created by mkoch201 on 7/27/19.
//

// cmocka & it's dependencies
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <icLog/logging.h>
#include <icConcurrent/icThreadSafeWrapper.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>

#define LOG_TAG "ThreadSafeWrapperTest"
#define INITIAL_VALUE_PREFIX "initialValue"
#define INITIAL_VALUE INITIAL_VALUE_PREFIX" "

static void *autoAssignFunc();
static bool checkReleaseFunc(void *item);

static bool doRelease = false;
static icThreadSafeWrapper autoAssignWrapper = THREAD_SAFE_WRAPPER_INIT(autoAssignFunc, checkReleaseFunc, free);
static icThreadSafeWrapper manualAssignWrapper = THREAD_SAFE_WRAPPER_INIT(NULL, NULL, free);
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t readStartCond;
static pthread_cond_t continueReadCond;

static void *autoAssignFunc()
{
    return strdup(INITIAL_VALUE);
}

static void *conditionAssignFunc()
{
    return strdup(INITIAL_VALUE_PREFIX"X");
}

static bool checkReleaseFunc(void *item)
{
    return doRelease;
}

static void modifyFunc(void *item, void *context)
{
    char *replaceChar = (char *)context;
    char *str = (char *)item;
    str[strlen(INITIAL_VALUE)-1] = *replaceChar;
}

static void readFunc(const void *item, const void *context)
{
    if (context == NULL)
    {
        assert_null(item);
    }
    else
    {
        assert_string_equal((char *) item, (char *) context);
    }
}

static void blockingReadFunc(const void *item, const void *context)
{
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&readStartCond);
    if (incrementalCondTimedWait(&continueReadCond, &mutex, 5) == ETIMEDOUT)
    {
        assert_true(false);
    }
    readFunc(item, context);
    pthread_mutex_unlock(&mutex);
}

static void *readThread(void *context)
{
    threadSafeWrapperReadItem(&autoAssignWrapper, blockingReadFunc, context);
    return NULL;
}

/*
 * test to make sure basic async modification then read works
 */
static void test_asyncModifyAndThenRead(void **state)
{
    icLogDebug(LOG_TAG, "running test '%s'", __FUNCTION__);

    // Assign
    threadSafeWrapperAssignItem(&manualAssignWrapper, strdup(INITIAL_VALUE));

    // Write
    threadSafeWrapperEnqueueModification(&manualAssignWrapper, modifyFunc, "A", threadSafeWrapperDoNotFreeContextFunc,
                                         NULL);

    // Read
    assert_true(threadSafeWrapperReadItem(&manualAssignWrapper, readFunc, INITIAL_VALUE_PREFIX"A"));

    // Cleanup
    threadSafeWrapperReleaseItem(&manualAssignWrapper);
}

/*
 * test to make sure basic sync modification then read works
 */
static void test_syncModifyAndThenRead(void **state)
{
    icLogDebug(LOG_TAG, "running test '%s'", __FUNCTION__);

    // Assign
    threadSafeWrapperAssignItem(&manualAssignWrapper, strdup(INITIAL_VALUE));

    // Write
    threadSafeWrapperModifyItem(&manualAssignWrapper, modifyFunc, "A");

    // Read
    assert_true(threadSafeWrapperReadItem(&manualAssignWrapper, readFunc, INITIAL_VALUE_PREFIX"A"));

    // Cleanup
    threadSafeWrapperReleaseItem(&manualAssignWrapper);
}

/*
 * test to make sure basic async modification then read works
 */
static void test_asyncModifyAndThenReadWithAutoAssign(void **state)
{
    icLogDebug(LOG_TAG, "running test '%s'", __FUNCTION__);

    // Put something in, should auto assign
    threadSafeWrapperEnqueueModification(&autoAssignWrapper, modifyFunc, "A", threadSafeWrapperDoNotFreeContextFunc,
                                         NULL);

    // Read
    assert_true(threadSafeWrapperReadItem(&autoAssignWrapper, readFunc, INITIAL_VALUE_PREFIX"A"));

    // Cleanup
    threadSafeWrapperReleaseItem(&autoAssignWrapper);
}

/*
 * test to make sure basic async modification then read works
 */
static void test_syncModifyAndThenReadWithAutoAssign(void **state)
{
    icLogDebug(LOG_TAG, "running test '%s'", __FUNCTION__);

    // Put something in, should auto assign
    threadSafeWrapperModifyItem(&autoAssignWrapper, modifyFunc, "A");

    // Read
    assert_true(threadSafeWrapperReadItem(&autoAssignWrapper, readFunc, INITIAL_VALUE_PREFIX"A"));

    // Cleanup
    threadSafeWrapperReleaseItem(&autoAssignWrapper);
}

/*
 * test to make sure async modifications while reading are deferred
 */
static void test_asyncModifyWhileReadingIsDelayed(void **state)
{
    icLogDebug(LOG_TAG, "running test '%s'", __FUNCTION__);

    // Put something in, should auto assign
    threadSafeWrapperEnqueueModification(&autoAssignWrapper, modifyFunc, "A", threadSafeWrapperDoNotFreeContextFunc,
                                         NULL);

    // Kick off a thead to do a read
    pthread_mutex_lock(&mutex);
    pthread_t thread;
    createThread(&thread, readThread, INITIAL_VALUE_PREFIX"A", "readA");

    if (incrementalCondTimedWait(&readStartCond, &mutex, 5) == ETIMEDOUT)
    {
        assert_true(false);
    }

    threadSafeWrapperEnqueueModification(&autoAssignWrapper, modifyFunc, "B", threadSafeWrapperDoNotFreeContextFunc,
                                         NULL);

    // Finish the read
    pthread_cond_broadcast(&continueReadCond);
    pthread_mutex_unlock(&mutex);
    pthread_join(thread, NULL);

    // Now read again, it should be applied
    assert_true(threadSafeWrapperReadItem(&autoAssignWrapper, readFunc, INITIAL_VALUE_PREFIX"B"));

    // Cleanup
    threadSafeWrapperReleaseItem(&autoAssignWrapper);
}

/*
 * test to make sure release while reads are outstanding are safe
 */
static void test_releaseWhileReadingIsSafe(void **state)
{
    icLogDebug(LOG_TAG, "running test '%s'", __FUNCTION__);

    // Put something in, should auto assign
    threadSafeWrapperEnqueueModification(&autoAssignWrapper, modifyFunc, "A", threadSafeWrapperDoNotFreeContextFunc,
                                         NULL);

    // Kick off a thead to do a read
    pthread_mutex_lock(&mutex);
    pthread_t thread;
    createThread(&thread, readThread, INITIAL_VALUE_PREFIX"A", "readA");

    if (incrementalCondTimedWait(&readStartCond, &mutex, 5) == ETIMEDOUT)
    {
        assert_true(false);
    }

    threadSafeWrapperEnqueueModification(&autoAssignWrapper, modifyFunc, "B", threadSafeWrapperDoNotFreeContextFunc,
                                         NULL);

    // release it
    threadSafeWrapperReleaseItem(&autoAssignWrapper);

    // Finish the read
    pthread_cond_broadcast(&continueReadCond);
    pthread_mutex_unlock(&mutex);
    pthread_join(thread, NULL);

    // Now read again, it should be gone
    assert_false(threadSafeWrapperReadItem(&autoAssignWrapper, readFunc, NULL));
}

/*
 * test assignIfReleased works
 */
static void test_assignIfReleased(void **state)
{
    icLogDebug(LOG_TAG, "running test '%s'", __FUNCTION__);

    // Currently unassigned, should succeed
    assert_true(threadSafeWrapperAssignItemIfReleased(&autoAssignWrapper, autoAssignFunc));

    // Currently assigned, should fail
    assert_false(threadSafeWrapperAssignItemIfReleased(&autoAssignWrapper, conditionAssignFunc));

    // Read
    assert_true(threadSafeWrapperReadItem(&autoAssignWrapper, readFunc, INITIAL_VALUE));

    // Cleanup
    threadSafeWrapperReleaseItem(&autoAssignWrapper);
}

/*
 * main
 */
int main(int argc, const char **argv)
{
    // init logging and set to ERROR so we don't output a ton of log lines
    // NOTE: feel free to make this INFO or DEBUG when debugging
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_ERROR);

    initTimedWaitCond(&readStartCond);
    initTimedWaitCond(&continueReadCond);

    // make our array of tests to run
    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_asyncModifyAndThenRead),
                    cmocka_unit_test(test_syncModifyAndThenRead),
                    cmocka_unit_test(test_asyncModifyAndThenReadWithAutoAssign),
                    cmocka_unit_test(test_syncModifyAndThenReadWithAutoAssign),
                    cmocka_unit_test(test_asyncModifyWhileReadingIsDelayed),
                    cmocka_unit_test(test_releaseWhileReadingIsSafe),
                    cmocka_unit_test(test_assignIfReleased),
            };

    // fire off the suite of tests
    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    // shutdown
    closeIcLogger();
    return retval;
}