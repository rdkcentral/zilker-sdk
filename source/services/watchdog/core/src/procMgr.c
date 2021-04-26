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

/*
 * procMgr.c
 *
 *  Created on: Apr 4, 2011
 *      Author: gfaulkner
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <sys/resource.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icIpc/ipcStockMessages.h>
#include <icTypes/icLinkedList.h>
#include <icTime/timeUtils.h>
#include <icConcurrent/timedWait.h>
#include <icConcurrent/delayedTask.h>
#include <icUtil/stringUtils.h>
#include <propsMgr/logLevel.h>
#include <watchdog/serviceStatsHelper.h>
#include <watchdog/watchdogChild.h>
#ifdef CONFIG_SERVICE_DIAGNOSTIC
#include <diagService/softwareTroubleHelper.h>
#endif

#include "procMgr.h"
#include "configParser.h"
#include "broadcastEvent.h"
#include "common.h"
#include "systemStatsCollector.h"
#include "watchdogService_ipc_handler.h"
#include "ipcHandler.h"

#define DEATH_WAIT_SECS             10
#define SINGLE_PHASE_STARTUP_WAIT_SECS 60
#define START_INIT_TIMEOUT_SECS 30

/* Use for Linux 2.6.11 (tca2xx) */
#define OOM_ADJ_PROC_ENTRY "oom_adj"
/* Make a task unkillable */
#define OOM_ADJ_DISABLE "-17"

/* For Linux 2.6.36 and later */
#define OOM_SCORE_ADJ_PROC_ENTRY "oom_score_adj"
/* Make a task unkillable */
#define OOM_SCORE_ADJ_DISABLE "-1000"

#define RESET_ACK       1U << 0U
#define FORCE_KILL      1U << 1U
#define IGNORE_DEATH    1U << 2U
#define DUMP_CORE       1U << 3U

typedef enum
{
    ALL_PROC_FILTER,
    SINGLE_PROC_FILTER,
    GROUP_PROC_FILTER,
    SINGLE_PHASE_FILTER,
    NON_SINGLE_PHASE_FILTER
} scopeFilter;


/*
 * private functions
 */

// 'search' function for LinkedList
static bool findServiceByNameInList(void *searchVal, void *item);
static bool findServiceByPidInList(void *searchVal, void *item);

// internal start/stop
static bool executeStartOperation(char *name, scopeFilter scope);

static bool executeStopOperation(const char *name, scopeFilter scope, uint8_t flags);

static void handleDeadChild(pid_t childPid);

// startup sequence functions
static void startMonitorAckThread();
static void allAcksReceived(bool gaveUp);
static uint16_t internalCountServicesToBeAcknowledged();
static uint16_t internalCountSinglePhaseServicesToBeAcknowledged();
static void monitorAckDelayCallback(void *arg);
static void resetBadServiceDelayCallback(void *arg);
static void performStartupSequence(icLinkedList *serviceNames);

#ifdef CONFIG_DEBUG_SINGLE_PROCESS
// function from main.c
bool getServiceState(char *serviceName);
#endif

/*
 * private variables
 */

#ifdef CONFIG_DEBUG_SINGLE_PROCESS
icLinkedList *managerList = NULL;   // allow visibility to main.c
#else
static icLinkedList *managerList = NULL;
#endif
static pthread_mutex_t SERVICE_MTX = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  SERVICE_COND = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  SINGLE_PHASE_START_COMPLETE_COND = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t ACK_MONITOR_MTX = PTHREAD_MUTEX_INITIALIZER;
static uint32_t monitorAckTask = 0;
static uint32_t resetBadServiceTask = 0;
static bool startupSequenceFinalized = false;

#ifndef _GNU_SOURCE
extern char **environ;
#endif

/**
 * called to actually start a process; this is a callback from configParser.c
 * assumes it is safe to access or change data stored in 'procDef' (in other
 * words, there is a mutex in place for it)
 */
#ifndef CONFIG_DEBUG_SINGLE_PROCESS
void startProcess(serviceDefinition *procDef, bool restartAfterCrash)
{
    int skipFD = -1;
    struct rlimit fdlim;
    uint16_t fdmax = 1024;

    if (getrlimit(RLIMIT_NOFILE, &fdlim) == 0)
    {
        fdmax = fdlim.rlim_cur;
    }

    /*
     * Do not setenv() after fork(); clone the environment and add to it if required.
     * TODO: Supply a standard environment instead. Accessing the calling thread environ is intrinsically
     * unsafe. This is okay for now, as setenv() is not called concurrently.
     */
#ifndef CONFIG_OS_DARWIN
    char **tmp = environ;
    while (*tmp != NULL)
    {
        tmp++;
    }

    size_t envMembers = tmp - environ;
    char **env = malloc((envMembers + 1) * sizeof(char *));

    for (size_t i = 0; i < envMembers; i++)
    {
        env[i] = strdup(environ[i]);
    }

    if (restartAfterCrash == true)
    {
        // set the 'zilkerChildRestarted=true' environment variable
        //
        envMembers++;
        env = realloc(env, (envMembers + 1) * sizeof(char *));
        env[envMembers - 1] = strdup(CHILD_WAS_RESTARTED_ENV_VAR "=true");
    }
    env[envMembers] = NULL;
#endif  // not CONFIG_OS_DARWIN

    // before we begin, reset the "received ack" time
    // so that we can easily detect if/when the process
    // sends us the acknowledgement.
    //
    procDef->lastActReceivedTime = 0;

    // perform the fork
    //
    pid_t pid = fork();
    if (pid < 0)
    {
        icLogWarn(WDOG_LOG, "Unable to start %s: fork failed (%s)", procDef->serviceName, strerror(errno));
#ifndef CONFIG_OS_DARWIN
        free(env);
#endif
        return;
    }

    if (pid > 0)
    {
        // parent process
        //
#ifndef CONFIG_OS_DARWIN
        free(env);
#endif
        icLogInfo(WDOG_LOG, "Process %s (%s) started as pid %d; restartAfterCrash=%s", procDef->serviceName,
                             procDef->execPath, pid, (restartAfterCrash == true) ? "true" : "false");
        procDef->currentPid = pid;

        // save off the time this started (in time_t format)
        //
        procDef->lastRestartTime = getCurrentTime_t(false);
        procDef->lastRestartTimeMono = getCurrentTime_t(true);
        return;
    }

    // Child process
    // Only functions that are listed in `man 7 signal-safety` may be called from here on.
    //
    // Close all fds above stdio to release any resources opened without FD_CLOEXEC,
    // and exec the given process.
    //
    for (int cnt = 3; cnt < fdmax; cnt++)
    {
        if (cnt != skipFD)
        {
            close(cnt);
        }
    }

    // child process, so execute the command.
    //
#ifndef CONFIG_OS_DARWIN
    execve(procDef->execPath, procDef->execArgs, env);
#else
    execv(procDef->execPath, procDef->execArgs);
#endif

    exit(1);
}
#endif // end !CONFIG_DEBUG_SINGLE_PROCESS

