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
 * Basic implementation of the libsysinfoHAL that can
 * be used during development execution/testing.  Works
 * on both Linux and Mac OS X
 * THIS SHOULD NOT BE USED ON REAL PLATFORMS
 *
 * Author: jelderton - 7/14/15
 *-----------------------------------------------*/

#include <string.h>
#include <sysinfo/sysinfoHAL.h>


/**
* @brief returns the hardware version which is incremented each time the hardware changes in a way that affects the software
* @param: char *hwVersion: the hardware version string.
* @param: int  hwVersionLen: the length of the buffer for the hardware version string.
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_get_hwver(char *hwVersion, int hwVersionLen)
{
    int len = 4;
    if (len < hwVersionLen)
    {
        len = hwVersionLen;
    }

    // copy up-to len then NULL terminate
    //
    strncpy((char *)hwVersion, "1234", len);
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
    int len = 5;
    if (len < length)
    {
        len = length;
    }

    // copy up-to len then NULL terminate
    //
    strncpy(serialNumber, "56789", len);
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
    // hard-code the mac to "00:03:7f:ff:ff:ff"
    //
    macAddress[0] = 0x00;
    macAddress[1] = 0x03;
    macAddress[2] = 0x7f;
    macAddress[3] = 0xff;
    macAddress[4] = 0xff;
    macAddress[5] = 0xff;

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
    if (capability == SYSINFO_ETHERNET_NETWORK ||
        capability == SYSINFO_CELLULAR_NETWORK ||
        capability == SYSINFO_BATTERY)
    {
        // dev, so only assume ethernet & pretend to have cell
        //
        return true;
    }

    // development env, so not applicable
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
    // not supported
    return -1;
}

/**
 * Note: Lie that we are a TCA203 for proper Mobile interaction
 *
 * @brief get the cpe manufacturer
 * @param: char *manufacturer: the manufacturer string
 * @param: uint8_t length: the length of the buffer for the manufacturer. do not exceed this
 * @return: 0: to indicate success
 *          Error Code : Positive number, use errno.h
 */
int hal_sysinfo_get_manufacturer(char *manufacturer, uint8_t length)
{
    size_t len = strlen("Flextronics");
    if (len < length)
    {
        len = length;
    }

    // copy up-to len then NULL terminate
    //
    strncpy(manufacturer, "Flextronics", len);
    manufacturer[len] = '\0';

    return 0;
}

/**
 * Note: Lie that we are a TCA203 for proper Mobile interaction
 * @brief get the cpe model
 * @param: char *model: the model string
 * @param uint8_t length: the length of the buffer for the model. do not exceed this
 * @reaturn: 0 : to indicate success
 *           Error Code: Positive number, use errno.h
 */
int hal_sysinfo_get_model(char *model, uint8_t length)
{
    size_t len = strlen("fcl5320");
    if (len < length)
    {
        len = length;
    }

    // copy up-to len then NULL terminate
    //
    strncpy(model, "fcl5320", len);
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

