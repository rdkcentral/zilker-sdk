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

/*
 * This is a device driver that supports the 'window covering' device class
 *
 * Created by Thomas Lea on 10/19/2018
 */

#include <deviceDriver.h>
#include <icLog/logging.h>
#include <string.h>
#include <commonDeviceDefs.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <resourceTypes.h>
#include <icBuildtime.h>
#include <inttypes.h>
#include <stdio.h>
#include <limits.h>
#include <zhal/zhal.h>
#include <errno.h>
#include <device/icDeviceResource.h>
#include <device/icDevice.h>
#include "../../src/deviceModelHelper.h"

#define LOG_TAG "ZigBeeWindowCDD"
#define DEVICE_DRIVER_NAME "ZigBeeWindowCDD"
#define DEVICE_CLASS_NAME "windowCovering"
#define DEVICE_PROFILE_NAME "windowCovering"

// only compile if we support zigbee
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

#define MY_DC_VERSION 1

static bool registerResources(icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails);
static bool executeResource(uint32_t endpointNumber, icDeviceResource *resource, const char *arg, char **response);

static const char *mapDeviceIdToProfile(uint16_t deviceId);

static uint16_t myDeviceIds[] = {WINDOW_COVERING_DEVICE_ID};

static ZigbeeBaseDriverContext *ctx = NULL;

DeviceDriver *zigbeeWindowCoveringDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    ctx = zigbeeBaseDriverInitialize(DEVICE_DRIVER_NAME,
                                     DEVICE_CLASS_NAME,
                                     MY_DC_VERSION,
                                     myDeviceIds,
                                     sizeof(myDeviceIds) / sizeof(uint16_t),
                                     deviceService);

    //set the callback overrides that we want to handle
    ctx->registerResources = registerResources;
    ctx->executeEndpointResource = executeResource;
    ctx->mapDeviceIdToProfile = mapDeviceIdToProfile;

    return ctx->deviceDriver;
}

static bool registerResources(icDevice *device, IcDiscoveredDeviceDetails *discoveredDeviceDetails)
{
    bool result = true;

    icLogDebug(LOG_TAG, "%s: uuid=%s", __FUNCTION__, device->uuid);

    for (uint8_t i = 0; i < discoveredDeviceDetails->numEndpoints; i++)
    {
        uint8_t endpointId = discoveredDeviceDetails->endpointDetails[i].endpointId;
        uint16_t deviceId = discoveredDeviceDetails->endpointDetails[i].appDeviceId;

        char epName[4]; //max uint8_t + \0
        sprintf(epName, "%" PRIu8, endpointId);

        if (deviceId == WINDOW_COVERING_DEVICE_ID)
        {
            icDeviceEndpoint *endpoint = createEndpoint(device, epName, WINDOW_COVERING_PROFILE, true);

            createEndpointResource(endpoint,
                                   COMMON_ENDPOINT_RESOURCE_LABEL,
                                   NULL,
                                   RESOURCE_TYPE_LABEL,
                                   RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                   CACHING_POLICY_ALWAYS);

            createEndpointResource(endpoint,
                                   WINDOW_COVERING_FUNCTION_UP,
                                   NULL,
                                   RESOURCE_TYPE_MOVE_UP_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER);

            createEndpointResource(endpoint,
                                   WINDOW_COVERING_FUNCTION_DOWN,
                                   NULL,
                                   RESOURCE_TYPE_MOVE_DOWN_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER);

            createEndpointResource(endpoint,
                                   WINDOW_COVERING_FUNCTION_STOP,
                                   NULL,
                                   RESOURCE_TYPE_STOP_OPERATION,
                                   RESOURCE_MODE_EXECUTABLE,
                                   CACHING_POLICY_NEVER);

            ctx->setEndpointNumber(ctx, endpoint, endpointId);
        }
    }

    return result;
}

static bool executeResource(uint32_t endpointNumber, icDeviceResource *resource, const char *arg, char **response)
{
    bool result = true;

    if (resource == NULL || endpointNumber == 0)
    {
        icLogDebug(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    uint64_t eui64 = zigbeeSubsystemIdToEui64(resource->deviceUuid);
    uint8_t epid = ctx->getEndpointNumber(ctx, resource->deviceUuid, resource->endpointId);

    uint8_t commandId = 0xff; //invalid
    if (strcmp(resource->id, WINDOW_COVERING_FUNCTION_UP) == 0)
    {
        commandId = WINDOW_COVERING_UP_COMMAND_ID;
    }
    else if (strcmp(resource->id, WINDOW_COVERING_FUNCTION_DOWN) == 0)
    {
        commandId = WINDOW_COVERING_DOWN_COMMAND_ID;
    }
    else if (strcmp(resource->id, WINDOW_COVERING_FUNCTION_STOP) == 0)
    {
        commandId = WINDOW_COVERING_STOP_COMMAND_ID;
    }

    zigbeeSubsystemSendCommand(eui64,
                               epid,
                               WINDOW_COVERING_CLUSTER_ID,
                               true,
                               commandId,
                               NULL,
                               0);

    return result;
}

static const char *mapDeviceIdToProfile(uint16_t deviceId)
{
    for (int i = 0; i < ctx->numDeviceIds; ++i)
    {
        if (ctx->deviceIds[i] == deviceId)
        {
            return WINDOW_COVERING_PROFILE;
        }
    }
    return NULL;
}

#endif // CONFIG_SERVICE_DEVICE_ZIGBEE

