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

#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <rtcoaWifi/rtcoaWifi.h>

pthread_cond_t monitoringCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t monitoringMtx = PTHREAD_MUTEX_INITIALIZER;

static void thermostatFoundCallback(const char *macAddress, const char *ipAddress)
{
    printf("tstat found: %s, %s\n", macAddress, ipAddress);
}

static void testTstatDiscovery()
{
    rtcoaWifiThermostatStartDiscovery(thermostatFoundCallback);
    usleep(3000000);
    rtcoaWifiThermostatStopDiscovery();
}


static void tstatChangedCallback(const char *macAddress, const char *ipAddress)
{
    pthread_mutex_lock(&monitoringMtx);
    printf("tstatChanged: %s\n", macAddress);
    pthread_cond_signal(&monitoringCond);
    pthread_mutex_unlock(&monitoringMtx);
}

static void tstatIpAddressChangedCallback(const char *macAddress, const char *newIpAddress)
{
    printf("tstatIpAddressChanged: %s is now at %s\n", macAddress, newIpAddress);
}

void testMonitoring(const char *macAddress, const char *ipAddress)
{
    pthread_mutex_lock(&monitoringMtx);

    rtcoaWifiThermostatStartMonitoring(macAddress, ipAddress, tstatChangedCallback, tstatIpAddressChangedCallback);

    pthread_cond_wait(&monitoringCond,&monitoringMtx);

    pthread_mutex_unlock(&monitoringMtx);

    rtcoaWifiThermostatStopMonitoring(ipAddress);
}

int main(int argc, char **argv)
{
    testMonitoring("themac", "172.16.12.116");


//    testTstatDiscovery();
//    printf("model = %s\n", rtcoaWifiThermostatGetModel("172.16.12.116"));

//    rtcoaWifiThermostatSetMode("172.16.12.116", RTCOA_WIFI_TSTAT_OP_MODE_OFF);
//    RtcoaWifiThermostatState *state = rtcoaWifiThermostatStateGet("172.16.12.116");

    return 0;
}