/**
 * starts all configured processes and then waits for one of them to die, and handles that.
 * this function DOES NOT return and is generally called from the main thread.
 */
void startConfiguredProcessesAndWait(const char *configDir, const char *homeDir)
{
    pthread_mutex_lock(&SERVICE_MTX);
    initTimedWaitCond(&SERVICE_COND);
    initTimedWaitCond(&SINGLE_PHASE_START_COMPLETE_COND);

    // load our config and save as our master list
    //
    managerList = loadServiceConfig(configDir, homeDir);
    if (managerList == NULL)
    {
        icLogError(WDOG_LOG, "error loading config.  exiting");
        pthread_mutex_unlock(&SERVICE_MTX);
        return;
    }

    // now that all services have been read, see if we have a misbehaving
    // service file stored from before our last boot.
    //
    char *badServiceName = readMisbehavingService();
    if (badServiceName != NULL)
    {
        // now find this serviceName in the linkedList we just created.
        // the idea is to switch out the current "reboot action" with "ignore"
        // to prevent a reboot-loop due to this misbehaving process.
        //
        serviceDefinition *found = (serviceDefinition *) linkedListFind(managerList, badServiceName, findServiceByNameInList);
        if (found != NULL && found->actionOnMaxRestarts == REBOOT_ACTION)
        {
            // found our culprit
            //
            icLogInfo(WDOG_LOG, "temporarily setting service %s to NOT reboot on failure for 1 hour", badServiceName);
            found->actionOnMaxRestarts = STOP_RESTARTING_ACTION;

            // now start a timer to restore this service's "reboot action" after
            // an hour or so (vs leaving it at "ignore")
            //
            resetBadServiceTask = scheduleDelayTask(1, DELAY_HOURS, resetBadServiceDelayCallback, badServiceName);
            if (resetBadServiceTask > 0)
            {
                // don't free badServiceName since we gave it to the delayed task
                //
                badServiceName = NULL;
            }
        }
        free(badServiceName);
    }

    // perform the startup sequence
    //
    performStartupSequence(NULL);
    pthread_mutex_unlock(&SERVICE_MTX);

#ifndef CONFIG_DEBUG_SINGLE_PROCESS
    // wait for children processes to die
    //
    while(1)
    {
        int statLoc;
        pid_t deadChild = wait(&statLoc);
        if (deadChild > 0)
        {
            if (WIFEXITED(statLoc))
            {
                int status = WEXITSTATUS(statLoc);
                if (status == 0)
                {
                    icLogTrace(WDOG_LOG, "Process %d exited normally", deadChild);
                }
                else
                {
                    icLogWarn(WDOG_LOG, "Process %d terminated (exited); exit status %d",deadChild,status);
                }
            }

            else if (WIFSIGNALED(statLoc))
            {
                icLogWarn(WDOG_LOG, "Process %d terminated (signal); termination signal %d",deadChild,WTERMSIG(statLoc));
            }
            else
            {
                icLogWarn(WDOG_LOG, "Process %d terminated for unknown reason (0x%X)",deadChild,statLoc);
            }

            // pass along to configParser
            //
            handleDeadChild(deadChild);
        }
        else if (deadChild == -1 && errno == ECHILD)
        {
            if (is_watchdogService_ipc_handler_shutdown() == true) // Check if ipc receiver is also shutdown
            {
                icLogInfo(WDOG_LOG, "All children have exited and we are shutting down.");
                break;
            }
            else
            {
                //we need to delay here or we will likely be in a tight loop.  since our ipc receiver is
                // still running, we are not shutting down.  We have likely been asked to stop all services
                // for RMA or some other situation and we expect our children to return.
                icLogTrace(WDOG_LOG, "All children have exited but we are still running.");
                sleep(5);
            }
        }
    }
#else
    // running in IDE, so wait for IPC to shutdown
    //
    IpcReceiver receiver = get_watchdogService_ipc_receiver();
    if (receiver != NULL)
    {
        waitForRequestHandlerToShutdown(receiver);
    }
#endif
}

/*
 * function to compare a name (searchVal) to the name
 * within a 'serviceDefinition'.
 * done this way so it can be supplied as a function
 * to the LinkedList object.
 */
static bool findServiceByNameInList(void *searchVal, void *item)
{
    char *searchStr = (char *)searchVal;
    serviceDefinition *mgr = (serviceDefinition *)item;

    if (strcmp(searchStr, mgr->serviceName) == 0)
    {
        // found match
        //
        return true;
    }

    return false;
}

/*
 * search the linked list for a serviceDefinition with a matching 'pid'
 */
static bool findServiceByPidInList(void *searchVal, void *item)
{
    int *pid = (int *)searchVal;
    serviceDefinition *mgr = (serviceDefinition *)item;

    if (*pid == mgr->currentPid)
    {
        // found match
        //
        return true;
    }

    return false;
}

/*
 * loop through and start 1 or more definitions
 * internal so assumes caller already has the lock
 */
static bool executeStartOperation(char *name, scopeFilter scope)
{
    // create a LinkedList Iterator
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(managerList);
    bool retVal = false;

    // start looping through all items in our list
    //
    while (linkedListIteratorHasNext(loop) == true)
    {
        serviceDefinition *curr = linkedListIteratorGetNext(loop);
        bool matches = false;

        // skip ones that do not have a binary path defined (java services for example)
        //
        if (stringIsEmpty(curr->execPath) == true)
        {
            icLogDebug(WDOG_LOG, "skipping launching service '%s', it has no binary defined", curr->serviceName);
            continue;
        }

        // look at the scope to see how we see if the filter passes
        //
        switch(scope)
        {
            case ALL_PROC_FILTER:
                // all processes, so generally passes the filter
                // however during start, look for the definitions
                // with 'autostart' set
                //
                if (curr->autoStart == true)
                {
                    matches = true;
                }
                break;

            case SINGLE_PROC_FILTER:
                // single process, look for name match
                //
                if (strcmp(name, curr->serviceName) == 0)
                {
                    matches = true;
                }
                break;

            case GROUP_PROC_FILTER:
                // group of processes, see if this is in the group
                //
                if (curr->logicalGroup != NULL && strcmp(name, curr->logicalGroup) == 0)
                {
                    matches = true;
                }
                break;

            case SINGLE_PHASE_FILTER:
                if (curr->singlePhaseStartup == true)
                {
                    matches = true;
                }
                break;

            case NON_SINGLE_PHASE_FILTER:
                if (curr->singlePhaseStartup == false && curr->autoStart == true)
                {
                    matches = true;
                }
                break;
        }

        // skip if this definition didn't pass the filter
        //
        if (matches == false)
        {
            continue;
        }

        // skip if this process is already running
        //
        if(curr->currentPid > 0)
        {
            continue;
        }
        // start the process
        // first, reset the 'ignore' flag
        //
        curr->tempIgoreDeath = false;
        icLogDebug(WDOG_LOG, "Starting process %s", curr->serviceName);

        // do the start, and pause slightly before going to the next
        //
        retVal = true;
        startProcess(curr, false);
        usleep(300);
    }

    // mem cleanup and return
    //
    linkedListIteratorDestroy(loop);

    return retVal;
}

