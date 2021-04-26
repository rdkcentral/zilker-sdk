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

/****************************************************************************
*
* @file    sysinfoHAL.h
* @brief   Header file for the System Information (sysinfo) HAL
*         All hardware implementations should support the API defined here.
*         In addition to implementing this API, the vendor should provide a *.so file
*
****************************************************************************/

#ifndef IC_SYSINFO_HAL_H
#define IC_SYSINFO_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

/**
 * @enum hal_sysinfo_capability_t
 *
 * @brief The capability to check support for.
 * @brief Currently, only one capability (DEVICE_MEM_LOCKDOWN) is potentially used for the 'set_capability'
 */
typedef enum {
    SYSINFO_DEVICE_MEM_LOCKDOWN  = 0,   // the system is configured to only boot from internal memory (NAND, NOR, etc.)
    SYSINFO_AUDIO,                      // the platform supports audio (i.e. has a speaker)
    SYSINFO_SIREN,                      // the platform supports an internal siren for alarming
    SYSINFO_DISPLAY,                    // the platform supports a visual display (i.e. touchscreen)
    SYSINFO_BATTERY,                    // the platform supports an internal battery
    SYSINFO_CELLULAR_NETWORK,           // the platform supports cellular networks via internal radio
    SYSINFO_WIFI_NETWORK,               // the platform supports WiFi networks via internal radio
    SYSINFO_WIFI_AP_NETWORK,            // the platform supports WiFi as an Access Point
    SYSINFO_ETHERNET_NETWORK,           // the platform supports ethernet networks
    SYSINFO_BLUETOOTH_RADIO,            // integrated bluetooth radio
    SYSINFO_ZIGBEE_RADIO,               // integrated ZigBee radio
    SYSINFO_ZWAVE_RADIO,                // integrated Z-Wave radio
} hal_sysinfo_capability_t;

/**
 * @enum hal_sysinfo_rem_dev_t
 * @brief Type of removable device.
 * @brief Currently, There are two devices SD card and USB.
 */
typedef enum {
    SYSINFO_REM_DEV_SD = 0,
    SYSINFO_REM_DEV_USB = 1,
} hal_sysinfo_rem_dev_t;

/**
 * @struct hal_sysinfo_rem_dev_info
 * @brief This structure contains path of mount point and it's type.
 */
typedef struct {
    char* mountpoint;
    hal_sysinfo_rem_dev_t removable_device_type;
}hal_sysinfo_rem_dev_info;

/*** PUBLIC FUNCTIONS */

/**
* @brief returns the hardware version which is incremented each time the hardware changes in a way that affects the software
* @param: char *hwVersion: the hardware version string.
* @param: int  hwVersionLen: the length of the buffer for the hardware version string.
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_get_hwver(char *hwVersion, int hwVersionLen);

/**
* @brief returns the device serial number as a NULL-terminated string
* @param: char *serialNumber: the serial number string
* @param: uint32_t length: the length of the buffer for the serial number. do not exceed this.
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_get_serialnum(char *serialNumber, uint8_t length);

/**
* @brief returns the device LAN MAC address into an unsigned byte array
* @param: uint8_t macAddress[]: ptr to the MAC address byte array
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_get_macaddr(uint8_t macAddress[6]);

/**
* @brief returns capability value (true or false)
* @param: hal_sysinfo_capability_t capability - the capability to check support for
* @return: 0 : the capability is false (not supported)
*          1 : the capability is true (supported & enabled)
*/
bool hal_sysinfo_get_capability(hal_sysinfo_capability_t capability);

/**
* @brief enables the capability in the device
* @warning: If device uses eFuses, this call may not be reversible.
*           Understand the function before calling this.  
* @param: hal_sysinfo_capability_t capability - the capability to enable
* @return: 0 : to indicate success
*          Error Code : Positive number, use errno.h
*/
int hal_sysinfo_set_capability(hal_sysinfo_capability_t capability);

/**
 * @brief get the cpe manufacturer
 * @param: char *manufacturer: the manufacturer string
 * @param: uint8_t length: the length of the buffer for the manufacturer. do not exceed this
 * @return: 0: to indicate success
 *          Error Code : Positive number, use errno.h
 */
int hal_sysinfo_get_manufacturer(char *manufacturer, uint8_t length);

/**
* @brief get the cpe model
* @param: char *model: the model string
* @param uint8_t length: the length of the buffer for the model. do not exceed this
* @reaturn: 0 : to indicate success
*           Error Code: Positive number, use errno.h
*/
int hal_sysinfo_get_model(char *model, uint8_t length);

/**
 * @brief Get the HAL version number
 * @param none
 * @return the version number as an int
 *         The version is a simple versioning (1, 2, 3, ...)
 */
extern int hal_sysinfo_get_version();

/**
 * @brief Get removable devices info in array (double pointer)
 * @param hal_sysinfo_rem_dev_info Contains paths of all removable devices
 *            Important : The memory for this will be allocated by this library but has to be freed by caller using free()
 *
 * @return int count of number of removable devices i.e. the size of array hal_sysinfo_rem_dev_info
 */
int hal_sysinfo_get_rem_dev_info(hal_sysinfo_rem_dev_info** removable_device_type);

#endif /*IC_SYSINFO_HAL_H*/

