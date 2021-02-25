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

#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icUtil/array.h>
#include <jsonHelper/jsonHelper.h>
#include <zigbeeClusters/iasZoneCluster.h>
#include <zigbeeClusters/helpers/iasZoneHelper.h>
#include <commonDeviceDefs.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <icLog/logging.h>
#include <errno.h>
#include <memory.h>
#include <zigbeeClusters/powerConfigurationCluster.h>
#include <zigbeeClusters/pollControlCluster.h>
#include <zigbeeClusters/helpers/comcastBatterySavingHelper.h>
#include "zigbeeSensorDeviceDriver.h"
#include "zigbeeDriverCommon.h"

#define LOG_TAG "ZigBeeSensorDD"
#define DEVICE_DRIVER_NAME "ZigBeeSensorDD"
#define MY_DC_VERSION 1

/* ZigbeeDriverCommonCallbacks */
static bool getDiscoveredDeviceMetadata(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details, icStringHashMap *metadata);
static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);
static bool preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext);

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues);

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues);

/* IASZoneClusterCallbacks */
static void onZoneStatusChanged(uint64_t eui64,
                                uint8_t endpointId,
                                const IASZoneStatusChangedNotification *notification,
                                const ComcastBatterySavingData *batterySavingData,
                                const void *driverCtx);

static const uint16_t myDeviceIds[] = { SENSOR_DEVICE_ID };

DeviceDriver *zigbeeSensorDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    static const ZigbeeDriverCommonCallbacks myHooks = {
            .fetchInitialResourceValues = fetchInitialResourceValues,
            .registerResources = registerResources,
            .mapDeviceIdToProfile = mapDeviceIdToProfile,
            .getDiscoveredDeviceMetadata = getDiscoveredDeviceMetadata,
            .preConfigureCluster = preConfigureCluster
    };

    static const IASZoneClusterCallbacks iasZoneClusterCallbacks = {
            .onZoneEnrollRequested = NULL,
            .onZoneStatusChanged = onZoneStatusChanged
    };

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DEVICE_DRIVER_NAME,
                                                                  SENSOR_DC,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  deviceService,
                                                                  &myHooks);

    zigbeeDriverCommonAddCluster(myDriver, iasZoneClusterCreate(&iasZoneClusterCallbacks, myDriver));

    return myDriver;
}

static bool fetchInitialResourceValues(ZigbeeDriverCommon *ctx, icDevice *device,
                                       IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                                       icInitialResourceValues *initialResourceValues)
{
    return iasZoneFetchInitialResourceValues(device, NULL, NULL, 0, discoveredDeviceDetails, initialResourceValues);
}

static bool
registerResources(ZigbeeDriverCommon *ctx, icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails,
                  icInitialResourceValues *initialResourceValues)
{
    return iasZoneRegisterResources(device, NULL, 0, discoveredDeviceDetails, initialResourceValues);
}

static void onZoneStatusChanged(uint64_t eui64,
                                uint8_t endpointId,
                                const IASZoneStatusChangedNotification *notification,
                                const ComcastBatterySavingData *batterySavingData,
                                const void *driverCtx)
{
    if(batterySavingData != NULL)
    {
        comcastBatterySavingHelperUpdateResources(eui64, batterySavingData, driverCtx);
    }

    iasZoneStatusChangedHelper(eui64, endpointId, notification, driverCtx);
}

static bool getDiscoveredDeviceMetadata(ZigbeeDriverCommon *ctx, IcDiscoveredDeviceDetails *details, icStringHashMap *metadata)
{
    bool ok = true;

    for (int i = 0; i < details->numEndpoints; i++)
    {
        uint8_t endpointId = details->endpointDetails[i].endpointId;
        if (icDiscoveredDeviceDetailsEndpointHasCluster(details, endpointId, IAS_ZONE_CLUSTER_ID, true))
        {
            AUTO_CLEAN(cJSON_Delete__auto) cJSON *endpoints = cJSON_CreateArray();

            cJSON_AddItemToArray(endpoints, cJSON_CreateNumber(endpointId));
            if (!stringHashMapPut(metadata, strdup(SENSOR_PROFILE_ENDPOINT_ID_LIST), cJSON_PrintUnformatted(endpoints)))
            {
                icLogWarn(LOG_TAG, "%s: Unable to write sensor zone endpoint number", __FUNCTION__);
                ok = false;
            }
            break;
        }
    }

    if (!stringHashMapPut(metadata, strdup(SENSOR_PROFILE_RESOURCE_QUALIFIED), strdup("true")))
    {
        icLogWarn(LOG_TAG, "%s: Unable to write sensor qualified flag", __FUNCTION__);
        ok = false;
    }

    return ok;
}

static bool preConfigureCluster(ZigbeeDriverCommon *ctx, ZigbeeCluster *cluster, DeviceConfigurationContext *deviceConfigContext)
{
    if (cluster->clusterId == POWER_CONFIGURATION_CLUSTER_ID)
    {
        powerConfigurationClusterSetConfigureBatteryAlarmState(deviceConfigContext, false);
    }

    if (cluster->clusterId == POLL_CONTROL_CLUSTER_ID)
    {
        //5 * 60 * 4 == 5 minutes in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, LONG_POLL_INTERVAL_QS_METADATA, "1200");

        //2 == half second in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, SHORT_POLL_INTERVAL_QS_METADATA, "2");

        //10 * 4 == 10 seconds in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, FAST_POLL_TIMEOUT_QS_METADATA, "40");

        //27 * 60 * 4 == 27 minutes in quarter seconds
        stringHashMapPutCopy(deviceConfigContext->configurationMetadata, CHECK_IN_INTERVAL_QS_METADATA, "6480");
    }

    return true;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case SENSOR_DEVICE_ID:
            profile = SENSOR_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}
