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

#include <pthread.h>
#include <icBuildtime.h>

/*
 * Simply detecting if we have a pthread_setname_np is insufficient to decide how get/setThreadName will work.
 * macOS and glibc, for example, both export that symbol but they have incompatible prototypes.
 *
 * prctl is Linux-specific and is not a suitable generic solution.
 */

/* Wrap tasks for C libraries that cannot set sibling thread names */
#if defined(CONFIG_OS_DARWIN) || defined(__UCLIBC__)
#define USE_TASK_WRAPPER
#endif

/* Android does not have pthread_getname_np, and uClibc has neither getname nor setname */
#if defined(__UCLIBC__) || defined(__ANDROID__)
#include <sys/prctl.h>
#endif

#include <icBuildtime.h>
#include <stdbool.h>
#include <icLog/logging.h>
#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include "icConcurrent/threadUtils.h"

#define LOG_TAG "threadUtils"

#ifdef CONFIG_OS_DARWIN
#define TASK_COMM_LEN 64
#else
#define TASK_COMM_LEN 16
#endif

#ifdef USE_TASK_WRAPPER
typedef struct
{
    taskFunc task;
    void *taskArg;
    char *name;
} TaskWrapper;
#endif

extern inline void pthread_mutex_unlock__auto(pthread_mutex_t **mutex);

static void setThreadName(const pthread_t *tid, const char *name)
{
#if defined(_GNU_SOURCE) || defined(__ANDROID__) || defined(CONFIG_OS_DARWIN)
    if (name != NULL)
    {
        char stdName[TASK_COMM_LEN];
        /*
         * The system is supposed to silently truncate the name if > TASK_COMM_LEN chars (including null)
         * but Linux will EFAULT if too long.
         */
        if (strlen(name) >= TASK_COMM_LEN)
        {
            icLogDebug(LOG_TAG, "thread name '%s' is too long, truncating to %d chars", name, TASK_COMM_LEN - 1);
            strncpy(stdName, name, TASK_COMM_LEN);
            stdName[TASK_COMM_LEN - 1] = 0;
            name = stdName;
        }

        int rc = 0;

#if defined(USE_TASK_WRAPPER)
        /* For MacOS 10.6+, we can only set our own thread's name */
        if (pthread_equal(*tid, pthread_self()))
        {
#ifdef CONFIG_OS_DARWIN
            rc = pthread_setname_np(name);
#elif defined(__linux__)
            prctl(PR_SET_NAME, name, 0, 0, 0);
#else
#warning No support for thread names available
#endif
        }
        else
        {
            rc = -1;
            errno = ESRCH;
        }
#else
        rc = pthread_setname_np(*tid, name);
#endif /* USE_TASK_WRAPPER */

        if (rc != 0)
        {
            AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(rc);
            icLogWarn(LOG_TAG, "Unable to set thread name '%s': %s", name, errStr);
        }
    }
#else
#warning No support for thread names available
#endif /* _GNU_SOURCE || __ANDROID__ || CONFIG_OS_DARWIN */
}

#ifdef USE_TASK_WRAPPER
static void *wrappedTask(void *taskWrapper)
{
    TaskWrapper wrapper = *((TaskWrapper *) taskWrapper);
    free(taskWrapper);
    taskWrapper = NULL;

    void *retVal = NULL;
    pthread_t self = pthread_self();
    setThreadName(&self, wrapper.name);
    free(wrapper.name);
    wrapper.name = NULL;

    retVal = wrapper.task(wrapper.taskArg);

    return retVal;
}
#endif

bool createDetachedThread(taskFunc task, void *taskArg, const char *name)
{
    bool ok;
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

#ifdef USE_TASK_WRAPPER
    TaskWrapper *wrapper = malloc(sizeof(TaskWrapper));
    wrapper->task = task;
    wrapper->name = name == NULL ? NULL : strdup(name);
    wrapper->taskArg = taskArg;

    task = wrappedTask;
    taskArg = wrapper;
#endif

    int rc = pthread_create(&tid, &attr, task, taskArg);
    pthread_attr_destroy(&attr);
    ok = rc == 0;

    if (ok)
    {
        setThreadName(&tid, name);
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(rc);
        icLogWarn(LOG_TAG, "Unable to create thread: %s", errStr);
    }

    return ok;
}