#ifndef CONFIG_DEBUG_SINGLE_PROCESS
/*
 * Try to determine if a particular process id is still alive
 */
static bool isProcessAlive(pid_t pid)
{

    // Kill with signal 0 will help us know whether the process is alive
    int rc = kill(pid, 0);
    if (rc == 0)
    {
        // Success return means its alive
        return true;
    }
    // Error return with a permission error also means its alive
    else if (rc == -1 && errno == EPERM)
    {
        return true;
    }

    // Otherwise, its dead Jim
    return false;
}
#endif

static bool waitForDeath(pid_t pid, serviceDefinition *target)
{
    // wait up to 5 seconds for the service to die
    //
    int checkCount = 0;
#ifndef CONFIG_DEBUG_SINGLE_PROCESS
    while (isProcessAlive(pid) && checkCount < DEATH_WAIT_SECS)
    {
        // since we're blocked on the mutex, allow the 'handleDeadChild' a
        // chance at processing the signal of the proc dying.
        //
        incrementalCondTimedWait(&SERVICE_COND, &SERVICE_MTX, 1);
#else
    while (getServiceState(target->serviceName) == false && checkCount < DEATH_WAIT_SECS)
    {
        sleep(1);
#endif
        checkCount++;
    }

    if (checkCount >= 5)
    {
        return false;
    }
    return true;
}

/*
 * loop through and mark all services as 'ignore death'.
 * internal so assumes caller already has the lock
 */
static void ignoreAllProcDeaths()
{
    icLinkedListIterator *loop = linkedListIteratorCreate(managerList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        serviceDefinition *curr = linkedListIteratorGetNext(loop);
        if (curr->currentPid > 0)
        {
            curr->tempIgoreDeath = true;
        }
    }
    linkedListIteratorDestroy(loop);
}

/*
 * loop through and stop 1 or more definitions
 * internal so assumes caller already has the lock
 */
static bool executeStopOperation(const char *name, scopeFilter scope, uint8_t flags)
{
    bool resetAckFlag   = (flags & RESET_ACK)   != 0;
    bool forceKill      = (flags & FORCE_KILL)  != 0;
    bool dumpCore       = (flags & DUMP_CORE)   != 0;
    bool ignoreDeath    = (flags & IGNORE_DEATH)!= 0;

#ifndef CONFIG_DEBUG_SINGLE_PROCESS
    // if stopping ALL processes, first mark them all as 'ignore death' in case
    // one of them dies before we attempt to ask it to die (prevent thrashing during shutdown)
    //
    if (scope == ALL_PROC_FILTER)
    {
        ignoreAllProcDeaths();
    }
#endif

    // create a LinkedList Iterator
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(managerList);
    bool retVal = false;

    // start looping through all items in our list
    //
    while (linkedListIteratorHasNext(loop) == true)
    {
        serviceDefinition *curr = linkedListIteratorGetNext(loop);
        bool matches = false;

#ifndef CONFIG_DEBUG_SINGLE_PROCESS
        // skip anything that doesn't have a PID
        //
        if (curr->currentPid <= 0)
        {
            continue;
        }
#endif

        // look at the scope to see how we see if the filter passes
        //
        switch(scope)
        {
            case ALL_PROC_FILTER:
                // all processes, so nothing to compare
                //
                matches = true;
                break;

            case SINGLE_PROC_FILTER:
                // single process, look for name match
                //
                if (strcmp(name, curr->serviceName) == 0)
                {
                    matches = true;
                }
                break;

            case GROUP_PROC_FILTER:
                // group of processes, see if this is in the group
                //
                if (curr->logicalGroup != NULL && strcmp(name, curr->logicalGroup) == 0)
                {
                    matches = true;
                }
                break;

            case SINGLE_PHASE_FILTER:
                // Single phase startup only
                if (curr->singlePhaseStartup == true)
                {
                    matches = true;
                }
                break;

            case NON_SINGLE_PHASE_FILTER:
                // Non single phase startup
                if (curr->singlePhaseStartup == false)
                {
                    matches = true;
                }
                break;
        }

        // skip if this definition didn't pass the filter
        //
        if (matches == false)
        {
            continue;
        }

        // set the 'ignore' flag, then kill the process
        // first, reset the 'ignore' flag
        //
        icLogInfo(WDOG_LOG, "Stopping service %s", curr->serviceName);
        curr->tempIgoreDeath = ignoreDeath;

        // potentially reset this process' 'lastAck' time
        //
        if (resetAckFlag == true)
        {
            curr->lastActReceivedTime = 0;
        }

#ifdef CONFIG_DEBUG_SINGLE_PROCESS
        // make the code below use IPC since we cannot do a 'kill'
        forceKill = false;
        dumpCore = false;
#endif

        // kill the process.
        //
        bool isDead = false;

        // Keep track of our pid before we do the stop.  Previously we could rely on detecting the process died
        // by waiting for its currentPid to be set to 0. But, when RESTART_FOR_RECOVERY was added, the process can
        // be stopped, but the automatic restart mechanism will restart it.  In this case the currentPid can be
        // updated to the new pid while we are in the midst of stopping.  This causes us to then proceed to more
        // forcefully kill the new process that was started.  Now waitForDeath() takes the old pid and waits for
        // signs that the old pid is gone to know the process died
        pid_t pid = curr->currentPid;
        if (forceKill == false && dumpCore == false)
        {
            // try the 'nice' way first
            //
            if (curr->shutdownToken != NULL)
            {
                // by default, use 5 second timeout for nice shutdown
                //
                time_t timeoutSecs = 5;
                if (curr->waitSecsOnShutdown > 0)
                {
                    // no timeout.  let this service take as-long as-needed
                    timeoutSecs = curr->waitSecsOnShutdown;
                }

                // ask the service to shutdown via IPC
                //
                icLogDebug(WDOG_LOG, "attempting to stop service %s via IPC (%ld second timeout)...", curr->serviceName, timeoutSecs);
                if (requestServiceShutdown((uint16_t) curr->serviceIpcPort, curr->shutdownToken, timeoutSecs) == IPC_SUCCESS)
                {
                    // wait up to 5 seconds for the service to die.  note this should also work
                    // for long-running shutdowns as we received the IPC response of SUCCESS
                    //
                    isDead = waitForDeath(pid, curr);
                }

                // handle scenario where IPC failed, yet process actually died
                //
                if (curr->currentPid <= 0)
                {
                    isDead = true;
                }
            }
        }

#ifndef CONFIG_DEBUG_SINGLE_PROCESS
        // regardless of IPC being sent or not, also go
        // the old-fashion route by sending a SIGTERM
        // When doing an emergency stop with dumpCore, send SIGQUIT first for diagnostics
        //
        int signal = SIGTERM;
        if (dumpCore == true)
        {
            signal = SIGQUIT;
        }

        if (isDead == false && curr->currentPid > 0)
        {
            icLogDebug(WDOG_LOG, "attempting to stop service %s (%d) via signal %d...", curr->serviceName, curr->currentPid, signal);
            kill(curr->currentPid, signal);
            isDead = waitForDeath(pid, curr);
        }

        // if still alive, use -9
        //
        if (isDead == false && curr->currentPid > 0)
        {
            icLogDebug(WDOG_LOG, "attempting to stop service %s (%d) via SIGKILL...", curr->serviceName, curr->currentPid);
            kill(curr->currentPid, SIGKILL);
        }
#endif

        icLogDebug(WDOG_LOG, "done stopping service %s", curr->serviceName);
        retVal = true;
    }

    // mem cleanup and return
    //
    linkedListIteratorDestroy(loop);
    return retVal;
}

/*
 * return a list of strings, composed of the names of
 * every process in our list that has a valid pid OR
 * has the autoStart flag set.  used specifically for
 * the "restart all processes" to ensure things that are
 * running (or should be) are brought back.  caller must
 * free the returned list and it's elements.
 */
static icLinkedList *getServiceNamesToBounce()
{
    icLinkedList *retVal = linkedListCreate();

    // start looping through all items in our list
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(managerList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // only care about running processes AND ones that should be running
        //
        serviceDefinition *curr = linkedListIteratorGetNext(loop);
        if (curr->serviceName != NULL && (curr->currentPid > 0 || curr->autoStart == true))
        {
            linkedListAppend(retVal, strdup(curr->serviceName));
        }
    }

    // mem cleanup and return
    //
    linkedListIteratorDestroy(loop);
    return retVal;
}

/**
 * starts, stops, or restarts all known services
 */
void operationOnAllProcesses(operationAction action, bool forceKill)
{
    // obtain lock
    //
    pthread_mutex_lock(&SERVICE_MTX);

    // NOTE: we do not send events from here as operations on "all" processes
    //       is generally done during init or shutdown - and we will let the
    //       calling function deal with the event
    //

    // perform "all" action
    //
    if (action == START_PROCESS_ACTION)
    {
        // assuming everything is down, run through our startup sequence
        //
        performStartupSequence(NULL);
    }
    else if (action == STOP_PROCESS_ACTION)
    {
        int flags = IGNORE_DEATH;
        if (forceKill == true)
        {
            flags |= FORCE_KILL;
        }

        executeStopOperation(NULL, ALL_PROC_FILTER, flags);
    }
    else if (action == RESTART_PROCESS_ACTION)
    {
        // before we begin, need to get the set of processes that are
        // currently running.  this was a problem on XB6 where depending
        // on our mode (battery or touchstone) we may have things started
        // via a group vs autoStart flag
        //
        icLinkedList *runningList = getServiceNamesToBounce();

        // note: when stopping the processes we'll wipe the 'lastAck' value
        //       to reproduce the same steps as watchdog startup.  this way
        //       the services still have to wait for the "WATCHDOG_INIT_COMPLETE" event
        //       before finalizing their initialization
        //
        int flags = RESET_ACK | IGNORE_DEATH;
        if (forceKill == true)
        {
            flags |= FORCE_KILL;
        }

        executeStopOperation(NULL, ALL_PROC_FILTER, flags);

        // Intentionally hold lock and synchronously wait to avoid multiple startups
        // coverity[sleep]
        struct timespec sleepTime = { .tv_sec = 2, .tv_nsec = 0 };
        if (nanosleep(&sleepTime, NULL) == 0)
        {
            // now perform the startup sequence, but only on the runningList
            //
            performStartupSequence(runningList);
        }
        else
        {
            icLogWarn(WDOG_LOG, "Cancelling restart: %s", strerror(errno));
        }

        linkedListDestroy(runningList, NULL);
    }

    // release lock
    //
    pthread_mutex_unlock(&SERVICE_MTX);
}

/**
 * starts, stops, or restarts a single processes with matching 'serviceName'
 * returns true if the operation is executed successfully, false otherwise.
 */
bool operationOnSingleProcesses(operationAction action, char *serviceName)
{
    bool rc = false;
    // first check for serviceName, if it is watchDog then return false.
    //
    if (stringCompare(serviceName,WATCH_DOG_SERVICE_NAME,true) == 0)
    {
        icLogWarn(WDOG_LOG, "%s: Not allowed to process for %s, returning false", __FUNCTION__,serviceName);
        return false;
    }
    // obtain lock
    //
    pthread_mutex_lock(&SERVICE_MTX);

    // perform "single" action
    //
    if (action == START_PROCESS_ACTION)
    {
        // do the start, then send the event
        //
        if (executeStartOperation(serviceName, SINGLE_PROC_FILTER) == true)
        {
            broadcastWatchdogEvent(WATCHDOG_SERVICE_STATE_CHANGED, WATCHDOG_EVENT_VALUE_ACTION_START, (const char *)serviceName);
            rc = true;
        }
    }
    else if (action == STOP_PROCESS_ACTION)
    {
        // stop then send the event (since the 'handle death' callback won't send
        // the event as we're doing this on purpose)
        //
        if (executeStopOperation(serviceName, SINGLE_PROC_FILTER, IGNORE_DEATH) == true)
        {
            broadcastWatchdogEvent(WATCHDOG_SERVICE_STATE_CHANGED, WATCHDOG_EVENT_VALUE_ACTION_DEATH, (const char *)serviceName);
            rc = true;
        }
    }
    else if (action == RESTART_PROCESS_ACTION || action == RESTART_FOR_RECOVERY_PROCESS_ACTION)
    {
        // When we restart for recovery we want to treat the death like a normal death(send a death event, only restart
        // if we haven't exceeded the max restarts, etc).
        uint8_t flags = 0;
        if (action == RESTART_PROCESS_ACTION)
        {
            flags |= IGNORE_DEATH;
        }

        if (action == RESTART_FOR_RECOVERY_PROCESS_ACTION)
        {
            flags |= (DUMP_CORE | FORCE_KILL);
        }

        // stop, then start, then send the event
        //
        if ((executeStopOperation(serviceName, SINGLE_PROC_FILTER, flags) == true) && action == RESTART_FOR_RECOVERY_PROCESS_ACTION)
        {
            rc = true;
        }
        // For recovery case, the process death should trigger the restart, if its still allowed
        if (action == RESTART_PROCESS_ACTION)
        {
            sleep(1);
            if (executeStartOperation(serviceName, SINGLE_PROC_FILTER) == true)
            {
                broadcastWatchdogEvent(WATCHDOG_SERVICE_STATE_CHANGED, WATCHDOG_EVENT_VALUE_ACTION_RESTART, (const char *)serviceName);
                rc = true;
            }
        }
    }

    // release lock
    //
    pthread_mutex_unlock(&SERVICE_MTX);
    return rc;
}

/**
 * starts, stops, or restarts a group of processes with matching 'groupName'
 */
void operationOnGroupOfProcesses(operationAction action, char *groupName)
{
    // obtain lock
    //
    pthread_mutex_lock(&SERVICE_MTX);

    // perform "group" action
    //
    if (action == START_PROCESS_ACTION)
    {
        // start the group, then send the event
        //
        if (executeStartOperation(groupName, GROUP_PROC_FILTER) == true)
        {
            broadcastWatchdogEvent(WATCHDOG_GROUP_STATE_CHANGED, WATCHDOG_EVENT_VALUE_ACTION_START, (const char *)groupName);
        }
    }
    else if (action == STOP_PROCESS_ACTION)
    {
        // stop then send the event (since the 'handle death' callback won't send
        // the event as we're doing this on purpose)
        //
        if (executeStopOperation(groupName, GROUP_PROC_FILTER, IGNORE_DEATH) == true)
        {
            broadcastWatchdogEvent(WATCHDOG_GROUP_STATE_CHANGED, WATCHDOG_EVENT_VALUE_ACTION_DEATH, (const char *)groupName);
        }
    }
    else if (action == RESTART_PROCESS_ACTION)
    {
        // note: when stopping the processes we'll wipe the 'lastAck' value
        //       to reproduce the same steps as watchdog startup.  this way
        //       the services still have to wait for the "WATCHDOG_INIT_COMPLETE" event
        //       before finalizing their initialization
        //
        executeStopOperation(groupName, GROUP_PROC_FILTER, IGNORE_DEATH);
        sleep(1);
        if (executeStartOperation(groupName, GROUP_PROC_FILTER) == true)
        {
            broadcastWatchdogEvent(WATCHDOG_GROUP_STATE_CHANGED, WATCHDOG_EVENT_VALUE_ACTION_RESTART, (const char *)groupName);
        }
    }

    // release lock
    //
    pthread_mutex_unlock(&SERVICE_MTX);
}

/*
 * populates the supplied linked list with process info,
 * which are duplicates of the 'serviceDefinition'
 * within the set of known serviceDefinition objects.
 * caller must free the items added to the linked list.
 * Via destroy_processInfo
 */
void getAllServiceProcessInfo(icLinkedList *target)
{
    // iterate through the definitions
    //
    pthread_mutex_lock(&SERVICE_MTX);
    icLinkedListIterator *iter = linkedListIteratorCreate(managerList);
    while(linkedListIteratorHasNext(iter) == true)
    {
        serviceDefinition *curr = (serviceDefinition *) linkedListIteratorGetNext(iter);

        // create the service process info
        // and populate it
        //
        processInfo *serviceInfo = calloc(1, sizeof(processInfo));
        transferServiceDefinitionToProcessInfo(curr, serviceInfo);

        // add contents to the return array
        //
        linkedListAppend(target, serviceInfo);
    }

    // cleanup
    linkedListIteratorDestroy(iter);
    pthread_mutex_unlock(&SERVICE_MTX);
}

/*
 * populates the supplied linked list with string values,
 * which are duplicates of the 'serviceName' value contained
 * within the set of known serviceDefinition objects.
 * caller must free the items added to the linked list.
 */
void getAllServiceNames(icLinkedList *target)
{
    // iterate through the definitions
    //
    pthread_mutex_lock(&SERVICE_MTX);
    icLinkedListIterator *loop = linkedListIteratorCreate(managerList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        serviceDefinition *curr = linkedListIteratorGetNext(loop);

        // add it's key to the return array
        //
        linkedListAppend(target, strdup(curr->serviceName));
    }

    // mem cleanup and return
    //
    linkedListIteratorDestroy(loop);
    pthread_mutex_unlock(&SERVICE_MTX);
}

/*
 * locates the service with the serviceName matching 'proc->serviceName'.
 * if found, sets the "lastAckReceivedTime" to now and returns true.
 */
bool acknowledgeServiceStarted(ackServiceDef *proc)
{
    bool retVal = false;

    // sanity check
    //
    if (proc->serviceName == NULL)
    {
        return retVal;
    }

    // search the linked list for this 'mgrName'
    //
    pthread_mutex_lock(&SERVICE_MTX);
    serviceDefinition *found = (serviceDefinition *)linkedListFind(managerList, proc->serviceName, findServiceByNameInList);
    if (found != NULL)
    {
        // save 'now' as the "lastAckReceivedTime" (in time_t format)
        //
        found->lastActReceivedTime = getCurrentTime_t(false);

        // save off IPC port so we can potentially ask
        // this service for the status, statistics, etc
        //
        found->serviceIpcPort = proc->ipcPortNum;

        // save shutdown token
        //
        if (proc->token != NULL)
        {
            // release previous token, then save this one
            //
            free(found->shutdownToken);
            found->shutdownToken = strdup(proc->token);
        }
        retVal = true;

        // see how many non-single-phase services are lacking the ack
        //
        uint16_t nonSingleCount = internalCountServicesToBeAcknowledged();

        // see if we have a monitorAck task running (i.e. in startup).  yes, we
        // are about to hold 2 mutex locks at the same time.  not optimal, but not
        // sure how else to do this and prevent the cancel timer from interfering
        //
        bool gotAllAcks = false;
        pthread_mutex_lock(&ACK_MONITOR_MTX);
        if (monitorAckTask != 0)
        {
            // see how many single-phase services are lacking the ack
            //
            uint16_t singlePhaseCount = internalCountSinglePhaseServicesToBeAcknowledged();
            if (singlePhaseCount == 0)
            {
                // unblock the performStartupSeqence by broadcasting on the condition.
                // we can do this here because we have the SERVICE_MTX held
                //
                // TODO: don't log this EVERY TIME
                icLogDebug(WDOG_LOG, "got acknowledgement from all single phase services, starting remainder of services");
                pthread_cond_signal(&SINGLE_PHASE_START_COMPLETE_COND);
            }
            else
            {
                icLogDebug(WDOG_LOG, "still waiting on ack from %"PRIi32" single phase startup service(s)", singlePhaseCount);
            }

            // see how many non-single-phase services are lacking the ack
            //
            if (nonSingleCount == 0)
            {
                // all are done, so cancel the task and complete our startup sequence
                //
                icLogDebug(WDOG_LOG, "got acknowledgement from ALL services, canceling timer..");
                cancelDelayTask(monitorAckTask);
                monitorAckTask = 0;

                // set flag to complete after we release the ACK_MONITOR_MTX
                //
                gotAllAcks = true;
            }
            else
            {
                icLogDebug(WDOG_LOG, "still waiting on ack from %"PRIi32" service(s)", nonSingleCount);
            }
            pthread_mutex_unlock(&ACK_MONITOR_MTX);
        }
        else
        {
            // nothing to do with startup or restart-all.  this is the ack
            // after that has occurred or from a single process restart then acknowledging
            //
            pthread_mutex_unlock(&ACK_MONITOR_MTX);

            // look at the nonSingleCount.  if 0, then send the WATCHDOG_INIT_COMPLETE event,
            // but with this service name as an arg and the WATCHDOG_EVENT_VALUE_SOME_SERVICES_STARTED qualifier
            //
            if (nonSingleCount == 0)
            {
                // re-send the WATCHDOG_INIT_COMPLETE event for this single service restart
                //
                icLogDebug(WDOG_LOG, "all services are acknowledged, but done outside of the start-all/restart-all; sending INIT COMPLETE for service %s", found->serviceName);
                broadcastWatchdogEvent(WATCHDOG_INIT_COMPLETE, WATCHDOG_EVENT_VALUE_SOME_SERVICES_STARTED, found->serviceName);
            }
        }

        if (gotAllAcks == true)
        {
            // initialize each service, then send the WATCHDOG_INIT_COMPLETE event
            //
            icLogDebug(WDOG_LOG, "got acknowledgement from ALL services, finalizing the startup sequence");
            allAcksReceived(false);
        }
    }
    pthread_mutex_unlock(&SERVICE_MTX);

    return retVal;
}

/*
 * returns true if all services are started (and the WATCHDOG_INIT_COMPLETE event was sent)
 */
bool areAllServicesStarted()
{
    bool retVal = false;

    // first see if the monitor thread is running
    pthread_mutex_lock(&ACK_MONITOR_MTX);
    if (monitorAckTask == 0)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&ACK_MONITOR_MTX);

    // last check that everything ack'd.  it's possible we
    // are being asked before it really started
    //
    if (countServicesToBeAcknowledged() > 0)
    {
        retVal = false;
    }

    return retVal;
}

