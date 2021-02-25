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
// Created by Thomas Lea on 7/28/15.
//

#ifndef ZILKER_OPENHOMECAMERADEVICEDRIVER_H
#define ZILKER_OPENHOMECAMERADEVICEDRIVER_H

#include "deviceDriver.h"
#include "cameraDevice.h"

/*
 * init the driver. called by the device service.
 */
DeviceDriver *openHomeCameraDeviceDriverInitialize(DeviceServiceCallbacks *deviceService);

bool openHomeCameraDeviceDriverAddMigratedCamera(cameraDevice *cameraDevice, DeviceMigrator *migrator);

cameraDevice *
openHomeCameraDeviceDriverCreateCameraDevice(const char *macAddress, const char *ipAddress, const char *adminUserId,
                                             const char *adminPassword, bool fetchDetails);

#endif //ZILKER_OPENHOMECAMERADEVICEDRIVER_H
