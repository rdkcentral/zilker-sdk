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
 * replayTracker.c
 *
 * Buckets of troubles that need to be re-broadcasted at certain
 * intervals to re-announce troubles that have not cleared.
 * The troubles are placed in buckets based on the source of the
 * trouble (life safety, burg, system or IoT).
 *
 * See https://etwiki.sys.comcast.net/display/CPE/Categorizing++Troubles for
 * details on bucket definitions, behavior, and properties available.
 *
 * Author: jelderton -  2/25/19.
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <icTime/timeUtils.h>
#include <icTypes/icLinkedList.h>
#include <icSystem/hardwareCapabilities.h>
#include <icConcurrent/repeatingTask.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/propsService_eventAdapter.h>
#include "alarm/alarmPanel.h"
#include "zone/securityZonePrivate.h"
#include "trouble/replayTracker.h"
#include "trouble/troubleStatePrivate.h"
#include "common.h"
#include "internal.h"
#include "broadcastEvent.h"

/**
 **  General Approach:
 **
 **  When there are troubles present, this will get notified via troubleState.  At that time,
 **  a repeating task will be created to fire once-a-minute - which will iterate through the
 **  'buckets' to determine which troubles need to be 'replayed' (visually or audibly).  In that
 **  function we'll use time-math to determine which buckets are enabled and need to replay troubles.
 **
 **  Each trouble associated with the bucket needs to be examined and dealt with based on the
 **  'acknowledged' flag.  If the trouble is not-acknowledged, it will get replayed using the
 **  'announceInterval' (i.e. beep once a minute).  If the trouble was acknowledged, then it
 **  it will utilize the 'ackVisualInterval' and/or 'ackAudibleInterval' to determine if
 **  the trouble needs a replay for visual, audible, or both
 **
 **
 **  When all troubles are cleared, troubleState will tell us to cancel the repeating task.
 **
 **  We chose this "once-a-minute" approach because it's safer then trying to organize and track
 **  several threads (one per bucket).
 **/

/*
 * the following properties dictate the behavior of trouble announcement intervals.
 * utilizes the indicationCatagory as the key to these values.
 * for ex: a ' SYSTEM' trouble will use the corresponding property values to determine
 *         announcement interval for both acknowledged or un-acknowledged states
 *
 * see troubleReplaySettings below
 */
#define IOT_TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY      "cpe.troubles.iot.annunciationIntervalMinutes"
#define BURG_TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY     "cpe.troubles.burg.annunciationIntervalMinutes"
#define SAFETY_TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY   "cpe.troubles.safety.annunciationIntervalMinutes"
#define SYSTEM_TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY   "cpe.systemTroubles.annunciationIntervalMinutes"

#define IOT_TROUBLES_BEEP_ACK_EXPIRE_MINUTES_TIER_PROPERTY           "cpe.troubles.iot.annunciationAckExpireMinutes"
#define BURG_TROUBLES_BEEP_ACK_EXPIRE_MINUTES_TIER_PROPERTY          "cpe.troubles.burg.annunciationAckExpireMinutes"
#define SAFETY_TROUBLES_BEEP_ACK_EXPIRE_MINUTES_TIER_PROPERTY        "cpe.troubles.safety.annunciationAckExpireMinutes"
#define SYSTEM_TROUBLES_BEEP_ACK_EXPIRE_MINUTES_TIER_PROPERTY        "cpe.troubles.system.annunciationAckExpireMinutes"

#define SAFETY_TROUBLE_ANNUNCIATION_USE_SECONDS_TIER_PROPERTY        "cpe.troubles.safety.annunciationUseSeconds"

#define PROPERTY_PREFIX_1   "cpe.troubles."
#define PROPERTY_PREFIX_2   "cpe.systemTroubles."   // who messed that up?

#define DEFAULT_TROUBLE_INTERVAL_BURG_MINUTES   1     // The default minutes for un-acknowledged burg & safety troubles to be announced
#define DEFAULT_TROUBLE_INTERVAL_GEN_MINUTES    60    // The default minutes for un-acknowledged troubles to be announced
#define DEFAULT_INDICATE_INTERVAL_MINUTES       240   // The default minutes for acknowledged troubles to be re-announced (visual/audible)
#define MIN_TROUBLE_INTERVAL_MINUTES            1     // min number of minutes allowed for non-visual
#define MAX_TROUBLE_INTERVAL_MINUTES            1440  // max number of minutes allowed

// defaults used for scheduling (min vs sec)
#define REPLAY_TASK_MINUTES     1
#define REPLAY_TASK_SECONDS     10
#define REPLAY_IN_SECS_DEFAULT  false

// single "interval" definition of the property that drives it, the time between replay, and the repeating task handle.
// there will be one of these for each scenario per indicationCategory bucket (unack, ack-visual, ack-audible)
//
typedef struct _interval {
    const char *propertyKey;    // the CPE property that drives this interval
    uint32_t  minutes;          // number of minutes for this interval
    uint32_t  minMinutes;       // minimum number of minutes allowed for this interval
    time_t    lastExecTimeMono; // last time this interval executed (monotonic, not wall-clock time)
} interval;

// The 'bucket', which is used to hold the replay interval values for rebroadcasting trouble announcements
// (audible and visual).  When a trouble is 'replayed', it needs to use the correct value based on the
// ack state (and when it was acknowledged or created).
//
// There should be one of these objects for each indicationCatagory (see troubleReplayBuckets below)
//
typedef struct _troubleReplaySettings {
    interval announceInterval;      // interval for "un-acknowledged trouble" replay announcement for this category (visual and/or audible)
    interval snoozeAnnounceInterval;// interval for "acknowledged trouble" replay announcement.
                                    //   note: this may be disabled on some categories (i.e. beep until ack'd, then never again)
} troubleReplaySettings;


// array of troubleReplaySettings, indexd by the indicationCatagory value (IOT, BURG, SAFETY, SYSTEM).
//
static troubleReplaySettings troubleReplayBuckets[4];

// set to true when we are initialized and realize the system supports sounds & a screen
//
static bool enabled = false;

// one-minute repeating task handle.  started/stopped via troubleState when troubles
// are present or not in the system
//
static uint32_t checkReplayTask = 0;
static bool     checkReplyInSecs = false;

/*
 * private functions
 */
static void handlePropertyChangedEvent(cpePropertyEvent *event);
static void loadTroubleReplayBucketIntervals(troubleReplaySettings *bucket);
static void setBucketTime(troubleReplaySettings *bucket, time_t when, bool acknowledged);
static void restartRepeatingTask(bool applyNow);
static void replayTaskFunc(void *arg);
static void updateReplayTroubleEvent(troubleEvent *event);


/*
 * initialize the replay trackers.
 * assumes the global SECURITY_MTX is already held.
 */
