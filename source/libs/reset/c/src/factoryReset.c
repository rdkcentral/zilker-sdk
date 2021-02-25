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
 * factoryReset.c
 *
 * Set of functions available for resetting the device
 * to the factory settings.
 *
 * Author: jelderton, gfaulkner - 6/19/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icReset/factoryReset.h>
#include <icReset/shutdown.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/fileUtils.h>
#include <propsMgr/paths.h>
#include <commMgr/commService_ipc.h>
#include <watchdog/watchdogService_ipc.h>

// set of files normally left intact after resetToFactory
//
static const char *SPECIAL_FILES[] = {
        "/communication.conf",
        "/communication.conf.bak",
        "/network.config",
        "/security.conf",
        NULL            // must be last to terminate the array
};

// set of files to never delete
//
static const char *RESERVED_FILES[] = {
        ".",
        "..",
        ".badblks",
        ".counts",
        ".reserved",
        "communication.conf",
        "macAddress",
        "provisionComplete",
        "serialNumber",
        "branding",
        "lost+found",
        NULL                // must be last to terminate the array
};

/*
 * private function declarations
 */
static void doReset(bool killProcesses);
static void recurseDir(char *fullDirPath, icLinkedList *targetList);

/*
 * Function to reset the CPE to factory defaults, and then reboot
 * Note that some settings are not reset
 */
void resetToFactory()
{
    // reset and kill processes
    //
    doReset(true);

    // now perform the shutdown of the system
    //
#ifdef CONFIG_LIB_SHUTDOWN
    icShutdown(SHUTDOWN_RESET);
#endif
}

/*
 * Resets the device completely, so that the branded factory defaults will be used
 */
void resetForRebranding()
{
    char *dir = getDynamicConfigPath();

    // reset and kill processes
    //
    doReset(true);

    // remove special files that are not part of standard reset
    //
    int i = 0;
    char path[1024];
    while (SPECIAL_FILES[i] != NULL)
    {
        sprintf(path, "%s%s", dir, SPECIAL_FILES[i]);
        int rc = unlink(path);
        if (rc != 0 && errno == EISDIR)
        {
            // this is a directory, not a file
            //
            deleteDirectory(path);
        }
        i++;
    }

    // cleanup
    //
    free(dir);

    // now perform the shutdown of the system
    //
#ifdef CONFIG_LIB_SHUTDOWN
    icShutdown(SHUTDOWN_RESET);
#endif
}

/*
 *
 */
static void doReset(bool killProcesses)
{
    icLinkedList *filenameList = linkedListCreate();

    // ask propsService for the configuration directory before we shut everything down
    //
    char *configDir = getDynamicConfigPath();

    if (killProcesses == true)
    {
        // before stopping services, reset commService settings to factory
        //
        icLogInfo("reset", "asking commService to reset");
        bool success = false;
        IPCCode rc = commService_request_RESET_COMM_SETTINGS_TO_DEFAULT(&success);
        if (rc != IPC_SUCCESS)
        {
            icLogError("reset", "failure to reset commService - %s", IPCCodeLabels[rc]);
        }

        // ensure nothing is running before we start deleting files.
        // otherwise services could re-write their files between now
        // and the effective restart (be a soft-boot of our services or
        // a hard-boot of the device)
        //
        icLogInfo("reset", "asking watchdog to shutdown all services");
        shutdownOptions *options = create_shutdownOptions();
#ifdef CONFIG_LIB_SHUTDOWN
        // going to reboot when this is done, so let it all go down nicely
        options->exit = true;
#else
        // not able to hard-boot, so leave watchdog running to allow a soft-boot
        options->exit = false;
#endif
        options->forReset = true;
        watchdogService_request_SHUTDOWN_ALL_SERVICES_timeout(options, 0);
        destroy_shutdownOptions(options);
    }

    // now, loop through all of the files in /opt/etc; remove all of them except for
    // the communication.conf and the macAddress
    //
    recurseDir(configDir, filenameList);
    free(configDir);
    configDir = NULL;

    // delete files from the linked list
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(filenameList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        char *next = (char *)linkedListIteratorGetNext(loop);

        // remove the file or directory
        //
        icLogDebug("reset", "deleting %s", next);
        int rc = unlink(next);
        if (rc != 0 && errno == EISDIR)
        {
            // this is a directory, not a file - so delete differently
            // ignore if this fails.  simply means the dir isn't empty
            //
            rc = rmdir(next);
        }

        // log the result if failure
        //
        if (rc != 0)
        {
            icLogWarn("reset", "problem deleting %s : %d - %s", next, rc, strerror(errno));
        }
    }
    linkedListIteratorDestroy(loop);
    linkedListDestroy(filenameList, NULL);

    // force the filesystem to apply the file removals
    //
    sync();
}

