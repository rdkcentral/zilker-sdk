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

#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <icLog/logging.h>
#include <icConcurrent/timedWait.h>
#include <signal.h>
#include <icConcurrent/threadUtils.h>
#include <unistd.h>
#include <wait.h>

#define LOG_TAG "threadUtilsTest"

static pthread_mutex_t testMtx;

static int teardown(void **state)
{
    pthread_mutex_destroy(&testMtx);

    return 0;
}

static void forkExpectSignal(void (*test)(void **state), void **state, int signal)
{
    pid_t pid = fork();
    if (pid > 0)
    {
        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFSIGNALED(status) == 0 || WTERMSIG(status) != signal)
        {
            fail_msg("Child did not receive signal: signaled: %d signo: %d",
                     WIFSIGNALED(status),
                     WTERMSIG(status));
        }
    }
    else if (pid ==0)
    {
        test(state);
        /* Abort! */
    }
    else
    {
        fail_msg("fork() failed to create test process!");
    }
}

/**
 * Do a bad mutex operation: Try to lock the same mtx in the same thread.
 * @note testMtx must be an ERRORCHECK mutex.
 * @param state
 */
static void doMutexDeadlock(void **state)
{
    mutexLock(&testMtx);
    mutexLock(&testMtx);
}

/**
 * Do a bad mutex operation: Try to unlock a mtx not owned by the calling thread.
 * @note testMtx must be an ERRORCHECK mutex.
 * @param state
 */
static void doMutexOverUnlock(void **state)
{
    mutexLock(&testMtx);
    mutexUnlock(&testMtx);

    /* Abort! */
    mutexUnlock(&testMtx);
}

static void test_mutexErrorCheck(void **state)
{
    mutexInitWithType(&testMtx, PTHREAD_MUTEX_ERRORCHECK);

    forkExpectSignal(doMutexDeadlock, state, SIGABRT);
    forkExpectSignal(doMutexOverUnlock, state, SIGABRT);
}

static void test_mutexReentrant(void **state)
{
    mutexInitWithType(&testMtx, PTHREAD_MUTEX_RECURSIVE);

    mutexLock(&testMtx);
    mutexLock(&testMtx);

    mutexUnlock(&testMtx);
    mutexUnlock(&testMtx);
}

static void doMutexLock(void **state)
{
    mutexLock(&testMtx);
}

static void doMutexUnlock(void **state)
{
    mutexUnlock(&testMtx);
}

static void doBadScopeUnlock(void **state)
{
    {
        LOCK_SCOPE(testMtx);
    }

    /* testMtx unlocked at end of previous scope; this will fail */
    mutexUnlock(&testMtx);
}

static void test_mutexUninitialized(void **state)
{
    forkExpectSignal(doMutexLock, state, SIGABRT);
    forkExpectSignal(doMutexUnlock, state, SIGABRT);
}

static void test_mutexLockScope(void **state)
{
    mutexInitWithType(&testMtx, PTHREAD_MUTEX_ERRORCHECK);
    {
        LOCK_SCOPE(testMtx);
    }

    forkExpectSignal(doBadScopeUnlock, state, SIGABRT);
}

int main(int argc, const char **argv)
{
    initIcLogger();

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test_teardown(test_mutexReentrant, teardown),
                    cmocka_unit_test_teardown(test_mutexErrorCheck, teardown),
                    cmocka_unit_test_teardown(test_mutexUninitialized, teardown),
                    cmocka_unit_test_teardown(test_mutexLockScope, teardown)
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    closeIcLogger();

    return retval;
}

