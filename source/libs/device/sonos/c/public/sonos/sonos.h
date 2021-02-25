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
 * sonos.h
 *
 * Sonos Library
 *
 * Author: tlea
 *-----------------------------------------------*/

#ifndef SONOS_H
#define SONOS_H

#include <stdbool.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icStringHashMap.h>

typedef struct
{
    char *id;
} SonosSpeaker;

typedef void (*SonosDiscoverCallback)(const char *macAddress, const char *ipAddress);

/*
 * Start discovering Sonos speakers on the local network.
 */
bool sonosStartDiscovery(SonosDiscoverCallback callback);

/*
 * Stop discovering Sonos speakers on the local network.
 */
bool sonosStopDiscovery();

/*
 * Callback invoked while monitoring and we find out that the speaker's ip address changed
 */
typedef void (*SonosIpAddressChangedCallback)(const char *macAddress, char *newIpAddress);

/*
 * Start monitoring a speaker for changes and problems
 */
bool sonosStartMonitoring(const char *macAddress,
                          const char *ipAddress,
                          SonosIpAddressChangedCallback ipAddressChangedCallback);
/*
 * Stop monitoring a speaker for changes and problems
 */
bool sonosStopMonitoring(const char *ipAddress);

/*
 * Play an audio clip via URL
 */
bool sonosPlayClip(const char *ipAddress, const char *clipUrl);

/*
 * Release the resources used by the provided speaker
 */
void sonosSpeakerDestroy(SonosSpeaker *speaker);

#endif //SONOS_H
