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
 * repeatingTask.c
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


#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>

#include <icTypes/icLinkedList.h>
#include <icConcurrent/repeatingTask.h>
#include <icConcurrent/timedWait.h>
#include <icTime/timeUtils.h>
#include <icConcurrent/threadUtils.h>
#include <icUtil/stringUtils.h>


/*
 * private data structures
 */

/*
 * The state of a task
 */
typedef enum
{
    REPEAT_TASK_IDLE,           // prior to 'wait'
    REPEAT_TASK_WAITING,        // waiting for time to expire
    REPEAT_TASK_RUNNING,        // executing callback function
    REPEAT_TASK_CANCELED,       // signal a 'stop waiting', returns to IDLE
    REPEAT_TASK_SHORT_CIRCUIT   // signal to run again immediately
} repeatTaskState;

/*
 * The different types of tasks
 */
typedef enum
{
    NORMAL_REPEATING_TASK_TYPE,         // basic repeating task type
    FIXED_RATE_REPEATING_TASK_TYPE,     // fixed rate repeating task type
    BACK_OFF_REPEATING_TASK_TYPE        // back off repeating task type
} repeatingTaskType;

/*
 * represents a repeatTask object
 */
typedef struct _repeatTask
{
    pthread_mutex_t                 mutex;                  // mutex for the object
    pthread_cond_t                  cond;                   // condition used for canceling
    pthread_t                       tid;                    // thread id of the tasks' thread

    uint32_t                        handle;                 // identifier returned to callers
    repeatTaskState                 state;                  // current state

    struct timespec                 targetRunTime;          // Time when we want to execute next
    delayUnits                      units;                  // The unit of time to use (min, secs, etc.)

    uint64_t                        currentDelay;           // the delay amount, might change depending on the task
    uint64_t                        startDelay;             // the the delay amount after 1st run (for back off tasks)
    uint64_t                        maxDelay;               // the max delay amount to hit (for back off tasks)
    uint64_t                        incrementDelay;         // the amount delay amount will increase after each iteration (for back off tasks)

    taskCallbackFunc                callback;               // function to execute after the delay (for all other tasks)
    backOffTaskRunCallbackFunc      backOffRunCallback;     // function to execute after the delay (for back off tasks)
    backOffTaskSuccessCallbackFunc  backOffSuccessCallback; // function to execute after backOffRunCallback is successfully (for back off tasks)
    void                            *arg;                   // optional arg supplied by caller

    repeatingTaskType               taskType;               // what kind of task this is
    bool                            didComplete;            // Used for backoff task to resolve race when canceling and completing at same time
} repeatTask;

static icLinkedList     *tasks = NULL;  // list of repeatTask objects
static pthread_mutex_t  TASK_MTX = PTHREAD_MUTEX_INITIALIZER;
static uint64_t         handleCounter = 0;

/*
 * private internal functions
 */
static uint32_t initRepeatingTask(uint64_t delayAmount, uint64_t initDelayAmount, uint64_t maxDelayAmount, uint64_t incrementDelayAmount, delayUnits units,
                                  taskCallbackFunc func, backOffTaskRunCallbackFunc backOffRunFunc, backOffTaskSuccessCallbackFunc backOffSuccessFunc, void *arg, repeatingTaskType taskType);
static void *runRepeatTaskThread(void *arg);
static void releaseRepeatTask(repeatTask *task);
static bool findRepeatTaskByHandle(void *searchVal, void *item);
static uint32_t assignHandle();

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
uint32_t createRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *arg)
{
    return initRepeatingTask(delayAmount, 0, 0, 0, units, func, NULL, NULL, arg, NORMAL_REPEATING_TASK_TYPE);
}

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
uint32_t createFixedRateRepeatingTask(uint64_t delayAmount, delayUnits units, taskCallbackFunc func, void *arg)
{
    return initRepeatingTask(delayAmount, 0, 0, 0, units, func, NULL, NULL, arg, FIXED_RATE_REPEATING_TASK_TYPE);
}

/*
 * create a repeating task that will invoke 'backOffTaskRunCallbackFunc'
 * (passing 'arg'); will wait 'initDelayAmount', then run again and again.
 * Every iteration will then increase the delay amount by 'delayInterval'
 * each time until 'maxDelayAmount' is reached, then stay at that delay amount.
 *
 * If the callback 'backOffTaskRunCallbackFunc' returns true then the backOffTask
 * will finish and invoke 'backOffTaskSuccessCallbackFunc' to notify caller of success.
 *
 * If the back off task is cancelled at any time it will be handled just like any other
 * repeating tasks.
 *
 * @param initDelayAmount - the delay amount after the 1st pass
 * @param maxDelayAmount - the max delay amount to be reached
 * @param delayInterval - the delay amount to be increased by each iteration
 * @param units - amount of time each 'amount' represents
 * @param runFunc - function to execute at each iteration
 * @param successFunc - function to execute when backOffTaskRunCallbackFunc gets a 'true' response
 * @param arg - optional argument passed to the backOffTaskRunCallbackFunc
 *
 * @return a repeatingTask handle that can be queried or canceled
 */
uint32_t createBackOffRepeatingTask(uint64_t initDelayAmount, uint64_t maxDelayAmount, uint64_t incrementDelayAmount, delayUnits units,
                                    backOffTaskRunCallbackFunc runFunc, backOffTaskSuccessCallbackFunc successFunc, void *arg)
{
    return initRepeatingTask(0, initDelayAmount, maxDelayAmount, incrementDelayAmount, units, NULL, runFunc, successFunc, arg, BACK_OFF_REPEATING_TASK_TYPE);
}

/*
 * Helper function for determining the new delay amount for the Back Off tasks
 *
 * @return the new delay amount
 */
static uint64_t calcBackOffDelayAmount(uint64_t currentDelay, uint64_t startDelay, uint64_t incrementDelay, uint64_t maxDelay)
{
    // the start delay
    //
    if (currentDelay < startDelay)
    {
        return startDelay;
    }

    // see if the delay amount needs to hit the max
    //
    else if (currentDelay + incrementDelay >= maxDelay)
    {
        return maxDelay;
    }

    // just increase the delay amount by the interval amount
    //
    else
    {
        return currentDelay + incrementDelay;
    }
}

/*
 * Given a delay and units convert it to a timespec representing the delay
 */
static void populateTimespecDelay(uint64_t delay, delayUnits units, struct timespec *delaySpec)
{
    if (units == DELAY_HOURS)
    {
        delaySpec->tv_sec = 60 * 60 * delay;
        delaySpec->tv_nsec = 0;
    }
    else if (units == DELAY_MINS)
    {
        delaySpec->tv_sec = 60 * delay;
        delaySpec->tv_nsec = 0;
    }
    else if (units == DELAY_SECS)
    {
        delaySpec->tv_sec = delay;
        delaySpec->tv_nsec = 0;
    }
    else
    {
        // Convert from millis
        delaySpec->tv_sec = delay / 1000;
        delaySpec->tv_nsec = (delay % 1000) * 1000000;
    }
}

/*
 * Helper function for creating all different tasks types
 */
