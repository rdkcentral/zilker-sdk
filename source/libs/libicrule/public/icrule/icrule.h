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
// Created by Boyd, Weston on 4/27/18.
//

#ifndef ZILKER_ICRULE_H
#define ZILKER_ICRULE_H

#include <stdint.h>
#include <stdbool.h>

#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>

#include <icrule/icrule_time.h>
#include <icrule/icrule_triggers.h>

// Triggers
typedef enum {
    TRIGGER_TYPE_SENSOR = 0,
    TRIGGER_TYPE_TOUCHSCREEN,
    TRIGGER_TYPE_LIGHTING,
    TRIGGER_TYPE_DOOR_LOCK,
    TRIGGER_TYPE_THERMOSTAT,
    TRIGGER_TYPE_THERMOSTAT_THRESHOLD,
    TRIGGER_TYPE_TIME,
    TRIGGER_TYPE_CLOUD,
    TRIGGER_TYPE_CLOUD_SERVICE,

    // Note: No proof of these types found in production!
    TRIGGER_TYPE_NETWORK,
    TRIGGER_TYPE_ZIGBEE_COMM_STATUS,
    TRIGGER_TYPE_SWITCH,
    TRIGGER_TYPE_RESOURCE,
    TRIGGER_TYPE_PANIC
} icrule_trigger_type_t;

typedef enum {
    TRIGGER_CATEGORY_SENSOR = 0,
    TRIGGER_CATEGORY_TOUCHSCREEN,
    TRIGGER_CATEGORY_SCENE,
    TRIGGER_CATEGORY_LIGHT,
    TRIGGER_CATEGORY_DOOR_LOCK,
    TRIGGER_CATEGORY_THERMOSTAT,
    TRIGGER_CATEGORY_NETWORK,
    TRIGGER_CATEGORY_PANIC,
    TRIGGER_CATEGORY_TIME,
    TRIGGER_CATEGORY_SWITCH,
    TRIGGER_CATEGORY_CLOUD,
    TRIGGER_CATEGORY_RESOURCE
} icrule_trigger_category_t;

typedef struct icrule_trigger {
    icrule_trigger_type_t type;
    icrule_trigger_category_t category;

    char* desc;

    union
    {
        icrule_trigger_sensor_t sensor;
        icrule_trigger_touchscreen_t touchscreen;
        icrule_trigger_light_t lighting;
        icrule_trigger_doorlock_t doorlock;
        icrule_trigger_thermostat_t thermostat;
        icrule_trigger_time_t time;
        icrule_trigger_cloud_t cloud;
        icrule_trigger_zigbee_comm_t zigbeecomm;
    } trigger;
} icrule_trigger_t;

typedef struct icrule_trigger_list {
    bool negate;
    int delay;

    icLinkedList* triggers;
} icrule_trigger_list_t;

// Constraints
typedef enum {
    CONSTRAINT_LOGIC_OR = 1,
    CONSTRAINT_LOGIC_AND,
} icrule_constraint_logic_t;

typedef struct icrule_constraint_time {
    icrule_time_t start;
    icrule_time_t end;
} icrule_constraint_time_t;

typedef struct icrule_constraint
{
    icrule_constraint_logic_t logic;

    icLinkedList* time_constraints;

    icLinkedList* child_constraints;
} icrule_constraint_t;

// Actions
typedef enum {
    ACTION_DEP_INVALID = -1,
    ACTION_DEP_CAMERA = 0,
    ACTION_DEP_LIGHTING,
    ACTION_DEP_DOOR_LOCK,
    ACTION_DEP_TEMPERATURE,
    ACTION_DEP_SIREN,
    ACTION_DEP_DISPLAY,
    ACTION_DEP_ALARM,
    ACTION_DEP_AUDIO,
    ACTION_DEP_SCENE,
    ACTION_DEP_CLOUD
} icrule_action_dependency_t;

typedef enum {
    ACTION_TYPE_INVALID = -1,
    ACTION_TYPE_CAMERAID = 0,
    ACTION_TYPE_SENSORID,
    ACTION_TYPE_LIGHTID,
    ACTION_TYPE_DOOR_LOCKID,
    ACTION_TYPE_THERMOSTATID,
    ACTION_TYPE_TIME,
    ACTION_TYPE_TOUCHSCREEN_STATE,
    ACTION_TYPE_ARM_TYPE,
    ACTION_TYPE_PANIC_STATE,
    ACTION_TYPE_NETWORK_STATE,
    ACTION_TYPE_DOOR_LOCK_STATE,
    ACTION_TYPE_SENSOR_STATE,
    ACTION_TYPE_SENSOR_TYPE,
    ACTION_TYPE_MESSAGE,
    ACTION_TYPE_STRING
} icrule_action_type_t;

typedef struct icrule_action_parameter {
    char* key;
    char* value;

    bool required;

    icrule_action_type_t type;
} icrule_action_parameter_t;

typedef struct icrule_action {
    uint64_t id;

    char* desc;
    char* target;

    icrule_action_dependency_t dependency;

    icHashMap* parameters;
} icrule_action_t;

// Schedule Entries
typedef enum {
    TSTAT_MODE_INVALID = -1,
    TSTAT_MODE_HEAT = 0,
    TSTAT_MODE_COOL,
    TSTAT_MODE_BOTH
} icrule_thermostat_mode_t;

typedef struct icrule_thermostat_schedule {
    icLinkedList* ids;

    icrule_time_t time;
    icrule_thermostat_mode_t mode;

    int temperature;
} icrule_thermostat_schedule_t;

// Top-Level Rule
typedef struct icrule {
    uint64_t id;

    char* desc;

    icrule_trigger_list_t triggers;

    icLinkedList* schedule_entries;
    icLinkedList* constraints;
    icLinkedList* actions;
} icrule_t;

/** Set the directory location the action definition
 * xml files are located.
 *
 * @param dir A valid directory used to look-up action
 * definition xml files.
 */
void icrule_set_action_list_dir(const char* dir);

/** Parse the provided iControl Rule XML from memory.
 *
 * @param xml The XML data that describes the iControl
 * Rule.
 * @param rule The class pointer for iControl Rules.
 * @return Zero on success, otherwise -1 and errno
 * will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 */
int icrule_parse(const char* xml, icrule_t* rule);

/** Parse the provided iControl Rule XML from a file.
 *
 * @param filename The full path to a file that will be
 * read.
 * @param rule The class pointer for iControl Rules.
 * @return Zero on success, otherwise -1 and errno
 * will be set.
 * EINVAL: An invalid parameter was supplied.
 * ENOMEM: Memory failed to allocate internally.
 * EBADMSG: The supplied schmea contained an error that caused
 * the transcoder to fail.
 */
int icrule_parse_file(const char* filename, icrule_t* rule);

/** Release all memory allocated during the parsing
 * process for an iControl Rule.
 *
 * @param rule The class pointer for iControl Rules.
 */
void icrule_destroy(icrule_t* rule);

#endif //ZILKER_ICRULE_H
