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
// Created by mkoch201 on 3/18/19.
//

#include <icBuildtime.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <memory.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "bridgeCluster.h"

#define LOG_TAG "bridgeCluster"

// Alarm codes
#define BRIDGE_TAMPER_ALARM_CODE 0x00

#define BRIDGE_REFRESH             0x00
#define BRIDGE_RESET               0x01
#define BRIDGE_START_CONFIGURATION 0x02
#define BRIDGE_STOP_CONFIGURATION  0x03

#define BRIDGE_REFRESH_REQUESTED 0x00
#define BRIDGE_REFRESH_COMPLETED 0x01

#define BRIDGE_ALARM_MASK_ATTRIBUTE_ID 0x00

#define BRIDGE_TAMPER_ALARM 0x01

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command);
static bool handleAlarm(ZigbeeCluster *ctx,
                        uint64_t eui64,
                        uint8_t endpointId,
                        const ZigbeeAlarmTableEntry *alarmTableEntry);
static bool handleAlarmCleared(ZigbeeCluster *ctx,
                               uint64_t eui64,
                               uint8_t endpointId,
                               const ZigbeeAlarmTableEntry *alarmTableEntry);

typedef struct
{
    ZigbeeCluster cluster;

    const BridgeClusterCallbacks *callbacks;
    const void *callbackContext;
} BridgeCluster;

ZigbeeCluster *bridgeClusterCreate(const BridgeClusterCallbacks *callbacks, const void *callbackContext)
{
    BridgeCluster *result = (BridgeCluster *) calloc(1, sizeof(BridgeCluster));

    result->cluster.clusterId = BRIDGE_CLUSTER_ID;

    result->cluster.configureCluster = configureCluster;
    result->cluster.handleClusterCommand = handleClusterCommand;
    result->cluster.handleAlarm = handleAlarm;
    result->cluster.handleAlarmCleared = handleAlarmCleared;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool bridgeClusterRefresh(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(eui64,
                                         endpointId,
                                         BRIDGE_CLUSTER_ID,
                                         true,
                                         BRIDGE_REFRESH,
                                         ICONTROL_MFG_ID,
                                         NULL,
                                         0) == 0;
}

bool bridgeClusterStartConfiguration(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(eui64,
                                         endpointId,
                                         BRIDGE_CLUSTER_ID,
                                         true,
                                         BRIDGE_START_CONFIGURATION,
                                         ICONTROL_MFG_ID,
                                         NULL,
                                         0) == 0;
}

bool bridgeClusterStopConfiguration(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(eui64,
                                         endpointId,
                                         BRIDGE_CLUSTER_ID,
                                         true,
                                         BRIDGE_STOP_CONFIGURATION,
                                         ICONTROL_MFG_ID,
                                         NULL,
                                         0) == 0;
}

bool bridgeClusterReset(uint64_t eui64, uint8_t endpointId)
{
    return zigbeeSubsystemSendMfgCommand(eui64,
                                         endpointId,
                                         BRIDGE_CLUSTER_ID,
                                         true,
                                         BRIDGE_RESET,
                                         ICONTROL_MFG_ID,
                                         NULL,
                                         0) == 0;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, BRIDGE_CLUSTER_ID) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to bind bridge cluster", __FUNCTION__);
        return false;
    }

    // Set up to get a tamper alarm
    if (zigbeeSubsystemWriteNumberMfgSpecific(configContext->eui64,
                                              configContext->endpointId,
                                              BRIDGE_CLUSTER_ID,
                                              ICONTROL_MFG_ID,
                                              true,
                                              BRIDGE_ALARM_MASK_ATTRIBUTE_ID,
                                              ZCL_BITMAP8_ATTRIBUTE_TYPE,
                                              BRIDGE_TAMPER_ALARM,
                                              1) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set battery alarm mask", __FUNCTION__);
        return false;
    }

    return true;
}

static bool handleClusterCommand(ZigbeeCluster *ctx, ReceivedClusterCommand *command)
{
    bool result = false;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    BridgeCluster *bridgeCluster = (BridgeCluster *) ctx;

    switch (command->commandId)
    {
        case BRIDGE_REFRESH_REQUESTED:
        {
            if (bridgeCluster->callbacks->refreshRequested != NULL)
            {

                bridgeCluster->callbacks->refreshRequested(command->eui64,
                                                           command->sourceEndpoint,
                                                           bridgeCluster->callbackContext);
            }
            result = true;
            break;
        }

        case BRIDGE_REFRESH_COMPLETED:
        {
            if (bridgeCluster->callbacks->refreshCompleted != NULL)
            {

                bridgeCluster->callbacks->refreshCompleted(command->eui64,
                                                           command->sourceEndpoint,
                                                           bridgeCluster->callbackContext);
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

static bool handleAlarm(ZigbeeCluster *ctx,
                        uint64_t eui64,
                        uint8_t endpointId,
                        const ZigbeeAlarmTableEntry *alarmTableEntry)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    BridgeCluster *cluster = (BridgeCluster *) ctx;

    switch (alarmTableEntry->alarmCode)
    {
        case BRIDGE_TAMPER_ALARM_CODE:
            icLogWarn(LOG_TAG, "%s: Tamper alarm", __FUNCTION__);
            if (cluster->callbacks->bridgeTamperStatusChanged != NULL)
            {
                cluster->callbacks->bridgeTamperStatusChanged(eui64, endpointId, cluster->callbackContext, true);
            }
            break;

        default:
            icLogWarn(LOG_TAG,
                      "%s: Unsupported bridge cluster alarm code 0x%02x",
                      __FUNCTION__,
                      alarmTableEntry->alarmCode);
            result = false;
            break;
    }

    return result;
}

static bool handleAlarmCleared(ZigbeeCluster *ctx,
                               uint64_t eui64,
                               uint8_t endpointId,
                               const ZigbeeAlarmTableEntry *alarmTableEntry)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    BridgeCluster *cluster = (BridgeCluster *) ctx;

    switch (alarmTableEntry->alarmCode)
    {
        case BRIDGE_TAMPER_ALARM_CODE:
            icLogWarn(LOG_TAG, "%s: Tamper alarm cleared", __FUNCTION__);
            if (cluster->callbacks->bridgeTamperStatusChanged != NULL)
            {
                cluster->callbacks->bridgeTamperStatusChanged(eui64, endpointId, cluster->callbackContext, false);
            }
            break;

        default:
            icLogWarn(LOG_TAG,
                      "%s: Unsupported bridge cluster alarm code 0x%02x",
                      __FUNCTION__,
                      alarmTableEntry->alarmCode);
            result = false;
            break;
    }

    return result;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE