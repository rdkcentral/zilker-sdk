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
// Created by Thomas Lea on 10/30/15.
//

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdbool.h>
#include "deviceDriver.h"
#include <icLog/logging.h>
#include "deviceModelHelper.h"
#include "rtcoaWifiDeviceDriver.h"
#include <commonDeviceDefs.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <icUtil/macAddrUtils.h>
#include <rtcoaWifi/rtcoaWifi.h>
#include <resourceTypes.h>
#include <icBuildtime.h>
#include <deviceDriver.h>

#define LOG_TAG                         "RTCoAWifiDD"
#define DEVICE_CLASS_VERSION            1
#define DEVICE_DRIVER_NAME              "rtcoaWifiDeviceDriver"
#define CURL_RETRY_COUNT                5
#define CURL_RETRY_SLEEP_MICROSECONDS   500000      // half second

#define MANUFACTURER "RTCoA"
#define MODEL "CT80"

#ifdef CONFIG_SERVICE_DEVICE_RTCOA_TSTAT

//Device Driver interface function prototypes
static void startupDriver(void *ctx);

static void shutdownDriver(void *ctx);

static bool discoverStart(void *ctx, const char *deviceClass);

static void discoverStop(void *ctx, const char *deviceClass);

static void deviceRemoved(void *ctx, icDevice *device);

static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor);

static bool readResource(void *ctx, icDeviceResource *resource, char **value);

static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue);

static void processDeviceDescriptor(void *ctx, icDevice *device, DeviceDescriptor *dd);


//Other private prototypes
static void thermostatDiscoveredCallback(const char *macAddress, const char *ipAddress);
static void thermostatStateChangedCallback(const char *macAddress, const char *ipAddress);
static void thermostatIpChangedCallback(const char *macAddress, const char *newIpAddress);

static DeviceDriver *deviceDriver = NULL;

static DeviceServiceCallbacks *deviceServiceCallbacks = NULL;


typedef struct
{
    char *ipAddress;
    char *macAddress;
} PendingTstat;

static pthread_mutex_t pendingTstatsMutex = PTHREAD_MUTEX_INITIALIZER;
icHashMap *pendingTstats = NULL;
/*---------------------------------------------------------------------------------------
 * rtcoaWifiDeviceDriverInitialize : initialize device driver callbacks
 *---------------------------------------------------------------------------------------*/
DeviceDriver *rtcoaWifiDeviceDriverInitialize(DeviceServiceCallbacks *deviceService)
{
    icLogDebug(LOG_TAG, "rtcoaWifiDeviceDriverInitialize");

    deviceDriver = (DeviceDriver *) calloc(1, sizeof(DeviceDriver));

    if (deviceDriver != NULL)
    {
        deviceDriver->driverName = strdup(DEVICE_DRIVER_NAME);
        deviceDriver->startup = startupDriver;
        deviceDriver->shutdown = shutdownDriver;
        deviceDriver->discoverDevices = discoverStart;
        deviceDriver->stopDiscoveringDevices = discoverStop;
        deviceDriver->deviceRemoved = deviceRemoved;
        deviceDriver->configureDevice = configureDevice;
        deviceDriver->readResource = readResource;
        deviceDriver->writeResource = writeResource;
        deviceDriver->processDeviceDescriptor = processDeviceDescriptor;

        deviceDriver->supportedDeviceClasses = linkedListCreate();
        linkedListAppend(deviceDriver->supportedDeviceClasses, strdup(THERMOSTAT_DC));

        deviceServiceCallbacks = deviceService;

    }
    else
    {
        icLogError(LOG_TAG, "failed to allocate DeviceDriver!");
    }

    return deviceDriver;
}

static void startupDriver(void *ctx)
{
    icLogDebug(LOG_TAG, "startupDriver");

    icLinkedList *devices = deviceServiceCallbacks->getDevicesByDeviceDriver(DEVICE_DRIVER_NAME);
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);

        icDeviceResource *macAddressResource = deviceServiceCallbacks->getResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_MAC_ADDRESS);
        icDeviceResource *ipAddressResource = deviceServiceCallbacks->getResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_IP_ADDRESS);

        rtcoaWifiThermostatStartMonitoring(macAddressResource->value, ipAddressResource->value, thermostatStateChangedCallback, thermostatIpChangedCallback);

        resourceDestroy(macAddressResource);
        resourceDestroy(ipAddressResource);
    }
    linkedListIteratorDestroy(iterator);

    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

