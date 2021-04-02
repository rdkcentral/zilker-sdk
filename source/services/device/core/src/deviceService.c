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
// Created by Thomas Lea on 7/28/15.
//

#include <sys/time.h>
#include <inttypes.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <pthread.h>
#include <database/jsonDatabase.h>

#include <icLog/logging.h>
#include <icTime/timeUtils.h>
#include <icConcurrent/timedWait.h>
#include <icUtil/stringUtils.h>
#include <icSystem/runtimeAttributes.h>
#include <propsMgr/paths.h>
#include <propsMgr/timezone.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsHelper.h>
#include <watchdog/watchdogService_ipc.h>
#include <deviceService/deviceService_event.h>
#include <backup/backupRestoreService_ipc.h>
#include <device/icDeviceMetadata.h>
#include <deviceService.h>
#include <commonDeviceDefs.h>
#include <deviceDescriptors.h>
#include <resourceTypes.h>
#include <device/icDeviceResource.h>
#include <database/jsonDatabase.h>
#include <propsMgr/logLevel.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <deviceHelper.h>
#include <icConcurrent/threadPool.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/delayedTask.h>
#include "zigbeeDriverCommon.h"
#include "deviceDriver.h"
#include "ipc/deviceServiceEventHandler.h"
#include "deviceService_ipc_handler.h"
#include "ipc/deviceEventProducer.h"
#include "subsystems/zigbee/zigbeeEventTracker.h"
#include "deviceDriverManager.h"
#include "deviceServicePrivate.h"
#include "deviceModelHelper.h"
#include "subsystemManager.h"
#include "deviceCommunicationWatchdog.h"
#include "deviceDescriptorHandler.h"

#define LOG_TAG "deviceService"

#define DEVICE_DESCRIPTOR_BYPASS_SYSTEM_PROP    "deviceDescriptorBypass"

#define SHOULD_NOT_PERSIST_AFTER_RMA_METADATA_NAME "shouldNotPersistAfterRMA"

static bool isDeviceServiceInLPM = false;

static pthread_mutex_t lowPowerModeMutex = PTHREAD_MUTEX_INITIALIZER;

char *deviceServiceConfigDir = NULL; //non-static with namespace friendly name so it can be altered by test code

/* Private functions */
static DeviceDriver *getDeviceDriverForUri(const char *uri);

static char *getEndpointUri(const char *deviceUuid, const char *endpointId);

static bool finalizeNewDevice(icDevice *device, bool sendEvents, bool inRepairMode);

static void subsystemManagerInitializedCallback(const char *subsystem);

static void subsystemManagerReadyForDevicesCallback();

static void deviceDescriptorsReadyForDevicesCallback();

static void descriptorsUpdatedCallback();

static bool deviceServiceIsReadyForDevicesInternal();

static icDeviceResource *deviceServiceGetResourceByIdInternal(const char *deviceUuid,
                                                              const char *endpointId,
                                                              const char *resourceId,
                                                              bool logDebug);

static void blacklistDevice(const char *uuid);

/* Private data */
typedef struct
{
    char *deviceClass;
    uint16_t timeoutSeconds;
    bool findOrphanedDevices;

    //used for shutdown delay and/or canceling
    bool useMonotonic;
    pthread_cond_t cond;
    pthread_mutex_t mtx;
} discoverDeviceClassContext;

static icHashMap *activeDiscoveries = NULL;

static uint16_t discoveryTimeoutSeconds = 0;

static pthread_mutex_t discoveryControlMutex = PTHREAD_MUTEX_INITIALIZER;     // mutex for the event producer

static discoverDeviceClassContext *startDiscoveryForDeviceClass(const char *deviceClass, uint16_t timeoutSeconds, bool findOrphanedDevices);

static pthread_mutex_t readyForDevicesMutex = PTHREAD_MUTEX_INITIALIZER;

static bool subsystemsReadyForDevices = false;

static bool deviceDescriptorReadyForDevices = false;

//set of UUIDs that are not fully added yet but have been marked to be removed.
static icHashMap *markedForRemoval = NULL;

static pthread_mutex_t markedForRemovalMutex = PTHREAD_MUTEX_INITIALIZER;

static icThreadPool *deviceInitializerThreadPool = NULL;
#define MAX_DEVICE_SYNC_THREADS 5
#define MAX_DEVICE_SYNC_QUEUE 128
static void startDeviceInitialization();

//1 more minute than we allow for a legacy sensor to upgrade
#define MAX_DRIVERS_SHUTDOWN_SECS (31 * 60)
static void shutdownDeviceDriverManager();

static pthread_mutex_t deviceDriverManagerShutdownMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t deviceDriverManagerShutdownCond = PTHREAD_COND_INITIALIZER;

#define DEVICE_DESCRIPTOR_PROCESSOR_DELAY_SECS 30
static uint32_t deviceDescriptorProcessorTask = 0;
static pthread_mutex_t deviceDescriptorProcessorMtx = PTHREAD_MUTEX_INITIALIZER;

bool deviceServiceRestoreConfig(const char *tempRestoreDir, const char *dynamicConfigPath)
{
    bool didSomething = false;

    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    icLinkedListIterator *preIter = linkedListIteratorCreate(deviceDrivers);

    // Let any drivers do anything they need to do pre config
    //
    while(linkedListIteratorHasNext(preIter))
    {
        DeviceDriver *driver = (DeviceDriver *)linkedListIteratorGetNext(preIter);
        if (driver->preRestoreConfig != NULL)
        {
            icLogDebug(LOG_TAG, "%s: performing pre restore config actions for driver %s", __FUNCTION__, driver->driverName);
            driver->preRestoreConfig(driver->callbackContext);
        }
    }

    linkedListIteratorDestroy(preIter);

    if (jsonDatabaseRestore(tempRestoreDir, dynamicConfigPath) == true)
    {
        didSomething = true;
    }
    else
    {
        icLogWarn(LOG_TAG, "Failed to restore json database config");
    }

    // Let any subsystems do their thing
    if (subsystemManagerRestoreConfig(tempRestoreDir, dynamicConfigPath) == true)
    {
        didSomething = true;
    }
    else
    {
        icLogWarn(LOG_TAG, "Failed to restore subsystem config");
    }

    icLinkedListIterator *iter = linkedListIteratorCreate(deviceDrivers);
    // Let any drivers do anything they need to do
    while(linkedListIteratorHasNext(iter))
    {
        DeviceDriver *driver = (DeviceDriver *)linkedListIteratorGetNext(iter);
        if (driver->restoreConfig != NULL)
        {
            if (driver->restoreConfig(driver->callbackContext, tempRestoreDir, dynamicConfigPath) == false)
            {
                icLogWarn(LOG_TAG, "Failed to restore config for driver %s", driver->driverName);
            }
            else
            {
                didSomething = true;
            }
        }
    }
    linkedListIteratorDestroy(iter);

    // ----------------------------------------------------------------
    // get list of devices and loop though them to check 'shouldNotPersistAfterRMA' metadata
    // ----------------------------------------------------------------
    icLinkedList *deviceList = deviceServiceGetAllDevices();
    icLinkedListIterator *deviceIter = linkedListIteratorCreate(deviceList);

    while(linkedListIteratorHasNext(deviceIter) == true)
    {
        // get device
        //
        icDevice *device = (icDevice *) linkedListIteratorGetNext(deviceIter);

        if (device->uuid == NULL || device->deviceClass == NULL)
        {
            icLogError(LOG_TAG, "%s: unable to use device with uuid=%s and deviceClass=%s", __FUNCTION__,
                       stringCoalesce(device->uuid), stringCoalesce(device->deviceClass));
            continue;
        }

        bool blacklistDeviceAfterRma = getBooleanMetadata(device->uuid, NULL, SHOULD_NOT_PERSIST_AFTER_RMA_METADATA_NAME);

        if (blacklistDeviceAfterRma == true)
        {
            blacklistDevice(device->uuid);
        }
    }

    linkedListIteratorDestroy(deviceIter);
    linkedListDestroy(deviceList, (linkedListItemFreeFunc) deviceDestroy);

    subsystemManagerPostRestoreConfig();

    icLinkedListIterator *postIter = linkedListIteratorCreate(deviceDrivers);

    // Let any drivers do anything they need to do post config
    //
    while(linkedListIteratorHasNext(postIter))
    {
        DeviceDriver *driver = (DeviceDriver *)linkedListIteratorGetNext(postIter);
        if (driver->postRestoreConfig != NULL)
        {
            icLogDebug(LOG_TAG, "%s: performing post restore config actions for driver %s", __FUNCTION__, driver->driverName);
            driver->postRestoreConfig(driver->callbackContext);
        }
    }

    linkedListIteratorDestroy(postIter);

    // This is a shallow clone, so free function doesn't really matter
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);

    return didSomething;
}

/*
 * assume 'deviceClasses' is a list of strings
 */
bool deviceServiceDiscoverStart(icLinkedList *deviceClasses, uint16_t timeoutSeconds, bool findOrphanedDevices)
{
    bool result = deviceClasses != NULL;

    icLogDebug(LOG_TAG, "deviceServiceDiscoverStart");

    if (deviceClasses != NULL)
    {
        // Check and warn, but don't fail for now
        if (!deviceServiceIsReadyForDevices())
        {
            icLogWarn(LOG_TAG, "discover start called before we are fully ready");
        }
        pthread_mutex_lock(&discoveryControlMutex);

        discoveryTimeoutSeconds = timeoutSeconds;

        icLinkedList *newDeviceClassDiscoveries = linkedListCreate();

        icLinkedListIterator *iterator = linkedListIteratorCreate(deviceClasses);
        while (linkedListIteratorHasNext(iterator) && result)
        {
            char *deviceClass = linkedListIteratorGetNext(iterator);

            //ensure we arent already discovering for this device class
            if (hashMapGet(activeDiscoveries, deviceClass, (uint16_t) (strlen(deviceClass) + 1)) != NULL)
            {
                icLogWarn(LOG_TAG,
                          "deviceServiceDiscoverStart: asked to start discovery for device class %s which is already running",
                          deviceClass);
                continue;
            }

            icLinkedList *drivers = deviceDriverManagerGetDeviceDriversByDeviceClass(deviceClass);

            // Indicate OK only when all device classes have at least one supported driver
            if (drivers != NULL && linkedListCount(drivers) > 0)
            {
                if (findOrphanedDevices == true)
                {
                    int recoveryDriversFound = 0;
                    icLinkedListIterator *driverIterator = linkedListIteratorCreate(drivers);
                    while (linkedListIteratorHasNext(driverIterator) == true)
                    {
                        DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(driverIterator);
                        if (driver->recoverDevices != NULL)
                        {
                            recoveryDriversFound++;
                        }
                        else
                        {
                            icLogWarn(LOG_TAG,"driver %s does not support device recovery",driver->driverName);
                        }
                    }
                    linkedListIteratorDestroy(driverIterator);
                    if (recoveryDriversFound  == 0)
                    {
                        result = false;
                    }
                }
                if (result == true)
                {
                    linkedListAppend(newDeviceClassDiscoveries, deviceClass);
                }
            }
            else
            {
                result = false;
            }

            linkedListDestroy(drivers, standardDoNotFreeFunc);
        }
        linkedListIteratorDestroy(iterator);

        result = result && linkedListCount(newDeviceClassDiscoveries) != 0;
        if (result)
        {
            sendDiscoveryStartedEvent(newDeviceClassDiscoveries, timeoutSeconds);

            iterator = linkedListIteratorCreate(newDeviceClassDiscoveries);
            while (linkedListIteratorHasNext(iterator))
            {
                const char *deviceClass = linkedListIteratorGetNext(iterator);

                discoverDeviceClassContext *ctx = startDiscoveryForDeviceClass(deviceClass, timeoutSeconds, findOrphanedDevices);
                hashMapPut(activeDiscoveries, ctx->deviceClass, (uint16_t) (strlen(deviceClass) + 1), ctx);
            }
            linkedListIteratorDestroy(iterator);
        }

        linkedListDestroy(newDeviceClassDiscoveries, standardDoNotFreeFunc);

        pthread_mutex_unlock(&discoveryControlMutex);
    }

    return result;
}

bool deviceServiceDiscoverStop(icLinkedList *deviceClasses)
{
    pthread_mutex_lock(&discoveryControlMutex);

    if (deviceClasses == NULL) //stop all active discovery
    {
        icLogDebug(LOG_TAG, "deviceServiceDiscoverStop: stopping all active discoveries");

        icHashMapIterator *iterator = hashMapIteratorCreate(activeDiscoveries);
        while (hashMapIteratorHasNext(iterator))
        {
            char *class;
            uint16_t classLen;
            discoverDeviceClassContext *ctx;
            if (hashMapIteratorGetNext(iterator, (void **) &class, &classLen, (void **) &ctx))
            {
                icLogDebug(LOG_TAG, "deviceServiceDiscoverStop: sending stop signal for device class %s", class);
                pthread_cond_signal(&ctx->cond);
            }
        }
        hashMapIteratorDestroy(iterator);
    }
    else
    {
        icLinkedListIterator *iterator = linkedListIteratorCreate(deviceClasses);
        while (linkedListIteratorHasNext(iterator))
        {
            char *item = (char *) linkedListIteratorGetNext(iterator);

            icLogDebug(LOG_TAG, "deviceServiceDiscoverStop: stopping discovery for device class %s", item);
            discoverDeviceClassContext *ctx = (discoverDeviceClassContext *) hashMapGet(activeDiscoveries,
                                                                                        item,
                                                                                        (uint16_t) (strlen(item) + 1));
            if (ctx != NULL)
            {
                pthread_cond_signal(&ctx->cond);
            }
        }
        linkedListIteratorDestroy(iterator);

    }

    pthread_mutex_unlock(&discoveryControlMutex);

    // The event will get sent when the discovery actually stops

    return true;
}

bool deviceServiceIsDiscoveryActive()
{
    bool result;
    pthread_mutex_lock(&discoveryControlMutex);
    result = hashMapCount(activeDiscoveries) > 0;
    pthread_mutex_unlock(&discoveryControlMutex);
    return result;
}

bool deviceServiceIsInRecoveryMode()
{
    bool retval = false;
    pthread_mutex_lock(&discoveryControlMutex);
    icHashMapIterator *iterator = hashMapIteratorCreate(activeDiscoveries);
    if (iterator != NULL)
    {
        while (hashMapIteratorHasNext(iterator) == true)
        {
            void *mapKey;
            void *mapVal;
            uint16_t mapKeyLen = 0;
            if (hashMapIteratorGetNext(iterator,&mapKey,&mapKeyLen,&mapVal) == true)
            {
                discoverDeviceClassContext *ddcCtx = (discoverDeviceClassContext *)mapVal;
                if (ddcCtx != NULL)
                {
                    if (ddcCtx->findOrphanedDevices == true)
                    {
                        retval = true;
                        break;
                    }
                }
            }
        }
        hashMapIteratorDestroy(iterator);
    }
    pthread_mutex_unlock(&discoveryControlMutex);
    return retval;
}

bool deviceServiceRemoveDevice(const char *uuid)
{
    bool result = false;

    icDevice *device = jsonDatabaseGetDeviceById(uuid);

    if (device != NULL)
    {
        if (jsonDatabaseRemoveDeviceById(uuid) == false)
        {
            icLogError(LOG_TAG, "Failed to remove device %s", uuid);
            deviceDestroy(device);
            return false;
        }

        icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(iterator))
        {
            icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);
            if (endpoint != NULL && endpoint->enabled == true)
            {
                sendEndpointRemovedEvent(endpoint, device->deviceClass);
            }
        }
        linkedListIteratorDestroy(iterator);

        DeviceDriver *driver = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
        if (driver != NULL && driver->deviceRemoved != NULL)
        {
            driver->deviceRemoved(driver->callbackContext, device);
        }

        sendDeviceRemovedEvent(device->uuid, device->deviceClass);

        deviceDestroy(device);

        result = true;
    }
    else if (uuid != NULL)
    {
        //It may currently be being added. Add the uuid to the toRemove list
        icLogDebug(LOG_TAG, "Device %s not created yet, marking for removal", uuid);
        pthread_mutex_lock(&markedForRemovalMutex);
        char *key = strdup(uuid);
        uint16_t keyLen = (uint16_t) strlen(key) + 1;
        int *val = calloc(1, sizeof(int));   //Not used
        hashMapPut(markedForRemoval, key, keyLen, val);
        pthread_mutex_unlock(&markedForRemovalMutex);
        result = true;
    }

    return result;
}

icDeviceResource *deviceServiceGetResourceByUri(const char *uri)
{
    icDeviceResource *result = NULL;

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "deviceServiceGetResourceByUri: invalid arguments");
        return NULL;
    }

    result = jsonDatabaseGetResourceByUri(uri);
    if (result == NULL)
    {
        // if this uri was for an endpoint resource, try again on the root device to support a sort of inheritance
        icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointByUri(uri);
        if (endpoint != NULL)
        {
            icDevice *device = jsonDatabaseGetDeviceById(endpoint->deviceUuid);
            if (device != NULL)
            {
                char *subUri = (char *) (uri + strlen(endpoint->uri));
                char *altUri = (char *) calloc(1, strlen(device->uri) + strlen(subUri) + 1);
                sprintf(altUri, "%s%s", device->uri, subUri);
                result = jsonDatabaseGetResourceByUri(altUri);
                free(altUri);
                deviceDestroy(device);
            }

            endpointDestroy(endpoint);
        }
    }

    if (result == NULL)
    {
        icLogError(LOG_TAG, "Could not find resource for URI %s", uri);
        return NULL;
    }

    // go to the driver if we are not supposed to cache it and it is readable.  If it is always
    // cached, then the resource should always have the value.  This means resources should not
    // be created with NULL values, unless the driver is going to populate it on its own later
    if (result->cachingPolicy == CACHING_POLICY_NEVER && result->mode & RESOURCE_MODE_READABLE)
    {
        //gotta go to the device driver
        DeviceDriver *driver = getDeviceDriverForUri(uri);

        if (driver == NULL)
        {
            icLogError(LOG_TAG, "Could not find device driver for URI %s", uri);
            resourceDestroy(result);
            return NULL;
        }

        char *value = NULL;
        if (driver->readResource(driver->callbackContext, result, &value))
        {
            updateResource(result->deviceUuid, result->endpointId, result->id, value, NULL);
            if (result->value != NULL)
            {
                //free the old value before updating with new
                free(result->value);
            }

            result->value = value;
        }
        else
        {
            //the read failed... dont send stale data back to caller
            resourceDestroy(result);
            result = NULL;
        }
    }

    return result;
}

static bool isUriPattern(const char *uri)
{
    // Right now we only support using * as wildcard
    return strchr(uri, '*') != NULL;
}

static char *createRegexFromPattern(const char *uri)
{
    // Figure out how big the regex will be
    int wildcardChars = 0;
    int i = 0;
    for (; uri[i]; ++i)
    {
        if (uri[i] == '*')
        {
            ++wildcardChars;
        }
    }

    // Build the regex, essentially just replacing * with .*
    char *regexUri = (char *) malloc(i + wildcardChars + 1);
    int j = 0;
    for (i = 0; uri[i]; ++i)
    {
        if (uri[i] == '*')
        {
            regexUri[j++] = '.';
        }
        regexUri[j++] = uri[i];
    }

    regexUri[j] = '\0';

    return regexUri;
}

static bool deviceServiceWriteResourceNoPattern(const char *uri, const char *value)
{
    bool result = false;

    icDeviceResource *resource = jsonDatabaseGetResourceByUri(uri);
    if (resource == NULL)
    {
        // if this uri was for an endpoint resource, try again on the root device to support a sort of inheritance
        icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointByUri(uri);
        if (endpoint != NULL)
        {
            icDevice *device = jsonDatabaseGetDeviceById(endpoint->deviceUuid);
            if (device != NULL)
            {
                char *subUri = (char *) (uri + strlen(endpoint->uri));
                char *altUri = (char *) calloc(1, strlen(device->uri) + strlen(subUri) + 1);
                sprintf(altUri, "%s%s", device->uri, subUri);
                resource = jsonDatabaseGetResourceByUri(altUri);
                free(altUri);
                deviceDestroy(device);
            }

            endpointDestroy(endpoint);
        }
    }

    if (resource == NULL)
    {
        icLogError(LOG_TAG, "Could not find resource for URI %s", uri);
        return false;
    }

    if ((resource->mode & RESOURCE_MODE_WRITEABLE) == 0)
    {
        icLogError(LOG_TAG, "Attempt to alter a non-writable resource (%s) rejected.", uri);
        resourceDestroy(resource);
        return false;
    }

    DeviceDriver *driver = getDeviceDriverForUri(uri);
    if (driver == NULL)
    {
        icLogError(LOG_TAG, "Could not find device driver for URI %s", uri);
    }
    else
    {
        //The device driver's contract states that they will call us back at updateResource if successful
        // we save the change there
        result = driver->writeResource(driver->callbackContext, resource, resource->value, value);
    }

    resourceDestroy(resource);

    return result;
}

bool deviceServiceWriteResource(const char *uri, const char *value)
{
    bool result = false;

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "deviceServiceWriteResource: invalid arguments");
        return false;
    }

    // Check if we are dealing with a pattern
    if (isUriPattern(uri))
    {
        char *regex = createRegexFromPattern(uri);
        icLinkedList *resources = jsonDatabaseGetResourcesByUriRegex(regex);
        // Cleanup
        free(regex);
        if (resources != NULL)
        {
            result = linkedListCount(resources) > 0;
            icLinkedListIterator *iter = linkedListIteratorCreate(resources);
            while (linkedListIteratorHasNext(iter))
            {
                icDeviceResource *item = (icDeviceResource *) linkedListIteratorGetNext(iter);
                // Do the write for this resource
                result = result && deviceServiceWriteResourceNoPattern(item->uri, value);
            }
            linkedListIteratorDestroy(iter);

            // Cleanup
            linkedListDestroy(resources, (linkedListItemFreeFunc) resourceDestroy);
        }
        else
        {
            icLogError(LOG_TAG, "Could not find resources for URI %s", uri);
        }
    }
    else
    {
        result = deviceServiceWriteResourceNoPattern(uri, value);
    }

    return result;
}

bool deviceServiceExecuteResource(const char *uri, const char *arg, char **response)
{
    bool result = false;

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "deviceServiceExecuteResource: invalid arguments");
        return false;
    }

    icDeviceResource *resource = jsonDatabaseGetResourceByUri(uri);
    if (resource == NULL)
    {
        // if this uri was for an endpoint resource, try again on the root device to support a sort of inheritance
        icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointByUri(uri);
        if (endpoint != NULL)
        {
            icDevice *device = jsonDatabaseGetDeviceById(endpoint->deviceUuid);
            if (device != NULL)
            {
                char *subUri = (char *) (uri + strlen(endpoint->uri));
                char *altUri = (char *) calloc(1, strlen(device->uri) + strlen(subUri) + 1);
                sprintf(altUri, "%s%s", device->uri, subUri);
                resource = jsonDatabaseGetResourceByUri(altUri);
                free(altUri);
                deviceDestroy(device);
            }

            endpointDestroy(endpoint);
        }
    }

    if (resource == NULL)
    {
        icLogError(LOG_TAG, "Could not find resource for URI %s", uri);
        return false;
    }

    if ((resource->mode & RESOURCE_MODE_EXECUTABLE) == 0)
    {
        icLogError(LOG_TAG, "Attempt to execute a non-executable resource (%s) rejected.", uri);
        resourceDestroy(resource);
        return false;
    }

    DeviceDriver *driver = getDeviceDriverForUri(uri);
    if (driver == NULL)
    {
        icLogError(LOG_TAG, "Could not find device driver for URI %s", uri);
    }
    else
    {
        if (driver->executeResource != NULL)
        {
            result = driver->executeResource(driver->callbackContext, resource, arg, response);
        }
    }

    resourceDestroy(resource);

    return result;
}

bool deviceServiceChangeResourceMode(const char *uri, uint8_t newMode)
{
    icLogDebug(LOG_TAG, "%s: uri=%s, newMode=%"PRIx8, __FUNCTION__, uri, newMode);

    if (uri == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return false;
    }

    icDeviceResource *resource = jsonDatabaseGetResourceByUri(uri);
    if (resource == NULL)
    {
        // if this uri was for an endpoint resource, try again on the root device to support a sort of inheritance
        icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointByUri(uri);
        if (endpoint != NULL)
        {
            icDevice *device = jsonDatabaseGetDeviceById(endpoint->deviceUuid);
            if (device != NULL)
            {
                char *subUri = (char *) (uri + strlen(endpoint->uri));
                char *altUri = (char *) calloc(1, strlen(device->uri) + strlen(subUri) + 1);
                sprintf(altUri, "%s%s", device->uri, subUri);
                resource = jsonDatabaseGetResourceByUri(altUri);
                free(altUri);
                deviceDestroy(device);
            }

            endpointDestroy(endpoint);
        }
    }

    if (resource == NULL)
    {
        icLogError(LOG_TAG, "Could not find resource for URI %s", uri);
        return false;
    }

    //we do not allow removing the sensitive bit
    if ((resource->mode & RESOURCE_MODE_SENSITIVE) && !(newMode & RESOURCE_MODE_SENSITIVE))
    {
        icLogWarn(LOG_TAG, "%s: attempt to remove sensitive mode rejected", __FUNCTION__);
        resource->mode = (newMode | RESOURCE_MODE_SENSITIVE);
    }
    else
    {
        resource->mode = newMode;
    }

    jsonDatabaseSaveResource(resource);

    resourceDestroy(resource);

    return true;
}

icDeviceEndpoint *deviceServiceGetEndpoint(const char *deviceUuid, const char *endpointId)
{
    icDeviceEndpoint *result = NULL;

    if (deviceUuid != NULL && endpointId != NULL)
    {
        result = jsonDatabaseGetEndpointById(deviceUuid, endpointId);

        if (result != NULL && result->enabled == false)
        {
            //we cant return this endpoint...
            endpointDestroy(result);
            result = NULL;
        }
    }

    return result;
}

