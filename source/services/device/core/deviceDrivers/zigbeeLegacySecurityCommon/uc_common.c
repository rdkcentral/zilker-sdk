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
// Created by tlea on 5/7/18.
//

#include <stdlib.h>
#include <icLog/logging.h>
#include <string.h>
#include <stdio.h>
#include <subsystems/zigbee/zigbeeIO.h>
#include <icTypes/sbrm.h>
#include <icUtil/stringUtils.h>
#include <deviceDriver.h>
#include <inttypes.h>
#include <commonDeviceDefs.h>
#include <resourceTypes.h>
#include <deviceModelHelper.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <errno.h>
#include <icBuildtime.h>
#ifdef CONFIG_OS_DARWIN
#include <machine/endian.h>
#else
#include <endian.h>
#endif
#include "uc_common.h"

#define LOG_TAG "uc_common"

bool parseDeviceInfoMessage(const uint8_t* message, uint16_t messageLen, uCDeviceInfoMessage* deviceInfoMessage)
{
    if (deviceInfoMessage == NULL)
    {
        return false;
    }

    memset(deviceInfoMessage, 0, sizeof(uCDeviceInfoMessage));

    //TODO
    icLogDebug(LOG_TAG, "%s: TODO validate message len", __FUNCTION__);

    deviceInfoMessage->firmwareVer[0] = message[0];
    deviceInfoMessage->firmwareVer[1] = message[1];
    deviceInfoMessage->firmwareVer[2] = message[2];

    deviceInfoMessage->devType = (uCDeviceType) message[3];

    deviceInfoMessage->devStatus.byte1 = message[4];
    deviceInfoMessage->devStatus.byte2 = message[5];

    deviceInfoMessage->config.lowTempLimit.tempFrac = message[6];
    deviceInfoMessage->config.lowTempLimit.tempInt = message[7];
    deviceInfoMessage->config.highTempLimit.tempFrac = message[8];
    deviceInfoMessage->config.highTempLimit.tempInt = message[9];
    deviceInfoMessage->config.lowBattThreshold = message[11] + (message[10] << 8);

    deviceInfoMessage->config.devNum = message[12];
    deviceInfoMessage->config.enables.byte = message[13];

    deviceInfoMessage->config.hibernateDuration = message[15] + (message[14] << 8);
    deviceInfoMessage->config.napDuration = message[16] + (message[17] << 8);

    return true;
}


char* deviceInfoMessageToString(uCDeviceInfoMessage* message)
{
    char* result = (char*) malloc(256);

    snprintf(result, 256,
             "deviceInfoMessage["
             "fwVer:%d.%d.%d"
             ", devType:0x%02x"
             ", devStatus:0x%04x"
             ", lowTempLimit: %d.%u"
             ", hightTempLimit: %d.%u"
             ", lowBattThreshold: %u"
             ", devNum: %u"
             ", enables: 0x%02x"
             ", hibernationDuration: 0x%04x"
             ", napDuration: 0x%04x"
             ", region: %u]",
             message->firmwareVer[0], message->firmwareVer[1], message->firmwareVer[2],
             message->devType,
             (message->devStatus.byte1 << 8) + message->devStatus.byte2,
             message->config.lowTempLimit.tempInt, message->config.lowTempLimit.tempFrac,
             message->config.highTempLimit.tempInt, message->config.highTempLimit.tempFrac,
             message->config.lowBattThreshold,
             message->config.devNum,
             message->config.enables.byte,
             message->config.hibernateDuration,
             message->config.napDuration,
             message->config.region);

    return result;
}

