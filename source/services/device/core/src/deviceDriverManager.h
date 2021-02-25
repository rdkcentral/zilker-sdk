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
 * The device driver manager handles the registration and interactions with the
 * various device drivers, each of which is responsible for understanding how to
 * interact with various device classes.
 *
 * Created by Thomas Lea on 7/28/15.
 */

#ifndef ZILKER_DEVICEDRIVERMANAGER_H
#define ZILKER_DEVICEDRIVERMANAGER_H

bool deviceDriverManagerInitialize();
bool deviceDriverManagerStartDeviceDrivers();
bool deviceDriverManagerShutdown();
DeviceDriver *deviceDriverManagerGetDeviceDriver(const char *driverName);
icLinkedList *deviceDriverManagerGetDeviceDriversByDeviceClass(const char *deviceClass);
icLinkedList *deviceDriverManagerGetDeviceDrivers();
icLinkedList *deviceDriverManagerGetDeviceDriversBySubsystem(const char *subsystem);

#endif //ZILKER_DEVICEDRIVERMANAGER_H