void initTroubleReplayTrackers()
{
    // gather some property values needed for our 'replay' logic and update our troubleReplayBuckets.
    // note we only do this if we're on a system that supports sounds or display
    //
    if (supportSounds() == true)
    {
        // clear the array just to be safe
        //
        memset(troubleReplayBuckets, 0, sizeof(troubleReplaySettings) * 4);

        // assign properties and default values for each of our categories.  then see if we have property values to obtain
        //
        troubleReplayBuckets[INDICATION_CATEGORY_IOT].announceInterval.propertyKey   = IOT_TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY;
        troubleReplayBuckets[INDICATION_CATEGORY_IOT].announceInterval.minutes       = DEFAULT_TROUBLE_INTERVAL_GEN_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_IOT].announceInterval.minMinutes    = MIN_TROUBLE_INTERVAL_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_IOT].snoozeAnnounceInterval.propertyKey = IOT_TROUBLES_BEEP_ACK_EXPIRE_MINUTES_TIER_PROPERTY;
        troubleReplayBuckets[INDICATION_CATEGORY_IOT].snoozeAnnounceInterval.minutes     = DEFAULT_INDICATE_INTERVAL_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_IOT].snoozeAnnounceInterval.minMinutes  = 0;

        troubleReplayBuckets[INDICATION_CATEGORY_BURG].announceInterval.propertyKey   = BURG_TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY;
        troubleReplayBuckets[INDICATION_CATEGORY_BURG].announceInterval.minutes       = DEFAULT_TROUBLE_INTERVAL_BURG_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_BURG].announceInterval.minMinutes    = MIN_TROUBLE_INTERVAL_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_BURG].snoozeAnnounceInterval.propertyKey = BURG_TROUBLES_BEEP_ACK_EXPIRE_MINUTES_TIER_PROPERTY;
        troubleReplayBuckets[INDICATION_CATEGORY_BURG].snoozeAnnounceInterval.minutes     = DEFAULT_INDICATE_INTERVAL_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_BURG].snoozeAnnounceInterval.minMinutes  = 0;

        troubleReplayBuckets[INDICATION_CATEGORY_SAFETY].announceInterval.propertyKey   = SAFETY_TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY;
        troubleReplayBuckets[INDICATION_CATEGORY_SAFETY].announceInterval.minutes       = DEFAULT_TROUBLE_INTERVAL_BURG_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_SAFETY].announceInterval.minMinutes    = MIN_TROUBLE_INTERVAL_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_SAFETY].snoozeAnnounceInterval.propertyKey = SAFETY_TROUBLES_BEEP_ACK_EXPIRE_MINUTES_TIER_PROPERTY;
        troubleReplayBuckets[INDICATION_CATEGORY_SAFETY].snoozeAnnounceInterval.minutes     = DEFAULT_INDICATE_INTERVAL_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_SAFETY].snoozeAnnounceInterval.minMinutes  = 0;

        troubleReplayBuckets[INDICATION_CATEGORY_SYSTEM].announceInterval.propertyKey   = SYSTEM_TROUBLE_ANNUNCIATION_INTERVAL_MINUTES_TIER_PROPERTY;
        troubleReplayBuckets[INDICATION_CATEGORY_SYSTEM].announceInterval.minutes       = DEFAULT_TROUBLE_INTERVAL_GEN_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_SYSTEM].announceInterval.minMinutes    = MIN_TROUBLE_INTERVAL_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_SYSTEM].snoozeAnnounceInterval.propertyKey = SYSTEM_TROUBLES_BEEP_ACK_EXPIRE_MINUTES_TIER_PROPERTY;
        troubleReplayBuckets[INDICATION_CATEGORY_SYSTEM].snoozeAnnounceInterval.minutes     = DEFAULT_INDICATE_INTERVAL_MINUTES;
        troubleReplayBuckets[INDICATION_CATEGORY_SYSTEM].snoozeAnnounceInterval.minMinutes  = 0;

        // load the property values for each category & interval
        //
        indicationCategory idx = INDICATION_CATEGORY_IOT;
        while (idx <= INDICATION_CATEGORY_SAFETY)
        {
            loadTroubleReplayBucketIntervals(&troubleReplayBuckets[idx]);
            idx++;
        }

        // get initial value for our task to be in seconds or minutes
        //
        checkReplyInSecs = getPropertyAsBool(SAFETY_TROUBLE_ANNUNCIATION_USE_SECONDS_TIER_PROPERTY, REPLAY_IN_SECS_DEFAULT);

        // register for property change events so that we keep our values in sync
        //
        register_cpePropertyEvent_eventListener(handlePropertyChangedEvent);

        // set our flag, and wait for troubleState to tell us to begin the timer (because troubles exist)
        //
        enabled = true;
    }
}

/*
 * cleanup the replay trackers (during shutdown).
 * assumes the global SECURITY_MTX is already held.
 */
void shutdownTroubleReplayTrackers()
{
    if (enabled == true)
    {
        // cleanup trouble repeating task
        //
        stopTroubleReplayTrackers();

        // cleanup event listener to support trouble replay
        //
        unregister_cpePropertyEvent_eventListener(handlePropertyChangedEvent);
    }
}

/*
 * called by troubleState to possibly begin replay tracking
 * because there is now a trouble in the system.
 * assumes the global SECURITY_MTX is already held.
 */
