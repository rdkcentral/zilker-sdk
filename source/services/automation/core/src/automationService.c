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
// Created by tlea on 3/29/18.
//

#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <inttypes.h>

#include <icIpc/baseEvent.h>
#include <icIpc/eventConsumer.h>
#include <icIpc/ipcStockMessagesPojo.h>
#include <icLog/logging.h>
#include <icConcurrent/icBlockingQueue.h>
#include <icConfig/storage.h>
#include <icConcurrent/threadPool.h>
#include <cslt/sheens.h>
#include <cslt/cslt.h>
#include <jsonHelper/jsonHelper.h>
#include <littlesheens/machines.h>
#include <propsMgr/paths.h>
#include <icUtil/array.h>
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>
#include <dirent.h>

#include "automationService.h"
#include "automationTimerTick.h"
#include "automationSunTime.h"
#include "automationEngine.h"
#include "automationAction.h"
#include "automationBroadcastEvent.h"
#include "automationServiceTranscoder.h"

#define JSON_EMITTED "emitted"

/* The Crew information maps directly into
 * what littlesheens expects the crew format to
 * be. The internal names are a little more
 * name friendly so that they reflect better what
 * the data actually is.
 */
#define JSON_CREW_ID "spec"
#define JSON_CREW_NODE "node"
#define JSON_CREW_BINDINGS "bs"

#define JSON_INFO_ENABLED "enabled"
#define JSON_INFO_SPEC "spec"
#define JSON_INFO_CREATED "dateCreated"
#define JSON_INFO_CONSUMED "consumedCount"
#define JSON_INFO_EMITTED "emittedCount"
#define JSON_INFO_ORIG_SPEC "origSpec"
#define JSON_INFO_TRANSCODER_VERSION "transcoderVersion"

#define TO_PROP "to"
#define CONSUMED_PROP "consumed"
#define ID_PROP "id"
#define MACHINES_PROP "machines"
#define AUTOMATIONS_PROP "automations"
#define START_NODE "start"

#define STOCK_RULES_URI "/stock/rules"

#define MY_STORAGE_NAMESPACE "automationService"

static pthread_mutex_t automationMtx = PTHREAD_MUTEX_INITIALIZER;

static cJSON* machineInfo;

static bool saveMachine(cJSON* machine);
static bool automationServiceAddMachineInternal(const char *id, const char *specification, const char *originalSpecification,
                                                int transcoderVersion, bool enabled);

// Order of directories to look for the default rules in. For any new spec type, add an entry here for default rule
// lookup/translation
static char *defaultRulesLookupOrder[] = {
            "legacy",
            "sheens"
        };

/**
 * A handler to attempt to add the contents of file dname as an automation spec.
 * @param pathname The path to the parent directory we are iterating on
 * @param dname The name of the file in pathname that we want to try to add as a spec
 * @param dtype The type of the file
 * @param private A boolean to determine if we have loaded at least one rule
 * @return 0
 */
static int loadSpecHandler(const char* pathname, const char* dname, unsigned char dtype, void* private)
{
    bool *addedRule = (bool *) private;

    if (dtype != DT_DIR)
    {
        AUTO_CLEAN(free_generic__auto) char *fullFilePath = stringBuilder("%s/%s", pathname, dname);
        AUTO_CLEAN(free_generic__auto) char *spec = readFileContents(fullFilePath);

        if (spec != NULL)
        {
            //Just try to create a new one. If we already have them, this will fail. If we ever have to make a change
            //to default rules (unlikely), then add a SET_AUTOMATION request statement here.
            if (automationServiceAddMachine(dname, spec, true) == true)
            {
                *addedRule = true;
            }
        }
    }

    return 0;
}

/**
 * Attempts to load stock rules through IPC. Will check /vendor/etc/stock/rules/<specType> for each
 * specType in defaultRulesLookupOrder, stopping once a set of defaults is found and successfully added/updated.
 */
static void installStockRules(void)
{
    icLogDebug(LOG_TAG, "Trying to install stock rules");
    AUTO_CLEAN(free_generic__auto) char *staticDir = getStaticPath();
    bool gotStockRules = false;

    for (int i = 0; i < ARRAY_LENGTH(defaultRulesLookupOrder) && gotStockRules == false; i++)
    {
        AUTO_CLEAN(free_generic__auto) char *dirToSearch = stringBuilder("%s%s/%s", staticDir, STOCK_RULES_URI,
                                                                         defaultRulesLookupOrder[i]);
        icLogDebug(LOG_TAG, "Looking for stock rules in %s", dirToSearch);

        //See if this directory exists
        if (doesDirExist(dirToSearch) == true)
        {
            //Try to add all the rules inside
            listDirectory(dirToSearch, loadSpecHandler, &gotStockRules);
        }
    }

    if (gotStockRules == true)
    {
        icLogDebug(LOG_TAG, "Installed stock rules into storage namespace");
    }
}

