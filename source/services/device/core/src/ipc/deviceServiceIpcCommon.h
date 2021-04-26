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
 * Some common IPC related encoding helper functions (events and IPC request/responses)
 * Created by Thomas Lea on 9/30/15.
 */

#ifndef ZILKER_DEVICESERVICEIPCCOMMON_H
#define ZILKER_DEVICESERVICEIPCCOMMON_H

#include <device/icDevice.h>
#include <device/icDeviceResource.h>
#include <device/icDeviceEndpoint.h>
#include "deviceService_ipc_handler.h"

bool populateDSResource(icDeviceResource *resource, const char *class, DSResource *output);
bool populateDSEndpoint(icDeviceEndpoint *endpoint, DSEndpoint *output);
bool populateDSDevice(icDevice *device, DSDevice *output);

#endif //ZILKER_DEVICESERVICEIPCCOMMON_H