void startTroubleReplayTrackers(troubleEvent *event)
{
    if (event == NULL || event->trouble == NULL)
    {
        icLogWarn(SECURITY_LOG, "%s: event or trouble is NULL", __FUNCTION__);
        return;
    }

    // TODO: should we ignore hidden troubles (INDICATION_NONE)?
    // ignore certain troubles
    //
    switch(event->trouble->reason)
    {
        case TROUBLE_REASON_SWINGER:
        case TROUBLE_REASON_GENERIC:
            return;

        default:
            break;
    }

    // start the repeating task (if not already running)
    //
    if (enabled == true)
    {
        // find this bucket and set the "time" to now if it's at 0
        //
        time_t now = getCurrentTime_t(true);
        setBucketTime(&troubleReplayBuckets[event->trouble->indicationGroup], now, event->trouble->acknowledged);

        // potentially reset the timer for this bucket if it's the last trouble to be acknowledged
        //
        if (event->trouble->acknowledged == true)
        {
            // Check if all troubles in the group have been acknowledged and are of indication type that is
            // acknowledgable (i.e. don't count hidden ones)
            troubleFilterConstraints *constraints = createTroubleFilterConstraints();
            constraints->ackValue = TROUBLE_ACK_NO;
            indicationType allowedTypes[] = {INDICATION_VISUAL, INDICATION_BOTH};
            constraints->allowedIndicationTypes->types = allowedTypes;
            constraints->allowedIndicationTypes->size = 2;

            uint32_t count = getTroubleCategoryCountPrivate(event->trouble->indicationGroup, constraints);
            if (count == 0)
            {
                // all troubles for this group are acknowledged.  we can reset the snooze interval
                //
                icLogDebug(SECURITY_LOG, "replay: no more un-ack troubles for group %s; resetting timer", indicationCategoryLabels[event->trouble->indicationGroup]);
                troubleReplayBuckets[event->trouble->indicationGroup].snoozeAnnounceInterval.lastExecTimeMono = now;
            }
            destroyTroubleFilterConstraints(constraints);
        }

        // start the repeating task if necessary
        //
        if (checkReplayTask == 0)
        {
            if (checkReplyInSecs == true)
            {
                // run our task every 10 seconds
                icLogDebug(SECURITY_LOG, "replay: starting repeating task for trouble replay (in seconds)....");
                checkReplayTask = createFixedRateRepeatingTask(REPLAY_TASK_SECONDS, DELAY_SECS, replayTaskFunc, NULL);
            }
            else
            {
                // run our task once a minute
                icLogDebug(SECURITY_LOG, "replay: starting repeating task for trouble replay (in minutes)....");
                checkReplayTask = createFixedRateRepeatingTask(REPLAY_TASK_MINUTES, DELAY_MINS, replayTaskFunc, NULL);
            }
        }

        // if we just added a 'life safety' trouble, then we need to make it take priority over
        // all other other troubles (see Jira HH-2508).  we will accomplish that by resetting all other
        // active buckets to use 'now' as the last exec time; effectively synchronizing all bucket timers
        // and leveraging the sorting so that we play life-safety beeps first.
        //
        else if (event->trouble->indicationGroup == INDICATION_CATEGORY_SAFETY &&
                 troubleReplayBuckets[event->trouble->indicationGroup].announceInterval.lastExecTimeMono != 0)
        {
            // assuming we just did the 'beep' for this life-safety event, reset the exec time and
            // reschedule our repeating task to fire from this point.  we are preventing a mid-cycle
            // beep throwing off all of the timings - ZILKER-2677
            // For example, if the first life-safety trouble is beeping on the 5's ( 00:15, 00:25, 00:35),
            // and we receive a second life-safety trouble at 00:28, shift the loop to now beep on the 8's.
            //
            icLogDebug(SECURITY_LOG, "replay: re-starting intervals to escalate life-safety");
            if (troubleReplayBuckets[INDICATION_CATEGORY_IOT].announceInterval.lastExecTimeMono != 0)
            {
                troubleReplayBuckets[INDICATION_CATEGORY_IOT].announceInterval.lastExecTimeMono = now;
            }
            if (troubleReplayBuckets[INDICATION_CATEGORY_BURG].announceInterval.lastExecTimeMono != 0)
            {
                troubleReplayBuckets[INDICATION_CATEGORY_BURG].announceInterval.lastExecTimeMono = now;
            }
            if (troubleReplayBuckets[INDICATION_CATEGORY_SYSTEM].announceInterval.lastExecTimeMono != 0)
            {
                troubleReplayBuckets[INDICATION_CATEGORY_SYSTEM].announceInterval.lastExecTimeMono = now;
            }

            // only reset the repeating task if this is a new unacknowledged event
            // (we don't want the 'ack' to alter our timing)
            //
            if (event->trouble->acknowledged == false)
            {
                troubleReplayBuckets[event->trouble->indicationGroup].announceInterval.lastExecTimeMono = now;
                restartRepeatingTask(true);
            }
        }
    }
}

/*
 * taskCallbackFunc to cancel the repeating task
 */
static void cancelRepeatingTaskDelayed(void *arg)
{
    // cancel the repeating task
    uint32_t *handle = (uint32_t *)arg;
    cancelRepeatingTask(*handle);

    // mem cleanup
    free(arg);
}

/*
 * called by troubleState to possibly cancel replay tracking
 * because there are NO troubles in the system.
 * assumes the global SECURITY_MTX is already held.
 */
