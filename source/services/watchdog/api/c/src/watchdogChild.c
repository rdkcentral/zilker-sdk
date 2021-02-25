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
 * watchdogChild.c
 *
 * Common definitions from watchdog that children
 * processes may wish to utilize.
 *
 * Author: John Elderton  -  9/4/19
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <watchdog/watchdogChild.h>

// looks for the existance of the CHILD_WAS_RESTARTED_ENV_VAR environment variable,
// and returns true if it is set.  that signifies that the calling process was
// restarted due to a crash
//
bool wasRestartedDueToCrash()
{
    // look for the existance of CHILD_WAS_RESTARTED_ENV_VAR
    //
    char *flag = getenv(CHILD_WAS_RESTARTED_ENV_VAR);
    if (flag != NULL && strcasecmp(flag, "true") == 0)
    {
        return true;
    }
    return false;
}