/*---------------------------------------------------------------------------------------
 * shutdownDriver : take down the device driver and free the memory
 *---------------------------------------------------------------------------------------*/
static void shutdownDriver(void *ctx)
{
    icLogDebug(LOG_TAG, "shutdownDriver");

    if (deviceDriver != NULL)
    {
        free(deviceDriver->driverName);
        linkedListDestroy(deviceDriver->supportedDeviceClasses, NULL);
        free(deviceDriver);
        deviceDriver = NULL;
    }

    deviceServiceCallbacks = NULL;
}

/*---------------------------------------------------------------------------------------
 * discoverStart : start discovering thermostats
 *
 * Starts a thread to discover thermostats in the background.
 *
 * Shutdown and cleanup of the thread and the resources below are handled in discoverStop.
 *---------------------------------------------------------------------------------------*/
static bool discoverStart(void *ctx, const char *deviceClass)
{
    icLogDebug(LOG_TAG, "discoverStart: deviceClass=%s", deviceClass);

    if (deviceServiceCallbacks == NULL)
    {
        icLogError(LOG_TAG, "Device driver not yet initialized!");
        return false;
    }

    pthread_mutex_lock(&pendingTstatsMutex);
    pendingTstats = hashMapCreate();
    pthread_mutex_unlock(&pendingTstatsMutex);

    rtcoaWifiThermostatStartDiscovery(thermostatDiscoveredCallback);

    return true;
}

static void pendingTstatFreeFunc(void *key, void *val)
{
    PendingTstat *tstat = (PendingTstat*)val;
    free(tstat->ipAddress);
    free(tstat->macAddress);
    free(tstat);

    free(key);
}

/*---------------------------------------------------------------------------------------
 * discoverStop : stop discovering thermostats
 *
 * Stop the discovery thread (if its running) and delete any 'pending' thermostats (thermostats
 * that have not been configured).
 *---------------------------------------------------------------------------------------*/
static void discoverStop(void *ctx, const char *deviceClass)
{
    icLogDebug(LOG_TAG, "discoverStop");

    rtcoaWifiThermostatStopDiscovery();

    pthread_mutex_lock(&pendingTstatsMutex);
    hashMapDestroy(pendingTstats, pendingTstatFreeFunc);
    pendingTstats = NULL;
    pthread_mutex_unlock(&pendingTstatsMutex);
}