void stopTroubleReplayTrackers()
{
    // cancel the repeating task
    //
    if (enabled == true)
    {
        // cancel the repeating task
        //
        if (checkReplayTask > 0)
        {
            // the problem is that we have the lock and could deadlock trying
            // to cancel.  therefore, we'll delay the cancel and reset our handle
            //
            icLogDebug(SECURITY_LOG, "replay: stopping repeating task for trouble replay....");
            uint32_t *arg = (uint32_t *)malloc(sizeof(uint32_t));
            *arg = checkReplayTask;
            checkReplayTask = 0;
            scheduleDelayTask(250, DELAY_MILLIS, cancelRepeatingTaskDelayed, arg);
        }

        // reset the 'time' in all of the buckets
        //
        indicationCategory idx = INDICATION_CATEGORY_IOT;
        while (idx <= INDICATION_CATEGORY_SYSTEM)
        {
            setBucketTime(&troubleReplayBuckets[idx], 0, false);
            idx++;
        }
    }
}

/*
 * called by troubleState to reset the 'time' on a category/bucket
 * because there are NO troubles in the system for this category.
 * assumes the global SECURITY_MTX is already held.
 */
void resetCategoryReplayTracker(indicationCategory category)
{
    if (enabled == true)
    {
        // reset the 'time' of this bucket
        //
        setBucketTime(&troubleReplayBuckets[category], 0, false);
    }
}


/*
 * restart our repeating task
 */
static void restartRepeatingTask(bool applyNow)
{
    if (checkReplayTask != 0)
    {
        // reschedule
        //
        if (checkReplyInSecs == true)
        {
            // run our task every 10 seconds
            icLogDebug(SECURITY_LOG, "replay: re-scheduling repeating task for trouble replay (in seconds)....");
            changeRepeatingTask(checkReplayTask, REPLAY_TASK_SECONDS, DELAY_SECS, applyNow);
        }
        else
        {
            // run our task once a minute
            icLogDebug(SECURITY_LOG, "replay: re-scheduling repeating task for trouble replay (in minutes)....");
            changeRepeatingTask(checkReplayTask, REPLAY_TASK_MINUTES, DELAY_MINS, applyNow);
        }
    }
}

/*
 * load the property and apply it for the given interval.
 * assumes this is called during init and the lock is held
 */
static void loadTroubleReplayInterval(interval *target)
{
    // load the property value and attempt to apply it
    //
    int32_t value = getPropertyAsInt32(target->propertyKey, -1);
    if (value < (int32_t)target->minMinutes)
    {
        return;
    }
    if (value > MAX_TROUBLE_INTERVAL_MINUTES)
    {
        value = MAX_TROUBLE_INTERVAL_MINUTES;
    }

    // apply the value, and wipe the current "last used time"
    //
    icLogDebug(SECURITY_LOG, "replay: initializing %s to %"PRIu32" minutes", target->propertyKey, value);
    target->minutes = (uint32_t)value;
    target->lastExecTimeMono = 0;
}

/*
 * load the CPE properties for each 'interval' within ths supplied bucket.
 * called during init to get the initial property values, and rely on
 * property change events to update these.
 */
static void loadTroubleReplayBucketIntervals(troubleReplaySettings *bucket)
{
    // for each 'interval', load the property value.  if set, then attempt to apply
    // as long as the value is within the limits (or cap it within the limits)
    //
    loadTroubleReplayInterval(&bucket->announceInterval);
    loadTroubleReplayInterval(&bucket->snoozeAnnounceInterval);
}

/*
 * called when we received a property changed event
 */
static void applyPropertyChange(uint32_t eventValue, interval *target)
{
    // only valid if within the bounds
    //
    if (eventValue >= target->minMinutes)
    {
        // apply while the lock is held
        //
        lockSecurityMutex();
        if (eventValue != target->minutes)
        {
            icLogDebug(SECURITY_LOG, "replay: updating %s to %"PRIu32" minutes", target->propertyKey, eventValue);
            target->minutes = eventValue;
        }
        unlockSecurityMutex();
    }
}

/*
 * event handler for CPE property change envents
 */
