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
// Created by Boyd, Weston on 5/4/18.
//

#include <stdio.h>
#include <string.h>
#include <uuid/uuid.h>
#include <icrule/icrule.h>
#include <errno.h>
#include <stdlib.h>
#include <deviceService/deviceService_event.h>
#include <icrule/icrule_time.h>
#include <icUtil/stringUtils.h>
#include <jsonrpc/jsonrpc.h>
#include <commonDeviceDefs.h>

#include "js/timeFunctions.js.h"
#include "sheens_javascript.h"
#include "sheens_json.h"
#include "sheens_request.h"
#include "sheens_transcoders.h"

/**  Length of a printed UUID plus null terminator. */
#define UUID_SIZE 37

/* old event codes (backward compat) */
#define ZONE_EVENT_FAULT_CODE                     220
#define ZONE_EVENT_RESTORE_CODE                   221
#define ZONE_EVENT_OCC_FAULT_VALUE                13
#define ZONE_EVENT_OCC_RESTORE_VALUE              14
#define TROUBLE_OCCURED_EVENT                     255

/* old zone type numeric values */
#define SECURITY_ZONE_TYPE_DOOR                1
#define SECURITY_ZONE_TYPE_WINDOW              2
#define SECURITY_ZONE_TYPE_MOTION              3
#define SECURITY_ZONE_TYPE_GLASS_BREAK         4
#define SECURITY_ZONE_TYPE_SMOKE               5
#define SECURITY_ZONE_TYPE_CO                  6
#define SECURITY_ZONE_TYPE_WATER               8

/* trouble of a device */
#define TROUBLE_TYPE_DEVICE   5

static const char* device_update_event_key = DEVICE_RESOURCE_UPDATED_EVENT_NAME;


/** Custom trigger handler definition.
 *
 * @param icrule Legacy iControl rule.
 * @param trigger Legacy iControl rule trigger definition.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return
 */
typedef cJSON* (*trigger_handler_t)(const icrule_trigger_t* trigger,
                                    const char* on_success_node,
                                    const char* on_failure_node,
                                    cJSON* nodes_object,
                                    cJSON* node_branches);

/**
 * Generates a new UUID for a node target name. Caller must free the memory.
 * @return a uuid string
 */
static char* generate_uuid()
{
    uuid_t uuid = "";
    char* uuid_str = malloc(UUID_SIZE);
    uuid_generate(uuid);
    uuid_unparse_upper(uuid, uuid_str);
    return uuid_str;
}

/**
 * Creates a generic state node to execute the given javascript. On success, the node will transition to on_success_node.
 * On failure, the node with transition to on_failure_node. The node will be added to node_objects, and will bind the
 * provided optional argument pattern_to_bind as its pattern to match (none otherwise). This will return the cJSON for
 * the new node, which will need to be added to the caller's node_object array.
 * @param js    The javascript to execute in this node.
 * @param on_success_node   The node to transition to on success.
 * @param on_failure_node   The node to transition to on failure.
 * @param success_pattern   The pattern to match at this node's starting branch, or NULL if no pattern needs matching
 * @return  A pointer to the new node
 */
static cJSON* create_conditional_state_node(const char* js, const char* on_success_node,
                                           const char* on_failure_node, cJSON* success_pattern)
{
    cJSON* branch_array_json;

    branch_array_json = cJSON_CreateArray();
    cJSON_AddItemToArray(branch_array_json,
                         sheens_create_branch(success_pattern,
                                              on_success_node,
                                              false));

    //Only add the failure target if the success target has a pattern to match against.
    if (success_pattern != NULL && cJSON_IsNull(success_pattern) == false)
    {
        cJSON_AddItemToArray(branch_array_json,
                             sheens_create_branch(NULL,
                                                  on_failure_node,
                                                  false));
    }

    return sheens_create_state_node(cJSON_CreateString(js),
                                             branch_array_json,
                                             false);
}

/**
 * Creates a new pass-through node that injects a key/value pair into the bindings. Useful when you have to match against
 * a pattern with a constant value, and reactively need to know what that value was in future states (for instance,
 * for action parameters).
 *
 * @param key   The local key to put into bindings.
 * @param value The value associated with that key to put into bindings.
 * @param next_node_target The node that this plain binding injection node should transition to
 * @return  The new node, or NULL if an invalid key is passed
 */
static cJSON* create_save_binding_state_node(const char* key, const char* value, const char* next_node_target)
{
    //Js to put {key : value} into the bindings
    static const char* binding_extension_js = "_.bindings['%s'] = %s; return _.bindings;";

    //Don't allow a NULL key
    if (key == NULL)
    {
        return NULL;
    }

    const char* null_coalesce_value = "null";

    //Coalesce the value to null if they pass null
    const char* coalesced_value = stringCoalesceAlt(value, null_coalesce_value);

    //Shove the key/value into the javascript.
    AUTO_CLEAN(free_generic__auto) char *expanded_js_script = stringBuilder(binding_extension_js, key, coalesced_value);

    return create_conditional_state_node(expanded_js_script, next_node_target, NULL, NULL);
}

static cJSON* build_device_pattern(const char* id, const char* resource, const char* value)
{
    cJSON *json[2];
    char* uuid;

    if (sheens_transcoder_map_device_id(id, &uuid, NULL) == true)
    {

        json[0] = cJSON_CreateObject();
        cJSON_AddItemToObjectCS(json[0], "id", cJSON_CreateString(resource));
        cJSON_AddItemToObjectCS(json[0], "value", cJSON_CreateString(value));

        json[1] = cJSON_CreateObject();
        cJSON_AddItemToObjectCS(json[1], "DSResource", json[0]);

        json[0] = cJSON_CreateObject();
        cJSON_AddItemToObjectCS(json[0], "resource", json[1]);
        cJSON_AddItemToObjectCS(json[0], "rootDeviceId", cJSON_CreateString(uuid));

        free(uuid);
    }

    return json[0];
}

/*
 * Builds Pattern Matching:
 *
 *   "troubleEvent":{
 *      "trouble":{
 *         "troubleObj":{
 *            "type": <type>
 *  Returns object at "troubleObj"
 */
static cJSON* build_trouble_pattern_no_extra(cJSON *add_to, int troubleType)
{
    cJSON *troubleObj, *trouble, *troubleEvent;
    troubleObj = cJSON_CreateObject();

    cJSON_AddNumberToObject(troubleObj,"type",troubleType);

    trouble = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(trouble,"troubleObj",troubleObj);

    troubleEvent = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(troubleEvent,"trouble",trouble);

    cJSON_AddItemToObjectCS(add_to,"troubleEvent",troubleEvent);

    return troubleObj;
}

/*
 *  Builds Pattern Matching:
 *
 *   "troubleEvent":{
 *      "trouble":{
 *         "troubleObj":{
 *            "type": <type>
 *            "extra":{
 *
 *  Returns object at "extra"
 */
static cJSON* build_trouble_pattern(cJSON* add_to, int troubleType)
{
    cJSON *trouble_object = build_trouble_pattern_no_extra(add_to, troubleType);

    cJSON *json = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(trouble_object, "extra", json);

    return json;
}

static bool sensor_trigger_build_state(cJSON* json, icrule_trigger_sensor_state_t state)
{
    bool ret = false;

    switch (state) {
        case TRIGGER_SENSOR_STATE_OPEN:
            cJSON_AddItemToObjectCS(json,
                                    sheens_event_code_key,
                                    cJSON_CreateNumber(ZONE_EVENT_FAULT_CODE));
            // In order to correctly process occ fault we need to know what the event value is
            cJSON_AddItemToObjectCS(json,
                                    sheens_event_value_key,
                                    cJSON_CreateStringReference(sheens_event_value_bound_key));
            ret = true;
            break;
        case TRIGGER_SENSOR_STATE_CLOSED:
            cJSON_AddItemToObjectCS(json,
                                    sheens_event_code_key,
                                    cJSON_CreateNumber(ZONE_EVENT_RESTORE_CODE));
            // In order to correctly process occ restore we need to know what the event value is
            cJSON_AddItemToObjectCS(json,
                                    sheens_event_value_key,
                                    cJSON_CreateStringReference(sheens_event_value_bound_key));
            ret = true;
            break;
        case TRIGGER_SENSOR_STATE_EITHER:
            cJSON_AddItemToObjectCS(json,
                                    sheens_event_code_key,
                                    cJSON_CreateStringReference("?sensorFaultCheck"));
            // In order to correctly process occ fault/restore we need to know what the event value is
            cJSON_AddItemToObjectCS(json,
                                    sheens_event_value_key,
                                    cJSON_CreateStringReference(sheens_event_value_bound_key));
            ret = true;
            break;
        case TRIGGER_SENSOR_STATE_TROUBLE:
            cJSON_AddItemToObjectCS(json,
                                    sheens_event_code_key,
                                    cJSON_CreateNumber(TROUBLE_OCCURED_EVENT));
            break;
    }

    return ret;
}

static bool sensor_trigger_build_sensortype(cJSON* json, icrule_trigger_sensor_type_t type)
{
    static const char* sensor_type_key = "sensorType";

    bool ret = false;

    switch (type) {
        case TRIGGER_SENSOR_TYPE_NONMOTION_SENSORS:
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateStringReference("?sensorNonMotion"));
            ret = true;

            break;
        case TRIGGER_SENSOR_TYPE_DOOR:
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateNumber(SECURITY_ZONE_TYPE_DOOR));
            break;
        case TRIGGER_SENSOR_TYPE_WINDOW:
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateNumber(SECURITY_ZONE_TYPE_WINDOW));
            break;
        case TRIGGER_SENSOR_TYPE_MOTION:
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateNumber(SECURITY_ZONE_TYPE_MOTION));
            break;
        case TRIGGER_SENSOR_TYPE_GLASS_BREAK:
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateNumber(SECURITY_ZONE_TYPE_GLASS_BREAK));
            break;
        case TRIGGER_SENSOR_TYPE_SMOKE:
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateNumber(SECURITY_ZONE_TYPE_SMOKE));
            break;
        case TRIGGER_SENSOR_TYPE_CO:
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateNumber(SECURITY_ZONE_TYPE_CO));
            break;
        case TRIGGER_SENSOR_TYPE_WATER:
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateNumber(SECURITY_ZONE_TYPE_WATER));
            break;
        case TRIGGER_SENSOR_TYPE_ALL_SENSORS:
            // We still want to capture to ensure the element is there, that way we know
            // it was a sensor that triggered this and not some other device type
            cJSON_AddItemToObjectCS(json,
                                    sensor_type_key,
                                    cJSON_CreateStringReference("?sensorType"));
            break;

        case TRIGGER_SENSOR_TYPE_INVALID:
        case TRIGGER_SENSOR_TYPE_DRY_CONTACT:
        case TRIGGER_SENSOR_TYPE_INERTIA:
        case TRIGGER_SENSOR_TYPE_LIGHTING:
        case TRIGGER_SENSOR_TYPE_TEMPERATURE:
        case TRIGGER_SENSOR_TYPE_DOOR_LOCK:
            break;
    }

    return ret;
}

/** Sensor trigger handler.
 *
 * The sensor trigger is primarily handled from sensor events,
 * however, the trouble portion of the trigger will have
 * to be handled from trouble events.
 *
 * @param icrule Legacy iControl rule.
 * @param trigger Legacy iControl rule trigger definition.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return A JSON object representing a single branch (pattern + target)
 * for a Sheens spec.
 */
static cJSON* sensor_trigger_handler(const icrule_trigger_t* trigger,
                                     const char* on_success_node,
                                     const char* on_failure_node,
                                     cJSON* nodes_object,
                                     cJSON* node_branches)
{
    static const char* sensor_trigger_js = ""
            "var fault_check = true;\n"
            "var type_check = true;\n"
            "\n"
            "if ('?sensorFaultCheck' in _.bindings) {\n"
            "    fault_check = ((_.bindings['?sensorFaultCheck'] == %d) || (_.bindings['?sensorFaultCheck'] == %d));\n"
            "}\n"
            "if ('?sensorNonMotion' in _.bindings) {\n"
            "    type_check = (_.bindings['?sensorNonMotion'] != %d);\n"
            "}\n"
            "if ('" sheens_event_value_bound_key "' in _.bindings) {\n"
            "    var evVal = _.bindings['" sheens_event_value_bound_key "'];\n"
            "    if ((evVal == %d) || (evVal == %d)) {\n"
            "        _.bindings['" sheens_event_on_demand_required_key "'] = true;\n"
            "    }\n"
            "}\n"
            "if (fault_check && type_check) {\n"
            "    _.bindings['" sheens_allowed_key "'] = true;\n"
            "    return _.bindings;\n"
            "} else {\n"
            "    return {'" sheens_allowed_key "': false};\n"
            "}\n";

    //Make a local copy since we may need to replace with a locally allocated version
    AUTO_CLEAN(free_generic__auto) char *local_success_node = strdup(on_success_node);
    cJSON *branch = NULL, *pattern;
    bool create_sensor_node = false;

    const icrule_trigger_sensor_t* sensor_trigger = &trigger->trigger.sensor;

    pattern = cJSON_CreateObject();

    sheens_pattern_add_constraints_required(pattern);

    icLinkedList *triggerPatterns = linkedListCreate();

    // FIXME: rework for trouble
    if (sensor_trigger->state == TRIGGER_SENSOR_STATE_TROUBLE) {
        /* Looking for trouble pattern:
         *   "pattern":{
         *       "_evId":"??evId",
         *       "_evTime":"?evTime",
         *       "_sunrise":"?_sunrise",
         *       "_sunset":"?_sunset",
         *       "_evCode":255,
         *       "troubleEvent":{
         *           "trouble":{
         *               "troubleObj":{
         *                   "type":5,
         *               }
         *           }
         *       }
         *   },
         */
        cJSON* trouble_object, *troubleEvent;

        // First build our standard sensor trouble pattern

        //Returns the json object at "extra"
        //
        trouble_object = build_trouble_pattern(pattern, TROUBLE_TYPE_DEVICE);

        //Panel status pattern needs to be added to the "troubleEvent" object, so we need to get it
        //
        troubleEvent = cJSON_GetObjectItem(pattern,"troubleEvent");

        create_sensor_node |= sensor_trigger_build_sensortype(trouble_object, sensor_trigger->type);

        linkedListAppend(triggerPatterns, pattern);

        // If the trigger sensor type is all sensors or nonmotions sensors, we're actually supposed to let PRM/PIM
        // troubles pass the trigger as well. This is parity with 9.x behavior.
        if (sensor_trigger->type == TRIGGER_SENSOR_TYPE_NONMOTION_SENSORS ||
            sensor_trigger->type == TRIGGER_SENSOR_TYPE_ALL_SENSORS)
        {
            // First we'll do PIM
            pattern = cJSON_CreateObject();
            sheens_pattern_add_constraints_required(pattern);

            // Returns the json object at "extra"
            //
            trouble_object = build_trouble_pattern(pattern, TROUBLE_TYPE_DEVICE);

            // Panel status pattern needs to be added to the "troubleEvent" object, so we need to get it
            //
            troubleEvent = cJSON_GetObjectItem(pattern,"troubleEvent");
            linkedListAppend(triggerPatterns, pattern);

            // Next do the same for PRM
            pattern = cJSON_CreateObject();
            sheens_pattern_add_constraints_required(pattern);

            // Returns the json object at "extra"
            //
            trouble_object = build_trouble_pattern(pattern, TROUBLE_TYPE_DEVICE);

            // Panel status pattern needs to be added to the "troubleEvent" object, so we need to get it
            //
            troubleEvent = cJSON_GetObjectItem(pattern,"troubleEvent");
            linkedListAppend(triggerPatterns, pattern);

            // Now the bridge endpoint profile.
            pattern = cJSON_CreateObject();
            sheens_pattern_add_constraints_required(pattern);

            // Returns the json object at "extra"
            //
            trouble_object = build_trouble_pattern(pattern, TROUBLE_TYPE_DEVICE);

            // Panel status pattern needs to be added to the "troubleEvent" object, so we need to get it
            //
            troubleEvent = cJSON_GetObjectItem(pattern,"troubleEvent");
            linkedListAppend(triggerPatterns, pattern);
        }

    } else if (sensor_trigger->id == NULL || strchr(sensor_trigger->id, '.') == NULL) {
        cJSON *pattern_sensor;

        /* Set up the pattern to be used by this sensor. */
        pattern_sensor = cJSON_CreateObject();

        //Filter out bypassed
        cJSON_AddBoolToObject(pattern_sensor, "isBypassed", false);

        create_sensor_node |= sensor_trigger_build_sensortype(pattern_sensor, sensor_trigger->type);

        /* We only care about the sub-json objects if elements were
         * added to the "sensor" object anyways.
         */
//        if (pattern_sensor->child != NULL) {
//            cJSON* __json = cJSON_CreateObject();
//
//            cJSON_AddItemToObjectCS(__json, "sensor", pattern_sensor);
//        } else {
//            cJSON_Delete(pattern_sensor);
//        }
        cJSON_AddItemToObjectCS(pattern, "securityZoneEvent", pattern_sensor);

        linkedListAppend(triggerPatterns, pattern);

        // Extend zone motion rule for camera motion
        // Add resourceUpdatedEvent for camera motion event
        if (sensor_trigger->type == TRIGGER_SENSOR_TYPE_MOTION)
        {
            pattern = cJSON_CreateObject();
            sheens_pattern_add_constraints_required(pattern);

            cJSON* resourceUpdatedEventPattern = cJSON_CreateObject();
            // added the rootDeviceClass so this triggers only for camera fault event
            cJSON_AddItemToObjectCS(resourceUpdatedEventPattern,
                                    DS_RESOURCE_ROOT_DEVICE_CLASS,
                                    cJSON_CreateString(CAMERA_DC));

            cJSON* resourceContainer = cJSON_AddObjectToObject(resourceUpdatedEventPattern, DEVICE_RESOURCE_UPDATED_EVENT_RESOURCE);

            // added DSResource with fault resource id
            cJSON* faultResource = cJSON_AddObjectToObject(resourceContainer, DS_RESOURCE);

            cJSON_AddItemToObjectCS(faultResource,
                                    DS_RESOURCE_ID,
                                    cJSON_CreateString(SENSOR_PROFILE_RESOURCE_FAULTED));
            // Check for the faulted resource. We don't have to worry about bypass because camera
            // motions that are bypassed mean motion is disabled and we won't ever trigger a fault
            cJSON_AddItemToObjectCS(faultResource,
                                    DS_RESOURCE_VALUE,
                                    cJSON_CreateString(bool2str(sensor_trigger->state == TRIGGER_SENSOR_STATE_OPEN)));
            cJSON_AddItemToObjectCS(pattern,
                                    sheens_event_code_key,
                                    cJSON_CreateNumber(DEVICE_SERVICE_EVENT_RESOURCE_UPDATED));
            cJSON_AddItemToObjectCS(pattern,
                                    device_update_event_key,
                                    resourceUpdatedEventPattern);

            cJSON *camera_device_class_action_node = create_save_binding_state_node(sheens_event_on_demand_required_key,
                    "true", on_success_node);
            if (camera_device_class_action_node == NULL)
            {
                cJSON_Delete(pattern);
                // destroy the list
                linkedListDestroy(triggerPatterns, (linkedListItemFreeFunc) cJSON_Delete);
                return NULL;
            }
            AUTO_CLEAN(free_generic__auto) char *action_node_name = generate_uuid();
            cJSON_AddItemToObject(nodes_object, action_node_name, camera_device_class_action_node);

            // Add the pattern to the branches node.
            // Added here because we don't need to create_zone_node and related binding for camera
            cJSON_AddItemToArray(node_branches, sheens_create_branch(pattern, action_node_name, false));
        }
    } else {
        // Camera motion sensor

        // TODO Not sure what trouble means for camera motion(its an option in the UI), so for now cause that to error
        if (sensor_trigger->state == TRIGGER_SENSOR_STATE_TROUBLE) {
            cJSON_Delete(pattern);
            // list is empty
            linkedListDestroy(triggerPatterns, NULL);
            return NULL;
        }

        // Check for the faulted resource.  We don't have to worry about bypass because camera motions that are
        // bypassed mean motion is disabled and we won't ever trigger a fault
        cJSON *json = build_device_pattern(sensor_trigger->id, "faulted",
                                           bool2str(sensor_trigger->state == TRIGGER_SENSOR_STATE_OPEN));
        if (json == NULL) {
            cJSON_Delete(pattern);
            // list is empty
            linkedListDestroy(triggerPatterns, NULL);
            return NULL;
        }

        cJSON_AddItemToObjectCS(pattern,
                                sheens_event_code_key,
                                cJSON_CreateNumber(DEVICE_SERVICE_EVENT_RESOURCE_UPDATED));

        cJSON_AddItemToObjectCS(pattern, device_update_event_key, json);

        cJSON *camera_device_class_action_node = create_save_binding_state_node(sheens_event_on_demand_required_key, "true",
                on_success_node);
        if (camera_device_class_action_node == NULL)
        {
            cJSON_Delete(pattern);
            // list is empty
            linkedListDestroy(triggerPatterns, NULL);
            return NULL;
        }

        char *action_node_name = generate_uuid();
        cJSON_AddItemToObject(nodes_object, action_node_name, camera_device_class_action_node);

        //we want our initial success node to be our intermediate node.
        free(local_success_node);
        local_success_node = action_node_name;

        linkedListAppend(triggerPatterns, pattern);
    }

    sbIcLinkedListIterator *patternIter = linkedListIteratorCreate(triggerPatterns);
    while (linkedListIteratorHasNext(patternIter) == true)
    {
        pattern = (cJSON *) linkedListIteratorGetNext(patternIter);

        // Append the event code/value for all non-camera-motion trigger patterns.
        if (sensor_trigger->id == NULL || strchr(sensor_trigger->id, '.') == NULL) {
            create_sensor_node |= sensor_trigger_build_state(pattern, sensor_trigger->state);
        }

        /* There was an either/or situation in the legacy rule. Now
         * we must create a secondary node that will hold that
         * check, and still branch to constraints and pass all the proper bindings.
         */
        if (create_sensor_node) {
            cJSON *pattern_json, *zone_node;

            /* Add in extra characters for the integers.
             * There are 9 '%d' expansions in the code base
             * Thus we will need 10 characters a piece. We
             * throw in a lot extra here just to be safe.
             */
            char expanded_js_script[strlen(sensor_trigger_js) + 256];

            snprintf(expanded_js_script,
                     sizeof(expanded_js_script),
                     sensor_trigger_js,
                     ZONE_EVENT_FAULT_CODE,
                     ZONE_EVENT_RESTORE_CODE,
                     SECURITY_ZONE_TYPE_MOTION,
                     ZONE_EVENT_OCC_FAULT_VALUE,
                     ZONE_EVENT_OCC_RESTORE_VALUE);

            pattern_json = cJSON_CreateObject();
            cJSON_AddItemToObjectCS(pattern_json, sheens_allowed_key, cJSON_CreateBool(true));

            // Now point our start trigger to this new custom node.
            zone_node = create_conditional_state_node(expanded_js_script, local_success_node, on_failure_node, pattern_json);
            AUTO_CLEAN(free_generic__auto) const char* zone_node_name = generate_uuid();
            cJSON_AddItemToObject(nodes_object, zone_node_name, zone_node);

            branch = sheens_create_branch(pattern, zone_node_name, false);
        } else {
            branch = sheens_create_branch(pattern, local_success_node, false);
        }

        // We have to return the last branch up the call chain, or else the transcoder will barf and exit.
        // For the other branches, we need to add them to our branch array manually.
        if (linkedListIteratorHasNext(patternIter) == true)
        {
            cJSON_AddItemToArray(node_branches, branch);
        }
    }

    // Just destroy the list. The contents are now owned by the branches we just inserted into the branches array
    linkedListDestroy(triggerPatterns, standardDoNotFreeFunc);

    return branch;
}

/** Light on/off/trouble trigger handler.
 *
 * @param icrule Legacy iControl rule.
 * @param trigger Legacy iControl rule trigger definition.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return A JSON object representing a single branch (pattern + target)
 * for a Sheens spec.
 */
static cJSON* lighting_trigger_handler(const icrule_trigger_t* trigger,
                                       const char* on_success_node,
                                       const char* on_failure_node,
                                       cJSON* nodes_object,
                                       cJSON* node_branches)
{
    const icrule_trigger_light_t* lighting = &trigger->trigger.lighting;
    cJSON *pattern, *json;

    json = build_device_pattern(lighting->id, "isOn", bool2str(lighting->enabled));
    if (json == NULL) return NULL;

    pattern = cJSON_CreateObject();

    sheens_pattern_add_constraints_required(pattern);

    cJSON_AddItemToObjectCS(pattern,
                            sheens_event_code_key,
                            cJSON_CreateNumber(DEVICE_SERVICE_EVENT_RESOURCE_UPDATED));
    cJSON_AddItemToObjectCS(pattern, device_update_event_key, json);

    return sheens_create_branch(pattern, on_success_node, false);
}

/** Door Lock trigger handler.
 *
 * @param icrule Legacy iControl rule.
 * @param trigger Legacy iControl rule trigger definition.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return A JSON object representing a single branch (pattern + target)
 * for a Sheens spec.
 */
static cJSON* doorlock_trigger_handler(const icrule_trigger_t* trigger,
                                       const char* on_success_node,
                                       const char* on_failure_node,
                                       cJSON* nodes_object,
                                       cJSON* node_branches)
{
    const icrule_trigger_doorlock_t* doorlock = &trigger->trigger.doorlock;
    cJSON* pattern;

    pattern = cJSON_CreateObject();
    sheens_pattern_add_constraints_required(pattern);

    if (doorlock->state == TRIGGER_DOORLOCK_STATE_TROUBLE) {
        /* Looking for trouble pattern:
         *  {
         *     "_evId":?eventId,
         *     "_evCode":255,
         *     "_evTime":?eventTime,
         *     "_sunrise":"?_sunrise",
         *     "_sunset":"?_sunset",
         *     "troubleEvent":{
         *        "trouble":{
         *           "troubleObj":{
         *              "type":5,
         *              "extra":{
         *                 "rootId": <string>
         *              }
         *           }
         *        }
         *     },
         *  }
         */
        char* id;

        sheens_transcoder_map_device_id(doorlock->id, &id, NULL);

        cJSON_AddItemToObjectCS(build_trouble_pattern(pattern, TROUBLE_TYPE_DEVICE),
                                "rootId",
                                cJSON_CreateString(id));
        free(id);
    } else {
        cJSON* json;

        json = build_device_pattern(doorlock->id,
                                    "locked",
                                    bool2str(doorlock->state == TRIGGER_DOORLOCK_STATE_LOCKED));
        if (json == NULL) return NULL;

        cJSON_AddItemToObjectCS(pattern,
                                sheens_event_code_key,
                                cJSON_CreateNumber(DEVICE_SERVICE_EVENT_RESOURCE_UPDATED));
        cJSON_AddItemToObjectCS(pattern, device_update_event_key, json);
    }

    return sheens_create_branch(pattern, on_success_node, false);
}

