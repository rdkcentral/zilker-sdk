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
 * internal.c
 *
 * internal objects/locks used by zone, trouble, and
 * alarm.  this should NOT be utilized by any public-facing
 * module (i.e. eventListener, ipcHandler, outbound IPC
 * requests).
 *
 * Author: jelderton -  11/13/18.
 *-----------------------------------------------*/


#include "internal.h"

/*
 * shared mutex lock, exclusivily reserved for use by the
 * zone, trouble, and alarm internals.  allows all three
 * subsystems to safely communicate with one another without
 * danger of deadlock and maintain thread safety.
 *
 * by convention, any functions that NEED the mutex will
 * have a "Public" suffix in the name - idea is for external
 * inputs to use the Public functions (ipc, events, etc).
 */
static pthread_mutex_t SECURITY_MTX = PTHREAD_MUTEX_INITIALIZER;

/*
 * shared taskExecutor for backgrounding tasks that need to
 * be performed outside of the SECURITY_MTX (ex: sending an event)
 *
 * this is created/drestroyed by main
 */
static icTaskExecutor *sharedTaskExecutor = NULL;

/*
 * Lock security mutex, exclusivily reserved for use by the
 * zone, trouble, and alarm internals.  allows all three
 * subsystems to safely communicate with one another without
 * danger of deadlock and maintain thread safety.
 *
 * by convention, any functions that NEED the mutex will
 * have a "Public" suffix in the name - idea is for external
 * inputs to use the Public functions (ipc, events, etc).
 */
void lockSecurityMutex()
{
    pthread_mutex_lock(&SECURITY_MTX);
}

/*
 * Unlock security mutex, exclusivily reserved for use by the
 * zone, trouble, and alarm internals.  allows all three
 * subsystems to safely communicate with one another without
 * danger of deadlock and maintain thread safety.
 *
 * by convention, any functions that NEED the mutex will
 * have a "Public" suffix in the name - idea is for external
 * inputs to use the Public functions (ipc, events, etc).
 */
void unlockSecurityMutex()
{
    pthread_mutex_unlock(&SECURITY_MTX);
}

/*
 * Init the shared taskExecutor
 */
void initSecurityTask()
{
    sharedTaskExecutor = createTaskExecutor();
}

/*
 * Destroy the shared taskExecutor
 */
void destroySecurityTask()
{
    destroyTaskExecutor(sharedTaskExecutor);
}

/*
 * For backgrounding tasks that need to be performed
 * outside of the security mutex (ex: sending an event)
 */
bool appendSecurityTask(void *taskObj, void *taskArg,
                        taskExecRunFunc runFunc, taskExecFreeFunc freeFunc)
{
    return appendTaskToExecutor(sharedTaskExecutor, taskObj, taskArg, runFunc, freeFunc);
}
