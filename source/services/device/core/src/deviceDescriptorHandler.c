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

/*
 * Created by Thomas Lea on 10/23/15.
 *
 * This code is responsible for ensuring that we have successfully downloaded the latest white and blacklists.
 * It will spawn a repeating task to download each if required.  These tasks will continue to run until we have
 * success.  The interval between each download attempt will increase until we reach our maximum interval.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <unistd.h>
#include <pthread.h>

#include <icLog/logging.h>
#include <propsMgr/propsHelper.h>
#include <propsMgr/commonProperties.h>
#include <deviceService.h>
#include <deviceDescriptors.h>
#include <icUtil/md5.h>
#include <icUtil/stringUtils.h>
#include <urlHelper/urlHelper.h>
#include "deviceDescriptorHandler.h"
#include <icConcurrent/repeatingTask.h>
#include <icUtil/fileUtils.h>

#define LOG_TAG "deviceDescriptorHandler"
#define CURRENT_DEVICE_DESCRIPTOR_URL "currentDeviceDescriptorUrl"
#define CURRENT_DEVICE_DESCRIPTOR_MD5 "currentDeviceDescriptorMd5"
#define CURRENT_BLACKLIST_URL         "currentBlacklistUrl"
#define CURRENT_BLACKLIST_MD5         "currentBlacklistMd5"
#define DEFAULT_INVALID_BLACKLIST_URL "http://toBeReplaced"

// a basic URL length check.  Shortest I could fathom is "file:///a"
#define MIN_URL_LENGTH 9

#define DOWNLOAD_TIMEOUT_SECS 60

#define INIT_DD_TASK_WAIT_TIME_SECONDS  15
#define INTERVAL_DD_TASK_TIME_SECONDS   15
#define MAX_DD_TASK_WAIT_TIME_SECONDS   120

static deviceDescriptorsReadyForDevicesFunc readyForDevicesCB;
static pthread_mutex_t deviceDescriptorMutex = PTHREAD_MUTEX_INITIALIZER;

static uint32_t whitelistTaskId = 0;
static char *currentWhitelistUrl;
static bool updateWhitelistTaskFunc(void *taskArg);
static void cancelWhitelistUpdate();

static uint32_t blacklistTaskId = 0;
static char *currentBlacklistUrl;
static bool updateBlacklistTaskFunc(void *taskArg);
static void cancelBlacklistUpdate();

static bool fileNeedsUpdating(const char *currentUrlSystemKey,
                              const char *newUrl,
                              const char *currentFilePath,
                              const char *currentFileMd5SystemKey);
typedef bool (*deviceDescriptorFileValidator)(const char *path);
static bool downloadFile(const char *url, const char *destFile, deviceDescriptorFileValidator fileValidator);
static char* getFileMd5(const char* path);

static deviceDescriptorsReadyForDevicesFunc readyForDevicesCB;
static deviceDescriptorsUpdatedFunc descriptorsUpdatedCB;

/*
 * Standard initialization performed at system startup.  This will retrieve the last known white and blacklist
 * URLs from prop service and ensure that we have them downloaded successfully.
 */
void deviceServiceDeviceDescriptorsInit(deviceDescriptorsReadyForDevicesFunc readyForDevicesCallback,
                                        deviceDescriptorsUpdatedFunc descriptorsUpdatedCallback)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&deviceDescriptorMutex);

    readyForDevicesCB = readyForDevicesCallback;
    descriptorsUpdatedCB = descriptorsUpdatedCallback;

    pthread_mutex_unlock(&deviceDescriptorMutex);

    char *whitelistUrl = NULL;

    if (hasProperty(DEVICE_DESC_WHITELIST_URL_OVERRIDE))
    {
        whitelistUrl = getPropertyAsString(DEVICE_DESC_WHITELIST_URL_OVERRIDE, NULL);
    }
    else if (hasProperty(DEVICE_DESCRIPTOR_LIST))
    {
        whitelistUrl = getPropertyAsString(DEVICE_DESCRIPTOR_LIST, NULL);
    }

    if (whitelistUrl != NULL)
    {
        deviceDescriptorsUpdateWhitelist(whitelistUrl);
        free(whitelistUrl);
    }

    //blacklist is optional.  If the blacklist url property gets deleted, we still need to process a NULL value
    char *blacklistUrl = getPropertyAsString(DEVICE_DESC_BLACKLIST, NULL);
    deviceDescriptorsUpdateBlacklist(blacklistUrl);
    free(blacklistUrl);
}

void deviceServiceDeviceDescriptorsDestroy()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    //Stop any scheduled updates
    cancelWhitelistUpdate();
    cancelBlacklistUpdate();

    pthread_mutex_lock(&deviceDescriptorMutex);

    readyForDevicesCB = NULL;
    descriptorsUpdatedCB = NULL;

    pthread_mutex_unlock(&deviceDescriptorMutex);
}

void deviceDescriptorsUpdateWhitelist(const char *url)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, url);

    // cancel any task that is already running
    cancelWhitelistUpdate();

    pthread_mutex_lock(&deviceDescriptorMutex);

    // Only start the task if we have a whitelist url
    if(url != NULL && strlen(url) >= MIN_URL_LENGTH)
    {
        free(currentWhitelistUrl);
        currentWhitelistUrl = strdup(url);
        // Kick it off in the background, with increasing backoff until it eventually completes
        whitelistTaskId = createBackOffRepeatingTask(INIT_DD_TASK_WAIT_TIME_SECONDS,
                                                     MAX_DD_TASK_WAIT_TIME_SECONDS,
                                                     INTERVAL_DD_TASK_TIME_SECONDS,
                                                     DELAY_SECS,
                                                     updateWhitelistTaskFunc,
                                                     NULL,
                                                     currentWhitelistUrl);
    }

    pthread_mutex_unlock(&deviceDescriptorMutex);
}

static void cancelWhitelistUpdate()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // Avoid holding the lock while we cancel as that can result in deadlock
    pthread_mutex_lock(&deviceDescriptorMutex);
    uint32_t localWhitelistTaskId = whitelistTaskId;
    if (localWhitelistTaskId != 0)
    {
        // Cancel the URL request first, or this thread may block
        // for many seconds waiting for a running download to finish.
        urlHelperCancel(currentWhitelistUrl);
    }
    pthread_mutex_unlock(&deviceDescriptorMutex);

    // see if there is already a task running
    //
    if (localWhitelistTaskId != 0)
    {
        // since there is one running go ahead and kill it
        //
        cancelRepeatingTask(localWhitelistTaskId);

        pthread_mutex_lock(&deviceDescriptorMutex);
        free(currentWhitelistUrl);
        currentWhitelistUrl = NULL;
        whitelistTaskId = 0;
        pthread_mutex_unlock(&deviceDescriptorMutex);
    }
}

static bool updateWhitelistTaskFunc(void *taskArg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AUTO_CLEAN(free_generic__auto) char *url = strdup(taskArg);
    bool fileupdated = false;
    bool readyForDevices = false;
    AUTO_CLEAN(free_generic__auto) char *whitelistPath = getWhiteListPath();

    if (whitelistPath == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to fetch whitelist, no local file path configured!", __FUNCTION__);

        return false; // this will cause us to try again.  Perhaps it wasnt set yet or props service not ready
    }

    // is an update even needed?
    if (fileNeedsUpdating(CURRENT_DEVICE_DESCRIPTOR_URL,
                          url,
                          whitelistPath,
                          CURRENT_DEVICE_DESCRIPTOR_MD5) == true)
    {
        if (downloadFile(url, whitelistPath, checkWhiteListValid) == false)
        {
            // log line used for Telemetry do not edit/delete
            //
            icLogError(LOG_TAG, "%s: failed to download whitelist!", __FUNCTION__);
        }
        else
        {
            readyForDevices = true;
            fileupdated = true;

            //update our system properties
            deviceServiceSetSystemProperty(CURRENT_DEVICE_DESCRIPTOR_URL, url);
            char *md5 = getFileMd5(whitelistPath);
            deviceServiceSetSystemProperty(CURRENT_DEVICE_DESCRIPTOR_MD5, md5);
            free(md5);
        }
    }
    else
    {
        // no need to download anything, we are up to date.
        readyForDevices = true;
    }

    if (readyForDevices)
    {
        if (readyForDevicesCB != NULL)
        {
            readyForDevicesCB();
        }

        // since we are done, we can clear out our task id here
        pthread_mutex_lock(&deviceDescriptorMutex);
        whitelistTaskId = 0;
        free(currentWhitelistUrl);
        currentWhitelistUrl = NULL;
        pthread_mutex_unlock(&deviceDescriptorMutex);
    }

    if (fileupdated && descriptorsUpdatedCB != NULL)
    {
        descriptorsUpdatedCB();
    }

    icLogDebug(LOG_TAG, "%s completed", __FUNCTION__);

    return readyForDevices;
}

void deviceDescriptorsUpdateBlacklist(const char *url)
{
    icLogDebug(LOG_TAG, "%s: %s", __FUNCTION__, url == NULL ? "(null)" : url);

    // cancel any task that is already running
    cancelBlacklistUpdate();

    pthread_mutex_lock(&deviceDescriptorMutex);

    // Only start the task if we have a valid blacklist url
    if(url != NULL && strlen(url) >= MIN_URL_LENGTH && strcasecmp(url, DEFAULT_INVALID_BLACKLIST_URL) != 0)
    {
        free(currentBlacklistUrl);
        currentBlacklistUrl = strdup(url);
        // Kick it off in the background, with increasing backoff until it eventually completes
        blacklistTaskId = createBackOffRepeatingTask(INIT_DD_TASK_WAIT_TIME_SECONDS,
                                                     MAX_DD_TASK_WAIT_TIME_SECONDS,
                                                     INTERVAL_DD_TASK_TIME_SECONDS,
                                                     DELAY_SECS,
                                                     updateBlacklistTaskFunc,
                                                     NULL,
                                                     currentBlacklistUrl);
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *blacklistPath = getBlackListPath();
        if (blacklistPath != NULL)
        {
            unlink(blacklistPath);

            //clear out our related properties
            deviceServiceSetSystemProperty(CURRENT_BLACKLIST_URL, "");
            deviceServiceSetSystemProperty(CURRENT_BLACKLIST_MD5, "");
        }
    }

    pthread_mutex_unlock(&deviceDescriptorMutex);
}

static void cancelBlacklistUpdate()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // Avoid holding the lock while we cancel as that can result in deadlock
    pthread_mutex_lock(&deviceDescriptorMutex);
    uint32_t localBlacklistTaskId = blacklistTaskId;
    if (localBlacklistTaskId != 0)
    {
        // Cancel the URL request first, or this thread may block
        // for many seconds waiting for a running download to finish.
        urlHelperCancel(currentBlacklistUrl);
    }
    pthread_mutex_unlock(&deviceDescriptorMutex);

    // see if there is already a task running
    //
    if (localBlacklistTaskId != 0)
    {
        // since there is one running go ahead and kill it
        //
        cancelRepeatingTask(localBlacklistTaskId);

        pthread_mutex_lock(&deviceDescriptorMutex);
        free(currentBlacklistUrl);
        currentBlacklistUrl = NULL;
        blacklistTaskId = 0;
        pthread_mutex_unlock(&deviceDescriptorMutex);
    }
}

static bool updateBlacklistTaskFunc(void *taskArg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AUTO_CLEAN(free_generic__auto) char *url = strdup(taskArg);
    bool result = false;
    bool filesupdated = false;
    AUTO_CLEAN(free_generic__auto) char *blacklistPath = getBlackListPath();

    if (blacklistPath == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to fetch blacklist, no local file path configured!", __FUNCTION__);

        return false; // this will cause us to try again.  Perhaps it wasnt set yet or props service not ready
    }

    if (fileNeedsUpdating(CURRENT_BLACKLIST_URL,
                          url,
                          blacklistPath,
                          CURRENT_BLACKLIST_MD5) == true)
    {
        if (downloadFile(url, blacklistPath, checkBlackListValid) == false)
        {
            // log line used for Telemetry do not edit/delete
            //
            icLogError(LOG_TAG, "%s: failed to download blacklist!", __FUNCTION__);
        }
        else
        {
            filesupdated = true;
            result = true;

            //update our system properties
            deviceServiceSetSystemProperty(CURRENT_BLACKLIST_URL, url);
            char *md5 = getFileMd5(blacklistPath);
            deviceServiceSetSystemProperty(CURRENT_BLACKLIST_MD5, md5);
            free(md5);
        }
    }
    else
    {
        result = true;
    }

    if (filesupdated && descriptorsUpdatedCB != NULL)
    {
        descriptorsUpdatedCB();
    }

    if(result == true)
    {
        // since we are done, we can clear out our task id here
        pthread_mutex_lock(&deviceDescriptorMutex);
        blacklistTaskId = 0;
        free(currentBlacklistUrl);
        currentBlacklistUrl = NULL;
        pthread_mutex_unlock(&deviceDescriptorMutex);
    }

    return result;
}