bool deviceServiceRemoveEndpointById(const char *deviceUuid, const char *endpointId)
{
    icLogDebug(LOG_TAG, "%s: deviceUuid=%s, endpointId=%s", __FUNCTION__, deviceUuid, endpointId);

    bool result = false;
    AUTO_CLEAN(deviceDestroy__auto) icDevice *device = jsonDatabaseGetDeviceById(deviceUuid);
    AUTO_CLEAN(endpointDestroy__auto) icDeviceEndpoint *endpoint = jsonDatabaseGetEndpointById(deviceUuid, endpointId);

    if (endpoint == NULL || device == NULL)
    {
        icLogError(LOG_TAG,
                   "deviceServiceRemoveEndpointById: device/endpoint not found for deviceId %s and endpointId %s",
                   deviceUuid,
                   endpointId);
        return false;
    }

    if (endpoint->enabled == true)
    {
        endpoint->enabled = false;

        //go ahead and save the change to this endpoint now even though we might drop the entire device below
        jsonDatabaseSaveEndpoint(endpoint);
        backupRestoreService_request_CONFIG_UPDATED();

        sendEndpointRemovedEvent(endpoint, device->deviceClass);

        //check to see if we have any enabled endpoints left and remove the whole device if not
        bool atLeastOneActiveEndpoint = false;

        icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
        while (linkedListIteratorHasNext(iterator))
        {
            icDeviceEndpoint *ep = linkedListIteratorGetNext(iterator);
            if (ep->enabled == true && strcmp(ep->id, endpoint->id) != 0) //dont count the one we just disabled
            {
                atLeastOneActiveEndpoint = true;
                break;
            }
        }
        linkedListIteratorDestroy(iterator);

        if (atLeastOneActiveEndpoint == false)
        {
            icLogInfo(LOG_TAG, "No more enabled endpoints exist on this device (%s), removing the whole thing.",
                      endpoint->deviceUuid);
            deviceServiceRemoveDevice(endpoint->deviceUuid);
            backupRestoreService_request_CONFIG_UPDATED();
        }
        else
        {
            // Let the driver know in case it wants to react
            DeviceDriver *driver = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
            if (driver != NULL && driver->endpointDisabled != NULL)
            {
                driver->endpointDisabled(driver->callbackContext, endpoint);
            }
        }


        result = true;
    }

    return result;
}

/*
 * Get a device descriptor for a device
 * @param device the device to retrieve the descriptor for
 * @return the device descriptor or NULL if its not found.  Caller is responsible for calling deviceDescriptorFree()
 */
DeviceDescriptor *deviceServiceGetDeviceDescriptorForDevice(icDevice *device)
{
    DeviceDescriptor *dd = NULL;
    icDeviceResource *manufacturer = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_MANUFACTURER);
    icDeviceResource *model = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_MODEL);
    icDeviceResource *hardwareVersion = deviceServiceFindDeviceResourceById(device,
                                                                            COMMON_DEVICE_RESOURCE_HARDWARE_VERSION);
    icDeviceResource *firmwareVersion = deviceServiceFindDeviceResourceById(device,
                                                                            COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    if (manufacturer != NULL && model != NULL && firmwareVersion != NULL && hardwareVersion != NULL)
    {
        dd = deviceDescriptorsGet(manufacturer->value, model->value, hardwareVersion->value, firmwareVersion->value);
    }

    return dd;
}

void deviceServiceProcessDeviceDescriptors()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    // presume that we are starting out, or just downloaded a new set of device descriptors
    // need to clear out what we had before so that we're forced to re-parse them
    //
    deviceDescriptorsCleanup();

    //Dont let the device driver manager shutdown while we are processing descriptors
    pthread_mutex_lock(&deviceDriverManagerShutdownMtx);

    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(iterator);
        DeviceDriver *driver = getDeviceDriverForUri(device->uri);
        if (driver == NULL)
        {
            icLogError(LOG_TAG,
                       "deviceServiceProcessDeviceDescriptors: could not find device driver for %s",
                       device->uuid);
            continue;
        }

        if (driver->processDeviceDescriptor == NULL)
        {
            //this driver doesnt support reprocessing device descriptors
            continue;
        }

        DeviceDescriptor *dd = deviceServiceGetDeviceDescriptorForDevice(device);
        if (dd != NULL)
        {
            // forward this descriptor to the device
            driver->processDeviceDescriptor(driver->callbackContext,
                                            device,
                                            dd);

            deviceDescriptorFree(dd);
        }
    }

    pthread_mutex_unlock(&deviceDriverManagerShutdownMtx);

    // cleanup
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

/****************** LOW POWER MODE FUNCTIONS ***********************/

/*
 * Tells all of the subsystem to enter LPM
 */
void deviceServiceEnterLowPowerMode()
{
    pthread_mutex_lock(&lowPowerModeMutex);

    // since we are already in LPM just bail,
    // nothing to do
    //
    if (isDeviceServiceInLPM == true)
    {
        pthread_mutex_unlock(&lowPowerModeMutex);
        return;
    }

    // now set our LPM flag to true
    //
    isDeviceServiceInLPM = true;
    pthread_mutex_unlock(&lowPowerModeMutex);

    // tell the subsystems to enter LPM
    //
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    zigbeeSubsystemEnterLPM();
#endif

    deviceServiceNotifySystemPowerEvent(POWER_EVENT_LPM_ENTER);

    // TODO: any other subsystem to notify
}

/*
 * Tells all of the subsystem to exit LPM
 */
void deviceServiceExitLowPowerMode()
{
    pthread_mutex_lock(&lowPowerModeMutex);

    // since we are already not in LPM just bail,
    // nothing to do
    //
    if (isDeviceServiceInLPM == false)
    {
        pthread_mutex_unlock(&lowPowerModeMutex);
        return;
    }

    // now set our LPM flag to false
    //
    isDeviceServiceInLPM = false;
    pthread_mutex_unlock(&lowPowerModeMutex);

    // tell the subsystems to exit LPM
    //
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    zigbeeSubsystemExitLPM();
#endif

    deviceServiceNotifySystemPowerEvent(POWER_EVENT_LPM_EXIT);

    // TODO: any other subsystem to notify
}

/*
 * Determines if system is in LPM
 */
bool deviceServiceIsInLowPowerMode()
{
    pthread_mutex_lock(&lowPowerModeMutex);
    bool retVal = isDeviceServiceInLPM;
    pthread_mutex_unlock(&lowPowerModeMutex);

    return retVal;
}

char *getCurrentTimestampString()
{
    return stringBuilder("%" PRIu64, getCurrentUnixTimeMillis());
}

void updateDeviceDateLastContacted(const char *deviceUuid)
{
    char *dateBuf = getCurrentTimestampString();

    updateResource(deviceUuid,
                   NULL,
                   COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED,
                   dateBuf,
                   NULL);

    free(dateBuf);
}

uint64_t getDeviceDateLastContacted(const char *deviceUuid)
{
    uint64_t result = 0;

    icDeviceResource *resource = deviceServiceGetResourceById(deviceUuid,
                                                              NULL,
                                                              COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED);

    if (resource != NULL)
    {
        result = strtoull(resource->value, NULL, 10);
    }

    return result;
}

/****************** PRIVATE INTERNAL FUNCTIONS ***********************/

static void deviceCommFailCallback(const char *uuid)
{
    icDevice *device = deviceServiceGetDevice(uuid);
    if (device != NULL)
    {
        DeviceDriver *driver = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
        if (driver != NULL)
        {
            driver->communicationFailed(driver->callbackContext, device);

            // Notify the subsystem that they had a device go into comm fail.
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
            if (stringCompare(driver->subsystemName, ZIGBEE_SUBSYSTEM_NAME, true) == 0)
            {
                zigbeeSubsystemNotifyDeviceCommFail(device);
            }
#endif
        }

        deviceDestroy(device);
    }
}

static void deviceCommRestoreCallback(const char *uuid)
{
    icDevice *device = deviceServiceGetDevice(uuid);
    if (device != NULL)
    {
        DeviceDriver *driver = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
        if (driver != NULL)
        {
            driver->communicationRestored(driver->callbackContext, device);
        }

        deviceDestroy(device);

        updateDeviceDateLastContacted(uuid);
    }
}

/*
 * step 2 of the startup sequence:
 * optional callback notification that occurs when
 * all services are initialized and ready for use.
 * this is triggered by the WATCHDOG_INIT_COMPLETE
 * event.
 */
static void allServicesAvailableNotify(void)
{
    icLogDebug(LOG_TAG, "got watchdog event that all services are running");

    // Initialize our device descriptor handling
    deviceServiceDeviceDescriptorsInit(deviceDescriptorsReadyForDevicesCallback, descriptorsUpdatedCallback);

    subsystemManagerAllServicesAvailable();
}

bool deviceServiceInitialize(bool block)
{
    // initialize logging
    //
    initIcLogger();

    startDeviceEventProducer();

    deviceCommunicationWatchdogInit(deviceCommFailCallback, deviceCommRestoreCallback);

    if (deviceServiceConfigDir == NULL)
    {
        deviceServiceConfigDir = getDynamicConfigPath();
    }

    if (deviceServiceConfigDir != NULL)
    {
        char *whitelistPath = NULL;
        char *blacklistPath = NULL;

        whitelistPath = (char *) calloc(1, strlen(deviceServiceConfigDir) + strlen("/WhiteList.xml") + 1);
        if (whitelistPath == NULL)
        {
            icLogError(LOG_TAG, "Failed to allocate whitelistPath");
            return false;
        }
        sprintf(whitelistPath, "%s/WhiteList.xml", deviceServiceConfigDir);

        blacklistPath = (char *) calloc(1, strlen(deviceServiceConfigDir) + strlen("/BlackList.xml") + 1);
        if (blacklistPath == NULL)
        {
            icLogError(LOG_TAG, "Failed to allocate blacklistPath");
            free(whitelistPath);
            return false;
        }
        sprintf(blacklistPath, "%s/BlackList.xml", deviceServiceConfigDir);

        deviceDescriptorsInit(whitelistPath, blacklistPath);

        if (jsonDatabaseInitialize() != true)
        {
            icLogError(LOG_TAG, "Failed to initialize database.");
            free(whitelistPath);
            free(blacklistPath);
            return false;
        }

        free(whitelistPath);
        free(blacklistPath);
    }

    activeDiscoveries = hashMapCreate();
    markedForRemoval = hashMapCreate();

    if (deviceDriverManagerInitialize() != true)
    {
        return false;
    }

    // adjust our log level to the "logging.deviceService" property
    //
    autoAdjustCustomLogLevel(DEVICE_SERVICE_NAME);
    autoAdjustTimezone();

    // start event receiver
    //
    deviceServiceEventHandlerInit();

    deviceInitializerThreadPool = threadPoolCreate("DevInit", 0, MAX_DEVICE_SYNC_THREADS, MAX_DEVICE_SYNC_QUEUE);

    subsystemManagerInitialize(getSystemCpeId(), subsystemManagerInitializedCallback, subsystemManagerReadyForDevicesCallback);

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    // start event tracker
    //
    initEventTracker();
#endif

    deviceDriverManagerStartDeviceDrivers();

    subsystemManagerAllDriversStarted();

    // begin the 'service startup sequence', and potentially block until the IPC receiver exits
    //
    startupService_deviceService(NULL, allServicesAvailableNotify, deviceServiceShutdown, 3, IPC_DEFAULT_MAX_THREADS * 2, 50, block);
    return true;
}

void deviceServiceShutdown()
{
    icLogDebug(LOG_TAG, "%s: shutting down", __FUNCTION__);

    // cleanup
    //

    deviceServiceEventHandlerShutdown();

    deviceCommunicationWatchdogTerm();

    icLogInfo(LOG_TAG, "%s: stopping device synchronization", __FUNCTION__);
    threadPoolDestroy(deviceInitializerThreadPool);
    deviceInitializerThreadPool = NULL;

    if (deviceServiceConfigDir != NULL)
    {
        free(deviceServiceConfigDir);
        deviceServiceConfigDir = NULL;
    }

    disableAutoAdjustTimezone();

    pthread_mutex_lock(&deviceDescriptorProcessorMtx);
    if (deviceDescriptorProcessorTask != 0)
    {
        cancelDelayTask(deviceDescriptorProcessorTask);
        deviceDescriptorProcessorTask = 0;
    }
    pthread_mutex_unlock(&deviceDescriptorProcessorMtx);

    deviceServiceDeviceDescriptorsDestroy();

    subsystemManagerShutdown();

    shutdownDeviceDriverManager();
    stopDeviceEventProducer();

#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    shutDownEventTracker();
#endif

    jsonDatabaseCleanup(true);
    closeIcLogger();

    hashMapDestroy(activeDiscoveries, standardDoNotFreeHashMapFunc);
    hashMapDestroy(markedForRemoval, NULL);

    icLogDebug(LOG_TAG, "%s: shutdown complete", __FUNCTION__);
}

static bool fetchCommonResourcesInitialValues(icDevice *device,
                                              const char *manufacturer,
                                              const char *model,
                                              const char *hardwareVersion,
                                              const char *firmwareVersion,
                                              icInitialResourceValues *initialResourceValues)
{
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_MANUFACTURER, manufacturer);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_MODEL, model);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION, hardwareVersion);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, firmwareVersion);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS, NULL);

    AUTO_CLEAN(free_generic__auto) const char *dateBuf = getCurrentTimestampString();

    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_DATE_ADDED, dateBuf);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, dateBuf);
    initialResourceValuesPutDeviceValue(initialResourceValues, COMMON_DEVICE_RESOURCE_COMM_FAIL, "false");

    return true;
}

