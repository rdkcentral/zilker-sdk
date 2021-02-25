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
// Created by mkoch201 on 5/16/19.
//

#include <subsystems/zigbee/zigbeeCommonIds.h>
#include <icUtil/array.h>
#include <commonDeviceDefs.h>
#include <icLog/logging.h>
#include "zigbeePresenceDeviceDriver.h"
#include "zigbeeDriverCommon.h"

#define LOG_TAG "ZigBeePresenceDD"
#define DEVICE_DRIVER_NAME "ZigBeePresenceDD"
#define MY_DC_VERSION 1

#define PRESENCE_DEVICE_ID 0x1080

/* ZigbeeDriverCommonCallbacks */
static void preStartup(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds);
static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId);

static const uint16_t myDeviceIds[] = { PRESENCE_DEVICE_ID };

DeviceDriver *zigbeePresenceDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    static const ZigbeeDriverCommonCallbacks myHooks = {
            .mapDeviceIdToProfile = mapDeviceIdToProfile,
            .preStartup = preStartup
    };

    DeviceDriver *myDriver = zigbeeDriverCommonCreateDeviceDriver(DEVICE_DRIVER_NAME,
                                                                  PRESENCE_DC,
                                                                  MY_DC_VERSION,
                                                                  myDeviceIds,
                                                                  ARRAY_LENGTH(myDeviceIds),
                                                                  deviceService,
                                                                  &myHooks);

    // Doesn't need to be in device descriptor, these are special experimental devices
    myDriver->neverReject = true;

    return myDriver;
}

static void preStartup(ZigbeeDriverCommon *ctx, uint32_t *commFailTimeoutSeconds)
{
    // This gives enough time for a few missed messages to avoid intermittent failures
    *commFailTimeoutSeconds = 95;
}

static const char *mapDeviceIdToProfile(ZigbeeDriverCommon *ctx, uint16_t deviceId)
{
    const char *profile = NULL;

    switch (deviceId)
    {
        case PRESENCE_DEVICE_ID:
            profile = PRESENCE_PROFILE;
            break;
        default:
            break;
    }

    return profile;
}