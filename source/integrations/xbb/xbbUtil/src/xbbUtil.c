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
 * Created by Thomas Lea on 8/10/16.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <xbb.h>

#include "xbb.h"

static char *getInputLine();
static int handleCommand(char *line);

int main(int argc, char ** argv)
{
    int running = 1;

    while (running)
    {
        printf("\nxbbUtil> ");
        fflush(stdout);
        char *line = getInputLine();

        running = handleCommand(line);

        free(line);
    }

    return 0;
}

static char *getInputLine()
{
    char *line = malloc(100), *linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if (line == NULL)
        return NULL;

    for (; ;)
    {
        c = fgetc(stdin);
        if (c == EOF)
        {
            free(line);
            return strdup("exit");
        }
        else if (c == '\n')
        {
            break;
        }

        if (--len == 0)
        {
            len = lenmax;
            char *linen = realloc(linep, lenmax *= 2);

            if (linen == NULL)
            {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if ((*line++ = c) == '\n')
        {
            break;
        }
    }
    *line = '\0';
    return linep;
}


static bool showInteractiveHelp()
{
    printf("\nCommands:\n");
    printf("\tgetStatus\n");
    printf("\tgetConfig\n");
    printf("\tsetConfig <PoweredDeviceIdlePower1,PoweredDeviceIdlePower2,ConfigLowBatteryTime,LowTempThreshold,HighTempThreshold,LowTempDwellTripPoint,HighTempDwellTripPoint>\n");
    printf("\tdiscover\n");
    printf("\tgetAlarms\n");
    printf("\tsirenStart <frequency, volumePercent, durationSeconds, temporalPattern, numPulses, onPhaseDurationMillis, offPhaseDurationMillis, pauseDurationMillis>\n");
    printf("\t\ttemporalPattern: 0=none, 1=3, 2=4, 3=user\n");
    printf("\tsirenStop\n");
    printf("\tsirenMute\n");
    printf("\tisBatteryPaired\n");

    printf("\tquit\n");

    return true;
}

static void getStatus()
{
    XbbStatus status;

    if(xbbGetStatus(&status) == false)
    {
        printf("failed to get status\n");
        return;
    }

    /*
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
    int16_t                     currentTemperature;
    int16_t                     minTempExperienced;
    int16_t                     maxTempExperienced;
    bool                        hasAlarms;
    char                        vendorSpecificData[MAX_VENDOR_SPECIFIC_DATA_LEN + 1];
     */

    printf("XBB Status:\n");
    printf("\tManufacturer: %s\n", status.manufacturer);
    printf("\tModel: %s\n", status.model);
    printf("\tFirmware Version: 0x%08" PRIx64 "\n", status.firmwareVersion);
    printf("\tHardware Version: 0x%02" PRIx8 "\n", status.hardwareVersion);
    printf("\tSerial Number: %s\n", status.serialNumber);
    printf("\tBattery Status: ");
    switch(status.batteryStatus)
    {
        case batteryStatusUnknown:
            printf("unknown\n");
            break;

        case batteryStatusNormal:
            printf("normal\n");
            break;

        case batteryStatusLow:
            printf("low\n");
            break;

        case batteryStatusDepleted:
            printf("depleted\n");
            break;

        default:
            printf("unsupported (%d)\n", status.batteryStatus);
            break;
    }

    printf("\tBattery Health: ");
    switch(status.batteryHealth)
    {
        case batteryHealthGood:
            printf("good\n");
            break;

        case batteryHealthFair:
            printf("fair\n");
            break;

        case batteryHealthPoor:
            printf("poor\n");
            break;

        case batteryHealthFailure:
            printf("failure\n");
            break;

        default:
            printf("unsupported (%d)\n", status.batteryHealth);
            break;
    }

    printf("\tIs Charging: %s\n", status.isCharging ? "true" : "false");
    printf("\tIs Under Test: %s\n", status.isUnderTest ? "true" : "false");

    printf("\tBattery Testing State: ");
    switch(status.testingState)
    {
        case batteryTestingStateNotDischarging:
            printf("not discharging\n");
            break;

        case batteryTestingStateDischarging:
            printf("discharging\n");
            break;

        case batteryTestingStateCharging:
            printf("charging\n");
            break;

        default:
            printf("unsupported (%d)\n", status.testingState);
            break;
    }

    printf("\tBattery Charging System Health: ");
    switch(status.chargingSystemHealth)
    {
        case batteryChargingSystemHealthGood:
            printf("good\n");
            break;

        case batteryChargingSystemHealthVoltageHigh:
            printf("voltage high\n");
            break;

        case batteryChargingSystemHealthCurrentHigh:
            printf("current high\n");
            break;

        case batteryChargingSystemHealthCurrentLow:
            printf("current low\n");
            break;

        case batteryChargingSystemHealthDischargingOrTestCurrentFailure:
            printf("discharging or test current failure\n");
            break;

        default:
            printf("unsupported (%d)\n", status.chargingSystemHealth);
            break;
    }

    printf("\tSeconds on Battery: %" PRId32 "\n", status.secondsOnBattery);
    printf("\tEstimated Minutes Remaining: %" PRIu32 "\n", status.estimatedMinutesRemaining);
    printf("\tEstimated Charge Remaining: %" PRIu32 "%%\n", status.estimatedChargeRemainingPercent);
    printf("\tCurrent Temperature (Celcius): %" PRId16 "\n", status.currentTemperature);
    printf("\tMin Temperature Experienced (Celcius): %" PRId16 "\n", status.minTempExperienced);
    printf("\tMax Temperature Experienced (Celcius): %" PRId16 "\n", status.maxTempExperienced);
    printf("\tHas Alarms: %s\n", status.hasAlarms ? "true" : "false");

    if(strlen(status.vendorSpecificData) > 0)
    {
        printf("\tVendor Specific Data: %s\n", status.vendorSpecificData);
    }
}

static void getConfig()
{
    XbbConfiguration config;

    if(xbbGetConfig(&config) == false)
    {
        printf("failed to get config\n");
        return;
    }

    /*
    uint32_t                poweredDeviceIdlePower1;
    uint32_t                poweredDeviceIdlePower2;
    uint32_t                configLowBatteryMinutes;
    int16_t                 lowTempThreshold;
    int16_t                 highTempThreshold;
    uint32_t                lowTempDwellTripPointSeconds;
    uint32_t                highTempDwellTripPointSeconds;
    uint8_t                 deviceTempAlarmMask;
    */

    printf("XBB Configuration:\n");
    printf("\tPowered Device Idle Power 1: %" PRIu32 "\n", config.poweredDeviceIdlePower1);
    printf("\tPowered Device Idle Power 2: %" PRIu32 "\n", config.poweredDeviceIdlePower2);
    printf("\tConfig Low Battery Minutes: %" PRIu32 "\n", config.configLowBatteryMinutes);
    printf("\tLow Temp Threshold (celcius): %" PRId16 "\n", config.lowTempThreshold);
    printf("\tHigh Temp Threshold (celcius): %" PRId16 "\n", config.highTempThreshold);
    printf("\tLow Temp Dwell Trip Point Seconds: %" PRIu32 "\n", config.lowTempDwellTripPointSeconds);
    printf("\tHigh Temp Dwell Trip Point Seconds: %" PRIu32 "\n", config.highTempDwellTripPointSeconds);
    printf("\tDevice Temp Alarm Mask: %" PRIu8 "\n", config.deviceTempAlarmMask);
}

static void setConfig(const char *json)
{
    XbbConfiguration config;
    memset(&config, 0, sizeof(XbbConfiguration));

    int numParsed = sscanf(json,"%" SCNu32 ",%" SCNu32 " ,%" SCNu32 ",%" SCNd16 ",%" SCNd16 ",%" SCNu32 ",%" SCNu32 ",%" SCNu8,
           &config.poweredDeviceIdlePower1,
           &config.poweredDeviceIdlePower2,
           &config.configLowBatteryMinutes,
           &config.lowTempThreshold,
           &config.highTempThreshold,
           &config.lowTempDwellTripPointSeconds,
           &config.highTempDwellTripPointSeconds,
           &config.deviceTempAlarmMask);

    if(numParsed != 8) // the number of elements in XbbConfiguration
    {
        printf("Invalid input.\n");
    }
    else
    {
        xbbSetConfig(&config);
    }
}

static void discover()
{
    printf("Discovering XBB (removing previously paired XBB if applicable)\n");
    xbbDiscover(300);
}

static void getAlarms()
{
    XbbAlarmInfo *alarmInfos = NULL;
    uint16_t numAlarmInfos;
    if(xbbGetAlarms(&alarmInfos, &numAlarmInfos))
    {
        printf("Got %d alarms:\n", numAlarmInfos);
        for (int i = 0; i < numAlarmInfos; i++)
        {
            printf("\tAlarmType: ");
            switch(alarmInfos[i].type)
            {
                case alarmTypeLowTemp:
                    printf("low temp, ");
                    break;
                case alarmTypeHighTemp:
                    printf("high temp, ");
                    break;
                case alarmTypeBatteryBad:
                    printf("battery bad, ");
                    break;
                case alarmTypeBatteryLow:
                    printf("battery low, ");
                    break;
                case alarmTypeChargingSystemBad:
                    printf("charging system bad, ");
                    break;
                case alarmTypeBatteryMissing:
                    printf("battery missing, ");
                    break;

                default:
                    printf("UNKNOWN!, ");
                    break;
            }

            printf("TimeStamp: %" PRIu32 "\n", alarmInfos[i].timestamp);
        }
    }
    else
    {
        printf("Failed to retrieve alarms\n");
    }

    if(alarmInfos != NULL)
    {
        free(alarmInfos);
    }
}

static void sirenStart(char *args)
{
    uint16_t frequency;
    uint8_t volumePercent;
    uint16_t durationSeconds;
    SirenTemporalPattern temporalPattern;
    uint8_t numPulses;
    uint16_t onPhaseDurationMillis;
    uint16_t offPhaseDurationMillis;
    uint16_t pauseDurationMillis;

    int numParsed = sscanf(args,"%" SCNu16 ",%" SCNu8 " ,%" SCNu16 ",%" SCNd32 ",%" SCNd8 ",%" SCNu16 ",%" SCNu16 ",%" SCNu16,
                           &frequency,
                           &volumePercent,
                           &durationSeconds,
                           (int*)&temporalPattern,
                           &numPulses,
                           &onPhaseDurationMillis,
                           &offPhaseDurationMillis,
                           &pauseDurationMillis);

    if(numParsed != 8) // the number of arguments required
    {
        printf("Invalid input.\n");
    }
    else
    {
        if(xbbSirenStart(frequency,
                volumePercent,
                durationSeconds,
                temporalPattern,
                numPulses,
                onPhaseDurationMillis,
                offPhaseDurationMillis,
                pauseDurationMillis))
        {
            printf("Success.\n");
        }
        else
        {
            printf("Failed.\n");
        }
    }
}

static void sirenStop()
{
    if(xbbSirenStop())
    {
        printf("Success.\n");
    }
    else
    {
        printf("Failed.\n");
    }
}

static void sirenMute()
{
    if(xbbSirenMute())
    {
        printf("Success.\n");
    }
    else
    {
        printf("Failed.\n");
    }
}

static void isBatteryPaired()
{
    bool paired;
    if(xbbIsBatteryPaired(&paired))
    {
        if(paired)
        {
            printf("Battery is paired\n");
        }
        else
        {
            printf("Battery is NOT paired\n");
        }
    }
    else
    {
        printf("Failed to determine if battery is paired\n");
    }
}

//return 0 to indicate quit/exit request
static int handleCommand(char *line)
{
    int rc = 1;
    char *args = line;

    while (*args++ != '\0')
    {
        if (*args == ' ')
        {
            *args++ = '\0';
            break;
        }
    }

    if (strlen(line) == 0)
    {
        //do nothing
    }
    else if (strcasecmp("getStatus", line) == 0)
    {
        getStatus();
    }
    else if (strcasecmp("getConfig", line) == 0)
    {
        getConfig();
    }
    else if (strcasecmp("setConfig", line) == 0)
    {
        setConfig(args);
    }
    else if (strcasecmp("discover", line) == 0)
    {
        discover();
    }
    else if (strcasecmp("getAlarms", line) == 0)
    {
        getAlarms();
    }
    else if (strcasecmp("sirenStart", line) == 0)
    {
        sirenStart(args);
    }
    else if (strcasecmp("sirenStop", line) == 0)
    {
        sirenStop();
    }
    else if (strcasecmp("sirenMute", line) == 0)
    {
        sirenMute();
    }
    else if (strcasecmp("isBatteryPaired", line) == 0)
    {
        isBatteryPaired();
    }
    else if (strcasecmp("help", line) == 0)
    {
        showInteractiveHelp();
    }
    else if (strcasecmp("quit", line) == 0 || strcasecmp("exit", line) == 0)
    {
        rc = 0;
    }
    else
    {
        printf("Unknown command.");
    }

    return rc;
}
