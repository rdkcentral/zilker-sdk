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
// Created by Christian Leithner on 1/5/20.
//

#ifndef ZILKER_AUTOMATIONCONSTANTS_H
#define ZILKER_AUTOMATIONCONSTANTS_H

/* Automation Utility Constants */
#define ENV_KEY_ZILKER_SDK_TOP "ZILKER_SDK_TOP"

#define AUTOMATION_UTIL_ORIG_AUTOMATION_FILENAME "originalAutomation"
#define AUTOMATION_UTIL_METADATA_FILENAME "metadata"
#define AUTOMATION_UTIL_LEGACY_FILENAME "legacySpec"
#define AUTOMATION_UTIL_ASSEMBLED_FILENAME "assembledAutomation"

#define URI_AUTOMATION_UTIL_DIR "/tools/automationTool"
#define URI_OUT_DIR "/out"
#define URI_ASSEMBLED_DIR "/assembled"
#define URI_DISASSEMBLED_DIR "/disassembled"
#define URI_METADATA_DIR "/metadata"
#define URI_SPECIFICATION_DIR "/specification"
#define URI_ORIG_SPECIFICATION_DIR "/origSpec"
#define URI_NODES_DIR "/nodes"

/* JSON CONSTANTS */

/* AUTOMATION JSON KEYS */
#define JSON_KEY_ENABLED "enabled"
#define JSON_KEY_SPEC "spec"
#define JSON_KEY_DATE_CREATED "dateCreated"
#define JSON_KEY_CONSUMED_COUNT "consumedCount"
#define JSON_KEY_EMITTED_COUNT "emittedCount"
#define JSON_KEY_ORIG_SPEC "origSpec"
#define JSON_KEY_TRANSCODER_VERSION "transcoderVersion"
#define JSON_KEY_SOURCE "source"

/* SHEENS SPECIFICATION KEYS */
#define SHEENS_KEY_SHEENS_VERSION "sheensVersion"
#define SHEENS_KEY_NAME "name"
#define SHEENS_KEY_NODES "nodes"

/* XML CONSTANTS */

#endif //ZILKER_AUTOMATIONCONSTANTS_H
