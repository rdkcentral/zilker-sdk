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
 * deviceEventProducer.c
 *
 * Responsible for generating device events and
 * broadcasting them to the listening iControl
 * processes (services & clients)
 *
 * Author: tlea
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <icLog/logging.h>                    // logging subsystem
#include <icIpc/eventProducer.h>
#include <deviceService/deviceService_event.h>
#include <deviceService.h>
#include <deviceDriver.h>
#include <device/icDeviceResource.h>
#include <device/icDeviceEndpoint.h>
#include "deviceEventProducer.h"                      // local header
#include "deviceServicePrivate.h"                      // local header
#include "deviceServiceIpcCommon.h"

#define LOG_TAG "deviceServiceEventProducer"

/*
 * local variables
 */
static EventProducer producer = NULL;
static pthread_mutex_t EVENT_MTX = PTHREAD_MUTEX_INITIALIZER;     // mutex for the event producer

/*
 * one-time initialization
 */
void startDeviceEventProducer()
{
    // call the EventProducer (from libicIpc) to
    // initialize our producer pointer
    //
    pthread_mutex_lock(&EVENT_MTX);
    if (producer == NULL)
    {
        icLogDebug(LOG_TAG, "starting event producer on port %d", DEVICESERVICE_EVENT_PORT_NUM);
        producer = initEventProducer(DEVICESERVICE_EVENT_PORT_NUM);
    }
    pthread_mutex_unlock(&EVENT_MTX);
}

/*
 * shutdown event producer
 */
void stopDeviceEventProducer()
{
    pthread_mutex_lock(&EVENT_MTX);

    if (producer != NULL)
    {
        shutdownEventProducer(producer);
        producer = NULL;
    }

    pthread_mutex_unlock(&EVENT_MTX);
}

void sendDiscoveryStartedEvent(const icLinkedList *deviceClasses, uint16_t timeoutSeconds)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting discovery started event");

    DeviceServiceDiscoveryStartedEvent *event = create_DeviceServiceDiscoveryStartedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_DISCOVERY_STARTED;
    setEventId(&(event->baseEvent));
    setEventTimeToNow(&(event->baseEvent));
    linkedListDestroy(event->deviceClasses, NULL);
    event->deviceClasses = linkedListDeepClone((icLinkedList*)deviceClasses, linkedListCloneStringItemFunc, NULL);
    event->discoveryTimeoutSeconds = timeoutSeconds;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceDiscoveryStartedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    destroy_DeviceServiceDiscoveryStartedEvent(event);
    cJSON_Delete(jsonNode);
}

void sendDiscoveryStoppedEvent(const char *deviceClass)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting discovery stopped event");

    DeviceServiceDiscoveryStoppedEvent *event = create_DeviceServiceDiscoveryStoppedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_DISCOVERY_STOPPED;
    setEventId(&(event->baseEvent));
    setEventTimeToNow(&(event->baseEvent));
    event->deviceClass = strdup(deviceClass);

    // convert to JSON objet
    //
    cJSON *jsonNode = encode_DeviceServiceDiscoveryStoppedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    destroy_DeviceServiceDiscoveryStoppedEvent(event);
    cJSON_Delete(jsonNode);
}

static void sendEarlyDeviceEvent(int eventCode, const DeviceFoundDetails* deviceFoundDetails)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting early device event, code=%d, uuid=%s, manufacturer=%s, model=%s, hardwareVersion=%s, firmwareVersion=%s", eventCode,
               deviceFoundDetails->deviceUuid, deviceFoundDetails->manufacturer, deviceFoundDetails->model,
               deviceFoundDetails->hardwareVersion, deviceFoundDetails->firmwareVersion);

    if(deviceFoundDetails->deviceUuid == NULL ||
            deviceFoundDetails->manufacturer == NULL ||
            deviceFoundDetails->model == NULL ||
            deviceFoundDetails->hardwareVersion == NULL ||
            deviceFoundDetails->firmwareVersion == NULL)
    {
        icLogError(LOG_TAG, "Missing required data from discovered device.");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    DSEarlyDeviceDiscoveryDetails *eventDetails = create_DSEarlyDeviceDiscoveryDetails();

    //fill in our common structure
    eventDetails->id = strdup(deviceFoundDetails->deviceUuid);
    eventDetails->manufacturer = strdup(deviceFoundDetails->manufacturer);
    eventDetails->model = strdup(deviceFoundDetails->model);
    eventDetails->hardwareVersion = strdup(deviceFoundDetails->hardwareVersion);
    eventDetails->firmwareVersion = strdup(deviceFoundDetails->firmwareVersion);
    eventDetails->deviceClass = strdup(deviceFoundDetails->deviceClass);
    if (deviceFoundDetails->metadata != NULL)
    {
        icStringHashMapIterator *iter = stringHashMapIteratorCreate(deviceFoundDetails->metadata);
        while (stringHashMapIteratorHasNext(iter))
        {
            char *key;
            char *value;
            stringHashMapIteratorGetNext(iter, &key, &value);
            put_metadataValue_in_DSEarlyDeviceDiscoveryDetails_metadata(eventDetails, key, value);
        }
        stringHashMapIteratorDestroy(iter);
    }
    if (deviceFoundDetails->endpointProfileMap != NULL)
    {
        icStringHashMapIterator *iter = stringHashMapIteratorCreate(deviceFoundDetails->endpointProfileMap);
        while (stringHashMapIteratorHasNext(iter))
        {
            char *key;
            char *value;
            stringHashMapIteratorGetNext(iter, &key, &value);
            put_endpointProfile_in_DSEarlyDeviceDiscoveryDetails_endpointProfileMap(eventDetails, key, value);
        }
        stringHashMapIteratorDestroy(iter);
    }

    cJSON *jsonNode = NULL;

    switch(eventCode)
    {
        case DEVICE_SERVICE_EVENT_DEVICE_DISCOVERED:
        {
            DeviceServiceDeviceDiscoveredEvent *event = create_DeviceServiceDeviceDiscoveredEvent();
            event->baseEvent.eventCode = eventCode;
            setEventId(&(event->baseEvent));
            setEventTimeToNow(&(event->baseEvent));
            destroy_DSEarlyDeviceDiscoveryDetails(event->details);
            event->details = eventDetails;
            jsonNode = encode_DeviceServiceDeviceDiscoveredEvent_toJSON(event);
            destroy_DeviceServiceDeviceDiscoveredEvent(event);
            break;
        }

        case DEVICE_SERVICE_EVENT_DEVICE_REJECTED:
        {
            DeviceServiceDeviceRejectedEvent *event = create_DeviceServiceDeviceRejectedEvent();
            event->baseEvent.eventCode = eventCode;
            setEventId(&(event->baseEvent));
            setEventTimeToNow(&(event->baseEvent));
            destroy_DSEarlyDeviceDiscoveryDetails(event->details);
            event->details = eventDetails;
            jsonNode = encode_DeviceServiceDeviceRejectedEvent_toJSON(event);
            destroy_DeviceServiceDeviceRejectedEvent(event);
            break;
        }

        default:
        {
            icLogWarn(LOG_TAG, "unable to broadcast event, invalid early device event code");
            destroy_DSEarlyDeviceDiscoveryDetails(eventDetails);
            pthread_mutex_unlock(&EVENT_MTX);
            return;
        }
    }

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
}

/*
 * broadcast a device discovered event to any listeners
 *
 * @param deviceFoundDetails the information about the device that was found
 */
void sendDeviceDiscoveredEvent(const DeviceFoundDetails *deviceFoundDetails)
{
    sendEarlyDeviceEvent(DEVICE_SERVICE_EVENT_DEVICE_DISCOVERED, deviceFoundDetails);
}

void sendDeviceDiscoveryFailedEvent(const char *uuid, const char *deviceClass)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting device discovery failed event, uuid=%s", uuid);

    DeviceServiceDeviceDiscoveryFailedEvent *event = create_DeviceServiceDeviceDiscoveryFailedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_DEVICE_DISCOVERY_FAILED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the device specific information
    //
    event->deviceId = strdup(uuid);
    event->deviceClass = strdup(deviceClass);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceDeviceDiscoveryFailedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceDeviceDiscoveryFailedEvent(event);
}

/*
 * broadcast a device rejected event to any listeners
 *
 * @param deviceFoundDetails the information about the device that was found
 */
void sendDeviceRejectedEvent(const DeviceFoundDetails *deviceFoundDetails)
{
    sendEarlyDeviceEvent(DEVICE_SERVICE_EVENT_DEVICE_REJECTED, deviceFoundDetails);
}

void sendDeviceDiscoveryCompletedEvent(icDevice *device)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting device discovery completed event, uuid=%s", device->uuid);

    DeviceServiceDeviceDiscoveryCompletedEvent *event = create_DeviceServiceDeviceDiscoveryCompletedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_DEVICE_DISCOVERY_COMPLETED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // populate into the event
    //
    populateDSDevice(device, event->device);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceDeviceDiscoveryCompletedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceDeviceDiscoveryCompletedEvent(event);
}

/*
 * broadcast a device added event to any listeners
 *
 * @param uuid - the uuid of the device
 */
void sendDeviceAddedEvent(const char *uuid)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting device added event, uuid=%s", uuid);

    DeviceServiceDeviceAddedEvent *event = create_DeviceServiceDeviceAddedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_DEVICE_ADDED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the device specific information
    //
    icDevice *device = deviceServiceGetDevice(uuid);
    if(device == NULL)
    {
        icLogError(LOG_TAG, "%s: no device found for uuid %s!", __func__,uuid);
        destroy_DeviceServiceDeviceAddedEvent(event);
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    event->details->deviceId = strdup(device->uuid);
    event->details->uri = strdup(device->uri);
    event->details->deviceClass = strdup(device->deviceClass);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceDeviceAddedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    deviceDestroy(device);
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceDeviceAddedEvent(event);
}

/*
 * broadcast a device recovered event to any listeners
 *
 * @param uuid - the uuid of the device
 */
void sendDeviceRecoveredEvent(const char *uuid)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting device recovered event, uuid=%s", uuid);

    DeviceServiceDeviceRecoveredEvent *event = create_DeviceServiceDeviceRecoveredEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_DEVICE_RECOVERED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the device specific information
    //
    icDevice *device = deviceServiceGetDevice(uuid);
    if(device == NULL)
    {
        icLogError(LOG_TAG, "%s: no device found for uuid %s!", __func__,uuid);
        destroy_DeviceServiceDeviceRecoveredEvent(event);
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    event->deviceId = strdup(device->uuid);
    event->deviceClass = strdup(device->deviceClass);
    event->deviceUri = strdup(device->uri);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceDeviceRecoveredEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    deviceDestroy(device);
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceDeviceRecoveredEvent(event);
}

/*
 * broadcast a device removed event to any listeners
 *
 * @param uuid - the uuid of the device
 * @param deviceClass - the class of the device
 */
void sendDeviceRemovedEvent(const char *uuid, const char *deviceClass)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting device removed event, uuid=%s", uuid);

    DeviceServiceDeviceRemovedEvent *event = create_DeviceServiceDeviceRemovedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_DEVICE_REMOVED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the device specific information
    //
    event->deviceId = strdup(uuid);
    event->deviceClass = strdup(deviceClass);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceDeviceRemovedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceDeviceRemovedEvent(event);
}

/*
 * broadcast a resource updated event to any listeners
 *
 * @param resource - the resource that was updated
 * @param metadata - any optional metadata about the resource updated event
 */
void sendResourceUpdatedEvent(icDeviceResource *resource, cJSON* metadata)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting resource updated event, uri=%s, newValue=%s", resource->uri, resource->value);

    DeviceServiceResourceUpdatedEvent *event = create_DeviceServiceResourceUpdatedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_RESOURCE_UPDATED;
    setEventId(&(event->baseEvent));
    setEventTimeToNow(&(event->baseEvent));

    // add the optional details / metadata
    if(metadata != NULL)
    {
        event->details = cJSON_Duplicate(metadata, true);
    }

    // Cleanup what's there before we populate the new one
    destroy_DSResource(event->resource);

    // now the device specific information
    //
    event->resource = create_DSResource();
    event->resource->id = strdup(resource->id);
    event->resource->type = strdup(resource->type);
    event->resource->uri = strdup(resource->uri);
    event->resource->mode = resource->mode;
    event->resource->dateOfLastSyncMillis = resource->dateOfLastSyncMillis;

    icDevice *device = deviceServiceGetDevice(resource->deviceUuid);
    if(device != NULL)
    {
        event->rootDeviceId = strdup(device->uuid);
        event->rootDeviceClass = strdup(device->deviceClass);
    }

    if(resource->value != NULL)
    {
        event->resource->value = strdup(resource->value);
    }

    if(resource->endpointId == NULL)
    {
        if(device != NULL)
        {
            event->resource->ownerId = strdup(device->uuid);
            event->resource->ownerClass = strdup(device->deviceClass);
        }
    }
    else
    {
        icDeviceEndpoint *endpoint = deviceServiceGetEndpointById(resource->deviceUuid, resource->endpointId);
        if(endpoint != NULL)
        {
            event->resource->ownerId = strdup(endpoint->id);
            event->resource->ownerClass = strdup(endpoint->profile);

            endpointDestroy(endpoint);
        }
        else
        {
            icLogWarn(LOG_TAG, "endpoint not found (disabled?)... not sending event");

            destroy_DeviceServiceResourceUpdatedEvent(event);
            if(device != NULL)
            {
                deviceDestroy(device);
            }
            pthread_mutex_unlock(&EVENT_MTX);
            return;
        }
    }

    if(device != NULL)
    {
        deviceDestroy(device);
    }

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceResourceUpdatedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    destroy_DeviceServiceResourceUpdatedEvent(event);
    cJSON_Delete(jsonNode);
}

void sendEndpointAddedEvent(icDeviceEndpoint *endpoint, const char *deviceClass)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting endpoint added event, endpoint uri=%s", endpoint->uri);

    DeviceServiceEndpointAddedEvent *event = create_DeviceServiceEndpointAddedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_ENDPOINT_ADDED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the device specific information
    //
    event->details->deviceUuid = strdup(endpoint->deviceUuid);
    event->details->id = strdup(endpoint->id);
    event->details->uri = strdup(endpoint->uri);
    event->details->profile = strdup(endpoint->profile);
    event->details->deviceClass = strdup(deviceClass);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceEndpointAddedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceEndpointAddedEvent(event);
}

void sendEndpointRemovedEvent(icDeviceEndpoint *endpoint, const char *deviceClass)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting endpoint removed event, endpoint uri=%s", endpoint->uri);

    DeviceServiceEndpointRemovedEvent *event = create_DeviceServiceEndpointRemovedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_ENDPOINT_REMOVED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // now the device specific information
    //
    populateDSEndpoint(endpoint, event->endpoint);
    event->deviceClass = strdup(deviceClass);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceEndpointRemovedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceEndpointRemovedEvent(event);
}

void sendReadyForDevicesEvent()
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting ready for devices event");

    DeviceServiceReadyForDevicesEvent *event = create_DeviceServiceReadyForDevicesEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_READY_FOR_DEVICES;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceReadyForDevicesEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceReadyForDevicesEvent(event);
}

void sendZigbeeChannelChangedEvent(bool success, uint8_t currentChannel, uint8_t targetedChannel)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting zigbee channel changed event");

    DeviceServiceZigbeeChannelChangedEvent *event = create_DeviceServiceZigbeeChannelChangedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_CHANNEL_CHANGED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // add our event specific bits
    //
    event->success = success;
    event->currentChannel = currentChannel;
    event->targetedChannel = targetedChannel;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceZigbeeChannelChangedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceZigbeeChannelChangedEvent(event);
}

void sendZigbeeNetworkInterferenceEvent(bool interferenceDetected)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting zigbee network interference event");

    DeviceServiceZigbeeNetworkInterferenceChangedEvent *event = create_DeviceServiceZigbeeNetworkInterferenceChangedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_ZIGBEE_NETWORK_INTERFERENCE_CHANGED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // add our event specific bits
    //
    event->interferenceDetected = interferenceDetected;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceZigbeeNetworkInterferenceChangedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceZigbeeNetworkInterferenceChangedEvent(event);
}

void sendZigbeePanIdAttackEvent(bool attackDetected)
{
    pthread_mutex_lock(&EVENT_MTX);

    // perform sanity checks
    //
    if (producer == NULL)
    {
        icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
        pthread_mutex_unlock(&EVENT_MTX);
        return;
    }

    icLogDebug(LOG_TAG, "broadcasting zigbee PAN ID attack event");

    DeviceServiceZigbeePanIdAttackChangedEvent *event = create_DeviceServiceZigbeePanIdAttackChangedEvent();

    // first set normal 'baseEvent' crud
    //
    event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_ZIGBEE_PAN_ID_ATTACK_CHANGED;
    setEventId(&event->baseEvent);
    setEventTimeToNow(&event->baseEvent);

    // add our event specific bits
    //
    event->attackDetected = attackDetected;

    // convert to JSON object
    //
    cJSON *jsonNode = encode_DeviceServiceZigbeePanIdAttackChangedEvent_toJSON(event);

    // broadcast the encoded event
    //
    broadcastEvent(producer, jsonNode);
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    cJSON_Delete(jsonNode);
    destroy_DeviceServiceZigbeePanIdAttackChangedEvent(event);
}

/*
 * broadcast a device configuration started event
 *
 * @param uuid - the UUID of the device that is being configured
 * @param deviceClass - the class of the device being configured
 */
void sendDeviceConfigureStartedEvent(const char* deviceClass, const char *uuid)
{
    DeviceServiceDeviceConfigureStartedEvent *event;
    cJSON *json;

    pthread_mutex_lock(&EVENT_MTX);
    {
        // perform sanity checks
        //
        if (producer == NULL)
        {
            icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
            pthread_mutex_unlock(&EVENT_MTX);
            return;
        }

        if (deviceClass == NULL || uuid == NULL)
        {
            icLogWarn(LOG_TAG, "unable to broadcast event, invalid deviceClass and/or uuid");
            pthread_mutex_unlock(&EVENT_MTX);
            return;
        }

        icLogDebug(LOG_TAG, "broadcasting device configuration started event");

        event = create_DeviceServiceDeviceConfigureStartedEvent();

        // first set normal 'baseEvent' crud
        //
        event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_CONFIGURE_STARTED;
        setEventId(&(event->baseEvent));
        setEventTimeToNow(&(event->baseEvent));
        event->deviceClass = strdup(deviceClass);
        event->deviceId = strdup(uuid);

        // convert to JSON object
        //
        json = encode_DeviceServiceDeviceConfigureStartedEvent_toJSON(event);

        // broadcast the encoded event
        //
        broadcastEvent(producer, json);
    }
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    destroy_DeviceServiceDeviceConfigureStartedEvent(event);
    cJSON_Delete(json);

}

/*
 * broadcast a device configuration completed event
 *
 * @param uuid - the UUID of the device that was configured
 * @param deviceClass - the class of the device being configured
 */
void sendDeviceConfigureCompletedEvent(const char *deviceClass, const char *uuid)
{
    DeviceServiceDeviceConfigureCompletedEvent *event;
    cJSON *json;

    pthread_mutex_lock(&EVENT_MTX);
    {
        // perform sanity checks
        //
        if (producer == NULL)
        {
            icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
            pthread_mutex_unlock(&EVENT_MTX);
            return;
        }

        if (deviceClass == NULL || uuid == NULL)
        {
            icLogWarn(LOG_TAG, "unable to broadcast event, invalid deviceClass and/or uuid");
            pthread_mutex_unlock(&EVENT_MTX);
            return;
        }

        icLogDebug(LOG_TAG, "broadcasting device configuration completed event");

        event = create_DeviceServiceDeviceConfigureCompletedEvent();

        // first set normal 'baseEvent' crud
        //
        event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_CONFIGURE_COMPLETED;
        setEventId(&(event->baseEvent));
        setEventTimeToNow(&(event->baseEvent));
        event->deviceClass = strdup(deviceClass);
        event->deviceId = strdup(uuid);

        // convert to JSON object
        //
        json = encode_DeviceServiceDeviceConfigureCompletedEvent_toJSON(event);

        // broadcast the encoded event
        //
        broadcastEvent(producer, json);
    }
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    destroy_DeviceServiceDeviceConfigureCompletedEvent(event);
    cJSON_Delete(json);
}

/*
 * broadcast a device configuration failed event
 *
 * @param uuid - the UUID of the device that was configured
 * @param deviceClass - the class of the device being configured
 */
void sendDeviceConfigureFailedEvent(const char *deviceClass, const char *uuid)
{
    DeviceServiceDeviceConfigureFailedEvent *event;
    cJSON *json;

    pthread_mutex_lock(&EVENT_MTX);
    {
        // perform sanity checks
        //
        if (producer == NULL)
        {
            icLogWarn(LOG_TAG, "unable to broadcast event, producer not initialized");
            pthread_mutex_unlock(&EVENT_MTX);
            return;
        }

        if (deviceClass == NULL || uuid == NULL)
        {
            icLogWarn(LOG_TAG, "unable to broadcast event, invalid deviceClass and/or uuid");
            pthread_mutex_unlock(&EVENT_MTX);
            return;
        }

        icLogDebug(LOG_TAG, "broadcasting device configuration failed event");

        event = create_DeviceServiceDeviceConfigureFailedEvent();

        // first set normal 'baseEvent' crud
        //
        event->baseEvent.eventCode = DEVICE_SERVICE_EVENT_CONFIGURE_FAILED;
        setEventId(&(event->baseEvent));
        setEventTimeToNow(&(event->baseEvent));
        event->deviceClass = strdup(deviceClass);
        event->deviceId = strdup(uuid);

        // convert to JSON object
        //
        json = encode_DeviceServiceDeviceConfigureFailedEvent_toJSON(event);

        // broadcast the encoded event
        //
        broadcastEvent(producer, json);
    }
    pthread_mutex_unlock(&EVENT_MTX);

    // memory cleanup
    //
    destroy_DeviceServiceDeviceConfigureFailedEvent(event);
    cJSON_Delete(json);

}