static void thermostatStateChangedCallback(const char *macAddress, const char *ipAddress)
{
    printf("thermostatStateChangedCallback: %s", macAddress);

    char *uuid = (char*)calloc(1, 13); //12 plus null
    macAddrToUUID(uuid, macAddress);

    icDeviceResource *systemModeResource = deviceServiceCallbacks->getResource(uuid, "1", THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE);

    RtcoaWifiThermostatState *state = rtcoaWifiThermostatStateGetState(ipAddress);
    if(state != NULL)
    {
        char *currentSystemModeStr = NULL;
        switch(state->tmode)
        {
            case 0:
                currentSystemModeStr = "off";
                break;

            case 1:
                currentSystemModeStr = "heat";
                break;

            case 2:
                currentSystemModeStr = "cool";
                break;

            default:
                break;
        }

        if(currentSystemModeStr != NULL && strcmp(currentSystemModeStr, systemModeResource->value) != 0)
        {
            deviceServiceCallbacks->updateResource(uuid, "1", THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE, currentSystemModeStr, updateResourceEventChanged);
        }

        if(state->t_cool > 0) //it will have 0 if cooling isnt active
        {
            icDeviceResource *coolSetpointResource = deviceServiceCallbacks->getResource(uuid, "1",
                                                                                            THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT);
            char tempStr[6]; //xx.xx\0
            float celcius = (float) ((state->t_cool - 32.0) * ((float) 5 / 9));
            sprintf(tempStr, "%2.2f", celcius);
            if (strcmp(tempStr, coolSetpointResource->value) != 0)
            {
                deviceServiceCallbacks->updateResource(uuid, "1", THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT, tempStr, updateResourceEventChanged);
            }

            resourceDestroy(coolSetpointResource);
        }

        if(state->t_heat > 0)
        {
            icDeviceResource *heatSetpointResource = deviceServiceCallbacks->getResource(uuid, "1",
                                                                                            THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT);
            char tempStr[6]; //xx.xx\0
            float celcius = (float) ((state->t_heat - 32.0) * ((float) 5 / 9));
            sprintf(tempStr, "%2.2f", celcius);
            if (strcmp(tempStr, heatSetpointResource->value) != 0)
            {
                deviceServiceCallbacks->updateResource(uuid, "1", THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT, tempStr, updateResourceEventChanged);
            }

            resourceDestroy(heatSetpointResource);
        }

        if(state->temp > 0)
        {
            icDeviceResource *localTempResource = deviceServiceCallbacks->getResource(uuid, "1",
                                                                                            THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP);
            char tempStr[6]; //xx.xx\0
            float celcius = (float) ((state->temp - 32.0) * ((float) 5 / 9));
            sprintf(tempStr, "%2.2f", celcius);
            if (strcmp(tempStr, localTempResource->value) != 0)
            {
                deviceServiceCallbacks->updateResource(uuid, "1", THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP, tempStr, updateResourceEventChanged);
            }

            resourceDestroy(localTempResource);
        }

        rtcoaWifiThermostatStateDestroyState(state);
    }

    resourceDestroy(systemModeResource);

    free(uuid);
}

static void thermostatIpChangedCallback(const char *macAddress, const char *newIpAddress)
{
    icLogDebug(LOG_TAG, "thermostatIpChangedCallback: %s is now at %s", macAddress, newIpAddress);

    char *uuid = (char*)calloc(1, 13); //12 plus null
    macAddrToUUID(uuid, macAddress);
    deviceServiceCallbacks->updateResource(uuid, NULL, COMMON_DEVICE_RESOURCE_IP_ADDRESS, newIpAddress, updateResourceEventChanged);
    free(uuid);
}

static void deviceRemoved(void *ctx, icDevice *device)
{
    if(device != NULL && device->uuid != NULL)
    {
        icLogDebug(LOG_TAG, "deviceRemoved: %s", device->uuid);

        icDeviceResource *ipAddressResource = deviceServiceCallbacks->getResource(device->uuid, NULL, COMMON_DEVICE_RESOURCE_IP_ADDRESS);
        if(ipAddressResource != NULL)
        {
            rtcoaWifiThermostatStopMonitoring(ipAddressResource->value);
            resourceDestroy(ipAddressResource);
        }
    }
}

/*---------------------------------------------------------------------------------------
 * configureDevice : configure the thermostat
 *---------------------------------------------------------------------------------------*/
