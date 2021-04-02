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
#include <libxml2/libxml/tree.h>
#include <uuid/uuid.h>

#include <cslt/cslt.h>
#include <cslt/sheens.h>
#include <cslt/icrules.h>

#include <errno.h>

#include <icrule/icrule.h>
#include <inttypes.h>
#include <deviceService/deviceService_event.h>
#include <commonDeviceDefs.h>
#include <automationService/automationService_event.h>
#include <icUtil/stringUtils.h>

#include "../cslt_internal.h"
#include "sheens_transcoders.h"
#include "../passthru_transcoder.h"
#include "sheens_javascript.h"
#include "sheens_json.h"

// Update this whenever a change is made in how icRules are transcoded to sheens
#define IC_RULE_TO_SHEENS_TRANSCODER_VERSION 20

#define SHEENS_VERSION_KEY "sheensVersion"

#define TRIGGER_LIST_NODE "triggerList"
#define ACTION_NODE "action"
#define CONSTRAINTS_NODE "constraints"
#define SCHEDULE_ENTRY_NODE "scheduleEntry"
#define DESCRIPTION_NODE "description"

#define TRIGGER_SENSOR_NODE "sensorTrigger"
#define TRIGGER_TOUCHSCREEN_NODE "touchscreenTrigger"
#define TRIGGER_PANIC_NODE "panicTrigger"
#define TRIGGER_NETWORK_NODE "networkTrigger"
#define TRIGGER_LIGHTING_NODE "lightingTrigger"
#define TRIGGER_DOORLOCK_NODE "doorLockTrigger"
#define TRIGGER_TSTAT_THRESHOLD_NODE "thermostatThresholdTrigger"
#define TRIGGER_TSTAT_NODE "thermostatTrigger"
#define TRIGGER_TIME_NODE "timeTrigger"
#define TRIGGER_ZIGBEE_COMMSTATUS_NODE "zigbeeCommStatusTrigger"
#define TRIGGER_SWITCH_NODE "switchTrigger"
#define TRIGGER_RESOURCE_NODE "resourceTrigger"
#define TRIGGER_CLOUD_SERVICE_NODE "cloudServiceTrigger"
#define TRIGGER_CLOUD_NODE "cloudTrigger"

#define ELEMENT_CATEGORY "category"
#define ELEMENT_DESCRIPTION "description"

#define ELEMENT_SENSOR_STATE "sensorState"
#define ELEMENT_SENSOR_ID    "sensorID"
#define ELEMENT_SENSOR_TYPE  "sensorType"

#define ELEMENT_CONSTRAINT_AND "and-expression"
#define ELEMENT_CONSTRAINT_OR "or-expression"
#define ELEMENT_CONSTRAINT_TIME "timeConstraint"
#define ELEMENT_CONSTRAINT_SYSTEM "systemConstraint"

static DeviceIdMapperFunc deviceIdMapper = NULL;

/** Decoding from iControl Legacy rules so make sure
 * that the schema being verified is XML and
 * has the namespace of "rules/v1".
 *
 * @param schema String containing some specification definition.
 * @return True if this transcoder can parse this schema, otherwise false.
 */
static bool icrule2sheens_is_valid(const char* schema)
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
             * It DOES NOT create a clone of it. Thus you DO NOT
             * need to free it!
             */
            xmlNsPtr ns2 = xmlSearchNsByHref(doc, node, (const xmlChar*) "http://ucontrol.com/rules/v1.0");
            if (ns2 != NULL) {
                // found the reference to our XSD schema
                valid = true;
            }
            else {
                // some generated rules don't do it "correct", so check the top node to ensure
                // the node name is "rule" and that it has an attribute of "ruleID"
                if (node->name != NULL && strcmp(node->name, "rule") == 0) {
                    // get the attribute
                    char *attrib = (char *)xmlGetProp(node, BAD_CAST "ruleID");
                    if (attrib != NULL) {
                        // calling this valid since it has the node and attribute we need
                        xmlFree(attrib);
                        valid = true;
                    }
                }
            }
        }
    }

    xmlFreeDoc(doc);

    return valid;
}

/**
 * Add a pattern that jumps to the 'constraints' branch when it matches
 */
