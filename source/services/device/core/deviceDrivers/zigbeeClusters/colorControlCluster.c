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
// Created by tlea on 2/19/19.
//


#include <icBuildtime.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <memory.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <stdio.h>
#include <commonDeviceDefs.h>
#include <device/icDeviceResource.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "colorControlCluster.h"

#define LOG_TAG "colorControlCluster"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

typedef struct
{
    ZigbeeCluster cluster;

    const ColorControlClusterCallbacks *callbacks;
    const void *callbackContext;
} ColorControlCluster;

ZigbeeCluster *colorControlClusterCreate(const ColorControlClusterCallbacks *callbacks, void *callbackContext)
{
    ColorControlCluster *result = (ColorControlCluster *) calloc(1, sizeof(ColorControlCluster));

    result->cluster.clusterId = COLOR_CONTROL_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool colorControlClusterGetX(uint64_t eui64, uint8_t endpointId, uint16_t *x)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, COLOR_CONTROL_CLUSTER_ID, true,
                                  COLOR_CONTROL_CURRENTX_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read current x attribute value", __FUNCTION__);
    }
    else
    {
        *x = (uint16_t) (val & 0xffff);
        result = true;
    }

    return result;
}

bool colorControlClusterGetY(uint64_t eui64, uint8_t endpointId, uint16_t *y)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, COLOR_CONTROL_CLUSTER_ID, true,
                                  COLOR_CONTROL_CURRENTY_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read current y attribute value", __FUNCTION__);
    }
    else
    {
        *y = (uint16_t) (val & 0xffff);
        result = true;
    }

    return result;
}

bool colorControlClusterMoveToColor(uint64_t eui64, uint8_t endpointId, uint16_t x, uint16_t y)
{
    uint8_t msg[6];

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    msg[0] = (uint8_t) (x & 0xff);
    msg[1] = (uint8_t) (x >> 8);
    msg[2] = (uint8_t) (y & 0xff);
    msg[3] = (uint8_t) (y >> 8);
#else
    msg[1] = (uint8_t) (x & 0xff);
    msg[0] = (uint8_t) (x >> 8);
    msg[3] = (uint8_t) (y & 0xff);
    msg[2] = (uint8_t) (y >> 8);
#endif

    msg[4] = 0; //transition time byte 1
    msg[5] = 0; //transition time byte 2

    return (zigbeeSubsystemSendCommand(eui64, endpointId, COLOR_CONTROL_CLUSTER_ID, true,
                               COLOR_CONTROL_MOVE_TO_COLOR_COMMAND_ID, msg, 6) == 0);
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    zhalAttributeReportingConfig colorConfigs[2];
    uint8_t numConfigs = 2;
    memset(&colorConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
    colorConfigs[0].attributeInfo.id = COLOR_CONTROL_CURRENTX_ATTRIBUTE_ID;
    colorConfigs[0].attributeInfo.type = ZCL_INT16U_ATTRIBUTE_TYPE;
    colorConfigs[0].minInterval = 1;
    colorConfigs[0].maxInterval = REPORTING_INTERVAL_MAX;
    colorConfigs[0].reportableChange = 1;
    colorConfigs[1].attributeInfo.id = COLOR_CONTROL_CURRENTY_ATTRIBUTE_ID;
    colorConfigs[1].attributeInfo.type = ZCL_INT16U_ATTRIBUTE_TYPE;
    colorConfigs[1].minInterval = 1;
    colorConfigs[1].maxInterval = REPORTING_INTERVAL_MAX;
    colorConfigs[1].reportableChange = 1;

    if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, COLOR_CONTROL_CLUSTER_ID) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to bind color control", __FUNCTION__);
        result = false;
    }
    else if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                   configContext->endpointId,
                                                   COLOR_CONTROL_CLUSTER_ID,
                                                   colorConfigs,
                                                   numConfigs) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting for color control", __FUNCTION__);
        result = false;
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ColorControlCluster *colorControlCluster = (ColorControlCluster *) ctx;

    if (report->reportDataLen == 5)
    {
        uint16_t attributeId = report->reportData[0] + (report->reportData[1] << 8);
        uint16_t val = report->reportData[3] + (report->reportData[4] << 8);

        if (attributeId == COLOR_CONTROL_CURRENTX_ATTRIBUTE_ID &&
            colorControlCluster->callbacks->currentXChanged != NULL)
        {
            colorControlCluster->callbacks->currentXChanged(report->eui64,
                                                            report->sourceEndpoint,
                                                            val,
                                                            colorControlCluster->callbackContext);
        }
        else if (attributeId == COLOR_CONTROL_CURRENTY_ATTRIBUTE_ID &&
                 colorControlCluster->callbacks->currentYChanged != NULL)
        {
            colorControlCluster->callbacks->currentYChanged(report->eui64,
                                                            report->sourceEndpoint,
                                                            val,
                                                            colorControlCluster->callbackContext);
        }
    }

    return true;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE