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
 * runtimeAttributes.h
 *
 * functions to help describe the runtime environment
 *
 * Author: jelderton - 8/27/15
 *-----------------------------------------------*/

#ifndef ZILKER_RUNTIME_H
#define ZILKER_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>

typedef struct _sysVersion
{
    char    versionString[128];
    char    dateStamp[128];
    char    serverVersionString[128];
    char    lastCompatibleVersion[128];
    char    builder[128];
    uint8_t  majorVersion;
    uint8_t  minorVersion;
    uint8_t  maintenanceVersion;
    uint64_t hotFixVersion;
    uint64_t buildNumber;
} systemVersion;

/*
 * populates the 'systemVersion' object with data
 * from our '$HOME/etc/version' file
 */
bool getSystemVersion(systemVersion *version);

/*
 * return the CPE ID (the MAC Address without colon chars)
 * this string MUST NOT BE RELEASED
 */
const char *getSystemCpeId();

/*
 * return the "lower case" CPE ID
 * this string MUST NOT BE RELEASED
 */
const char *getSystemCpeIdLowerCase();

/*
 * return the "upper case" CPE ID
 * this string MUST NOT BE RELEASED
 */
const char *getSystemCpeIdUpperCase();

/*
 * return the System MAC Address
 * this string MUST NOT BE RELEASED
 */
const char *getSystemMacAddress();

/*
 * return the Wifi MAC Address
 * this string MUST NOT BE RELEASED
 */
const char *getWifiMacAddress();

/*
 * return the Serial Number Label (MUST NOT BE RELEASED)
 */
const char *getSystemSerialLabel();

/*
 * return the Manufacturer Label (MUST NOT BE RELEASED)
 */
const char *getSystemManufacturerLabel();

/*
 * return the Model Label (MUST NOT BE RELEASED)
 */
const char *getSystemModelLabel();

/*
 * return the Hardware Version Label (MUST NOT BE RELEASED)
 */
const char *getSystemHardwareVersionLabel();


#endif //ZILKER_RUNTIME_H