/*
 * see if 'filename' is in our RESERVED_FILES array
 */
static bool isReservedFilename(char *filename)
{
    if (filename == NULL)
    {
        return true;
    }

    // assume file. check the filename to see if it matches any
    // of our reserved filenames (meaning do not remove it)
    //
    int j = 0;
    while (RESERVED_FILES[j] != NULL)
    {
        if (strcmp(filename, RESERVED_FILES[j]) == 0)
        {
            // matches
            return true;
        }

        // go to next reserved file
        //
        j++;
    }

    // not found
    //
    return false;
}

/*
 * loop through the files in 'fullDirPath', adding each non-reserved
 * filename to the 'targetList'
 */
static void recurseDir(char *fullDirPath, icLinkedList *targetList)
{
    // first open the directory so we can get the filenames within
    //
    DIR *directory = opendir(fullDirPath);
    if (directory == NULL)
    {
        printf("[reset] Cannot open %s for reading: %s\n", fullDirPath, strerror(errno));
        icLogWarn("reset", "Cannot open %s for reading: %s", fullDirPath, strerror(errno));
        return;
    }

    // now, loop through all of the files in /opt/etc; remove all of them except for
    // the communication.conf and the macAddress
    //
    icLogDebug("reset", "examining directory %s", fullDirPath);
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL)
    {
        // skip 'self' and 'parent'
        //
        if ((entry->d_type == DT_DIR) &&
            (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0))
        {
            continue;
        }

        // skip links.  has the potential of endless recursion
        //
        if (entry->d_type == DT_LNK)
        {
            continue;
        }

        // skip if this name is in the reserved list (dir or file)
        //
        if (isReservedFilename(entry->d_name) == true)
        {
            icLogDebug("reset", "skipping reserved file %s", entry->d_name);
            continue;
        }

        // see if this entry is a sub-dir
        //
        if (entry->d_type == DT_DIR)
        {
            // create the new path, then recurse into that dir
            //
            char *subdir = malloc(sizeof(char) * (strlen(fullDirPath) + strlen(entry->d_name) + 2));
            sprintf(subdir, "%s/%s", fullDirPath, entry->d_name);
            recurseDir(subdir, targetList);

            // add the subdir name to the list so the directory gets removed (if empty)
            //
            icLogDebug("reset", "adding dir %s to the 'can be deleted' list", subdir);
            linkedListAppend(targetList, subdir);
            subdir = NULL;
        }
        else
        {
            // add this filename to the list so we can delete it
            //
            char *buffer = malloc(sizeof(char) * (strlen(fullDirPath) + strlen(entry->d_name) + 2));
            sprintf(buffer, "%s/%s", fullDirPath, entry->d_name);
            icLogDebug("reset", "adding %s to the 'can be deleted' list", buffer);
            linkedListAppend(targetList, buffer);
            buffer = NULL;
        }
    }
    icLogDebug("reset", "done examining directory %s", fullDirPath);

    // cleanup the directory
    //
    closedir(directory);
}

