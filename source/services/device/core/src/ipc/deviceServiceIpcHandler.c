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
// Created by Thomas Lea on 7/27/15.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icBuildtime.h>
#include <icIpc/ipcMessage.h>
#include <icIpc/eventConsumer.h>
#include <icUtil/stringUtils.h>
#include <deviceService.h>
#include <icLog/logging.h>                   // logging subsystem
#include <device/icDeviceResource.h>
#include <device/icDeviceEndpoint.h>
#include <propsMgr/propsHelper.h>
#include <deviceDescriptorHandler.h>
#include <propsMgr/commonProperties.h>
#include <zhal/zhal.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <subsystems/zigbee/zigbeeEventTracker.h>
#include <pthread.h>
#include <watchdog/serviceStatsHelper.h>
#include <device/icDeviceMetadata.h>
#include <icTime/timeUtils.h>
#include "deviceService_ipc_handler.h"            // local IPC handler
#include "deviceServicePrivate.h"
#include "deviceServiceIpcCommon.h"
#include "deviceServiceGatherer.h"

#define LOG_TAG "deviceServiceIpcHandler"

// same enum values as what power service sends us.
//
typedef enum
{
    SYSTEM_POWER_LEVEL_TEAR_DOWN    = 3,  // tell Zigbee to get out of LPM
    SYSTEM_POWER_LEVEL_STANDBY      = 4,  // tell Zigbee we're about to suspend
}deviceServiceLowPowerLevel;

/**
 * obtain the current runtime statistics of the service
 *   input - if true, reset stats after collecting them
 *   output - map of string/string to use for getting statistics
 **/
IPCCode handle_deviceService_GET_RUNTIME_STATS_request(bool input, runtimeStatsPojo *output)
{
    // gather stats about Event and IPC handling
    //
    collectEventStatistics(output, input);
    collectIpcStatistics(get_deviceService_ipc_receiver(), output, input);

    // memory process stats
    collectServiceStats(output);

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    // the state of Zigbee Core
    collectZigbeeCoreNetworkStatistics(output);

    // all device stats
    collectAllDeviceStatistics(output);

    // all zigbee counters
    collectZigbeeNetworkCounters(output);

    // all device firmware upgrade failures and success
    collectAllDeviceFirmwareEvents(output);

    // all the channel scan data stats
    collectChannelScanStats(output);

    // all camera stats
    collectCameraDeviceStats(output);
#endif

    output->serviceName = strdup(DEVICE_SERVICE_NAME);
    output->collectionTime = getCurrentUnixTimeMillis();

    return IPC_SUCCESS;
}

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 **/
IPCCode handle_deviceService_GET_SERVICE_STATUS_request(serviceStatusPojo *output)
{
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    // collect device status
    collectAllDeviceStatus(output);
#endif

    return IPC_SUCCESS;
}

/**
 * inform a service that the configuration data was restored, into 'restoreDir'.
 * allows the service an opportunity to import files from the restore dir into the
 * normal storage area.  only happens during RMA situations.
 *   details - both the 'temp dir' the config was extracted to, and the 'target dir' of where to store
 */
IPCCode handle_deviceService_CONFIG_RESTORED_request(configRestoredInput *input, configRestoredOutput* output)
{
    if(deviceServiceRestoreConfig(input->tempRestoreDir, input->dynamicConfigPath))
    {
        output->action = CONFIG_RESTORED_RESTART;
    }
    else
    {
        output->action = CONFIG_RESTORED_FAILED;
    }

    return IPC_SUCCESS;
}

/**
 * Start discovering devices of the provided device class
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_DISCOVER_DEVICES_BY_CLASS_request(DSDiscoverDevicesByClassRequest *input, bool *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    // add the single deviceClass into a list, then call 'discover'
    //
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, input->deviceClass);
    *output = deviceServiceDiscoverStart(deviceClasses, (uint16_t)(0xffff & input->timeoutSeconds), false);
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);

    return IPC_SUCCESS;
}

/**
 * Start discovering orphaned devices of the provided device class
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_DISCOVER_ORPHANED_DEVICES_BY_CLASS_request(DSDiscoverDevicesByClassRequest *input, bool *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    // add the single deviceClass into a list, then call 'discover'
    //
    icLinkedList *deviceClasses = linkedListCreate();
    linkedListAppend(deviceClasses, input->deviceClass);
    *output = deviceServiceDiscoverStart(deviceClasses, (uint16_t)(0xffff & input->timeoutSeconds), true);
    linkedListDestroy(deviceClasses, standardDoNotFreeFunc);

    return IPC_SUCCESS;
}

/**
 * Start discovering devices of the provided set of device class.  Similar to running DISCOVER_DEVICES_BY_CLASS multiple times.
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_DISCOVER_MULTI_DEVICES_BY_CLASS_request(DSDiscoverDevicesByClassSetRequest *input, bool *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    // call discover with the list of classes
    //
    *output = deviceServiceDiscoverStart(input->set, (uint16_t)(0xffff & input->timeoutSeconds), false);

    return IPC_SUCCESS;
}

/**
 * Retrieve the devices in the system.
 *   output - response from service
 **/
