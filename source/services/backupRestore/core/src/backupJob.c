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
 * backupJob.c
 *
 * Task to perform a 'backup' of our configuration.
 *
 * Similar to the Java counterpart, the idea is to
 * schedule a random time in the future to create a
 * backup of our configuration data, and send it to
 * the server.  Triggered when another Service changes
 * it's config data, this allows us to not perform a
 * backup every time something is changed - instead simply
 * start a timer to execute the backup after a period.
 * This allows lots of changes to occur in a window and
 * supply the server with a single backup to store.
 *
 * Once scheduled, the timer can be canceled in case
 * we need to reset or restart.
 *
 * Certain situations require a 'force backup', where
 * the timer is ignored and we immediately perform the
 * task.
 *
 * Author: jelderton - 7/1/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icConcurrent/timedWait.h>
#include <icTime/timeTracker.h>
#include <icSystem/runtimeAttributes.h>
#include <propsMgr/paths.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <commMgr/commService_ipc.h>
#include <icConcurrent/threadUtils.h>
#include "backupJob.h"
#include "common.h"

#define USEC_PER_SEC        1000000

/*
 * local datatypes
 */
typedef enum
{
    BACKUP_IDLE,        // normal state - nothing going on
    BACKUP_SCHEDULED,   // told to schedule, waiting for timer to expire
    BACKUP_RUNNING,     // timer expired, performing backup
    BACKUP_CANCELED     // told to cancel - reset back to IDLE once cancel is done
} backupState;

/*
 * local variables
 */
static pthread_mutex_t JOB_MTX = PTHREAD_MUTEX_INITIALIZER;     // mutex for the backupJob
static pthread_cond_t  JOB_COND = PTHREAD_COND_INITIALIZER;     // condition used for 'scheduler'
static backupState     state = BACKUP_IDLE; // current state of backupJob
static bool       didInit = false;

/*
 * private function declarations
 */
static void initLocalVars();
static void internalScheduleBackup(uint16_t delay, scheduleUnits units);
static void *doBackupThread(void *arg);
static void internalRunBackup();

/*
 * Returns if a backup job is currently in progress
 */
bool isBackupRunning()
{
    bool retVal = false;

    // examine the state to see if BACKUP_RUNNING
    //
    pthread_mutex_lock(&JOB_MTX);
    if (state == BACKUP_RUNNING)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&JOB_MTX);

    return retVal;
}

/*
 * Returns if a backup job is scheduled for later
 */
bool isBackupScheduled()
{
    bool retVal = false;

    // examine the state to see if BACKUP_SCHEDULED
    //
    pthread_mutex_lock(&JOB_MTX);
    if (state == BACKUP_SCHEDULED)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&JOB_MTX);

    return retVal;
}

/*
 * Cancel a backup job that is scheduled
 */
void cancelScheduledBackup()
{
    // only applicable if our state is BACKUP_SCHEDULED
    //
    pthread_mutex_lock(&JOB_MTX);
    if (state == BACKUP_SCHEDULED)
    {
        // set to BACKUP_CANCEL
        //
        state = BACKUP_CANCELED;

        // notify timer to stop (via JOB_COND)
        //
        icLogDebug(BACKUP_LOG, "canceling backup timer");
        initLocalVars();
        pthread_cond_broadcast(&JOB_COND);
    }
    pthread_mutex_unlock(&JOB_MTX);
}

/*
 * schedule a backup job to run some time in the future
 * will fail if it is already scheduled or running
 */
bool scheduleBackup(uint16_t delay, scheduleUnits units)
{
    bool retVal = false;

    // make sure our state is at BACKUP_IDLE
    //
    pthread_mutex_lock(&JOB_MTX);
    if (state == BACKUP_IDLE)
    {
        // not already scheduled, running, or canceling
        //
        retVal = true;
        internalScheduleBackup(delay, units);
    }
    pthread_mutex_unlock(&JOB_MTX);

    return retVal;
}

/*
 * get random number between 1 & 12
 */