static cJSON* add_constraints_event_branch(cJSON* start_branches, const int eventId)
{
    cJSON* json = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(json,
                            sheens_event_code_key,
                            cJSON_CreateNumber(eventId));

    // Required by the scheduled actions.
    sheens_pattern_add_constraints_required(json);

    // We always branch to constraints as it is always universally valid.
    cJSON_AddItemToArray(start_branches,
                         sheens_create_branch(json, sheens_constraints_key, true));

    return json;
}

static void add_automation_id_match(cJSON* pattern, uint64_t ruleId)
{
    // ruleId in the event is a string, so have to match that
    char ruleIdStr[21]; // UINT64_MAX + null term
    snprintf(ruleIdStr, sizeof(ruleIdStr), "%"PRIu64, ruleId);
    cJSON* automationEventPattern = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(automationEventPattern, AUTOMATION_EVENT_RULE_ID, cJSON_CreateString(ruleIdStr));
    cJSON_AddItemToObjectCS(pattern, AUTOMATION_EVENT_NAME, automationEventPattern);
}

/** Transcode legacy schedule entries into new Sheens specifications.
 * The 'start' node pattern matching will be configured, and any
 * _actions_ will be produced for all configured thermostats.
 *
 * From 'start' we will either fail to match the incoming message, or
 * branch to 'constraints'.
 *
 * @param icrule Legacy iControl rule.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @param start_branches The 'start' branches array for message pattern
 * matching.
 * @return Zero on success. Otherwise -1 and errno will be filled in.
 */
static int transcode_schedules(icrule_t* icrule, cJSON* nodes_object, cJSON* start_branches)
{
    cJSON* json;

    // Create schedule timer tick pattern for start.
    add_constraints_event_branch(start_branches, TIMER_TICK_EVENT_CODE);

    //React to automation create/update immediately instead of on the next tick
    cJSON* automationPattern = add_constraints_event_branch(start_branches, AUTOMATION_CREATED_EVENT);
    add_automation_id_match(automationPattern, icrule->id);

    automationPattern = add_constraints_event_branch(start_branches, AUTOMATION_MODIFIED_EVENT);
    add_automation_id_match(automationPattern, icrule->id);

    // React to hold mode changes
    cJSON* holdPatternJSON = add_constraints_event_branch(start_branches, DEVICE_SERVICE_EVENT_RESOURCE_UPDATED);

    cJSON* holdResourcePattern = cJSON_CreateObject();
    cJSON* resourceContainer = cJSON_AddObjectToObject(holdResourcePattern, DEVICE_RESOURCE_UPDATED_EVENT_RESOURCE);
    cJSON* holdResource = cJSON_AddObjectToObject(resourceContainer, DS_RESOURCE);
    cJSON_AddItemToObjectCS(holdResource, DS_RESOURCE_ID, cJSON_CreateStringReference(THERMOSTAT_PROFILE_RESOURCE_HOLD_ON));
    cJSON_AddItemToObjectCS(holdResource, DS_RESOURCE_OWNER_CLASS, cJSON_CreateStringReference(THERMOSTAT_PROFILE));
    cJSON_AddItemToObjectCS(holdResource, DS_RESOURCE_VALUE, cJSON_CreateStringReference("?holdOn"));
    cJSON_AddItemToObjectCS(holdPatternJSON, DEVICE_RESOURCE_UPDATED_EVENT_NAME, holdResourcePattern);

    json = sheens_schedules2javascript(linkedListIteratorCreate(icrule->schedule_entries),
                                       nodes_object,
                                       start_branches);
    if (json == NULL) {
        return -1;
    }

    /* Add actions node to top-level nodes.
     * We only have the default branch over to reset so let the system
     * fill that in for us.
     */
    cJSON_AddItemToObjectCS(nodes_object,
                            sheens_actions_key,
                            sheens_create_state_node(json, NULL, false));

    return 0;
}

/**
 * Search within a legacy iControl Rule's constraints and
 * find the first occurence of a "time constraint".
 * The constraint will be removed from the list so that it is not
 * used later in the constraints itself.
 *
 * Note: This breaks the intent of the icrule library! The
 * library is a snapshot of the legacy rule so it does not
 * expose any routines to cleanup the internal data. It
 * is up to the user of the icrule data to cleanup any
 * data prematurely removed.
 *
 * @param constraints The list of all constraints
 * @return An instance of a constraint "time", or NULL if
 * no time constraint found.
 */
