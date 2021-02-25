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

//
// Created by mkoch201 on 2/20/19.
//

#include "xhCron/cronEventRegistrar.h"
#include "xhCron/cron_eventAdapter.h"
#include "xhCron/crontab.h"
#include <icTypes/icHashMap.h>
#include <propsMgr/paths.h>

#include <pthread.h>
#include <stdio.h>

static pthread_mutex_t MTX = PTHREAD_MUTEX_INITIALIZER;
static icHashMap *registrations = NULL;
static char *scheduleScript = NULL;

static void eventHandler(cronEvent *event);
static void mapFreeFunc(void *key, void *value);
static char *buildEntryLine(const char *name, const char *schedule);
static void unregisterForCronEventInternal(const char *name, bool clearCronEntry);

/**
 * Register to receive a callback when a cron event happens
 *
 * @param name the globally unique name for this entry, use serviceName to help with uniqueness
 * @param schedule the cron schedule, can be NULL to try and just listen for an existing cron
 * tab entry, though this is atypical.  Most services should be passing the schedule they want
 * so if its not there yet it will be created.
 * @param callback the callback for when the cron entry fires
 * @return true if successfully registered
 */
bool registerForCronEvent(const char *name, const char *schedule, cronEventHandler callback)
{
    bool retval = false;

    if (name != NULL)
    {
        pthread_mutex_lock(&MTX);

        bool cronTabEntryCreated;
        if (schedule == NULL)
        {
            // This probably isn't all that useful for real code, but its helps in the utility
            // coverity[sleep]
            cronTabEntryCreated = hasCrontabEntry(NULL, name) >= 0;
        }
        else
        {
            char *entryLine = buildEntryLine(name, schedule);
            // coverity[sleep]
            cronTabEntryCreated = addOrUpdatePreformattedCrontabEntry(entryLine, name) == 0;
            free(entryLine);
        }

        if (cronTabEntryCreated)
        {

            if (registrations == NULL)
            {
                register_cronEvent_eventListener(eventHandler);
                registrations = hashMapCreate();
            }

            // Clear out any existing entry
            hashMapDelete(registrations, (void *) name, strlen(name), mapFreeFunc);
            // Place the entry
            hashMapPut(registrations, strdup(name), strlen(name), callback);
            retval = true;
        }
        else
        {
            retval = false;
        }

        pthread_mutex_unlock(&MTX);
    }

    return retval;
}

/**
 * Update the schedule for a cron event
 *
 * @param name the unique name
 * @param schedule the new cron schedule
 * @return true if updated, false if not found
 */
bool updateCronEventSchedule(const char *name, const char *schedule)
{
    bool retval = false;
    if (name != NULL && schedule != NULL)
    {
        pthread_mutex_lock(&MTX);

        // Check if its one we know about
        if (hashMapGet(registrations, (void *) name, strlen(name)) != NULL)
        {
            char *entryLine = buildEntryLine(name, schedule);

            // Do the update
            // coverity[sleep]
            if (addOrUpdatePreformattedCrontabEntry(entryLine, name) == 0)
            {
                retval = true;
            }

            // Cleanup
            free(entryLine);
        }

        pthread_mutex_unlock(&MTX);
    }
    return retval;
}

/**
 * Unregister to receive events for the given name
 *
 * @param name the name originally registered under
 * @param clearCronEntry if true the cron entry will be removed from the cron tab
 */
void unregisterForCronEvent(const char *name, bool clearCronEntry)
{
    pthread_mutex_lock(&MTX);

    // coverity[sleep]
    unregisterForCronEventInternal(name, clearCronEntry);

    pthread_mutex_unlock(&MTX);
}

/**
 * @see unregisterForCronEvent
 * Expects &MTX to already to be locked
 *
 * @param name the name originally registered under
 * @param clearCronEntry if true the cron entry will be removed from the cron tab
 */
static void unregisterForCronEventInternal(const char *name, bool clearCronEntry)
{
    if (registrations != NULL && name != NULL)
    {
        if (hashMapDelete(registrations, (void *)name, strlen(name), mapFreeFunc))
        {
            if (hashMapCount(registrations) == 0)
            {
                // Destroy and unregister our listener
                hashMapDestroy(registrations, mapFreeFunc);
                registrations = NULL;
                free(scheduleScript);
                scheduleScript = NULL;
                unregister_cronEvent_eventListener(eventHandler);
            }
        }

        // Even if we didn't know about it try to clean up the cron tab entry if requested

        if (clearCronEntry)
        {
            removeCrontabEntry(name);
        }
    }
}

/**
 * Callback when we receive a cron event, will lookup any handlers and call them
 * @param event the cron event
 */
static void eventHandler(cronEvent *event)
{
    pthread_mutex_lock(&MTX);

    if (registrations != NULL && event != NULL && event->name != NULL)
    {
        // Find the handler and invoke if its there
        cronEventHandler handler = (cronEventHandler)hashMapGet(registrations, event->name, strlen(event->name));
        if (handler != NULL)
        {
            // Call the registered handler and see if it returns true/false
            // (NOTE: we will release our lock in case this callback wants to re-register)
            //
            pthread_mutex_unlock(&MTX);
            bool unreg = handler(event->name);
            pthread_mutex_lock(&MTX);

            if (unreg == true)
            {
                // Returned true, so must be a onetime scheduled event, unregister and remove
                //
                // coverity[sleep]
                unregisterForCronEventInternal(event->name,true);
            }
        }
    }

    pthread_mutex_unlock(&MTX);
}

/**
 * Helper to clean up entries in our registrations map
 * @param key
 * @param value
 */
static void mapFreeFunc(void *key, void *value)
{
    // Free the key, which is a string
    free(key);
    // Nothing to do for this, just a function pointer
    (void)value;
}

/**
 * Build a cron entry line for the given schedule.  Assumes lock is held
 * @param name the name of the entry
 * @param schedule the schedule to build the entry for
 * @return a string with the entry line, caller must free
 */
static char *buildEntryLine(const char *name, const char *schedule)
{
    // Build the path to the script we are going to run
    if (scheduleScript == NULL)
    {
        char *staticPath = getStaticPath();
        int size = snprintf(scheduleScript, 0, "%s/bin/xhCronEventUtil", staticPath);
        scheduleScript = (char *)malloc(size+1);
        snprintf(scheduleScript, size+1, "%s/bin/xhCronEventUtil", staticPath);
        free(staticPath);
    }

    char *entryLine = NULL;
    int size = snprintf(entryLine, 0, "%s %s -b -n \"%s\"", schedule, scheduleScript, name);
    entryLine = (char *)malloc(size+1);
    snprintf(entryLine, size+1, "%s %s -b -n \"%s\"", schedule, scheduleScript, name);

    return entryLine;
}
