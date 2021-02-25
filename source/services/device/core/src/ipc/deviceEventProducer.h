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
 * deviceEventProducer.h
 *
 * Responsible for generating device events and
 * broadcasting them to the listening iControl
 * processes (services & clients)
 *
 * Author: tlea
 *-----------------------------------------------*/

#ifndef ZILKER_DEVICEEVENTPRODUCER_H
#define ZILKER_DEVICEEVENTPRODUCER_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <deviceDriver.h>

/*
 * one-time initialization
 */
void startDeviceEventProducer();

/*
 * shutdown event producer
 */
void stopDeviceEventProducer();

/*
 * broadcast a discovery started event
 *
 * @param deviceClasses - the list of device classes for which discovery has started
 * @param timeoutSeconds - the timeout in seconds until discovery ends
 */
void sendDiscoveryStartedEvent(const icLinkedList *deviceClasses, uint16_t timeoutSeconds);

/*
 * broadcast a discovery stopped event
 *
 * @param deviceClass - the device class for which discovery has stopped
 */
void sendDiscoveryStoppedEvent(const char *deviceClass);

/*
 * broadcast a device discovered event to any listeners
 *
 * @param deviceFoundDetails - info about the device that was found
 */
void sendDeviceDiscoveredEvent(const DeviceFoundDetails *deviceFoundDetails);

/*
 * broadcast a device discovery failed event to any listeners
 *
 * @param uuid - the UUID of the device that failed discovery
 * @param deviceClass - the class of device
 */
void sendDeviceDiscoveryFailedEvent(const char *uuid, const char *deviceClass);

/*
 * broadcast a device rejected event to any listeners
 *
 * @param deviceFoundDetails - info about the device that was found
 */
void sendDeviceRejectedEvent(const DeviceFoundDetails *deviceFoundDetails);

/*
 * broadcast a device discovery completed event to any listeners
 *
 * @param device - the device
 */
void sendDeviceDiscoveryCompletedEvent(icDevice *device);

/*
 * broadcast a device added event to any listeners
 *
 * @param uuid - the uuid of the device
 */
void sendDeviceAddedEvent(const char *uuid);

/*
 * broadcast a device recovered event to any listeners
 *
 * @param uuid - the uuid of the device
 */
void sendDeviceRecoveredEvent(const char *uuid);

/*
 * broadcast a device removed event to any listeners
 *
 * @param uuid - the uuid of the device
 * @param deviceClass - the class of the device
 */
void sendDeviceRemovedEvent(const char *uuid, const char *deviceClass);

/*
 * broadcast a resource updated event to any listeners
 *
 * @param resource - the resource that changed
 * @param metadata - any optional metadata about the resource updated event
 */
void sendResourceUpdatedEvent(icDeviceResource *resource, cJSON* metadata);

/*
 * broadcast an endpoint added event to any listeners
 */
void sendEndpointAddedEvent(icDeviceEndpoint *endpoint, const char *deviceClass);

/*
 * broadcast an endpoint removed event to any listeners
 */
void sendEndpointRemovedEvent(icDeviceEndpoint *endpoint, const char *deviceClass);

/*
 * broadcast a ready for devices event
 */
void sendReadyForDevicesEvent();

/*
 * broadcast a zigbee channel changed event
 */
void sendZigbeeChannelChangedEvent(bool success, uint8_t currentChannel, uint8_t targetedChannel);

/*
 * broadcast a zigbee network interference changed event
 */
void sendZigbeeNetworkInterferenceEvent(bool interferenceDetected);

/*
 * broadcast a zigbee pan id attack changed event
 */
void sendZigbeePanIdAttackEvent(bool attackDetected);

/*
 * broadcast a device configuration started event
 *
 * @param uuid - the UUID of the device that is being configured
 * @param deviceClass - the class of the device being configured
 */
void sendDeviceConfigureStartedEvent(const char *deviceClass, const char *uuid);

/*
 * broadcast a device configuration completed event
 *
 * @param uuid - the UUID of the device that was configured
 * @param deviceClass - the class of the device being configured
 */
void sendDeviceConfigureCompletedEvent(const char *deviceClass, const char *uuid);

/*
 * broadcast a device configuration failed event
 *
 * @param uuid - the UUID of the device that was configured
 * @param deviceClass - the class of the device being configured
 */
void sendDeviceConfigureFailedEvent(const char *deviceClass, const char *uuid);

#endif // ZILKER_DEVICEEVENTPRODUCER_H
