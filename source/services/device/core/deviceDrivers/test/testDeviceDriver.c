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
 * This is a simple test device driver that supports the 'test' device class
 * and returns dummy devices useful to test the device service.
 *
 * Created by Thomas Lea on 7/28/15.
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <deviceDriver.h>
#include <icLog/logging.h>
#include "../../src/deviceModelHelper.h"

#define LOG_TAG "testDeviceDriver"
#define DEVICE_DRIVER_NAME "testDeviceDriver"
#define DEVICE_CLASS_NAME "testDeviceClass"
#define DEVICE_PROFILE_NAME "testProfile"
#define DEVICE_UUID "testsomeuuid"

static DeviceServiceCallbacks *deviceServiceCallbacks = NULL;

static DeviceDriver *deviceDriver = NULL;

static bool discoverStart(void *ctx, const char *deviceClass);

static void discoverStop(void *ctx, const char *deviceClass);

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor);

static bool readResource(void *ctx, icDeviceResource *resource, char **value);

static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response);

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);

static void startup(void *ctx);
static void shutdown(void *ctx);
static void deviceRemoved(void *ctx, icDevice *device);

static char *devattr = NULL;

DeviceDriver *testDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    icLogDebug(LOG_TAG, "testDeviceDriverInitialize");

    deviceDriver = (DeviceDriver *) calloc(1, sizeof(DeviceDriver));
    if (deviceDriver != NULL)
    {
        deviceDriver->driverName = strdup(DEVICE_DRIVER_NAME);
        deviceDriver->startup = startup;
        deviceDriver->shutdown = shutdown;
        deviceDriver->discoverDevices = discoverStart;
        deviceDriver->stopDiscoveringDevices = discoverStop;
        deviceDriver->configureDevice = configureDevice;
        deviceDriver->readResource = readResource;
        deviceDriver->writeResource = writeResource;
        deviceDriver->executeResource = executeResource;
        deviceDriver->deviceRemoved = deviceRemoved;

        deviceDriver->supportedDeviceClasses = linkedListCreate();
        linkedListAppend(deviceDriver->supportedDeviceClasses, strdup(DEVICE_CLASS_NAME));

        deviceServiceCallbacks = deviceService;

    }
    else
    {
        icLogError(LOG_TAG, "failed to allocate DeviceDriver!");
    }

    return deviceDriver;
}

static void startup(void *ctx)
{
    devattr = strdup("device resource value");
}

static void shutdown(void *ctx)
{
    icLogDebug(LOG_TAG, "shutdown");

    free(devattr);

    if (deviceDriver != NULL)
    {
        free(deviceDriver->driverName);
        linkedListDestroy(deviceDriver->supportedDeviceClasses, NULL);
        free(deviceDriver);
        deviceDriver = NULL;
    }
    deviceServiceCallbacks = NULL;
}

static bool discoverStart(void *ctx, const char *deviceClass)
{
    icLogDebug(LOG_TAG, "discoverStart: deviceClass=%s", deviceClass);

    if (deviceServiceCallbacks == NULL)
    {
        icLogError(LOG_TAG, "Device driver not yet initialized!");
        return false;
    }

    DeviceFoundDetails deviceFoundDetails = {
            .deviceDriver = deviceDriver,
            .deviceMigrator = NULL,
            .deviceClass = DEVICE_CLASS_NAME,
            .deviceClassVersion = 1,
            .deviceUuid = DEVICE_UUID,
            .manufacturer = "testsomemanufacturer",
            .model = "testsomemodel",
            .hardwareVersion = "testsomehardwareversion",
            .firmwareVersion = "testsomefirmwareversion",
            .metadata = NULL,
            .endpointProfileMap = NULL
    };

    deviceServiceCallbacks->deviceFound(&deviceFoundDetails, false);

    return true;
}

static void discoverStop(void *ctx, const char *deviceClass)
{
    icLogDebug(LOG_TAG, "discoverStop");
}

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor)
{
    icLogDebug(LOG_TAG, "configureDevice: uuid=%s", device->uuid);

    createDeviceResource(device, "devatt1", "device resource value", "type/string", RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_NEVER);

    icDeviceEndpoint *endpoint = createEndpoint(device, "1", DEVICE_PROFILE_NAME, true);
    endpoint->profileVersion = 1;
    createEndpointResource(endpoint, "epatt1", "endpoint resource value", "type/string", RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_ALWAYS);

    return true;
}

static bool readResource(void *ctx, icDeviceResource *resource, char **value)
{
    bool result = false;

    if(resource == NULL || value == NULL)
    {
        return false;
    }

    if(resource->endpointId == NULL)
    {
        //this resource is on our root device
        if(strcmp(resource->id, "devatt1") == 0)
        {
            *value = strdup(devattr);

            result = true;
        }
    }
    else
    {
        // this resource is on an endpoint
        if(strcmp(resource->id, "epatt1") == 0)
        {
            *value = strdup("some value for epattr1");

            result = true;
        }
    }

    return result;
}

static bool executeResource(void *ctx, icDeviceResource *resource, const char *arg, char **response)
{
    icLogDebug(LOG_TAG, "executeResource: just returning true...");
    return true;
}

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue)
{
    if(resource == NULL)
    {
        icLogDebug(LOG_TAG, "writeResource: invalid arguments");
        return false;
    }

    if(resource->endpointId == NULL)
    {
        icLogDebug(LOG_TAG, "writeResource on device: id=%s, previousValue=%s, newValue=%s", resource->id, previousValue, newValue);
        if(strcmp(resource->id, "devatt1") == 0)
        {
            free(devattr);
            devattr = strdup(newValue);
        }
    }
    else
    {
        icLogDebug(LOG_TAG, "writeResource on endpoint %s: id=%s, previousValue=%s, newValue=%s", resource->endpointId, resource->id, previousValue, newValue);
    }

    deviceServiceCallbacks->updateResource(resource->deviceUuid, resource->endpointId, resource->id, newValue, NULL);

    return true;
}

static void deviceRemoved(void *ctx, icDevice *device)
{
}