bool getLegacyDeviceDetails(uCDeviceType devType, uint32_t firmwareVersion, LegacyDeviceDetails** details)
{
    bool result = true;
    char* manufacturer = NULL;
    char* model = NULL;
    uint8_t hwVer = 1;
    uint8_t legacyFwVer[3] = { 0, 0, 0 };
    bool isMainsPowered = false;
    bool isBatteryBackedUp = false;
    uCLegacyDeviceClassification classification = UC_DEVICE_CLASS_CONTACT_SWITCH;

    if (details == NULL)
    {
        return false;
    }

    /*
     * Firmware version is a 24-bit big-endian value
     * Skip the least significant (first) byte
     */
#ifdef CONFIG_OS_DARWIN
    const uint32_t beFirmwareVersion = (uint32_t)htonl(firmwareVersion);
#else
    const uint32_t beFirmwareVersion = htobe32(firmwareVersion);
#endif
    memcpy(legacyFwVer, (uint8_t *) &beFirmwareVersion + 1, 3);

    switch (devType)
    {
        case DOOR_WINDOW_1:
            hwVer = 3;
            manufacturer = "SMC";
            model = "SMCDW01-Z";
            classification = UC_DEVICE_CLASS_CONTACT_SWITCH;
            break;
        case SMOKE_1:
            manufacturer = "SMC";
            model = "SMCSM01-Z";
            classification = UC_DEVICE_CLASS_SMOKE;
            break;
        case MOTION_1:
            manufacturer = "BAD";
            model = "UNUSED-MOTION";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case GLASS_BREAK_1:
            manufacturer = "SMC";
            model = "SMCGB01-Z";
            classification = UC_DEVICE_CLASS_GLASS_BREAK;
            break;
        case WATER_1:
            manufacturer = "SMC";
            model = "SMCWA01-Z";
            classification = UC_DEVICE_CLASS_WATER;
            break;
        case INERTIA_1:
            manufacturer = "BAD";
            model = "UNUSED-INERTIA";
            classification = UC_DEVICE_CLASS_UNKNOWN;
            break;
        case KEYFOB_1:
            /* The middle byte is a magic number used as an extra type selector */
            if (legacyFwVer[1] == 3)
            {
                // smc keyfob
                manufacturer = "SMC";
                model = "SMCKF01-Z";
            }
            else
            {
                // hitron keyfob
                manufacturer = "Hitron";
                model = "NCZ-3201";
            }
            classification = UC_DEVICE_CLASS_KEYFOB;
            break;
        case REPEATER_SIREN_1:
            isMainsPowered = true;
            manufacturer = "SMC";
            model = "SMCSR00-Z";
            classification = UC_DEVICE_CLASS_SIREN;
            break;
        case MOTION_BIG_GE:
            manufacturer = "SMC";
            model = "SMCMT01-Z";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case MOTION_SUREN_W_RF:
            manufacturer = "SMC";
            model = "SMCMT00-Z";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case KEYPAD_1:
            manufacturer = "SMC";
            model = "SMCWK01-Z";
            classification = UC_DEVICE_CLASS_KEYPAD;
            break;
        case TAKEOVER_1:
            // TODO - i think there is a way to tell a liteon pim from smc and hitron...
            isMainsPowered = true;
            isBatteryBackedUp = true;
            manufacturer = "SMC";
            model = "SMCTB01-Z";
            classification = UC_DEVICE_CLASS_KEYPAD;
            break;
        case MICRO_DOOR_WINDOW:
            manufacturer = "SMC";
            model = "SMCUD01-Z";
            classification = UC_DEVICE_CLASS_CONTACT_SWITCH;
            break;
        case MTL_REPEATER_SIREN:
            isMainsPowered = true;
            isBatteryBackedUp = true;
            manufacturer = "SMC";
            model = "SMCSR01-Z";
            classification = UC_DEVICE_CLASS_SIREN;
            break;
        case GE_CO_SENSOR_1:
            manufacturer = "SMC";
            model = "SMCCO01-Z";
            classification = UC_DEVICE_CLASS_CO;
            break;
        case MTL_DOOR_WINDOW:
            manufacturer = "SMC";
            model = "SMCDW02-Z";
            classification = UC_DEVICE_CLASS_CONTACT_SWITCH;
            break;
        case MTL_GLASS_BREAK:
            manufacturer = "SMC";
            model = "SMCGB02-Z";
            classification = UC_DEVICE_CLASS_GLASS_BREAK;
            break;
        case MTL_GE_SMOKE:
            manufacturer = "SMC";
            model = "SMCSM02-Z";
            classification = UC_DEVICE_CLASS_SMOKE;
            break;
        case MTL_GE_MOTION:
            manufacturer = "SMC";
            model = "SMCMT03-Z";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case MTL_SUREN_MOTION:
            manufacturer = "SMC";
            model = "SMCMT02-Z";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case MTL_GE_CO_SENSOR:
            manufacturer = "SMC";
            model = "SMCCO02-Z";
            classification = UC_DEVICE_CLASS_CO;
            break;
        case MTL_ED_CO_SENSOR:
            manufacturer = "SMC";
            model = "SMCCO03O-ED-Z";
            classification = UC_DEVICE_CLASS_CO;
            break;
        case SMC_MOTION:
            manufacturer = "SMC";
            model = "SMCMT10-Z";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case SMC_SMOKE:
            manufacturer = "SMC";
            model = "SMCSM10-Z";
            classification = UC_DEVICE_CLASS_SMOKE;
            break;
        case SMC_SMOKE_NO_SIREN:
            manufacturer = "SMC";
            model = "SMCSD10-Z";
            classification = UC_DEVICE_CLASS_SMOKE;
            break;
        case SMC_GLASS_BREAK:
            manufacturer = "SMC";
            model = "SMCGB10-Z";
            classification = UC_DEVICE_CLASS_GLASS_BREAK;
            break;
        case SMC_CO_SENSOR:
            manufacturer = "SMC";
            model = "SMCCO10-Z";
            classification = UC_DEVICE_CLASS_CO;
            break;
        case MCT_320_DW:
            manufacturer = "Visonic";
            model = "MCT-320 SMA";
            classification = UC_DEVICE_CLASS_CONTACT_SWITCH;
            break;
        case CLIP_CURTAIN:
            manufacturer = "Visonic";
            model = "CLIP SMA";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case NEXT_K9_85_MOTION:
            manufacturer = "Visonic";
            model = "NEXT K85 SMA";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case MCT_550_FLOOD:
            manufacturer = "Visonic";
            model = "MCT-550 SMA";
            classification = UC_DEVICE_CLASS_WATER;
            break;
        case MCT_302_DW_1_WIRED:
            manufacturer = "Visonic";
            model = "MCT-302 SMA";
            classification = UC_DEVICE_CLASS_CONTACT_SWITCH;
            break;
        case MCT_427_SMOKE:
            manufacturer = "Visonic";
            model = "MCT-427 SMA";
            classification = UC_DEVICE_CLASS_SMOKE;
            break;
        case MCT_442_CO:
            manufacturer = "Visonic";
            model = "MCT-442 SMA";
            classification = UC_DEVICE_CLASS_CO;
            break;
        case MCT_100_UNIVERSAL:
            manufacturer = "Visonic";
            model = "MCT-100 SMA";
            classification = UC_DEVICE_CLASS_REMOTE_CONTROL;
            break;
        case MCT_101_1_BTN:
            manufacturer = "Visonic";
            model = "MCT-101 SMA";
            classification = UC_DEVICE_CLASS_REMOTE_CONTROL;
            break;
        case MCT_102_2_BTN:
            manufacturer = "Visonic";
            model = "MCT-102 SMA";
            classification = UC_DEVICE_CLASS_REMOTE_CONTROL;
            break;
        case MCT_103_3_BTN:
            manufacturer = "Visonic";
            model = "MCT-103 SMA";
            classification = UC_DEVICE_CLASS_REMOTE_CONTROL;
            break;
        case MCT_104_4_BTN:
            manufacturer = "Visonic";
            model = "MCT-104 SMA";
            classification = UC_DEVICE_CLASS_REMOTE_CONTROL;
            break;
        case MCT_124_4_BTN:
            manufacturer = "Visonic";
            model = "MCT-124 SMA";
            classification = UC_DEVICE_CLASS_REMOTE_CONTROL;
            break;
        case MCT_220_EMER_BTN:
            manufacturer = "Visonic";
            model = "MCT-220 SMA";
            classification = UC_DEVICE_CLASS_PERSONAL_EMERGENCY;
            break;
        case MCT_241_PENDANT:
            manufacturer = "Visonic";
            model = "MCT-241 SMA";
            classification = UC_DEVICE_CLASS_PERSONAL_EMERGENCY;
            break;
        case DISCOVERY_PIR_MOT:
            manufacturer = "Visonic";
            model = "Discovery PIR SMA";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case TOWER_40_MCW_MOTION:
            manufacturer = "Visonic";
            model = "Tower 40 MCW SMA";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case K_940_MCW_MOTION:
            manufacturer = "Visonic";
            model = "K9-40 MCW SMA";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case DISCOVERY_K9_80_MOT:
            manufacturer = "Visonic";
            model = "Discovery K9-80 SMA";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case DISCOVERY_QUAD_80_MOT:
            manufacturer = "Visonic";
            model = "Discovery Quad 80 SMA";
            classification = UC_DEVICE_CLASS_MOTION;
            break;
        case MCT_441_GAS:
            manufacturer = "Visonic";
            model = "MCT-441 SMA";
            classification = UC_DEVICE_CLASS_CO;
            break;
        case MCT_501_GLASS_BREAK:
            manufacturer = "Visonic";
            model = "MCT-501 SMA";
            classification = UC_DEVICE_CLASS_GLASS_BREAK;
            break;
        case MCT_560_TEMP:
            manufacturer = "Visonic";
            model = "MCT-560 SMA";
            classification = UC_DEVICE_CLASS_UNKNOWN;
            break;
        case NEXTP_K9_85_MOTION:
            manufacturer = "Visonic";
            model = "NEXTPlus-K9-85 SMA";
            classification = UC_DEVICE_CLASS_MOTION;
            break;

        default:
            //will fall through without details
            result = false;
            break;
    }

    if (result)
    {
        LegacyDeviceConfig config = getLegacyDeviceConfig(devType);
        *details = (LegacyDeviceDetails*) calloc(1, sizeof(LegacyDeviceDetails));
        (*details)->lowBatteryVoltage = config.lowBattThreshold;
        snprintf((*details)->manufacturer, sizeof((*details)->manufacturer), "%s", manufacturer);
        snprintf((*details)->model, sizeof((*details)->model), "%s", model);
        (*details)->hardwareVersion = hwVer;
        memcpy((*details)->firmwareVer, legacyFwVer, 3);
        (*details)->isMainsPowered = isMainsPowered;
        (*details)->isBatteryBackedUp = isBatteryBackedUp;
        (*details)->devType = devType;
        (*details)->classification = classification;
    }

    return result;
}

LegacyDeviceDetails *cloneLegacyDeviceDetails(const LegacyDeviceDetails *src)
{
    LegacyDeviceDetails *dst = NULL;

    if (src)
    {
        dst = calloc(1, sizeof(LegacyDeviceDetails));
        memcpy(dst, src, sizeof(*src));

        if (src->upgradeAppFilename)
        {
            dst->upgradeAppFilename = strdup(src->upgradeAppFilename);
        }

        if (src->upgradeBootloaderFilename)
        {
            dst->upgradeBootloaderFilename = strdup(src->upgradeBootloaderFilename);
        }
    }

    return dst;
}

void legacyDeviceDetailsDestroy__auto(LegacyDeviceDetails **details)
{
    legacyDeviceDetailsDestroy(*details);
}

void legacyDeviceDetailsDestroy(LegacyDeviceDetails *details)
{
    if (details != NULL)
    {
        free(details->upgradeAppFilename);
        free(details->upgradeBootloaderFilename);
    }
    free(details);
}

LegacyDeviceConfig getLegacyDeviceConfig(uCDeviceType devType)
{
    LegacyDeviceConfig config = {
            .lowBattThreshold = 2700,
            .magSwitchEnabled = true,
            .extContactEnabled = false,
            .tamperIsMagnetic = false
    };

    switch (devType)
    {
        case DOOR_WINDOW_1:
        case REPEATER_SIREN_1:
        case SMOKE_1:
        case GE_CO_SENSOR_1:
        case INERTIA_1:
        case KEYPAD_1:
        case TAKEOVER_1:
        case MTL_DOOR_WINDOW:
        case MTL_GE_SMOKE:
        case MTL_GE_CO_SENSOR:
        case MTL_ED_CO_SENSOR:
        case SMC_SMOKE:
        case SMC_SMOKE_NO_SIREN:
        case SMC_CO_SENSOR:
            config.lowBattThreshold = 2400;
            config.extContactEnabled = true;
            break;
        case MTL_REPEATER_SIREN:
            config.lowBattThreshold = 3400;
            config.extContactEnabled = true;
            break;
        case MICRO_DOOR_WINDOW:
            config.lowBattThreshold = 2200;
            config.extContactEnabled = true;
            break;

        case WATER_1:
            config.lowBattThreshold = 2400;
            config.magSwitchEnabled = false;
            config.extContactEnabled = true;
            break;

        case GLASS_BREAK_1:
        case MTL_GLASS_BREAK:
        case SMC_GLASS_BREAK:
            config.lowBattThreshold = 2400;
            config.magSwitchEnabled = false;
            config.extContactEnabled = true;
            config.tamperIsMagnetic = true;
            break;

        case MOTION_1:
        case MOTION_BIG_GE:
        case MOTION_SUREN_W_RF:
        case MTL_GE_MOTION:
        case MTL_SUREN_MOTION:
            config.lowBattThreshold = 2400;
            break;

        case SMC_MOTION:
            break;

        case KEYFOB_1:
            config.lowBattThreshold = 2200;
            config.extContactEnabled = true;
            break;

        case MCT_320_DW:
            config.lowBattThreshold = 2400;
            config.extContactEnabled = true;
            break;
        case MCT_302_DW_1_WIRED:
        case MCT_550_FLOOD:
        case MCT_442_CO:
        case MCT_427_SMOKE:
            config.lowBattThreshold = 2700;
            config.extContactEnabled = true;
            break;

        case CLIP_CURTAIN:
        case NEXT_K9_85_MOTION:
        case NEXTP_K9_85_MOTION:
            config.lowBattThreshold = 2700;
            break;

            // currently unused types
        default:
        case DISCOVERY_PIR_MOT:
        case TOWER_40_MCW_MOTION:
        case K_940_MCW_MOTION:
        case DISCOVERY_K9_80_MOT:
        case DISCOVERY_QUAD_80_MOT:
        case MCT_100_UNIVERSAL:
        case MCT_101_1_BTN:
        case MCT_102_2_BTN:
        case MCT_103_3_BTN:
        case MCT_104_4_BTN:
        case MCT_124_4_BTN:
        case MCT_220_EMER_BTN:
        case MCT_241_PENDANT:
        case MCT_441_GAS:
        case MCT_501_GLASS_BREAK:
        case MCT_560_TEMP:
            break;
    }

    return config;
}

