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
// Created by tlea on 5/1/19.
//

#include <icBuildtime.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zigbeeClusters/comcastBatterySaving.h>
#include <stddef.h>
#include <icLog/logging.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <icTypes/sbrm.h>
#include <commonDeviceDefs.h>
#include <zigbeeDriverCommon.h>
#include "comcastBatterySavingHelper.h"

#define LOG_TAG "ComcastBatterySavingHelper"

ComcastBatterySavingData *comcastBatterySavingDataParse(uint8_t *buffer, uint16_t bufferLen)
{
    ComcastBatterySavingData *result = NULL;

    if (buffer == NULL || bufferLen < sizeof(ComcastBatterySavingData))
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
    }
    else
    {
        result = calloc(1, sizeof(*result));

        sbZigbeeIOContext *zio = zigbeeIOInit(buffer, bufferLen, ZIO_READ);

        result->battVoltage = zigbeeIOGetUint16(zio);
        result->battUsedMilliAmpHr = zigbeeIOGetUint16(zio);
        result->temp = zigbeeIOGetInt16(zio);
        result->rssi = zigbeeIOGetInt8(zio);
        result->lqi = zigbeeIOGetUint8(zio);
        result->retries = zigbeeIOGetUint32(zio);
        result->rejoins = zigbeeIOGetUint32(zio);
    }

    return result;
}

void comcastBatterySavingHelperUpdateResources(uint64_t eui64,
                                               const ComcastBatterySavingData *data,
                                               const ZigbeeDriverCommon *ctx)
{
    const DeviceServiceCallbacks *deviceService = zigbeeDriverCommonGetDeviceService((ZigbeeDriverCommon *) ctx);
    AUTO_CLEAN(free_generic__auto) const char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    char valStr[11];

    //Battery Voltage
    snprintf(valStr, 6, "%u", data->battVoltage); //65535 + \0 worst case
    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE,
                                  valStr,
                                  NULL);

    //FE RSSI
    snprintf(valStr, 5, "%"PRIi8, data->rssi); //-127 + \0 worst case
    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_FERSSI,
                                  valStr,
                                  NULL);

    //FE LQI
    snprintf(valStr, 4, "%u", data->lqi); //255 + \0 worst case
    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_FELQI,
                                  valStr,
                                  NULL);

    //Sensor Temp
    snprintf(valStr, 7, "%"PRIi16, data->temp); //-32768 + \0 worst case
    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_TEMPERATURE,
                                  valStr,
                                  NULL);

    //Battery used mAH
    snprintf(valStr, 6, "%u", data->battUsedMilliAmpHr); //65535 + \0 worst case
    deviceService->setMetadata(deviceUuid,
                               NULL,
                               COMMON_DEVICE_METADATA_BATTERY_USED_MAH,
                               valStr);

    //Rejoins
    snprintf(valStr, 11, "%u", data->rejoins); //4294967295 + \0 worst case
    deviceService->setMetadata(deviceUuid,
                               NULL,
                               COMMON_DEVICE_METADATA_REJOINS,
                               valStr);

    //Retries
    snprintf(valStr, 11, "%u", data->retries); //4294967295 + \0 worst case
    deviceService->setMetadata(deviceUuid,
                               NULL,
                               COMMON_DEVICE_METADATA_RETRIES,
                               valStr);
}
