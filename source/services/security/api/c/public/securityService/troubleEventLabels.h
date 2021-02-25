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
 * troubleEventLabels.h
 *
 * Set of static strings to allow logging of trouble
 * type & subtype (since these are easier to read then numbers)
 *
 * Author: jelderton - 4/26/16
 *-----------------------------------------------*/

#ifndef ZILKER_TROUBLEEVENTLABELS_H
#define ZILKER_TROUBLEEVENTLABELS_H

#include <stdlib.h>
#include "securityService_event.h"

/*
 * NULL terminated array that should correlate to the
 * 'troubleType' enumeration
 */
static const char *troubleTypeLabels[] = {
    "TROUBLE_TYPE_NONE",        // TROUBLE_TYPE_NONE
    "TROUBLE_TYPE_GENERIC",     // TROUBLE_TYPE_GENERIC
    "TROUBLE_TYPE_SYSTEM",      // TROUBLE_TYPE_SYSTEM
    "TROUBLE_TYPE_NETWORK",     // TROUBLE_TYPE_NETWORK
    "TROUBLE_TYPE_POWER",       // TROUBLE_TYPE_POWER
    "TROUBLE_TYPE_DEVICE",      // TROUBLE_TYPE_DEVICE
    NULL
};

/*
 * NULL terminated array that should correlate to the
 * 'troubleSubtype' enumeration
 */
static const char *troubleSubtypeLabels[] = {
    "TROUBLE_SUBTYPE_NONE",             // TROUBLE_SUBTYPE_NONE
    "TROUBLE_SUBTYPE_SYSTEM_SOFTWARE",  // TROUBLE_SUBTYPE_SYSTEM_SOFTWARE
    "TROUBLE_SUBTYPE_SYSTEM_HARDWARE",  // TROUBLE_SUBTYPE_SYSTEM_HARDWARE
    "TROUBLE_SUBTYPE_SYSTEM_TAMPER",    // TROUBLE_SUBTYPE_SYSTEM_TAMPER
    "TROUBLE_SUBTYPE_SYSTEM_STORAGE",   // TROUBLE_SUBTYPE_SYSTEM_STORAGE
    "TROUBLE_SUBTYPE_SYSTEM_GENERIC",   // TROUBLE_SUBTYPE_SYSTEM_GENERIC

    "TROUBLE_SUBTYPE_NETWORK_WIFI",     // TROUBLE_SUBTYPE_NETWORK_WIFI
    "TROUBLE_SUBTYPE_NETWORK_ETHERNET", // TROUBLE_SUBTYPE_NETWORK_ETHERNET
    "TROUBLE_SUBTYPE_NETWORK_CELLULAR", // TROUBLE_SUBTYPE_NETWORK_CELLULAR
    "TROUBLE_SUBTYPE_NETWORK_ZIGBEE",   // TROUBLE_SUBTYPE_NETWORK_ZIGBEE

    "TROUBLE_SUBTYPE_POWER_AC",         // TROUBLE_SUBTYPE_POWER_AC
    "TROUBLE_SUBTYPE_POWER_BATTERY",    // TROUBLE_SUBTYPE_POWER_BATTERY

    "TROUBLE_SUBTYPE_DEVICE_TAMPER",    // TROUBLE_SUBTYPE_DEVICE_TAMPER
    "TROUBLE_SUBTYPE_DEVICE_COMM",      // TROUBLE_SUBTYPE_DEVICE_COMM
    "TROUBLE_SUBTYPE_DEVICE_BATT",      // TROUBLE_SUBTYPE_DEVICE_BATT
    "TROUBLE_SUBTYPE_DEVICE_DIRTY",     // TROUBLE_SUBTYPE_DEVICE_DIRTY
    "TROUBLE_SUBTYPE_DEVICE_TEMP",      // TROUBLE_SUBTYPE_DEVICE_TEMP
    "TROUBLE_SUBTYPE_DEVICE_BOOTLOADER",// TROUBLE_SUBTYPE_DEVICE_BOOTLOADER
    "TROUBLE_SUBTYPE_DEVICE_ROUTER",    // TROUBLE_SUBTYPE_DEVICE_ROUTER
    "TROUBLE_SUBTYPE_DEVICE_EOL",       // TROUBLE_SUBTYPE_DEVICE_EOL
    "TROUBLE_SUBTYPE_DEVICE_LOCK_BOLT", // TROUBLE_SUBTYPE_DEVICE_LOCK_BOLT
    "TROUBLE_SUBTYPE_DEVICE_PIN",       // TROUBLE_SUBTYPE_DEVICE_PIN
    NULL
};

/*
 * NULL terminated array that should correlate to the
 * 'troubleDetails' enumeration
 */
static const char *troubleDetailLabels[] = {
        "TROUBLE_DETAIL_NONE",          // TROUBLE_DETAIL_NONE
        "TROUBLE_DETAIL_LOW",           // TROUBLE_DETAIL_LOW
        "TROUBLE_DETAIL_HIGH",          // TROUBLE_DETAIL_HIGH
        "TROUBLE_DETAIL_BAD",           // TROUBLE_DETAIL_BAD
        "TROUBLE_DETAIL_MISSING",       // TROUBLE_DETAIL_MISSING
        "TROUBLE_DETAIL_JAM",           // TROUBLE_DETAIL_JAM
        NULL
};

#endif //ZILKER_TROUBLEEVENTLABELS_H
