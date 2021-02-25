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
 * securitySystemTracker
 * 
 * Collects events that need to be added into our statistics gathering.
 *
 * Author: jelder380 - 5/6/19.
 *-----------------------------------------------*/

#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <icLog/logging.h>

#include <icTypes/icLinkedList.h>
#include <icTime/timeUtils.h>
#include <icUtil/stringUtils.h>
#include "common.h"
#include "securitySystemTracker.h"

/*
 * The keys to be add into stats
 */

// arm and disarm failure keys
#define ARM_FAILURE_KEY     "armFailure"
#define DISARM_FAILURE_KEY  "disarmFailure"

/*
 * Values to be add into stats
 */

// arm failure reasons
#define ARM_FAILURE_TROUBLE_REASON                  "TROUBLE"
#define ARM_FAILURE_ZONE_REASON                     "ZONE"
#define ARM_FAILURE_ALREADY_ARMED_REASON            "ALREADY_ARMED"
#define ARM_FAILURE_UPGRADE_REASON                  "UPGRADE"
#define ARM_FAILURE_SO_MANY_DEVICES_REASON          "TOO_MANY_DEVICES"
#define ARM_FAILURE_ACCOUNT_SUSPENDED_REASON        "ACCOUNT_SUSPENDED"
#define ARM_FAILURE_ACCOUNT_DEACTIVATED_REASON      "ACCOUNT_DEACTIVATED"

// disarm reasons
#define DISARM_FAILURE_ALREADY_DISARMED_REASON      "ALREADY_DISARMED"

// both arm and disarm reasons
#define ARM_DISARM_FAILURE_INVALID_ARG_REASON       "INVALID_ARGS"
#define ARM_DISARM_FAILURE_USER_CODE_REASON         "USER_CODE"
#define ARM_DISARM_FAILURE_INTERNAL_SYSTEM_REASON   "INTERNAL_SYS_FAIL"

/*
 * Object(s) to go into hashmap
 */

// the type of event
typedef enum
{
    ARM_FAILURE_EVENT = 0,
    DISARM_FAILURE_EVENT
}eventType;

static const char *eventTypeLabels[] = {
    "ARM_FAILURE_EVENT",
    "DISARM_FAILURE_EVENT",
    NULL
};

// the event with reason and type
typedef struct
{
    time_t eventTime;
    int value;
    eventType type;
}failureEvent;

/*
 * Hash map(s) containing the events
 */
static pthread_mutex_t trackerMtx = PTHREAD_MUTEX_INITIALIZER;
static icLinkedList *armDisarmFailureCollection = NULL;

/*
 * private functions
 */
static void addFailureEventInternal(int eventValue, eventType type);
static bool isArmFailureEvent(armResultType armResult);
static bool isDisarmFailureEvent(disarmResultType disarmResult);
static const char *determineArmFailureReason(armResultType armResult);
static const char *determineDisarmFailureReason(disarmResultType disarmResult);

/**
 * Checks and adds arm failure reasons, will only
 * keep track of the events if arm reason is not successful
 *
 * @param armResult - the result of trying to arm the system
 */
void addArmFailureEventToTracker(armResultType armResult)
{
    // see if it is a failure event
    //
    if (isArmFailureEvent(armResult) == false)
    {
        return; // just bail
    }

    // if so add it
    //
    addFailureEventInternal(armResult, ARM_FAILURE_EVENT);
}

/**
 * Checks and adds disarm failure reasons, will only
 * keep track of the events if disarm reason is not successful
 *
 * @param disarmResult - the result of trying to disarm the system
 */
void addDisarmFailureEventToTracker(disarmResultType disarmResult)
{
    // see if it is a failure event
    //
    if (isDisarmFailureEvent(disarmResult) == false)
    {
        return; // just bail
    }

    // if so add it
    //
    addFailureEventInternal(disarmResult, DISARM_FAILURE_EVENT);
}

/**
 * Collects the events and adds them into the runtime
 * stats hash map.
 *
 * NOTE: will remove rest the events once collected.
 *
 * @param output - the stats hash map
 */
void collectArmDisarmFailureEvents(runtimeStatsPojo *output)
{
    // need to reset collection and copy it,
    // this is to not prevent events from
    // coming in since the lock is needed
    //
    pthread_mutex_lock(&trackerMtx);
    icLinkedList *tmpCollection = armDisarmFailureCollection;
    armDisarmFailureCollection = linkedListCreate();
    pthread_mutex_unlock(&trackerMtx);

    // and iterate through it
    //
    icLinkedListIterator *collectionIter = linkedListIteratorCreate(tmpCollection);
    while(linkedListIteratorHasNext(collectionIter))
    {
        failureEvent *event = (failureEvent *) linkedListIteratorGetNext(collectionIter);

        // sanity check
        if (event == NULL)
        {
            continue;
        }

        // determine if arm or disarm failure event
        // create key and get the simplified string value
        //
        char *key = NULL;
        const char *eventValue = NULL;
        if (event->type == ARM_FAILURE_EVENT)
        {
            key = stringBuilder("%s_%ld", ARM_FAILURE_KEY, event->eventTime);
            eventValue = determineArmFailureReason(event->value);
        }
        else
        {
            key = stringBuilder("%s_%ld", DISARM_FAILURE_KEY, event->eventTime);
            eventValue = determineDisarmFailureReason(event->value);
        }

        // add to stats
        //
        if (eventValue != NULL)
        {
            put_string_in_runtimeStatsPojo(output, key, (char *) eventValue);
        }

        // cleanup
        free(key);
    }

    // cleanup
    //
    linkedListIteratorDestroy(collectionIter);
    linkedListDestroy(tmpCollection, NULL);
}