static bool addCommonResources(icDevice *device,
                               icInitialResourceValues *initialResourceValues)
{
    bool ok = createDeviceResourceIfAvailable(device,
                                              COMMON_DEVICE_RESOURCE_MANUFACTURER,
                                              initialResourceValues,
                                              RESOURCE_TYPE_STRING,
                                              RESOURCE_MODE_READABLE,
                                              CACHING_POLICY_ALWAYS) != NULL;

    ok &= createDeviceResourceIfAvailable(device,
                                          COMMON_DEVICE_RESOURCE_MODEL,
                                          initialResourceValues,
                                          RESOURCE_TYPE_STRING,
                                          RESOURCE_MODE_READABLE,
                                          CACHING_POLICY_ALWAYS) != NULL;

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_HARDWARE_VERSION,
                                           initialResourceValues,
                                           RESOURCE_TYPE_VERSION,
                                           RESOURCE_MODE_READABLE,
                                           CACHING_POLICY_ALWAYS) != NULL);

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION,
                                           initialResourceValues,
                                           RESOURCE_TYPE_VERSION,
                                           RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                           CACHING_POLICY_ALWAYS) != NULL); //the device driver will update after firmware upgrade

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_FIRMWARE_UPDATE_STATUS,
                                           initialResourceValues,
                                           RESOURCE_TYPE_FIRMWARE_VERSION_STATUS,
                                           RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                           CACHING_POLICY_ALWAYS) != NULL); //the device driver will update as it detects status about any updates

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_DATE_ADDED,
                                           initialResourceValues,
                                           RESOURCE_TYPE_DATETIME,
                                           RESOURCE_MODE_READABLE,
                                           CACHING_POLICY_ALWAYS) != NULL);

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED,
                                           initialResourceValues,
                                           RESOURCE_TYPE_DATETIME,
                                           RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_LAZY_SAVE_NEXT,
                                           CACHING_POLICY_ALWAYS) != NULL);

    ok &= (createDeviceResourceIfAvailable(device,
                                           COMMON_DEVICE_RESOURCE_COMM_FAIL,
                                           initialResourceValues,
                                           RESOURCE_TYPE_TROUBLE,
                                           RESOURCE_MODE_READABLE | RESOURCE_MODE_DYNAMIC | RESOURCE_MODE_EMIT_EVENTS,
                                           CACHING_POLICY_ALWAYS) != NULL);

    ok &= (createDeviceResource(device,
                               COMMON_DEVICE_FUNCTION_RESET_TO_FACTORY,
                               NULL,
                               RESOURCE_TYPE_RESET_TO_FACTORY_OPERATION,
                               RESOURCE_MODE_EXECUTABLE,
                               CACHING_POLICY_NEVER) != NULL);

    return ok;
}

/*
 * check the system properties to see if device descriptors are bypassed
 */
static bool isDeviceDescriptorBypassed()
{
    bool retVal = false;

    // get from 'system properties'
    char *flag = NULL;
    if (deviceServiceGetSystemProperty(DEVICE_DESCRIPTOR_BYPASS_SYSTEM_PROP, &flag) == true)
    {
        if (flag != NULL && strcasecmp(flag, "true") == 0)
        {
            retVal = true;
        }
        free(flag);
    }

    return retVal;
}

/*
 * Callback invoked when a device driver finds a device.
 */
bool deviceServiceDeviceFound(DeviceFoundDetails *deviceFoundDetails, bool neverReject)
{
    icLogDebug(LOG_TAG,
               "%s: deviceClass=%s, deviceClassVersion=%u, uuid=%s, manufacturer=%s, model=%s, hardwareVersion=%s, firmwareVersion=%s",
               __FUNCTION__,
               deviceFoundDetails->deviceClass,
               deviceFoundDetails->deviceClassVersion,
               deviceFoundDetails->deviceUuid,
               deviceFoundDetails->manufacturer,
               deviceFoundDetails->model,
               deviceFoundDetails->hardwareVersion,
               deviceFoundDetails->firmwareVersion);

    if (deviceServiceIsDeviceBlacklisted(deviceFoundDetails->deviceUuid) == true)
    {
        icLogWarn(LOG_TAG, "%s: This device's UUID is blacklisted!  Rejecting device %s.", __FUNCTION__,
                  deviceFoundDetails->deviceUuid);
        sendDeviceRejectedEvent(deviceFoundDetails);

        // tell the device driver that we have rejected this device so it can do any cleanup
        return false;
    }

    bool allowPairing = true;

    //A device has been found.  We now check to see if we will keep or reject it.  If we find a matching device
    // descriptor, then we keep it.  If we dont find one we will keep it anyway if it is either the XBB battery
    // backup special device or if device descriptor bypass is enabled (which is used for testing/developement).

    //Attempt to locate the discovered device's descriptor
    AUTO_CLEAN(deviceDescriptorFree__auto) DeviceDescriptor *dd = deviceDescriptorsGet(deviceFoundDetails->manufacturer,
                                                                                       deviceFoundDetails->model,
                                                                                       deviceFoundDetails->hardwareVersion,
                                                                                       deviceFoundDetails->firmwareVersion);

    if (dd == NULL)
    {
        if (neverReject)
        {
            icLogDebug(LOG_TAG, "%s: device added with 'neverReject'; allowing device to be paired", __FUNCTION__);
        }
        else if (deviceFoundDetails->deviceMigrator != NULL)
        {
            icLogDebug(LOG_TAG, "%s: device added for migration; allowing device to be paired", __FUNCTION__);
        }
        else if (isDeviceDescriptorBypassed()) // see if the device descriptors are bypassed
        {
            // bypassed, so proceed with the pairing/configuring
            icLogDebug(LOG_TAG, "%s:deviceDescriptorBypass is SET; allowing device to be paired", __FUNCTION__);
        }
        else
        {
            //no device descriptor, no bypass, and not an XBB.  Dont allow pairing
            allowPairing = false;
        }
    }

    if (!allowPairing)
    {
        icLogWarn(LOG_TAG, "%s: No device descriptor found!  Rejecting device %s.", __FUNCTION__,
                  deviceFoundDetails->deviceUuid);
        sendDeviceRejectedEvent(deviceFoundDetails);

        // tell the device driver that we have rejected this device so it can do any cleanup
        return false;
    }

    bool pairingSuccessful = true;

    //Create a device instance populated with all required items from the base device class specification
    AUTO_CLEAN(deviceDestroy__auto) icDevice *device = createDevice(deviceFoundDetails->deviceUuid,
                                                                    deviceFoundDetails->deviceClass,
                                                                    deviceFoundDetails->deviceClassVersion,
                                                                    deviceFoundDetails->deviceDriver->driverName,
                                                                    dd);

    ConfigureDeviceFunc configureFunc;
    FetchInitialResourceValuesFunc fetchValuesFunc;
    RegisterResourcesFunc registerFunc;
    DevicePersistedFunc persistedFunc;
    void *ctx;

    bool sendEvents = true;
    if (deviceFoundDetails->deviceMigrator == NULL)
    {
        sendDeviceDiscoveredEvent(deviceFoundDetails);

        configureFunc = deviceFoundDetails->deviceDriver->configureDevice;
        fetchValuesFunc = deviceFoundDetails->deviceDriver->fetchInitialResourceValues;
        registerFunc = deviceFoundDetails->deviceDriver->registerResources;
        persistedFunc = deviceFoundDetails->deviceDriver->devicePersisted;
        ctx = deviceFoundDetails->deviceDriver->callbackContext;
    }
    else
    {
        configureFunc = deviceFoundDetails->deviceMigrator->configureDevice;
        fetchValuesFunc = deviceFoundDetails->deviceMigrator->fetchInitialResourceValues;
        registerFunc = deviceFoundDetails->deviceMigrator->registerResources;
        persistedFunc = deviceFoundDetails->deviceMigrator->devicePersisted;
        ctx = deviceFoundDetails->deviceMigrator->callbackContext;
        deviceFoundDetails->deviceMigrator->callbackContext->deviceDriver = deviceFoundDetails->deviceDriver;
        sendEvents = false;
    }

    if (sendEvents == true)
    {
        sendDeviceConfigureStartedEvent(deviceFoundDetails->deviceClass, deviceFoundDetails->deviceUuid);
    }

    // here the device descriptor is used for initial configuration, not necessarily full and normal handling
    if (configureFunc(ctx,
                      device,
                      dd) == false)
    {
        //Note, parts of the deviceFoundDetails may have be freed by this point. For instance, for cameras, some of the
        // details point to the cameraDevice used in configuration above. If configuration fails, that cameraDevice is
        // cleaned up. So by this point, some of deviceFoundDetails (such as uuid) would have been freed already.
        icLogWarn(LOG_TAG, "%s: %s failed to configure.", __FUNCTION__, device->uuid);

        if (sendEvents == true)
        {
            sendDeviceConfigureFailedEvent(deviceFoundDetails->deviceClass, deviceFoundDetails->deviceUuid);
        }
        pairingSuccessful = false;
    }
    else
    {
        if (sendEvents == true)
        {
            sendDeviceConfigureCompletedEvent(deviceFoundDetails->deviceClass, deviceFoundDetails->deviceUuid);
        }
        icInitialResourceValues *initialValues = initialResourceValuesCreate();

        pairingSuccessful = fetchCommonResourcesInitialValues(device,
                                                              deviceFoundDetails->manufacturer,
                                                              deviceFoundDetails->model,
                                                              deviceFoundDetails->hardwareVersion,
                                                              deviceFoundDetails->firmwareVersion,
                                                              initialValues);

        if (pairingSuccessful)
        {
            // populate initial resource values
            pairingSuccessful &= fetchValuesFunc(
                    ctx,
                    device,
                    initialValues);
        }
        else
        {
            icLogError(LOG_TAG, "%s: %s failed to fetch common resource values", __FUNCTION__, device->uuid);
        }

        if (pairingSuccessful)
        {
            initialResourcesValuesLogValues(initialValues);

            pairingSuccessful = addCommonResources(device,
                                                   initialValues);
        }
        else
        {
            icLogError(LOG_TAG, "%s: %s failed to fetch initial resource values", __FUNCTION__, device->uuid);
        }

        if (pairingSuccessful)
        {
            //add driver specific resources
            pairingSuccessful &= registerFunc(
                    ctx,
                    device,
                    initialValues);
        }
        else
        {
            icLogError(LOG_TAG, "%s: %s failed to register common resources", __FUNCTION__, device->uuid);
        }

        initialResourceValuesDestroy(initialValues);

        // after everything is all ready, let regular device descriptor processing take place
        if (pairingSuccessful)
        {
            if (deviceFoundDetails->deviceDriver->processDeviceDescriptor != NULL && dd != NULL)
            {
                pairingSuccessful &= deviceFoundDetails->deviceDriver->processDeviceDescriptor(
                        deviceFoundDetails->deviceDriver->callbackContext,
                        device,
                        dd);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: %s failed to register resources", __FUNCTION__, device->uuid);
        }
    }

    if (pairingSuccessful)
    {
        //Before we send the discovery complete event, let's do a final check to see if this device has been marked for
        // removal
        pthread_mutex_lock(&markedForRemovalMutex);
        bool marked = hashMapDelete(markedForRemoval, device->uuid, (uint16_t) strlen(device->uuid) + 1, NULL);
        pthread_mutex_unlock(&markedForRemovalMutex);

        if (marked == true)
        {
            icLogDebug(LOG_TAG, "Device marked for removal before finishing setup. Not adding...");
            pairingSuccessful = false;
        }
    }

    char *deviceClass = (char * )deviceFoundDetails->deviceClass;

    bool inRepairMode = false;
    discoverDeviceClassContext *ddcCtx = (discoverDeviceClassContext *)hashMapGet(activeDiscoveries, deviceClass, (uint16_t) (strlen(deviceClass) + 1));
    if (ddcCtx != NULL)
    {
        inRepairMode = ddcCtx->findOrphanedDevices;
    }
    if (pairingSuccessful)
    {
        //perform any processing to make this device real and accessible now that the device driver is done
        pairingSuccessful = finalizeNewDevice(device, sendEvents, inRepairMode);
    }

    //Finally, if we made it here and are still successful, let everyone know.
    if (pairingSuccessful)
    {
        if (sendEvents)
        {
            // Signal that we finished discovering the device including all its endpoints
            sendDeviceDiscoveryCompletedEvent(device);
        }

        if (persistedFunc != NULL)
        {
            persistedFunc(ctx, device);
        }
    }
    else
    {
        //We need to make sure the managing driver is told to remove any persistent models of the device that they
        //may have (for instance, cameras). Otherwise, they may never be made accessible again in the current process
        //if the driver thinks it already has discovered the device.
        if(deviceFoundDetails->deviceMigrator == NULL && deviceFoundDetails->deviceDriver->deviceRemoved != NULL)
        {
            deviceFoundDetails->deviceDriver->deviceRemoved(deviceFoundDetails->deviceDriver->callbackContext, device);
        }

        if (sendEvents)
        {
            sendDeviceDiscoveryFailedEvent(device->uuid, deviceFoundDetails->deviceClass);
        }
    }

    return pairingSuccessful;
}

static void setUrisOnResources(const char *baseUri, icLinkedList *resources)
{
    icLinkedListIterator *iterator = linkedListIteratorCreate(resources);
    while (linkedListIteratorHasNext(iterator))
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(iterator);

        // base uri + "/r/" + id + \0
        free(resource->uri);
        resource->uri = (char *) malloc(strlen(baseUri) + strlen("/r/") + strlen(resource->id) + 1 + 1);
        sprintf(resource->uri, "%s/r/%s", baseUri, resource->id);

        icLogDebug(LOG_TAG, "Created URI: %s", resource->uri);
    }
    linkedListIteratorDestroy(iterator);
}

static void setUris(icDevice *device)
{
    //  "/" + uuid + \0
    device->uri = (char *) malloc(1 + strlen(device->uuid) + 1);
    sprintf(device->uri, "/%s", device->uuid);

    icLogDebug(LOG_TAG, "Created URI: %s", device->uri);

    setUrisOnResources(device->uri, device->resources);

    icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
    while (linkedListIteratorHasNext(iterator))
    {
        icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);

        endpoint->uri = getEndpointUri(device->uuid, endpoint->id);

        icLogDebug(LOG_TAG, "Created URI: %s", endpoint->uri);
        setUrisOnResources(endpoint->uri, endpoint->resources);
    }
    linkedListIteratorDestroy(iterator);
}

static bool finalizeNewDevice(icDevice *device, bool sendEvents, bool inRepairMode)
{
    bool result = true;

    if (device == NULL)
    {
        icLogError(LOG_TAG, "deviceConfigured: NULL device!");
        return false;
    }

    //populate URIs on the device's tree
    setUris(device);

    //if this device has the timezone resource, set it now
    icDeviceResource *tzResource = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_TIMEZONE);
    if (tzResource != NULL)
    {
        char *posixTZ = getPropertyAsString(POSIX_TIME_ZONE_PROP, NULL);
        if (posixTZ != NULL)
        {
            deviceServiceWriteResource(tzResource->uri, posixTZ);
            free(posixTZ);
        }
    }

    if (inRepairMode == false)
    {
        result &= jsonDatabaseAddDevice(device);
        backupRestoreService_request_CONFIG_UPDATED();
    }

    icLogDebug(LOG_TAG, "device finalized:");
    devicePrint(device, "");

    if (sendEvents == true)
    {
        if (inRepairMode == false)
        {
            sendDeviceAddedEvent(device->uuid);

            icLinkedListIterator *iterator = linkedListIteratorCreate(device->endpoints);
            while (linkedListIteratorHasNext(iterator))
            {
                icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iterator);
                if (endpoint->enabled == true)
                {
                    sendEndpointAddedEvent(endpoint, device->deviceClass);
                }
            }
            linkedListIteratorDestroy(iterator);
        }
        else
        {
            sendDeviceRecoveredEvent(device->uuid);
        }
    }

    return result;
}

// the endpoint must have already been added to the icDevice
void deviceServiceAddEndpoint(icDevice *device, icDeviceEndpoint *endpoint)
{
    if (device == NULL || endpoint == NULL)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    endpoint->uri = getEndpointUri(device->uuid, endpoint->id);
    icLogDebug(LOG_TAG, "Created URI: %s", endpoint->uri);
    setUrisOnResources(endpoint->uri, endpoint->resources);
    jsonDatabaseAddEndpoint(endpoint);
    backupRestoreService_request_CONFIG_UPDATED();

    if (endpoint->enabled == true)
    {
        sendEndpointAddedEvent(endpoint, device->deviceClass);
    }
}

/**
 * Update an endpoint, persist to database and send out events.
 * @param device the device
 * @param endpoint the endpoint
 * @note Currently allows 'enabled' and 'resources' to change
 * FIXME: allow possibly anything to be changed, i.e., just save a valid endpoint
 */
void deviceServiceUpdateEndpoint(icDevice *device, icDeviceEndpoint *endpoint)
{
    if (device == NULL || endpoint == NULL || strcmp(device->uuid, endpoint->deviceUuid) != 0)
    {
        icLogError(LOG_TAG, "%s: invalid arguments", __FUNCTION__);
        return;
    }

    AUTO_CLEAN(endpointDestroy__auto) icDeviceEndpoint *currentEp = jsonDatabaseGetEndpointByUri(endpoint->uri);
    bool wasEnabled = false;

    if (currentEp != NULL)
    {
        wasEnabled = currentEp->enabled;
    }
    else
    {
        icLogError(LOG_TAG, "Device endpoint %s not found!", endpoint->uri);
        return;
    }

    setUrisOnResources(endpoint->uri, endpoint->resources);
    jsonDatabaseSaveEndpoint(endpoint);
    backupRestoreService_request_CONFIG_UPDATED();

    if (wasEnabled == false && endpoint->enabled == true)
    {
        sendEndpointAddedEvent(endpoint, device->deviceClass);
    }
}

//Used to notify watchers when an resource changes
//Used by device drivers when they need to update the resource for one of their devices
void updateResource(const char *deviceUuid,
                    const char *endpointId,
                    const char *resourceId,
                    const char *newValue,
                    cJSON *metadata)
{
    // dont debug print on frequently updated resource ids to preserve log files
    if(resourceId != NULL &&
       strcmp(COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, resourceId) != 0)
    {
        icLogDebug(LOG_TAG, "%s: deviceUuid=%s, endpointId=%s, resourceId=%s, newValue=%s",
                   __FUNCTION__, deviceUuid, endpointId, resourceId, newValue);
    }

    icDeviceResource *resource = deviceServiceGetResourceByIdInternal(deviceUuid, endpointId, resourceId, false);

    if (resource != NULL)
    {
        bool sendEvent = false;

        if(resource->cachingPolicy == CACHING_POLICY_NEVER && (resource->mode & RESOURCE_MODE_EMIT_EVENTS))
        {
            // we cannot compare previous values for non cached resources, so just stuff what we got in it and
            // send the event
            free(resource->value);
            if(newValue != NULL)
            {
                resource->value = strdup(newValue);
            }
            else
            {
                resource->value = NULL;
            }
            sendEvent = true;
        }
        else
        {
            bool didChange = false;

            if (resource->value != NULL)
            {
                if (newValue == NULL)
                {
                    //from non-null to null
                    didChange = true;
                    free(resource->value);
                    resource->value = NULL;
                }
                else if (strcmp(resource->value, newValue) != 0)
                {
                    didChange = true;
                    free(resource->value);
                    resource->value = strdup(newValue);
                }
            }
            else if (newValue != NULL)
            {
                //changed from null to not null
                didChange = true;

                // TODO: We should only update for resources marked as READABLE
                resource->value = strdup(newValue);
            }

            if (didChange)
            {
                resource->dateOfLastSyncMillis = getCurrentUnixTimeMillis();

                //the database knows how to honor lazy saves
                jsonDatabaseSaveResource(resource);

                if ((resource->mode & RESOURCE_MODE_LAZY_SAVE_NEXT) == 0)
                {
                    backupRestoreService_request_CONFIG_UPDATED();
                }
            }
            else
            {
                // nothing really changed, but we are in sync so let the database deal with that without persisting now
                jsonDatabaseUpdateDateOfLastSyncMillis(resource);
            }

            if ((resource->mode & RESOURCE_MODE_EMIT_EVENTS) && didChange)
            {
                sendEvent = true;
            }
        }

        if(sendEvent)
        {
            sendResourceUpdatedEvent(resource, metadata);
        }
    }

    resourceDestroy(resource);
}

void setMetadata(const char *deviceUuid, const char *endpointId, const char *name, const char *value)
{
    icLogDebug(LOG_TAG, "%s: deviceUuid=%s, endpointId=%s, name=%s, value=%s", __FUNCTION__, deviceUuid, endpointId,
               name, value);

    if (deviceUuid == NULL || name == NULL || value == NULL)
    {
        icLogWarn(LOG_TAG, "setMetadata invalid args");
        return;
    }

    //first lets get any previous value and compare.  If they are not different, we dont need to do anything.
    // Note: we cannot store NULL for a metadata item.  A null result from getMetadata means it wasnt set at all.
    AUTO_CLEAN(free_generic__auto) const char *metadataUri = getMetadataUri(deviceUuid, endpointId, name);
    icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(metadataUri);
    bool doSave = false;

    if(metadata != NULL)
    {
        if(strcmp(metadata->value, value) != 0)
        {
            free(metadata->value);
            metadata->value = strdup(value);
            doSave = true;
        }
    }
    else
    {
        metadata = (icDeviceMetadata *) calloc(1, sizeof(icDeviceMetadata));
        metadata->deviceUuid = strdup(deviceUuid);
        if (endpointId != NULL)
        {
            metadata->endpointId = strdup(endpointId);
        }
        metadata->id = strdup(name);
        metadata->value = strdup(value);
        metadata->uri = strdup(metadataUri);

        doSave = true;
    }

    if(doSave)
    {
        jsonDatabaseSaveMetadata(metadata);
    }

    metadataDestroy(metadata);
}

/*
 * Caller frees result
 */
char *getMetadata(const char *deviceUuid, const char *endpointId, const char *name)
{
    char *result = NULL;

    if (deviceUuid == NULL || name == NULL)
    {
        icLogWarn(LOG_TAG, "getMetadata invalid args");
        return NULL;
    }

    char *uri = getMetadataUri(deviceUuid, endpointId, name);
    icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(uri);
    free(uri);

    if (metadata != NULL)
    {
        //we cant return the actual pointer that is stored in our hash map, so we strdup it and the caller must free
        if (metadata->value != NULL)
        {
            result = strdup(metadata->value);
        }

        metadataDestroy(metadata);
    }

    return result;
}

void setBooleanMetadata(const char* deviceUuid, const char* endpointId, const char* name, bool value)
{
    setMetadata(deviceUuid, endpointId, name, value ? "true" : "false");
}

bool getBooleanMetadata(const char* deviceUuid, const char* endpointId, const char* name)
{
    bool result = false;

    char *value = getMetadata(deviceUuid, endpointId, name);

    if(value != NULL && strcmp(value, "true") == 0)
    {
        result = true;
    }

    free(value);
    return result;
}

icLinkedList *deviceServiceGetDevicesBySubsystem(const char *subsystem)
{
    icLinkedList *result = linkedListCreate();

    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = linkedListIteratorGetNext(iterator);
        DeviceDriver *driver = getDeviceDriverForUri(device->uri);
        if (driver != NULL &&
            driver->subsystemName != NULL &&
            strcmp(driver->subsystemName, subsystem) == 0)
        {
            // Take it out of the source list so we can put it in our list
            linkedListIteratorDeleteCurrent(iterator, standardDoNotFreeFunc);
            linkedListAppend(result, device);
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);

    return result;
}

static DeviceDriver *getDeviceDriverForUri(const char *uri)
{
    DeviceDriver *result = NULL;

    icDevice *device = deviceServiceGetDeviceByUri(uri);
    if (device != NULL)
    {
        result = deviceDriverManagerGetDeviceDriver(device->managingDeviceDriver);
        deviceDestroy(device);
    }
    else
    {
        icLogWarn(LOG_TAG, "getDeviceDriverForUri: did not find device at uri %s", uri);
    }

    return result;
}

char *getMetadataUri(const char *deviceUuid, const char *endpointId, const char *name)
{
    char *uri;

    if (endpointId == NULL)
    {
        /*   / + deviceUuid + /m/ + name  */
        uri = (char *) calloc(1, 1 + strlen(deviceUuid) + 3 + strlen(name) + 1);
        sprintf(uri, "/%s/m/%s", deviceUuid, name);
    }
    else
    {
        /*   endpointUri + /m/ + name  */
        char *epUri = getEndpointUri(deviceUuid, endpointId);
        uri = (char *) calloc(1, strlen(epUri) + 3 + strlen(name) + 1);
        sprintf(uri, "%s/m/%s", epUri, name);
        free(epUri);
    }

    return uri;
}

bool deviceServiceIsReadyForDevices()
{
    pthread_mutex_lock(&readyForDevicesMutex);
    bool retVal = deviceServiceIsReadyForDevicesInternal();
    pthread_mutex_unlock(&readyForDevicesMutex);
    return retVal;
}

void deviceServiceSetOtaUpgradeDelay(uint32_t delaySeconds)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

#ifndef CONFIG_DEBUG_ZITH_CI_TESTS
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE
    icLogDebug(LOG_TAG, "Setting zigbee OTA upgrade delay to : %" PRIu32 " seconds", delaySeconds);

    zigbeeSubsystemSetOtaUpgradeDelay(delaySeconds);
#endif /* CONFIG_SERVICE_DEVICE_ZIGBEE */
#endif /* CONFIG_DEBUG_ZITH_CI_TESTS */
}

/****************** PRIVATE INTERNAL FUNCTIONS ***********************/

static bool deviceServiceIsReadyForDevicesInternal()
{
    return subsystemsReadyForDevices && deviceDescriptorReadyForDevices;
}

static void processDeviceDescriptorsBackgroundTask(void *arg)
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&deviceDescriptorProcessorMtx);
    deviceDescriptorProcessorTask = 0;
    pthread_mutex_unlock(&deviceDescriptorProcessorMtx);

    deviceServiceProcessDeviceDescriptors();

    icLogDebug(LOG_TAG, "%s done", __FUNCTION__);
}