static uint32_t initRepeatingTask(uint64_t delayAmount, uint64_t initDelayAmount, uint64_t maxDelayAmount, uint64_t incrementDelayAmount, delayUnits units,
                              taskCallbackFunc func, backOffTaskRunCallbackFunc backOffRunFunc, backOffTaskSuccessCallbackFunc backOffSuccessFunc, void *arg, repeatingTaskType taskType)
{
    // sanity checks
    //
    if (taskType == NORMAL_REPEATING_TASK_TYPE || taskType == FIXED_RATE_REPEATING_TASK_TYPE)
    {
        if (delayAmount == 0 || func == NULL)
        {
            return 0;
        }
    }
    else if (taskType == BACK_OFF_REPEATING_TASK_TYPE)
    {
        if (incrementDelayAmount == 0 || initDelayAmount == 0 || maxDelayAmount == 0 || backOffRunFunc == NULL)
        {
            return 0;
        }
    }
    else
    {
        // unknown type
        return 0;
    }

    // create and populate a task definition
    //
    repeatTask *def = (repeatTask *)calloc(1, sizeof(repeatTask));
    def->state = REPEAT_TASK_IDLE;
    def->taskType = taskType;
    def->callback = func;
    def->backOffRunCallback = backOffRunFunc;
    def->backOffSuccessCallback = backOffSuccessFunc;
    def->arg = arg;
    def->currentDelay = delayAmount;
    def->units = units;
    def->incrementDelay = incrementDelayAmount;
    def->startDelay = initDelayAmount;
    def->maxDelay = maxDelayAmount;
    def->didComplete = false;

    // We run the first iteration immediately
    //
    getCurrentTime(&def->targetRunTime, true);
    pthread_mutex_init(&def->mutex, NULL);
    initTimedWaitCond(&def->cond);

    // assign a handle and add to our list
    //
    pthread_mutex_lock(&TASK_MTX);
    if (tasks == NULL)
    {
        tasks = linkedListCreate();
    }
    uint32_t retVal = assignHandle();
    def->handle = retVal;
    linkedListAppend(tasks, def);
    pthread_mutex_unlock(&TASK_MTX);

    // create and start the thread
    //
    char *threadName = stringBuilder("rptTask:%"PRIu32, retVal);
    createThread(&def->tid, runRepeatTaskThread, def, threadName);
    free(threadName);

    return retVal;
}

/*
 * cancel the repeating task. returns the original 'arg' supplied
 * during the creation - allowing cleanup to safely occur.
 * WARNING:
 * this should NOT be called while holding a lock that the task
 * function can also hold or a deadlock might result
 */
void *cancelRepeatingTask(uint32_t task)
{
    // find the task for this handle
    //
    pthread_mutex_lock(&TASK_MTX);
    repeatTask *obj = (repeatTask *)linkedListFind(tasks, &task, findRepeatTaskByHandle);
    if (obj == NULL)
    {
        // task is not in the list anymore.  possible that
        // it's currently executing or already canceled.
        //
        pthread_mutex_unlock(&TASK_MTX);
        return NULL;
    }
    else
    {
        // now remove the task from our list.  This way we can release the TASK_MTX before
        // we do the business of canceling the task
        //
        linkedListDelete(tasks, &task, findRepeatTaskByHandle, standardDoNotFreeFunc);

        // if the list is empty, release it (could be we're shutting down)
        //
        if (linkedListCount(tasks) == 0)
        {
            linkedListDestroy(tasks, standardDoNotFreeFunc);
            tasks = NULL;
        }
        pthread_mutex_unlock(&TASK_MTX);
    }

    // look at the state to see if 'WAITING' or 'RUNNING'
    //
    void *retVal = NULL;
    pthread_mutex_lock(&obj->mutex);
    if (obj->state != REPEAT_TASK_CANCELED)
    {
        // set state to cancel, then pop the condition
        // (in case we're in the 'wait' state)
        //
        obj->state = REPEAT_TASK_CANCELED;
        pthread_cond_broadcast(&obj->cond);

        // return the 'arg' from within this task
        //
        retVal = obj->arg;
        pthread_mutex_unlock(&obj->mutex);

        // wait for the thread to die, so we know it's safe to delete
        //
        pthread_join(obj->tid, NULL);

        // One last sanity check, if the task completed while we were waiting on it don't return the arg, since the
        // task has already free'd it.
        pthread_mutex_lock(&obj->mutex);
        if (obj->didComplete == true)
        {
            retVal = NULL;
        }
        pthread_mutex_unlock(&obj->mutex);
    }
    else
    {
        // already 'canceling'
        //
        pthread_mutex_unlock(&obj->mutex);
    }

    // Destroy the task
    releaseRepeatTask(obj);

    return retVal;
}

/*
 * cancels the pause, forcing the task to loop around and execute
 */