/**
 * Initializes the security system tracker
 */
void initSecuritySystemTracker()
{
    pthread_mutex_lock(&trackerMtx);
    armDisarmFailureCollection = linkedListCreate();
    pthread_mutex_unlock(&trackerMtx);
}

/**
 * Cleans up the security system tracker
 */
void destroySecuritySystemTracker()
{
    pthread_mutex_lock(&trackerMtx);
    linkedListDestroy(armDisarmFailureCollection, NULL);
    pthread_mutex_unlock(&trackerMtx);
}

/**
 * Adds the event whether it is arm/disarm failure events.
 * Creates the event, creates a time stamp, and adds it to
 * the collection.
 *
 * NOTE: will grab the lock when adding the events into the collection
 *
 * @param eventValue - the arm/disarm failure reason
 * @param type - whether the event was a arm/disarm failure reason
 */
static void addFailureEventInternal(int eventValue, eventType type)
{
    // create the new object
    //
    failureEvent *event = calloc(1, sizeof(failureEvent));
    event->eventTime = getCurrentTime_t(false);
    event->type = type;
    event->value = eventValue;

    pthread_mutex_lock(&trackerMtx);

    // add into hash map
    //
    if (linkedListAppend(armDisarmFailureCollection, event) == false)
    {
        // could not add for some reason, need to clean up
        //
        icLogError(SECURITY_LOG, "%s: unable to add %s failure event %s into event tracker",
                   __FUNCTION__, eventTypeLabels[type], type == ARM_FAILURE_EVENT ?
                   armResultTypeLabels[eventValue] : disarmResultTypeLabels[eventValue]);
        free(event);
    }
    else
    {
        icLogDebug(SECURITY_LOG, "%s: added %s failure event %s into event tracker",
                   __FUNCTION__, eventTypeLabels[type], type == ARM_FAILURE_EVENT ?
                   armResultTypeLabels[eventValue] : disarmResultTypeLabels[eventValue]);
    }

    pthread_mutex_unlock(&trackerMtx);
}

/**
 * Helper function for determining if the arm result
 * is considered a failure or not
 *
 * @param armResult - the arm result from trying to arm the system
 * @return - true if result is a failure, false if otherwise
 */
static bool isArmFailureEvent(armResultType armResult)
{
    if (armResult == SYSTEM_ARM_SUCCESS)
    {
        return false;
    }
    else
    {
        return true;
    }
}

/**
 * Helper function for determining if the disarm result
 * is considered a failure or not
 *
 * @param disarmResult - the disarm result from trying to disarm the system
 * @return - true if result is a failure, false if otherwise
 */
static bool isDisarmFailureEvent(disarmResultType disarmResult)
{
    if (disarmResult == SYSTEM_DISARM_SUCCESS)
    {
        return false;
    }
    else
    {
        return true;
    }
}

/**
 * Helper function for determining the simple string
 * for the arm failure reason
 *
 * @param armResult - the arm result from trying to arm the system
 * @return - the simple string for the result, or NULL if should not be a failure
 */
static const char *determineArmFailureReason(armResultType armResult)
{
    switch (armResult)
    {
        case SYSTEM_ARM_INVALID_ARGS:
            return ARM_DISARM_FAILURE_INVALID_ARG_REASON;
        case SYSTEM_ARM_FAIL_TROUBLE:
            return ARM_FAILURE_TROUBLE_REASON;
        case SYSTEM_ARM_FAIL_ZONE:
            return ARM_FAILURE_ZONE_REASON;
        case SYSTEM_ARM_FAIL_USERCODE:
            return ARM_DISARM_FAILURE_USER_CODE_REASON;
        case SYSTEM_ARM_SYS_FAILURE:
            return ARM_DISARM_FAILURE_INTERNAL_SYSTEM_REASON;
        case SYSTEM_ARM_ALREADY_ARMED:
            return ARM_FAILURE_ALREADY_ARMED_REASON;
        case SYSTEM_ARM_FAIL_UPGRADE:
            return ARM_FAILURE_UPGRADE_REASON;
        case SYSTEM_ARM_FAIL_TOO_MANY_SECURITY_DEVICES:
            return ARM_FAILURE_SO_MANY_DEVICES_REASON;
        case SYSTEM_ARM_FAIL_ACCOUNT_SUSPENDED:
            return ARM_FAILURE_ACCOUNT_SUSPENDED_REASON;
        case SYSTEM_ARM_FAIL_ACCOUNT_DEACTIVATED:
            return ARM_FAILURE_ACCOUNT_DEACTIVATED_REASON;
        default:
            return NULL;    // this should never happen
    }
}

/**
 * Helper function for determining the simple string
 * for the disarm failure reason
 *
 * @param disarmResult - the disarm result from trying to disarm the system
 * @return - the simple string for the result, or NULL if should not be a failure
 */
static const char *determineDisarmFailureReason(disarmResultType disarmResult)
{
    switch (disarmResult)
    {
        case SYSTEM_DISARM_INVALID_ARGS:
            return ARM_DISARM_FAILURE_INVALID_ARG_REASON;
        case SYSTEM_DISARM_FAIL_USERCODE:
            return ARM_DISARM_FAILURE_USER_CODE_REASON;
        case SYSTEM_DISARM_SYS_FAILURE:
            return ARM_DISARM_FAILURE_INTERNAL_SYSTEM_REASON;
        case SYSTEM_DISARM_ALREADY_DISARMED:
            return DISARM_FAILURE_ALREADY_DISARMED_REASON;
        default:
            return NULL;    // this should never happen
    }
}

//+++++++++++
//EOF
//+++++++++++
