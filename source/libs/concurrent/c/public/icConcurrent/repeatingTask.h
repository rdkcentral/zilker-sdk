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
 * repeatingTask.h
 *
 * Creates a repeating task that will loop until told to cancel.
 * Each iteration of the loop can pause for a specified amount of
 * time before executing again.  Helpful for things such as "monitor threads"
 * that need to execute the same operation over and over again, with an
 * optional delay between executions.
 *
 * NOTE: uses the concept of 'handles' vs 'objects' due to
 * the nature of tasks executing and releasing in the background.
 * this prevents memory issues with the caller having a pointer
 * to an object that may have been released in the background.
 *
 * Author: jelderton - 10/8/18
 *-----------------------------------------------*/

#ifndef IC_REPEATTASK_H
#define IC_REPEATTASK_H

#include <stdint.h>
#include <stdbool.h>

// steal function prototypes and enums
#include "icConcurrent/delayedTask.h"

/*
 * function prototype for being executed every iteration
 * of a back off repeating task
 *
 * @param arg - optional input variable
 * @return - true to end task, false to keep going
 */
typedef bool (*backOffTaskRunCallbackFunc)(void *arg);

/*
 * function prototype for when backOffTaskRunCallbackFunc
 * gets a return value of true (which is success)
 *
 * this prototype can be used as a cleanup function
 *
 * @param arg - original args from backOffTaskRunCallbackFunc
 */
typedef void (*backOffTaskSuccessCallbackFunc)(void *arg);

/*
 * create a repeating task that will invoke 'taskCallbackFunc'
 * (passing 'arg'); then pause for 'delayAmount' before executing
 * again.  this pattern will continue until the task is canceled.
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time each 'amount' represents
 * @param func - function to execute at each iteration
 * @param arg - optional argument passed to the taskCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled
 */
uint32_t createRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *arg);

/*
 * create a repeating task that will invoke 'taskCallbackFunc'
 * (passing 'arg'); then run it again after the given delay relative
 * to when the task initially started.  Then again after 2 * delay, etc.
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time each 'amount' represents
 * @param func - function to execute at each iteration
 * @param arg - optional argument passed to the taskCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled
 */
uint32_t createFixedRateRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *arg);

/*
 * TODO: get this enhancement working
 *
 * create a repeating task that will invoke 'taskCallbackFunc'
 * (passing 'arg'); will wait 'delayAmount' before executing
 * the first one then wait 'delayAmount' before executing again.
 * This pattern will continue until the task is canceled.
 *
 * @param delayAmount - number of 'units' to delay
 * @param units - amount of time each 'amount' represents
 * @param func - function to execute at each iteration
 * @param arg - optional argument passed to the taskCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled

uint32_t createInitialDelayRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *arg);
 */

/*
 * create a repeating task that will invoke 'backOffTaskRunCallbackFunc'
 * (passing 'arg'); will wait 'initDelayAmount', then run again and again.
 * Every iteration will then increase the delay amount by 'delayInterval'
 * each time until 'maxDelayAmount' is reached, then stay at that delay amount.
 *
 * If the callback 'backOffTaskRunCallbackFunc' returns true then the backOffTask
 * will finish and invoke 'backOffTaskSuccessCallbackFunc' to notify of success.
 *
 * If the back off task is cancelled at any time it will be handled just like any other
 * repeating tasks.
 *
 * @param initDelayAmount - the delay amount after the 1st pass
 * @param maxDelayAmount - the max delay amount to be reached
 * @param incrementDelayAmount - the delay amount to be increased by each iteration
 * @param units - amount of time each 'amount' represents
 * @param runFunc - function to execute at each iteration
 * @param successFunc - function to execute when backOffTaskRunCallbackFunc gets a 'true' response
 * @param arg - optional argument passed to the backOffTaskRunCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled
 */
uint32_t createBackOffRepeatingTask(uint64_t initDelayAmount, uint64_t maxDelayAmount, uint64_t incrementDelayAmount, delayUnits units,
        backOffTaskRunCallbackFunc runFunc, backOffTaskSuccessCallbackFunc cleanFunc, void *arg);

/*
 * cancel the repeating task. returns the original 'arg' supplied
 * during the creation - allowing cleanup to safely occur.
 * WARNING:
 * this should NOT be called while holding a lock that the task
 * function can also hold or a deadlock might result
 */
void *cancelRepeatingTask(uint32_t task);

/*
 * cancels the pause, forcing the task to loop around and execute
 */
void shortCircuitRepeatingTask(uint32_t task);

/*
 * change the delayAmount for a repeating task.  if the 'changeNow' flag
 * is true, then it will reset the current pause time and start again.  otherwise
 * this will not apply until the next pause.
 */
void changeRepeatingTask(uint32_t task, uint32_t delayAmount, delayUnits units, bool changeNow);

#endif // IC_REPEATTASK_H