static void deviceDescriptorsReadyForDevicesCallback()
{
    icLogDebug(LOG_TAG, "Device descriptors ready for devices");
    pthread_mutex_lock(&readyForDevicesMutex);
    if (!deviceDescriptorReadyForDevices)
    {
        deviceDescriptorReadyForDevices = true;
        if (deviceServiceIsReadyForDevicesInternal())
        {
            sendReadyForDevicesEvent();
        }

        pthread_mutex_lock(&deviceDescriptorProcessorMtx);
        if (deviceDescriptorProcessorTask == 0)
        {
            deviceDescriptorProcessorTask = scheduleDelayTask(DEVICE_DESCRIPTOR_PROCESSOR_DELAY_SECS,
                                                              DELAY_SECS,
                                                              processDeviceDescriptorsBackgroundTask,
                                                              NULL);
        }
        pthread_mutex_unlock(&deviceDescriptorProcessorMtx);
    }
    pthread_mutex_unlock(&readyForDevicesMutex);
}

static void descriptorsUpdatedCallback()
{
    icLogDebug(LOG_TAG, "%s", __FUNCTION__);

    pthread_mutex_lock(&deviceDescriptorProcessorMtx);
    if (deviceDescriptorProcessorTask == 0)
    {
        deviceDescriptorProcessorTask = scheduleDelayTask(DEVICE_DESCRIPTOR_PROCESSOR_DELAY_SECS,
                                                          DELAY_SECS,
                                                          processDeviceDescriptorsBackgroundTask,
                                                          NULL);
    }
    pthread_mutex_unlock(&deviceDescriptorProcessorMtx);
}

static void subsystemManagerInitializedCallback(const char *subsystem)
{
    // Let any drivers do anything they need to do
    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDriversBySubsystem(subsystem);
    icLinkedListIterator *iter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(iter))
    {
        DeviceDriver *driver = (DeviceDriver *)linkedListIteratorGetNext(iter);
        if (driver->subsystemInitialized != NULL)
        {
            driver->subsystemInitialized(driver->callbackContext);
        }
    }
    linkedListIteratorDestroy(iter);
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);

}

static void subsystemManagerReadyForDevicesCallback()
{
    icLogDebug(LOG_TAG, "Subsystem manager ready for devices");
    pthread_mutex_lock(&readyForDevicesMutex);
    if (!subsystemsReadyForDevices)
    {
        subsystemsReadyForDevices = true;
        if (deviceServiceIsReadyForDevicesInternal())
        {
            sendReadyForDevicesEvent();
        }
    }
    pthread_mutex_unlock(&readyForDevicesMutex);

    // Load up a background threadpool that will perform any required device initialization
    //
    startDeviceInitialization();
}

// the time zone changed... notify any devices that have the well-known timezone resource
void timeZoneChanged(const char *timeZone)
{
    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = linkedListIteratorGetNext(iterator);
        icDeviceResource *tzResource = deviceServiceFindDeviceResourceById(device, COMMON_DEVICE_RESOURCE_TIMEZONE);
        if (tzResource != NULL)
        {
            deviceServiceWriteResource(tzResource->uri, timeZone);
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

static void blacklistDevice(const char *uuid)
{
    AUTO_CLEAN(free_generic__auto) char *propValue = getPropertyAsString(CPE_BLACKLISTED_DEVICES_PROPERTY_NAME, "");

    if (stringIsEmpty(uuid) == true)
    {
        setPropertyValue(CPE_BLACKLISTED_DEVICES_PROPERTY_NAME, uuid, true, PROPERTY_SRC_DEFAULT);

        icLogDebug(LOG_TAG, "Device uuid=%s is now blacklisted", uuid);
    }
    else if (deviceServiceIsDeviceBlacklisted(uuid) == true)
    {
        icLogDebug(LOG_TAG, "Device uuid=%s is already in blacklist", uuid);
    }
    else
    {
        AUTO_CLEAN(free_generic__auto) char *newPropValue = stringBuilder("%s,%s", propValue, uuid);

        if (newPropValue != NULL)
        {
            setPropertyValue(CPE_BLACKLISTED_DEVICES_PROPERTY_NAME, newPropValue, true, PROPERTY_SRC_DEFAULT);

            icLogDebug(LOG_TAG, "Device uuid=%s is now added to blacklist", uuid);
        }
    }
}

bool deviceServiceIsDeviceBlacklisted(const char *uuid)
{
    bool retVal = false;

    AUTO_CLEAN(free_generic__auto) char *blacklistedDevices = getPropertyAsString(CPE_BLACKLISTED_DEVICES_PROPERTY_NAME, "");

    if (stringIsEmpty(blacklistedDevices) == false)
    {
        char *savePtr;
        char *uuidUntrimmed = strtok_r(blacklistedDevices, ",", &savePtr);

        while (uuidUntrimmed != NULL)
        {
            AUTO_CLEAN(free_generic__auto) char *uuidTrimmed = trimString(uuidUntrimmed);
            icLogDebug(LOG_TAG, "Device uuid=%s is blacklisted. Matching with uuid=%s", uuidTrimmed, uuid);

            if (stringCompare(uuid, uuidTrimmed, true) == 0)
            {
                retVal = true;
                break;
            }

            uuidUntrimmed = strtok_r(NULL, ",", &savePtr);
        }
    }

    return retVal;
}

// the blacklisted devices property passed as function arg... remove a device if already paired
void processBlacklistedDevices(const char *propValue)
{
    if (stringIsEmpty(propValue) == false)
    {
        AUTO_CLEAN(free_generic__auto) char *blacklistedDevices = strdup(propValue);
        char *savePtr;
        char *uuid = strtok_r(blacklistedDevices, ",", &savePtr);

        while (uuid != NULL)
        {
            AUTO_CLEAN(free_generic__auto) char *uuidTrimmed = trimString(uuid);
            icLogDebug(LOG_TAG, "Deleting device uuid=%s because it is blacklisted", uuidTrimmed);
            deviceServiceRemoveDevice(uuidTrimmed);

            uuid = strtok_r(NULL, ",", &savePtr);
        }
    }
}

static void *discoverDeviceClassThreadProc(void *arg)
{
    discoverDeviceClassContext *ctx = (discoverDeviceClassContext *) arg;

    //start discovery for all device drivers that support this device class

    icLinkedList *startedDeviceDrivers = linkedListCreate();

    bool atLeastOneStarted = false;
    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDriversByDeviceClass(ctx->deviceClass);
    if (deviceDrivers != NULL)
    {
        icLinkedListIterator *driverIterator = linkedListIteratorCreate(deviceDrivers);
        while (linkedListIteratorHasNext(driverIterator))
        {
            DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(driverIterator);

            if (ctx->findOrphanedDevices == true)
            {
                if (driver->recoverDevices != NULL)
                {
                    icLogDebug(LOG_TAG,"telling %s to start device recovery...",driver->driverName);
                    bool started = driver->recoverDevices(driver->callbackContext, ctx->deviceClass);

                    if (started == false)
                    {
                        // this is only a warning because other drivers for this class might successfully
                        // start recovery
                        icLogWarn(LOG_TAG, "device driver %s failed to start device recovery",driver->driverName);
                    }
                    else
                    {
                        icLogDebug(LOG_TAG,"device driver %s start device recovery successfully",driver->driverName);
                        atLeastOneStarted = true;
                        linkedListAppend(startedDeviceDrivers,driver);
                    }
                }
                else
                {
                    icLogInfo(LOG_TAG,"device driver %s does not support device recovery",driver->driverName);
                }
            }
            else
            {
                icLogDebug(LOG_TAG, "telling %s to start discovering...", driver->driverName);
                bool started = driver->discoverDevices(driver->callbackContext, ctx->deviceClass);
                if (started == false)
                {
                    icLogError(LOG_TAG, "device driver %s failed to start discovery", driver->driverName);
                }
                else
                {
                    icLogDebug(LOG_TAG, "device driver %s started discovering successfully", driver->driverName);
                    atLeastOneStarted = true;
                    linkedListAppend(startedDeviceDrivers, driver);
                }
            }
        }
        linkedListIteratorDestroy(driverIterator);
        linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);
    }

    if (atLeastOneStarted)
    {
        pthread_mutex_lock(&ctx->mtx);
        if (ctx->timeoutSeconds > 0)
        {
            icLogDebug(LOG_TAG,
                       "discoverDeviceClassThreadProc: waiting %d seconds to auto stop discovery/recovery of %s",
                       ctx->timeoutSeconds,
                       ctx->deviceClass);
            incrementalCondTimedWait(&ctx->cond, &ctx->mtx, ctx->timeoutSeconds);
        }
        else
        {
            icLogDebug(LOG_TAG,
                       "discoverDeviceClassThreadProc: waiting for explicit stop discovery/recovery of %s",
                       ctx->deviceClass);
            //TODO switch this over to timedWait library
            pthread_cond_wait(&ctx->cond, &ctx->mtx);
        }
        pthread_mutex_unlock(&ctx->mtx);

        icLogInfo(LOG_TAG, "discoverDeviceClassThreadProc: stopping discovery/recovery of %s", ctx->deviceClass);

        //stop discovery
        icLinkedListIterator *iterator = linkedListIteratorCreate(startedDeviceDrivers);
        while (linkedListIteratorHasNext(iterator))
        {
            DeviceDriver *driver = (DeviceDriver *) linkedListIteratorGetNext(iterator);
            driver->stopDiscoveringDevices(driver->callbackContext, ctx->deviceClass);
        }
        linkedListIteratorDestroy(iterator);

        sendDiscoveryStoppedEvent(ctx->deviceClass);
    }

    pthread_mutex_lock(&discoveryControlMutex);
    hashMapDelete(activeDiscoveries,
                  ctx->deviceClass,
                  (uint16_t) (strlen(ctx->deviceClass) + 1),
                  standardDoNotFreeHashMapFunc);
    pthread_mutex_unlock(&discoveryControlMutex);

    linkedListDestroy(startedDeviceDrivers, standardDoNotFreeFunc);
    pthread_mutex_destroy(&ctx->mtx);
    pthread_cond_destroy(&ctx->cond);
    free(ctx->deviceClass);
    free(ctx);

    return NULL;
}

static discoverDeviceClassContext *startDiscoveryForDeviceClass(const char *deviceClass, uint16_t timeoutSeconds, bool findOrphanedDevices)
{
    icLogDebug(LOG_TAG, "startDiscoveryForDeviceClass: %s for %d seconds", deviceClass, timeoutSeconds);

    discoverDeviceClassContext *ctx = (discoverDeviceClassContext *) calloc(1, sizeof(discoverDeviceClassContext));

    ctx->useMonotonic = true;
    initTimedWaitCond(&ctx->cond);

    pthread_mutex_init(&ctx->mtx, NULL);
    ctx->timeoutSeconds = timeoutSeconds;
    ctx->deviceClass = strdup(deviceClass);
    ctx->findOrphanedDevices = findOrphanedDevices;


    char *name = stringBuilder("discoverDC:%s", deviceClass);
    createDetachedThread(discoverDeviceClassThreadProc, ctx, name);
    free(name);

    return ctx;
}

static char *getEndpointUri(const char *deviceUuid, const char *endpointId)
{
    char *uri = (char *) malloc(1 + strlen(deviceUuid) + 4 + strlen(endpointId) + 1);
    sprintf(uri, "/%s/ep/%s", deviceUuid, endpointId);
    return uri;
}

/* return a portion of a URI.  If uri = /3908023984/ep/3/m/test and numSlashes is 3 then
 * we return /3908023984/ep/3
 */
static char *getPartialUri(const char *uri, int numSlashes)
{
    if (uri == NULL)
    {
        return NULL;
    }

    char *result = strdup(uri);
    char *tmp = result;
    int slashCount = 0;
    while (*tmp != '\0')
    {
        if (*tmp == '/')
        {
            if (slashCount++ == numSlashes)
            {
                *tmp = '\0';
                break;
            }
        }

        tmp++;
    }

    return result;
}

static void yoinkResource(icDeviceResource *oldRes,
                          icDevice *newDevice,
                          icLinkedList *resources,
                          const char *resourceId)
{
    sbIcLinkedListIterator *newResIt = linkedListIteratorCreate(resources);
    while (linkedListIteratorHasNext(newResIt) == true)
    {
        icDeviceResource *newRes = linkedListIteratorGetNext(newResIt);
        if (stringCompare(resourceId, newRes->id, false) == 0)
        {
            // found it
            free(newRes->value);
            newRes->value = oldRes->value;
            oldRes->value = NULL;
            break;
        }
    }
}

/*
 * Copy/move (aka 'yoink') some custom data from one device to another as part of reconfiguration.  This data
 * includes metadata, date added, and labels.  Return true on success.
 *
 * TODO: if a driver has any non-standard custom stuff that isnt covered here, this may need to get expanded
 */
static bool yoinkCustomizedDeviceData(icDevice *oldDevice, icDevice *newDevice)
{
    bool result = true;

    linkedListDestroy(newDevice->metadata, (linkedListItemFreeFunc) metadataDestroy);
    newDevice->metadata = oldDevice->metadata;
    oldDevice->metadata = NULL;

    // grab the original date added
    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *oldDateAddedRes =
            deviceServiceGetResourceByIdInternal(oldDevice->uuid, NULL, COMMON_DEVICE_RESOURCE_DATE_ADDED,
                                                 false);

    // yoink the date added from old and apply to new
    if (oldDateAddedRes != NULL)
    {
        yoinkResource(oldDateAddedRes, newDevice, newDevice->resources, COMMON_DEVICE_RESOURCE_DATE_ADDED);
    }

    // loop over each endpoint on the old device so we can yoink metadata and labels
    sbIcLinkedListIterator *it = linkedListIteratorCreate(oldDevice->endpoints);
    while(linkedListIteratorHasNext(it) == true)
    {
        icDeviceEndpoint *endpoint = linkedListIteratorGetNext(it);

        // find the label resource, if it exists
        AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *oldLabelRes =
                deviceServiceGetResourceByIdInternal(oldDevice->uuid, endpoint->id, COMMON_ENDPOINT_RESOURCE_LABEL,
                                                     false);

        // find the matching endpoint on the new instance
        bool endpointMatched = false;
        sbIcLinkedListIterator *newIt = linkedListIteratorCreate(newDevice->endpoints);
        while(linkedListIteratorHasNext(newIt) == true)
        {
            icDeviceEndpoint *newEndpoint = linkedListIteratorGetNext(newIt);

            if (stringCompare(endpoint->id, newEndpoint->id, false) == 0)
            {
                // this is the matching endpoint, engage yoinkification technology

                linkedListDestroy(newEndpoint->metadata, (linkedListItemFreeFunc) metadataDestroy);
                newEndpoint->metadata = endpoint->metadata;
                endpoint->metadata = NULL;

                if (oldLabelRes != NULL)
                {
                    // yoink the label from old and apply to new
                    yoinkResource(oldLabelRes, newDevice, newEndpoint->resources, COMMON_ENDPOINT_RESOURCE_LABEL);
                }

                endpointMatched = true;
            }
        }

        if (endpointMatched == false)
        {
            icLogError(LOG_TAG, "%s: failed to match endpoints for metadata migration! (%s not found)",
                       __func__, endpoint->id);
            result = false;
            break;
        }
    }

    return result;
}

static bool reconfigureDevice(icDevice *device, DeviceDriver *driver)
{
    bool result = false;

    if (driver->getDeviceClassVersion == NULL)
    {
        icLogError(LOG_TAG, "%s: device reconfiguration required, but unable to determine new device class version",
                __func__);
        return false;
    }

    // these resources are destroyed when the device is destroyed in deviceInitializationTask
    icDeviceResource *manufacturer = deviceServiceFindDeviceResourceById(device,
                                                             COMMON_DEVICE_RESOURCE_MANUFACTURER);
    icDeviceResource *model = deviceServiceFindDeviceResourceById(device,
                                                             COMMON_DEVICE_RESOURCE_MODEL);
    icDeviceResource *hardwareVersion = deviceServiceFindDeviceResourceById(device,
                                                             COMMON_DEVICE_RESOURCE_HARDWARE_VERSION);
    icDeviceResource *firmwareVersion = deviceServiceFindDeviceResourceById(device,
                                                             COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    if (manufacturer == NULL || model == NULL || firmwareVersion == NULL || hardwareVersion == NULL)
    {
        icLogError(LOG_TAG, "%s: device reconfiguration required, but unable to locate required resources", __func__);
        return false;
    }

    // Reconfiguration works by creating a new icDevice instance much like how we did when we first
    // discovered it.  We then move over any transient data (metadata) from the old instance, then
    // persist it and discard the old one.

    uint8_t newDeviceClassVersion = 0;
    if (driver->getDeviceClassVersion(driver->callbackContext, device->deviceClass, &newDeviceClassVersion) == true)
    {
        icInitialResourceValues *initialValues = initialResourceValuesCreate();

        AUTO_CLEAN(deviceDescriptorFree__auto) DeviceDescriptor *dd = deviceDescriptorsGet(manufacturer->value,
                model->value, hardwareVersion->value, firmwareVersion->value);

        //Create a device instance populated with all required items from the base device class specification
        AUTO_CLEAN(deviceDestroy__auto) icDevice *newDevice = createDevice(device->uuid,
                                                                           device->deviceClass,
                                                                           newDeviceClassVersion,
                                                                           driver->driverName,
                                                                           dd);

        //NOTE: this will not currently work for sleepy zigbee devices.  Once we need this mechanism for one,
        //  we will have to schedule for their next checkin.
        if (driver->configureDevice(driver->callbackContext, newDevice, dd) == false)
        {
            icLogError(LOG_TAG, "%s: failed to reconfigure device", __func__);
        }
        else if (fetchCommonResourcesInitialValues(newDevice,
                                                   manufacturer->value,
                                                   model->value,
                                                   hardwareVersion->value,
                                                   firmwareVersion->value,
                                                   initialValues) == false)
        {
            icLogError(LOG_TAG, "%s: failed to fetch common initial resource values for reconfiguration", __func__);
        }
        else if (addCommonResources(newDevice, initialValues) == false)
        {
            icLogError(LOG_TAG, "%s: failed to add common resources for reconfiguration", __func__);
        }
        else if (driver->fetchInitialResourceValues(driver->callbackContext, newDevice, initialValues) == false)
        {
            icLogError(LOG_TAG, "%s: failed to fetch initial resource values for reconfiguration", __func__);
        }
        else if (driver->registerResources(driver->callbackContext, newDevice, initialValues) == false)
        {
            icLogError(LOG_TAG, "%s: failed to register resources for reconfiguration", __func__);
        }
        else
        {
            icLogInfo(LOG_TAG, "%s: device reconfigured -- persisting.", __func__);

            // changes were likely made to the device, persist it.  Steal metadata from the old instance.
            result = yoinkCustomizedDeviceData(device, newDevice);

            if (result == true)
            {
                jsonDatabaseRemoveDeviceById(device->uuid);
                finalizeNewDevice(newDevice, false, false);
            }
        }

        initialResourceValuesDestroy(initialValues);
    }
    else
    {
        icLogError(LOG_TAG, "%s: failed to get device class version.  Skipping reconfiguration", __func__ );
    }

    return result;
}

static void deviceInitializationTask(void *arg)
{
    char *uuid = (char*)arg;

    AUTO_CLEAN(deviceDestroy__auto) icDevice *device = deviceServiceGetDevice(uuid);
    if(device != NULL)
    {
        DeviceDriver *driver = getDeviceDriverForUri(device->uri);
        if(driver != NULL)
        {
            bool initDone = false;

            // a device can optionally be reconfigured OR synchronized (reconfiguration covers synchronization)
            if (driver->deviceNeedsReconfiguring != NULL &&
                driver->deviceNeedsReconfiguring(driver->callbackContext, device) == true)
            {
                initDone = reconfigureDevice(device, driver);
            }

            // if we tried reconfiguration but it failed, just synchronize and move on.
            if(driver->synchronizeDevice != NULL && initDone == false)
            {
                driver->synchronizeDevice(driver->callbackContext, device);
            }
        }
        else
        {
            icLogError(LOG_TAG, "%s: driver for device %s not found", __FUNCTION__, uuid);
        }
    }
    else
    {
        icLogError(LOG_TAG, "%s: device %s not found", __FUNCTION__, uuid);
    }
}

static void startDeviceInitialization()
{
    icLinkedList *devices = jsonDatabaseGetDevices();
    icLinkedListIterator *iterator = linkedListIteratorCreate(devices);
    while (linkedListIteratorHasNext(iterator))
    {
        icDevice *device = linkedListIteratorGetNext(iterator);
        if (threadPoolAddTask(deviceInitializerThreadPool, deviceInitializationTask, strdup(device->uuid), NULL) == false)
        {
            icLogError(LOG_TAG,"%s: failed to add deviceInitializationTask to thread pool",__FUNCTION__ );
        }
    }
    linkedListIteratorDestroy(iterator);
    linkedListDestroy(devices, (linkedListItemFreeFunc) deviceDestroy);
}

static void *shutdownDeviceDriverManagerThreadProc(void *arg)
{
    deviceDriverManagerShutdown();

    pthread_mutex_lock(&deviceDriverManagerShutdownMtx);
    pthread_cond_signal(&deviceDriverManagerShutdownCond);
    pthread_mutex_unlock(&deviceDriverManagerShutdownMtx);

    return NULL;
}

/*
 * Give the device driver manager a maximum amount of time to shut down the device drivers.  Some may be in the
 * middle of a firmware upgrade and we need to give them ample time to finish.
 */
static void shutdownDeviceDriverManager()
{
    icLogDebug(LOG_TAG, "%s: shutting down", __FUNCTION__);

    initTimedWaitCond(&deviceDriverManagerShutdownCond);
    pthread_mutex_lock(&deviceDriverManagerShutdownMtx);

    createDetachedThread(shutdownDeviceDriverManagerThreadProc, NULL, "driverMgrShutdown");

    if(incrementalCondTimedWait(&deviceDriverManagerShutdownCond,
                                &deviceDriverManagerShutdownMtx,
                                MAX_DRIVERS_SHUTDOWN_SECS) != 0)
    {
        icLogWarn(LOG_TAG, "%s: timed out waiting for drivers to shut down.", __FUNCTION__);
    }

    pthread_mutex_unlock(&deviceDriverManagerShutdownMtx);

    icLogDebug(LOG_TAG, "%s: finished shutting down", __FUNCTION__);
}

/****************** Simple Data Accessor Functions ***********************/
icLinkedList *deviceServiceGetDevicesByProfile(const char *profileId)
{
    //just pass through to database
    return jsonDatabaseGetDevicesByEndpointProfile(profileId);
}

icLinkedList *deviceServiceGetDevicesByDeviceClass(const char *deviceClass)
{
    //just pass through to database
    return jsonDatabaseGetDevicesByDeviceClass(deviceClass);
}

icLinkedList *deviceServiceGetDevicesByDeviceDriver(const char *deviceDriver)
{
    //just pass through to database
    return jsonDatabaseGetDevicesByDeviceDriver(deviceDriver);
}

icLinkedList *deviceServiceGetAllDevices()
{
    //just pass through to database
    return jsonDatabaseGetDevices();
}

icLinkedList *deviceServiceGetEndpointsByProfile(const char *profileId)
{
    //just pass through to database
    return jsonDatabaseGetEndpointsByProfile(profileId);
}

icDevice *deviceServiceGetDevice(const char *uuid)
{
    //just pass through to database
    return jsonDatabaseGetDeviceById(uuid);
}

bool deviceServiceIsDeviceKnown(const char *uuid)
{
    //just pass through to database
    return jsonDatabaseIsDeviceKnown(uuid);
}

icDevice *deviceServiceGetDeviceByUri(const char *uri)
{
    icDevice *device = NULL;
    device = jsonDatabaseGetDeviceByUri(uri);
    return device;
}

icDeviceEndpoint *deviceServiceGetEndpointByUri(const char *uri)
{
    icLogDebug(LOG_TAG, "%s: uri=%s", __FUNCTION__, uri);

    icDeviceEndpoint *endpoint = NULL;
    endpoint = jsonDatabaseGetEndpointByUri(uri);
    return endpoint;
}

icDeviceEndpoint *deviceServiceGetEndpointById(const char *deviceUuid, const char *endpointId)
{
    //just pass through to database
    return jsonDatabaseGetEndpointById(deviceUuid, endpointId);
}

/*
 * if endpointId is NULL, we are after an resource on the root device
 */
static icDeviceResource *deviceServiceGetResourceByIdInternal(const char *deviceUuid,
                                                              const char *endpointId,
                                                              const char *resourceId,
                                                              bool logDebug)
{
    icDeviceResource *result = NULL;

    // dont debug print on frequently fetched resource ids to preserve log files
    if(logDebug == true &&
       resourceId != NULL &&
       strcmp(COMMON_DEVICE_RESOURCE_DATE_LAST_CONTACTED, resourceId) != 0)
    {
        icLogDebug(LOG_TAG, "deviceServiceGetResource: deviceUuid=%s, endpointId=%s, resourceId=%s", deviceUuid,
                   stringCoalesce(endpointId),
                   resourceId);
    }

    if (deviceUuid != NULL && resourceId != NULL)
    {
        char *uri;

        if (endpointId == NULL)
        {
            //  / + uuid + /r/ + resource id + \0
            uri = (char *) calloc(1, 1 + strlen(deviceUuid) + 3 + strlen(resourceId) + 1);
            sprintf(uri, "/%s/r/%s", deviceUuid, resourceId);
        }
        else
        {
            //  endpointUri + /r/ + resource id + \0
            char *epUri = getEndpointUri(deviceUuid, endpointId);
            uri = (char *) calloc(1, strlen(epUri) + 3 + strlen(resourceId) + 1);
            sprintf(uri, "%s/r/%s", epUri, resourceId);
            free(epUri);
        }

        result = jsonDatabaseGetResourceByUri(uri);
        free(uri);
    }

    if (logDebug == true && result == NULL)
    {
        icLogDebug(LOG_TAG, "did not find the resource");
    }

    return result;
}

/*
 * Get the age (in milliseconds) since the provided resource was last updated/sync'd with the device.
 */
bool deviceServiceGetResourceAgeMillis(const char *deviceUuid,
                                       const char *endpointId,
                                       const char *resourceId,
                                       uint64_t *ageMillis)
{
    bool result = true;

    if (deviceUuid == NULL || resourceId == NULL || ageMillis == NULL)
    {
        return false;
    }

    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *resource = deviceServiceGetResourceByIdInternal(deviceUuid,
                                                                                                        endpointId,
                                                                                                        resourceId,
                                                                                                        true);

    if (resource != NULL)
    {
        *ageMillis = getCurrentUnixTimeMillis();
        *ageMillis -= resource->dateOfLastSyncMillis;
        result = true;
    }
    else
    {
        *ageMillis = 0;
        result = false;
    }

    return result;
}

/*
 * if endpointId is NULL, we are after an resource on the root device
 */
icDeviceResource *deviceServiceGetResourceById(const char *deviceUuid, const char *endpointId, const char *resourceId)
{
    return deviceServiceGetResourceByIdInternal(deviceUuid, endpointId, resourceId, true);
}

/*
 * Retrieve an icDeviceResource by id.  This will not look on any endpoints, but only on the device itself
 *
 * Caller should NOT destroy the icDeviceResource on the non-NULL result, as it belongs to the icDevice object
 *
 * @param device - the device
 * @param resourceId - the unique id of the desired resource
 *
 * @returns the icDeviceResource or NULL if not found
 */
icDeviceResource *deviceServiceFindDeviceResourceById(icDevice *device, const char *resourceId)
{
    icDeviceResource *retval = NULL;
    if (device == NULL)
    {
        icLogDebug(LOG_TAG, "%s: NULL device passed", __FUNCTION__);
        return NULL;
    }
    icLinkedListIterator *iter = linkedListIteratorCreate(device->resources);
    while (linkedListIteratorHasNext(iter))
    {
        icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(iter);
        if (strcmp(resource->id, resourceId) == 0)
        {
            retval = resource;
            break;
        }
    }
    linkedListIteratorDestroy(iter);

    return retval;
}

bool deviceServiceGetSystemProperty(const char *name, char **value)
{
    return jsonDatabaseGetSystemProperty(name, value);
}

bool deviceServiceSetSystemProperty(const char *name, const char *value)
{
    return jsonDatabaseSetSystemProperty(name, value);
}

bool deviceServiceGetMetadata(const char *uri, char **value)
{
    bool result = false;

    if (value != NULL)
    {
        icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(uri);

        if (metadata != NULL && metadata->value != NULL)
        {
            *value = strdup(metadata->value);
        }
        else
        {
            *value = NULL;
        }

        metadataDestroy(metadata);
        result = true;
    }

    return result;
}

bool deviceServiceSetMetadata(const char *uri, const char *value)
{
    bool result = false;
    bool saveDb = false;

    icLogDebug(LOG_TAG, "%s: setting metadata %s %s", __FUNCTION__, uri, value);

    icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(uri);
    if (metadata == NULL)
    {
        //new item
        metadata = (icDeviceMetadata *) calloc(1, sizeof(icDeviceMetadata));
        saveDb = true;

        char *deviceId = (char *) calloc(1, strlen(uri)); //allocate worst case
        char *name = (char *) calloc(1, strlen(uri)); //allocate worst case
        char *endpointId = NULL;

        bool urlParseOk = false;

        if (strstr(uri, "/ep/") == NULL)
        {
            //this URI is for metadata on a device
            int numParsed = sscanf(uri, "/%[^/]/m/%s", deviceId, name);
            if (numParsed == 2)
            {
                urlParseOk = true;
            }
        }
        else
        {
            //this URI is for metadata on an endpoint
            endpointId = (char *) calloc(1, strlen(uri)); //allocate worst case
            int numParsed = sscanf(uri, "/%[^/]/ep/%[^/]/m/%s", deviceId, endpointId, name);
            if (numParsed == 3)
            {
                urlParseOk = true;
            }
        }

        if (urlParseOk == false)
        {
            icLogError(LOG_TAG, "Invalid URI %s", uri);
            free(deviceId);
            free(name);
            free(metadata);
            free(endpointId);
            return false;
        }

        metadata->id = name;
        metadata->deviceUuid = deviceId;
        metadata->endpointId = endpointId;
        metadata->uri = strdup(uri);
    }
    else
    {
        if (strcmp(uri, metadata->uri) != 0 || strcmp(value, metadata->value) != 0)
        {
            saveDb = true;
        }

        if (metadata->value != NULL)
        {
            free(metadata->value);
            metadata->value = NULL;
        }
    }

    if (saveDb)
    {
        if (value != NULL)
        {
            metadata->value = strdup(value);
        }

        result = jsonDatabaseSaveMetadata(metadata);
    }
    else
    {
        //there was no change, so just return success
        result = true;
    }

    metadataDestroy(metadata);

    return result;
}

/*
 * Retrieve a list of devices that contains the metadataId or
 * contains the metadataId value that is equal to valueToCompare
 *
 * If valueToCompare is NULL, will only look if the metadata exists.
 * Otherwise will only add devices that equal the metadata Id and it's value.
 *
 * Caller is responsible for calling deviceDestroy() on each returned device.
 *
 * @return - linked list of icDevices with found metadata, or NULL error occurred
 */
icLinkedList *deviceServiceGetDevicesByMetadata(const char *metadataId, const char *valueToCompare)
{
    // sanity check
    if (metadataId == NULL)
    {
        icLogWarn(LOG_TAG, "%s: unable to use metadataId ... bailing", __FUNCTION__);
        return NULL;
    }

    // get the list of devices
    //
    icLinkedList *deviceList = deviceServiceGetAllDevices();

    // sanity check
    if (deviceList == NULL)
    {
        icLogError(LOG_TAG, "%s: unable to get list of all devices ... bailing", __FUNCTION__);
        return NULL;
    }

    // create the found list
    //
    icLinkedList *devicesFound = linkedListCreate();

    // iterate though each device
    //
    icLinkedListIterator *listIter = linkedListIteratorCreate(deviceList);
    while (linkedListIteratorHasNext(listIter))
    {
        icDevice *device = (icDevice *) linkedListIteratorGetNext(listIter);

        // need to create the metadata deviceUri
        //
        char *deviceMetadataUri = createDeviceMetadataUri(device->uuid, metadataId);
        if (deviceMetadataUri != NULL)
        {
            // now get the metadata value from device
            //
            char *deviceMetadataValue = NULL;
            if (deviceServiceGetMetadata(deviceMetadataUri, &deviceMetadataValue) && deviceMetadataValue != NULL)
            {
                bool addDevice = false;

                // determine if device should be added to list
                //
                if (valueToCompare == NULL)
                {
                    addDevice = true;
                }
                else if (strcasecmp(deviceMetadataValue, valueToCompare) == 0)
                {
                    addDevice = true;
                }

                // remove from device list and add to found list
                //
                if (addDevice)
                {
                    linkedListIteratorDeleteCurrent(listIter, standardDoNotFreeFunc);
                    linkedListAppend(devicesFound, device);
                }

                // cleanup value
                free(deviceMetadataValue);
            }

            // cleanup uri
            free(deviceMetadataUri);
        }
        else
        {
            icLogWarn(LOG_TAG, "%s: unable to create device Metadata Uri for device %s", __FUNCTION__, device->uuid);
        }
    }

    // cleanup
    linkedListIteratorDestroy(listIter);
    linkedListDestroy(deviceList, (linkedListItemFreeFunc) deviceDestroy);

    return devicesFound;
}

bool deviceServiceReloadDatabase()
{
    return jsonDatabaseReload();
}

icLinkedList *deviceServiceGetMetadataByUriPattern(char *uriPattern)
{
    icLinkedList *retval = NULL;
    if (uriPattern == NULL)
    {
        icLogWarn(LOG_TAG, "%s: uriPattern is NULL", __FUNCTION__);
        return NULL;
    }

    if (isUriPattern(uriPattern))
    {
        char *regex = createRegexFromPattern(uriPattern);
        retval = jsonDatabaseGetMetadataByUriRegex(regex);
        free(regex);
    }
    else
    {
        retval = linkedListCreate();
        icDeviceMetadata *metadata = jsonDatabaseGetMetadataByUri(uriPattern);
        if (metadata != NULL)
        {
            linkedListAppend(retval, metadata);
        }
    }

    return retval;
}

icLinkedList *deviceServiceGetResourcesByUriPattern(char *uriPattern)
{
    icLinkedList *retval = NULL;

    if (uriPattern == NULL)
    {
        icLogError(LOG_TAG, "%s: URI pattern is NULL", __FUNCTION__);
        return NULL;
    }

    if (isUriPattern(uriPattern))
    {
        char *regex = createRegexFromPattern(uriPattern);
        retval = jsonDatabaseGetResourcesByUriRegex(regex);
        free(regex);
    }
    else
    {
        retval = linkedListCreate();
        icDeviceResource *resource = jsonDatabaseGetResourceByUri(uriPattern);
        if (resource != NULL)
        {
            linkedListAppend(retval, resource);
        }
    }

    return retval;
}

void deviceServiceNotifySystemPowerEvent(DeviceServiceSystemPowerEventType powerEvent)
{
    icLogDebug(LOG_TAG, "%s: state=%s", __FUNCTION__, DeviceServiceSystemPowerEventTypeLabels[powerEvent]);

    // Let any drivers do anything they need to do
    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    icLinkedListIterator *iter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(iter))
    {
        DeviceDriver *driver = (DeviceDriver *)linkedListIteratorGetNext(iter);
        if (driver->systemPowerEvent != NULL)
        {
            driver->systemPowerEvent(driver->callbackContext, powerEvent);
        }
    }
    linkedListIteratorDestroy(iter);
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);

}

