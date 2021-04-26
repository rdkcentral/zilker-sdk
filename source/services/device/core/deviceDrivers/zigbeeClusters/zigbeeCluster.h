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
// Created by tlea on 2/13/19.
//

#ifndef ZILKER_ZIGBEECLUSTER_H
#define ZILKER_ZIGBEECLUSTER_H

#include <icBuildtime.h>
#include <stdint.h>
#include <stdbool.h>
#include <zhal/zhal.h>
#include <deviceDriver.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

typedef enum
{
    CLUSTER_PRIORITY_DEFAULT = 0,
    CLUSTER_PRIORITY_HIGHEST
} ClusterPriority;

typedef struct ZigbeeCluster ZigbeeCluster;
typedef struct
{
    uint8_t alarmCode;
    uint16_t clusterId;
    uint32_t timeStamp;
} ZigbeeAlarmTableEntry;

typedef struct
{
    uint64_t eui64;
    uint8_t endpointId;
    const DeviceDescriptor *deviceDescriptor;
    const IcDiscoveredDeviceDetails *discoveredDeviceDetails;
    icStringHashMap *configurationMetadata;
} DeviceConfigurationContext;

struct ZigbeeCluster
{
    uint16_t clusterId;
    ClusterPriority priority;

    /**
     * Perform cluster configuration tasks, such as binding and attribute reporting setup
     * @param ctx This cluster instance
     * @param eui64
     * @param endpointId
     */
    bool (* configureCluster)(ZigbeeCluster *ctx, const DeviceConfigurationContext *deviceConfigurationContext);

    /**
     * Handle a received cluster command
     * @param ctx This cluster instance
     * @param command
     */
    bool (* handleClusterCommand)(ZigbeeCluster *ctx, ReceivedClusterCommand* command);

    /**
     * Handle an attribute report
     * @param ctx
     * @param report
     */
    bool (* handleAttributeReport)(ZigbeeCluster *ctx, ReceivedAttributeReport* report);

    /**
     * Handle an alarm
     * @param ctx
     * @param eui64
     * @param endpointId
     * @param alarmTableEntry
     */
    bool (* handleAlarm)(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry);


    /**
     * Handle an alarm cleared
     * @param ctx
     * @param eui64
     * @param endpointId
     * @param alarmTableEntry
     */
    bool (* handleAlarmCleared)(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId, const ZigbeeAlarmTableEntry *alarmTableEntry);

    /**
     * Special function to allow any cluster to do something during poll control checkin
     * @param ctx
     * @param eui64
     * @param endpointId
     */
    void (* handlePollControlCheckin)(ZigbeeCluster *ctx, uint64_t eui64, uint8_t endpointId);

    /**
     * Destroy this cluster instance
     * @param ctx
     */
    void (* destroy)(const ZigbeeCluster *ctx);
};

/**
 * Add a boolean value to configuration metadata
 *
 * @param configurationMetadata the metadata to write to
 * @param key the key to add
 * @param value the value to add
 */
void addBoolConfigurationMetadata(icStringHashMap *configurationMetadata, const char *key, bool value);

/**
 * Get a boolean value from configuration metadata
 * @param configurationMetadata the metadata to get from
 * @param defaultValue the default value if its not found
 * @return the value in the map, or defaultValue if not found
 */
bool getBoolConfigurationMetadata(icStringHashMap *configurationMetadata, const char *key, bool defaultValue);

/**
 * Add a number value to configuration metadata
 *
 * @param configurationMetadata the metadata to write to
 * @param key the key to add
 * @param value the value to add
 */
void addNumberConfigurationMetadata(icStringHashMap *configurationMetadata, const char *key, uint64_t value);

/**
 * Get a number value from configuration metadata
 *
 * @param configurationMetadata the metadata to get from
 * @param defaultValue the default value if its not found
 * @return the value in the map, or defaultValue if not found
 */
uint64_t getNumberConfigurationMetadata(icStringHashMap *configurationMetadata, const char *key, uint64_t defaultValue);

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE

#endif //ZILKER_ZIGBEECLUSTER_H
