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
 * shutdown.c
 *
 * Set of functions available for rebooting the device.
 *
 * Author: jelderton, gfaulkner - 6/19/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <inttypes.h>

#include <icBuildtime.h>            // check for various configuration support (i.e. reboot, ssh, etc)
#include <icReset/shutdown.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <icUtil/array.h>
#include <icTime/timeUtils.h>
#include <icConcurrent/delayedTask.h>
#include <icConcurrent/timedWait.h>
#include <watchdog/watchdogService_ipc.h>
#include <propsMgr/paths.h>
#include <icConcurrent/threadUtils.h>

#ifdef CONFIG_LIB_SHUTDOWN
#include <deviceService/deviceService_pojo.h>
#include <commMgr/commService_pojo.h>
#endif

#define LOG_TAG "shutdown"

/*
 * the file that stores the shutdown_reason and reboot status code value
 * should be combined with getDynamicConfigPath() (from paths.h)
 * (i.e.  getDynamicConfigPath() + /reboot_reason)
 */
#define REBOOT_REASON_FILE              "/reboot_reason"
#define REBOOT_REASON_XCONF_FILE        "/reboot_reason_xconf"
#define REBOOT_STATUS_CODE_FILE         "/reboot_status_code"
#define REBOOT_STATUS_CODE_XCONF_FILE   "/reboot_status_code_xconf"

/*
 * private variables
 */
static pthread_mutex_t REASON_MTX = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t REASON_COND = PTHREAD_COND_INITIALIZER;

/*
 * private function declarations
 */
static char *getShutdownReasonFilePath(bool forTelemetry);
static char *getShutdownStatusCodeFilePath(bool forTelemetry);
static shutdownReason readShutdownReasonCode(const char *inputFile);
static uint32_t readShutdownStatusCode(const char *inputFile);
static void writeToFile(const char *fileName, void *input, size_t smemb, size_t nmemb);

/*
 * if the CONFIG_LIB_SHUTDOWN option is set:
 *    store the 'reason' to our REBOOT_REASON_FILE, then perform
 *    a system reboot.  This is the preferred mechanism since it
 *    attempts to coordinate with running services so that we
 *    flush caches and preserve states prior to rebooting
 * otherwise:
 *    perform a soft restart of all services via watchdog
 */
void icShutdown(shutdownReason reason)
{
#ifdef CONFIG_LIB_SHUTDOWN

    // we very well may be called from something that is going to get
    // a SIGTERM outside of our control; we need to ignore that fact
    //
    signal(SIGTERM, SIG_IGN);

    // save the reason in our file
    //
  	recordShutdownReason(reason);

  	// ask watchdog to shutdown 'core' services.  at this time, we'll
  	// request shutdown of devices/zigbee.  killing 'all' is risky
  	// because we don't know which service is initiating the shutdown.
  	// NOTE: going to set "no timeout" because it can take a really long time
  	//       for zigbee as it could be in the middle of a sensor update
  	//
    printf("[shutdown] stopping core services\n");
    bool worked = false;
    watchdogService_request_STOP_SERVICE_timeout((char *)DEVICE_SERVICE_NAME, &worked, 0);
    watchdogService_request_STOP_SERVICE((char *)COMM_SERVICE_NAME, &worked);
    printf("[shutdown] done stopping core services; starting reboot...\n");

    // ensure our filesystem is good before the boot
    //
    sync();
    sync();

    // same function run via /system/bin/reboot
    //
    int rc = reboot(RB_AUTOBOOT);

    // only prints out if there was an error
    printf("[shutdown] done with reboot, rc=%d\n", rc);

#else   // CONFIG_LIB_SHUTDOWN

    // ask watchdog to bounce all services
    //
    printf("[shutdown] reboot not supported... restarting all services instead\n");
    shutdownOptions *options = create_shutdownOptions();
    options->exit = false;
    options->forReset = (reason == SHUTDOWN_RESET) ? true : false;
    watchdogService_request_RESTART_ALL_SERVICES(options);
    destroy_shutdownOptions(options);

#endif  // CONFIG_LIB_SHUTDOWN
}

/*
 * callback from 'scheduleDelayTask' call
 */
void delayedShutdownCallback(void *arg)
{
    // do the shutdown and release memory allocated
    // during the schedule setup.
    //
    shutdownReason *parm = (shutdownReason *)arg;
    icShutdown(*parm);
    free(parm);
}


