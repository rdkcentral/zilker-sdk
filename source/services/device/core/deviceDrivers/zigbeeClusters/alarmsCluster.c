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
// Created by tlea on 2/15/19.
//


#include <icBuildtime.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <memory.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "alarmsCluster.h"

#define LOG_TAG "alarmsCluster"

#define ALARMS_CLUSTER_DISABLE_BIND_KEY "alarmsClusterDisableBind"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);

typedef struct
{
    ZigbeeCluster cluster;

    const AlarmsClusterCallbacks *callbacks;
    const void *callbackContext;
} AlarmsCluster;

ZigbeeCluster *alarmsClusterCreate(const AlarmsClusterCallbacks *callbacks, const void *callbackContext)
{
    AlarmsCluster *result = (AlarmsCluster *) calloc(1, sizeof(AlarmsCluster));

    result->cluster.clusterId = ALARMS_CLUSTER_ID;

    result->cluster.configureCluster = configureCluster;
    result->cluster.handleClusterCommand = handleClusterCommand;
    // Want to configure first, so that we bind and get any alarms that happen right after we
    // configure any alarm masks
    result->cluster.priority = CLUSTER_PRIORITY_HIGHEST;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

void alarmsClusterSetBindingEnabled(const DeviceConfigurationContext *deviceConfigurationContext, bool bind)
{
    addBoolConfigurationMetadata(deviceConfigurationContext->configurationMetadata,
                                 ALARMS_CLUSTER_DISABLE_BIND_KEY,
                                 bind);
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    //If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, ALARMS_CLUSTER_DISABLE_BIND_KEY, true))
    {
        if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, ALARMS_CLUSTER_ID) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to bind alarms cluster", __FUNCTION__);
            return false;
        }
    }

    return true;
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    AlarmsCluster *alarmsCluster = (AlarmsCluster *) ctx;

    switch (command->commandId)
    {
        case ALARMS_ALARM_COMMAND_ID:
        {
            if (alarmsCluster->callbacks->alarmReceived != NULL)
            {
                ZigbeeAlarmTableEntry entry;
                memset(&entry, 0, sizeof(ZigbeeAlarmTableEntry));

                entry.alarmCode = command->commandData[0];
                entry.clusterId = command->commandData[1] + (command->commandData[2] << 8);

                alarmsCluster->callbacks->alarmReceived(command->eui64,
                                                        command->sourceEndpoint,
                                                        &entry,
                                                        1, //TODO handle multiple alarms at the same time
                                                        alarmsCluster->callbackContext);
            }
            result = true;
            break;
        }

        case ALARMS_CLEAR_ALARM_COMMAND_ID:
        {
            if (alarmsCluster->callbacks->alarmCleared != NULL)
            {
                ZigbeeAlarmTableEntry entry;
                memset(&entry, 0, sizeof(ZigbeeAlarmTableEntry));

                entry.alarmCode = command->commandData[0];
                entry.clusterId = command->commandData[1] + (command->commandData[2] << 8);

                alarmsCluster->callbacks->alarmCleared(command->eui64,
                                                       command->sourceEndpoint,
                                                       &entry,
                                                       1, //TODO handle multiple alarms at the same time
                                                       alarmsCluster->callbackContext);
            }
            result = true;
            break;
        }

        default:
            icLogError(LOG_TAG, "%s: unexpected command id 0x%02x", __FUNCTION__, command->commandId);
            break;

    }

    return result;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE