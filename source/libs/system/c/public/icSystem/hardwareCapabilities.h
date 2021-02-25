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


#ifndef IC_HARDWARECAPABILITIES_H
#define IC_HARDWARECAPABILITIES_H

#include <stdbool.h>

/*
 * If this device supports cellular radio communication.
 */
extern bool supportCellularNetworks();

/*
 * If this device has a WiFi radio
 */
extern bool supportWifiNetworks();

/*
 * If this device has a WiFi radio that can run in AP Mode
 */
extern bool supportWifiAcessPointMode();

/*
 * If this device has an accessible Ethernet network
 */
extern bool supportEthernetNetworks();

/*
 * If this device has a Bluetooth radio installed
 */
extern bool supportBluetooth();

/*
 * If this device has a ZigBee radio installed
 */
extern bool supportZigBee();

/*
 * If this device has an internal speaker capable of
 * emmiting auidble tones
 */
extern bool supportSounds();

/*
 * If this device has a display capable of presenting
 * a user interface to the user (i.e. Touchscreen)
 */
extern bool hasDisplayScreen();

/*
 * if this device can alter the system clock
 */
extern bool supportClockAlteration();

#endif // IC_HARDWARECAPABILITIES_H
