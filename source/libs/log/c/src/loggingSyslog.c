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
 * loggerSyslog.c
 *
 * Syslog implementation of the logging.h facade.
 * Will be built and included based on #define values
 *
 * Author: jelderton - 7/8/15
 *-----------------------------------------------*/

// look at buildtime settings to see if this log implementation should be enabled
#include <icBuildtime.h>

#ifdef CONFIG_LIB_LOG_SYSLOG

#include <syslog.h>
#include <stdarg.h>
#include <sys/syscall.h>

#include <icLog/logging.h>
#include <memory.h>
#include <stdio.h>
#include <stdbool.h>

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include "loggingCommon.h"

#define FALLBACKTAG "library"
#define BUFFER_SIZE 512
#define THREAD_NAME_BUFFER_SIZE 30
#define FORMAT_BUFFER_SIZE 10

//Setting the chunk size to 900 to be safe
//I tested some higher values, but there were still occasions of messages getting truncated
//Probably do to my inability to adquately account for all additional bytes
//added to the message through syslog itself and adjustFormat
//
#define LOG_CHUNK_SIZE 900

static char chunkBuffer[LOG_CHUNK_SIZE+1] = "";

static pthread_mutex_t	init_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool initialized = false;
static char *logName = NULL;



//static methods
static char* findNameByPid(int pid);
static char *getNameForThread();
static char *adjustFormat(const char *origFormat);
//end static methods

/*
 * initialize the logger
 */
void initIcLogger()
{
    pthread_mutex_lock(&init_mutex);
    if (!initialized)
    {
        logName = findNameByPid(getpid());
        initialized = true;
        openlog(logName, LOG_PID, LOG_USER);
    }
    pthread_mutex_unlock(&init_mutex);
}

static void checkInitStatus()
{
    pthread_mutex_lock(&init_mutex);
    if (!initialized)
    {
        icLogWarn("loggingSyslog","Syslog not initialized, initializing");
        logName = findNameByPid(getpid());
        initialized = true;
        openlog(logName, LOG_PID, LOG_USER);
    }
    pthread_mutex_unlock(&init_mutex);
}

/*
 * close the logger
 */
void closeIcLogger()
{
    pthread_mutex_lock(&init_mutex);
    closelog();
    initialized = false;
    free(logName);
    logName = NULL;
    pthread_mutex_unlock(&init_mutex);

}

/*
 * Find the process name from the PID
 * returns the process name, must be freed by the caller
 */
static char* findNameByPid(int pid)
{
    ssize_t rc;
    int commFd;
    char fname[BUFFER_SIZE];
    char *pidName;
    snprintf(fname,BUFFER_SIZE,"/proc/%d/cmdline",pid);
    commFd = open(fname,O_RDONLY);
    if (commFd < 0)  //Can't read the file, fall back to
    {
        pidName = calloc(1,BUFFER_SIZE);
        snprintf(pidName,BUFFER_SIZE,"%s",FALLBACKTAG);
    }
    else
    {
        char *pidNameTemp = calloc(1,BUFFER_SIZE);
        rc = read(commFd, pidNameTemp, BUFFER_SIZE);
        if (rc <= 0)
        {
            pidName = calloc(1,BUFFER_SIZE);
            snprintf(pidName,BUFFER_SIZE,"%s",FALLBACKTAG);
        }
        else
        {
            char *lastSlash = strrchr(pidNameTemp, '/');
            if (lastSlash != NULL)
            {
                lastSlash++;
                pidName = strdup(lastSlash);
            }
            else
            {
                pidName = strdup(pidNameTemp);
            }
        }
        close(commFd);
        free(pidNameTemp);
    }

    return pidName;
}

/*
 * Get the name of the thread
 * returns the thread name, must be freed by the caller
 */
static char *getNameForThread()
{
    long tid = syscall(SYS_gettid);
    pid_t pid = getpid();
    char *retval = calloc(1,THREAD_NAME_BUFFER_SIZE);
    if (tid == pid)
    {
        snprintf(retval,THREAD_NAME_BUFFER_SIZE,"main-%05d",pid);
    }
    else
    {
        snprintf(retval,THREAD_NAME_BUFFER_SIZE,"thread-%05d",(int)tid);
    }
    return retval;
}

