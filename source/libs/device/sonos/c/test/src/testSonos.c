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

#include <sonos/sonos.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

static void speakerFoundCallback(const char *macAddress, const char *ipAddress)
{
    printf("speaker found: %s, %s\n", macAddress, ipAddress);
}

static void testSpeakerDiscovery()
{
    sonosStartDiscovery(speakerFoundCallback);
    usleep(5000000);
    sonosStopDiscovery();
}

pthread_cond_t monitoringCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t monitoringMtx = PTHREAD_MUTEX_INITIALIZER;

static void speakerIpAddressChangedCallback(const char *macAddress, char *newIpAddress)
{
    printf("speakerIpAddressChanged: %s is now at %s\n", macAddress, newIpAddress);
}

//Start monitoring speakers
void testMonitoring(const char *macAddress, const char *ipAddress)
{
    pthread_mutex_lock(&monitoringMtx);

    sonosStartMonitoring(macAddress, ipAddress, speakerIpAddressChangedCallback);

    pthread_cond_wait(&monitoringCond,&monitoringMtx);

    pthread_mutex_unlock(&monitoringMtx);
}

int main(int argc, char **argv)
{
//    testSpeakerDiscovery();

    testMonitoring("94:9f:3e:5:d9:12", "10.0.1.85");

    return 0;
}
