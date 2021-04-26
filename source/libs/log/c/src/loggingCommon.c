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

// Created by dcalde202 on 2/12/19.
//

#include <stdbool.h>
#include <pthread.h>


#include <icLog/logging.h>

static pthread_mutex_t LOG_MTX = PTHREAD_MUTEX_INITIALIZER;
static logPriority logLevel = IC_LOG_DEBUG;


/*
 * return the system-level logging priority setting, which dictates
 * what is actually sent to the log and what is ignored
 */
logPriority getIcLogPriorityFilter()
{
    logPriority retVal = IC_LOG_DEBUG;

    pthread_mutex_lock(&LOG_MTX);
    retVal = logLevel;
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * set the system-level logging priority setting, allowing messages
 * to be sent or filtered out
 */
void setIcLogPriorityFilter(logPriority priority)
{
    pthread_mutex_lock(&LOG_MTX);
    logLevel = priority;
    pthread_mutex_unlock(&LOG_MTX);
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_TRACE' messages
 */
bool isIcLogPriorityTrace()
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel == IC_LOG_TRACE)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_DEBUG' messages
 */
bool isIcLogPriorityDebug()
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel <= IC_LOG_DEBUG)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_INFO' messages
 */
bool isIcLogPriorityInfo()
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel <= IC_LOG_INFO)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_WARN' messages
 */
bool isIcLogPriorityWarn()
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel <= IC_LOG_WARN)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_ERROR' messages
 */
bool isIcLogPriorityError()
{
    bool retVal = false;

    pthread_mutex_lock(&LOG_MTX);
    if (logLevel <= IC_LOG_ERROR)
    {
        retVal = true;
    }
    pthread_mutex_unlock(&LOG_MTX);

    return retVal;
}

bool shouldLogMessage(logPriority priority)
{
    pthread_mutex_lock(&LOG_MTX);
    bool retval = true;

    if (logLevel > priority)
    {
        retval = false;
    }

    pthread_mutex_unlock(&LOG_MTX);
    return retval;
}