static icrule_constraint_time_t* find_and_remove_time_constraint(icLinkedList* constraints)
{
    icrule_constraint_time_t* ret = NULL;
    uint32_t i, size;

    size = linkedListCount(constraints);
    for (i = 0; (ret == NULL) && (i < size); i++) {
        icrule_constraint_t* constraint = linkedListGetElementAt(constraints, i);

        if (constraint) {
            if (linkedListCount(constraint->time_constraints) > 0) {
                ret = linkedListRemove(constraint->time_constraints, 0);

                if ((linkedListCount(constraint->time_constraints) == 0) &&
                    (linkedListCount(constraint->child_constraints) == 0)) {
                    /*
                     * Ok, this breaks the way that the icrule library is
                     * *supposed* to be used! The library is a snapshot
                     * of the original rule. We are causing the system
                     * to "lie" here by removing the constraint altogether.
                     *
                     * This is not pretty, but because we need this time
                     * constraint gone, and we no longer need this particular
                     * constraint we want it removed.
                     *
                     * We know that all of the lists are empty so we don't have
                     * to worry about destroying anything except the internal
                     * data of the list.
                     */
                    linkedListRemove(constraints, i);

                    linkedListDestroy(constraint->time_constraints, NULL);
                    linkedListDestroy(constraint->child_constraints, NULL);

                    free(constraint);
                }
            } else {
                ret = find_and_remove_time_constraint(constraint->child_constraints);
            }
        }
    }

    return ret;
}

/**
 * Negative "triggers" are really just whole new rules. So much so that
 * the constraints are actually the rule which drives the check to
 * see if the "trigger" <em>didn't</em> fire. Thus none of this
 * fits with what we currently have for what <em>should</em> be a
 * standardized "flow".
 *
 * The "constraints" in this scenario are not normal. They are in fact
 * the rule itself. Thus, the normal set of constraints are NOT actually
 * supported. Instead only a single "timeConstraint" will be allowed as
 * it is the core of the rule trigger window.
 *
 * The state machine will flow as follows:
 * Start:
 *   Wait for timer tick, and then determine if this matches the "start" time.
 *   If the start time has been reached then we jump to the "trigger window" node.
 * Trigger Window:
 *   Wait for both the timer tick and a sensor event.
 *   If the timer tick reaches the "end" time then jump to the "action" node.
 *   If the sensor event matches the trigger condition then jump to "reset" as we
 *   have been validated.
 * Action:
 *   Perform whatever actions have been assigned then jump to "reset" node.
 * Reset:
 *   Clear out any left over non-persistent data and jump to "start" node.
 *
 * @param icrule Legacy iControl rule.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @param start_branches The 'start' branches array for message pattern
 * matching.
 * @return 0 on success, otherwise -1.
 * EINVAL: There are no constraints thus we don't know what time period to
 * verify something "didn't" happen.
 */
static int transcode_negative_rule(const icrule_t* icrule, cJSON* nodes_object, cJSON* start_branches)
{
    cJSON *json;
    icrule_constraint_time_t* constraint_time;

    if (linkedListCount(icrule->triggers.triggers) == 0) {
        errno = EINVAL;
        return -1;
    }

    constraint_time = find_and_remove_time_constraint(icrule->constraints);
    if (constraint_time == NULL) {
        errno = EINVAL;
        return -1;
    }

    json = sheens_negative2javascript(&icrule->triggers,
                                      constraint_time,
                                      nodes_object);

    /*
     * We need to release the constraint time no matter what
     * so go ahead and do it before we test for NULL.
     */
    free(constraint_time);

    if (json == NULL) return -1;

    cJSON_AddItemToArray(start_branches, json);

    return 0;
}

/** Transcode all legacy triggers into new Sheens 'patterns'.
 * Each pattern will be added into the 'start' branches array.
 * If a trigger needs to peform any special verification then
 * it will need to create its own state node and have 'start'
 * branch to it.
 *
 * From triggers we will always move to either 'reset' or 'constraints'.
 *
 * @param icrule Legacy iControl rule.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @param start_branches The 'start' branches array for message pattern
 * matching.
 * @return Zero on success. Otherwise -1 and errno will be filled in.
 */