IPCCode handle_GET_DEVICES_request(DSDeviceList *output)
{
    IPCCode result = IPC_SUCCESS;

    if(output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icLinkedList *devices = deviceServiceGetAllDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while(linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice*)linkedListIteratorGetNext(iterator);

        DSDevice *data = create_DSDevice();
        populateDSDevice(device, data);

        linkedListAppend(output->devices, data);
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    icLogDebug(LOG_TAG, "response has %d devices", linkedListCount(output->devices));

    return result;
}

/**
 * Retrieve the devices in the system by device class.
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_GET_DEVICES_BY_DEVICE_CLASS_request(char *input, DSDeviceList *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icLinkedList *devices = deviceServiceGetDevicesByDeviceClass(input);
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while(linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice*)linkedListIteratorGetNext(iterator);

        DSDevice *data = create_DSDevice();
        populateDSDevice(device, data);

        linkedListAppend(output->devices, data);
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    icLogDebug(LOG_TAG, "response has %d devices", linkedListCount(output->devices));

    return result;
}

IPCCode handle_GET_DEVICE_BY_URI_request(char *input, DSDevice *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icDevice *device = deviceServiceGetDeviceByUri(input);

    if(device != NULL)
    {
        populateDSDevice(device, output);
        deviceDestroy(device);
        return IPC_SUCCESS;
    }

    return IPC_GENERAL_ERROR;
}

IPCCode handle_GET_DEVICE_BY_ID_request(char *input, DSDevice *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icDevice *device = deviceServiceGetDevice(input);

    if(device != NULL)
    {
        populateDSDevice(device, output);
        deviceDestroy(device);
        return IPC_SUCCESS;
    }

    return IPC_GENERAL_ERROR;
}

IPCCode handle_GET_DEVICES_BY_SUBSYSTEM_request(char *input, DSDeviceList *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icLinkedList *devices = deviceServiceGetDevicesBySubsystem(input);
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while(linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice*)linkedListIteratorGetNext(iterator);

        DSDevice *data = create_DSDevice();
        populateDSDevice(device, data);

        linkedListAppend(output->devices, data);
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    icLogDebug(LOG_TAG, "response has %d devices", linkedListCount(output->devices));

    return result;
}

IPCCode handle_STOP_DISCOVERING_DEVICES_request(bool *output)
{
    IPCCode result = IPC_SUCCESS;

    if(output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    *output = deviceServiceDiscoverStop(NULL);

    return result;
}

/**
 * Get a resource's details from a device
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_GET_RESOURCE_request(char *input, DSResource *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icDeviceResource *resource = deviceServiceGetResourceByUri(input);
    if(resource != NULL)
    {
        char *class = NULL;

        if(resource->endpointId != NULL)
        {
            icDeviceEndpoint *endpoint = deviceServiceGetEndpointByUri(resource->uri);
            if(endpoint != NULL)
            {
                class = strdup(endpoint->profile);
                endpointDestroy(endpoint);
            }
        }
        else
        {
            icDevice *device = deviceServiceGetDeviceByUri(resource->uri);
            if(device != NULL)
            {
                class = strdup(device->deviceClass);
                deviceDestroy(device);
            }
        }

        populateDSResource(resource, class, output);
        resourceDestroy(resource);
        free(class);
    }
    else
    {
        result = IPC_GENERAL_ERROR;
    }

    return result;
}

/**
 * Read a resource from a device or endpoint
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_READ_RESOURCE_request(char *input, DSReadResourceResponse *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icDeviceResource *resource = deviceServiceGetResourceByUri(input);
    if(resource != NULL)
    {
        if((resource->mode & RESOURCE_MODE_READABLE) == 0) //this resource is not readable
        {
            output->success = false;
        }
        else
        {
            output->success = true;
            output->response = resource->value != NULL ? strdup(resource->value) : NULL;
        }

        resourceDestroy(resource);
    }
    else
    {
        output->success = false;
        result = IPC_GENERAL_ERROR;
    }

    return result;
}

/**
 * Write a resource on a device
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_WRITE_RESOURCE_request(DSWriteResourceRequest *input, bool *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    if(deviceServiceWriteResource(input->uri, input->value) == true)
    {
        *output = true;
    }
    else
    {
        result = IPC_GENERAL_ERROR;
        *output = false;
    }

    return result;
}

/**
 * Execute a resource on a device or endpoint
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_EXECUTE_RESOURCE_request(DSExecuteResourceRequest *input, DSExecuteResourceResponse *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || input->uri == NULL || strlen(input->uri) == 0 || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    char *response = NULL;

    if(deviceServiceExecuteResource(input->uri, input->arg, &response) == true)
    {
        output->success = true;
    }
    else
    {
        result = IPC_GENERAL_ERROR;
        output->success = false;
    }

    // even if it failed there might be response with additional info
    output->response = response;

    return result;
}

/**
 * Remove a device by uuid.
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_REMOVE_DEVICE_request(char *input, bool *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    *output = deviceServiceRemoveDevice(input);

    if(*output == true)
    {
        return IPC_SUCCESS;
    }

    return IPC_GENERAL_ERROR;
}

/**
 * Retrieve endpoints in the system by profile
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_GET_ENDPOINTS_BY_PROFILE_request(char *input, DSEndpointList *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icLinkedList *endpoints = deviceServiceGetEndpointsByProfile(input);
    icLinkedListIterator *iterator = linkedListIteratorCreate(endpoints);
    while(linkedListIteratorHasNext(iterator))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint*)linkedListIteratorGetNext(iterator);
        if (endpoint->enabled == true)
        {
            DSEndpoint *dsEndpoint = create_DSEndpoint();
            populateDSEndpoint(endpoint, dsEndpoint);
            put_DSEndpoint_in_DSEndpointList_endpointList(output, dsEndpoint);
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(endpoints, (linkedListItemFreeFunc) endpointDestroy);

    icLogDebug(LOG_TAG, "response has %d endpoints", linkedListCount(output->endpointList));

    return result;
}

/**
 * Retrieve an endpoint from the system by id
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_GET_ENDPOINT_request(DSEndpointRequest *input, DSEndpoint *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icDeviceEndpoint *endpoint = deviceServiceGetEndpointById(input->deviceUuid, input->endpointId);

    if (endpoint != NULL && endpoint->enabled)
    {
        populateDSEndpoint(endpoint, output);
        endpointDestroy(endpoint);
        return IPC_SUCCESS;
    }

    return IPC_GENERAL_ERROR;
}

/**
 * Retrieve an endpoint from the system by uri
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_GET_ENDPOINT_BY_URI_request(char *input, DSEndpoint *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    IPCCode ipcCode = IPC_GENERAL_ERROR;

    icDeviceEndpoint *endpoint = deviceServiceGetEndpointByUri(input);

    if (endpoint != NULL && endpoint->enabled)
    {
        populateDSEndpoint(endpoint, output);
        ipcCode = IPC_SUCCESS;
    }

    endpointDestroy(endpoint);

    return ipcCode;
}

/**
 * Remove an endpoint from the system by id
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_REMOVE_ENDPOINT_request(DSEndpointRequest *input, bool *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    *output = deviceServiceRemoveEndpointById(input->deviceUuid, input->endpointId);
    return IPC_SUCCESS;
}

/**
 * Remove a device by uri.
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_REMOVE_DEVICE_BY_URI_request(char *input, bool *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icDevice *device = deviceServiceGetDeviceByUri(input);
    if(device != NULL)
    {
        deviceServiceRemoveDevice(device->uuid);
        deviceDestroy(device);
        return IPC_SUCCESS;
    }
    else
    {
        return IPC_GENERAL_ERROR;
    }
}

/**
 * Remove an endpoint from the system by uri
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_REMOVE_ENDPOINT_BY_URI_request(char *input, bool *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icDeviceEndpoint *endpoint = deviceServiceGetEndpointByUri(input);
    if(endpoint != NULL)
    {
        deviceServiceRemoveEndpointById(endpoint->deviceUuid, endpoint->id);
        endpointDestroy(endpoint);
        return IPC_SUCCESS;
    }
    else
    {
        return IPC_GENERAL_ERROR;
    }
}

/**
 * Retrieve a system property
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_GET_SYSTEM_PROPERTY_request(char *input, DSGetSystemPropertyResponse *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    char *value = NULL;

    if(deviceServiceGetSystemProperty(input, &value))
    {
        output->success = true;
        output->response = value;
        return IPC_SUCCESS;
    }
    else
    {
        output->success = false;
        return IPC_GENERAL_ERROR;
    }
}

/**
 * Retrieve a system property
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_SET_SYSTEM_PROPERTY_request(DSSetSystemPropertyRequest *input, bool *output)
{
    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    *output = deviceServiceSetSystemProperty(input->key, input->value);
    if(*output)
    {
        return IPC_SUCCESS;
    }
    else
    {
        return IPC_GENERAL_ERROR;
    }
}

/**
 * Read metadata from a device or endpoint
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_READ_METADATA_request(char *input, DSReadMetadataResponse *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    char *value = NULL;

    if(deviceServiceGetMetadata(input, &value))
    {
        output->success = true;
        output->response = value;
    }
    else
    {
        output->success = false;
        result = IPC_GENERAL_ERROR;
    }

    return result;
}

/**
 * Write metadata to a device or endpoint
 *   input - value to send to the service
 *   output - response from service (boolean)
 **/
IPCCode handle_WRITE_METADATA_request(DSWriteMetadataRequest *input, bool *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    if(deviceServiceSetMetadata(input->uri, input->value) == true)
    {
        *output = true;
    }
    else
    {
        result = IPC_GENERAL_ERROR;
        *output = false;
    }

    return result;
}


IPCCode handle_RELOAD_DATABASE_request(bool *output)
{
    IPCCode result = IPC_SUCCESS;

    if (deviceServiceReloadDatabase())
    {
        *output = true;
    }
    else
    {
        result = IPC_GENERAL_ERROR;
        *output = false;
    }

    return result;
}

/**
 * Find a set of metadata by their URI
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_QUERY_METADATA_BY_URI_request(char *input, DSMetadataUriMap *output)
{
    IPCCode result = IPC_SUCCESS;

    icLinkedList *metadatas = deviceServiceGetMetadataByUriPattern(input);
    if (metadatas != NULL)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(metadatas);
        while(linkedListIteratorHasNext(iter))
        {
            icDeviceMetadata *metadata = (icDeviceMetadata *)linkedListIteratorGetNext(iter);
            if(metadata != NULL)
            {
                put_metadataValue_in_DSMetadataUriMap_metadataByUri(output, metadata->uri, metadata->value);
            }
            else
            {
                result = IPC_GENERAL_ERROR;
            }
        }
        linkedListIteratorDestroy(iter);

        linkedListDestroy(metadatas, (linkedListItemFreeFunc)metadataDestroy);
    }
    else
    {
        result = IPC_GENERAL_ERROR;
    }

    return result;
}

IPCCode handle_QUERY_RESOURCES_BY_URI_request(char *input, DSResourceList *output)
{
    IPCCode result = IPC_SUCCESS;

    icLinkedList *resources = deviceServiceGetResourcesByUriPattern(input);
    if (resources != NULL)
    {
        icLinkedListIterator *iter = linkedListIteratorCreate(resources);
        while(linkedListIteratorHasNext(iter))
        {
            icDeviceResource *resource = (icDeviceResource *)linkedListIteratorGetNext(iter);
            if(resource != NULL)
            {
                // wow this sucks!
                char *class = NULL;

                if(resource->endpointId != NULL)
                {
                    icDeviceEndpoint *endpoint = deviceServiceGetEndpointByUri(resource->uri);
                    if(endpoint != NULL)
                    {
                        class = strdup(endpoint->profile);
                        endpointDestroy(endpoint);
                    }
                }
                else
                {
                    icDevice *device = deviceServiceGetDeviceByUri(resource->uri);
                    if(device != NULL)
                    {
                        class = strdup(device->deviceClass);
                        deviceDestroy(device);
                    }
                }
                DSResource *dsResource = create_DSResource();
                populateDSResource(resource, class, dsResource);
                linkedListAppend(output->resourceList, dsResource);
                free(class);
            }
            else
            {
                result = IPC_GENERAL_ERROR;
            }
        }
        linkedListIteratorDestroy(iter);

        linkedListDestroy(resources, (linkedListItemFreeFunc)resourceDestroy);
    }
    else
    {
        result = IPC_GENERAL_ERROR;
    }

    return result;
}

IPCCode handle_PROCESS_DEVICE_DESCRIPTORS_request(bool *output)
{
    // If we are using an override, force an update whenever we get this IPC request
    char *overrideUrl = getPropertyAsString(DEVICE_DESC_WHITELIST_URL_OVERRIDE, NULL);
    if (overrideUrl != NULL)
    {
        deviceDescriptorsUpdateWhitelist(overrideUrl);
        free(overrideUrl);
    }

    deviceServiceProcessDeviceDescriptors();

    *output = true;
    return IPC_SUCCESS;
}


IPCCode handle_RESOURCE_EXISTS_request(char *input, bool *output)
{
    IPCCode result = IPC_SUCCESS;

    if(input == NULL || output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    icDeviceResource *resource = deviceServiceGetResourceByUri(input);
    if(resource != NULL)
    {
        *output = true;
        resourceDestroy(resource);
    }
    else
    {
        *output = false;
    }

    return result;
}

IPCCode handle_GET_ZIGBEE_SUBSYSTEM_STATUS_request(DSZigbeeSubsystemStatus *output)
{
    if(output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

#ifndef CONFIG_SERVICE_DEVICE_ZIGBEE
    icLogError(LOG_TAG, "%s: Zigbee support not enabled", __FUNCTION__);
    output->isAvailable = false;
    return IPC_SERVICE_DISABLED;
#else
    zhalSystemStatus status;
    memset(&status, 0, sizeof(zhalSystemStatus));
    if(zigbeeSubsystemGetSystemStatus(&status) != 0)
    {
        icLogError(LOG_TAG, "%s: zigbeeSubsystemGetSystemStatus failed", __FUNCTION__);
        return IPC_GENERAL_ERROR;
    }

    output->isAvailable = true;
    output->isUp = status.networkIsUp;
    output->isOpenForJoin = status.networkIsOpenForJoin;
    output->eui64 = zigbeeSubsystemEui64ToId(status.eui64);
    output->originalEui64 = zigbeeSubsystemEui64ToId(status.originalEui64);
    output->channel = status.channel;
    output->panId = status.panId;
    output->networkKey = (char*)calloc(1, 33); //16 hex bytes + \0
    for(int i = 15; i >= 0; i--)
    {
        sprintf(output->networkKey+((15-i)*2), "%02x", status.networkKey[i]);
    }

    return IPC_SUCCESS;
#endif
}

IPCCode handle_GET_ZIGBEE_COUNTERS_request(char **output)
{
    if(output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

#ifndef CONFIG_SERVICE_DEVICE_ZIGBEE
    icLogError(LOG_TAG, "%s: Zigbee support not enabled", __FUNCTION__);
    *output = NULL;
    return IPC_SERVICE_DISABLED;
#else
    cJSON *counters = zigbeeSubsystemGetAndClearCounters();
    if(counters != NULL)
    {
        *output = cJSON_PrintUnformatted(counters);
    }
    else
    {
        return IPC_GENERAL_ERROR;
    }

    return IPC_SUCCESS;
#endif
}

/**
 * Check for whether all subsystems are ready to start working with devices
 *   output - response from service (boolean)
 **/
IPCCode handle_READY_FOR_DEVICES_request(bool *output)
{
    if(output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

    *output = deviceServiceIsReadyForDevices();

    return IPC_SUCCESS;
}

/**
 * Attempt to change the current zigbee channel
 *   input - value to send to the service
 *   output - response from service
 **/
IPCCode handle_CHANGE_ZIGBEE_CHANNEL_request(DSZigbeeChangeChannelRequest *input, DSZigbeeChangeChannelResponse *output)
{
    if (output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

#ifndef CONFIG_SERVICE_DEVICE_ZIGBEE
    icLogError(LOG_TAG, "%s: Zigbee support not enabled", __FUNCTION__);
    return IPC_SERVICE_DISABLED;
#else
    ChannelChangeResponse channelChangeResponse = zigbeeSubsystemChangeChannel(input->channel, input->dryRun);
    output->channel = channelChangeResponse.channelNumber;

    switch(channelChangeResponse.responseCode)
    {
        case channelChangeSuccess:
            output->status = CHANNEL_CHANGE_STATUS_SUCCESS;
            break;

        case channelChangeNotAllowed:
            output->status = CHANNEL_CHANGE_STATUS_NOT_ALLOWED;
            break;

        case channelChangeInvalidChannel:
            output->status = CHANNEL_CHANGE_STATUS_INVALID_CHANNEL;
            break;

        case channelChangeInProgress:
            output->status = CHANNEL_CHANGE_STATUS_IN_PROGRESS;
            break;

        case channelChangeUnableToCalculate:
            output->status = CHANNEL_CHANGE_STATUS_FAILED_TO_CALCULATE;
            break;

        case channelChangeUnknown:
            output->status = CHANNEL_CHANGE_STATUS_UNKNOWN;
            break;

        case channelChangeFailed:
        default:
            output->status = CHANNEL_CHANGE_STATUS_FAILED;
            break;
    }

    return IPC_SUCCESS;
#endif
}

IPCCode handle_GET_ZIGBEE_NETWORK_MAP_request(DSZigbeeNetworkMap *output)
{
    if (output == NULL)
    {
        return IPC_INVALID_ERROR;
    }

#ifndef CONFIG_SERVICE_DEVICE_ZIGBEE
    icLogError(LOG_TAG, "%s: Zigbee support not enabled", __FUNCTION__);
    return IPC_SERVICE_DISABLED;
#else
    icLinkedList *networkMap = zigbeeSubsystemGetNetworkMap();

    icLinkedListIterator *iter = linkedListIteratorCreate(networkMap);
    while(linkedListIteratorHasNext(iter))
    {
        ZigbeeSubsystemNetworkMapEntry *entry = (ZigbeeSubsystemNetworkMapEntry *)linkedListIteratorGetNext(iter);
        DSZigbeeNetworkMapEntry *mapEntry = create_DSZigbeeNetworkMapEntry();
        mapEntry->address = zigbeeSubsystemEui64ToId(entry->address);
        mapEntry->nextCloserHop = zigbeeSubsystemEui64ToId(entry->nextCloserHop);
        mapEntry->lqi = entry->lqi;
        linkedListAppend(output->entries, mapEntry);
    }
    linkedListIteratorDestroy(iter);

    // Cleanup
    linkedListDestroy(networkMap, NULL);

    return IPC_SUCCESS;
#endif
}

/**
 * Called by powerService as we go into/out-of low power modes.
 *   input - value to send to the service
 **/
IPCCode handle_LOW_POWER_MODE_CHANGED_DEVICE_request(int32_t input)
{
    // note, the 'input' integer correlates to lowPowerLevel
    // from power service, but deviceServiceLowPowerLevel is the
    // same enum just doesn't require the hook into power service
    //
    deviceServiceLowPowerLevel level = (deviceServiceLowPowerLevel)input;

    if (level == SYSTEM_POWER_LEVEL_TEAR_DOWN)
    {
        deviceServiceExitLowPowerMode();
    }
    else if(level == SYSTEM_POWER_LEVEL_STANDBY)
    {
        deviceServiceEnterLowPowerMode();
    }

    return IPC_SUCCESS;
}

IPCCode handle_GET_ZIGBEE_FIRMWARE_VERSION_request(char **output)
{
    IPCCode result = IPC_SUCCESS;
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    *output = zigbeeSubsystemGetFirmwareVersion();
    if (*output == NULL)
    {
        result = IPC_GENERAL_ERROR;
    }
#else
    *output = NULL;
    result = IPC_SERVICE_DISABLED;
#endif

    return result;
}

/**
 * Determine if we are actively discovering devices
 *   output - response from service (boolean)
 **/
IPCCode handle_IS_DISCOVERY_ACTIVE_request(bool *output)
{
    IPCCode result = IPC_SUCCESS;
    *output = deviceServiceIsDiscoveryActive();
    return result;
}

IPCCode handle_CHANGE_RESOURCE_MODE_request(DSChangeResourceModeRequest *input, bool *output)
{
    IPCCode result = IPC_SUCCESS;

    *output = deviceServiceChangeResourceMode(input->uri, input->newMode);

    return result;
}

IPCCode handle_ZIGBEE_ENERGY_SCAN_request(DSZigbeeEnergyScanRequest *input, DSZigbeeEnergyScanResponse *output)
{
    IPCCode result = IPC_GENERAL_ERROR;

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    if (input->channels == NULL)
    {
        icLogError(LOG_TAG, "%s: no channels provided", __FUNCTION__);
        return IPC_INVALID_ERROR;
    }

    uint16_t numChannels = linkedListCount(input->channels);
    if (numChannels == 0 || numChannels > 16) //There are sixteen 802.15.4 channels
    {
        icLogError(LOG_TAG, "%s: invalid number of channels", __FUNCTION__);
        return IPC_INVALID_ERROR;
    }

    uint8_t *channels = calloc(numChannels, 1);
    icLinkedListIterator *it = linkedListIteratorCreate(input->channels);
    int i = 0;
    while(linkedListIteratorHasNext(it))
    {
        int32_t *channel = linkedListIteratorGetNext(it);

        if (*channel < 11 || *channel > 26) //802.15.4 channels are 11-26
        {
            icLogError(LOG_TAG, "%s: invalid channel input %d", __FUNCTION__, *channel);
            free(channels);
            channels = NULL;
            break;
        }

        channels[i++] = (uint8_t) *channel;
    }
    linkedListIteratorDestroy(it);

    if (channels != NULL)
    {
        icLinkedList *results = zigbeeSubsystemPerformEnergyScan(channels,
                                                                 numChannels,
                                                                 input->durationMs,
                                                                 input->numScans);

        it = linkedListIteratorCreate(results);
        while(linkedListIteratorHasNext(it))
        {
            zhalEnergyScanResult *scanResult = linkedListIteratorGetNext(it);

            DSZigbeeEnergyScanResult *ipcScanResult = create_DSZigbeeEnergyScanResult();
            ipcScanResult->channel = scanResult->channel;
            ipcScanResult->maxRssi = scanResult->maxRssi;
            ipcScanResult->minRssi = scanResult->minRssi;
            ipcScanResult->averageRssi = scanResult->averageRssi;
            ipcScanResult->score = scanResult->score;

            put_DSZigbeeEnergyScanResult_in_DSZigbeeEnergyScanResponse_scanResults(output, ipcScanResult);
        }
        linkedListIteratorDestroy(it);

        free(channels);
        linkedListDestroy(results, NULL);
        result = IPC_SUCCESS;
    }
#else
    result = IPC_SERVICE_DISABLED;
#endif

    return result;
}

IPCCode handle_GET_STATUS_request(DSStatus *output)
{
    DeviceServiceStatus *status = deviceServiceGetStatus();

    output->zigbeeReady = status->zigbeeReady;

    // ditch the empty linked list that is automatically created for the pojo and replace
    // with the list we get from device service.
    linkedListDestroy(output->supportedDeviceClasses, NULL);
    output->supportedDeviceClasses = status->supportedDeviceClasses;
    status->supportedDeviceClasses = NULL;

    output->discoveryRunning = status->discoveryRunning;

    if (status->discoveryRunning == true)
    {
        output->discoveryTimeoutSeconds = status->discoveryTimeoutSeconds;

        // move the link list over to the output
        linkedListDestroy(output->discoveringDeviceClasses, NULL);
        output->discoveringDeviceClasses = status->discoveringDeviceClasses;
        status->discoveringDeviceClasses = NULL;
    }

    deviceServiceDestroyServiceStatus(status);

    return IPC_SUCCESS;
}

IPCCode handle_ZIGBEE_TEST_REQUEST_LEAVE_request(DSZigbeeRequestLeave *input)
{
    IPCCode result = IPC_GENERAL_ERROR;

    if (input == NULL)
    {
        return IPC_INVALID_ERROR;
    }

#ifndef CONFIG_SERVICE_DEVICE_ZIGBEE
    icLogError(LOG_TAG, "%s: Zigbee support not enabled", __FUNCTION__);
    return IPC_SERVICE_DISABLED;
#else
    // Note:  Calling the zhal layer directly from an IPC handler
    //        is not normally allowed.
    //
    // Since this IPC handler is for test purposes only, we are calling
    // zhalRequestLeave() instead of expanding the zigbee subsystem.
    //
    uint64_t eui64 = zigbeeSubsystemIdToEui64(input->eui64);
    if (zhalRequestLeave(eui64, input->withRejoin, input->isEndDevice) == ZHAL_STATUS_OK)
    {
        result = IPC_SUCCESS;
    }

    return result;
#endif
}
