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
 * logLevel.c.c
 *
 * Helper to simplify the tracking and adjustment of granual
 * log levels within the various components of the system.
 *
 * Author: jelderton - 8/31/15
 *-----------------------------------------------*/

#include <icBuildtime.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <propsMgr/logLevel.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/propsService_event.h>
#include <propsMgr/propsService_eventAdapter.h>
#include <propsMgr/propsService_pojo.h>

#define LOG_KEY_PREFIX  "logging."
#define LOG_TRACE_STR   "trace"
#define LOG_DEBUG_STR   "debug"
#define LOG_INFO_STR    "info"
#define LOG_WARN_STR    "warn"
#define LOG_ERROR_STR   "error"
#define LOG_NONE_STR    "none"

/*
 * private functions
 */
static char *calculateLogKey(const char *name);
static logPriority logPriorityForString(char *value, logPriority defVal);
static char *stringForlogPriority(logPriority value);
static void logPropertyValueChangedEventListener(cpePropertyEvent *event);

/*
 * private variables
 */
static char *myLogKey = NULL;

/*
 * Uses 'name' to construct a property pattern and
 * ask the propsService for the value, which can be used
 * for setting the log level.  For example:
 *   getCustomLogLevel("commService") would look for the
 *   property "logging.commService"
 *
 * returns 'defVal' if the property is not set
 */
logPriority getCustomLogLevel(const char *name, logPriority defVal)
{
    logPriority retVal = defVal;

    // calculate the key, then see if we have that property set
    //
    char *key = calculateLogKey(name);

    // create the output, but zero it first in case this property is not defined
    //
    char *levelStr = getPropertyAsString(key, NULL);
    if (levelStr != NULL)
    {
        // convert from 'string' to 'logPriority'
        //
        retVal = logPriorityForString(levelStr, defVal);
        free (levelStr);
    }
    else
    {
        // log level not set, so assign it now
        //
        char *val = stringForlogPriority(retVal);
        setPropertyValue(key, val, true, PROPERTY_SRC_DEFAULT);
        free(val);
    }

    // cleanup
    //
    free(key);
    return retVal;
}

/*
 * Uses 'name' to construct a property pattern and
 * tell the propsService to set that key to the
 * supplied log level value
 */
void setCustomLogLevel(const char *name, logPriority newLevel)
{
    // calculate the key, then see if we have that property set
    //
    char *key = calculateLogKey(name);

    // get the string varsion of 'newLevel'
    //
    char *val = stringForlogPriority(newLevel);

    // do the save
    //
    setPropertyValue(key, val, true, PROPERTY_SRC_DEFAULT);

    // cleanup
    //
    free(key);
    free(val);
}

/*
 * Setup routine to get and set the log level for the given 'name'.
 * This also sets up an eventListener to perform a "setLogPriority"
 * when a "property changed event" occurs with the corresponding key.
 */
void autoAdjustCustomLogLevel(const char *name)
{
    // save the key we'll be monitoring
    //
    if (myLogKey != NULL)
    {
        // clear previous value
        //
        free(myLogKey);
    }
    else
    {
        // register for property changed event notifications
        //
        register_cpePropertyEvent_eventListener(logPropertyValueChangedEventListener);
    }
    myLogKey = calculateLogKey(name);

    // get the current property value, and apply to the log filter
    //
    logPriority level = getCustomLogLevel(name, IC_LOG_DEBUG);
    if (level != getIcLogPriorityFilter())
    {
        icLogDebug("log", "adjusting log level to %d", level);
        setIcLogPriorityFilter(level);
    }
}

/*
 *
 */
static char *calculateLogKey(const char *name)
{
    uint16_t len = (uint16_t)(strlen(name) + strlen(LOG_KEY_PREFIX) + 2);

    char *retVal = (char *)malloc(sizeof(char) * len);
    sprintf(retVal, "%s%s", LOG_KEY_PREFIX, name);

    return retVal;
}

/*
 *
 */
static logPriority logPriorityForString(char *value, logPriority defVal)
{
    if (value == NULL)
    {
        return defVal;
    }
    else if (strcasecmp(value, LOG_TRACE_STR) == 0)
    {
        return IC_LOG_TRACE;
    }
    else if (strcasecmp(value, LOG_DEBUG_STR) == 0)
    {
        return IC_LOG_DEBUG;
    }
    else if (strcasecmp(value, LOG_INFO_STR) == 0)
    {
        return IC_LOG_INFO;
    }
    else if (strcasecmp(value, LOG_WARN_STR) == 0)
    {
        return IC_LOG_WARN;
    }
    else if (strcasecmp(value, LOG_ERROR_STR) == 0)
    {
        return IC_LOG_ERROR;
    }
    else if (strcasecmp(value, LOG_NONE_STR) == 0)
    {
        return IC_LOG_NONE;
    }

    return defVal;
}

/*
 *
 */
static char *stringForlogPriority(logPriority value)
{
    switch (value)
    {
        case IC_LOG_TRACE:
            return strdup(LOG_TRACE_STR);

        case IC_LOG_DEBUG:
            return strdup(LOG_DEBUG_STR);

        case IC_LOG_INFO:
            return strdup(LOG_INFO_STR);

        case IC_LOG_WARN:
            return strdup(LOG_WARN_STR);

        case IC_LOG_ERROR:
            return strdup(LOG_ERROR_STR);

        case IC_LOG_NONE:
        default:
            return strdup(LOG_NONE_STR);
    }
}

/*
 * called when property change events occur
 */
static void logPropertyValueChangedEventListener(cpePropertyEvent *event)
{
    // first see if this event is remotely close
    //
    if (myLogKey != NULL && event != NULL && strcmp(event->propKey, myLogKey) == 0)
    {
        // property for our log has changed.  convert to logPriority then apply
        //
        logPriority level = logPriorityForString(event->propValue, IC_LOG_DEBUG);
        if (level != getIcLogPriorityFilter())
        {
            icLogDebug("log", "adjusting log level to %d", level);
            setIcLogPriorityFilter(level);
        }
    }
}


