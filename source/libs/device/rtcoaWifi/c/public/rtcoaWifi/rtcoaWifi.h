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

/*-----------------------------------------------
 * rtcoaWifi.h
 *
 * RTCoA over Wifi (IP) Library
 *
 * Author: tlea
 *-----------------------------------------------*/

#ifndef RTCOAWIFI_H
#define RTCOAWIFI_H

#include <stdbool.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icStringHashMap.h>

typedef struct
{
    float temp;
    float t_cool;
    float t_heat;
    int tmode;
    int fmode;
    int override;
    int hold;
    int program_mode;
    int tstate;
    int fstate;
} RtcoaWifiThermostatState;

typedef enum
{
    RTCOA_WIFI_TSTAT_OP_MODE_OFF,
    RTCOA_WIFI_TSTAT_OP_MODE_HEAT,
    RTCOA_WIFI_TSTAT_OP_MODE_COOL,
} RtcoaWifiThermostatOperatingMode;

typedef void (*RtcoaWifiThermostatDiscoverCallback)(const char *macAddress, const char *ipAddress);

/*
 * Start discovering RTCoA wifi thermostats on the local network.
 */
bool rtcoaWifiThermostatStartDiscovery(RtcoaWifiThermostatDiscoverCallback callback);

/*
 * Stop discovering RTCoA wifi thermostats on the local network.
 */
bool rtcoaWifiThermostatStopDiscovery();

/*
 * Retrieve the state of the thermostat
 */
RtcoaWifiThermostatState* rtcoaWifiThermostatStateGetState(const char *ipAddress);

/*
 * Retrieve the model of the thermostat
 *
 * caller frees
 */
char* rtcoaWifiThermostatGetModel(const char *ipAddress);

/*
 * Release the resources used by the provided thermostat state
 */
void rtcoaWifiThermostatStateDestroyState(RtcoaWifiThermostatState *thermostat);

typedef void (*RtcoaWifiThermostatStateChangedCallback)(const char *macAddress, const char *ipAddress);
typedef void (*RtcoaWifiThermostatIpChangedCallback)(const char *macAddress, const char *newIpAddress);

/*
 * Start monitoring a thermostat for changes and problems
 */
bool rtcoaWifiThermostatStartMonitoring(const char *macAddress,
                                        const char *ipAddress,
                                        RtcoaWifiThermostatStateChangedCallback stateChangedCallback,
                                        RtcoaWifiThermostatIpChangedCallback ipChangedCallback);

/*
 * Stop monitoring a thermostat for changes and problems
 */
bool rtcoaWifiThermostatStopMonitoring(const char *ipAddress);

/*
 * Set the overall system mode of the thermostat
 */
bool rtcoaWifiThermostatSetMode(const char *ipAddress, RtcoaWifiThermostatOperatingMode mode);

/*
 * Set the cool setpoint
 */
bool rtcoaWifiThermostatSetCoolSetpoint(const char *ipAddress, float newTemp);

/*
 * Set the heat setpoint
 */
bool rtcoaWifiThermostatSetHeatSetpoint(const char *ipAddress, float newTemp);

/*
 * Turn on or off 'simple' mode
 */
bool rtcoaWifiThermostatSetSimpleMode(const char *ipAddress, bool enabled);

#endif //RTCOAWIFI_H
