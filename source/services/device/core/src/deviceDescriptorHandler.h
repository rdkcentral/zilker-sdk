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

/*
 * The device descriptor handler is responsible for ensuring that the latest whitelist and blacklist (which provide
 * the set of device descriptors) are downloaded and available.
 *
 * Created by Thomas Lea on 10/23/15.
 */

#ifndef ZILKER_DEVICEDESCRIPTORHANDLER_H
#define ZILKER_DEVICEDESCRIPTORHANDLER_H

#include <stdbool.h>

/*
 * Callback for when we have device descriptors and are ready for devices
 */
typedef void (*deviceDescriptorsReadyForDevicesFunc)();

/*
 * Callback for when device descriptors have been updated
 */
typedef void (*deviceDescriptorsUpdatedFunc)();

/*
 * Initialize the device descriptor handler.  The readyForDevicesCallback will be invoked once
 * we have a valid set of device descriptors.  The descriptorsUpdatedCallback will be invoked whenever
 * the white or black lists change.
 */
void deviceServiceDeviceDescriptorsInit(deviceDescriptorsReadyForDevicesFunc readyForDevicesCallback,
                                        deviceDescriptorsUpdatedFunc descriptorsUpdatedCallback);

/*
 * Cleanup the handler for shutdown.
 */
void deviceServiceDeviceDescriptorsDestroy();

/*
 * Process the provided whitelist URL.  This will download it if required then invoke the readyForDevicesCallback
 * if we have a valid list.
 *
 * @param whitelistUrl - the whitelist url to download from
 */
void deviceDescriptorsUpdateWhitelist(const char *whitelistUrl);

/*
 * Process the provided blacklist URL.  This will download it if required.
 *
 * @param blacklistUrl - the blacklist url to download from
 */
void deviceDescriptorsUpdateBlacklist(const char *blacklistUrl);

#endif //ZILKER_DEVICEDESCRIPTORHANDLER_H