void shortCircuitRepeatingTask(uint32_t task)
{
    // find the task with this handle
    //
    pthread_mutex_lock(&TASK_MTX);
    repeatTask *obj = (repeatTask *)linkedListFind(tasks, &task, findRepeatTaskByHandle);
    pthread_mutex_unlock(&TASK_MTX);
    if (obj != NULL)
    {
        // lock this task, then broadcast to stop the 'wait'
        //
        pthread_mutex_lock(&obj->mutex);
        if (obj->state != REPEAT_TASK_CANCELED)
        {
            // Technically we could set targetRunTime to now, but that would throw off
            // fixed rate scheduling, so we we add a short circuit state to handle this
            obj->state = REPEAT_TASK_SHORT_CIRCUIT;
            pthread_cond_broadcast(&obj->cond);
        }
        pthread_mutex_unlock(&obj->mutex);
    }
}

/*
 * change the delayAmount for a repeating task.  if the 'changeNow' flag
 * is true, then it will reset the current pause time and start again.  otherwise
 * this will not apply until the next pause.
 */
void changeRepeatingTask(uint32_t task, uint32_t delayAmount, delayUnits units, bool changeNow)
{
    if (delayAmount == 0)
    {
        return;
    }

    // find the task with this handle
    //
    pthread_mutex_lock(&TASK_MTX);
    repeatTask *obj = (repeatTask *)linkedListFind(tasks, &task, findRepeatTaskByHandle);
    pthread_mutex_unlock(&TASK_MTX);
    if (obj != NULL)
    {
        // lock this task, then change the delay values
        //
        pthread_mutex_lock(&obj->mutex);
        if (obj->state != REPEAT_TASK_CANCELED)
        {
            // store the new delay amount
            //
            obj->currentDelay = (uint64_t) delayAmount;
            obj->units = units;

            // If the task itself is calling us, we don't want to modify the targetRunTime as the run task thread
            // is about to modify it as well.
            if (obj->state != REPEAT_TASK_RUNNING)
            {
                // Recalculate the target run time relative to now
                //
                struct timespec delay;
                struct timespec now;
                populateTimespecDelay(delayAmount, units, &delay);

                getCurrentTime(&now, true);
                timespecAdd(&now, &delay, &(obj->targetRunTime));
            }

            if (changeNow == true)
            {
                // force it to break from the wait
                pthread_cond_broadcast(&obj->cond);
            }
        }
        pthread_mutex_unlock(&obj->mutex);
    }
}

/*
 * calculate the amount of time to wait.  assumes
 * caller has the task->lock
 */
static void calcTimeToPause(struct timespec *executeTime, struct timespec *pauseTime)
{
    struct timespec now;
    getCurrentTime(&now, true);
    // remaining = targetRunTime - now
    timespecDiff(executeTime, &now, pauseTime);
}

/*
 * thread to execute in
 */
