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
 * Created by Thomas Lea on 9/27/19.
 */
#include <deviceService/deviceService_event.h>
#include <deviceService/deviceService_eventAdapter.h>
#include "eventHandler.h"
#include "util.h"
#include <inttypes.h>

static void deviceDiscoveredEventHandler(DeviceServiceDeviceDiscoveredEvent *event);

static void deviceRejectedEventHandler(DeviceServiceDeviceRejectedEvent *event);

static void deviceAddedEventHandler(DeviceServiceDeviceAddedEvent *event);

static void deviceRemovedEventHandler(DeviceServiceDeviceRemovedEvent *event);

static void deviceRecoveredEventHandler(DeviceServiceDeviceRecoveredEvent *event);

static void resourceUpdatedEventHandler(DeviceServiceResourceUpdatedEvent *event);

static void deviceDiscoveryStartedEventHandler(DeviceServiceDiscoveryStartedEvent *event);

static void deviceDiscoveryStoppedEventHandler(DeviceServiceDiscoveryStoppedEvent *event);

static void endpointAddedEventHandler(DeviceServiceEndpointAddedEvent *event);

static void endpointRemovedEventHandler(DeviceServiceEndpointRemovedEvent *event);

static void deviceDiscoveryCompletedCallback(DeviceServiceDeviceDiscoveryCompletedEvent *event);

static void deviceDiscoveryFailedCallback(DeviceServiceDeviceDiscoveryFailedEvent *event);

static void zigbeeChannelChangedCallback(DeviceServiceZigbeeChannelChangedEvent *event);

void registerEventHandlers()
{
    register_DeviceServiceDeviceDiscoveredEvent_eventListener(deviceDiscoveredEventHandler);
    register_DeviceServiceDeviceRejectedEvent_eventListener(deviceRejectedEventHandler);
    register_DeviceServiceDeviceAddedEvent_eventListener(deviceAddedEventHandler);
    register_DeviceServiceDeviceRemovedEvent_eventListener(deviceRemovedEventHandler);
    register_DeviceServiceResourceUpdatedEvent_eventListener(resourceUpdatedEventHandler);
    register_DeviceServiceDiscoveryStartedEvent_eventListener(deviceDiscoveryStartedEventHandler);
    register_DeviceServiceDiscoveryStoppedEvent_eventListener(deviceDiscoveryStoppedEventHandler);
    register_DeviceServiceEndpointAddedEvent_eventListener(endpointAddedEventHandler);
    register_DeviceServiceEndpointRemovedEvent_eventListener(endpointRemovedEventHandler);
    register_DeviceServiceDeviceDiscoveryCompletedEvent_eventListener(deviceDiscoveryCompletedCallback);
    register_DeviceServiceDeviceDiscoveryFailedEvent_eventListener(deviceDiscoveryFailedCallback);
    register_DeviceServiceZigbeeChannelChangedEvent_eventListener(zigbeeChannelChangedCallback);
    register_DeviceServiceDeviceRecoveredEvent_eventListener(deviceRecoveredEventHandler);
}

void unregisterEventHandlers()
{
    unregister_DeviceServiceDeviceDiscoveredEvent_eventListener(deviceDiscoveredEventHandler);
    unregister_DeviceServiceDeviceRejectedEvent_eventListener(deviceRejectedEventHandler);
    unregister_DeviceServiceDeviceAddedEvent_eventListener(deviceAddedEventHandler);
    unregister_DeviceServiceDeviceRemovedEvent_eventListener(deviceRemovedEventHandler);
    unregister_DeviceServiceResourceUpdatedEvent_eventListener(resourceUpdatedEventHandler);
    unregister_DeviceServiceDiscoveryStartedEvent_eventListener(deviceDiscoveryStartedEventHandler);
    unregister_DeviceServiceDiscoveryStoppedEvent_eventListener(deviceDiscoveryStoppedEventHandler);
    unregister_DeviceServiceEndpointAddedEvent_eventListener(endpointAddedEventHandler);
    unregister_DeviceServiceEndpointRemovedEvent_eventListener(endpointRemovedEventHandler);
    unregister_DeviceServiceZigbeeChannelChangedEvent_eventListener(zigbeeChannelChangedCallback);
    unregister_DeviceServiceDeviceRecoveredEvent_eventListener(deviceRecoveredEventHandler);
}

static void deviceDiscoveredEventHandler(DeviceServiceDeviceDiscoveredEvent *event)
{
    printf("\r\ndevice discovered! uuid=%s, manufacturer=%s, model=%s, hardwareVersion=%s, firmwareVersion=%s\n",
           event->details->id,
           event->details->manufacturer,
           event->details->model,
           event->details->hardwareVersion,
           event->details->firmwareVersion);
}

static void deviceRejectedEventHandler(DeviceServiceDeviceRejectedEvent *event)
{
    printf("\r\ndevice rejected! deviceId=%s, manufacturer=%s, model=%s, hardwareVersion=%s, firmwareVersion=%s\n",
           event->details->id,
           event->details->manufacturer,
           event->details->model,
           event->details->hardwareVersion,
           event->details->firmwareVersion);
}

static void deviceAddedEventHandler(DeviceServiceDeviceAddedEvent *event)
{
    printf("\r\ndevice added! deviceId=%s, uri=%s, deviceClass=%s, deviceClassVersion=%d\n",
           event->details->deviceId,
           event->details->uri,
           event->details->deviceClass,
           event->details->deviceClassVersion);
}

static void deviceRemovedEventHandler(DeviceServiceDeviceRemovedEvent *event)
{
    printf("\r\ndevice removed! deviceId=%s, deviceClass=%s\n", event->deviceId, event->deviceClass);
}

static void deviceRecoveredEventHandler(DeviceServiceDeviceRecoveredEvent *event)
{
    printf("\r\ndevice recovered! deviceId=%s, deviceClass=%s\n",event->deviceId, event->deviceClass);
}

static void resourceUpdatedEventHandler(DeviceServiceResourceUpdatedEvent *event)
{
    char *resourceDump = getResourceDump(event->resource);
    if (event->details != NULL)
    {
        char *details = cJSON_PrintUnformatted(event->details);
        printf("\r\nresourceUpdated: %s (details=%s)\n", resourceDump, details);
        free(details);
    }
    else
    {
        printf("\r\nresourceUpdated: %s\n", resourceDump);
    }

    free(resourceDump);
}

static void deviceDiscoveryStartedEventHandler(DeviceServiceDiscoveryStartedEvent *event)
{
    printf("\r\ndiscoveryStarted\n");
}

static void deviceDiscoveryStoppedEventHandler(DeviceServiceDiscoveryStoppedEvent *event)
{
    printf("\r\ndiscoveryStopped\n");
}

static void endpointAddedEventHandler(DeviceServiceEndpointAddedEvent *event)
{
    printf("\r\nendpointAdded: deviceUuid=%s, id=%s, uri=%s, profile=%s, profileVersion=%d\n",
           event->details->deviceUuid,
           event->details->id,
           event->details->uri,
           event->details->profile,
           event->details->profileVersion);
}

static void endpointRemovedEventHandler(DeviceServiceEndpointRemovedEvent *event)
{
    printf("\r\nendpointRemoved: endpointId=%s, profile=%s\n",
           event->endpoint->id,
           event->endpoint->profile);
}

static void deviceDiscoveryCompletedCallback(DeviceServiceDeviceDiscoveryCompletedEvent *event)
{
    printf("\r\ndeviceDiscoveryCompleted: uuid=%s, class=%s\n",
           event->device->id,
           event->device->deviceClass);
}

static void deviceDiscoveryFailedCallback(DeviceServiceDeviceDiscoveryFailedEvent *event)
{
    printf("\r\ndeviceDiscoveryFailed: uuid=%s\n",
           event->deviceId);
}

static void zigbeeChannelChangedCallback(DeviceServiceZigbeeChannelChangedEvent *event)
{
    printf("\r\nzigbeeChannelChanged: currentChannel=%"
    PRIu16
    ", targetedChannel=%"
    PRIu16
    ", success=%s\n",
            event->currentChannel, event->targetedChannel, event->success ? "true" : "false");
}
