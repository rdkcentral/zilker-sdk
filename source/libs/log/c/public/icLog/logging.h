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
 * logger.h
 *
 * Facade of a common interface for logging.  Allows us
 * to supply different runtime implementations based
 * on the environment (ex: android, log4c, etc)
 * During runtime, the appropriate underlying logging
 * mechanism will be available and loaded.
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_LOGGING_H
#define IC_LOGGING_H

#include <stdlib.h>
#include <stdbool.h>

/*
 * define the log priorities
 */
typedef enum _logPriority
{
    IC_LOG_TRACE = 0,
    IC_LOG_DEBUG = 1,
    IC_LOG_INFO  = 2,
    IC_LOG_WARN  = 3,
    IC_LOG_ERROR = 4,
    IC_LOG_NONE  = 5 //disables all log output
} logPriority;


/*
 * initialize the logger
 */
void initIcLogger();

/*
 * close the logger
 */
void closeIcLogger();


/*
 * return the system-level logging priority setting, which dictates
 * what is actually sent to the log and what is ignored
 */
logPriority getIcLogPriorityFilter();

/*
 * set the system-level logging priority setting, allowing messages
 * to be sent or filtered out
 */
void setIcLogPriorityFilter(logPriority priority);

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_TRACE' messages
 */
bool isIcLogPriorityTrace();

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_DEBUG' messages
 */
bool isIcLogPriorityDebug();

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_INFO' messages
 */
bool isIcLogPriorityInfo();

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_WARN' messages
 */
bool isIcLogPriorityWarn();

/*
 * returns 0 if the system-level logging priority is
 * set to allow 'IC_LOG_ERROR' messages
 */
bool isIcLogPriorityError();


/*
 * Issue logging message based on a 'categoryName' and 'priority'
 */
extern void icLogMsg(const char *file, size_t filelen,
                     const char *func, size_t funclen,
                     long line,
                     const char *categoryName, logPriority priority, const char *format, ...);

/*
 * Issue trace log message for 'categoryName'
 */
#define icLogTrace(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_TRACE, __VA_ARGS__)

/*
 * Issue debug log message for 'categoryName'
 */
#define icLogDebug(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_DEBUG, __VA_ARGS__)

/*
 * Issue informative log message for 'categoryName'
 */
#define icLogInfo(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_INFO, __VA_ARGS__)

/*
 * Issue warning log message for 'categoryName'
 */
#define icLogWarn(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_WARN, __VA_ARGS__)

/*
 * Issue error log message for 'categoryName'
 */
#define icLogError(cat, ...) \
    icLogMsg(__FILE__, sizeof(__FILE__)-1, __func__, sizeof(__func__)-1, __LINE__, \
    cat, IC_LOG_ERROR, __VA_ARGS__)

#endif // IC_LOGGING_H

