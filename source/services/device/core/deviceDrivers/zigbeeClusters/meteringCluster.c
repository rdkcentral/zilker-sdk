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

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "meteringCluster.h"

#define LOG_TAG "meteringCluster"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

typedef struct
{
    ZigbeeCluster cluster;

    const MeteringClusterCallbacks *callbacks;
    const void *callbackContext;
} MeteringCluster;

ZigbeeCluster *meteringClusterCreate(const MeteringClusterCallbacks *callbacks, void *callbackContext)
{
    MeteringCluster *result = (MeteringCluster *) calloc(1, sizeof(MeteringCluster));

    result->cluster.clusterId = METERING_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool meteringClusterGetInstantaneousDemand(uint64_t eui64, uint8_t endpointId, int32_t *demand)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, METERING_CLUSTER_ID, true,
                                  METERING_INSTANTANEOUS_DEMAND_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read instantaneous power attribute value", __FUNCTION__);
    }
    else
    {
        *demand = (int32_t) val; //24 bit signed
        result = true;
    }

    return result;
}

bool meteringClusterGetDivisor(uint64_t eui64, uint8_t endpointId, uint32_t *divisor)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, METERING_CLUSTER_ID, true, METERING_DIVISOR_ATTRIBUTE_ID, &val) !=
        0)
    {
        icLogError(LOG_TAG, "%s: failed to read divisor attribute value", __FUNCTION__);
    }
    else
    {
        *divisor = (uint32_t) (val & 0xffffff); //24 bit unsigned
        result = true;
    }

    return result;
}

bool meteringClusterGetMultiplier(uint64_t eui64, uint8_t endpointId, uint32_t *multiplier)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, METERING_CLUSTER_ID, true, METERING_MULTIPLIER_ATTRIBUTE_ID,
                                  &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read multiplier attribute value", __FUNCTION__);
    }
    else
    {
        *multiplier = (uint32_t) (val & 0xffffff); //24 bit unsigned
        result = true;
    }

    return result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uint32_t multiplier;
    uint32_t divisor;

    if (meteringClusterGetMultiplier(configContext->eui64, configContext->endpointId, &multiplier) == false)
    {
        return false;
    }

    if (meteringClusterGetDivisor(configContext->eui64, configContext->endpointId, &divisor) == false)
    {
        return false;
    }

    zhalAttributeReportingConfig configs[1];
    uint8_t numConfigs = 1;
    memset(&configs[0], 0, sizeof(zhalAttributeReportingConfig));
    configs[0].attributeInfo.id = METERING_INSTANTANEOUS_DEMAND_ATTRIBUTE_ID;
    configs[0].attributeInfo.type = ZCL_INT24S_ATTRIBUTE_TYPE;
    configs[0].minInterval = 1;
    configs[0].maxInterval = REPORTING_INTERVAL_MAX;
    configs[0].reportableChange =
            divisor / multiplier / 1000; //by default, report 1 watt changes. metering units are kilowats

    if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, METERING_CLUSTER_ID) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to bind metering", __FUNCTION__);
        result = false;
    }
    else if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                   configContext->endpointId,
                                                   METERING_CLUSTER_ID,
                                                   configs,
                                                   numConfigs) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting for metering", __FUNCTION__);
        result = false;
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    MeteringCluster *meteringCluster = (MeteringCluster *) ctx;

    if (meteringCluster->callbacks->instantaneousDemandChanged != NULL)
    {
        if (report->reportDataLen == 6)
        {
            int32_t val = report->reportData[3] + (report->reportData[4] << 8) + (report->reportData[5] << 16);
            icLogDebug(LOG_TAG, "%s: instantaneous power now %"PRId32" kW", __FUNCTION__, val);

            meteringCluster->callbacks->instantaneousDemandChanged(report->eui64,
                                                                   report->sourceEndpoint,
                                                                   val,
                                                                   meteringCluster->callbackContext);
        }
    }

    return true;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE