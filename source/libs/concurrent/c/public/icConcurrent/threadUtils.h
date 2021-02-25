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

#ifndef ZILKER_THREADUTILS_H
#define ZILKER_THREADUTILS_H

#include <stdbool.h>
#include <pthread.h>
#include <icTypes/sbrm.h>

typedef void * (*taskFunc)(void *);

/**
 * Create a detached thread
 * @param task
 * @param taskArg
 * @param name The thread name; may be NULL. This should be limited to 15 characters; long strings will be truncated.
 * @return true when the thread was created
 */
bool createDetachedThread(taskFunc task, void *taskArg, const char *name);

/**
 * Create a joinable thread
 * @param tid
 * @param task
 * @param taskArg
 * @param name The thread name; may be NULL. This should be limited to 15 characters; long strings will be truncated.
 * @return true when the thread was created
 */
bool createThread(pthread_t *tid, taskFunc task, void *taskArg, const char *name);

/**
 * Get the thread name by tid
 * @note Android only supports getting the calling thread name
 * @param tid
 * @return thread name or NULL if unavailable (caller must free)
 */
char *getThreadName(const pthread_t tid);

/**
 * Initialize a mutex with a type
 * @param mtx
 * @param type The lock type, e.g., PTHREAD_MUTEX_RECURSIVE, PTHREAD_MUTEX_ERRORCHECK
 */
void mutexInitWithType(pthread_mutex_t *mtx, int type);

/**
 * This implements mutexLock - use the macro below to automatically localize errors
 * @param mtx The mutex to lock
 * @param file The mutexLock call site file
 * @param line The mutexLock call site line number
 */
void _mutexLock(pthread_mutex_t *mtx, const char *file, const int line);

/**
 * Lock a mutex, blocking if held by another thread.
 * @param mtx
 * @note When mutex error checking is enabled, an invalid operation will abort the program.
 */
#define mutexLock(mtx) _mutexLock(mtx, __FILE__, __LINE__)

/* --- */

/**
 * This implements mutexUnlock - use the macro below to automatically localize errors
 * @param mtx The mutex to unlock
 * @param file The mutexUnlock call site file
 * @param line The mutexUnlock call site line number
 */
void _mutexUnlock(pthread_mutex_t *mtx, const char *file, const int line);

/**
 * Unlock a mutex
 * @param mtx
 * @note When mutex error checking is enabled, an invalid unlock will abort the program
 */
#define mutexUnlock(mtx) _mutexUnlock(mtx, __FILE__, __LINE__)

/**
 * Convenience macro to create a guard pointer for a mutex and lock it, releasing it when the guard leaves the current scope.
 * @note This should be limited to use in small sections. It generally is not suitable for use in an entire function.
 */
#define LOCK_SCOPE(m) AUTO_CLEAN(pthread_mutex_unlock__auto) pthread_mutex_t *lock_guard_##m = &m;\
mutexLock(lock_guard_##m)

inline void pthread_mutex_unlock__auto(pthread_mutex_t **mutex)
{
    mutexUnlock(*mutex);
}

#endif //ZILKER_THREADUTILS_H