static int transcode_triggers(const icrule_t* icrule, cJSON* nodes_object, cJSON* start_branches)
{
    const icrule_trigger_list_t* triggers = &icrule->triggers;

    icLinkedListIterator* iterator;

    iterator = linkedListIteratorCreate(triggers->triggers);

    while (linkedListIteratorHasNext(iterator)) {
        icrule_trigger_t* trigger = linkedListIteratorGetNext(iterator);

        if (trigger) {
            cJSON* json = sheens_trigger2javascript(trigger,
                                                    sheens_constraints_key,
                                                    sheens_reset_value,
                                                    nodes_object,
                                                    start_branches);

            if (json == NULL) {
                linkedListIteratorDestroy(iterator);

                return -1;
            }

            cJSON_AddItemToArray(start_branches, json);
        }
    }

    linkedListIteratorDestroy(iterator);

    /* It is an error for the trigger list to have been zero.
     * Thus let the higher levels know to bail out.
     */
    return (cJSON_GetArraySize(start_branches) == 0) ? -1 : 0;
}

/** Transcode all constraints into a new Sheens state node.
 * If a message meets the conditions of the constraints then
 * we will transition to actions, otherwise we will 'reset'
 * the state machine and move back to 'start'.
 *
 * Note: Constraints require the following bindings to be in place.
 * ?evTime
 * ?_sunrise
 * ?_sunset
 * ?_systemStatus
 *
 * @param icrule Legacy iControl rule.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return Zero on success. Otherwise -1 and errno will be filled in.
 */
static int transcode_constraints(icrule_t* icrule, cJSON* nodes_object)
{
    if (linkedListCount(icrule->constraints) == 0) {
        /* No constraints are present. Thus we will branch straight over
         * to actions.
         */
        cJSON* branch_array = cJSON_CreateArray();

        // Create default branch over to actions since we do not have any constraints.
        cJSON_AddItemToArray(branch_array, sheens_create_branch(NULL, sheens_actions_key, true));
        cJSON_AddItemToObjectCS(nodes_object,
                                sheens_constraints_key,
                                sheens_create_state_node(NULL, branch_array, false));
    } else {
        char javascript[4096]; // TODO: Dangerous as we may blow the stack.

        cJSON *pattern_array_json, *source_json;

        javascript[0] = '\0';

        /* Recursively pass through all constraints building
         * the JavaScript that will determine if the
         * automation should be allowed to proceed or not.
         */
        // TODO: should return JSON
        if (sheens_constraints2javascript(linkedListIteratorCreate(icrule->constraints), javascript, NULL) < 0) {
            return -1;
        }

        pattern_array_json = cJSON_CreateArray();

        source_json = NULL;

        /* If we have JavaScript then we need the action
         * element so that Sheens knows what to do.
         *
         * This will force a pattern in the mix for
         * "allowed".
         */
        if (strlen(javascript) > 0) {
            cJSON* allowed_json;

            // TODO: remove this crap!
            source_json = cJSON_CreateString(javascript);

            allowed_json = cJSON_CreateObject();
            cJSON_AddItemToObjectCS(allowed_json, sheens_allowed_key, cJSON_CreateBool(true));

            cJSON_AddItemToArray(pattern_array_json,
                                 sheens_create_branch(allowed_json,
                                                      sheens_actions_key, true));
        }

        /* Always force a default target that sends us to reset
         * so that we clean out all bindings and branch back to
         * 'start'.
         *
         * Note: We don't know if we will have actual constraints or not
         * so we have to manually add in the default reset vector.
         */
        cJSON_AddItemToArray(pattern_array_json,
                             sheens_create_branch(NULL, sheens_reset_value, true));
        cJSON_AddItemToObjectCS(nodes_object,
                                sheens_constraints_key,
                                sheens_create_state_node(source_json, pattern_array_json, false));
    }

    return 0;
}

/** Transcode legacy actions into new Sheens states.
 *
 * @param icrule Legacy iControl rule.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @param start_branches The 'start' branches array for message pattern
 * matching.
 * @return Zero on success. Otherwise -1 and errno will be filled in.
 */
static int transcode_actions(icrule_t* icrule, cJSON* nodes_object, cJSON* start_branches)
{
    cJSON *source_json;

    source_json = sheens_actions2javascript(icrule->id,
                                            linkedListIteratorCreate(icrule->actions),
                                            nodes_object,
                                            start_branches);
    if (source_json == NULL) return -1;

    cJSON_AddItemToObjectCS(nodes_object,
                            sheens_actions_key,
                            sheens_create_state_node(source_json, NULL, false));

    return 0;
}