/*
 * Adjust the format to add in additional information
 * Returns the adjusted format string(char*).  Must be freed by the caller
 */
static char *adjustFormat(const char *origFormat)
{
    char *threadName = getNameForThread();
    size_t reqSpace = strlen(origFormat)+strlen(threadName)+FORMAT_BUFFER_SIZE;
    char *retval = calloc(1,reqSpace);
    snprintf(retval,reqSpace," [tid=%s] %s",threadName,origFormat);
    free(threadName);
    return retval;
}

/*
 * Issue logging message based on a 'categoryName' and 'priority'
 */
void icLogMsg(const char *file, size_t filelen,
              const char *func, size_t funclen,
              long line,
              const char *categoryName, logPriority priority, const char *format, ...)
{
    checkInitStatus();  //check if its been initialized
    va_list  arglist;
    int      logPriority = LOG_DEBUG;

    // skip if priority is > logLevel
    //
    if (!shouldLogMessage(priority))
    {
        return;
    }

    // map priority to Log4c syntax
    //
    switch (priority)
    {
        case IC_LOG_TRACE:  // no TRACE, so use DEBUG
        case IC_LOG_DEBUG:
            logPriority = LOG_DEBUG;
            break;

        case IC_LOG_INFO:
            logPriority = LOG_INFO;
            break;

        case IC_LOG_WARN:
            logPriority = LOG_WARNING;
            break;

        case IC_LOG_ERROR:
            logPriority = LOG_ERR;
            break;

        default:
            break;
    }

    // TODO: deal with 'category name'

    // preprocess the variable args format, then forward to syslog
    //
    char *realFmt = adjustFormat(format);

    //Initialize the list of arguments after format, in this case its the "..." in the method
    //
    va_start(arglist, format);

    char *formattedMessage = NULL;

    //get the size of the log message
    //
    int formattedMessageLen = vsnprintf(formattedMessage, 0, realFmt, arglist);

    //invalidate the list of args
    //
    va_end(arglist);

    if (formattedMessageLen > LOG_CHUNK_SIZE)
    {

        //Initialize the list of arguments after format, in this case its the "..." in the function
        //Get the length of the formatted string without having adjusted the format
        //Assign the memory to the length +1 for the null byte
        //
        va_start(arglist, format);
        formattedMessageLen = vsnprintf(formattedMessage, 0, format, arglist);
        formattedMessage = calloc(1, formattedMessageLen + 1);  //add 1 for the null byte

        //invalidate the list of args after use
        //
        va_end(arglist);

        //Re-initialize the list of arguments after format, in this case its the "..." in the function
        //Print the formatted message into the buffer
        //
        va_start(arglist, format);
        vsnprintf(formattedMessage, formattedMessageLen + 1, format, arglist);

        //invalidate the list of args
        //
        va_end(arglist);

        //determine how many times we need to split it up
        //
        int numChunks = formattedMessageLen / LOG_CHUNK_SIZE;

        //if there is a remainder, then add one to account for it
        //
        if(formattedMessageLen % LOG_CHUNK_SIZE != 0)
        {
            numChunks++;
        }
        char *ptrLocation = formattedMessage;
        for(int chunk = 0 ; chunk < numChunks; chunk++)
        {
            memcpy(chunkBuffer,ptrLocation,LOG_CHUNK_SIZE);
            chunkBuffer[LOG_CHUNK_SIZE] = '\0';

            char *chunkedRealFmt = adjustFormat(chunkBuffer);
            syslog(logPriority, "%s", chunkedRealFmt);
            free(chunkedRealFmt);

            ptrLocation = ptrLocation + LOG_CHUNK_SIZE;  //Move down the message in LOG_CHUNK_SIZE increments
            memset(chunkBuffer,0,LOG_CHUNK_SIZE+1);  //reset the buffer
        }
        free(formattedMessage);
    }
    else
    {
        //reinitialize the argument list
        //Send the format string and the arg list to syslog
        //
        va_start(arglist, format);
        vsyslog(logPriority, realFmt, arglist);

        //invalidate the arg list
        //
        va_end(arglist);
    }

    free(realFmt);
}

#endif /* CONFIG_LIB_LOG_SYSLOG */
