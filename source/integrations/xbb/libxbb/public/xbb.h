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
 * Interface to XBB (Xfinity Battery Backup).
 *
 * Based on Comcast-SP-XBB-ZigBee-SW-D03-160613 draft June 13, 2016
 *
 * Created by Thomas Lea on 7/18/16.
 */

#ifndef ZILKER_XBB_H
#define ZILKER_XBB_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define MAX_MANUFACTURER_LEN 64
#define MAX_MODEL_LEN 64
#define MAX_SERIAL_NUMBER_LEN 32
#define MAX_VENDOR_SPECIFIC_DATA_LEN 128

typedef enum
{
    batteryStatusUnknown,
    batteryStatusNormal,
    batteryStatusLow,
    batteryStatusDepleted,
} BatteryStatus;

typedef enum
{
    batteryHealthGood,    /* 80-100% Capacity + Impedance low */
    batteryHealthFair,    /* 50% to 79% Capacity + Impedance moderately low */
    batteryHealthPoor,    /* <= 50% Capacity or Impedance moderately high */
    batteryHealthFailure, /* = Battery is Dead, Impedance High, or Turned Off for Safety Issue. */
} BatteryHealth;

typedef enum
{
    batteryTestingStateNotDischarging,
    batteryTestingStateDischarging,
    batteryTestingStateCharging,
} BatteryTestingState;

typedef enum
{
    batteryChargingSystemHealthGood,
    batteryChargingSystemHealthVoltageHigh,
    batteryChargingSystemHealthCurrentHigh,
    batteryChargingSystemHealthCurrentLow,
    batteryChargingSystemHealthDischargingOrTestCurrentFailure, //TODO: this entry may need renaming if its use becomes clear
} BatteryChargingSystemHealth;

typedef struct
{
    char                        manufacturer[MAX_MANUFACTURER_LEN+1];
    char                        model[MAX_MODEL_LEN+1];
    uint64_t                    firmwareVersion;
    uint8_t                     hardwareVersion;
    char                        serialNumber[MAX_SERIAL_NUMBER_LEN + 1];
    BatteryStatus               batteryStatus;
    BatteryHealth               batteryHealth;
    bool                        isCharging;
    bool                        isUnderTest;
    BatteryTestingState         testingState;
    BatteryChargingSystemHealth chargingSystemHealth;
    uint32_t                    secondsOnBattery;
    uint32_t                    estimatedMinutesRemaining;
    uint8_t                     estimatedChargeRemainingPercent;
    int16_t                     currentTemperature; /* degrees Celsius */
    int16_t                     minTempExperienced; /* degrees Celsius */
    int16_t                     maxTempExperienced; /* degrees Celsius */
    bool                        hasAlarms;
    char                        vendorSpecificData[MAX_VENDOR_SPECIFIC_DATA_LEN + 1];
} XbbStatus;

typedef struct
{
    uint32_t                poweredDeviceIdlePower1;
    uint32_t                poweredDeviceIdlePower2; /* ignored on XBB1 */
    uint32_t                configLowBatteryMinutes;
    int16_t                 lowTempThreshold; /* degrees Celcius */
    int16_t                 highTempThreshold; /* degrees Celcius */
    uint32_t                lowTempDwellTripPointSeconds;
    uint32_t                highTempDwellTripPointSeconds;

    /**
     * deviceTempAlarmMask is a bitmask of:
     * bit 0 - enable alarms for when the temperature drops below lowTempThreshold for longer than lowTempDwellTripPointSeconds
     * bit 1 - enable alarms for when the temperature raises above highTempThreshold for longer than highTempDwellTripPointSeconds
     */
    uint8_t                 deviceTempAlarmMask;

} XbbConfiguration;

typedef enum
{
    alarmTypeUnknown,
    alarmTypeLowTemp,
    alarmTypeHighTemp,
    alarmTypeBatteryBad,
    alarmTypeBatteryLow,
    alarmTypeChargingSystemBad,
    alarmTypeBatteryMissing,
} AlarmType;

typedef struct
{
    AlarmType type;
    uint32_t timestamp; //POSIX timestamp
} XbbAlarmInfo;

