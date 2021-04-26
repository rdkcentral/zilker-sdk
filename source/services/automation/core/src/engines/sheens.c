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
// Created by wboyd747 on 10/25/18.
//

#include <string.h>
#include <pthread.h>
#include <malloc.h>
#include <errno.h>

#include <littlesheens/machines.h>

#include <automationService.h>
#include <automationEngine.h>
#include <icTypes/icHashMap.h>
#include <automationAction.h>

#include "sheens.h"

#define HASHMAP_KEYLEN(key) ((uint16_t) strlen(key))

#define sheens_version_key "sheensVersion"
#define JSON_EMITTED "emitted"

#define JSON_CREW_ID "spec"
#define JSON_CREW_NODE "node"
#define JSON_CREW_BINDINGS "bs"

#define TO_PROP "to"
#define CONSUMED_PROP "consumed"
#define ID_PROP "id"
#define MACHINES_PROP "machines"
#define AUTOMATIONS_PROP "automations"
#define START_NODE "start"

#define CREW_BUFFER_STEP_SIZE (1024)
#define STEPPEDS_BUFFER_STEP_SIZE (1024)

/* This is used to manage the current iteration
 * of littlesheens as it expects us to track
 * the context.
 *
 * With the current 0.1.0 version the context is
 * not passed to each routine. Instead the
 * library expects either:
 * (a) that we never change the context after "set".
 * (b) that we call "set" before every littlesheens call.
 *
 * We have chosen (a) for now.
 */
static void* sheensContext;

static icHashMap* specMap;

static cJSON* crew;
static cJSON* crewMachines; // Belongs to 'crew'

static bool needStateUpdate = true;

static char* steppeds;
static size_t stepped_size = STEPPEDS_BUFFER_STEP_SIZE;

static char* state;
static size_t state_size = CREW_BUFFER_STEP_SIZE;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void freeSpecMap(void* key, void* value)
{
    free(key);
    unused(value);
}

/**
 * Update the crew string state buffer with the
 * latest JSON representation.
 *
 * If the buffer is not large enough to hold the crew state
 * then expand the buffer.
 *
 * @param buffer The buffer to hold the string representation
 * of the current crew state.
 * @return The same as 'buffer' if the memory was not
 * [re]allocated, otherwise the pointer to the new buffer. The
 * caller should only free the buffer when they 100% done with the
 * crew.
 */
static void updateCrewState(void)
{
    pthread_mutex_lock(&mutex);
    /* Only update the crew state if the JSON object has been updated. */
    if (needStateUpdate) {
        dbg(VERBOSITY_LEVEL_0, "");

        while (!cJSON_PrintPreallocated(crew, state, (const int) state_size, false)) {
            state_size += CREW_BUFFER_STEP_SIZE;
            state = realloc(state, state_size);
        }

        needStateUpdate = false;
    }
    pthread_mutex_unlock(&mutex);
}

/**
 * Provide littesheens with the desired machine specification.
 *
 * Note: Littlesheens will not manage (or own) the specification buffer as it
 * was configured to allow the specification provider to manage it. This saves
 * a number of 'strdup' calls.
 *
 * @param ctx unused
 * @param specName The name of the machine specification.
 * @param cached The cached version of the machine specification, otherwise NULL.
 * @return The string representation of the machine specification.
 */
static char* spec_provider(void* ctx, const char* specName, const char* cached)
{
    char* spec;

    unused(ctx);
    unused(cached);

    dbg(VERBOSITY_LEVEL_0, "specName=%s", specName);

    pthread_mutex_lock(&mutex);
    spec = hashMapGet(specMap, (void*) specName, HASHMAP_KEYLEN(specName));
    pthread_mutex_unlock(&mutex);

    return spec;
}

/**
 * Process the crew state portion related to the machine.
 *
 * Warning: Mutex is expected to be held!
 *
 * @param machine The current machine state JSON object.
 * @param stepJson The JSON object representing the new
 * machine state.
 * @return True if updated the crew machine state.
 */
static bool handle_step_crew(cJSON* machine, cJSON* stepJson)
{
    bool ret = false;
    cJSON* json;

    if (stepJson) {
        json = cJSON_GetObjectItem(stepJson, TO_PROP);
        if (machine && json) {
            cJSON* jsonObject;

            jsonObject = cJSON_DetachItemFromObject(json, JSON_CREW_NODE);
            if (jsonObject) {
                ret = true;

                cJSON_ReplaceItemInObject(machine, JSON_CREW_NODE, jsonObject);

                jsonObject = cJSON_DetachItemFromObject(json, JSON_CREW_BINDINGS);
                if (jsonObject) {
                    cJSON_ReplaceItemInObject(machine, JSON_CREW_BINDINGS, jsonObject);
                }
            }
        }
    }

    return ret;
}

