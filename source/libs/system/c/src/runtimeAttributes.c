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
 * runtimeAttributes.c
 *
 * functions to help describe the runtime environment
 *
 * Author: jelderton - 8/27/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>
#include <icSystem/runtimeAttributes.h>
#include <propsMgr/paths.h>
#include <sysinfo/sysinfoHAL.h>

#define RUNTIME_LOG                 "runtime"
#define VERSION_FILE                "version"
#define SYSTEM_MAC_ADDRESS_FILE     "macAddress"
#define WIFI_MAC_ADDRESS_FILE       "macAddress.wifi"

/*
 * private variables
 */
static pthread_mutex_t  RUNTIME_MTX = PTHREAD_MUTEX_INITIALIZER;
static char *etcConfigDir = NULL;
static char *cpeId = NULL;
static char *cpeIdLower = NULL;
static char *cpeIdUpper = NULL;
static char *systemMacAddress = NULL;
static char *wifiMacAddress = NULL;
static char *serialNum = NULL;
static char *manufacturer = NULL;
static char *model = NULL;
static char *hwVersion = NULL;

/*
 * private functions
 */
static void getEtcConfigDir();

static const char* getSystemMacAddressLocked(void)
{
    const char* ret;

    if (systemMacAddress == NULL)
    {
        // first ask propsService for the configuration directory
        //
        getEtcConfigDir();

        if (etcConfigDir != NULL)
        {
            // need to read the effective "/opt/etc/macAddress".
            //
            char path[1024];
            sprintf(path, "%s/%s", etcConfigDir, SYSTEM_MAC_ADDRESS_FILE);

            // get the contents
            //
            char *contents = readFileContentsWithTrim(path);
            if (contents != NULL)
            {
                // save the MAC
                //
                systemMacAddress = contents;

                icLogDebug(RUNTIME_LOG, "MAC Address calculated as %s", systemMacAddress);
            }
            else
            {
                // file missing or unable to be read.  just return an empty string
                //
                icLogWarn(RUNTIME_LOG, "unable to read macAddress file %s", path);
            }
        }
        else
        {
            icLogWarn(RUNTIME_LOG, "unable to get system configuration dir, so cannot obtain CPE ID");
        }
    }

    ret = systemMacAddress;

    return ret;
}

static const char* getWifiMacAddressLocked(void)
{
    const char* ret;

    if (wifiMacAddress == NULL)
    {
        // first ask propsService for the configuration directory
        //
        getEtcConfigDir();

        if (etcConfigDir != NULL)
        {
            // need to read the effective "/opt/etc/macAddress.wifi".
            //
            char path[1024];
            sprintf(path, "%s/%s", etcConfigDir, WIFI_MAC_ADDRESS_FILE);

            // get the contents
            //
            char *contents = readFileContentsWithTrim(path);
            if (contents != NULL)
            {
                // save the wifi MAC
                //
                wifiMacAddress = contents;

                icLogDebug(RUNTIME_LOG, "Wifi MAC Address calculated as %s", wifiMacAddress);
            }
            else
            {
                // file missing or unable to be read.  just return an empty string
                //
                icLogWarn(RUNTIME_LOG, "unable to read wifi macAddress file %s", path);
            }
        }
        else
        {
            icLogWarn(RUNTIME_LOG, "unable to get system configuration dir, so cannot obtain Wifi MAC address");
        }
    }

    ret = wifiMacAddress;

    return ret;
}

/*
 * populates the 'systemVersion' object with data
 * from our '$HOME/etc/version' file
 */
