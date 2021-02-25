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
 * Created by Thomas Lea on 9/30/15.
 */

#include <icLog/logging.h>
#include <device/icDeviceResource.h>
#include <device/icDeviceEndpoint.h>
#include <deviceDriver.h>
#include <deviceHelper.h>
#include <device/icDeviceMetadata.h>
#include "deviceServiceIpcCommon.h"
#include "deviceServicePrivate.h"

#define LOG_TAG "deviceServiceIpcCommon"

bool populateDSResource(icDeviceResource *resource, const char *class, DSResource *output)
{
    bool result = true;

    if(resource == NULL || output == NULL)
    {
        icLogError(LOG_TAG, "populateDSResource: invalid args");
        return false;
    }

    output->id = strdup(resource->id);
    output->uri = strdup(resource->uri);

    if(resource->endpointId != NULL)
    {
        output->ownerId = strdup(resource->endpointId);
    }
    else
    {
        output->ownerId = strdup(resource->deviceUuid);
    }

    if(class != NULL)
    {
        output->ownerClass = strdup(class);
    }

    if(resource->value != NULL)
    {
        output->value = strdup(resource->value);
    }

    output->type = strdup(resource->type);
    output->mode = resource->mode;
    output->dateOfLastSyncMillis = resource->dateOfLastSyncMillis;

    return result;
}

bool populateDSEndpoint(icDeviceEndpoint *endpoint, DSEndpoint *output)
{
    bool result = true;

    if(endpoint == NULL || output == NULL)
    {
        icLogError(LOG_TAG, "populateDSEndpoint: invalid args");
        return false;
    }

    output->id = strdup(endpoint->id);
    output->uri = strdup(endpoint->uri);
    output->ownerId = strdup(endpoint->deviceUuid);
    output->profile = strdup(endpoint->profile);
    output->profileVersion = endpoint->profileVersion;

    icLinkedListIterator *iterator = linkedListIteratorCreate(endpoint->resources);
    while(linkedListIteratorHasNext(iterator))
    {
        icDeviceResource *resource = (icDeviceResource*)linkedListIteratorGetNext(iterator);
        DSResource *dsResource = create_DSResource();
        populateDSResource(resource, endpoint->profile, dsResource);
        put_DSResource_in_DSEndpoint_resources(output, resource->uri, dsResource);
    }
    linkedListIteratorDestroy(iterator);

    if(endpoint->metadata != NULL)
    {
        iterator = linkedListIteratorCreate(endpoint->metadata);
        while(linkedListIteratorHasNext(iterator))
        {
            icDeviceMetadata *metadata = (icDeviceMetadata*)linkedListIteratorGetNext(iterator);
            put_metadataValue_in_DSEndpoint_metadata(output, metadata->id, metadata->value);

        }
        linkedListIteratorDestroy(iterator);
    }

    return result;
}

bool populateDSDevice(icDevice *device, DSDevice *output)
{
    bool result = true;

    if(device == NULL || output == NULL)
    {
        icLogError(LOG_TAG, "populateDSDevice: invalid args");
        return false;
    }

    output->id = strdup(device->uuid);
    output->uri = strdup(device->uri);
    output->deviceClass = strdup(device->deviceClass);
    output->deviceClassVersion = device->deviceClassVersion;
    output->managingDeviceDriver = strdup(device->managingDeviceDriver);

    icLinkedListIterator *iterator = linkedListIteratorCreate(device->resources);
    while(linkedListIteratorHasNext(iterator))
    {
        icDeviceResource *resource = (icDeviceResource*)linkedListIteratorGetNext(iterator);
        DSResource *dsResource = create_DSResource();
        populateDSResource(resource, device->deviceClass, dsResource);
        put_DSResource_in_DSDevice_resources(output, resource->uri, dsResource);
    }
    linkedListIteratorDestroy(iterator);

    iterator = linkedListIteratorCreate(device->endpoints);
    while(linkedListIteratorHasNext(iterator))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint*)linkedListIteratorGetNext(iterator);
        if(endpoint->enabled == true)
        {
            DSEndpoint *dsEndpoint = create_DSEndpoint();
            populateDSEndpoint(endpoint, dsEndpoint);
            put_DSEndpoint_in_DSDevice_endpoints(output, endpoint->uri, dsEndpoint);
        }
    }
    linkedListIteratorDestroy(iterator);

    if(device->metadata != NULL)
    {
        iterator = linkedListIteratorCreate(device->metadata);
        while(linkedListIteratorHasNext(iterator))
        {
            icDeviceMetadata *metadata = (icDeviceMetadata*)linkedListIteratorGetNext(iterator);
            put_metadataValue_in_DSDevice_metadata(output, metadata->id, metadata->value);

        }
        linkedListIteratorDestroy(iterator);
    }

    return result;
}