static bool downloadFile(const char *url, const char *destFile, deviceDescriptorFileValidator fileValidator)
{
    icLogDebug(LOG_TAG, "%s: url=%s, destFile=%s", __FUNCTION__, url, destFile);

    bool result = false;
    long httpCode = -1;
    size_t fileSize = 0;

    // Write to a temp file so we don't end up with a bogus whitelist in case of any failure
    AUTO_CLEAN(free_generic__auto) char *tmpfilename = stringBuilder("%s.tmp", destFile);

    fileSize = urlHelperDownloadFile(url,
                                     &httpCode,
                                     NULL,
                                     NULL,
                                     DOWNLOAD_TIMEOUT_SECS,
                                     getSslVerifyProperty(SSL_VERIFY_HTTP_FOR_SERVER),
                                     true,
                                     tmpfilename);

    if (fileSize > 0 && (httpCode == 200 || httpCode == 0))
    {
        if (fileValidator == NULL || fileValidator(tmpfilename) == true)
        {
            // Move into place
            if(rename(tmpfilename, destFile))
            {
                icLogError(LOG_TAG, "%s: failed to move %s to %s", __FUNCTION__, tmpfilename, destFile);
            }
            else
            {
                result = true;
                icLogInfo(LOG_TAG, "%s: %s downloaded to %s", __FUNCTION__, url, destFile);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: downloaded file failed to validate", __FUNCTION__);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to download %s.  httpCode=%ld, fileSize=%ld", __FUNCTION__,
           url, httpCode, fileSize);
    }

    if (result == false)
    {
        if(remove(tmpfilename))
        {
            icLogError(LOG_TAG, "%s: failed to remove %s", __FUNCTION__, tmpfilename);
        }
    }

    return result;
}

/*
 * See if the provided URLs are different or if the provided file does not match the provided md5sum.  Any of this
 * would indicate a difference that should trigger a download.
 *
 * @return true if we need to download.
 */
static bool fileNeedsUpdating(const char *currentUrlSystemKey,
                              const char *newUrl,
                              const char *currentFilePath,
                              const char *currentFileMd5SystemKey)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;

    // Fast exit:
    // If either the URL or MD5 is missing from our system properties (or if we fail to fetch them), we need to download
    char *currentUrl = NULL;
    if (deviceServiceGetSystemProperty(currentUrlSystemKey, &currentUrl) == false || currentUrl == NULL)
    {
        icLogWarn(LOG_TAG, "%s: unable to get %s system prop -- triggering download", __FUNCTION__, currentUrlSystemKey);
        return true;
    }

    char *currentMd5 = NULL;
    if (deviceServiceGetSystemProperty(currentFileMd5SystemKey, &currentMd5) == false || currentMd5 == NULL)
    {
        icLogWarn(LOG_TAG, "%s: unable to get %s system prop -- triggering download", __FUNCTION__,
                  currentFileMd5SystemKey);
        free(currentUrl);
        return true;
    }

    // see if our target file (local) exists
    //
    if (doesFileExist(currentFilePath) == false)
    {
        // missing local file
        //
        icLogWarn(LOG_TAG,
                  "%s: stat failed with error \"%s\", attempting download.",
                  __FUNCTION__, strerror(errno));
    }
    else
    {
        // see if the URL changed
        if (strcmp(currentUrl, newUrl) == 0)
        {
            // URL is the same, so compare the MD5
            AUTO_CLEAN(free_generic__auto) char *localMd5 = getFileMd5(currentFilePath);

            // compare to what we have in our database (to see if the local file was altered, replaced, etc)
            if (stringCompare(currentMd5, localMd5, true) != 0)
            {
                // MD5 mis-match
                icLogWarn(LOG_TAG,
                          "%s: md5 mismatch between dbase and local file, attempting download.", __FUNCTION__);
            }
            else
            {
                // URL and MD5 match
                icLogDebug(LOG_TAG,
                           "%s: URL (%s) and MD5 sums match (%s), no need to download",
                           __FUNCTION__,
                           newUrl,
                           currentFilePath);

                result = false;
            }
        }
        else
        {
            // URL mismatch
            icLogDebug(LOG_TAG,
                       "%s: currentUrl = %s, new url = %s -- need to update",
                       __FUNCTION__,
                       currentUrl,
                       newUrl);
        }
    }

    // cleanup and return
    //
    free(currentUrl);
    free(currentMd5);

    return result;
}

/*
 * read the contents of a local file (presumably white/black list)
 * and produce an md5sum of the local file.  the caller is
 * responsible for releasing the returned string
 */
static char* getFileMd5(const char* path)
{
    char *retVal = NULL;
    char *content = readFileContents(path);

    if (content != NULL)
    {
        retVal = icMd5sum(content);
        free(content);
    }

    return retVal;
}

