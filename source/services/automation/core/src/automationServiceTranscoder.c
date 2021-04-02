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

//
// Created by mkoch201 on 8/12/19.
//

#include "automationServiceTranscoder.h"
#include <cslt/sheens.h>
#include <propsMgr/paths.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define STOCK_SUBDIR    "stock"
#define RULE_SUBDIR     "rules"
#define ACTIONS_SUBDIR  "actions"

static const cslt_factory_t* csltFactory = NULL;

static void settingsMapFree(void *key, void* value)
{
    if (strcmp(key, SHEENS_TRANSCODER_DEVICE_ID_MAPPER) != 0)
    {
        free(value);
    }
}

static void transcoderSettingsFree(void* key, void* value)
{
    /* All settings hashmaps are other hashmaps */
    hashMapDestroy(value, settingsMapFree);
}

void automationTranscoderInit(void)
{
    icHashMap* settings = hashMapCreate();
    icHashMap* sheensSettings = hashMapCreate();

    char* homeDir;

    homeDir = getStaticPath();
    if (homeDir) {
        char* actionListDir;

        /* Create the buffer for the action list directory.
         * The '2' is for the '/'s in the directory path.
         */
        actionListDir = malloc(2 + strlen(homeDir) + strlen(STOCK_SUBDIR) + strlen(ACTIONS_SUBDIR) + 1);
        sprintf(actionListDir, "%s/%s/%s", homeDir, STOCK_SUBDIR, ACTIONS_SUBDIR);
        free(homeDir);

        hashMapPut(settings,
                   TRANSCODER_NAME_SHEENS,
                   (uint16_t) strlen(TRANSCODER_NAME_SHEENS),
                   sheensSettings);

        hashMapPut(sheensSettings,
                   SHEENS_TRANSCODER_SETTING_ACTION_LIST_DIR,
                   (uint16_t) strlen(SHEENS_TRANSCODER_SETTING_ACTION_LIST_DIR),
                   actionListDir);
    } else {
        /* We cannot get the action list dir so just let cslt use the defaults. */
        hashMapDestroy(sheensSettings, NULL);
    }

    cslt_init(settings);
    csltFactory = cslt_get_transcode_factory(TRANSCODER_NAME_SHEENS);

    // Release the settings.
    hashMapDestroy(settings, transcoderSettingsFree);
}

const cslt_transcoder_t *automationServiceGetTranscoder(const char *specification)
{
   return cslt_get_transcoder(csltFactory, specification);
}