/*
 * same as 'icShutdown', but performed after the 'delaySecs'
 * has elapsed.
 */
void icDelayedShutdown(shutdownReason reason, uint16_t delaySecs)
{
    // need to create memory to hold our 'reason'
    //
    shutdownReason *mem  = (shutdownReason *)malloc(sizeof(shutdownReason));
    *mem = reason;

    // created a delay task to call our icShutdown
    //
    scheduleDelayTask(delaySecs, DELAY_SECS, delayedShutdownCallback, mem);
}

/*
 * attempts to read the REBOOT_REASON_FILE and return the
 * contents as an enumeration value.
 *
 * NOTE: after reading this will *delete* the reason file.
 *       to simply check the file without deleting, use the
 *       'peekShutdownReasonCode() function
 *
 *       This will most likely return SHUTDOWN_IGNORE
 *       if the CONFIG_LIB_SHUTDOWN option is NOT SET
 *       or if the reboot reason needs to be ignored.
 *
 *       This will return SHUTDOWN_MISSING if the
 *       reboot reason file does not exist.
 */
shutdownReason getShutdownReasonCode(bool forTelemetry)
{
    // determine which file to use based on the flag (pronunced tel-E-mEtry)
    //
    char *filename = getShutdownReasonFilePath(forTelemetry);

    // get the reason from the file
    //
    shutdownReason retVal = readShutdownReasonCode((const char *)filename);

    // delete the file so it can't be used again
    //
    unlink(filename);
    free(filename);

    return retVal;
}

/*
 * attempts to read the REBOOT_REASON_FILE and return the
 * contents as an enumeration value.  unlike the
 * getShutdownReasonCode() function, this is NOT destructive
 *
 * NOTE: This will most likely return SHUTDOWN_IGNORE
 *       if the CONFIG_LIB_SHUTDOWN option is NOT SET
 *       or if the reboot reason needs to be ignored.
 *
 *       This will return SHUTDOWN_MISSING if the
 *       reboot reason file does not exist.
 */
shutdownReason peekShutdownReasonCode(bool forTelemetry)
{
    // determine which file to use based on the flag (pronunced tel-E-mEtry)
    //
    char *filename = getShutdownReasonFilePath(forTelemetry);

    // get the reason from the file
    //
    shutdownReason retVal = readShutdownReasonCode((const char *)filename);
    free(filename);

    return retVal;
}

/*
 * Attempts to read the shutdown status file, will use
 * the 'forTelemetry' flag to determine which file to look at.
 *
 * NOTE: after reading this will *delete* the status file.
 *       to simply check the file without deleting, use the
 *       'peekShutdownStatusCode() function
 *
 * NOTE: the shutdown status code from the file,
 *       will return 0 if file does not exist
 */
uint32_t getShutdownStatusCode(bool forTelemetry)
{
    // get the file to read
    //
    char *filename = getShutdownStatusCodeFilePath(forTelemetry);

    // read it
    //
    uint32_t retVal = readShutdownStatusCode((const char *)filename);

    // delete the file so it can't be used again
    //
    unlink(filename);
    free(filename);

    return retVal;
}

/*
 * Attempts to read the shutdown status file, will use
 * the 'forTelemetry' flag to determine which file to look at.
 * unlike getShutdownStatusCode(), this is not destructive.
 *
 * NOTE: the shutdown status code from the file,
 *       will return 0 if file does not exist
 */
uint32_t peekShutdownStatusCode(bool forTelemetry)
{
    // get the file to read
    //
    char *filename = getShutdownStatusCodeFilePath(forTelemetry);

    // read it (no delete)
    //
    uint32_t retVal = readShutdownStatusCode((const char *)filename);
    free(filename);

    return retVal;
}

/*
 * converts the supplied string to the shutdownReason enum code.
 * primarily used by CLI utilities when performing a shutdown
 */
shutdownReason getShutdownCodeForString(const char *reason)
{
    // loop through the static array looking for a match
    //
    shutdownReason retVal = SHUTDOWN_UNKNOWN;

    if (reason != NULL)
    {
        shutdownReason loop;
        for (loop = SHUTDOWN_IGNORE ; loop < SHUTDOWN_UNKNOWN ; loop++)
        {
            if (strcmp(reason, shutdownReasonNames[loop]) == 0)
            {
                retVal = loop;
                break;
            }
        }
    }

    return retVal;
}

/*
 * record a shutdown reason code as a thread
 */
static void *recordShutdownReasonThread(void *targs)
{
    icLogDebug(LOG_TAG, "%s started",__FUNCTION__);
    pthread_mutex_lock(&REASON_MTX);

    // init thread
    shutdownReason *reason = (shutdownReason *)targs;
    struct stat sb;

    // see if reason is valid
    //
    if (*reason < SHUTDOWN_MISSING && *reason >= SHUTDOWN_IGNORE)
    {
        // save the reason to the files that can be read when we come back up
        // but only do it if the reason file does not already exist
        //

        // check to see normal shutdown reason file exists
        //
        char *normalReasonFile = getShutdownReasonFilePath(false);
        if (stat(normalReasonFile, &sb) != 0)
        {
            icLogDebug(LOG_TAG, "%s: recording reason of %d to %s", __FUNCTION__, *reason, normalReasonFile);
            writeToFile(normalReasonFile, reason, sizeof(shutdownReason), 1);
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: NOT recording reason of %d; %s file is present", __FUNCTION__, *reason, normalReasonFile);
        }
        free(normalReasonFile);

        // check to see if xconf reason file exists
        //
        char *xconfReasonFile = getShutdownReasonFilePath(true);
        if (stat(xconfReasonFile, &sb) != 0)
        {
            icLogDebug(LOG_TAG, "%s: recording reason of %d to %s", __FUNCTION__, *reason, xconfReasonFile);
            writeToFile(xconfReasonFile, reason, sizeof(shutdownReason), 1);
        }
        else
        {
            icLogDebug(LOG_TAG, "%s: NOT recording reason of %d; %s file is present", __FUNCTION__, *reason, xconfReasonFile);
        }
        free(xconfReasonFile);
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: got an invalid reboot reason of %d NOT saving reboot reason", __FUNCTION__, *reason);
    }

    // cleanup
    free(targs);

    icLogDebug(LOG_TAG, "%s: telling calling thread we are done",__FUNCTION__);
    pthread_cond_broadcast(&REASON_COND);
    pthread_mutex_unlock(&REASON_MTX);
    icLogDebug(LOG_TAG, "%s: returning",__FUNCTION__);

    return NULL;
}

/*
 * record a shutdown reason code.
 * Called both internally and externally.
 */
void recordShutdownReason(shutdownReason reason)
{
    // we will attempt to record this to the file in a separate thread, so that
    // if the system is really hosed, we don't hang the shutdown for too long
    icLogDebug(LOG_TAG, "%s called, starting thread",__FUNCTION__);

    initTimedWaitCond(&REASON_COND);

    // grab lock
    pthread_mutex_lock(&REASON_MTX);

    // start thread, passing it the reason as an argument
    shutdownReason *arg = malloc(sizeof(shutdownReason));
    *arg = reason;
    createDetachedThread(recordShutdownReasonThread, arg, "shutdownReason");

    // set the conditional wait time
    incrementalCondTimedWait(&REASON_COND,&REASON_MTX,5);

    // release lock
    pthread_mutex_unlock(&REASON_MTX);

    icLogDebug(LOG_TAG, "%s: finished",__FUNCTION__);
}

/*
 * record a shutdown reason code on start up.
 * Will remove reboot reason file(s) if they exist.
 */
void recordShutdownReasonOnStartUp(shutdownReason reason)
{
    // get file paths
    //
    char *normalReasonFile = getShutdownReasonFilePath(false);
    char *xconfReasonFile = getShutdownReasonFilePath(true);

    // remove the normal reason file
    //
    unlink(normalReasonFile);

    // remove the xconf reason file
    //
    unlink(xconfReasonFile);

    // now write the reboot reason files
    //
    recordShutdownReason(reason);

    // cleanup
    free(normalReasonFile);
    free(xconfReasonFile);
}

/*
 * Record a Shutdown status code, will rewrite both
 * normal status code file and the telemetry (xconf) file.
 */
void recordShutdownStatusCode(uint32_t statusCode)
{
    char *normalStatusCodeFile = getShutdownStatusCodeFilePath(false);
    char *telemetryStatusCodeFile = getShutdownStatusCodeFilePath(true);

    // save the status code to files that can be read when system needs them
    //

    // for the normal status code file
    //
    icLogDebug(LOG_TAG, "%s: recording status code of 0x%08"PRIx32" to %s",
               __FUNCTION__, statusCode, normalStatusCodeFile);

    // write and cleanup
    writeToFile(normalStatusCodeFile, &statusCode, sizeof(uint32_t), 1);
    free(normalStatusCodeFile);

    // for the telemetry status code file
    //
    icLogDebug(LOG_TAG, "%s: recording status code of 0x%08"PRIx32" to %s",
               __FUNCTION__, statusCode, telemetryStatusCodeFile);

    // write and cleanup
    writeToFile(telemetryStatusCodeFile, &statusCode, sizeof(uint32_t), 1);
    free(telemetryStatusCodeFile);
}


