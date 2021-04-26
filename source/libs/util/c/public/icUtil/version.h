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
 * version.h
 *
 * Model the standard iControl Version.
 * When represented as a string, looks similar to:
 *   7_2_0_0_201505221523
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_VERSION_H
#define IC_VERSION_H

#include <stdint.h>
#include <stdbool.h>

// the parsed representation of a "version"
typedef struct _icVersion
{
    uint8_t  releaseNumber;
    uint8_t  serviceUpdateNumber;
    uint8_t  maintenanceReleaseNumber;
    uint64_t hotfixNumber;
    int64_t  buildNumber;
    int32_t  buildNumTolerance;     // used in Upgrade scenarios.  has to be manually assigned
    bool     isSnapshot;
} icVersion;

/*
 * parse the "versionStr" string, and populate a "version" structure
 * returns if the parse was successful or not
 */
extern bool parseVersionString(const char *versionStr, icVersion *target);

/*
 * create the "versionStr" (what could be parsed).  caller
 * MUST free the returned string
 */
extern char *produceVersionString(icVersion *info);

/*
 * compares two version structures to see which is more recent
 * returns:
 *   -1 if 'left' is newer
 *    1 if 'right' is newer
 *    0 if they are the same
 */
extern int compareVersions(icVersion *left, icVersion *right);

/*
 * returns true if the version object is empty (all values set to 0)
 */
extern bool isVersionEmpty(icVersion *info);


#endif // IC_VERSION_H