/**
 * Process the machine information and emitted messages from the stepped.
 *
 * @param stepJson The crew machine processed "step".
 * @param consumed Pointer to hold the number of consumed messages by the machine.
 * Defaulted to zero.
 * @param emitted Pointer to a JSON object to hold the JSON array/object emitted from
 * the crew machine. Defaulted to NULL.
 */
static void handle_step_info(cJSON* stepJson, uint32_t* consumed, cJSON** emitted)
{
    *consumed = 0;
    *emitted = NULL;

    if (stepJson) {
        if (cJSON_IsTrue(cJSON_GetObjectItem(stepJson, CONSUMED_PROP))) {
            *consumed = 1;
        }

        *emitted = cJSON_DetachItemFromObject(stepJson, JSON_EMITTED);
        if (*emitted &&
            cJSON_IsArray(*emitted) &&
            (cJSON_GetArraySize(*emitted) == 0)) {
            /* Only emit actual JSON arrays that contain information.
             * We must delete the JSON object as it was detached from
             * the "step" JSON object already.
             */
            cJSON_Delete(*emitted);
            *emitted = NULL;
        }
    }
}

/** Decoding from Sheens so make sure
 * that the schema being verified is JSON and
 * has the required version field "sheensVersion".
 *
 * @param specification String containing some specification definition.
 * @return True if this transcoder can parse this schema, otherwise false.
 */
static bool sheens_is_valid(const char* specification)
{
    cJSON* root;
    bool valid;

    if (unlikely(specification == NULL)) return false;
    if (unlikely(specification[0] == '\0')) return false;

    root = cJSON_Parse(specification);
    valid = ((root != NULL) &&
             cJSON_IsObject(root) &&
             (cJSON_GetObjectItem(root, sheens_version_key) != NULL));

    cJSON_Delete(root);

    return valid;
}

static void sheens_destroy(void)
{
    mach_close();

    /* littlesheens v0.1.0 does not provide a "free context"
     * routine.
     */
    free(sheensContext);
    free(state);
    free(steppeds);

    cJSON_Delete(crew);

    hashMapDestroy(specMap, freeSpecMap);
}

static bool sheens_enable(const char* id, const char* specification)
{
    cJSON* json;
    bool ret = false;

    if (unlikely(id == NULL) || unlikely(id[0] == '\0')) {
        icLogError(LOG_TAG, "Invalid specification ID supplied.");
        return false;
    }

    if (unlikely(specification == NULL) || unlikely(specification[0] =='\0')) {
        icLogError(LOG_TAG, "Invalid specification specified.");
        return false;
    }

    if (sheens_is_valid(specification)) {
        const char* oldSpec;

        json = cJSON_CreateObject();
        cJSON_AddItemToObjectCS(json, JSON_CREW_ID, cJSON_CreateString(id));
        cJSON_AddItemToObjectCS(json, JSON_CREW_NODE, cJSON_CreateStringReference(START_NODE));
        cJSON_AddItemToObjectCS(json, JSON_CREW_BINDINGS, cJSON_CreateObject());

        pthread_mutex_lock(&mutex);
        oldSpec = hashMapGet(specMap, (void*) id, HASHMAP_KEYLEN(id));

        if (oldSpec) {
            if (strcmp(oldSpec, specification) == 0) {
                cJSON_Delete(json);
            } else {
                cJSON_ReplaceItemInObject(crewMachines, id, json);

                /* Make sure an entry does not exist before replacing. */
                hashMapDelete(specMap, (void*) id, HASHMAP_KEYLEN(id), freeSpecMap);
                hashMapPut(specMap, strdup(id), HASHMAP_KEYLEN(id), (void*) specification);

                needStateUpdate = true;
            }
        } else {
            cJSON_AddItemToObject(crewMachines, id, json);
            hashMapPut(specMap, strdup(id), HASHMAP_KEYLEN(id), (void*) specification);

            needStateUpdate = true;
        }
        pthread_mutex_unlock(&mutex);

        ret = true;
    }

    return ret;
}

static void sheens_disable(const char* id)
{
    pthread_mutex_lock(&mutex);
    if (hashMapDelete(specMap, (void*) id, HASHMAP_KEYLEN(id), freeSpecMap)) {
        cJSON_DeleteItemFromObject(crewMachines, id);
        needStateUpdate = true;
    }
    pthread_mutex_unlock(&mutex);
}

static cJSON* sheens_get_state(const char* id)
{
    cJSON* ret = NULL;

    pthread_mutex_lock(&mutex);
    ret = cJSON_GetObjectItem(crewMachines, id);

    if (ret) {
        ret = cJSON_Duplicate(ret, true);
    }
    pthread_mutex_unlock(&mutex);

    return ret;
}

static cJSON* sheens_process(const cJSON* message, cJSON* stats)
{
    cJSON* response;
    cJSON* actionArray = cJSON_CreateArray();

    char* msg;

    updateCrewState();

    msg = cJSON_PrintBuffered(message, 512, false);

    /* Attempt to process the message with the littlesheens crew.
     * If the "steppeds" buffer is not large enough then attempt to reallocate.
     *
     * Ok, so "steppeds" is not an intuitive name. It is, however, the name
     * used within littlesheens so we stick with it. The 'steppeds' represents
     * an array (or error condition) of machine "updated" objects. We will
     * need to process these machine updates later to reflect the current
     * crew state, form stats on the number of consumed/emitted messages, and
     * to process the actual emitted messages from the machine.
     */
    while (mach_crew_process(state, (JSON) msg, steppeds, stepped_size) == MACH_TOO_BIG) {
        icLogWarn(LOG_TAG,
                  "steppeds size too small (%lu), increasing to %lu and trying again.",
                  stepped_size,
                  stepped_size + STEPPEDS_BUFFER_STEP_SIZE);

        stepped_size += STEPPEDS_BUFFER_STEP_SIZE;
        steppeds = realloc(steppeds, stepped_size);
    }

    dbg(VERBOSITY_LEVEL_0, "state was %s, consuming message %s yielded steppeds %s", state, msg, steppeds);

    /* We no longer need the message, and we own it. */
    free(msg);

    response = cJSON_Parse(steppeds);
    if (response) {
        /* The crew didn't like something we passed to it. */
        if (cJSON_HasObjectItem(response, "err")) {
            cJSON* err = cJSON_GetObjectItem(response, "err");

            icLogError(LOG_TAG,
                       "Error running crew. [%s]",
                       (err->valuestring == NULL) ? "null" : err->valuestring);
        } else {
            cJSON* step;

            /* Walk through each machine update (step) in order to
             * update the crew machine state and grab the emitted
             * messages.
             */
            cJSON_ArrayForEach(step, response) {
                if (step->string) {
                    cJSON* json;

                    dbg(VERBOSITY_LEVEL_1, "Machine Step: [%s]", step->string);

                    pthread_mutex_lock(&mutex);
                    json = cJSON_GetObjectItem(crewMachines, step->string);
                    if (json) {
                        /* We need to process the machine with the mutex held
                         * so that we can update the crew state.
                         *
                         * We bitwise-OR the updated flag here so that we
                         * can aggregate all crew updates into one flag, and
                         * then update the crew state string later.
                         */
                        needStateUpdate |= handle_step_crew(json, step);
                    }
                    pthread_mutex_unlock(&mutex);

                    json = cJSON_DetachItemFromObject(step, JSON_EMITTED);
                    if (json) {
                        if (cJSON_IsArray(json) &&
                            (cJSON_GetArraySize(json) == 0)) {
                            /* Only emit actual JSON arrays that contain information.
                             * We must delete the JSON object as it was detached from
                             * the "step" JSON object already.
                             */
                            cJSON_Delete(json);
                        } else {
                            char *action = cJSON_PrintUnformatted(json);
                            dbg(VERBOSITY_LEVEL_1, "Post Action Step: [%s] %s", step->string, action);
                            free(action);
                            automationActionPost(step->string, json);
                        }
                    }
                }
            }
        }

        cJSON_Delete(response);
    } else {
        icLogError(LOG_TAG, "Error parsing steppeds response.");
    }

    return actionArray;
}

static automationEngineOps ops = {
    "sheens",
    sheens_destroy,
    sheens_enable,
    sheens_disable,
    sheens_get_state,
    sheens_process
};

void sheensEngineInit(void)
{
    /* littlesheens requires us to allocate the Context
 * before using any call to littelsheens.
 *
 * Unfortunately the context is _not_ passed to each
 * littesheens routine, but instead littlesheens expects
 * the user to either (a) never change the context or (b) perform a
 * 'set' before each routine call. We have chosen (a) as it
 * is the safest option as of now.
 */
    pthread_mutex_lock(&mutex);
    sheensContext = mach_make_ctx();
    mach_set_ctx(sheensContext);

    if (sheensContext && (mach_open() == MACH_OKAY)) {
        crew = cJSON_CreateObject();
        crewMachines = cJSON_CreateObject();

        cJSON_AddItemToObjectCS(crew, ID_PROP, cJSON_CreateStringReference(AUTOMATIONS_PROP));
        cJSON_AddItemToObjectCS(crew, MACHINES_PROP, crewMachines);

        specMap = hashMapCreate();
        steppeds = malloc(stepped_size);
        state = malloc(state_size);

        mach_set_spec_provider(NULL, spec_provider, 0);

        needStateUpdate = true;

        automationEngineRegister(&ops);
    } else {
        icLogError(LOG_TAG, "Failed to initialize littlesheens.");
        free(sheensContext);
    }
    pthread_mutex_unlock(&mutex);
}