static uint16_t getRandomValue()
{
    // seed the generator, then get a rand number
    //
    srandom((uint32_t)time(NULL));
    uint16_t val = 0;
    while (val == 0)
    {
        val = (uint16_t)(random() % 12);
    }
    return val;
}

/*
 * return if our MARKER_FILE is present
 */
static bool delayInMinutes()
{
    bool retVal = false;

    // look for the "config.fastbackuptimer.flag" CPE property
    //
    if (getPropertyAsBool(CONFIG_FASTBACKUP_BOOL_PROPERTY, false) == true)
    {
        // use minutes instead of hours
        //
        icLogDebug(BACKUP_LOG, "%s property is set to 'true'; must be in 'minute' mode", CONFIG_FASTBACKUP_BOOL_PROPERTY);
        retVal = true;
    }
    return retVal;
}

/*
 * helper function to choose a random number (1-12) and
 * start the scheduled backup IF it's not already running
 * or scheduled.  More of an atomic operation then:
 *   if ( !isRunning() && !isScheduled() ) { scheduleBackup() }
 */
bool scheduleBackupIfPossible()
{
    bool retVal = false;

    pthread_mutex_lock(&JOB_MTX);
    if (state == BACKUP_IDLE)
    {
        // shift in approach...  in the past, we would delay for hours
        // before performing the backup.  when moving to Zilker, we had
        // a need to backup in seconds for devices that had no volatile
        // storage.  after some discussion, we believe this should always
        // be minutes (not hours) except for the storage situation.
        //
        // get a positive random number between 1-12
        //
        unsigned short delay = 0;
        while (delay == 0)
        {
            delay = getRandomValue();
        }

        // by default do this in HOURS, but look for the property
        // that wants this to be in MINUTES (fast backup property)
        //
        scheduleUnits units = SCHEDULE_HOURS;
        if (delayInMinutes() == true)
        {
            units = SCHEDULE_MINS;
        }

        // start the thread
        //
        retVal = true;
        internalScheduleBackup(delay, units);
    }
    else
    {
        icLogDebug(BACKUP_LOG, "asked to schedule backup, but current state = %d", state);
    }
    pthread_mutex_unlock(&JOB_MTX);

    return retVal;
}

/*
 * perform an immediate backup (ignoring the timer).
 * this will block until the backup is complete.
 */
bool forceBackup()
{
    // look at the current state.
    //
    pthread_mutex_lock(&JOB_MTX);
    if (state == BACKUP_SCHEDULED)
    {
        // thread is running, so just set the state
        // and let the thread take care of it
        //
        state = BACKUP_RUNNING;
        pthread_cond_broadcast(&JOB_COND);
        pthread_mutex_unlock(&JOB_MTX);

        return true;
    }

    // not scheduled, so just run it now
    //
    internalRunBackup();
    pthread_mutex_unlock(&JOB_MTX);
    return true;
}

/*
 * internal, assume caller has the JOB_MTX held
 */
static void initLocalVars()
{
    if (didInit == false)
    {
        initTimedWaitCond(&JOB_COND);
        didInit = true;
    }
}

/*
 * internal function to start the thread.  assumes
 * caller has the mutex held
 */
static void internalScheduleBackup(uint16_t delay, scheduleUnits units)
{
    // set the state
    //
    state = BACKUP_SCHEDULED;
    initLocalVars();

    icLogDebug(BACKUP_LOG, "scheduling backup to occur in %"PRIu16" %s", delay,
               (units == SCHEDULE_HOURS) ? "hours" : "minutes");

    // convert 'delay' into "seconds from now" so we can
    // apply that to a timerTask that will expire at roughly
    // the correct time.
    //
    timeTracker *tracker = timeTrackerCreate();
    uint32_t seconds = delay;
    if (units == SCHEDULE_MINS)
    {
        seconds *= 60;
    }
    else if (units == SCHEDULE_HOURS)
    {
        seconds *= (60 * 60);
    }
    timeTrackerStart(tracker, seconds);

    // pass along to the 'doBackupThread'
    //
    createDetachedThread(doBackupThread, tracker, "backupJob");
}

/*
 *
 */