bool getLegacyDeviceConfigMessage(uCDeviceType devType, uint8_t devNum, uint8_t region, uint8_t** payload,
                                  uint8_t* payloadLen)
{
    LegacyDeviceConfig config = getLegacyDeviceConfig(devType);

    uint8_t* result = (uint8_t*) calloc(1, 13); //device config message is 13 bytes

    //low temp limit
    result[0] = 0;
    result[1] = 0;

    //high temp limit
    result[2] = 0;
    result[3] = 0;

    //low batt threshold
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    result[4] = (uint8_t) (config.lowBattThreshold & 0xff);
    result[5] = (uint8_t) (config.lowBattThreshold >> 8);
#else
    result[5] = (uint8_t) (lowBattThreshold & 0xff);
    result[4] = (uint8_t) (lowBattThreshold >> 8);
#endif

    //device number
    result[6] = devNum;

    //enables
    if(config.magSwitchEnabled)
    {
        result[7] |= 0x2;
    }
    if(config.extContactEnabled)
    {
        result[7] |= 0x4;
    }
    if(config.tamperIsMagnetic)
    {
        result[7] |= 0x8;
    }
    result[7] |= 0x10; //armed
    result[7] |= 0x80; //paired

    //hibernation duration
    uint16_t duration = 30*60; //30 minutes
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    result[8] = (uint8_t) (duration & 0xff);
    result[9] = (uint8_t) (duration >> 8);
#else
    result[9] = (uint8_t) (duration & 0xff);
    result[8] = (uint8_t) (duration >> 8);
#endif

    //nap duration (bytes 10 and 11)
    result[10] = 1; //1 second

    //region
    result[12] = region;

    *payload = result;
    *payloadLen = 13;

    return true;
}

