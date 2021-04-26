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

//
// Created by tlea on 2/4/19.
//

#ifndef ZILKER_DEVICENUMBERALLOCATOR_H
#define ZILKER_DEVICENUMBERALLOCATOR_H

#include <stdint.h>
#include <device/icDevice.h>

/**
 * Gets the next available legacy security device number and persists it on the device.
 *
 * @return the next available device number or 0 on failure
 */
uint8_t allocateDeviceNumber(icDevice *device);

/**
 * Gets the device number used by the specified device from persistent storage, or 0 if none found.
 *
 * @return the device number used by the device or 0 if none
 */
uint8_t getDeviceNumberForDevice(const char *uuid);

/**
 * Retrieve the EUI64 of the device possessing the provided device number.
 *
 * @return true on success
 */
bool getEui64ForDeviceNumber(uint8_t devNum, uint64_t *eui64);

/**
 * Clear any temporary device numbers used during pairing.
 */
void clearTemporaryDeviceNumbers();

/**
 * Set the device number for a device.  This should only be used for migrations.
 * For other cases the allocate will store it on the device for you
 * @param device the device
 * @param deviceNumber the device number
 * @return true if successful, false otherwise
 */
bool setDeviceNumberForDevice(icDevice *device, uint8_t deviceNumber);

#endif //ZILKER_DEVICENUMBERALLOCATOR_H