/**
 * Load a single machine specification and current stats into
 * the JSON machine container.
 *
 * @param machineId The ID of the machine to load.
 */
static void loadMachine(const char* machineId)
{
    bool result = false;

    char* machineStr = NULL;

    if (storageLoad(MY_STORAGE_NAMESPACE, machineId, &machineStr) && machineStr) {
        cJSON* machine = cJSON_Parse(machineStr);

        bool resaveMachine = false;
        if (machine) {
            // Check if we need to re-transcode
            cJSON* origSpecJson = cJSON_GetObjectItem(machine, JSON_INFO_ORIG_SPEC);
            cJSON* transcoderVersionJson = cJSON_GetObjectItem(machine, JSON_INFO_TRANSCODER_VERSION);
            if (origSpecJson != NULL) {
                int transcoderVersion = -1;
                if (transcoderVersionJson != NULL) {
                    transcoderVersion = transcoderVersionJson->valueint;
                }
                icLogDebug(LOG_TAG, "%s: transcoder version %d", machineId, transcoderVersion);
                const char *origSpec = origSpecJson->valuestring;
                char* transcodedSpec = NULL;
                const cslt_transcoder_t *transcoder = automationServiceGetTranscoder(origSpec);
                if (transcoder != NULL && transcoder->transcoder_version != transcoderVersion) {
                    icLogDebug(LOG_TAG, "Re-transcoding out of date machine %s", machineId);
                    int cslt_ret = cslt_transcode(transcoder, origSpec, &transcodedSpec);
                    if (cslt_ret < 0) {
                        icLogError(LOG_TAG, "Unable to transcode specification. [%s]", strerror(errno));
                    } else {
                        cJSON *newSpecJson = cJSON_CreateString(transcodedSpec);
                        cJSON *newTranscoderVersion = cJSON_CreateNumber(transcoder->transcoder_version);
                        // Update with new values
                        cJSON_ReplaceItemInObject(machine, JSON_INFO_SPEC, newSpecJson);
                        if (transcoderVersionJson != NULL) {
                            cJSON_ReplaceItemInObject(machine, JSON_INFO_TRANSCODER_VERSION, newTranscoderVersion);
                        } else {
                            cJSON_AddItemToObjectCS(machine, JSON_INFO_TRANSCODER_VERSION, newTranscoderVersion);
                        }
                        // Save after we have set the machineId onto the machine JSON since saveMachine requires that
                        resaveMachine = true;

                        // Cleanup
                        free(transcodedSpec);
                    }
                }
            }


            cJSON_AddItemToObject(machineInfo, machineId, machine);
            // We did a retranscode, so we should re-save the machine
            if (resaveMachine == true) {
                saveMachine(machine);
            }

            /* Only add the crew information if the machine
             * is actively enabled.
             */
            if (cJSON_IsTrue(cJSON_GetObjectItem(machine, JSON_INFO_ENABLED))) {
                cJSON* jsonSpec = cJSON_GetObjectItem(machine, JSON_INFO_SPEC);

                if (jsonSpec) {
                    automationEngineEnable(machineId, jsonSpec->valuestring);
                }
            }

            result = true;
        }

        free(machineStr);
    }

    if (!result) {
        icLogError(LOG_TAG, "Failed to load machine [%s]", machineId);
    }
}

/**
 * Load all machines from storage into the JSON machine container.
 *
 * This is only run at initialization (phase 1) time. Thus no need for
 * mutex locking here as nothing should be able to interact with the
 * system.
 */
static void loadMachines(void)
{
    dbg(VERBOSITY_LEVEL_0, "");

    machineInfo = cJSON_CreateObject();

    // load the JSON machine definitions
    icLinkedList* machineIds = storageGetKeys(MY_STORAGE_NAMESPACE);
    if (machineIds) {
        icLinkedListIterator* it = linkedListIteratorCreate(machineIds);

        while (linkedListIteratorHasNext(it)) {
            loadMachine(linkedListIteratorGetNext(it));
        }

        linkedListIteratorDestroy(it);
        linkedListDestroy(machineIds, NULL);
    }
}

/**
 * Save the machine specification and stats to persistent storage.
 *
 * Warning: The mutex must be held during this call.
 *
 * @param machine The machine JSON object to store.
 * @return True if the machine was persisted to storage.
 */
static bool saveMachine(cJSON* machine)
{
    bool ret = false;
    char* storedData;

    dbg(VERBOSITY_LEVEL_2, "machine %s", machine->string);

    storedData = cJSON_PrintBuffered(machine, 1048, true);
    if (storedData) {
        ret = storageSave(MY_STORAGE_NAMESPACE, machine->string, storedData);
        free(storedData);
    }

    return ret;
}

/**
 * One minute timer tick event listener.
 * This will produce a new message into the crew.
 */
static void minuteTimerTickHandler(void)
{
    cJSON* json = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(json, EVENT_CODE_JSON_KEY, cJSON_CreateNumber(EVENT_CODE_TIMER_TICK));
    dbg(VERBOSITY_LEVEL_1, "Injecting tick event");

    automationEnginePost(json);
    cJSON_Delete(json);
}

/**
 * Listen for <em>all</em> events sent by the entire system!
 *
 * @param eventCode The event code for this particular event.
 * @param eventValue shrug, no idea
 * @param jsonPayload The JSON objec that contains all information about the event.
 */
static void allSystemEventsListener(int32_t eventCode, int32_t eventValue, cJSON* jsonPayload)
{
    dbg(VERBOSITY_LEVEL_1, "Process code=%d, value=%d", eventCode, eventValue);
    automationEnginePost(jsonPayload);
}

static bool automationServiceAddMachineInternal(const char *id, const char *specification, const char *originalSpecification,
                                                int transcoderVersion, bool enabled)
{
    bool result = false;

    if ((id == NULL) || (strlen(id) == 0))
    {
        icLogError(LOG_TAG, "%s: invalid args", __FUNCTION__);
        return false;
    }

    if ((specification == NULL) || (strlen(specification) == 0))
    {
        icLogError(LOG_TAG, "%s: invalid args", __FUNCTION__);
        return false;
    }

    /* Attempt to add the machine if it does not already exist. */
    pthread_mutex_lock(&automationMtx);
    if (!cJSON_HasObjectItem(machineInfo, id)) {
        cJSON *item, *jsonSpec;

        dbg(VERBOSITY_LEVEL_2, "id=%s, spec=%s", id, specification);

        jsonSpec = cJSON_CreateString(specification);

        // Setup info entry
        item = cJSON_CreateObject();
        cJSON_AddItemToObjectCS(item, JSON_INFO_ENABLED, cJSON_CreateBool(enabled));
        cJSON_AddItemToObjectCS(item, JSON_INFO_SPEC, jsonSpec);
        cJSON_AddItemToObjectCS(item, JSON_INFO_CREATED, cJSON_CreateNumber(time(NULL)));
        cJSON_AddItemToObjectCS(item, JSON_INFO_CONSUMED, cJSON_CreateNumber(0));
        cJSON_AddItemToObjectCS(item, JSON_INFO_EMITTED, cJSON_CreateNumber(0));
        if (originalSpecification != NULL) {
            cJSON_AddItemToObjectCS(item, JSON_INFO_ORIG_SPEC, cJSON_CreateString(originalSpecification));
        }
        cJSON_AddItemToObjectCS(item, JSON_INFO_TRANSCODER_VERSION, cJSON_CreateNumber(transcoderVersion));
        cJSON_AddItemToObject(machineInfo, id, item);

        saveMachine(item);

        if (enabled) {
            result = automationEngineEnable(id, jsonSpec->valuestring);
        } else {
            automationEngineDisable(id);
            result = true;
        }

    } else {
        icLogError(LOG_TAG, "%s: machine %s already exists", __FUNCTION__, id);
    }
    pthread_mutex_unlock(&automationMtx);

    return result;
}

bool automationServiceInitPhase1(void)
{
    dbg(VERBOSITY_LEVEL_0, "");

    automationTranscoderInit();

    /* Only loading those items that need to be available before
     * IPC is enabled.
     */
    automationEngineInit();
    automationActionInit();

    startAutomationEventProducer();

    return true;
}

