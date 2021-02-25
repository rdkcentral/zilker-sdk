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

#include "electricalMeasurementCluster.h"

#define LOG_TAG "electricalMeasurementCluster"

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

typedef struct
{
    ZigbeeCluster cluster;

    const ElectricalMeasurementClusterCallbacks *callbacks;
    const void *callbackContext;
} ElectricalMeasurementCluster;

ZigbeeCluster *electricalMeasurementClusterCreate(const ElectricalMeasurementClusterCallbacks *callbacks, void *callbackContext)
{
    ElectricalMeasurementCluster *result = (ElectricalMeasurementCluster *) calloc(1, sizeof(ElectricalMeasurementCluster));

    result->cluster.clusterId = ELECTRICAL_MEASUREMENT_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->cluster.handleAttributeReport = handleAttributeReport;

    result->callbacks = callbacks;
    result->callbackContext = callbackContext;

    return (ZigbeeCluster *) result;
}

bool electricalMeasurementClusterGetActivePower(uint64_t eui64, uint8_t endpointId, int16_t *watts)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, ELECTRICAL_MEASUREMENT_CLUSTER_ID, true, ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read active power attribute value", __FUNCTION__);
    }
    else
    {
        *watts = (int16_t) (val & 0xffff);
        result = true;
    }

    return result;
}

bool electricalMeasurementClusterGetAcPowerDivisor(uint64_t eui64, uint8_t endpointId, uint16_t *divisor)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, ELECTRICAL_MEASUREMENT_CLUSTER_ID, true, ELECTRICAL_MEASUREMENT_AC_DIVISOR_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read ac divisor attribute value", __FUNCTION__);
    }
    else
    {
        *divisor = (uint16_t) (val & 0xffff);
        result = true;
    }

    return result;
}

bool electricalMeasurementClusterGetAcPowerMultiplier(uint64_t eui64, uint8_t endpointId, uint16_t *multiplier)
{
    bool result = false;

    uint64_t val = 0;

    if (zigbeeSubsystemReadNumber(eui64, endpointId, ELECTRICAL_MEASUREMENT_CLUSTER_ID, true, ELECTRICAL_MEASUREMENT_AC_MULTIPLIER_ATTRIBUTE_ID, &val) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to read ac multiplier attribute value", __FUNCTION__);
    }
    else
    {
        *multiplier = (uint16_t) (val & 0xffff);
        result = true;
    }

    return result;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    uint16_t multiplier;
    uint16_t divisor;

    if(electricalMeasurementClusterGetAcPowerMultiplier(configContext->eui64, configContext->endpointId, &multiplier) == false)
    {
        return false;
    }

    if(electricalMeasurementClusterGetAcPowerDivisor(configContext->eui64, configContext->endpointId, &divisor) == false)
    {
        return false;
    }

    zhalAttributeReportingConfig configs[1];
    uint8_t numConfigs = 1;
    memset(&configs[0], 0, sizeof(zhalAttributeReportingConfig));
    configs[0].attributeInfo.id = ELECTRICAL_MEASUREMENT_ACTIVE_POWER_ATTRIBUTE_ID;
    configs[0].attributeInfo.type = ZCL_INT16S_ATTRIBUTE_TYPE;
    configs[0].minInterval = 1;
    configs[0].maxInterval = REPORTING_INTERVAL_MAX;
    configs[0].reportableChange = divisor / multiplier; //by default, report 1 watt changes

    if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, ELECTRICAL_MEASUREMENT_CLUSTER_ID) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to bind electrical measurement", __FUNCTION__);
        result = false;
    }
    else if (zigbeeSubsystemAttributesSetReporting(configContext->eui64,
                                                   configContext->endpointId,
                                                   ELECTRICAL_MEASUREMENT_CLUSTER_ID,
                                                   configs,
                                                   numConfigs) != 0)
    {
        icLogError(LOG_TAG, "%s: failed to set reporting for electrical measurement", __FUNCTION__);
        result = false;
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ElectricalMeasurementCluster *electricalMeasurementCluster = (ElectricalMeasurementCluster *) ctx;

    if (electricalMeasurementCluster->callbacks->activePowerChanged != NULL)
    {
        char *uuid = zigbeeSubsystemEui64ToId(report->eui64);
        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, report->sourceEndpoint);

        if (report->reportDataLen == 5)
        {
            int16_t val = report->reportData[3] + (report->reportData[4] << 8);
            electricalMeasurementCluster->callbacks->activePowerChanged(report->eui64,
                                                         report->sourceEndpoint,
                                                         val,
                                                         electricalMeasurementCluster->callbackContext);
        }

        free(uuid);
    }

    return true;
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE