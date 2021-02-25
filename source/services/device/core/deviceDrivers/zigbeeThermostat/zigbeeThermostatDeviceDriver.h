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
// Created by Thomas Lea on 3/1/19
//

#ifndef ZILKER_ZIGBEETHERMOSTATDEVICEDRIVER_H
#define ZILKER_ZIGBEETHERMOSTATDEVICEDRIVER_H

#include <icBuildtime.h>
#include <deviceDriver.h>

// only compile if we support zigbee
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE

DeviceDriver *zigbeeThermostatDeviceDriverInitialize(DeviceServiceCallbacks *deviceService);

#endif // CONFIG_SERVICE_DEVICE_ZIGBEE

#endif // ZILKER_ZIGBEETHERMOSTATDEVICEDRIVER_H