static bool configureDevice(void *ctx, icDevice *device, DeviceDescriptor *descriptor)
{
    icLogDebug(LOG_TAG, "configureDevice: uuid=%s", device->uuid);

    pthread_mutex_lock(&pendingTstatsMutex);
    PendingTstat *pendingTstat = (PendingTstat*)hashMapGet(pendingTstats, device->uuid, (uint16_t) (strlen(device->uuid) + 1));
    pthread_mutex_unlock(&pendingTstatsMutex);

    if(pendingTstat == NULL)
    {
        icLogError(LOG_TAG, "configureDevice: uuid %s not found in pending list", device->uuid);
        return false;
    }

    rtcoaWifiThermostatSetSimpleMode(pendingTstat->ipAddress, true);

    RtcoaWifiThermostatState *tstatState = rtcoaWifiThermostatStateGetState(pendingTstat->ipAddress);

    createDeviceResource(device, COMMON_DEVICE_RESOURCE_MAC_ADDRESS, pendingTstat->macAddress, RESOURCE_TYPE_MAC_ADDRESS, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createDeviceResource(device, COMMON_DEVICE_RESOURCE_IP_ADDRESS, pendingTstat->ipAddress, RESOURCE_TYPE_IP_ADDRESS, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);

    icDeviceEndpoint *endpoint = createEndpoint(device, "1", THERMOSTAT_PROFILE, true);
    createEndpointResource(endpoint, COMMON_ENDPOINT_RESOURCE_LABEL, NULL, RESOURCE_TYPE_LABEL, RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_ALWAYS);

    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_ON, "false", RESOURCE_TYPE_BOOLEAN, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE, "heat", RESOURCE_TYPE_TSTAT_SYSTEM_MODE, RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_HOLD_ON, "false", RESOURCE_TYPE_BOOLEAN, RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_FAN_MODE, "auto", RESOURCE_TYPE_TSTAT_FAN_MODE, RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_FAN_ON, "false", RESOURCE_TYPE_BOOLEAN, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP, "20.00", RESOURCE_TYPE_TEMPERATURE, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_ABS_MIN_HEAT, "1.67", RESOURCE_TYPE_TEMPERATURE, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_ABS_MAX_HEAT, "35.00", RESOURCE_TYPE_TEMPERATURE, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_ABS_MIN_COOL, "1.67", RESOURCE_TYPE_TEMPERATURE, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_ABS_MAX_COOL, "35.00", RESOURCE_TYPE_TEMPERATURE, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_LOCAL_TEMP_CALIBRATION, "0.0", RESOURCE_TYPE_TEMPERATURE, RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT, "30.00", RESOURCE_TYPE_TEMPERATURE, RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_ALWAYS);
    createEndpointResource(endpoint, THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT, "30.00", RESOURCE_TYPE_TEMPERATURE, RESOURCE_MODE_READWRITEABLE, CACHING_POLICY_ALWAYS);

    deviceServiceCallbacks->deviceConfigured(device);

    rtcoaWifiThermostatStartMonitoring(pendingTstat->macAddress, pendingTstat->ipAddress, thermostatStateChangedCallback, thermostatIpChangedCallback);

    rtcoaWifiThermostatStateDestroyState(tstatState);

    return true;
}

/*---------------------------------------------------------------------------------------
 * readResource :
 *---------------------------------------------------------------------------------------*/
static bool readResource(void *ctx, icDeviceResource *resource, char **value)
{
    //all of our attributes are currently cached...  nothing to return here
    return false;
}

/*---------------------------------------------------------------------------------------
 * writeResource :
 *---------------------------------------------------------------------------------------*/
