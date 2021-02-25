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
 * systemMode.h
 *
 * Define the state machine for the 'system mode'.
 * Keeps a config file to track the mode, so requires
 * an initialization call during startup.
 *
 * NOTE: this has it's own mutex and is not part
 *       of the shared SECURITY_MTX.
 *
 * Author: jelderton - 7/9/15
 *-----------------------------------------------*/

#ifndef IC_SYSTEM_MODE_H
#define IC_SYSTEM_MODE_H

#include <stdint.h>
#include <stdbool.h>
#include <securityService/securityService_pojo.h>

/*
 * define the set of possible 'system modes' (used to be called 'scenes')
 */
typedef enum
{
    SYSTEM_MODE_HOME = 0,
    SYSTEM_MODE_AWAY,
    SYSTEM_MODE_NIGHT,
    SYSTEM_MODE_VACATION
} systemModeSet;

/*
 * define the inner label for each systemModeSet value
 * note that these are internal and not display worthy
 * as they would need internationalization
 *
 * these directly correlate to CannedScenes.java
 */
static const char *systemModeNames[] = {
    SYSTEM_MODE_HOME_LABEL,     // SYSTEM_MODE_HOME
    SYSTEM_MODE_AWAY_LABEL,     // SYSTEM_MODE_AWAY
    SYSTEM_MODE_NIGHT_LABEL,    // SYSTEM_MODE_NIGHT
    SYSTEM_MODE_VACATION_LABEL, // SYSTEM_MODE_VACATION
};

/*
 * one-time init to load the config.  since this makes
 * requests to propsService, should not be called until
 * all of the services are available.
 */
void initSystemMode();

/*
 * called during RMA/Restore
 */
bool restoreSystemModeConfig(char *tempDir, char *destDir);

/*
 * returns the current systemMode value
 *
 * NOTE: should only be called if "supportSystemMode() == true"
 */
systemModeSet getCurrentSystemMode();

/*
 * set the current systemMode value
 * returns false if the mode didn't change
 * (generally because it matches the current mode)
 *
 * NOTE: should only be called if "supportSystemMode() == true"
 */
bool setCurrentSystemMode(systemModeSet newMode, uint64_t requestId);

/**
 * gets the version of the storage file
 */
uint64_t getSystemModeConfigFileVersion();

#endif // IC_SYSTEM_MODE_H