bool getSystemVersion(systemVersion *version)
{
    // clear version values (all static, nothing dynamic)
    //
    memset(version, 0, sizeof(systemVersion));

    // first ask propsService for the configuration directory
    //
    char *homeConfigPath = getStaticConfigPath();
    if (homeConfigPath == NULL)
    {
        icLogWarn(RUNTIME_LOG, "unable to get system configuration dir, so cannot obtain System Version");
        return false;
    }

    // need to read the effective "/icontrol/etc/version".
    //
    char path[1024];
    sprintf(path, "%s/%s", homeConfigPath, VERSION_FILE);

    // get the contents
    //
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        // file missing or unable to be rad.  just return an empty string
        //
        icLogWarn(RUNTIME_LOG, "unable to read version file %s", path);
        free(homeConfigPath);
        return false;
    }

    // init vars before reading
    //
    bool retVal = false;
    size_t amount = 0;
    char buffer[2048];
    memset(buffer, 0, sizeof(char) * 2048);

    // read up to 2k of info
    //
    if ((amount = fread(buffer, sizeof(char), 2047, fp)) > 0)
    {
        // ensure buffer is null terminated
        //
        buffer[amount] = '\0';

        // each line is a "key = value" format
        //
        char *line, *key, *value;
        char *savePtr1, *savePtr2;
        char *tokenize = buffer;

        // break the file into lines (i.e. tokenize on newline)
        //
        while ((line = strtok_r(tokenize, "\n", &savePtr1)) != NULL)
        {
            // should have a line, so tokenize on key: value
            //
            key = strtok_r(line, ":", &savePtr2);
            value = strtok_r(NULL, ":", &savePtr2);
            if (key != NULL && value != NULL)
            {
                // skip over the space after the ':'
                //
                value++;

                if (strcmp(key, "release_ver") == 0)
                {
                    stringToUint8((const char *)value, &version->majorVersion);
                }
                else if (strcmp(key, "service_ver") == 0)
                {
                    stringToUint8((const char *)value, &version->minorVersion);
                }
                else if (strcmp(key, "maintenance_ver") == 0)
                {
                    stringToUint8((const char *)value, &version->maintenanceVersion);
                }
                else if (strcmp(key, "hot_fix_ver") == 0)
                {
                    stringToUint64(value, &version->hotFixVersion);
                }
                else if (strcmp(key, "svn_build") == 0)
                {
                    stringToUint64((const char *)value, &version->buildNumber);
                }
                else if (strcmp(key, "build_by") == 0)
                {
                    safeStringCopy(version->builder, 128, value);
                }
                else if (strcmp(key,"server_version") == 0)
                {
                    safeStringCopy(version->serverVersionString, 128, value);
                }
                else if (strcmp(key,"lastCompatibleVersion") == 0)
                {
                    safeStringCopy(version->lastCompatibleVersion, 128, value);
                }
                else if (strcmp(key, "build_date") == 0)
                {
                    safeStringCopy(version->dateStamp, 128, value);

                    // dates have : chars in them, so keep getting tokens
                    //
                    while ((value = strtok_r(NULL, ":", &savePtr2)) != NULL)
                    {
                        safeStringAppend(version->dateStamp, 128, ":");
                        safeStringAppend(version->dateStamp, 128, value);
                    }
                }
                else if (strcmp(key, "LONG_VERSION") == 0)
                {
                    safeStringCopy(version->versionString, 128, value);
                }
            }

            // reset tokenize for next 'line'
            //
            tokenize = NULL;
        }

        retVal = true;
    }

    // cleanup
    //
    fclose(fp);
    free(homeConfigPath);
    return retVal;
}

/*
 * return the CPE ID (the MAC Address without colon chars)
 * this string MUST NOT BE RELEASED
 */
const char *getSystemCpeId()
{
    const char* ret;

    // return cached value if already did this
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    if (cpeId == NULL)
    {
        const char* mac = getSystemMacAddressLocked();

        if ((mac != NULL) && (mac[0] != '\0'))
        {
            // now we need to remove the colon chars
            //
            size_t len = strlen(mac);
            cpeId = (char *)malloc(len + 1);
            memset(cpeId, 0, len+1);

            int index = 0;
            int offset = 0;
            while (index < (int)len)
            {
                // append to 'cpeId' if a digit
                //
                if (isxdigit(mac[index]))
                {
                    cpeId[offset] = mac[index];
                    offset++;
                }

                index++;
            }

            icLogDebug(RUNTIME_LOG, "cpeId calculated as %s", cpeId);
        }
        else
        {
            icLogWarn(RUNTIME_LOG, "unable to read macAddress file");
        }
    }

    ret = cpeId;
    pthread_mutex_unlock(&RUNTIME_MTX);

    return (ret == NULL) ? "" : ret;
}

/*
 * return the "lower case" CPE ID
 * this string MUST NOT BE RELEASED
 */
const char *getSystemCpeIdLowerCase()
{
    // return cached value if already did this
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    if (cpeIdLower != NULL)
    {
        pthread_mutex_unlock(&RUNTIME_MTX);
        return (const char *)cpeIdLower;
    }
    pthread_mutex_unlock(&RUNTIME_MTX);

    // get CPE ID
    //
    const char *id = getSystemCpeId();
    if (id != NULL)
    {
        // dup the string, then convert to lower case
        //
        char *buffer = strdup(id);
        stringToLowerCase(buffer);

        // save as our global
        //
        pthread_mutex_lock(&RUNTIME_MTX);
        cpeIdLower = buffer;
        pthread_mutex_unlock(&RUNTIME_MTX);

        return (const char *)cpeIdLower;
    }

    return "";
}

/*
 * return the "upper case" CPE ID
 * this string MUST NOT BE RELEASED
 */
