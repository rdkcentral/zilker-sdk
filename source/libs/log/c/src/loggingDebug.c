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
 * loggerDebug.c
 *
 * Development implementation of the logging.h facade
 * to print all logging messages to STDOUT.  Like the
 * other implementations, built and included based on
 * #define values
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

// look at buildtime settings to see if this log implementation should be enabled
#include <icBuildtime.h>

#ifdef CONFIG_LIB_LOG_STDOUT

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <icLog/logging.h>
#include <pthread.h>
#include <sys/time.h>

#include "loggingCommon.h"


static void printCurrentTime();

/*
 * initialize the logger
 */
void initIcLogger()
{
    //uses STDOUT, so nothing to open
}

/*
 * close the logger
 */
void closeIcLogger()
{
    // this log uses STDOUT, so nothing to close
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

    // skip if priority is > logLevel
    //
    if(!shouldLogMessage(priority))
    {
        return;
    }

    // print the cat name, priority, then the message
    // NOTE: trying to keep format the same as zlog.conf
    //
    printCurrentTime();
    printf("[%s %d] - ", categoryName, getpid());

    // map priority to Android syntax
    //
    switch (priority)
    {
        case IC_LOG_TRACE:
            printf("TRACE: ");
            break;

        case IC_LOG_DEBUG:
            printf("DEBUG: ");
            break;

        case IC_LOG_INFO:
            printf("INFO: ");
            break;

        case IC_LOG_WARN:
            printf("WARN: ");
            break;

        case IC_LOG_ERROR:
            printf("ERROR: ");
            break;

        default:
            break;
    }

    // preprocess the variable args format, then forward to STDOUT
    //
    va_start(arglist, format);
    vprintf(format, arglist);
    printf("\n");
    va_end(arglist);

    if (fflush(stdout) == EOF)
    {
        // Output is gone, prevent an infinite cycle when logging any SIGPIPEs
        setIcLogPriorityFilter(IC_LOG_NONE);
    }
}

static void printCurrentTime()
{
    char buff[32];
    struct timeval now;
    struct tm ptr;

    // get local time & millis
    //
    gettimeofday(&now, NULL);
    localtime_r(&now.tv_sec, &ptr);

    // pretty-print the time
    //
    strftime(buff, 31, "%Y-%m-%d %H:%M:%S", &ptr);

    // add millis to the end of the formatted string
    //
    printf("%s.%03ld : ", buff, (long)(now.tv_usec / 1000));
}

#endif /* CONFIG_LIB_LOG_STDOUT */