bool parseDeviceStatus(uint8_t endpointId, const uint8_t* message, uint16_t messageLen, uCStatusMessage* status)
{
    //Depending on the endpointId, the status message should be either 9 or 13 bytes
    if(message == NULL || status == NULL ||
            (endpointId <= 1 && messageLen < 9) ||
            (endpointId > 1 && messageLen < 13))
    {
        return false;
    }

    //first all the common fields to both 'versions'
    status->devNum = message[0];
    status->status.byte1 = message[1];
    status->status.byte2 = message[2];
    status->batteryVoltage = (message[3] << 8) + (uint16_t)(message[4] & 0xFF);
    status->rssi = message[5];
    status->lqi = message[6];
    int16_t wholeDegrees = ((int8_t)message[7])*100;
    int16_t fractionDegrees = message[8]*10;
    if (wholeDegrees < 0)
    {
        status->temperature = wholeDegrees - fractionDegrees;
    }
    else
    {
        status->temperature = wholeDegrees + fractionDegrees;
    }

    //next we optionally handle the extra stuff if endpointId > 1
    if(endpointId > 1)
    {
        status->hasExtra = 1;

        status->QSDelay = (message[9] << 8) + (uint16_t)(message[10] & 0xFF);
        status->retryCount = message[11];
        status->rejoinCount = message[12];
    }

    return true;
}

