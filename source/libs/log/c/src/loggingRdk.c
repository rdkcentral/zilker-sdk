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
 * loggerRdk.c
 *
 * RDK implementation of the logging.h facade.
 * Like the other implementations, built and included
 * based on #define values
 *
 * This version logs rdklogger, the standard logging mechanism in RDK-B
 *
 * Author: jelderton - 6/19/16
 *-----------------------------------------------*/

// look at buildtime settings to see if this log implementation should be enabled
#include <icBuildtime.h>

#ifdef CONFIG_LIB_LOG_RDKLOG

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>

#include <rdk_debug.h>
#include <icLog/logging.h>

#include "loggingCommon.h"


#define MODULE_NAME "LOG.RDK.TOUCHSTONE"

static bool didInit = false;

#define BUFFER_SIZE 16*1024
static char buffer[BUFFER_SIZE];

/*
 * initialize the logger
 */
void initIcLogger()
{
    if (didInit == false)
    {
        // init RDK
        rdk_logger_init("/etc/debug.ini");

        // set local vars
        didInit = true;
    }
}

/*
 * close the logger
 */
void closeIcLogger()
{
    if (didInit == true)
    {
        rdk_logger_deinit();
        didInit = false;
    }
}
/*
 * Issue logging message based on a 'categoryName' and 'priority'
 */
void icLogMsg(const char *file, size_t filelen,
              const char *func, size_t funclen,
              long line,
              const char *categoryName, logPriority priority, const char *format, ...)
{
    va_list     arglist;

    // skip if priority is > logLevel or never initialized
    //
    if (!shouldLogMessage(priority) || didInit == false)
    {
        return;
    }

    // map priority to RDK
    //
    rdk_LogLevel rdkLevel = RDK_LOG_INFO;
    switch (priority)
    {
        case IC_LOG_TRACE:  // no TRACE, so use DEBUG
        case IC_LOG_DEBUG:
            rdkLevel = RDK_LOG_DEBUG;
            break;

        case IC_LOG_INFO:
            rdkLevel = RDK_LOG_INFO;
            break;

        case IC_LOG_WARN:
            rdkLevel = RDK_LOG_WARN;
            break;

        case IC_LOG_ERROR:
            rdkLevel = RDK_LOG_ERROR;
            break;

        default:
            break;
    }

    // preprocess the variable args format, then forward to RDK as a single string
    //
    int prefixLen = sprintf(buffer, "[%s] ", categoryName);
    va_start(arglist, format);
    vsnprintf(buffer + prefixLen, BUFFER_SIZE-1, format, arglist); //guaranteed to be null terminated, but save an extra char for \n
    int len = strlen(buffer);
    buffer[len++] = '\n';
    buffer[len++] = '\0';
    va_end(arglist);

    rdk_dbg_MsgRaw(rdkLevel, MODULE_NAME, buffer);
}

#endif /* CONFIG_LIB_LOG_RDKLOG */
