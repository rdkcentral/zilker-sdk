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
 * development/sysinfoHW.c
 *
 * sysinfo HAL implementation for the XB platforms.
 * Since most of the information we need is actually
 * stored on the ARM side, we need to run a script
 * to obtain the info and cache it locally (since it
 * never changes while running).
 *
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/stat.h>

#include <sysinfo/sysinfoHAL.h>
#include <icUtil/parsePropFile.h>

// script to gather information from ARM
//
#define SCRIPT_OUTPUT   "/tmp/getInfo.out"
/*
 * NOTE: moved to setup_xb3.sh script
 *
#define SCRIPT_FILE     "/tmp/getInfo.sh"
#define ASK_ARM_SCRIPT  "#!/bin/sh\n" \
                        "if [ $# -ne 1 ]; then\n  echo \"Usage gather <outfile>\";\n  exit 1;\nfi\n\n" \
                        "hwRev=`dmcli eRT getv Device.DeviceInfo.HardwareVersion | grep value | cut -f3 -d : | tr -d ' '`\n" \
                        "manuf=`dmcli eRT getv Device.DeviceInfo.Manufacturer | grep value | cut -f3 -d :`\n" \
                        "model=`dmcli eRT getv Device.DeviceInfo.ModelName | grep value | cut -f3 -d : | tr -d ' '`\n\n" \
                        "echo \"hwRev = $hwRev\" >> $1\n" \
                        "echo \"manuf = $manuf\" >> $1\n" \
                        "echo \"model = $model\" >> $1\n"
 */

//"description=`dmcli eRT getv Device.DeviceInfo.Description | grep value | cut -f3 -d :`\n"
//"bootloader=`dmcli eRT getv Device.DeviceInfo.X_CISCO_COM_BootloaderVersion | grep value | cut -f3 -d : | tr -d ' '`\n"
//"swVersion=`dmcli eRT getv Device.DeviceInfo.SoftwareVersion | grep value | cut -f3 -d : | tr -d ' '` \n"
//"fwVersion=`dmcli eRT getv Device.DeviceInfo.X_CISCO_COM_FirmwareName | grep value | cut -f3 -d : | tr -d ' '`\n"
//"sw_fw_version=\"$swVersion\"_\"$fwVersion\"\n"

#define MAX_LINE_LEN    2048

// private variables
static pthread_mutex_t XB3_HAL_MTX = PTHREAD_MUTEX_INITIALIZER;
static bool didRead = false;
static char *localHwRevision = NULL;
static char *localManufacturer = NULL;
static char *localModel = NULL;
static char *localSerial = NULL;

/*
 * return if the line is a comment (# char) or blank
 */
static bool shouldIgnoreLine(char *line)
{
    // sanity check
    //
    if (line == NULL)
    {
        return true;
    }

    // get line length
    //
    size_t stringLen = strlen(line);
    if (stringLen < 3)
    {
        // blank
        return true;
    }

    // see if first char is '#'
    //
    if (line[0] == '#')
    {
        return true;
    }

    return false;
}

/*
 * string function to copy chars from 'dest' into 'src', until
 * we hit NULL or the 'stop' location.  will skip over leading
 * whitespace and attempt to trim the trailing whitespace.
 */
static int copyAndTrimBuffer(char *dest, char *src, char *stop)
{
    int offset = 0;
    char *ptr = src;
    bool gotChar = false;

    // loop until our ptr points to 'stop'
    // or hit 'end of string'
    //
    while (ptr != stop && ptr != NULL && *ptr != '\0')
    {
        // see if we've copied any chars yet
        // if not, then check for whitespace
        //
        if (gotChar == false && isspace(*ptr))
        {
            ptr++;
            continue;
        }

        // not space, so copy over
        //
        gotChar = true;
        dest[offset] = *ptr;
        ptr++;
        offset++;
    }

    if (offset >- 0)
    {
        // null terminate the string
        //
        dest[offset] = '\0';
    }

    // walk backward replacing any whitespace chars with NULL chars
    // to remove the trailing spaces
    //
    while (offset > 0 && isspace(dest[offset-1]))
    {
        dest[offset-1] = '\0';
        offset--;
    }

    return offset;
}

/*
 *
 */
static bool loadFromArm()
{
    bool result = true;

    // parse the output file, saving the values into memory
    //
    icPropertyIterator *loop = propIteratorCreate(SCRIPT_OUTPUT);
    while (propIteratorHasNext(loop) == true)
    {
        icProperty *prop = propIteratorGetNext(loop);

        // look for one of our 3 output values we care about
        //
        if (strcmp(prop->key, "hwRev") == 0 && prop->value != NULL)
        {
            localHwRevision = strdup(prop->value);
        }
        else if (strcmp(prop->key, "manuf") == 0 && prop->value != NULL)
        {
            localManufacturer = strdup(prop->value);
        }
        else if (strcmp(prop->key, "model") == 0 && prop->value != NULL)
        {
            localModel = strdup(prop->value);
        }
        else if (strcmp(prop->key, "serial") == 0 && prop->value != NULL)
        {
            localSerial = strdup(prop->value);
        }
    }
    propIteratorDestroy(loop);

    // if we dont successfully read all of the required values, return failure
    //
    if(localHwRevision == NULL || localManufacturer == NULL || localModel == NULL || localSerial == NULL)
    {
        result = false;
    }

    return result;
}

static bool initXb3()
{
    // load the 'info' file if necessary
    //
    pthread_mutex_lock(&XB3_HAL_MTX);
    if (didRead == false)
    {
        didRead = loadFromArm();
    }
    pthread_mutex_unlock(&XB3_HAL_MTX);

    return didRead;
}