bool parseKeypadMessage(uint8_t *message, uint16_t messageLen, uCKeypadMessage *keypadMessage)
{
    errno = 0;
    bool ok;
    sbZigbeeIOContext *zio = zigbeeIOInit(message, messageLen, ZIO_READ);

    keypadMessage->devNum = zigbeeIOGetUint8(zio);
    keypadMessage->actionButton = zigbeeIOGetUint8(zio);
    zigbeeIOGetBytes(zio, keypadMessage->code, sizeof(keypadMessage->code));

    /* Battery voltage is a big endian uint16 */
    keypadMessage->batteryVoltage[0] = zigbeeIOGetUint8(zio);
    keypadMessage->batteryVoltage[1] = zigbeeIOGetUint8(zio);

    keypadMessage->rssi = zigbeeIOGetInt8(zio);
    keypadMessage->lqi = zigbeeIOGetUint8(zio);
    keypadMessage->tempInt = zigbeeIOGetInt8(zio);
    keypadMessage->tempFrac = zigbeeIOGetUint8(zio);

    ok = errno == 0;
    if (errno != 0)
    {
        AUTO_CLEAN(free_generic__auto) const char *errStr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "%s failed: %s", __FUNCTION__, errStr);
    }

    return ok;
}

bool parseKeyfobMessage(uint8_t *message, uint16_t messageLen, uCKeyfobMessage *keyfobMessage)
{
    errno = 0;
    bool ok;
    sbZigbeeIOContext *zio = zigbeeIOInit(message, messageLen, ZIO_READ);

    keyfobMessage->devNum = zigbeeIOGetUint8(zio);
    keyfobMessage->buttons = zigbeeIOGetUint8(zio);

    /* Battery voltage is a big endian uint16 */
    keyfobMessage->batteryVoltage[0] = zigbeeIOGetUint8(zio);
    keyfobMessage->batteryVoltage[1] = zigbeeIOGetUint8(zio);

    keyfobMessage->rssi = zigbeeIOGetInt8(zio);
    keyfobMessage->lqi = zigbeeIOGetUint8(zio);
    keyfobMessage->presses = zigbeeIOGetUint16(zio);
    keyfobMessage->successes = zigbeeIOGetUint16(zio);

    ok = errno == 0;
    if (!ok)
    {
        AUTO_CLEAN(free_generic__auto) const char *errStr = strerrorSafe(errno);
        icLogWarn(LOG_TAG, "%s failed: %s", __FUNCTION__, errStr);
    }

    return ok;
}

uint32_t getFirmwareVersionFromDeviceInfoMessage(uCDeviceInfoMessage deviceInfoMessage)
{
    return convertLegacyFirmwareVersionToUint32(deviceInfoMessage.firmwareVer);
}

uint32_t convertLegacyFirmwareVersionToUint32(const uint8_t firmwareVer[3])
{
    return (firmwareVer[0] << 16) +
           (firmwareVer[1] << 8) +
           firmwareVer[2];
}

void legacyDeviceUpdateCommonResources(const DeviceServiceCallbacks *deviceService,
                                       const uint64_t eui64,
                                       const uCStatusMessage *status,
                                       bool isBatteryLow)
{
    char valStr[7]; //-32768 + \0 worst case for below usages
    AUTO_CLEAN(free_generic__auto) const char *deviceUuid = zigbeeSubsystemEui64ToId(eui64);

    snprintf(valStr, 4, "%u", (unsigned int) status->lqi);
    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_FELQI,
                                  valStr,
                                  NULL);

    snprintf(valStr, 5, "%"PRIi8, status->rssi);
    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_FERSSI,
                                  valStr,
                                  NULL);

    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_BATTERY_LOW,
                                  isBatteryLow ? "true" : "false",
                                  NULL);

    snprintf(valStr, 6, "%u", status->batteryVoltage);
    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_BATTERY_VOLTAGE,
                                  valStr,
                                  NULL);

    snprintf(valStr, 7, "%d", status->temperature);
    deviceService->updateResource(deviceUuid,
                                  NULL,
                                  COMMON_DEVICE_RESOURCE_TEMPERATURE,
                                  valStr,
                                  NULL);
}

uCLegacyDeviceClassification getLegacyDeviceClassification(uCDeviceType devType)
{
    return UC_DEVICE_CLASS_UNKNOWN;
}
