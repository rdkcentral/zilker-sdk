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
 * Created by Thomas Lea on 3/10/16.
 */

#ifndef ZILKER_SUBSYSTEMMANAGER_H
#define ZILKER_SUBSYSTEMMANAGER_H

#include <stdint.h>
#include <stdbool.h>

#define ZIGBEE_SUBSYSTEM_ID "zigbee"

/*
 * Callback for when a subsystem has been initialized.
 */
typedef void (*subsystemManagerInitializedFunc)(const char *subsystem);

/*
 * Callback for when all subsystems are ready for devices
 */
typedef void (*subsystemManagerReadyForDevicesFunc)();

/*
 * Initialize the subsystem manager.
 */
int subsystemManagerInitialize(const char *cpeId, subsystemManagerInitializedFunc initializedCallback, subsystemManagerReadyForDevicesFunc readyForDevicesCallback);

/*
 * Shutdown the subsystem manager
 */
void subsystemManagerShutdown();

/*
 * Inform the subsystem manager that all device drivers have loaded.
 */
void subsystemManagerAllDriversStarted();

/*
 * Inform the subsystem manager that all services are available.
 */
void subsystemManagerAllServicesAvailable();

/*
 * Check if OCF is enabled.
 */
bool subsystemManagerIsOcfEnabled();

/*
 * Check if a specific subsystem is ready for devices
 */
bool subsystemManagerIsSubsystemReady(const char *subsystem);

/*
 * Check if all subsystems are ready for devices
 */
bool subsystemManagerIsReadyForDevices();

/*
 * Restore config for RMA
 */
bool subsystemManagerRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath);

/*
 * Post restore config actions for RMA
 */
void subsystemManagerPostRestoreConfig();

#endif //ZILKER_SUBSYSTEMMANAGER_H