/**
* @brief returns the hardware version which is incremented each time the hardware changes in a way that affects the software
* @param: char *hwVersion: the hardware version string.
* @param: int  hwVersionLen: the length of the buffer for the hardware version string.
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_get_hwver(char *hwVersion, int length)
{
    // make sure we loaded our data
    //
    if (initXb3() == false || hwVersion == NULL || localHwRevision == NULL)
    {
        return 1;
    }

    // transfer localHwRevision it to 'hwVersion', up to the
    // min amount of allowable characters
    //
    int len = (int)strlen(localHwRevision);
    if (length < len)
    {
        len = length;
    }
    strncpy((char *)hwVersion, localHwRevision, len);
    hwVersion[len] = '\0';

    return 0;
}

/**
* @brief returns the device serial number as a NULL-terminated string
* @param: char *serialNumber: the serial number string
* @param: uint32_t length: the length of the buffer for the serial number. do not exceed this.
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_get_serialnum(char *serialNumber, uint8_t length)
{
    // make sure we loaded our data
    //
    if (initXb3() == false || serialNumber == NULL || localSerial == NULL)
    {
        return 1;
    }

    // transfer localSerial to 'serialNumber', up to the
    // min amount of allowable characters
    //
    int len = (int)strlen(localSerial);
    if (length < len)
    {
        len = length;
    }
    strncpy((char *)serialNumber, localSerial, len);
    serialNumber[len] = '\0';

    return 0;
}

/**
* @brief returns the device LAN MAC address into an unsigned byte array
* @param: uint8_t macAddress[]: ptr to the MAC address byte array
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_get_macaddr(uint8_t macAddress[6])
{
    // use the contents of our macAddress file which is generated at startup
    //
    char buffer[32];
    FILE *input = fopen("/nvram/icontrol/etc/macAddress", "r");
    if (input == NULL)
    {
        // unable to open the file
        //
        return -1;
    }

    if (fgets(buffer, 32, input) == NULL)
    {
        // unable to read the file
        //
        fclose(input);
        return -1;
    }
    fclose(input);

    // take 2 bytes from the 'buffer' and assign
    // as a single integer in our output
    //
    char *ptr = buffer;
    int i = 0;
    for (i = 0 ; i < 6 ; i++)
    {
        macAddress[i] = (uint8_t)strtoul(ptr, 0, 16);
        ptr += 3;
    }

    return 0;
}

/**
* @brief returns capability value (true or false)
* @param: hal_sysinfo_capability_t capability - the capability to check support for
* @return: 0 : the capability is false (not supported)
*          1 : the capability is true (supported & enabled)
*/
bool hal_sysinfo_get_capability(hal_sysinfo_capability_t capability)
{
//    printf("xb3 sysinfo hal not complete, returning fake data for capability\n");

    if (capability == SYSINFO_ETHERNET_NETWORK ||
        capability == SYSINFO_WIFI_NETWORK ||
        capability == SYSINFO_WIFI_AP_NETWORK)
    {
        // supported
        //
        return true;
    }

    // not applicable
    //
    return false;
}

/**
* @brief enables the capability in the device
* @warning: If device uses eFuses, this call may not be reversible.
*           Understand the function before calling this.
* @param: hal_sysinfo_capability_t capability - the capability to enable
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_set_capability(hal_sysinfo_capability_t capability)
{
    // not applicable
    return -1;
}

/**
 * @brief get the cpe manufacturer
 * @param: char *manufacturer: the manufacturer string
 * @param: uint8_t length: the length of the buffer for the manufacturer. do not exceed this
 * @return: 0: to indicate success
 *          Error Code : Positive number, use errno.h
 */
int hal_sysinfo_get_manufacturer(char *manufacturer, uint8_t length)
{
    // make sure we loaded our data
    //
    if (initXb3() == false || manufacturer == NULL || localManufacturer == NULL)
    {
        return 1;
    }

    // transfer localManufacturer it to 'manufacturer', up to the
    // min amount of allowable characters
    //
    int len = (int)strlen(localManufacturer);
    if (length < len)
    {
        len = length;
    }
    strncpy((char *)manufacturer, localManufacturer, len);
    manufacturer[len] = '\0';

    return 0;
}

/**
* @brief get the cpe model
* @param: char *model: the model string
* @param uint8_t length: the length of the buffer for the model. do not exceed this
* @reaturn: 0 : to indicate success
*           Error Code: Positive number, use errno.h
*/
int hal_sysinfo_get_model(char *model, uint8_t length)
{
    // make sure we loaded our data
    //
    if (initXb3() == false || model == NULL || localModel == NULL)
    {
        return 1;
    }

    // transfer localModel to 'model', up to the
    // min amount of allowable characters
    //
    int len = (int)strlen(localModel);
    if (length < len)
    {
        len = length;
    }
    strncpy((char *)model, localModel, len);
    model[len] = '\0';

    return 0;
}

/**
 * @brief Get the HAL version number
 * @param none
 * @return the version number as an int
 *         The version is a simple versioning (1, 2, 3, ...)
 */
int hal_sysinfo_get_version()
{
    return 3;   // this correct?
}

/**
 * @brief Get removable devices info in array (double pointer)
 * @param hal_sysinfo_rem_dev_info Contains paths of all removable devices
 *            Important : The memory for this will be allocated by this library but has to be freed by caller using free()
 *
 * @return int count of number of removable devices i.e. the size of array hal_sysinfo_rem_dev_info
 */
int hal_sysinfo_get_rem_dev_info(hal_sysinfo_rem_dev_info** remDevInfo)
{
    return 0;
}