const char *getSystemCpeIdUpperCase()
{
    // return cached value if already did this
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    if (cpeIdUpper != NULL)
    {
        pthread_mutex_unlock(&RUNTIME_MTX);
        return (const char *)cpeIdUpper;
    }
    pthread_mutex_unlock(&RUNTIME_MTX);

    // get CPE ID
    //
    const char *id = getSystemCpeId();
    if (id != NULL)
    {
        // dup the string, then convert to upper case
        //
        char *buffer = strdup(id);
        stringToUpperCase(buffer);

        // save as our global
        //
        pthread_mutex_lock(&RUNTIME_MTX);
        cpeIdUpper = buffer;
        pthread_mutex_unlock(&RUNTIME_MTX);

        return (const char *)cpeIdUpper;
    }

    return "";
}

/*
 * return the System MAC Address
 * this string MUST NOT BE RELEASED
 */
const char *getSystemMacAddress()
{
    const char* ret;

    // return cached value if already did this
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    ret = getSystemMacAddressLocked();
    pthread_mutex_unlock(&RUNTIME_MTX);

    return (ret == NULL) ? "" : ret;
}

/*
 * return the Wifi MAC Address
 * this string MUST NOT BE RELEASED
 */
const char *getWifiMacAddress()
{
    const char* ret;

    // return cached value if already did this
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    ret = getWifiMacAddressLocked();
    pthread_mutex_unlock(&RUNTIME_MTX);

    return (ret == NULL) ? "" : ret;
}

/*
 * return the Serial Number Label (MUST NOT BE RELEASED)
 */
const char *getSystemSerialLabel()
{
    // use cached version
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    if (serialNum != NULL)
    {
        pthread_mutex_unlock(&RUNTIME_MTX);
        return serialNum;
    }

    // ask the sysInfo HAL for the serial number
    //
    static char serial[64];
    if (hal_sysinfo_get_serialnum(serial, 63) == 0)
    {
        // create cached copy
        //
        serialNum = trimString(serial);

        pthread_mutex_unlock(&RUNTIME_MTX);
        return serialNum;
    }
    icLogWarn(RUNTIME_LOG, "unable to get serial number via HAL %d - %s", errno, strerror(errno));

    pthread_mutex_unlock(&RUNTIME_MTX);
    return "";
}

/*
 * return the Manufacturer Label (must not be released)
 */
const char *getSystemManufacturerLabel()
{
    // use cached version
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    if (manufacturer != NULL)
    {
        pthread_mutex_unlock(&RUNTIME_MTX);
        return manufacturer;
    }

    // ask the sysInfo HAL
    //
    static char temp[64];
    if (hal_sysinfo_get_manufacturer(temp, 63) == 0)
    {
        // create cached copy
        //
        manufacturer = strdup(temp);
        pthread_mutex_unlock(&RUNTIME_MTX);
        return manufacturer;
    }
    icLogWarn(RUNTIME_LOG, "unable to get manufacturer via HAL %d - %s", errno, strerror(errno));

    pthread_mutex_unlock(&RUNTIME_MTX);
    return "";
}

/*
 * return the Model Label (must not be released)
 */
const char *getSystemModelLabel()
{
    // use cached version
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    if (model != NULL)
    {
        pthread_mutex_unlock(&RUNTIME_MTX);
        return model;
    }

    // ask the sysInfo HAL
    //
    static char temp[64];
    if (hal_sysinfo_get_model(temp, 63) == 0)
    {
        // create cached copy
        //
        model = strdup(temp);
        pthread_mutex_unlock(&RUNTIME_MTX);
        return model;
    }
    icLogWarn(RUNTIME_LOG, "unable to get model via HAL %d - %s", errno, strerror(errno));

    pthread_mutex_unlock(&RUNTIME_MTX);
    return "";
}

/*
 * return the Hardware Version Label (MUST NOT BE RELEASED)
 */
const char *getSystemHardwareVersionLabel()
{
    // use cached version
    //
    pthread_mutex_lock(&RUNTIME_MTX);
    if (hwVersion != NULL)
    {
        pthread_mutex_unlock(&RUNTIME_MTX);
        return hwVersion;
    }

    // ask the sysInfo HAL
    //
    static char temp[64];
    if (hal_sysinfo_get_hwver(temp, 63) == 0)
    {
        // create cached copy
        //
        hwVersion = strdup(temp);
        pthread_mutex_unlock(&RUNTIME_MTX);
        return hwVersion;
    }
    icLogWarn(RUNTIME_LOG, "unable to get hardware version via HAL %d - %s", errno, strerror(errno));

    pthread_mutex_unlock(&RUNTIME_MTX);
    return "";
}

/*
 * load (if necessary) the location of our configuration dir
 */
static void getEtcConfigDir()
{
    if (etcConfigDir == NULL)
    {
        // ask propsService, then keep so we don't have to keep asking
        //
        etcConfigDir = getDynamicConfigPath();
    }
}