static void *doBackupThread(void *arg)
{
    // get the 'timeTracker' parameter
    //
    timeTracker *tracker = (timeTracker *)arg;

    // the trick is to not just "sleep" for the entire duration
    // because of time skew, changes in clock, delays in CPU, etc
    // we will just loop on our JOB_COND (so we can be canceled)
    // in 1 minute intervals.  This allows us to bail if need-be
    // and/or make adjustments during our delay window
    //
    bool keepGoing = true;
    while (keepGoing == true)
    {
        int delay = 60;
        uint32_t temp = timeTrackerSecondsUntilExpiration(tracker);
        if (delay > temp)
        {
            delay = (int)temp;
        }

        // wait up to 1 minute
        //
        pthread_mutex_lock(&JOB_MTX);
        int rc = incrementalCondTimedWait(&JOB_COND, &JOB_MTX, delay);
        if (rc != 0 && rc != ETIMEDOUT)
        {
            // 'timeout' should happen every minute, so log the error
            // if we happen to get something different
            //
            icLogWarn(BACKUP_LOG, "job: error waiting: %d - %s", rc, strerror(rc));
        }

        // see if we were told to cancel
        //
        if (state == BACKUP_CANCELED)
        {
            icLogDebug(BACKUP_LOG, "job: told to cancel the timer");
            state = BACKUP_IDLE;
            keepGoing = false;
        }
        else if (state == BACKUP_RUNNING)
        {
            // told to 'force', so bail
            //
            icLogDebug(BACKUP_LOG, "job: told to force the timer, starting backup!");
            internalRunBackup();
            keepGoing = false;
        }
        else if (state == BACKUP_SCHEDULED)
        {
            uint32_t elapsed = timeTrackerElapsedSeconds(tracker);

            // see if we expired yet
            //
            if (timeTrackerExpired(tracker) == true)
            {
                // time to perform the backup
                //
                icLogDebug(BACKUP_LOG, "job: timer expired after %" PRIu32 " seconds, starting backup!", elapsed);
                internalRunBackup();
                keepGoing = false;
            }
            else
            {
                // should have the number of seconds that have elapsed
                //
                icLogDebug(BACKUP_LOG, "job: elapsed time is %" PRIu32 "; %" PRIu32 " seconds to go...",
                           elapsed, timeTrackerSecondsUntilExpiration(tracker));
            }
        }

        // release lock since the top of the loop will reclaim it,
        // and allow other threads to have it in case blocked waiting
        //
        pthread_mutex_unlock(&JOB_MTX);
    }

    // mem cleanup
    //
//    free(arg);
    timeTrackerDestroy(tracker);
    return NULL;
}

/*
 * called internally, so assume mutex is held
 */
static void internalRunBackup()
{
    // set state just to be complete
    //
    state = BACKUP_RUNNING;

    // first, get the information the script will need
    // TODO: Needs implementation
    //
    char *serverUrl = NULL;
    char *username = NULL;
    char *password = NULL;
    uint64_t version = 0;
    uint64_t identifier = 0;

    // now the location of the script
    //
    char *homeDir = getStaticPath();
    char *configDir = getDynamicConfigPath();

    // put it together and pass all of this information as args to the script
    //
    char script[FILENAME_MAX];
    sprintf(script, "%s/bin/xhBackup.sh %s %s %s %s %s %"PRIu64" %"PRIu64, homeDir, configDir, serverUrl,
            username, password, getSystemCpeIdLowerCase(), version, identifier);
    icLogInfo(BACKUP_LOG, "performing backup via script '%s/bin/xhBackup.sh'", homeDir);

    // execute the tar command
    //
    int rc = system(script);
    if (rc != 0)
    {
        icLogWarn(BACKUP_LOG, "backup command failed with rc %d", rc);
#ifdef CONFIG_DEBUG_RMA
        // show what command we ran
        //
        icLogDebug(BACKUP_LOG, "backup failed running script '%s'", script);
#endif
    }
    else
    {
        icLogDebug(BACKUP_LOG, "backup command success");
    }

    // set state to IDLE
    //
    state = BACKUP_IDLE;

    // cleanup
    //
    free(homeDir);
    free(configDir);
}