/*
 * linkedListIterateFunc implementation to count services.
 * used during internalCountServicesToBeAcknowledged.
 */
static bool countNonSinglePhaseIterateFunc(void *item, void *arg)
{
    serviceDefinition *curr = (serviceDefinition *) item;
    uint16_t *counter = (uint16_t *) arg;

    // see if this one 'should' send us an ack, and increment the count
    // if it has not.  Note that we only look for services that were
    // launched during startup (or java services we are waiting on)
    //
    if ((curr->autoStart == true || curr->isJavaService == true) &&
        curr->expectStartupAck == true &&
        curr->lastActReceivedTime == 0)
    {
//icLogDebug(WDOG_LOG, "waiting for service %s to ack", curr->serviceName);
        *counter = (uint16_t)(*counter + 1);
    }

    return true;
}

/*
 * internal impl of countServicesToBeAcknowledged where the lock is held
 */
static uint16_t internalCountServicesToBeAcknowledged()
{
    // loop through all known services and count ones that have the ack
    //
    uint16_t count = 0;
    linkedListIterate(managerList, countNonSinglePhaseIterateFunc, &count);
    return count;
}

/*
 * counts the number of services that have 'expectStartupAck' set to true,
 * but have not sent the ACK notification yet.  Used to help determine if
 * all critical services are initialized and ready.
 */