static int icrule2sheens_transcode(const char* src, char** dst, size_t size)
{
    icrule_t icrule;
    cJSON *nodes_json, *start_branches;

    int written_bytes = 0;

    if ((src == NULL) || (strlen(src) == 0)) {
        errno = EINVAL;
        return -1;
    }

    if ((dst == NULL) || (size == 0)) {
        errno = EINVAL;
        return -1;
    }

    if ((*dst == NULL) && (size != SHEEN_MSGSIZE)) {
        errno = EINVAL;
        return -1;
    }

    if (icrule_parse(src, &icrule) < 0) {
        errno = EBADMSG;
        return -1;
    }

    /* The 'Nodes' JSON represents all the "state" nodes
     * within the Sheens spec. Any trigger, schedule, actions, etc.
     * that needs to create a custom state will place that state here.
     * Our 'start', 'reset', and 'constraints' will also land here.
     *
     * Note: The nodes JSON will be passed to each building block withing
     * the transcoder.
     */
    nodes_json = cJSON_CreateObject();

    /* The "branches" represent all potential brances within our
     * 'start' node. These branches will have their patterns matched
     * against incoming messages.
     *
     * Note: The start branches array will be passed to each building
     * block within the transcoder.
     */
    start_branches = cJSON_CreateArray();

    /* Create our 'start' state with a type of 'message'.
     * Currently only the 'start' state will have this type. This
     * is because the 'message' type is meant for a state that will
     * handle external incoming messages, and is incompatible with
     * special actions.
     *
     * Note: Since 'start' is of type 'message' it does _not_ need
     * a default target. If no patterns are matched with the external
     * message then the state will not move.
     */
    cJSON_AddItemToObjectCS(nodes_json,
                            sheens_start_value,
                            sheens_create_state_node(NULL, start_branches, true));

    /* The legacy rules XSD states that we must have _either_ a sequence
     * of Schedule Entries, or a list of triggers. If there are triggers
     * then _Actions_ are required. The schedule entries and triggers are
     * mutually exclusive. If schedule entries are listed then _no_ actions
     * may be present.
     */
    if (linkedListCount(icrule.schedule_entries) > 0) {
        if (transcode_schedules(&icrule, nodes_json, start_branches) < 0) {
            cJSON_Delete(nodes_json);
            icrule_destroy(&icrule);

            errno = EBADMSG;
            return -1;
        }
    } else {
        if (icrule.triggers.negate) {
            if (transcode_negative_rule(&icrule, nodes_json, start_branches) < 0) {
                cJSON_Delete(nodes_json);
                icrule_destroy(&icrule);

                errno = EBADMSG;
                return -1;
            }
        } else {
            if (transcode_triggers(&icrule, nodes_json, start_branches) < 0) {
                cJSON_Delete(nodes_json);
                icrule_destroy(&icrule);

                errno = EBADMSG;
                return -1;
            }
        }

        if (transcode_actions(&icrule, nodes_json, start_branches) < 0) {
            cJSON_Delete(nodes_json);
            icrule_destroy(&icrule);

            errno = EBADMSG;
            return -1;
        }
    }

    if (transcode_constraints(&icrule, nodes_json) < 0) {
        cJSON_Delete(nodes_json);
        icrule_destroy(&icrule);

        errno = EBADMSG;
        return -1;
    }

    /** Create a default 'reset' node that cleans
     * up the bindings, and preserves any 'persist' data.
     */
    cJSON_AddItemToObjectCS(nodes_json,
                            sheens_reset_value,
                            sheens_create_reset_node(sheens_start_value));

    if (nodes_json->child == NULL) {
        /* Truthfully we should _never_ hit this state. If we did
         * something really strange has happened. This is because
         * we _always_ create a start and reset node. However, we
         * just want to make sure all bases are covered.
         */
        cJSON_Delete(nodes_json);
        icrule_destroy(&icrule);

        errno = EBADMSG;
        return -1;
    } else {
        cJSON *root_json;
        cJSON_bool success;

        char id_str[64]; // Space for 64-bit unsigned value

        sprintf(id_str, "%" PRIu64, icrule.id);

        root_json = cJSON_CreateObject();

        cJSON_AddItemToObjectCS(root_json, sheens_version_key, cJSON_CreateNumber(1));
        cJSON_AddItemToObjectCS(root_json, "name", cJSON_CreateString(id_str));
        cJSON_AddItemToObjectCS(root_json, "nodes", nodes_json);

        /* If the user did _not_ provide a buffer then we should
         * let cJSON create one for us. We take a guess at the
         * initial size so that we lessen our chance at a 'realloc'
         * occurring.
         *
         * If the user did provide a buffer then attempt to fill it
         * with the new spec. If it does not fit then bail out.
         */
        if (*dst == NULL) {
            *dst = cJSON_PrintBuffered(root_json, 1024 * 8, false);
            success = (*dst != NULL);
        } else {
            success = cJSON_PrintPreallocated(root_json, *dst, (const int) size, false);
        }

        cJSON_Delete(root_json);
        icrule_destroy(&icrule);

        if (success) {
            written_bytes = (int) (strlen(*dst) + 1);
        } else {
            errno = E2BIG;
            return -1;
        }
    }

    return written_bytes;
}

