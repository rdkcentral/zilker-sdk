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
// Created by Thomas Lea on 4/11/2019
//

#ifndef ZILKER_XBBDEVICEDRIVER_H
#define ZILKER_XBBDEVICEDRIVER_H

#include <icBuildtime.h>
#include <deviceDriver.h>

// only compile if we support zigbee
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

DeviceDriver *xbbDeviceDriverInitialize(DeviceServiceCallbacks *deviceService);

#endif // CONFIG_SERVICE_DEVICE_ZIGBEE

#endif // ZILKER_XBBDEVICEDRIVER_H