typedef enum
{
    sirenTemporalPatternNone,
    sirenTemporalPattern3,
    sirenTemporalPattern4,
    sirenTemporalPatternUserDefined
} SirenTemporalPattern;

/**
 * Check if a battery is currently paired.
 *
 * NOTE: even though a battery might be considered paired, it could be offline and require rediscovery to recover
 *
 * @param isPaired set to true if a battery is paired
 *
 * @return true on success
 */
bool xbbIsBatteryPaired(bool *isPaired);

/**
 * Retrieve the current status information from the XBB.
 *
 * @param status a pointer to an allocated XbbStatus structure that will be populated upon success
 *
 * @return true on success
 */
bool xbbGetStatus(XbbStatus *status);

/**
 * Retrieve the current XBB configuration.
 *
 * @param config a pointer to an allocated XbbConfiguration structure that will be populated upon success
 *
 * @return true on success
 */
bool xbbGetConfig(XbbConfiguration *config);

/**
 * Set the configuration options on the XBB.
 *
 * @param config a pointer to a populated XbbConfiguration structure
 *
 * @return true on success
 */
bool xbbSetConfig(XbbConfiguration *config);

/**
 * Attempt to locate the XBB.  This will discard any currently associated XBB.  XBB needs to also be attempting to pair.
 *
 * @param timeoutSeconds the number of seconds for the locate operation before giving up
 *
 * @return true if the XBB is successfully located
 */
bool xbbDiscover(uint16_t timeoutSeconds);

/**
 * Retrieve the current list of alarms (if any).
 *
 * @param alarmInfos will receive the list of XbbAlarmInfos currently active in the XBB.  Will be set to NULL if no alarms.
 * @param numAlarmInfos will receive the number of alarms returned in alarmInfos.  Will be set to 0 if no alarms.
 *
 * @return true on success
 */
bool xbbGetAlarms(XbbAlarmInfo **alarmInfos, uint16_t *numAlarmInfos);


/**
 * Start the siren with the provided configuration
 *
 * @param frequency the frequency to be used for the siren in Hz. When the value is set to zero, the siren is to
 *        remain off. This value is disregarded if the siren operates at a fixed frequency
 * @param volumePercent the percentage of maximum volume for the siren to use. This value is disregarded if the siren
 *        operates at a fixed volume.
 * @param durationSeconds the duration for the siren to be active in seconds. When set to zero, the siren is to remain
 *        off.
 * @param temporalPattern the temporal pattern to use.  See SirenTemporalPattern enumeration.
 * @param numPulses the number of pulses in the temporal pattern.
 *        This value must be greater than zero.
 * @param onPhaseDurationMillis the duration in milliseconds for which each pulse is audible.
 *        This value must be greater than zero.
 * @param offPhaseDurationMillis the duration in milliseconds for which each pulse is not audible.
 *        This value must be greater than zero.
 * @param pauseDurationMillis the duration in milliseconds between end of the last pulse and the beginning of the
 *        new series of pulses.
 *
 * @return true on success
 */
bool xbbSirenStart( uint16_t frequency,
                    uint8_t volumePercent,
                    uint16_t durationSeconds,
                    SirenTemporalPattern temporalPattern,
                    uint8_t numPulses,
                    uint16_t onPhaseDurationMillis,
                    uint16_t offPhaseDurationMillis,
                    uint16_t pauseDurationMillis);

/**
 * Stop the siren that was started with xbbSirenStart.  Does not stop default sirens.
 *
 * @return true on success
 */
bool xbbSirenStop();

/**
 * Mutes an active low or bad battery siren for a 24-hour period or until the beginning of the next daily window
 * starting at 6pm.
 *
 * Note – Because the XBB1’s Zigbee radio is not active when in battery backup mode, this command is only applicable
 * to the XBB1 bad battery siren.  For XBB2, this command is applicable to both the low battery and the bad battery
 * siren.
 *
 * @return true on success
 */
bool xbbSirenMute();


#ifdef __cplusplus
}
#endif

#endif //ZILKER_XBB_H