static bool writeResource(void *ctx, icDeviceResource *resource, const char *previousValue, const char *newValue)
{
    if (resource == NULL)
    {
        icLogDebug(LOG_TAG, "writeResource: invalid arguments");
        return false;
    }

    if (resource->endpointId == 0)
    {
        icLogDebug(LOG_TAG, "writeResource on device [INVALID]: id=%s, previousValue=%s, newValue=%s", resource->id, previousValue, newValue);
    }
    else if (strcmp("1", resource->endpointId) == 0)
    {
        icLogDebug(LOG_TAG, "writeResource on endpoint %s: id=%s, previousValue=%s, newValue=%s", resource->endpointId, resource->id, previousValue, newValue);

        icDeviceResource *macAddressResource = deviceServiceCallbacks->getResource(resource->deviceUuid, NULL, COMMON_DEVICE_RESOURCE_MAC_ADDRESS);
        icDeviceResource *ipAddressResource = deviceServiceCallbacks->getResource(resource->deviceUuid, NULL, COMMON_DEVICE_RESOURCE_IP_ADDRESS);

        if(strcmp(resource->id, THERMOSTAT_PROFILE_RESOURCE_SYSTEM_MODE) == 0 && newValue != NULL)
        {
            RtcoaWifiThermostatOperatingMode mode;

            if(strcmp(newValue, "heat") == 0)
            {
                mode = RTCOA_WIFI_TSTAT_OP_MODE_HEAT;
            }
            else if(strcmp(newValue, "cool") == 0)
            {
                mode = RTCOA_WIFI_TSTAT_OP_MODE_COOL;
            }
            else if(strcmp(newValue, "off") == 0)
            {
                mode = RTCOA_WIFI_TSTAT_OP_MODE_OFF;
            }
            else
            {
                icLogError(LOG_TAG, "invalid system mode %s", newValue);
                resourceDestroy(macAddressResource);
                resourceDestroy(ipAddressResource);
                return false;
            }

            rtcoaWifiThermostatSetMode(ipAddressResource->value, mode);
            usleep(500000); //if we dont wait a bit, then when we go back to read the attribiute it hasnt changed yet.  Since we have to poll for changes, we dont have many options
            thermostatStateChangedCallback(macAddressResource->value, ipAddressResource->value); //trigger resource change handling now instead of waiting... harmless if it didnt actually change
        }
        else if(strcmp(resource->id, THERMOSTAT_PROFILE_RESOURCE_COOL_SETPOINT) == 0 && newValue != NULL)
        {
            float newTemp;
            sscanf(newValue, "%f", &newTemp);
            newTemp = (float) (1.8 * newTemp + 32.0); //convert to fahrenheit

            rtcoaWifiThermostatSetCoolSetpoint(ipAddressResource->value, newTemp);
            usleep(500000); //if we dont wait a bit, then when we go back to read the attribiute it hasnt changed yet.  Since we have to poll for changes, we dont have many options
            thermostatStateChangedCallback(macAddressResource->value, ipAddressResource->value); //trigger resource change handling now instead of waiting... harmless if it didnt actually change
        }
        else if(strcmp(resource->id, THERMOSTAT_PROFILE_RESOURCE_HEAT_SETPOINT) == 0 && newValue != NULL)
        {
            float newTemp;
            sscanf(newValue, "%f", &newTemp);
            newTemp = (float) (1.8 * newTemp + 32.0); //convert to fahrenheit

            rtcoaWifiThermostatSetHeatSetpoint(ipAddressResource->value, newTemp);
            usleep(500000); //if we dont wait a bit, then when we go back to read the attribiute it hasnt changed yet.  Since we have to poll for changes, we dont have many options
            thermostatStateChangedCallback(macAddressResource->value, ipAddressResource->value); //trigger resource change handling now instead of waiting... harmless if it didnt actually change
        }

        resourceDestroy(macAddressResource);
        resourceDestroy(ipAddressResource);
    }

    //TODO if this is an resource that actually goes to the thermostat, send it along.  For now, we will just pass up and pretend everything is good
    deviceServiceCallbacks->updateResource(resource->deviceUuid, resource->endpointId, resource->id, newValue, updateResourceEventChanged);

    return true;
}

static void thermostatDiscoveredCallback(const char *macAddress, const char *ipAddress)
{
    icLogDebug(LOG_TAG, "thermostat found: %s, %s\n", macAddress, ipAddress);

    pthread_mutex_lock(&pendingTstatsMutex);
    if(pendingTstats != NULL)
    {
        PendingTstat *tstat = (PendingTstat*)calloc(1, sizeof(PendingTstat));

        tstat->macAddress = strdup(macAddress);
        tstat->ipAddress = strdup(ipAddress);

        char *uuid = (char*)calloc(1, 13); //12 plus null
        macAddrToUUID(uuid, macAddress);
        hashMapPut(pendingTstats, (void *) uuid, (uint16_t) (strlen(uuid) + 1), tstat);
        pthread_mutex_unlock(&pendingTstatsMutex);

        deviceServiceCallbacks->deviceFound(deviceDriver, THERMOSTAT_DC, 1, strdup(uuid), MANUFACTURER, MODEL, "1", "1");
    }
    else
    {
        pthread_mutex_unlock(&pendingTstatsMutex);
    }
}

static void processDeviceDescriptor(void *ctx, icDevice *device, DeviceDescriptor *dd)
{
    icLogDebug(LOG_TAG, "processDeviceDescriptor: %s", device->uuid);
}

#endif
