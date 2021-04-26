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
 * version.c
 *
 * Model the standard iControl Version.
 * When represented as a string, looks similar to:
 *   7_2_0_0_201505221523
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#include <icUtil/version.h>
#include <icUtil/stringUtils.h>

#define DELIMETER   "_"
#define SNAPSHOT_BUILD_NUMBER   -99

/*
 * parse the "versionStr" string, and populate a "version" structure
 * returns if the parse was successful or not
 */
bool parseVersionString(const char *versionStr, icVersion *target)
{
    int tokensProcessed = 0;

    // strings are formted as R_SU_MR_HF_build
    // so tokenize on underscore and populate the values
    //
    if (versionStr != NULL && target != NULL)
    {
        // zero out all of the numbers within 'target'
        //
        memset(target, 0, sizeof(icVersion));

        // ensure string ends with our token so we don't have
        // do perform some crazy logic to grab the last piece
        //
        char *copy = (char *)malloc(strlen(versionStr) + strlen(DELIMETER) + 1);
        sprintf(copy, "%s%s", versionStr, DELIMETER);

        // being getting tokens from the input string
        //
        char *ctx;
        char *token = strtok_r(copy, DELIMETER, &ctx);
        if (token != NULL)
        {
            // extract R, then go to next token
            stringToUint8((const char *)token, &target->releaseNumber);
            token = strtok_r(NULL, DELIMETER, &ctx);
            tokensProcessed++;
        }
        if (token != NULL)
        {
            // extract SU, then go to next token
            stringToUint8((const char *)token, &target->serviceUpdateNumber);
            token = strtok_r(NULL, DELIMETER, &ctx);
            tokensProcessed++;
        }
        if (token != NULL)
        {
            // extract MR, then go to next token
            stringToUint8((const char *)token, &target->maintenanceReleaseNumber);
            token = strtok_r(NULL, DELIMETER, &ctx);
            tokensProcessed++;
        }
        if (token != NULL)
        {
            // extract HF, then go to next token
            stringToUint64((const char *)token, &target->hotfixNumber);
            token = strtok_r(NULL, DELIMETER, &ctx);
            tokensProcessed++;
        }
        if (token != NULL)
        {
            // special case for SNAPSHOT
            //
            if (strcmp(token, "SNAPSHOT") == 0)
            {
                target->isSnapshot = true;
                target->buildNumber = SNAPSHOT_BUILD_NUMBER;
            }
            else
            {
                // extract buildNumber
                stringToInt64((const char *)token, &target->buildNumber);
            }
            tokensProcessed++;
        }

        // mem cleanup
        //
        free(copy);
    }

    // success if we got at least 2 tokens processed
    //
    return (tokensProcessed >= 2);
}

/*
 * create the "versionStr" (what could be parsed).  caller
 * MUST free the returned string
 */
char *produceVersionString(icVersion *info)
{
    // return a string that looks similar to:
    //    7_2_0_0_201505221523
    //
    char retVal[1024];

    if (info->isSnapshot == true)
    {
        // add numbers + SNAPSHOT
        //
        sprintf(retVal, "%" PRIu8 "_%" PRIu8 "_%" PRIu8 "_%" PRIu64 "_SNAPSHOT", info->releaseNumber,
                info->serviceUpdateNumber, info->maintenanceReleaseNumber, info->hotfixNumber);
    }
    else
    {
        // add basic numbers
        //
        sprintf(retVal, "%" PRIu8 "_%" PRIu8 "_%" PRIu8 "_%" PRIu64 "_%"PRIi64, info->releaseNumber,
                info->serviceUpdateNumber, info->maintenanceReleaseNumber, info->hotfixNumber, info->buildNumber);
    }

    // return copy
    //
    return strdup(retVal);
}

/*
 * compares two version structures to see which is more recent
 * returns:
 *   -1 if 'left' is newer
 *    1 if 'right' is newer
 *    0 if they are the same
 */
int compareVersions(icVersion *left, icVersion *right)
{
    // start at the top, and compare each number
    // first, release number
    //
    if (left->releaseNumber > right->releaseNumber)
    {
        return -1;
    }
    else if (right->releaseNumber > left->releaseNumber)
    {
        return 1;
    }

    // compare SU number
    //
    if (left->serviceUpdateNumber > right->serviceUpdateNumber)
    {
        return -1;
    }
    else if (right->serviceUpdateNumber > left->serviceUpdateNumber)
    {
        return 1;
    }

    // compare MR number
    //
    if (left->maintenanceReleaseNumber > right->maintenanceReleaseNumber)
    {
        return -1;
    }
    else if (right->maintenanceReleaseNumber > left->maintenanceReleaseNumber)
    {
        return 1;
    }

    // compare HF number
    //
    if (left->hotfixNumber > right->hotfixNumber)
    {
        return -1;
    }
    else if (right->hotfixNumber > left->hotfixNumber)
    {
        return 1;
    }

    // if 'either' are snapshots then allow the update.
    // this way we can go to/from a SNAPSHOT build
    //
    if (left->isSnapshot == true || right->isSnapshot == true)
    {
        // claim left is greater (allow it to win)
        //
        return -1;
    }

    // now the build number, incorporating the 'tolerance' to the left
    // (this is how the Java implementation did it)
    //
    if ((left->buildNumber + left->buildNumTolerance) > right->buildNumber)
    {
        return -1;
    }
    else if ((left->buildNumber + left->buildNumTolerance) < right->buildNumber)
    {
        return 1;
    }

    // must be the same
    //
    return 0;
}

/*
 * returns true if the version object is empty (all values set to 0)
 */
bool isVersionEmpty(icVersion *info)
{
    if (info->releaseNumber == 0 &&
        info->serviceUpdateNumber == 0 &&
        info->maintenanceReleaseNumber == 0 &&
        info->hotfixNumber == 0 &&
        (info->buildNumber == 0 || info->buildNumber == SNAPSHOT_BUILD_NUMBER))
    {
        return true;
    }

    return false;
}