static void *runRepeatTaskThread(void *arg)
{
    // get the taskDef parameters
    //
    repeatTask *def = (repeatTask *)arg;
    bool toldToCancel = false;
    repeatingTaskType taskType = def->taskType;

    // loop until told to stop
    //
    while (toldToCancel == false)
    {
        repeatTaskState taskEntryState;
        // check our state to see if told to cancel
        //
        pthread_mutex_lock(&def->mutex);
        if (def->state == REPEAT_TASK_CANCELED)
        {
            toldToCancel = true;
            pthread_mutex_unlock(&def->mutex);
            continue;
        }

        // not canceled yet, so perform our operation
        //
        taskEntryState = def->state;
        def->state = REPEAT_TASK_RUNNING;
        pthread_mutex_unlock(&def->mutex);

        // run a different operation for back off tasks
        //
        if (taskType == BACK_OFF_REPEATING_TASK_TYPE)
        {
            if ((def->backOffRunCallback(def->arg)) == true)
            {
                // since operation returned true, end the task
                //
                break;
            }
        }
        else
        {
            def->callback(def->arg);
        }

        // Calculate when we should run next
        //
        pthread_mutex_lock(&def->mutex);

        // determine the new delay amount for back off tasks
        //
        if (taskType == BACK_OFF_REPEATING_TASK_TYPE)
        {
            def->currentDelay = calcBackOffDelayAmount(def->currentDelay, def->startDelay, def->incrementDelay, def->maxDelay);
        }

        // convert current delay amount into time
        //
        struct timespec delay;
        populateTimespecDelay(def->currentDelay, def->units, &delay);
        if (taskType != FIXED_RATE_REPEATING_TASK_TYPE || taskEntryState == REPEAT_TASK_SHORT_CIRCUIT)
        {
            struct timespec now;
            getCurrentTime(&now, true);

            // We want to run "delay" time after now
            //
            timespecAdd(&now, &delay, &(def->targetRunTime));
        }
        else
        {
            // We want to run "delay" time after the last time we ran
            //
            timespecAdd(&(def->targetRunTime), &delay, &(def->targetRunTime));
        }

        // check if told to cancel (again)
        //
        if (def->state != REPEAT_TASK_CANCELED && def->state != REPEAT_TASK_SHORT_CIRCUIT)
        {
            // update state to 'waiting', then pause
            //
            def->state = REPEAT_TASK_WAITING;

            // figure out the pause time
            //
            struct timespec timeToPause;
            calcTimeToPause(&(def->targetRunTime), &timeToPause);

            // Handle spurious wake-ups by doing this in a loop
            //
            while (timeToPause.tv_sec > 0 || (timeToPause.tv_sec == 0 && timeToPause.tv_nsec >= 0))
            {
                // now pause
                //
                granularIncrementalCondTimedWait(&def->cond, &def->mutex, &timeToPause);

                // check to see if told to wake up
                //
                if (def->state == REPEAT_TASK_CANCELED)
                {
                    toldToCancel = true;
                    break;
                }
                else if (def->state == REPEAT_TASK_SHORT_CIRCUIT)
                {
                    break;
                }

                // figure out new pause time
                //
                calcTimeToPause(&(def->targetRunTime), &timeToPause);
            }
        }
        pthread_mutex_unlock(&def->mutex);
    }

    // if this was a back-off task, need to cleanup the 'arg'
    //
    if (taskType == BACK_OFF_REPEATING_TASK_TYPE)
    {
        // let the callback perform any task cleanup
        //
        if (def->backOffSuccessCallback != NULL)
        {
            def->backOffSuccessCallback(def->arg);
        }

        // if our state is already "CANCEL", then something is trying to remove us.
        // otherwise, change the state to "CANCEL" and delete the task from the list
        //
        if (toldToCancel == false)
        {
            // need to set the state to CANCEL before calling 'cancel', or it will try to join on this thread
            //
            pthread_mutex_lock(&def->mutex);
            def->state = REPEAT_TASK_CANCELED;
            uint32_t delHandle = def->handle;
            // Mark this so if there is another thread trying to do a cancel right now they know not to free the arg
            def->didComplete = true;
            pthread_mutex_unlock(&def->mutex);

            cancelRepeatingTask(delHandle);
        }
    }

    return NULL;
}

/*
 * release the task
 */
static void releaseRepeatTask(repeatTask *task)
{
    if (task != NULL)
    {
        // destroy mutex and the container.
        // NOTE: not doing anything with the task's "arg"
        //
        pthread_mutex_destroy(&task->mutex);
        pthread_cond_destroy(&task->cond);
        free(task);
    }
}

/*
 * 'linkedListCompareFunc' to find a delayTask object from our 'tasks' list,
 * using the 'handle' as the search criteria.
 */
static bool findRepeatTaskByHandle(void *searchVal, void *item)
{
    uint32_t *id = (uint32_t *)searchVal;
    repeatTask *task = (repeatTask *)item;

    // comparet he handle values
    //
    if (*id == task->handle)
    {
        return true;
    }
    return false;
}

/*
 * internal call, assumes LIST_MTX is held
 */
static uint32_t assignHandle()
{
    // Don't overflow, ensures we never use 0
    if (handleCounter == UINT32_MAX)
    {
        handleCounter = 0;
    }

    return ++handleCounter;
}