static void handlePropertyChangedEvent(cpePropertyEvent *event)
{
    if (event == NULL || event->propKey == NULL)
    {
        return;
    }

    // ignore the event if we're not enabled
    //
    lockSecurityMutex();
    if (enabled == false)
    {
        unlockSecurityMutex();
        return;
    }
    unlockSecurityMutex();

    // look for "use seconds instead of minutes" property
    //
    if (strcmp(event->propKey, SAFETY_TROUBLE_ANNUNCIATION_USE_SECONDS_TIER_PROPERTY) == 0)
    {
        // get the value of the property
        //
        bool newVal = REPLAY_IN_SECS_DEFAULT;
        if (event->baseEvent.eventValue != GENERIC_PROP_DELETED)
        {
            newVal = getPropertyEventAsBool(event, REPLAY_IN_SECS_DEFAULT);
        }

        lockSecurityMutex();
        if (newVal != checkReplyInSecs)
        {
            // update our secs/min value and potentially reschedule our repeating task
            //
            icLogDebug(SECURITY_LOG, "replay: setting 'use seconds' to %s", (newVal == true) ? "true" : "false");
            checkReplyInSecs = newVal;

            // now restart our repeating task (if running)
            //
            restartRepeatingTask(true);
        }
        unlockSecurityMutex();
    }

    // we need to lock if this property change is a match to any of our bucket intervals.
    // to keep the locking to a minimum, we'll see of the key starts with something that most
    // likely will apply
    //
    if ((stringStartsWith(event->propKey, PROPERTY_PREFIX_1, false) == false) &&
        (stringStartsWith(event->propKey, PROPERTY_PREFIX_2, false) == false))
    {
        return;
    }

    // assume this will get used, so convert the value and check the boundries
    //
    int32_t value = getPropertyEventAsInt32(event, -1);
    if (value < 0)
    {
        // bogus
        return;
    }
    if (value > MAX_TROUBLE_INTERVAL_MINUTES)
    {
        value = MAX_TROUBLE_INTERVAL_MINUTES;
    }

    // safe to loop through these without holding the lock since the
    // objects and keys are not changing (only the values & time_t needs the lock)
    //
    indicationCategory idx = INDICATION_CATEGORY_IOT;
    while (idx <= INDICATION_CATEGORY_SAFETY)
    {
        if (strcmp(event->propKey, troubleReplayBuckets[idx].announceInterval.propertyKey) == 0)
        {
            // apply then break (no need to keep looping)
            //
            applyPropertyChange((uint32_t)value, &troubleReplayBuckets[idx].announceInterval);
            break;
        }
        else if (strcmp(event->propKey, troubleReplayBuckets[idx].snoozeAnnounceInterval.propertyKey) == 0)
        {
            // apply then break (no need to keep looping)
            //
            applyPropertyChange((uint32_t)value, &troubleReplayBuckets[idx].snoozeAnnounceInterval);
            break;
        }

        // go to the next bucket
        //
        idx++;
    }
}

/*
 * used to initialize or reset the time values in each interval of the bucket
 */
static void setBucketTime(troubleReplaySettings *bucket, time_t when, bool acknowledged)
{
    if (when != 0)
    {
        // only apply time to intervals that don't have a time set
        //
        if (acknowledged == false)
        {
            // only adjust the announcement interval.  the thought here
            // is that we're setting this up initially so don't want to
            // skew the 'ack' intervals until something is acknowledged
            //
            if (bucket->announceInterval.lastExecTimeMono == 0)
            {
                bucket->announceInterval.lastExecTimeMono = when;
            }
        }
        else
        {
            if (bucket->snoozeAnnounceInterval.lastExecTimeMono == 0)
            {
                bucket->snoozeAnnounceInterval.lastExecTimeMono = when;
            }
        }
    }
    else
    {
        // doing a reset.  just clear all
        //
        bucket->announceInterval.lastExecTimeMono = 0;
        bucket->snoozeAnnounceInterval.lastExecTimeMono = 0;
    }
}

/*
 * do the time math to see if the difference between "now"
 * and the last time "target" was fired exceeds the setting
 *
 * if 'overrideSecs' > 0, it will be used instead of target->minutes
 */