/*
 * inner function to extract the contents of the supplied file
 */
static shutdownReason readShutdownReasonCode(const char *inputFile)
{
    shutdownReason retVal = SHUTDOWN_MISSING;
    if (stringIsEmpty(inputFile) == true)
    {
        return retVal;
    }

    // see if we can read the file
    //
    FILE *fin = fopen(inputFile, "r");
    if (fin != NULL)
    {
        // get the contents of the file
        //
        size_t len = fread(&retVal, sizeof(shutdownReason), 1, fin);
        int maxShutdownReason = ARRAY_LENGTH(shutdownReasonNames) - 1;

        // sanity check on what was read
        //
        if (len < 1 || retVal < SHUTDOWN_IGNORE || retVal > maxShutdownReason)
        {
            icLogWarn(LOG_TAG, "%s: invalid shutdown reason data; using UNKNOWN", __FUNCTION__);
            retVal = SHUTDOWN_UNKNOWN;
        }
        fclose(fin);
    }

    return retVal;
}

/*
 * inner function to extract the contents of the supplied file
 */
static uint32_t readShutdownStatusCode(const char *inputFile)
{
    uint32_t retVal = 0;
    if (stringIsEmpty(inputFile) == true)
    {
        return retVal;
    }

    FILE *fin = fopen(inputFile, "r");
    if (fin != NULL)
    {
        // read the file
        //
        size_t len = fread(&retVal, sizeof(uint32_t), 1, fin);

        // check that we read some amount
        //
        if (len < 1)
        {
            // make sure return val is 0
            retVal = 0;
            icLogWarn(LOG_TAG, "%s: invalid shutdown status code; using default 0x%08"PRIx32, __FUNCTION__, retVal);
        }

        fclose(fin);
    }

    return retVal;
}

/*
 * return the full path of the REBOOT_REASON_FILE
 * the caller must free the returned string
 */
static char *getShutdownReasonFilePath(bool forTelemetry)
{
    // get the directory name
    //
    char *dir = getDynamicConfigPath();

    // create the path
    //
    char *filePath = stringBuilder("%s%s", dir, forTelemetry ? REBOOT_REASON_XCONF_FILE : REBOOT_REASON_FILE);

    // cleanup and return
    free(dir);
    printf("[shutdown] checking for shutdown file in %s\n", filePath);
    return filePath;
}

/*
 * Helper function to create the filepath for
 * the shutdown status code file, will use
 * the 'forTelemetry' flag to determine
 * which file to look at.
 *
 * NOTE: caller must free object if not NULL
 */
static char *getShutdownStatusCodeFilePath(bool forTelemetry)
{
    // get the directory name
    //
    char *dir = getDynamicConfigPath();

    // create the path
    //
    char *filePath = stringBuilder("%s%s", dir,
            forTelemetry ? REBOOT_STATUS_CODE_XCONF_FILE : REBOOT_STATUS_CODE_FILE);

    // cleanup and return
    free(dir);
    return filePath;
}

/*
 * Helper function to write the input into filename
 *
 * @param fileName - the file to write to
 * @param input - the contents to go into the file
 * @param smemb - the size of each element
 * @param nmemb - the number of elements
 */
static void writeToFile(const char *fileName, void *input, size_t smemb, size_t nmemb)
{
    if (input == NULL || fileName == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments, not writing to file", __FUNCTION__);
        return;
    }

    FILE *fout;
    fout = fopen(fileName, "w+");
    if (fout)
    {
        size_t writeLen = fwrite(input, smemb, nmemb, fout);
        fflush(fout);
        fclose(fout);

        // print error if couldn't write everything
        //
        if (writeLen != nmemb)
        {
            icLogWarn(LOG_TAG, "%s: unable to write the amount needed for file %s",
                      __FUNCTION__, fileName);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: unable to to write to file %s, could not open the file",
                   __FUNCTION__, fileName);
    }
}