/** Thermostat trigger handler.
 *
 * @param icrule Legacy iControl rule.
 * @param trigger Legacy iControl rule trigger definition.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return A JSON object representing a single branch (pattern + target)
 * for a Sheens spec.
 */
static cJSON* thermostat_trigger_handler(const icrule_trigger_t* trigger,
                                         const char* on_success_node,
                                         const char* on_failure_node,
                                         cJSON* nodes_object,
                                         cJSON* node_branches)
{
    const icrule_trigger_thermostat_t* thermostat = &trigger->trigger.thermostat;
    cJSON* pattern;

    pattern = cJSON_CreateObject();
    sheens_pattern_add_constraints_required(pattern);

    if (thermostat->trouble) {
        /* Looking for trouble pattern:
         *  {
         *     "_evId":?eventId,
         *     "_evCode":255,
         *     "_evTime":?eventTime,
         *     "_sunrise":"?_sunrise",
         *     "_sunset":"?_sunset",
         *     "troubleEvent":{
         *        "trouble":{
         *           "troubleObj":{
         *              "type":5,
         *              "extra":{
         *                 "rootId": <string>
         *              }
         *           }
         *        }
         *     },
         *  }
         */
        char* id;

        sheens_transcoder_map_device_id(thermostat->id, &id, NULL);

        cJSON_AddItemToObjectCS(build_trouble_pattern(pattern, TROUBLE_TYPE_DEVICE),
                                "rootId",
                                cJSON_CreateString(id));
        free(id);

        return sheens_create_branch(pattern, on_success_node, false);
    } else {
        static const char* temperature_key = "?temperature";

        static const char* thermostat_js = ""
            "_.bindings['" sheens_allowed_key "'] = ((_.bindings['%s'] <= %d) || (_.bindings['%s'] >= %d));\n"
            "return _.bindings;\n";

        cJSON *json, *branch_array, *node_pattern;
        char* js;

        uuid_t uuid = "";
        char uuid_str[UUID_SIZE];

        js = malloc(strlen(thermostat_js) +
                    (strlen(temperature_key) * 2) +
                    (66 * 2) + // xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx.yy * 2 - temperature
                    1);
        if (js == NULL) {
            errno = ENOMEM;
            return NULL;
        }

        sprintf(js,
                thermostat_js,
                temperature_key, thermostat->bounds.lower,
                temperature_key, thermostat->bounds.upper);

        // Add in our new Node to the Sheens spec.
        uuid_generate(uuid);
        uuid_unparse_upper(uuid, uuid_str);

        branch_array = cJSON_CreateArray();
        node_pattern = cJSON_CreateObject();

        cJSON_AddItemToObjectCS(node_pattern, sheens_allowed_key, cJSON_CreateBool(true));
        cJSON_AddItemToArray(branch_array, sheens_create_branch(node_pattern, on_success_node, false));
        cJSON_AddItemToArray(branch_array, sheens_create_branch(NULL, on_failure_node, false));

        // Add our new custom node to the state machine
        cJSON_AddItemToObject(nodes_object,
                              uuid_str,
                              sheens_create_state_node(cJSON_CreateString(js), branch_array, false));

        free(js);

        json = build_device_pattern(thermostat->id, "localTemperature", temperature_key);
        if (json == NULL) return NULL;

        cJSON_AddItemToObjectCS(pattern,
                                sheens_event_code_key,
                                cJSON_CreateNumber(DEVICE_SERVICE_EVENT_RESOURCE_UPDATED));
        cJSON_AddItemToObjectCS(pattern, device_update_event_key, json);

        return sheens_create_branch(pattern, uuid_str, false);
    }
}

/** Thermostat threshold trigger handler.
 *
 * @param icrule Legacy iControl rule.
 * @param trigger Legacy iControl rule trigger definition.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return A JSON object representing a single branch (pattern + target)
 * for a Sheens spec.
 */
static cJSON* thermostat_threshold_trigger_handler(const icrule_trigger_t* trigger,
                                                   const char* on_success_node,
                                                   const char* on_failure_node,
                                                   cJSON* nodes_object,
                                                   cJSON* node_branches)
{
    return thermostat_trigger_handler(trigger,
                                      on_success_node,
                                      on_failure_node,
                                      nodes_object,
                                      node_branches);
}

static char* build_time_object(const icrule_time_t* time)
{
    cJSON* json;
    char* object = NULL;

    json = sheens_create_time_object(time);

    if (json != NULL) {
        object = cJSON_PrintBuffered(json, 128, false);
        cJSON_Delete(json);
    }

    return object;
}