bool createThread(pthread_t *tid, taskFunc task, void *taskArg, const char *name)
{
    bool ok;

    if (tid == NULL)
    {
        icLogError(LOG_TAG, "tid cannot be NULL");
        return false;
    }

#ifdef USE_TASK_WRAPPER
    TaskWrapper *wrapper = malloc(sizeof(TaskWrapper));
    wrapper->task = task;
    wrapper->name = name == NULL ? NULL : strdup(name);
    wrapper->taskArg = taskArg;

    task = wrappedTask;
    taskArg = wrapper;
#endif

    int rc = pthread_create(tid, NULL, task, taskArg);

    ok = rc == 0;
    if (rc == 0)
    {
        setThreadName(tid, name);
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(rc);
        icLogWarn(LOG_TAG, "Unable to create thread: %s", errStr);
    }

    return ok;
}

char *getThreadName(const pthread_t tid)
{
    char *threadName = NULL;
#if defined(_GNU_SOURCE) || defined(__ANDROID__) || defined(CONFIG_OS_DARWIN)
    threadName = malloc(TASK_COMM_LEN);
    int rc = 0;

#if defined(__UCLIBC__) || defined(__ANDROID__)
    if (pthread_equal(tid, pthread_self()))
    {
        /*
         * Android (until Pie/9.0) and uClibc don't have pthread_getname_np but support getting the current
         * thread name via prctl.
         */
        rc = prctl(PR_GET_NAME, threadName, 0, 0, 0);
    }
    else
    {
        rc = -1;
        errno = ESRCH;
    }
#else
    /* This is supported in MacOS (their getname and setname implementations are inconsistent) */
    rc = pthread_getname_np(tid, threadName, TASK_COMM_LEN);
#endif /* __UCLIBC__ || __ANDROID__ */

    if (rc != 0)
    {
        AUTO_CLEAN(free_generic__auto) char *errStr = strerrorSafe(errno);
        icLogDebug(LOG_TAG, "%s: unable to get thread name: %s", __func__, errStr);

        free(threadName);
        threadName = NULL;
    }
#else
#warning No support for thread names available
#endif /* _GNU_SOURCE || __ANDROID__ || CONFIG_OS_DARWIN */
    return threadName;
}

void mutexInitWithType(pthread_mutex_t *mtx, int type)
{
    int pthreadError = 0;
    pthread_mutexattr_t attrs;
    pthreadError = pthread_mutexattr_init(&attrs);
    if (pthreadError != 0)
    {
        icLogWarn(LOG_TAG, "Mutex attr init at %p failed!", mtx);
    }
    assert(pthreadError == 0);

    if (type == PTHREAD_MUTEX_NORMAL)
    {
        icLogWarn(LOG_TAG, "PTHREAD_MUTEX_NORMAL removes safety mechanisms; upgrading to PTHREAD_MUTEX_ERRORCHECK");
        type = PTHREAD_MUTEX_ERRORCHECK;
    }

    pthreadError = pthread_mutexattr_settype(&attrs, type);
    if (pthreadError != 0)
    {
        icLogWarn(LOG_TAG, "Mutex set type to %d at %p failed!", type, mtx);
    }
    assert(pthreadError == 0);

    pthreadError = pthread_mutex_init(mtx, &attrs);
    pthread_mutexattr_destroy(&attrs);

    if (pthreadError != 0)
    {
        icLogWarn(LOG_TAG, "Mutex init at %p failed!", mtx);
        abort();
    }
}

void _mutexLock(pthread_mutex_t *mtx, const char *file, const int line)
{
    int error = pthread_mutex_lock(mtx);

    if (error != 0)
    {
        char *errStr;
        switch(error)
        {
            case EAGAIN:
                errStr = "recursion limit reached";
                break;

            case EDEADLK:
                errStr = "already locked by current thread";
                break;

            case EINVAL:
                errStr = "uninitialized mutex or priority inversion";
                break;

            default:
                errStr = "unknown";
                break;
        }
        icLogError(LOG_TAG, "mutex lock failed: [%d](%s) at %s:%d!", error, errStr, file, line);

        abort();
    }
}

void _mutexUnlock(pthread_mutex_t *mtx, const char *file, const int line)
{
    int error = pthread_mutex_unlock(mtx);

    if (error != 0)
    {
        const char *errStr = "unknown";
        if (error == EPERM)
        {
            errStr = "current thread does not own mutex";
        }
        else if (error == EINVAL)
        {
            errStr = "mutex not initialized";
        }

        icLogError(LOG_TAG, "mutex unlock failed: [%d](%s) at %s:%d!", error, errStr, file, line);

        abort();
    }
}