/** Decoding from Sheens so make sure
 * that the schema being verified is JSON and
 * has the required version field "sheensVersion".
 *
 * @param schema String containing some specification definition.
 * @return True if this transcoder can parse this schema, otherwise false.
 */
static bool sheens2sheens_is_valid(const char* schema)
{
    cJSON* root;
    bool valid = false;

    if (schema == NULL) return false;
    if (strlen(schema) == 0) return false;

    root = cJSON_Parse(schema);
    if ((root != NULL) && (cJSON_IsObject(root))) {
        cJSON* version = cJSON_GetObjectItem(root, sheens_version_key);

        if (version) {
            valid = true;
        }
    }

    cJSON_Delete(root);

    return valid;
}

static cslt_factory_t sheens_factory = {
        .encoder = TRANSCODER_NAME_SHEENS
};

static const cslt_transcoder_t icrule2sheens_transcoder = {
        .decoder = TRANSCODER_NAME_ICRULES,
        .encoder = TRANSCODER_NAME_SHEENS,

        .is_valid = icrule2sheens_is_valid,
        .transcode = icrule2sheens_transcode,
        .transcoder_version = IC_RULE_TO_SHEENS_TRANSCODER_VERSION
};

static const cslt_transcoder_t sheens2sheens_transcoder = {
        .decoder = TRANSCODER_NAME_SHEENS,
        .encoder = TRANSCODER_NAME_SHEENS,

        .is_valid = sheens2sheens_is_valid,
        .transcode = passthru_transcode,
        .transcoder_version = 0
};

void sheens_transcoder_init(icHashMap* settings)
{
    char* icrule_action_list_dir = NULL;

    if (settings != NULL) {
        icrule_action_list_dir = hashMapGet(settings,
                                            SHEENS_TRANSCODER_SETTING_ACTION_LIST_DIR,
                                            (uint16_t) strlen(SHEENS_TRANSCODER_SETTING_ACTION_LIST_DIR));

        deviceIdMapper = hashMapGet(settings, SHEENS_TRANSCODER_DEVICE_ID_MAPPER,
                                    (uint16_t) strlen(SHEENS_TRANSCODER_DEVICE_ID_MAPPER));
    }

    icrule_set_action_list_dir(icrule_action_list_dir);

    cslt_register_factory(&sheens_factory);

    cslt_register_transcoder(&icrule2sheens_transcoder);
    cslt_register_transcoder(&sheens2sheens_transcoder);
}

/**
 * Map a server identifier to a device service id and endpoint id
 * @param deviceId the input server device id to map
 * @param mappedDeviceId the mapped device id
 * @param mappedEndpointId the mapped endpoint id
 * @return the url
 */
bool sheens_transcoder_map_device_id(const char* deviceId, char **mappedDeviceId, char **mappedEndpointId)
{
    bool retVal = false;
    if (deviceIdMapper != NULL)
    {
        retVal = deviceIdMapper(deviceId, mappedDeviceId, mappedEndpointId);
    }
    else
    {
        // Default implementation that deals with zilker IDs
        char *sep = strchr(deviceId, '.');
        if (sep != NULL)
        {
            *mappedDeviceId = strdup(sep+1);
            if (mappedEndpointId != NULL)
            {
                *mappedEndpointId = strdup("*");
            }
            retVal = true;
        }
    }

    return retVal;
}