void automationServiceInitPhase2(void)
{
    dbg(VERBOSITY_LEVEL_0, "");

    // Load in our machines
    loadMachines();

    //Add our default rules
    installStockRules();

    /* Start up everything! Woooohooooo! */
    automationStartSunMonitor(60); // 60 minutes of randomness
    automationStartTimerTick(60, minuteTimerTickHandler); // 1-minute timer tick
    automationEngineStart();

    /* Finally startup the event listener. */
    bool readyForEvents = startEventListener(EVENTCONSUMER_SUBSCRIBE_ALL, allSystemEventsListener);
    assert(readyForEvents);
}

void automationServiceCleanup(void)
{
    dbg(VERBOSITY_LEVEL_0, "");

    stopAutomationEventProducer();

    // Holding the lock while we unregister can cause a deadlock if we are currently handling an event
    stopEventListener(EVENTCONSUMER_SUBSCRIBE_ALL);

    pthread_mutex_lock(&automationMtx);
    automationStopTimerTick();
    automationStopSunMonitor();

    automationEngineStop();

    automationActionDestroy();
    automationEngineDestroy();

    cJSON_Delete(machineInfo);
    pthread_mutex_unlock(&automationMtx);
}

void automationServiceRestore(const configRestoredInput* input)
{
    if (input && input->tempRestoreDir && (input->tempRestoreDir[0] != '\0')) {
        dbg(VERBOSITY_LEVEL_0, "");

        automationServiceCleanup();

        pthread_mutex_lock(&automationMtx);
        /*
         * Now destroy current storage (if any) and restore
         * the automations from a previous storage.
         */
        storageDeleteNamespace(MY_STORAGE_NAMESPACE);
        bool worked = storageRestoreNamespace(MY_STORAGE_NAMESPACE, input->tempRestoreDir);
        // if the above failed, at least log a warning.  note that it does not fail if there
        // was nothing to restore.
        if (worked == false)
        {
            icLogWarn(LOG_TAG,"Failed to restore configuration");
        }

        pthread_mutex_unlock(&automationMtx);

        automationServiceInitPhase1();
        automationServiceInitPhase2();
    }
}

bool automationServiceAddMachine(const char *id, char *specification, bool enabled)
{
    const cslt_transcoder_t* transcoder;
    char* transcodedSpec = NULL;
    int cslt_ret;
    bool success = false;

    transcoder = automationServiceGetTranscoder(specification);
    cslt_ret = cslt_transcode(transcoder, specification, &transcodedSpec);
    if (cslt_ret < 0) {
        icLogError(LOG_TAG, "Unable to transcode specification. [%s]", strerror(errno));
    } else {
        char *originalSpecification = NULL;
        if (transcodedSpec != specification) {
            originalSpecification = specification;
        }
        success = automationServiceAddMachineInternal(id, transcodedSpec, originalSpecification,
                                                      transcoder->transcoder_version, enabled);

        if (transcodedSpec != specification) {
            free(transcodedSpec);
        }
    }

    return success;
}

bool automationServiceRemoveMachine(const char* machineId)
{
    dbg(VERBOSITY_LEVEL_2, "id=%s", machineId);

    bool result = false;

    pthread_mutex_lock(&automationMtx);
    if (cJSON_HasObjectItem(machineInfo, machineId)) {
        cJSON_DeleteItemFromObject(machineInfo, machineId);

        result = storageDelete(MY_STORAGE_NAMESPACE, machineId);

        automationEngineDisable(machineId);
    }
    pthread_mutex_unlock(&automationMtx);

    return result;
}

bool automationServiceSetMachineEnabled(const char* machineId, bool enabled)
{
    bool ret = false;
    cJSON* json;

    pthread_mutex_lock(&automationMtx);
    json = cJSON_GetObjectItem(machineInfo, machineId);
    if (json) {
        cJSON* enabledJson = cJSON_GetObjectItem(json, JSON_INFO_ENABLED);
        cJSON* specJson = cJSON_GetObjectItem(json, JSON_INFO_SPEC);

        if (enabledJson && specJson) {
            if (((bool) cJSON_IsTrue(enabledJson)) != enabled) {
                setCJSONBool(enabledJson, enabled);
                saveMachine(json);

                if (enabled) {
                    automationEngineEnable(machineId, specJson->valuestring);
                } else {
                    automationEngineDisable(machineId);
                }
            }

            ret = true;
        }
    }
    pthread_mutex_unlock(&automationMtx);

    return ret;
}

