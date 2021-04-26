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
 * ssdp.h
 *
 * SSDP Functions
 *
 * Author: jgleason, tlea
 *-----------------------------------------------*/

#ifndef IC_SSDP_H
#define IC_SSDP_H

#include <stdint.h>
#include <icTypes/icLinkedList.h>


/*
 * the 'type' of device to search for
 */
typedef enum {
    CAMERA,
    PHILIPSHUE, // lighting device
    RTCOA,      // thermostat device
    SONOS,      // sonos speaker device
    WIFI,       // NOTE: not supported yet
    ROUTER,     // NOTE: not supported yet
    ANY
} SearchType;

/*
 * Describe a discovered device.
 */
typedef struct _SsdpDevice {
    char         *upnpURL;        // URL to access the UPnP information
    char         *upnpST;         // raw UPnP service type
    char         *upnpUSN;        // raw UPnP unique service name
    char         *upnpServer;     // raw UPnP 'SERVER' field (optional)
    char         *marvellServiceName; // if using Marvell's discovery format, this is the returned service name
    char         ipAddress[32];   // derived from the 'upnpURL'
    char         macAddress[20];  // captured from ARP tables
    unsigned int port;            // derived from the 'upnpURL'
    SearchType   type;            // derived 'SearchType' after examining the 'upnpST'
} SsdpDevice;

/*
 * callback when a device is discovered.  receiver should copy off any values
 * it needs as 'device' will be relased immediately after the callback
 */
typedef void (*SsdpDiscoverCallback)(SsdpDevice *device);

/*---------------------------------------------------------------------------------------
 * ssdpDiscoverStart : Start SSDP discovery
 *
 * This will start background discovery of specific devices.  As devices are found, the
 * provided callback will be invoked.
 *
 * Caller must stop discovery with ssdpDiscoverStop.
 *
 * @return handle that can be used for the 'stop' call.  if <= 0, then the discover failed to start.
 *---------------------------------------------------------------------------------------*/
uint32_t ssdpDiscoverStart(SearchType type, SsdpDiscoverCallback callback);

/*---------------------------------------------------------------------------------------
 * ssdpDiscoverStop : Stop SSDP discovery
 *
 * This must be called after a successful discovery start with ssdpDiscoverStart.
 *---------------------------------------------------------------------------------------*/
void ssdpDiscoverStop(uint32_t handle);

/*---------------------------------------------------------------------------------------
 * ssdpRecoverIpAddress : Attempt to locate a device matching the provided mac address via SSDP discovery.
 *
 * This function is typically used to locate devices that had their ip address change due to DHCP.
 * If the device is found, the ipAddress will point to the new/current ip address.
 *
 * @return true if the device was found
 *---------------------------------------------------------------------------------------*/
bool ssdpRecoverIpAddress(SearchType type, const char *macAddress, char **ipAddress, uint32_t timeoutSeconds);

#endif // IC_SSDP_H
