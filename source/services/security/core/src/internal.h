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
 * internal.h
 *
 * internal objects/locks used by zone, trouble, and
 * alarm.  this should NOT be utilized by any public-facing
 * module (i.e. eventListener, ipcHandler, outbound IPC
 * requests).
 *
 * Author: jelderton -  11/13/18.
 *-----------------------------------------------*/

#ifndef ZILKER_INTERNAL_H
#define ZILKER_INTERNAL_H

#include <pthread.h>
#include <icConcurrent/taskExecutor.h>

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
void lockSecurityMutex();

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
void unlockSecurityMutex();

/*
 * Init the shared taskExecutor
 */
void initSecurityTask();

/*
 * Destroy the shared taskExecutor
 */
void destroySecurityTask();

/*
 * For backgrounding tasks that need to be performed
 * outside of the security mutex (ex: sending an event)
 */
bool appendSecurityTask(void *taskObj, void *taskArg,
        taskExecRunFunc runFunc, taskExecFreeFunc freeFunc);



#endif // ZILKER_INTERNAL_H