static bool hasIntervalElapsed(time_t now, interval *target, uint32_t overrideSecs)
{
    bool retVal = false;

    // skip if this interval has no exec time established
    //
    if (target->lastExecTimeMono == 0)
    {
        return retVal;
    }

    // get the elapsed time in seconds, then convert to minutes
    //
    time_t elapsedSecs = (now - target->lastExecTimeMono);
    uint32_t elapsedMin = (uint32_t) (elapsedSecs / 60);
    bool override = (overrideSecs > 0);

    icLogTrace(SECURITY_LOG, "%s: Now Secs = %ld; lastExecTimeMono Secs = %ld; Delay Time Mins = %"PRIu32"; Elapsed Time Secs =%ld; Override = %s; Override Secs = %"PRIu32,
            __FUNCTION__,
            now,
            target->lastExecTimeMono,
            target->minutes,
            elapsedSecs,
            override ? "true": "false",
            overrideSecs);

    if (override)
    {
        // calculate based on seconds elapsed
        //
        if (elapsedSecs >= overrideSecs)
        {
            // need to fire now
            //
            retVal = true;
        }
    }
    else
    {
        // calculate based on minutes elapsed
        //
        if (elapsedMin >= target->minutes)
        {
            // need to fire now
            //
            retVal = true;
        }
    }

    return retVal;
}

/*
 * taskCallbackFunc that is called once-per-minute via repeatingTask
 * this is where the rubber meets the road....
 */
static void replayTaskFunc(void *arg)
{
    bool sentBeep = false;

    // get current monotonic time
    //
    time_t now = getCurrentTime_t(true);

    // get a copy of all troubles, then loop through them.  note that we use
    // the "public" function so that we don't have to obtain the lock.
    // we ask for the troubles sorted by INDICATION_GROUP (descending) so that we
    // play any Life Safety or System troubles before short-circuiting the loop.
    //
    icLinkedList *allTroubles = linkedListCreate();
    getTroublesPublic(allTroubles, TROUBLE_FORMAT_EVENT, true, TROUBLE_SORT_BY_INDICATION_GROUP);
    icLinkedListIterator *loop = linkedListIteratorCreate(allTroubles);

    // variable to track previous bucket category while iterating on allTroubles
    //
    indicationCategory prevIndGroup = -1;

    // variable to forceReplay if we need to
    //
    bool forceReplay = false;

    while (linkedListIteratorHasNext(loop) == true)
    {
        // skip this event if it's totally hidden from the user
        //
        troubleEvent *event = (troubleEvent *)linkedListIteratorGetNext(loop);
//icLogDebug(SECURITY_LOG, "replay: looking at trouble; group=%s vis=%s troubleId=%"PRIu64,
//                   indicationCategoryLabels[event->trouble->indicationGroup],
//                   indicationTypeLabels[event->trouble->indication],
//                   event->trouble->troubleId);
        if (event->trouble->indication == INDICATION_NONE)
        {
            continue;
        }

        // find the bucket this trouble falls into.
        //
        troubleReplaySettings *bucket = &troubleReplayBuckets[event->trouble->indicationGroup];
        lockSecurityMutex();

        // look at the trouble's "ack" value to see which interval to use
        //
        if (event->trouble->acknowledged == false)
        {
            // not acknowledged, so use announceInterval.  before we do, see if
            // this is the LIFE_SAFETY bucket and the "use seconds" option is set
            // OR this is the SYSTEM bucket and we have a life-safety device
            //
            uint32_t overrideSecs = 0;
            if (checkReplyInSecs == true)
            {
                if (event->trouble->indicationGroup == INDICATION_CATEGORY_SAFETY ||
                    (event->trouble->indicationGroup == INDICATION_CATEGORY_SYSTEM && haveLifeSafetyZonePrivate() == true))
                {
                    // force us to beep every 10 seconds
                    //
                    overrideSecs = REPLAY_TASK_SECONDS;
                }
            }

            // If the previous and current trouble in the list belong
            // to same category/bucket e.g. INDICATION_CATEGORY_BURG
            // then hasIntervalElapsed will return false for current trouble,
            // because we just replayed that bucket's trouble.
            // So, only the trouble present on the top of the list in the bucket gets replayed
            // other troubles of that bucket down the list get ignored because announceInterval for the
            // bucket has not passed 1min.
            // To process remaining troubles of that bucket, we need to forceReplay for that bucket
            // If current trouble category is same as the previous one
            //
            if (prevIndGroup != -1
                && event->trouble->indicationGroup == prevIndGroup)
            {
                forceReplay = true;
            }

            // check the time since the last 'beep'
            // Replay will be forced if the forceReplay is set regardless of announceInterval
            //
            if (hasIntervalElapsed(now, &bucket->announceInterval, overrideSecs) == true
                || forceReplay == true)
            {
                // need to 'beep' by resending this trouble, but make this visual only if we
                // already sent something audible this minute (don't beep multiple times)
                //
                indicationType showMe = INDICATION_VISUAL;

                // only try to beep if the original indication included a beep (and we did
                // not send one yet this execution)
                //
                if (sentBeep == false && event->trouble->indication == INDICATION_BOTH)
                {
                    showMe = INDICATION_BOTH;
                }

                // apply the indication, update the alarm/panel details, then re-send the trouble event.
                // we tack on the "REPLAY_VALUE" to the eventValue so that non user-interfacing
                // receivers don't attempt to interpret the trouble.  for ex: commService & rules
                //
                icLogDebug(SECURITY_LOG, "replay: re-sending un-ack trouble for bucket %s with forceReplay=%d", indicationCategoryLabels[event->trouble->indicationGroup], forceReplay);
                event->trouble->indication = showMe;
                updateReplayTroubleEvent(event);

                // return value of broadcastTroubleEvent will determine is a beep was sent or not
                //
                indicationType indication = broadcastTroubleEvent(event, TROUBLE_OCCURED_EVENT, TROUBLE_EVENT_REPLAY_VALUE);
                sentBeep = (indication == INDICATION_AUDIBLE || indication == INDICATION_BOTH);

                // reset the exec time on this interval
                //
                bucket->announceInterval.lastExecTimeMono = now;
                //record this eventCategory in previousIndicationGroup
                //
                prevIndGroup = event->trouble->indicationGroup;

                // reset forceReplay if already set
                //
                if (forceReplay == true)
                {
                    forceReplay = false;
                }
            }
        }
        else
        {
            // we have a trouble in "snooze mode".  need to check snoozeAnnounceInterval
            // to see if it's time to re-send the event (no override of the time).
            //
            if (bucket->snoozeAnnounceInterval.minutes > 0 && hasIntervalElapsed(now, &bucket->snoozeAnnounceInterval, 0) == true)
            {
                // trouble needs to be re-announced, check and see if there was already a beep sent, and if not,
                // just visually indicate the trouble
                //
                if (sentBeep == false)
                {
                    event->trouble->indication = INDICATION_BOTH;
                }
                else
                {
                    event->trouble->indication = INDICATION_VISUAL;
                }

                // we tack on the "REPLAY_VALUE" to the eventValue so that non user-interfacing
                // receivers don't attempt to interpret the trouble.  for ex: commService & rules
                //
                updateReplayTroubleEvent(event);
                icLogDebug(SECURITY_LOG, "replay: re-sending ack trouble for bucket %s", indicationCategoryLabels[event->trouble->indicationGroup]);
                indicationType indication = broadcastTroubleEvent(event, TROUBLE_OCCURED_EVENT, TROUBLE_EVENT_REPLAY_VALUE);

                // return value of broadcastTroubleEvent will determine if a beep was sent or not
                // this prevents us from filtering out the beep for deferrable troubles and preventing
                // non-deferrable troubles from beeping
                //
                sentBeep = (indication == INDICATION_AUDIBLE || indication == INDICATION_BOTH);


                // reset the exec time on this interval
                //
                icLogDebug(SECURITY_LOG, "replay: resetting ack and announce timer on bucket %s", indicationCategoryLabels[event->trouble->indicationGroup]);
                bucket->snoozeAnnounceInterval.lastExecTimeMono = now;
                bucket->announceInterval.lastExecTimeMono = now;

                // reset the ack flag on this trouble, but don't send another event
                //
                unacknowledgeTroublePrivate(event->trouble->troubleId, false);

                // in the odd case that this trouble has never been "announced" set the time to now
                // this will allow it to be announced again in the future
                // this will happen to troubles that were acknowledged on bootup
                //
                if(bucket->announceInterval.lastExecTimeMono == 0)
                {
                    bucket->announceInterval.lastExecTimeMono = now;
                }
            }
        }
        unlockSecurityMutex();
    }

    // cleanup
    //
    linkedListIteratorDestroy(loop);
    linkedListDestroy(allTroubles, destroyTroubleEventFromList);
}

/**
 * Updates information about a troubleEvent so that consumers of a rebroadcast have up-to-date information. Meant to be
 * used with replay troubles. Assumes security mutex is locked!
 * @param event
 */
static void updateReplayTroubleEvent(troubleEvent *event)
{
    //Repopulate the panel status
    populateSystemPanelStatusPrivate(event->panelStatus);

    //Repopulate the alarm details
    populateSystemCurrentAlarmStatusPrivate(event->alarm);
}
