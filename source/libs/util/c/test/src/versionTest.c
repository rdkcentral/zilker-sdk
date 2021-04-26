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
 * versionTest.c
 *
 * Implement functions that were stubbed from the
 * generated IPC Handler.  Each will be called when
 * IPC requests are made from various clients.
 *
 * Author: jelderton -  6/21/18.
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <icUtil/version.h>
#include "versionTest.h"

#define BAD_INPUT   -99
#define LEFT_WINS   -1
#define RIGHT_WINS  1
#define BOTH_WINS   0

/*
 * returns -99 if unable to proceed
 */
static int compareVersionStrings(const char *left, const char *right)
{
    icVersion leftVer;
    icVersion rightVer;

    // parse 'left'
    memset(&leftVer, 0, sizeof(icVersion));
    if (parseVersionString(left, &leftVer) == false)
    {
        // unable to parse
        //
        printf("version: FAILED to parse '%s' as version\n", left);
        return BAD_INPUT;
    }

    // parse 'right'
    memset(&rightVer, 0, sizeof(icVersion));
    if (parseVersionString(right, &rightVer) == false)
    {
        // unable to parse
        //
        printf("version: FAILED to parse '%s' as version\n", right);
        return BAD_INPUT;
    }

    // do the compare
    return compareVersions(&leftVer, &rightVer);
}

bool runVersionTests()
{
    // compare versions:   1_2_3_4_1234
    //                     1_2
    if (compareVersionStrings("1_2_3_4_1234", "1_2") != LEFT_WINS)
    {
        printf("version: FAILED to compare mismatch\n");
        return false;
    }

    // compare versions:   1_3 & 1_3
    //
    if (compareVersionStrings("1_3", "1_3") != BOTH_WINS)
    {
        printf("version: FAILED to compare equality\n");
        return false;
    }

    // Install snapshot over a release build:   1_2_3_4_SNAPSHOT
    //                                          1_2_3_4_5000
    if (compareVersionStrings("1_2_3_4_SNAPSHOT", "1_2_3_4_5000") != LEFT_WINS)
    {
        printf("version: FAILED to compare snapshot\n");
        return false;
    }

    // realistic test.  install prod version on-top of a SNAPSHOT build
    //
    if (compareVersionStrings("9_9_0_0_1", "9_9_0_0_SNAPSHOT") != LEFT_WINS)
    {
        printf("version: FAILED to compare snapshot #2\n");
        return false;
    }

    return true;
}