void automationServiceSetMachineSpecification(const char *machineId, const char *specification,
                                              const char *originalSpecification, int transcoderVersion)
{
    if (machineId && specification && (strlen(specification) > 0)) {
        cJSON* json;

        dbg(VERBOSITY_LEVEL_2, "id=%s, spec=%s", machineId, specification);

        pthread_mutex_lock(&automationMtx);
        json = cJSON_GetObjectItem(machineInfo, machineId);
        if (json) {
            cJSON* enabledJson = cJSON_GetObjectItem(json, JSON_INFO_ENABLED);

            cJSON *json_info_spec = cJSON_CreateString(specification);

            bool enabled = enabledJson && cJSON_IsTrue(enabledJson);
            if (enabled) {
                // Make sure it gets cleaned up before we do the replace, since the replace
                // will free the old spec
                automationEngineDisable(machineId);
            }

            // Update the spec
            cJSON_ReplaceItemInObject(json,
                                      JSON_INFO_SPEC,
                                      json_info_spec);

            // Do the adds/updates to orig spec and transcoder version
            if (originalSpecification != NULL) {
                cJSON *origSpecJson = cJSON_GetObjectItem(json, JSON_INFO_ORIG_SPEC);
                if (origSpecJson != NULL) {
                    cJSON_ReplaceItemInObject(json,
                                              JSON_INFO_ORIG_SPEC,
                                              cJSON_CreateString(originalSpecification));
                } else {
                    cJSON_AddItemToObjectCS(json,
                                            JSON_INFO_ORIG_SPEC,
                                            cJSON_CreateString(originalSpecification));
                }
            }

            cJSON *origTranscoderVersion = cJSON_GetObjectItem(json, JSON_INFO_TRANSCODER_VERSION);
            if (origTranscoderVersion != NULL) {
                cJSON_ReplaceItemInObject(json,
                                          JSON_INFO_TRANSCODER_VERSION,
                                          cJSON_CreateNumber(transcoderVersion));
            } else {
                cJSON_AddItemToObjectCS(json,
                                        JSON_INFO_TRANSCODER_VERSION,
                                        cJSON_CreateNumber(transcoderVersion));
            }

            saveMachine(json);

            if (enabled) {
                /* We may need to "reload" the specification if it has
                 * changed.
                 */
                automationEngineEnable(machineId,json_info_spec->valuestring);
            }
        }
        pthread_mutex_unlock(&automationMtx);
    }
}

bool getMachineState(const char* machineId, char** state)
{
    dbg(VERBOSITY_LEVEL_0, "id=%s", machineId);

    bool result = false;
    cJSON* json;

    json = automationEngineGetState(machineId);
    if (json) {
        cJSON* _tmp_json = cJSON_GetObjectItem(json, JSON_CREW_NODE);

        if (_tmp_json && _tmp_json->valuestring) {
            *state = strdup(_tmp_json->valuestring);
            result = true;
        }

        cJSON_Delete(json);
    }

    return result;
}

icLinkedList* automationServiceGetMachineInfos()
{
    dbg(VERBOSITY_LEVEL_0, "");

    cJSON* entry;
    icLinkedList* result = linkedListCreate();

    pthread_mutex_lock(&automationMtx);
    cJSON_ArrayForEach(entry, machineInfo) {
        MachineInfo* info;
        cJSON* json;

        info = (MachineInfo*) calloc(1, sizeof(MachineInfo));
        if (info) {
            info->id = strdup(entry->string);

            json = cJSON_GetObjectItem(entry, JSON_INFO_ENABLED);
            if (json) {
                info->enabled = (bool) cJSON_IsTrue(json);
            }

            json = cJSON_GetObjectItem(entry, JSON_INFO_CREATED);
            if (json) {
                info->dateCreatedSecs = (uint32_t) json->valueint;
            }

            json = cJSON_GetObjectItem(entry, JSON_INFO_CONSUMED);
            if (json) {
                info->messagesConsumed = (uint32_t) json->valueint;
            }

            json = cJSON_GetObjectItem(entry, JSON_INFO_EMITTED);
            if (json) {
                info->messagesEmitted = (uint32_t) json->valueint;
            }

            linkedListAppend(result, info);
        }
    }
    pthread_mutex_unlock(&automationMtx);

    return result;
}

void machineInfoFreeFunc(void* item)
{
    MachineInfo* info = (MachineInfo*) item;
    free(info->id);
    free(info);
}
