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

#include <icBuildtime.h>
#include <stdlib.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icLog/logging.h>
#include <memory.h>
#include <icUtil/array.h>
#include <icUtil/stringUtils.h>
#include <icTime/timeUtils.h>
#include <subsystems/zigbee/zigbeeAttributeTypes.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#include "basicCluster.h"

#define LOG_TAG "basicCluster"
#define BASIC_CLUSTER_ENABLE_BIND_KEY "basicClusterEnableBind"
#define BASIC_CLUSTER_CONFIGURE_REBOOT_REASON_KEY "basicClusterConfigureRebootReason"

typedef struct
{
    ZigbeeCluster cluster;
    const BasicClusterCallbacks *callbacks;
    void *callbackContext;
} BasicCluster;

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext);

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report);

ZigbeeCluster *basicClusterCreate(const BasicClusterCallbacks *callbacks, void *callbackContext)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);
    BasicCluster *result = (BasicCluster *) calloc(1, sizeof(BasicCluster));

    result->cluster.clusterId = BASIC_CLUSTER_ID;
    result->cluster.configureCluster = configureCluster;
    result->callbacks = callbacks;
    result->callbackContext = callbackContext;
    result->cluster.handleAttributeReport = handleAttributeReport;

    return (ZigbeeCluster *) result;
}

void basicClusterSetConfigureRebootReason(const DeviceConfigurationContext *deviceConfigurationContext, bool configure)
{
    addBoolConfigurationMetadata(deviceConfigurationContext->configurationMetadata, BASIC_CLUSTER_CONFIGURE_REBOOT_REASON_KEY, configure);
}

bool basicClusterRebootDevice(uint64_t eui64, uint8_t endpointId, uint16_t mfgId)
{
    icLogDebug(LOG_TAG, "%s: %016"PRIx64" endpoint 0x%02"PRIx8, __FUNCTION__, eui64, endpointId);
    /*
     *  We have to fire and forget this command as the device rebooting pulls the rug out on ZigbeeCore,
     *  leading it to believe the transmission of the command failed.
     */
    (void)zigbeeSubsystemSendMfgCommand(eui64, endpointId, BASIC_CLUSTER_ID, true, BASIC_REBOOT_DEVICE_MFG_COMMAND_ID,
                                        mfgId, NULL, 0);
    return true;
}

static bool configureCluster(ZigbeeCluster *ctx, const DeviceConfigurationContext *configContext)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    bool result = true;
    bool configuredReporting = false;

    // Check whether to configure reboot reason, default to false
    if (getBoolConfigurationMetadata(configContext->configurationMetadata, BASIC_CLUSTER_CONFIGURE_REBOOT_REASON_KEY, false))
    {
        // configure attribute reporting on reboot reason
        zhalAttributeReportingConfig rebootReasonConfigs[1];
        uint8_t numConfigs = 1;
        memset(&rebootReasonConfigs[0], 0, sizeof(zhalAttributeReportingConfig));
        rebootReasonConfigs[0].attributeInfo.id = COMCAST_BASIC_CLUSTER_MFG_SPECIFIC_MODEM_REBOOT_REASON_ATTRIBUTE_ID;
        rebootReasonConfigs[0].attributeInfo.type = ZCL_ENUM8_ATTRIBUTE_TYPE;
        rebootReasonConfigs[0].minInterval = 1;
        rebootReasonConfigs[0].maxInterval = 3600;
        rebootReasonConfigs[0].reportableChange = 1;

        if (zigbeeSubsystemAttributesSetReportingMfgSpecific(configContext->eui64,
                                                             configContext->endpointId,
                                                             BASIC_CLUSTER_ID,
                                                             COMCAST_MFG_ID,
                                                             rebootReasonConfigs,
                                                             numConfigs) != 0)
        {
            icLogError(LOG_TAG, "%s: failed to set reporting for reboot reason", __FUNCTION__);
            result = false;
        }

        // Record that we configured reporting
        configuredReporting = true;
    }

    // Only worry about binding if we have configured some reporting
    if (configuredReporting == true)
    {
        // If the property is set to false we skip, otherwise accept its value or the default of true if nothing was set
        if (getBoolConfigurationMetadata(configContext->configurationMetadata, BASIC_CLUSTER_ENABLE_BIND_KEY, true))
        {
            if (zigbeeSubsystemBindingSet(configContext->eui64, configContext->endpointId, BASIC_CLUSTER_ID) != 0)
            {
                icLogError(LOG_TAG, "%s: failed to bind basic cluster", __FUNCTION__);
                result = false;
            }
        }
    }

    return result;
}

static bool handleAttributeReport(ZigbeeCluster *ctx, ReceivedAttributeReport *report)
{
    icLogTrace(LOG_TAG, "%s", __FUNCTION__);

    BasicCluster *cluster = (BasicCluster *) ctx;

    sbZigbeeIOContext *zio = zigbeeIOInit(report->reportData, report->reportDataLen, ZIO_READ);
    uint16_t attributeId = zigbeeIOGetUint16(zio);
    uint8_t attributeType = zigbeeIOGetUint8(zio);
    uint8_t attributeValue = zigbeeIOGetUint8(zio);

    icLogDebug(LOG_TAG, "%s: 0x%15"PRIx64" attributeId %"PRIu16" attributeType %"PRIu8" attributeValue %"PRIu16, __FUNCTION__, report->eui64, attributeId, attributeType, attributeValue);

    if (report->mfgId == COMCAST_MFG_ID &&
        attributeId == COMCAST_BASIC_CLUSTER_MFG_SPECIFIC_MODEM_REBOOT_REASON_ATTRIBUTE_ID)
    {
        if (cluster->callbacks->rebootReasonChanged != NULL)
        {
            if (attributeValue < ARRAY_LENGTH(basicClusterRebootReasonLabels))
            {
                cluster->callbacks->rebootReasonChanged(cluster->callbackContext,
                                                        report->eui64,
                                                        report->sourceEndpoint,
                                                        (basicClusterRebootReason)attributeValue);
            }
            else
            {
                icLogError(LOG_TAG, "%s unsupported reboot reason %"PRIu8, __FUNCTION__, attributeValue);
            }
        }
    }
    return true;
}

int basicClusterResetRebootReason(uint64_t eui64, uint8_t endPointId)
{
    return zigbeeSubsystemWriteNumberMfgSpecific(eui64, endPointId, BASIC_CLUSTER_ID, COMCAST_MFG_ID, true, COMCAST_BASIC_CLUSTER_MFG_SPECIFIC_MODEM_REBOOT_REASON_ATTRIBUTE_ID,
                                              ZCL_ENUM8_ATTRIBUTE_TYPE, REBOOT_REASON_DEFAULT, 1);
}

#endif //CONFIG_SERVICE_DEVICE_ZIGBEE