uint16_t countServicesToBeAcknowledged()
{
    // get the lock, then count the services that sent an ack
    //
    pthread_mutex_lock(&SERVICE_MTX);
    uint16_t retVal = internalCountServicesToBeAcknowledged();
    pthread_mutex_unlock(&SERVICE_MTX);

    return retVal;
}

/*
 * linkedListIterateFunc implementation to count services.
 * used during internalCountSinglePhaseServicesToBeAcknowledged.
 */
static bool countSinglePhaseIterateFunc(void *item, void *arg)
{
    serviceDefinition *curr = (serviceDefinition *)item;
    uint16_t *counter = (uint16_t *)arg;

    // see if this one 'should' send us an ack, and increment
    // the count if it has not.  Note that we only look for services
    // that were launched during startup
    //
    if (curr->autoStart == true && curr->expectStartupAck == true && curr->lastActReceivedTime == 0 && curr->singlePhaseStartup)
    {
        *counter = (uint16_t)(*counter + 1);
    }
    return true;
}

/*
 * internal impl of countSinglePhaseServicesToBeAcknowledged where the lock is held
 */
static uint16_t internalCountSinglePhaseServicesToBeAcknowledged()
{
    // loop through all known services and count our single phase services with an ack
    //
    uint16_t count = 0;
    linkedListIterate(managerList, countSinglePhaseIterateFunc, &count);
    return count;
}

/*
 * counts the number of services that have 'expectStartupAck' and
 * 'singlePhaseStartup' set to true, but have not sent the ACK
 * notification yet.  Used to help determine if all singlePhase services
 * are initialized and ready.
 *
 * internal call that assumes the mutex is held
 */
uint16_t countSinglePhaseServicesToBeAcknowledged()
{
    // get the lock, then count the single-pahse services that sent an ack
    //
    pthread_mutex_lock(&SERVICE_MTX);
    uint16_t retVal = internalCountSinglePhaseServicesToBeAcknowledged();
    pthread_mutex_unlock(&SERVICE_MTX);

    return retVal;
}

/**
 * disable the 'restartOnFail' flag on the service with this name
 * returns false if the named process is not found
 */
bool stopMonitoringService(const char *serviceName)
{
    // obtain lock
    //
    bool retVal = false;
    pthread_mutex_lock(&SERVICE_MTX);

    // search the linked list for this 'mgrName'
    //
    serviceDefinition *found = (serviceDefinition *)linkedListFind(managerList, (void *)serviceName, findServiceByNameInList);
    if (found != NULL)
    {
        // found the process, see if it's running
        //
        if (found->currentPid > 0)
        {
            // disable restart
            //
            found->restartOnFail = false;
            retVal = true;
        }
    }

    // release lock
    //
    pthread_mutex_unlock(&SERVICE_MTX);
    return retVal;
}

/**
 * locates the service with this name.  if found returns
 * a clone of the definition. caller MUST free the returned object.
 *
 * @see destroyServiceDefinition
 */
serviceDefinition *getServiceForName(const char *serviceName)
{
    serviceDefinition *retVal = NULL;

    // search the linked list for this 'mgrName'
    //
    pthread_mutex_lock(&SERVICE_MTX);
    serviceDefinition *found = (serviceDefinition *)linkedListFind(managerList, (void *)serviceName, findServiceByNameInList);
    if (found != NULL)
    {
        // found the definition, create the clone
        //
        retVal = (serviceDefinition *)calloc(1, sizeof(serviceDefinition));
        if (found->serviceName != NULL)
        {
            retVal->serviceName = strdup(found->serviceName);
        }
        if (found->logicalGroup != NULL)
        {
            retVal->logicalGroup = strdup(found->logicalGroup);
        }
        if (found->shutdownToken != NULL)
        {
            retVal->shutdownToken = strdup(found->shutdownToken);
        }
        retVal->restartOnFail = found->restartOnFail;
        retVal->expectStartupAck = found->expectStartupAck;
        retVal->secondsBetweenRestarts = found->secondsBetweenRestarts;
        retVal->maxRestartsPerMinute = found->maxRestartsPerMinute;
        retVal->restartsWithinPastMinute = found->restartsWithinPastMinute;
        retVal->actionOnMaxRestarts = found->actionOnMaxRestarts;
        retVal->autoStart = found->autoStart;
        retVal->lastRestartTime = found->lastRestartTime;
        retVal->lastRestartTimeMono = found->lastRestartTimeMono;
        retVal->lastActReceivedTime = found->lastActReceivedTime;
        retVal->waitSecsOnShutdown = found->waitSecsOnShutdown;
        retVal->currentPid = found->currentPid;
        retVal->tempIgoreDeath = found->tempIgoreDeath;
        retVal->deathCount = found->deathCount;
        retVal->serviceIpcPort = found->serviceIpcPort;
        retVal->isJavaService = found->isJavaService;
        retVal->singlePhaseStartup = found->singlePhaseStartup;
    }

    // release lock
    //
    pthread_mutex_unlock(&SERVICE_MTX);
    return retVal;
}

/**
 * handles a dead child, based on the child's configuration
 */
static void handleDeadChild(pid_t childPid)
{
    // find manager with this pid
    //
    pthread_mutex_lock(&SERVICE_MTX);
    serviceDefinition *target = (serviceDefinition *)linkedListFind(managerList, &childPid, findServiceByPidInList);
    if (target != NULL)
    {
        // Log line used for Telemetry... DO NOT CHANGE
        //
        icLogInfo(WDOG_LOG, "Process %s (pid %d) died", target->serviceName, childPid);

        target->currentPid = 0;

        // send event that this process died (since we were not expecting it)
        //
        if (target->tempIgoreDeath == false)
        {
            target->deathCount++;
            broadcastWatchdogEvent(WATCHDOG_SERVICE_STATE_CHANGED, WATCHDOG_EVENT_VALUE_ACTION_DEATH, (const char *) target->serviceName);
        }

        // first, make sure we are supposed to restart this one when it dies
        //
        if (target->restartOnFail == false)
        {
            icLogDebug(WDOG_LOG, "Process %s marked to NOT RESTART on fail", target->serviceName);
            pthread_cond_broadcast(&SERVICE_COND);  // possibly blocked because we told it to fail
            pthread_mutex_unlock(&SERVICE_MTX);
            return;
        }
        else if (target->tempIgoreDeath == true)
        {
            icLogDebug(WDOG_LOG, "Temporarily ignoring the fact Process %s died", target->serviceName);
            pthread_cond_broadcast(&SERVICE_COND);  // possibly blocked because we told it to fail
            pthread_mutex_unlock(&SERVICE_MTX);
            return;
        }

        // make sure we don't restart more quickly than this child is configured
        //
        time_t now = getCurrentTime_t(true);
        if (target->secondsBetweenRestarts > 0)
        {
            time_t restartTimeDiff = now - target->lastRestartTimeMono;
            if (restartTimeDiff >= 0)
            {
                if (restartTimeDiff < target->secondsBetweenRestarts)
                {
                    // Hold the lock while synchronously waiting to avoid spurious starts
                    // coverity[sleep]
                    struct timespec waitTime = { .tv_sec = target->secondsBetweenRestarts - restartTimeDiff, .tv_nsec= 0 };
                    if (nanosleep(&waitTime, NULL) != 0)
                    {
                        char *errStr = strerrorSafe(errno);

                        icLogInfo(WDOG_LOG, "Cancelling dead child restart: %s", errStr);

                        free(errStr);
                        pthread_mutex_unlock(&SERVICE_MTX);
                        return;
                    }
                }
            }
        }

        // make sure (roughly) that we aren't restarting too often
        //
        if (target->maxRestartsPerMinute > 0)
        {
            if ((now - target->lastRestartTimeMono) < 60)
            {
                target->restartsWithinPastMinute++;
            }
            else
            {
                target->restartsWithinPastMinute = 1;     // XHSCPEB-564 : we just restarted, has to be 1, not 0
            }
            if (target->restartsWithinPastMinute > target->maxRestartsPerMinute)
            {
                switch(target->actionOnMaxRestarts)
                {
                    case REBOOT_ACTION:
                    {
#ifdef CONFIG_LIB_SHUTDOWN
                        icLogWarn(WDOG_LOG, "Max restarts per minute (%d) of %s reached; desired action is reboot; rebooting...",
                                  target->maxRestartsPerMinute, target->serviceName);

                        // before we actually boot, save off the service that is causing the reboot.  that way we can
                        // treat this one different on our next startup.  we are trying to prevent a reboot-loop due
                        // to a single misbehaving process.
                        //
                        saveMisbehavingService((const char *)target->serviceName);
                        pthread_mutex_unlock(&SERVICE_MTX);

#ifdef CONFIG_SERVICE_DIAGNOSTIC
                        // one more step before physically rebooting...
                        // create a mini diag that we'll save off for upload later on (presumably after the reboot)
                        //
                        icLogInfo(WDOG_LOG, "Creating mini diag prior to reboot");
                        createMiniDiagForLater();
#endif // CONFIG_SERVICE_DIAGNOSTIC

                        // finally, the reboot
                        //
                        sleep(2);
                        sync();
                        sleep(2);
                        reboot(RB_AUTOBOOT);
#else // NOT CONFIG_LIB_SHUTDOWN
                        // no reboot allowed, so act like 'default' below
                        icLogWarn(WDOG_LOG, "Max restarts per minute (%d) of %s reached; desired action is reboot, but reboot disabled.  Process will not be restarted",
                                  target->maxRestartsPerMinute, target->serviceName);
                        pthread_mutex_unlock(&SERVICE_MTX);
#endif // CONFIG_LIB_SHUTDOWN
                        return;
                    }

                    case STOP_RESTARTING_ACTION:
                    {
                        icLogWarn(WDOG_LOG, "Max restarts per minute (%d) of %s reached; desired action is stop restarting. Process will not be restarted",
                                  target->maxRestartsPerMinute, target->serviceName);
                        pthread_mutex_unlock(&SERVICE_MTX);
                        return;
                    }

                    default:
                    {
                        icLogWarn(WDOG_LOG, "Max restarts per minute (%d) of %s reached; unknown action %d configured; continuing to restart process",
                                  target->maxRestartsPerMinute, target->serviceName, target->actionOnMaxRestarts);
                        break;
                    }
                }
            }
        }

        // restart the single process that died, then send the event
        //
        icLogDebug(WDOG_LOG, "Attempting to start %s after it died", target->serviceName);
        startProcess(target, true);
        broadcastWatchdogEvent(WATCHDOG_SERVICE_STATE_CHANGED, WATCHDOG_EVENT_VALUE_ACTION_START, (const char *)target->serviceName);
    }

    pthread_mutex_unlock(&SERVICE_MTX);
}


/*-----------------------------------------------
 *
 * startup sequence functions
 *
 *-----------------------------------------------*/

static void startMonitorAckThread()
{
    // create monitorAck object with a timeout of 5 minutes
    // this allows us to wait for all critical services to complete
    // and send out a WATCHDOG_INIT_COMPLETE event
    // it also provides us with a realistic timeout so we don't
    // wait forever
    //
    pthread_mutex_lock(&ACK_MONITOR_MTX);
    if (monitorAckTask == 0)
    {
        monitorAckTask = scheduleDelayTask(5, DELAY_MINS, monitorAckDelayCallback, NULL);
    }
    pthread_mutex_unlock(&ACK_MONITOR_MTX);
}

//#ifndef CONFIG_DEBUG_SINGLE_PROCESS
///**
// * Notification that all 'singlePhaseStartup' services are complete
// */
//void notifySinglePhaseStartupComplete()
//{
//    pthread_mutex_lock(&SERVICE_MTX);
//    // Just signal that we are done
//    pthread_cond_signal(&SINGLE_PHASE_START_COMPLETE_COND);
//    pthread_mutex_unlock(&SERVICE_MTX);
//}
//#endif

/*
 * called when all of the "acknowledgements" were received
 * OR our timeout hit and we're pushing forward.
 *
 * assumes the SERVICE_MTX is held
 */
static void allAcksReceived(bool gaveUp)
{
    if (startupSequenceFinalized == true)
    {
        icLogWarn(WDOG_LOG, "%s: refusing to replay startup sequence", __FUNCTION__);
        return;
    }

    // get the list of services that are supposed to send an ack at startup.
    // we'll then loop through each and let them know that it's time to
    // begin "phase 2" of initialization.
    // IF we are in a "restart" scenario, we don't know which services
    // were restarted and which weren't.  therefore we have to assume each service
    // will be smart about getting this IPC call...
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(managerList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // the thought here is that we tell everything we have an IPC port for
        //
        bool worked = false;
        serviceDefinition *curr = linkedListIteratorGetNext(loop);
        if (curr->serviceIpcPort > 0)
        {
            // got a running service (native or java)
            //
            icLogInfo(WDOG_LOG, "calling service %s 'startInitialization'", curr->serviceName);
            IPCCode rc = startInitialization((uint16_t)curr->serviceIpcPort, &worked, START_INIT_TIMEOUT_SECS);
            icLogDebug(WDOG_LOG, "service %s 'startInitialization' returned %s", curr->serviceName, IPCCodeLabels[rc]);
        }
    }
    linkedListIteratorDestroy(loop);

    // last thing to do, send the event
    //
    icLogInfo(WDOG_LOG, "startup sequence complete; sending WATCHDOG_INIT_COMPLETE event");
    broadcastWatchdogEvent(WATCHDOG_INIT_COMPLETE, WATCHDOG_EVENT_VALUE_ALL_SERVICES_STARTED, NULL);

    startupSequenceFinalized = true;
}

/*
 * callback to scheduleDelayTask()
 */
static void monitorAckDelayCallback(void *arg)
{
    // close up our handle
    //
    pthread_mutex_lock(&ACK_MONITOR_MTX);
    monitorAckTask = 0;
    pthread_mutex_unlock(&ACK_MONITOR_MTX);

    // delay expired, so log a warning then send event that startup is complete
    //
    icLogWarn(WDOG_LOG, "timeout while waiting for services to acknowledge initialization; something is problbably wrong, but finalizing the startup sequence regardless");
    pthread_mutex_lock(&SERVICE_MTX);
    allAcksReceived(true);
    pthread_mutex_unlock(&SERVICE_MTX);
}

/*
 * callback from scheduleDelayTask()
 */
static void resetBadServiceDelayCallback(void *arg)
{
    char *badServiceName = (char *)arg;

    // find this serviceName, and reset it's actionOnMaxRestarts to REBOOT_ACTION
    //
    pthread_mutex_lock(&SERVICE_MTX);
    resetBadServiceTask = 0;
    serviceDefinition *found = (serviceDefinition *) linkedListFind(managerList, badServiceName, findServiceByNameInList);
    if (found != NULL)
    {
        icLogInfo(WDOG_LOG, "restoring service %s to reboot on failure for 1 hour", badServiceName);
        found->actionOnMaxRestarts = REBOOT_ACTION;
    }
    pthread_mutex_unlock(&SERVICE_MTX);

    // clear the memory
    free(arg);
}

/*
 * run through the steps required to launch all of our services.
 * called internal and assumes the SERVICE_MTX is held
 */
static void performStartupSequence(icLinkedList *serviceNames)
{
    // this requires some explaining as the startup of all processes is complicated...
    // to ensure the services are brought-up in order of dependency (to a degree), we
    // follow the sequence of steps:
    //
    // 1) start a timer (via delayed task) so we don't wait forever for service acks.
    //    think of it as "jump to step 6"
    //
    // 2) start the services tagged for SINGLE_PHASE, which are critical
    //    services that do not have dependencies (ex: properties)
    //
    // 3) wait for those to 'ack' that they are ready for use
    //
    // 4) start all services tagged for NON_SINGLE_PHASE (essentially anything that
    //    will 'ack' and isn't SINGLE_PHASE)
    //
    // 5) wait for all of those services to send in their 'ack'
    //
    // 6) in definition order, call the startInitialization() IPC on each service.
    //    this tells the service that all of them are available and allows a controlled
    //    startup where they can query each other for information.
    //
    // 7) send the WATCHDOG_INIT_COMPLETE event for non-service use (ex. the UI)
    //
    // the caveats:
    //  - bouncing a service relies on that service using the ackAndRegisterForAllComplete() helper
    //    because it would be starting AFTER all of the steps above were complete (meaning the serivce
    //    will not get the WATCHDOG_INIT_COMPLETE event)
    //  - we need to refactor some of this because this startup sequence is spread out in this file
    //    and the ipcHandler file...making this less-then-ideal from a debugging standpoint.
    //

    // 1) start the timer
    startMonitorAckThread();

    // 2) start just the single phase stuff
    executeStartOperation(NULL, SINGLE_PHASE_FILTER);

    // 3) wait for all single phase processes to start, or give up after waiting awhile
    incrementalCondTimedWait(&SINGLE_PHASE_START_COMPLETE_COND, &SERVICE_MTX, SINGLE_PHASE_STARTUP_WAIT_SECS);

    // now that properties is running, adjust our log level
    autoAdjustCustomLogLevel(WATCH_DOG_SERVICE_NAME);

    // see if we start all remaining auto-start services, or just the ones provided (for restart)
    //
    if (serviceNames == NULL)
    {
        // 4) start the remainder
        executeStartOperation(NULL, NON_SINGLE_PHASE_FILTER);
    }
    else
    {
        // 4) start the processes provided
        //
        icLinkedListIterator *loop = linkedListIteratorCreate(serviceNames);
        while (linkedListIteratorHasNext(loop) == true)
        {
            // get the service name, and restart just that one
            //
            char *mgrName = (char *) linkedListIteratorGetNext(loop);
            executeStartOperation(mgrName, SINGLE_PROC_FILTER);
        }

        // cleanup
        //
        linkedListIteratorDestroy(loop);
    }

    // steps 5 - 7 will happen once all of the service 'acks' are received (or the timer expires)
    // see:
    //   acknowledgeServiceStarted()
    //   monitorAckDelayCallback()
}