static char* build_repeat_interval(const icrule_trigger_time_t* time,
                                   const char* on_success_node,
                                   const char* on_failure_node,
                                   cJSON* nodes_object,
                                   cJSON* node_branches)
{
    /* JavaScript only necessary if there is a repeat interval for
     * this timer. This code sets up an "end" absolute time
     * then tells the outside world to create a timer for the
     * interval.
     *
     * Positional parameters:
     *
     * 5: interval (seconds)
     * 6: Make timer emit object
     */
    static const char* interval_js = ""
        "    // Create a jrpc time ticker only if the timer does not already exist\n"
        "    if (('persist' in _.bindings) == false) {\n"
        "        _.bindings['" sheens_allowed_key "'] = true;\n"
        "        var endDate = new Date(_.bindings['" sheens_event_time_bound_key "']).getTime();\n"
        "        endDate = new Date(endDate + (getEndDate(now, end) * 1000));\n"
        "\n"
        "        _.bindings['persist'] = { endTime: endDate, interval: %d }\n"
        "        _.out([%s]);\n"
        "    }\n"
    ;

    /* JavaScript to handle the interval timer. If we can create a new
     * timer interval without going past the "end" time then create
     * a new timer and jump to constraints, otherwise reset.
     *
     * Positional Parameters:
     *
     * 1: Timer emit object
     */
    static const char* interval_timer_js = ""
        "if ('persist' in _.bindings) {\n"
        "    var checkTime = new Date().getTime();\n"
        "    var persist = _.bindings['persist'];\n"
        "\n"
        "    checkTime = new Date(checkTime + (persist.interval * 1000));\n"
        "\n"
        "    if (checkTime < new Date(persist.endTime)) {\n"
        "        _.out([%s]);\n"
        "        _.bindings['" sheens_allowed_key "'] = true;\n"
        "    } else {\n"
        "        _.bindings['" sheens_allowed_key "'] = false;\n"
        "        delete _.bindings['persist'];\n"
        "    }\n"
        "}\n"
        "\n"
        "return _.bindings;\n"
    ;

    AUTO_CLEAN(free_generic__auto) char *mktimer_request;
    char *js;

    cJSON *pattern, *branch_array;

    uuid_t uuid = "";
    char uuid_str[UUID_SIZE];

    // New UUID for our interval timer
    uuid_generate(uuid);
    uuid_unparse_upper(uuid, uuid_str);

    /* Create the timer start pattern with proper constraint handling */
    pattern = sheens_create_timer_fired_object(uuid_str);
    if (jsonrpc_is_valid(pattern) == true)
    {
        cJSON* params = cJSON_GetObjectItem(pattern, "params");

        if (params == NULL) {
            params = cJSON_CreateObject();
            cJSON_AddItemToObjectCS(pattern, "params", params);
        }

        // Unlike the structure of our typical events, automation service injects the required constraints into the
        // "params" node of a json rpc message.
        sheens_pattern_add_constraints_required(params);
    }
    cJSON_AddItemToArray(node_branches, sheens_create_branch(pattern, uuid_str, false));

    /* Create the '_.out' emit message that will setup
     * the timer. The timer will fire on our interval.
     *
     * The emit message will go into two different places:
     * 1) In the timer trigger node.
     * 2) In the repeat interval timer node.
     */
    pattern = sheens_create_timer_emit_object((uint32_t) time->repeat_interval,
                                              uuid_str,
                                              NULL);

    /* Do not delete the emit message as we will need it later on. */
    mktimer_request = cJSON_PrintBuffered(pattern, 256, false);
    cJSON_Delete(pattern);

    if (mktimer_request == NULL)
    {
        return NULL;
    }

    js = stringBuilder(interval_timer_js, mktimer_request);

    if (js == NULL)
    {
        return NULL;
    }

    /* Create the new node with UUID to handle the custom action. */
    branch_array = cJSON_CreateArray();
    pattern = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(pattern, sheens_allowed_key, cJSON_CreateBool(true));
    cJSON_AddItemToArray(branch_array, sheens_create_branch(pattern, on_success_node, false));
    cJSON_AddItemToArray(branch_array, sheens_create_branch(NULL, on_failure_node, false));

    // Add our new custom node to the state machine
    cJSON_AddItemToObject(nodes_object,
                          uuid_str,
                          sheens_create_state_node(cJSON_CreateString(js), branch_array, false));

    free(js);


    // Build the interval js that will be the finale of time interval trigger source nodes
    js = stringBuilder(interval_js, time->repeat_interval, mktimer_request);

    if (js == NULL)
    {
        errno = ENOMEM;
    }

    return js;
}

/** Time trigger handler.
 *
 * @param icrule Legacy iControl rule.
 * @param trigger Legacy iControl rule trigger definition.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return A JSON object representing a single branch (pattern + target)
 * for a Sheens spec.
 */
