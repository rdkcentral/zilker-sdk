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
 * hardwareCapabilities.h
 *
 * Single point of reference to query the system for
 * any hardware abilities that are present.  This used
 * to reside in the Java utilsLib, but is a better fit
 * in it's own library as not all services need to
 * know which functionality is present.
 *
 * Utilizes a variety of HALs to determine the abilities,
 * but mostly this information comes from sysinfo HAL
 *
 * Note that none of these could ever change during runtime
 * as they are dependent on physical hardware being present
 *
 * Author: jelderton - 7/13/15
 *-----------------------------------------------*/

#include <icBuildtime.h>
#include <icSystem/hardwareCapabilities.h>
#include <sysinfo/sysinfoHAL.h>

/*
 * If this device supports cellular radio communication.
 */
bool supportCellularNetworks()
{
    // due to inconsistencies with some of the HAL implementations,
    // first use the build configuration that is setup per hardware device
    //
#ifdef CONFIG_CAP_CELLULAR
    return true;
#else
    // ask sysInfo HAL
    //
    if (hal_sysinfo_get_capability(SYSINFO_CELLULAR_NETWORK) == true)
    {
        return true;
    }
    return false;
#endif
}

/*
 * If this device has a WiFi radio
 */
bool supportWifiNetworks()
{
    // due to inconsistencies with some of the HAL implementations,
    // first use the build configuration that is setup per hardware device
    //
#ifdef CONFIG_CAP_NETWORK_WIFI
    return true;
#else
    // ask sysInfo HAL
    //
    if (hal_sysinfo_get_capability(SYSINFO_WIFI_NETWORK) == true)
    {
        return true;
    }
    return false;
#endif
}

/*
 * If this device has a WiFi radio that can run in AP Mode
 */
bool supportWifiAcessPointMode()
{
    // due to inconsistencies with some of the HAL implementations,
    // first use the build configuration that is setup per hardware device
    //
#ifdef CONFIG_CAP_NETWORK_WIFI_AP
    return true;
#else
    // ask sysInfo HAL
    //
    if (hal_sysinfo_get_capability(SYSINFO_WIFI_AP_NETWORK) == true)
    {
        return true;
    }
    return false;
#endif
}

/*
 * If this device has an accessible Ethernet network
 */
bool supportEthernetNetworks()
{
    // due to inconsistencies with some of the HAL implementations,
    // first use the build configuration that is setup per hardware device
    //
#ifdef CONFIG_CAP_NETWORK_ETHERNET
    return true;
#else
    // ask sysInfo HAL
    //
    if (hal_sysinfo_get_capability(SYSINFO_ETHERNET_NETWORK) == true)
    {
        return true;
    }
    return false;
#endif
}

/*
 * If this device has a Bluetooth radio installed
 */
bool supportBluetooth()
{
    // ask sysInfo HAL
    //
    if (hal_sysinfo_get_capability(SYSINFO_BLUETOOTH_RADIO) == true)
    {
        return true;
    }
    return false;
}

/*
 * If this device has a ZigBee radio installed
 */
bool supportZigBee()
{
    // due to inconsistencies with some of the HAL implementations,
    // first use the build configuration that is setup per hardware device
    //
#ifdef CONFIG_CAP_ZIGBEE
    return true;
#else
    // ask sysInfo HAL
    //
    if (hal_sysinfo_get_capability(SYSINFO_ZIGBEE_RADIO) == true)
    {
        return true;
    }
    return false;
#endif
}

/*
 * If this device has an internal speaker capable of
 * emmiting auidble tones
 */
bool supportSounds()
{
    // due to inconsistencies with some of the HAL implementations,
    // first use the build configuration that is setup per hardware device
    //
#ifdef CONFIG_CAP_AUDIO
    return true;
#else
    // ask sysInfo HAL
    //
    if (hal_sysinfo_get_capability(SYSINFO_AUDIO) == true)
    {
        return true;
    }
    return false;
#endif
}

/*
 * If this device has a display capable of presenting
 * a user interface to the user (i.e. Touchscreen)
 */
bool hasDisplayScreen()
{
    // due to inconsistencies with some of the HAL implementations,
    // first use the build configuration that is setup per hardware device
    //
#ifdef CONFIG_CAP_SCREEN
    return true;
#else
    // ask sysInfo HAL
    //
    if (hal_sysinfo_get_capability(SYSINFO_DISPLAY) == true)
    {
        return true;
    }
    return false;
#endif
}

/*
 * if this device can alter the system clock
 */
bool supportClockAlteration()
{
    return false;
}

