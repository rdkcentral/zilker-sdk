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
 * ohcmDiscover.h
 *
 * Open Home Camera Discover
 *
 * Author: jgleason
 *-----------------------------------------------*/

#ifndef OHCM_DISCOVER_H
#define OHCM_DISCOVER_H

#include <stdbool.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>
#include <icTypes/icStringHashMap.h>

/*
 * enumerated list of possible return codes from
 * the Open Home Camera library.
 */
typedef enum {
    OPEN_HOME_CAMERA_CODE_SUCCESS = 0,
    OPEN_HOME_CAMERA_CODE_ERROR,
    OPEN_HOME_CAMERA_DISCOVERY_ERROR,
    OPEN_HOME_CAMERA_CODE_CONNECTION_ERROR,
    OPEN_HOME_CAMERA_CODE_HTTP_ERROR,
    OPEN_HOME_CAMERA_CODE_XML_ERROR,
    OPEN_HOME_CAMERA_CODE_INVALID_STREAM_ERROR,
    OPEN_HOME_CAMERA_CODE_INVALID_PARAMETER,

    /* the final entry is just for looping over the enum */
    OPEN_HOME_CAMERA_CODE_LAST
} openHomeCameraCode;

/*
 * callback when a camera device is located
 */
typedef void (*ohcmDiscoveredCallback)(const char *ipAddress, const char *macAddress);

/*
 *
 * PUBLIC APIs
 *
 */
openHomeCameraCode ohcmDiscoverStart(ohcmDiscoveredCallback callback);
void ohcmDiscoverStop();

#endif //OHCM_DISCOVER_H
