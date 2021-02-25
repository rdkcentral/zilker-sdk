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
// Created by Boyd, Weston on 4/17/18.
//

#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <libxml/tree.h>

#include <cslt/cslt.h>
#include <cslt/sheens.h>
#include <cslt/icrules.h>

#include <icrule/icrule.h>

#include "../cslt_internal.h"
#include "icrule_transcoders.h"
#include "../passthru_transcoder.h"

#define SHEENS_VERSION_KEY "sheensVersion"

// Start Sheens to iControl Legacy Rule transcode.

/** Decoding from Sheens so make sure
 * that the schema being verified is JSON and
 * has the required version field "sheensVersion".
 *
 * @param schema String containing some specification definition.
 * @return True if this transcoder can parse this schema, otherwise false.
 */
static bool sheens2icrule_is_valid(const char* schema)
{
    cJSON* root;
    bool valid = false;

    if (schema == NULL) return false;
    if (strlen(schema) == 0) return false;

    root = cJSON_Parse(schema);
    if ((root != NULL) && (cJSON_IsObject(root))) {
        cJSON* version = cJSON_GetObjectItem(root, SHEENS_VERSION_KEY);

        if (version) {
            valid = true;
        }
    }

    cJSON_Delete(root);

    return valid;
}

static int sheens2icrule_transcode(const char* src, char** dst, size_t size)
{
    return -1;
}

// Start iControl Legacy rule transcode pass through.

/** Decoding from iControl Legacy rules so make sure
 * that the schema being verified is XML and
 * has the namespace of "ruels/v1".
 *
 * @param schema String containing some specification definition.
 * @return True if this transcoder can parse this schema, otherwise false.
 */
static bool icrule2icrule_is_valid(const char* schema)
{
    bool valid = false;

    xmlDocPtr doc;

    if (schema == NULL) return false;
    if (strlen(schema) == 0) return false;

    doc = xmlParseMemory(schema, (int) strlen(schema));
    if (doc != NULL) {
        xmlNodePtr node;

        node = xmlDocGetRootElement(doc);
        if (node != NULL) {
            /* xmlSearchNsByHref returns a valid pointer to a node.
             * It DOES NOT create a clone of it. Thus you  DO NOT
             * need to free it!
             */
            xmlNsPtr ns2 = xmlSearchNsByHref(doc, node, (const xmlChar*) "http://ucontrol.com/rules/v1.0");

            valid = (ns2 != NULL);
        }
    }

    xmlFreeDoc(doc);

    return valid;
}

static cslt_factory_t icrule_factory = {
        .encoder = TRANSCODER_NAME_ICRULES
};

static const cslt_transcoder_t sheens2icrule_transcoder = {
        .decoder = TRANSCODER_NAME_SHEENS,
        .encoder = TRANSCODER_NAME_ICRULES,

        .is_valid = sheens2icrule_is_valid,
        .transcode = sheens2icrule_transcode,
        .transcoder_version = 0
};

static const cslt_transcoder_t icrule2icrule_transcoder = {
        .decoder = TRANSCODER_NAME_ICRULES,
        .encoder = TRANSCODER_NAME_ICRULES,

        .is_valid = icrule2icrule_is_valid,
        .transcode = passthru_transcode,
        .transcoder_version = 0
};

void icrule_transcoder_init(icHashMap* settings)
{
    char* icrule_action_list_dir = NULL;

    if (settings != NULL) {
        icrule_action_list_dir = hashMapGet(settings,
                                            SHEENS_TRANSCODER_SETTING_ACTION_LIST_DIR,
                                            (uint16_t) strlen(SHEENS_TRANSCODER_SETTING_ACTION_LIST_DIR));
    }

    icrule_set_action_list_dir(icrule_action_list_dir);

    cslt_register_factory(&icrule_factory);

    cslt_register_transcoder(&sheens2icrule_transcoder);
    cslt_register_transcoder(&icrule2icrule_transcoder);
}