static cJSON* time_trigger_handler(const icrule_trigger_t* trigger,
                                   const char* on_success_node,
                                   const char* on_failure_node,
                                   cJSON* nodes_object,
                                   cJSON* node_branches)
{
    static const char* exact_match_js = ""
        "if (isTimeMatch(now, when, _.bindings['" sheens_sunrise_bound_key "'], _.bindings['" sheens_sunset_bound_key "'])) {\n"
        "    _.bindings['" sheens_allowed_key "'] = true;\n"
        "}\n"
    ;

    /* Positional parameters:
     * End time object
     * Interval js
     *
     * Note: The reason we have this mutually exclusive from exact match is to allow "resuming" of an interval rule when
     * the CPE loses power or is restarted.
     */
    static const char* interval_match_js = ""
       "var end = %s;\n"
       "end.seconds = getTimeSeconds(end, _.bindings['" sheens_sunrise_bound_key "'], _.bindings['" sheens_sunset_bound_key "']);\n"
       "\n"
       "if (isInInterval(now, when, end)) {"
       "%s"
       "}\n"
   ;

    /* Positional parameters:
     * When Object (when to trigger).
     * Matching js conditional (exact or interval)
     */
    static const char* time_js = TIMEFUNCTIONS_JS_BLOB
        "\n"
        "var now = new WeekTime(_.bindings['" sheens_event_time_bound_key "']);\n"
        "var when = %s;\n"
        "when.seconds = getTimeSeconds(when, _.bindings['" sheens_sunrise_bound_key "'], _.bindings['" sheens_sunset_bound_key "']);\n"
        "\n"
        "%s"
        "\n"
        "return _.bindings;"
        "\n"
    ;

    const icrule_trigger_time_t* time = &trigger->trigger.time;
    AUTO_CLEAN(free_generic__auto) char *interval_javascript = NULL;
    AUTO_CLEAN(free_generic__auto) char *js = NULL;
    AUTO_CLEAN(free_generic__auto) char *whenTimeJs = NULL;
    AUTO_CLEAN(free_generic__auto) char *endTimeJs = NULL;

    cJSON *pattern, *branch_array;

    uuid_t uuid = "";
    char uuid_str[UUID_SIZE];

    whenTimeJs = build_time_object(&time->when);
    if (whenTimeJs == NULL)
    {
        return NULL;
    }

    /* Repeat a timer "fire" every f(x) number of seconds
     * until an "end" time is reached.
     *
     * According to the XSD a value of '0' disables repeat, and
     * a value of '-1000' cause the interval to randomize.
     *
     * Note: No instances of '-1000' have been found in production
     * thus a decision has been made to not build in support.
     */
    if (time->repeat_interval > 0)
    {
        endTimeJs = build_time_object(&time->end);

        interval_javascript = build_repeat_interval(time,
                                                    on_success_node,
                                                    on_failure_node,
                                                    nodes_object,
                                                    node_branches);
        if (interval_javascript == NULL || endTimeJs == NULL)
        {
            return NULL;
        }

        AUTO_CLEAN(free_generic__auto) char *intervalConditionJs = stringBuilder(interval_match_js, endTimeJs, interval_javascript);

        // Build the final javascript for interval rules
        js = stringBuilder(time_js, whenTimeJs, intervalConditionJs);
    }
    else
    {
        // Build the final javascript for exact time triggers
        js = stringBuilder(time_js, whenTimeJs, exact_match_js);
    }

    if (js == NULL)
    {
        return NULL;
    }

    // Add in our new Node to the Sheens spec.
    uuid_generate(uuid);
    uuid_unparse_upper(uuid, uuid_str);

    branch_array = cJSON_CreateArray();
    pattern = cJSON_CreateObject();

    cJSON_AddItemToObjectCS(pattern, sheens_allowed_key, cJSON_CreateBool(true));
    cJSON_AddItemToArray(branch_array, sheens_create_branch(pattern, on_success_node, false));
    cJSON_AddItemToArray(branch_array, sheens_create_branch(NULL, on_failure_node, false));

    // Add our new custom node to the state machine
    cJSON_AddItemToObject(nodes_object,
                          uuid_str,
                          sheens_create_state_node(cJSON_CreateString(js), branch_array, false));

    // Create schedule timer tick pattern for start.
    pattern = cJSON_CreateObject();
    cJSON_AddItemToObjectCS(pattern,
                            sheens_event_code_key,
                            cJSON_CreateNumber(TIMER_TICK_EVENT_CODE));

    // Required by the scheduled actions.
    sheens_pattern_add_constraints_required(pattern);

    // We always branch to constraints as it is always universally valid.
    return sheens_create_branch(pattern, uuid_str, false);
}

/** Zigbee Comm trigger handler.
 *
 * @param icrule Legacy iControl rule.
 * @param trigger Legacy iControl rule trigger definition.
 * @param nodes_object The top-level nodes JSON that holds all states.
 * @return A JSON object representing a single branch (pattern + target)
 * for a Sheens spec.
 */
static cJSON* zigbee_comm_trigger_handler(const icrule_trigger_t* trigger,
                                          const char* on_success_node,
                                          const char* on_failure_node,
                                          cJSON* nodes_object,
                                          cJSON* node_branches)
{
    const icrule_trigger_zigbee_comm_t* zigbee = &trigger->trigger.zigbeecomm;
    cJSON* pattern;

    pattern = cJSON_CreateObject();
    sheens_pattern_add_constraints_required(pattern);

    cJSON* json;

    json = build_device_pattern(zigbee->id,
                                "communicationFailure",
                                bool2str(zigbee->state == TRIGGER_ZIGBEE_COMM_STATE_LOST));
    if (json == NULL)
    {
        return NULL;
    }

    cJSON_AddItemToObjectCS(pattern,
                            sheens_event_code_key,
                            cJSON_CreateNumber(DEVICE_SERVICE_EVENT_RESOURCE_UPDATED));
    cJSON_AddItemToObjectCS(pattern, device_update_event_key, json);

    return sheens_create_branch(pattern, on_success_node, false);
}

/* maps to icrule_trigger_type_t enum */
static const trigger_handler_t trigger_handlers[] = {
        sensor_trigger_handler,
        NULL, //touchscreen_trigger_handler,
        lighting_trigger_handler,
        doorlock_trigger_handler,
        thermostat_trigger_handler,
        thermostat_threshold_trigger_handler,
        time_trigger_handler,
        NULL, //cloud_trigger_handler,
        NULL, //cloud_service_trigger_handler,

        // Note: No proof of these types found in production!
        NULL, //network_trigger_handler,
        zigbee_comm_trigger_handler,
        NULL, //switch_trigger_handler,
        NULL, //resource_trigger_handler,
        NULL, //panic_trigger_handler,
};

cJSON* sheens_trigger2javascript(const icrule_trigger_t* trigger,
                                 const char* on_success_node,
                                 const char* on_failure_node,
                                 cJSON* nodes_object,
                                 cJSON* node_branches)
{
    trigger_handler_t handler;

    if (trigger->type >= sizeof_array(trigger_handlers)) {
        errno = EINVAL;
        return NULL;
    }

    handler = trigger_handlers[trigger->type];
    if (handler) {
        return handler(trigger,
                       on_success_node,
                       on_failure_node,
                       nodes_object,
                       node_branches);
    } else {
        errno = ENOTSUP;
        return NULL;
    }
}
