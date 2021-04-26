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
// Created by Boyd, Weston on 5/15/18.
//

#ifndef ZILKER_ICRULE_TRIGGERS_H
#define ZILKER_ICRULE_TRIGGERS_H

#include <icrule/icrule_time.h>

// Triggers - Zone
typedef enum {
    TRIGGER_SENSOR_STATE_OPEN = 0,
    TRIGGER_SENSOR_STATE_CLOSED,
    TRIGGER_SENSOR_STATE_EITHER,
    TRIGGER_SENSOR_STATE_TROUBLE
} icrule_trigger_sensor_state_t;

typedef enum {
    TRIGGER_SENSOR_TYPE_INVALID = -1,
    TRIGGER_SENSOR_TYPE_ALL_SENSORS = 0,
    TRIGGER_SENSOR_TYPE_NONMOTION_SENSORS,
    TRIGGER_SENSOR_TYPE_DOOR,
    TRIGGER_SENSOR_TYPE_WINDOW,
    TRIGGER_SENSOR_TYPE_MOTION,
    TRIGGER_SENSOR_TYPE_GLASS_BREAK,
    TRIGGER_SENSOR_TYPE_SMOKE,
    TRIGGER_SENSOR_TYPE_CO,
    TRIGGER_SENSOR_TYPE_WATER,
    TRIGGER_SENSOR_TYPE_DRY_CONTACT,
    TRIGGER_SENSOR_TYPE_INERTIA,
    TRIGGER_SENSOR_TYPE_LIGHTING,
    TRIGGER_SENSOR_TYPE_TEMPERATURE,
    TRIGGER_SENSOR_TYPE_DOOR_LOCK
} icrule_trigger_sensor_type_t;

typedef struct icrule_trigger_sensor {
    // This can refer to an a sensor id, or a camera id for camera motion.  camera ids are not numeric
    const char* id;

    icrule_trigger_sensor_state_t state;
    icrule_trigger_sensor_type_t type;
} icrule_trigger_sensor_t;

// Trigger - Touchscreen
typedef enum {
    TRIGGER_TOUCHSCREEN_STATE_INVALID = -1,
    TRIGGER_TOUCHSCREEN_STATE_TROUBLE,
    TRIGGER_TOUCHSCREEN_STATE_POWER_LOST,
} icrule_trigger_touchscreen_state_t;

typedef struct icrule_trigger_touchscreen {
    icrule_trigger_touchscreen_state_t state;
} icrule_trigger_touchscreen_t;

// Trigger - Lighting
typedef struct icrule_trigger_light {
    const char* id;

    bool enabled;
} icrule_trigger_light_t;

// Trigger - Door lock
typedef enum {
    TRIGGER_DOORLOCK_STATE_INVALID = -1,
    TRIGGER_DOORLOCK_STATE_LOCKED = 0,
    TRIGGER_DOORLOCK_STATE_UNLOCKED,
    TRIGGER_DOORLOCK_STATE_TROUBLE,
} icrule_trigger_doorlock_state_t;

typedef struct icrule_trigger_doorlock {
    const char* id;

    icrule_trigger_doorlock_state_t state;
} icrule_trigger_doorlock_t;

// Trigger - Thermostat
/** The upper and lower bounds that are valid
 * for a thermostate.
 *
 * A value of -1000 forces the trigger to
 * ignore that bound limit.
 */
typedef struct icrule_thermostat_bounds {
    int upper;
    int lower;
} icrule_thermostat_bounds_t;

typedef struct icrule_trigger_thermostat {
    const char* id;

    bool trouble;

    icrule_thermostat_bounds_t bounds;
} icrule_trigger_thermostat_t;

// Trigger - Time
typedef struct icrule_trigger_time {
    /** Time to fire the time trigger */
    icrule_time_t when;

    /** Only defined if either repeat_interval is
     * non-zero, or randomize is enabled.
     */
    icrule_time_t end;

    /** Enable repeating the time trigger
     * every f(x) seconds.
     *
     * States:
     * interval > 0: Repeat every f(x) seconds.
     * interval = 0: Disable the repeater.
     * interval < 0: Randomize the time interval.
     */
    int repeat_interval;

    /** Randomize when the time trigger fires to an
     * interval between 'when' and 'end'.
     *
     * Note: This means the trigger may not fire at 'when'
     * but at any time from 'when' to 'end'.
     * but at any time from 'when' to 'end'.
     */
    bool randomize;
} icrule_trigger_time_t;

// Trigger - Cloud
typedef enum {
    ICRULE_CLOUD_CMP_TYPE_INVALID = -1,
    ICRULE_CLOUD_CMP_TYPE_SIMPLE = 0,
    ICRULE_CLOUD_CMP_TYPE_COMPLEX,
} icrule_cloud_comparison_type_t;

typedef enum {
    ICRULE_CLOUD_OPERATOR_INVALID = -1,
    ICRULE_CLOUD_OPERATOR_EQ = 0,
    ICRULE_CLOUD_OPERATOR_LT,
    ICRULE_CLOUD_OPERATOR_LE,
    ICRULE_CLOUD_OPERATOR_GT,
    ICRULE_CLOUD_OPERATOR_GE,
} icrule_cloud_operator_t;

typedef struct icrule_cloud_comparison_simple {
    const char* event_name;
} icrule_cloud_comparison_simple_t;

typedef struct icrule_cloud_comparison_complex {
    const char* attribute_name;
    icrule_cloud_operator_t operator;
    double value;
} icrule_cloud_comparison_complex_t;

typedef struct icrule_trigger_cloud {
    const char* id;

    icrule_cloud_comparison_type_t comparison_type;

    union {
        icrule_cloud_comparison_simple_t simple;
        icrule_cloud_comparison_complex_t complex;
    } comparison;
} icrule_trigger_cloud_t;

// Trigger - ZigbeeComm
typedef enum {
    TRIGGER_ZIGBEE_COMM_STATE_INVALID = -1,
    TRIGGER_ZIGBEE_COMM_STATE_LOST = 0,
    TRIGGER_ZIGBEE_COMM_STATE_RESTORED
} icrule_trigger_zigbee_comm_state_t;
typedef struct icrule_zigbee_comm {
    const char* id;
    icrule_trigger_zigbee_comm_state_t state;
} icrule_trigger_zigbee_comm_t;

#endif //ZILKER_ICRULE_TRIGGERS_H