void deviceServiceNotifyPropertyChange(cpePropertyEvent *event)
{
    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    icLinkedListIterator *iter = linkedListIteratorCreate(deviceDrivers);

    while (linkedListIteratorHasNext(iter))
    {
        DeviceDriver *driver = (DeviceDriver *)linkedListIteratorGetNext(iter);
        if (driver->propertyChanged != NULL)
        {
            driver->propertyChanged(driver->callbackContext, event);
        }
    }
    linkedListIteratorDestroy(iter);
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);

}

static icLinkedList *getSupportedDeviceClasses(void)
{
    icLinkedList *result = linkedListCreate();

    icLinkedList *deviceDrivers = deviceDriverManagerGetDeviceDrivers();
    sbIcLinkedListIterator *drivers = linkedListIteratorCreate(deviceDrivers);
    while (linkedListIteratorHasNext(drivers))
    {
        DeviceDriver *driver = (DeviceDriver *)linkedListIteratorGetNext(drivers);
        sbIcLinkedListIterator *deviceClassesIter = linkedListIteratorCreate(driver->supportedDeviceClasses);
        while (linkedListIteratorHasNext(deviceClassesIter))
        {
            char *deviceClass = linkedListIteratorGetNext(deviceClassesIter);

            // a hash set would be more efficient, but probably not worth the complexity given that this
            // is expected to be very infrequently invoked.
            if (linkedListFind(result, deviceClass, linkedListStringCompareFunc) == false)
            {
                linkedListAppend(result, strdup(deviceClass));
            }
        }
    }
    // This is a shallow clone, so free function doesn't really matter
    linkedListDestroy(deviceDrivers, standardDoNotFreeFunc);

    return result;
}

DeviceServiceStatus *deviceServiceGetStatus()
{
    DeviceServiceStatus *result = calloc(1, sizeof(DeviceServiceStatus));
    result->zigbeeReady = subsystemManagerIsSubsystemReady(ZIGBEE_SUBSYSTEM_ID);
    result->supportedDeviceClasses = getSupportedDeviceClasses();
    result->discoveryRunning = deviceServiceIsDiscoveryActive();
    result->discoveringDeviceClasses = linkedListCreate();

    if (result->discoveryRunning == true)
    {
        pthread_mutex_lock(&discoveryControlMutex);

        result->discoveryTimeoutSeconds = discoveryTimeoutSeconds;

        icHashMapIterator *iterator = hashMapIteratorCreate(activeDiscoveries);
        while (hashMapIteratorHasNext(iterator))
        {
            char *class;
            uint16_t classLen;
            discoverDeviceClassContext *ctx;
            if (hashMapIteratorGetNext(iterator, (void **) &class, &classLen, (void **) &ctx))
            {
                if (linkedListFind(result->discoveringDeviceClasses,
                        class,
                        linkedListStringCompareFunc) == false)
                {
                    linkedListAppend(result->discoveringDeviceClasses, strdup(class));
                }
            }
        }
        hashMapIteratorDestroy(iterator);

        pthread_mutex_unlock(&discoveryControlMutex);
    }

    return result;
}

void deviceServiceDestroyServiceStatus(DeviceServiceStatus *status)
{
    if (status != NULL)
    {
        linkedListDestroy(status->supportedDeviceClasses, NULL);
        linkedListDestroy(status->discoveringDeviceClasses, NULL);
        free(status);
    }
}

bool deviceServiceIsDeviceInCommFail(const char *deviceUuid)
{
    if (deviceUuid == NULL)
    {
        return false;
    }

    bool retVal = false;

    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *commFailResource =
            deviceServiceGetResourceById(deviceUuid,
                                         NULL,
                                         COMMON_DEVICE_RESOURCE_COMM_FAIL);

    if(commFailResource != NULL)
    {
        retVal = stringToBool(commFailResource->value);
    }

    return retVal;
}

char *deviceServiceGetDeviceFirmwareVersion(const char *deviceUuid)
{
    char *retVal = NULL;

    if (deviceUuid == NULL)
    {
        return NULL;
    }

    AUTO_CLEAN(resourceDestroy__auto) icDeviceResource *fwResource =
            deviceServiceGetResourceById(deviceUuid,
                                         NULL,
                                         COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION);

    if(fwResource != NULL)
    {
        retVal = fwResource->value;
        fwResource->value = NULL;
    }

    return retVal;
}

/****************** Simple Data Accessor Functions ***********************/
