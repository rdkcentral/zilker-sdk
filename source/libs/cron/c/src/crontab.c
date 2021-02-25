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

// Ported by dcalde202 on 11/27/18.
// Created by gfaulk200 on 8/10/18.
//

#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <icLog/logging.h>
#include <propsMgr/paths.h>
#include <xhCron/crontab.h>
#include <icTypes/icStringHashMap.h>
#include <icConfig/storage.h>
#include "icBuildtime.h"

#define STORAGE_NAMESPACE "cron"
#define VERSION_STRING "#CCCRONV1"
/** Cron file paths **/
#define CRON_ROOT_LOCK_FILEPATH "/cron/crontabs/root.lock"
#define CRON_ROOT_FILEPATH      "/cron/crontabs/root" // the file which contains the cron entries
#define CRON_UPDATE_FILEPATH    "/cron/crontabs/cron.update"
#define LOG_TAG "xhCron"
#define PATH_BUFFER 128
#define NANOS_IN_ONE_MILLI 1000000
#define LOCK_FILE_RETRIES 5

static pthread_mutex_t fileMTX = PTHREAD_MUTEX_INITIALIZER;


static char cronRootLockFilePath[PATH_BUFFER] = "";

static char* getCronRootLockFilePath()
{
    if(cronRootLockFilePath[0] == 0 )
    {
        char *path = getDynamicConfigPath();
        snprintf(cronRootLockFilePath, PATH_BUFFER, "%s%s", path, CRON_ROOT_LOCK_FILEPATH);
        free(path);
    }
    return cronRootLockFilePath;
}

static char cronRootFilePath[PATH_BUFFER] = "";

static char* getCronRootFilePath()
{
    if(cronRootFilePath[0] == 0 )
    {
        char *path = getDynamicConfigPath();
        snprintf(cronRootFilePath, PATH_BUFFER, "%s%s", path, CRON_ROOT_FILEPATH);
        free(path);
    }
    return cronRootFilePath;
}

static char cronUpdateFilePath[PATH_BUFFER] = "";

static char* getCronUpdateFilePath()
{
    if(cronUpdateFilePath[0] == 0 )
    {
        char *path = getDynamicConfigPath();
        snprintf(cronUpdateFilePath, PATH_BUFFER, "%s%s", path, CRON_UPDATE_FILEPATH);
        free(path);
    }
    return cronUpdateFilePath;
}

static void createCronUpdateFile()
{
#ifndef CONFIG_DEBUG_ZITH_CI_TESTS
#ifdef CONFIG_OS_LINUX
    // On linux we can't do the update file mechanism, just call crontab with the name of the file that
    // contains the entries and it will add them as the users crontab.  Only gotcha is this may blow away
    // any user defined crontab that existed
    char *cronFilePath = getCronRootFilePath();
    char *buf = NULL;
    int bufSize = snprintf(buf, 0, "crontab %s", cronFilePath);
    buf = malloc(bufSize + 1);
    snprintf(buf, bufSize + 1, "crontab %s", cronFilePath);
    if (system(buf) != 0)
    {
        icLogError(LOG_TAG, "Failed to update crontab");
    }
    free(buf);
#else
    FILE *cronUpdateFile = fopen(getCronUpdateFilePath(),"w");
    if (cronUpdateFile == NULL)
    {
        icLogError(LOG_TAG,"%s: Unable to create %s: %s",__FUNCTION__,getCronUpdateFilePath(),strerror(errno));
        return;
    }
    fprintf(cronUpdateFile,"root\n");
    fflush(cronUpdateFile);
    fclose(cronUpdateFile);
#endif //CONFIG_OS_LINUX
#endif //CONFIG_DEBUG_ZITH_CIT_TESTS
}

static icStringHashMap *parseFromStorage()
{
    icStringHashMap *retval = stringHashMapCreate();

    icLinkedList *keys = storageGetKeys(STORAGE_NAMESPACE);
    if (keys != NULL)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(keys);
        while (linkedListIteratorHasNext(iter))
        {
            char *key = (char *) linkedListIteratorGetNext(iter);

            char *value;
            if (storageLoad(STORAGE_NAMESPACE, key, &value) == true)
            {
                stringHashMapPut(retval, strdup(key), value);
            }
        }
        linkedListIteratorDestroy(iter);
    }
    linkedListDestroy(keys, NULL);

    return retval;
}

static int saveCrontab(icStringHashMap *crontabEntries)
{
    FILE *crontab = fopen(getCronRootFilePath(),"w");
    if (crontab == NULL)
    {
        icLogError(LOG_TAG,"%s: Unable to open %s for writing: %s",__FUNCTION__,getCronRootFilePath(),strerror(errno));
        return -1;
    }
    fprintf(crontab,"%s\n",VERSION_STRING);
    icStringHashMapIterator *iter = stringHashMapIteratorCreate(crontabEntries);
    while(stringHashMapIteratorHasNext(iter))
    {
        char *key;
        char *value;
        stringHashMapIteratorGetNext(iter, &key, &value);
        fprintf(crontab,"#%s\n",key);
        fprintf(crontab,"%s\n",value);
    }
    stringHashMapIteratorDestroy(iter);

    fflush(crontab);
    fclose(crontab);
    createCronUpdateFile();
    return 0;
}

struct sigaction sa;
struct sigaction originalAction;
static bool handlerInstalled = false;


static int lockFileFD = -1;
static void releaseFileLock()
{
    if (lockFileFD >= 0)
    {
        flock(lockFileFD,LOCK_UN);
        close(lockFileFD);
        unlink(getCronRootLockFilePath());
    }
    pthread_mutex_unlock(&fileMTX);
}

static void releaseFileLockOnSig(int nSignum)
{
    releaseFileLock();
    // Restore the original and re-raise
    sigaction(SIGSEGV,&originalAction,NULL);
    raise(nSignum);
}

static int getFileLock()
{
    pthread_mutex_lock(&fileMTX);
    lockFileFD = -1;
    // Try to avoid making clients deal with lock file failures, spin and retry
    // TODO long term we want a scheduler service that won't have to deal with lock files and can deal with cron
    // expressions and timers that fire only one time
    for (int i = 0; lockFileFD < 0 && i < LOCK_FILE_RETRIES; ++i)
    {
        lockFileFD = open(getCronRootLockFilePath(), O_CREAT | O_RDWR | O_EXCL, S_IRWXU);
        if (lockFileFD < 0)
        {
            // Sleep and try again
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 200*NANOS_IN_ONE_MILLI };
            // coverity[sleep]
            nanosleep(&ts, NULL);
        }
    }
    if (lockFileFD < 0)
    {
        icLogError(LOG_TAG,"%s: Unable to create lock file (%s): %s",__FUNCTION__,getCronRootLockFilePath(),strerror(errno));
        pthread_mutex_unlock(&fileMTX);
        return -1;
    }
    flock(lockFileFD,LOCK_EX);
    // We have the lock here, so this should happen once
    if (handlerInstalled == false)
    {
        sa.sa_handler = releaseFileLockOnSig;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGSEGV,&sa,&originalAction);
        handlerInstalled = true;
    }

    return 0;
}

/**
 * add or update a pre-formatted crontab entry in the root crontab file
 * @param entryLine the entire crontab line to add or update
 * @param entryName the name of this entry, for future reference
 * @return 0 on success, non-zero otherwise
 */
int addOrUpdatePreformattedCrontabEntry(const char *entryLine, const char *entryName)
{
    int rc = getFileLock();
    if (rc < 0)
    {
        icLogError(LOG_TAG,"%s: Unable to get file lock",__FUNCTION__);
        return -1;
    }
    // Create/Update storage.  Storage is our version of the truth.  This way we don't rely on parsing a specially
    // formatted crontab, which was fragile.  We then take what is in storage and construct our crontab
    storageSave(STORAGE_NAMESPACE, entryName, entryLine);
    // Get the new list
    icStringHashMap *crontabEntries = parseFromStorage();
    // Take care of crontab
    icLogDebug(LOG_TAG,"%s: saving crontab file for add/update of %s",__FUNCTION__,entryName);
    int retval = saveCrontab(crontabEntries);

    // Cleanup
    stringHashMapDestroy(crontabEntries, NULL);
    releaseFileLock();
    return retval;
}

/**
 * remove a crontab entry
 * @param entryName the entry name of the entry to remove
 * @return 0 on success, non-zero otherwise
 */
int removeCrontabEntry(const char *entryName)
{
    int retval = -1;
    int rc = getFileLock();
    if (rc < 0)
    {
        icLogError(LOG_TAG,"%s: unable to get file lock",__FUNCTION__);
        return retval;
    }

    // Try to delete from storage
    if (storageDelete(STORAGE_NAMESPACE, entryName) == true)
    {
        // Get and save the entries
        icStringHashMap *crontabEntries = parseFromStorage();
        retval = saveCrontab(crontabEntries);
        stringHashMapDestroy(crontabEntries, NULL);
    }
    else
    {
        icLogWarn(LOG_TAG,"%s: did not find crontab entry with name %s",__FUNCTION__,entryName);
    }

    releaseFileLock();
    return retval;
}


int hasCrontabEntry(const char *entryLine, const char *entryName)
{
    int rc = getFileLock();
    if (rc < 0)
    {
        icLogError(LOG_TAG,"%s: unable to get file lock",__FUNCTION__);
        return -2;
    }
    icStringHashMap *crontabEntries = parseFromStorage();
    int retval = -1;
    char *value = stringHashMapGet(crontabEntries, entryName);
    if (value != NULL)
    {
        if(entryLine != NULL && strcmp(value,entryLine) == 0)
        {
            retval = 0;  //entryName exists and entryLine matches
        }
        else
        {
            retval = 1;  //entryName exists and entryLine doesn't match
        }
    }
    stringHashMapDestroy(crontabEntries, NULL);
    releaseFileLock();
    return retval;  //entryName doesn't exist
}


