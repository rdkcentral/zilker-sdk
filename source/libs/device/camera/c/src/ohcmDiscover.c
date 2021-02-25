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
* ohcmDiscover.c
*
* This file contains the public functions for
* discovering an Open Home Camera.
* These functions use SSDP to discover cameras.
*
* Author: jgleason, tlea
*-----------------------------------------------*/

#include <stdio.h>
#include <inttypes.h>
#include <libxml/tree.h>
#include <ssdp/ssdp.h>
#include <openHomeCamera/ohcmDiscover.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>

/* defined values */
#define OH_CAMERA_LOG_TAG     "ohcmDiscover"

/* private function prototypes */
static ohcmDiscoveredCallback clientCallback = NULL;
static uint32_t ssdpHandle = 0;

/*---------------------------------------------------------------------------------------
 * ohcmDiscoverStop : free the linked list and components from an open
 * home camera discover
 *---------------------------------------------------------------------------------------*/
void ohcmDiscoverStop()
{
    icLogDebug(OH_CAMERA_LOG_TAG, "ohcmDiscoverStop");

    ssdpDiscoverStop(ssdpHandle);
    ssdpHandle = 0;
}

static void discoverCallback(SsdpDevice *device)
{
    /* Call the client back, and provide the IP address of the camera */
    // Mostly just for mock device testing if the port is non-default port then include it in with the IP
    // port 6789: SSDP (http)
    char *ipAddress = device->ipAddress;
    if (device->port != 80 && device->port != 443 && device->port != 6789)
    {
        ipAddress = stringBuilder("%s:%" PRIu32, device->ipAddress, device->port);
    }
    clientCallback((const char *)ipAddress, (const char *)device->macAddress);
    if (ipAddress != device->ipAddress)
    {
        free(ipAddress);
    }
}

/*---------------------------------------------------------------------------------------
* openHomeCameraDiscover : Discover Open Home Cameras on the network.
*
* Camera discovery is achieved using UPnP's SSDP (Simple Service Discovery Protocol).
* The cameras are searched for using the USN (Unique Service Name) specified by Icontrol.
*
* Inputs:  callback - the callback function to be invoked as cameras are found
*
* Returns: openHomeCameraCode  - return code
*
*---------------------------------------------------------------------------------------*/
openHomeCameraCode ohcmDiscoverStart(ohcmDiscoveredCallback callback)
{
    openHomeCameraCode result = OPEN_HOME_CAMERA_CODE_ERROR;
    icLogDebug(OH_CAMERA_LOG_TAG, "Starting SSDP camera discovery scan");

    clientCallback = callback;

    /* start discovering cameras using SSDP */
    uint32_t handle = ssdpDiscoverStart(CAMERA, discoverCallback);
    if (handle == 0)
    {
        icLogError(OH_CAMERA_LOG_TAG, "Failed to start camera discovery.");
        clientCallback = NULL;
    }
    else
    {
        // save the handle and set the return to SUCCESS
        ssdpHandle = handle;
        result = OPEN_HOME_CAMERA_CODE_SUCCESS;
    }

    return result;
}



