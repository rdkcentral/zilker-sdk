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
 * backupUtils.c
 *
 * functions for safely reading/writing files
 *   to avoid corrupted files
 *
 * Author: jgleason - 7/5/16
 *-----------------------------------------------*/

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <icLog/logging.h>
#include <icConfig/backupUtils.h>
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>

#define LOG_TAG                     "backupUtil"

/*
 * Backup Utility Functions
 */


/*
 * safeFileSave() - safely save a file by renaming the temporary file
 *   to the desired file and creating a backup if possible.
 *   The temporary file should contain the updated information.
 *   The temporary file will be removed before returning.
 *   The backup file will be created if the desired file existed before the call.
 *
 *  Note: ALL FILENAMES MUST INCLUDE COMPLETE PATH TO FILE.
 *
 *  FLOW:
 *  if (desiredFile exists), desiredFile-->backupFile
 *  else, remove backupFile (it is stale)
 *  finally, tempFile-->desiredFile
 *
 */
void safeFileSave(char *tempFile, char *originalFile, char *backupFile)
{
    // rename config file to .bak if it exists
    //
    if (rename(originalFile, backupFile) != 0)
    {
        if (errno == ENOENT)
        {
            // no file to backup, remove the .bak file if it exists (stale)
            //
            if (unlink(backupFile) != 0 && errno != ENOENT)
            {
                char *errStr = strerrorSafe(errno);
                icLogWarn(LOG_TAG, "%s: Failed to remove file '%s': %s", __FUNCTION__, backupFile, errStr);
                free(errStr);
            }
        }
        else
        {
            // This should not happen, but attempt to put the new file into service anyway
            char *errStr = strerrorSafe(errno);
            icLogWarn(LOG_TAG, "%s: Failed to rename file '%s' to '%s': %s", __FUNCTION__, originalFile, backupFile, errStr);
            free(errStr);
        }
    }

    if (rename(tempFile, originalFile) != 0)
    {
        char *errStr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "%s: Failed to rename file '%s' to '%s': %s", __FUNCTION__, tempFile, originalFile, errStr);
        free(errStr);
    }
}

/*
 * chooseFileToRead() - determines if Original File can be read, if not if Backup File can be read.
 *
 *     returns the fileToRead enum result (ORIGINAL_FILE, BACKUP_FILE, or FILE_NOT_PRESENT)
 *
 */
fileToRead chooseFileToRead(const char *originalFile, const char *backupFile, const char *configDir)
{
    struct stat fileInfo;
    if ((stat(originalFile, &fileInfo) == 0) && (fileInfo.st_size > 0))
    {
        icLogDebug(LOG_TAG, "File is safe to read, %s", originalFile);

        // original file is present and readable
        //
        return ORIGINAL_FILE;
    }
    else
    {
        // does .bak version of config file exist?
        //
        if ((stat(backupFile, &fileInfo) == 0) && (fileInfo.st_size > 0))
        {
            icLogDebug(LOG_TAG, "File does not exist for reading, using backup file - %s", backupFile);
            // .bak file exists with at least 1 byte, so rename it to config then parse it
            //
            return BACKUP_FILE;
        }
        else
        {
            // create the directory (if provided) so that 'write' works later on
            //
            if (configDir != NULL && mkdir(configDir, 0755) != 0)
            {
                icLogWarn(LOG_TAG, "error creating directory %s - %d %s", configDir, errno, strerror(errno));
            }

            // neither original nor backup file exist - file must be created
            //
            icLogWarn(LOG_TAG, "Original and backup files are not present - must create a new default %s", originalFile);
            return FILE_NOT_PRESENT;
        }
    }
}