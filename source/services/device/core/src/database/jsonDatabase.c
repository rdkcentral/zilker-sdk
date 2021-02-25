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
 * Created by Micah Koch on 5/4/18.
 */

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "jsonDatabase.h"
#include <icConfig/storage.h>
#include <icTypes/icStringHashMap.h>
#include <cjson/cJSON.h>
#include <icLog/logging.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <device/icDeviceMetadata.h>
#include <regex.h>
#include <icTime/timeUtils.h>

#define LOG_TAG "jsonDeviceDatabase"
#define STORAGE_NAMESPACE "devicedb"
#define SYSTEM_PROPERTIES_KEY "systemProperties"

// We keep per device files, and keep an in memory cache of all devices
// This structure keeps track of which devices might be dirty and require
// flushing to disk
typedef struct
{
    icDevice *device;
    bool dirty;
} DeviceCacheEntry;

// For our URI hashmap, we need to know what sort of object is pointed to
typedef enum
{
    LOCATOR_TYPE_DEVICE,
    LOCATOR_TYPE_ENDPOINT,
    LOCATOR_TYPE_RESOURCE,
    LOCATOR_TYPE_METADATA
} LocatorType;

// For our URI hashmap, a point to an endpoint and its device
typedef struct
{
    DeviceCacheEntry *deviceCacheEntry;
    icDeviceEndpoint *endpoint;
} EndpointLocator;

// For our URI hashmap, a pointer to a resource, its device and/or its endpoint
typedef struct
{
    DeviceCacheEntry *deviceCacheEntry;
    icDeviceEndpoint *endpoint;
    icDeviceResource *resource;
} ResourceLocator;

// For our URI hashmap, a pointer to metadata, its device and/or its endpoint
typedef struct
{
    DeviceCacheEntry *deviceCacheEntry;
    icDeviceEndpoint *endpoint;
    icDeviceMetadata *metadata;
} MetadataLocator;

// The value structure for our URI hashmap
typedef struct
{
    LocatorType locatorType;
    union {
        DeviceCacheEntry *deviceCacheEntry;
        EndpointLocator endpointLocator;
        ResourceLocator resourceLocator;
        MetadataLocator metadataLocator;
    } locator;
} Locator;

// Map of devices by their uuid, this map "owns" all the devices and their resources
static icHashMap *devices = NULL;
// Map of "resources" by their uri, this includes devices, endpoints, resources, and metadata
// It only owns the Locator value that is stored in the map.
static icHashMap *resourcesByUri = NULL;
// Map of system properties, simple name value pairs
static icStringHashMap *systemProperties = NULL;
// Lock when we want to read or write our in memory cache.  The locking strategy of this class is
// static (private) functions assume the lock is held.   All public functions should lock before
// calling functions which manipulate global data structures
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static bool jsonDatabaseSetSystemPropertyNoLock(const char *key, const char *value);
static bool loadDeviceIntoCache(icDevice *newDevice);
static void removeDeviceURIEntries(const icDevice *device);

/**
 * Replace all resources on a device endpoint
 * @param dst The endpoint cache locator (authoritative copy) for the destination endpoing
 * @param src The endpoint containing the current resource set.
 */
static void replaceEndpointResources(EndpointLocator *dst, icDeviceEndpoint *src);

/**
 * Create a new cache entry
 *
 * @param device the device to put into the cache entry
 * @return the cache entry with dirty flag initialized to false
 */
static DeviceCacheEntry *createCacheEntry(icDevice *device)
{
    DeviceCacheEntry *entry = (DeviceCacheEntry *)calloc(1, sizeof(DeviceCacheEntry));
    entry->device = device;
    entry->dirty = false;

    return entry;
}

/**
 * Destroy a cache entry, includes removing any resources in our URI and destroying the
 * device itself
 *
 * @param entry the entry to destroy
 */
static void destroyCacheEntry(DeviceCacheEntry *entry)
{
    // Cleanup the resources map first
    removeDeviceURIEntries(entry->device);
    // Now clean up the device
    deviceDestroy(entry->device);
    // Now cleanup the cache entry itself
    free(entry);
}

/**
 * Convert a stringHashMap to JSON
 *
 * @param hashMap the hashmap to convert
 * @return the json object
 */
static cJSON *stringHashMapToJson(icStringHashMap *hashMap)
{
    cJSON *body = cJSON_CreateObject();
    icStringHashMapIterator *iter = stringHashMapIteratorCreate(hashMap);
    while(stringHashMapIteratorHasNext(iter))
    {
        char *key;
        char *value;
        stringHashMapIteratorGetNext(iter, &key, &value);
        cJSON_AddStringToObject(body, key, value);
     }
     stringHashMapIteratorDestroy(iter);

    return body;
}

/**
 * HashMap destroy function for our URI map.  Simply frees the Locator structure
 * @param key this will be the URI, which is not owned by this map
 * @param value the resource locator record, which can be freed
 */
static void destroyLocator(void *key, void *value)
{
    // Only the locator struct itself is owned, so free only it
    (void)key;
    free(value);
}

/**
 * Remove the URI entry for some metadata
 *
 * @param deviceMetadata the metadata
 */
static void removeMetadataURIEntry(const icDeviceMetadata *deviceMetadata)
{
    if (deviceMetadata != NULL)
    {
        // The map only owns the locator, so free that
        hashMapDelete(resourcesByUri, deviceMetadata->uri, strlen(deviceMetadata->uri) + 1, destroyLocator);
    }
}

/**
 * Remove the URI entry for a resource
 *
 * @param deviceResource the resource
 */
static void removeDeviceResourceURIEntry(const icDeviceResource *deviceResource)
{
    if (deviceResource != NULL && deviceResource->uri != NULL)
    {
        // The map only owns the locator, so free that
        hashMapDelete(resourcesByUri, deviceResource->uri, strlen(deviceResource->uri) + 1, destroyLocator);
    }
}

/**
 * Remove all URI entries for an endpoint
 *
 * @param endpoint the endpoint
 */
static void removeEndpointURIEntries(const icDeviceEndpoint *endpoint)
{
    if (endpoint != NULL && endpoint->uri != NULL)
    {
        // The map only owns the locator, so free that
        hashMapDelete(resourcesByUri, endpoint->uri, strlen(endpoint->uri) + 1, destroyLocator);
        // Remove all resource URI entries
        icLinkedListIterator *iter = linkedListIteratorCreate(endpoint->resources);
        while(linkedListIteratorHasNext(iter))
        {
            icDeviceResource *resource = (icDeviceResource *)linkedListIteratorGetNext(iter);
            removeDeviceResourceURIEntry(resource);
        }
        linkedListIteratorDestroy(iter);

        // Remove all metadata URI entries
        icLinkedListIterator *metadataIter = linkedListIteratorCreate(endpoint->metadata);
        while(linkedListIteratorHasNext(metadataIter))
        {
            icDeviceMetadata *metadata = (icDeviceMetadata *)linkedListIteratorGetNext(metadataIter);
            removeMetadataURIEntry(metadata);
        }
        linkedListIteratorDestroy(metadataIter);
    }
}

/**
 * Remove all URI entries for a device
 * @param device the device
 */
static void removeDeviceURIEntries(const icDevice *device)
{
    if (device != NULL && device->uri != NULL)
    {
        // The map only owns the locator, so free that
        hashMapDelete(resourcesByUri, device->uri, strlen(device->uri) + 1, destroyLocator);
        // Remove all endpoint URI entries
        icLinkedListIterator *iter = linkedListIteratorCreate(device->endpoints);
        while(linkedListIteratorHasNext(iter))
        {
            icDeviceEndpoint *endpoint = (icDeviceEndpoint *)linkedListIteratorGetNext(iter);
            removeEndpointURIEntries(endpoint);
        }
        linkedListIteratorDestroy(iter);

        // Remove all resource URI entries
        icLinkedListIterator *resourcesIter = linkedListIteratorCreate(device->resources);
        while(linkedListIteratorHasNext(resourcesIter))
        {
            icDeviceResource *resource = (icDeviceResource *)linkedListIteratorGetNext(resourcesIter);
            removeDeviceResourceURIEntry(resource);
        }
        linkedListIteratorDestroy(resourcesIter);

        // Remove all metadata URI entries
        icLinkedListIterator *metadatasIter = linkedListIteratorCreate(device->metadata);
        while(linkedListIteratorHasNext(metadatasIter))
        {
            icDeviceMetadata *metadata = (icDeviceMetadata *)linkedListIteratorGetNext(metadatasIter);
            removeMetadataURIEntry(metadata);
        }
        linkedListIteratorDestroy(metadatasIter);
    }
}

/**
 * Add a URI entry for some metadata
 *
 * @param deviceCacheEntry the device cache entry
 * @param endpoint the endpoint
 * @param deviceMetadata the metadata
 * @return true on success, failure otherwise
 */
static bool addDeviceMetadataURIEntry(DeviceCacheEntry *deviceCacheEntry, icDeviceEndpoint *endpoint, icDeviceMetadata *deviceMetadata)
{
    bool retval = true;
    Locator *locator = (Locator *) calloc(1, sizeof(Locator));
    locator->locatorType = LOCATOR_TYPE_METADATA;
    locator->locator.metadataLocator.deviceCacheEntry = deviceCacheEntry;
    locator->locator.metadataLocator.endpoint = endpoint;
    locator->locator.metadataLocator.metadata = deviceMetadata;
    if (!hashMapPut(resourcesByUri, deviceMetadata->uri, strlen(deviceMetadata->uri)+1, locator))
    {
        icLogError(LOG_TAG, "Failed to add metadata locator with uri %s", deviceMetadata->uri);
        free(locator);
        retval = false;
    }

    return retval;
}

/**
 * Add a URI entry for a resource
 *
 * @param deviceCacheEntry the device cache entry
 * @param endpoint the endpoint
 * @param deviceResource the resource
 * @return true on success, failure otherwise
 */
static bool addDeviceResourceURIEntry(DeviceCacheEntry *deviceCacheEntry, icDeviceEndpoint *endpoint, icDeviceResource *deviceResource)
{
    bool retval = true;
    if (deviceResource != NULL && deviceResource->uri != NULL)
    {
        Locator *locator = (Locator *) calloc(1, sizeof(Locator));
        locator->locatorType = LOCATOR_TYPE_RESOURCE;
        locator->locator.resourceLocator.deviceCacheEntry = deviceCacheEntry;
        locator->locator.resourceLocator.endpoint = endpoint;
        locator->locator.resourceLocator.resource = deviceResource;
        if (!hashMapPut(resourcesByUri, deviceResource->uri, strlen(deviceResource->uri) + 1, locator))
        {
            icLogError(LOG_TAG, "Failed to add resource locator with uri %s", deviceResource->uri);
            free(locator);
            retval = false;
        }
    }
    else
    {
        icLogError(LOG_TAG, "Cannot add NULL device resource or device resource with no uri");
        retval = false;
    }

    return retval;
}

/**
 * Add URI entries for an endpoint and all its contained items.
 *
 * @param deviceCacheEntry the device cache entry
 * @param endpoint the endpoint
 * @return true on success, false otherwise.
 */
static bool addEndpointURIEntries(DeviceCacheEntry *deviceCacheEntry, icDeviceEndpoint *endpoint)
{
    bool retval = true;
    if (endpoint != NULL && endpoint->uri != NULL)
    {
        Locator *locator = (Locator *) calloc(1, sizeof(Locator));
        locator->locatorType = LOCATOR_TYPE_ENDPOINT;
        locator->locator.endpointLocator.deviceCacheEntry = deviceCacheEntry;
        locator->locator.endpointLocator.endpoint = endpoint;
        if (hashMapPut(resourcesByUri, endpoint->uri, strlen(endpoint->uri) + 1, locator))
        {
            icLinkedListIterator *endpointResourcesIter = linkedListIteratorCreate(endpoint->resources);
            while (linkedListIteratorHasNext(endpointResourcesIter))
            {
                icDeviceResource *endpointResource = (icDeviceResource *) linkedListIteratorGetNext(
                        endpointResourcesIter);
                if (!addDeviceResourceURIEntry(deviceCacheEntry, endpoint, endpointResource))
                {
                    retval = false;
                    break;
                }
            }
            linkedListIteratorDestroy(endpointResourcesIter);

            if (retval)
            {
                icLinkedListIterator *metdataIter = linkedListIteratorCreate(endpoint->metadata);
                while (linkedListIteratorHasNext(metdataIter))
                {
                    icDeviceMetadata *metadata = (icDeviceMetadata *) linkedListIteratorGetNext(metdataIter);
                    if (!addDeviceMetadataURIEntry(deviceCacheEntry, endpoint, metadata))
                    {
                        retval = false;
                        break;
                    }
                }
                linkedListIteratorDestroy(metdataIter);
            }

        }
        else
        {
            icLogError(LOG_TAG, "Failed to add endpoint locator with uri %s", endpoint->uri);
            free(locator);
            retval = false;
        }
    }
    else
    {
        icLogError(LOG_TAG, "Failed to add NULL endpoint or endpoint with NULL uri");
        retval = false;
    }

    return retval;
}

/**
 * Add URI entries for a device and all its contained items.  Should be atomic on success/failure:
 * Either all are added on success, or none are added on failure
 *
 * @param deviceCacheEntry the device cache entry
 * @return true on success, false otherwise.
 */
static bool addDeviceURIEntries(DeviceCacheEntry *deviceCacheEntry)
{
    bool retval = true;
    if (deviceCacheEntry != NULL && deviceCacheEntry->device != NULL && deviceCacheEntry->device->uri != NULL)
    {
        Locator *locator = (Locator *) calloc(1, sizeof(Locator));
        locator->locatorType = LOCATOR_TYPE_DEVICE;
        locator->locator.deviceCacheEntry = deviceCacheEntry;
        if (hashMapPut(resourcesByUri, deviceCacheEntry->device->uri, strlen(deviceCacheEntry->device->uri) + 1, locator))
        {
            icLinkedListIterator *iter = linkedListIteratorCreate(deviceCacheEntry->device->endpoints);
            while (linkedListIteratorHasNext(iter))
            {
                icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(iter);
                if (!addEndpointURIEntries(deviceCacheEntry, endpoint))
                {
                    retval = false;
                    break;
                }

            }
            linkedListIteratorDestroy(iter);

            if (retval)
            {
                icLinkedListIterator *resourcesIter = linkedListIteratorCreate(deviceCacheEntry->device->resources);
                while (linkedListIteratorHasNext(resourcesIter))
                {
                    icDeviceResource *resource = (icDeviceResource *) linkedListIteratorGetNext(resourcesIter);
                    if (!addDeviceResourceURIEntry(deviceCacheEntry, NULL, resource))
                    {
                        retval = false;
                        break;
                    }
                }
                linkedListIteratorDestroy(resourcesIter);
            }

            if (retval)
            {
                icLinkedListIterator *metdataIter = linkedListIteratorCreate(deviceCacheEntry->device->metadata);
                while (linkedListIteratorHasNext(metdataIter))
                {
                    icDeviceMetadata *metadata = (icDeviceMetadata *) linkedListIteratorGetNext(metdataIter);
                    if (!addDeviceMetadataURIEntry(deviceCacheEntry, NULL, metadata))
                    {
                        retval = false;
                        break;
                    }
                }
                linkedListIteratorDestroy(metdataIter);
            }
        }
        else
        {
            icLogError(LOG_TAG, "Failed to add device locator with uri %s", deviceCacheEntry->device->uri);
            free(locator);
            retval = false;
        }
    }
    else
    {
        icLogError(LOG_TAG, "Failed to add device locator for NULL device or device without uri");
        retval = false;
    }

    if (!retval && deviceCacheEntry != NULL)
    {
        // Cleanup
        removeDeviceURIEntries(deviceCacheEntry->device);
    }

    return retval;

}

/**
 * Load system properties from storage
 * @return true if successful, false otherwise
 */
static bool loadSystemProperties()
{
    bool retval = false;
    if (systemProperties == NULL)
    {
        systemProperties = stringHashMapCreate();
    }

    char *value;
    if (storageLoad(STORAGE_NAMESPACE, SYSTEM_PROPERTIES_KEY, &value))
    {
        cJSON *body = cJSON_Parse(value);
        if (body != NULL)
        {
            int count = cJSON_GetArraySize(body);
            for(int i = 0; i < count; ++i)
            {
                cJSON *item = cJSON_GetArrayItem(body, i);
                if (item->type == cJSON_String && item->valuestring != NULL)
                {
                    stringHashMapPut(systemProperties, strdup(item->string), strdup(item->valuestring));
                }
                else
                {
                    // We will still say we are successful, but log the error.  Really not sure how this
                    // would ever happen
                    icLogWarn(LOG_TAG, "Skipping unreadable system property %s", item->string);
                }
            }
            retval = true;
        }
        else
        {
            icLogError(LOG_TAG, "Failed to parse system properties: %s", value);
        }
        // Cleanup
        cJSON_Delete(body);
    }
    // Cleanup
    free(value);

    return retval;
}

/**
 * Load a device from storage into our in-memory cache
 * @param jsonData a valid JSON document to load
 * @return whether the device was loaded into the cache successfully.
 */
static bool loadDevice(const char *jsonData, void *notUsed)
{
    icDevice *device = NULL;

    cJSON *json = cJSON_Parse(jsonData);
    if (json != NULL)
    {
        icSerDesContext *context = serDesCreateContext();
        serDesSetContextValue(context, "namespace", STORAGE_NAMESPACE);

        device = deviceFromJSON(json, context);

        if (!loadDeviceIntoCache(device))
        {
            // If loadDeviceIntoCache fails it cleans up the device as well, so return NULL to the caller
            device = NULL;
        }
        // Cleanup
        cJSON_Delete(json);
        serDesDestroyContext(context);
    }

    return device != NULL;
}

static const StorageCallbacks loadDeviceCallback = {
        .parse = loadDevice
};

/**
 * Load all devices from storage into our in memory cache
 *
 * @return true if success, false otherwise
 */
static bool loadDevices()
{
    bool retval = true;
    if (devices == NULL)
    {
        devices = hashMapCreate();
    }
    if (resourcesByUri == NULL)
    {
        resourcesByUri = hashMapCreate();
    }

    icLinkedList *keys = storageGetKeys(STORAGE_NAMESPACE);
    icLinkedListIterator *iter = linkedListIteratorCreate(keys);

    while(linkedListIteratorHasNext(iter))
    {
        char *key = (char *)linkedListIteratorGetNext(iter);
        if (key != NULL && strcmp(key, SYSTEM_PROPERTIES_KEY) != 0)
        {
            if (storageParse(STORAGE_NAMESPACE, key, &loadDeviceCallback) == false)
            {
                // For now if we fail to load the device for some reason, we will just log it and
                // move on.
                icLogWarn(LOG_TAG, "Failed to load device %s", key);
            }
        }
    }
    linkedListIteratorDestroy(iter);
    linkedListDestroy(keys, NULL);

    return retval;
}

/**
 * Flush system properties to storage
 * @return true if success, false otherwise
 */
static bool saveSystemProperties()
{
    bool didSave = true;
    cJSON *body = stringHashMapToJson(systemProperties);
    char *toWrite = cJSON_Print(body);
    if (!storageSave(STORAGE_NAMESPACE, SYSTEM_PROPERTIES_KEY, toWrite))
    {
        didSave = false;
        icLogError(LOG_TAG, "Failed to write system properties");
    }
    // Cleanup
    free(toWrite);
    cJSON_Delete(body);

    return didSave;
}

/**
 * Flush a device to storage
 *
 * @param device the device to write
 * @return true if success, false otherwise
 */
static bool saveDevice(icDevice *device)
{
    icSerDesContext *context = serDesCreateContext();
    bool didSave = true;

    serDesSetContextValue(context, "namespace", STORAGE_NAMESPACE);

    cJSON *body = deviceToJSON(device, context);
    char *toWrite = cJSON_Print(body);

    if (!storageSave(STORAGE_NAMESPACE, device->uuid, toWrite))
    {
        didSave = false;
        icLogError(LOG_TAG, "Failed to write device %s", device->uuid);
    }
    // Cleanup
    free(toWrite);
    cJSON_Delete(body);
    serDesDestroyContext(context);

    return didSave;
}

/**
 * Open or create our jsonDatabase.  Assumes caller owns the mutex
 *
 * @return true on success (either opening existing or creating new)
 */
static bool jsonDatabaseInitializeNoLock()
{
    bool retval = false;
    if (loadSystemProperties())
    {
        // Load devices
        retval = loadDevices();
    }
    else
    {
        // Intialize empty database
        devices = hashMapCreate();
        resourcesByUri = hashMapCreate();
        jsonDatabaseSetSystemPropertyNoLock(JSON_DATABASE_SCHEMA_VERSION_KEY,JSON_DATABASE_CURRENT_SCHEMA_VERSION);
        retval = true;
    }
    return retval;
}

/**
 * Open or create our jsonDatabase.
 *
 * @return true on success (either opening existing or creating new)
 */
bool jsonDatabaseInitialize()
{
    bool retval = false;
    // Take the lock to initialize things
    pthread_mutex_lock(&mtx);
    retval = jsonDatabaseInitializeNoLock();
    pthread_mutex_unlock(&mtx);

    return retval;
}

/**
 * HashMap free function for devices map
 * @param key the key
 * @param value the value
 */
static void freeDevicesItem(void *key, void *value)
{
    // The key is the device uuid stored within the actual device, so no need to free
    (void)key;
    destroyCacheEntry((DeviceCacheEntry *)value);
}

/**
 * Close the jsonDatabase and release any related resources.  Assumes caller owns the mutex
 *
 * @param persist Whether or not to persist everything while shutting down
 */
static void jsonDatabaseCleanupNoLock(bool persist)
{
    if (persist)
    {
        // Flush system properties to disk
        saveSystemProperties();
    }
    // Cleanup
    stringHashMapDestroy(systemProperties, NULL);
    systemProperties = NULL;
    // Flush devices to disk
    icHashMapIterator *iter = hashMapIteratorCreate(devices);
    while(hashMapIteratorHasNext(iter))
    {
        char *deviceUuid;
        uint16_t keyLen;
        DeviceCacheEntry *deviceCacheEntry;
        hashMapIteratorGetNext(iter, (void**)&deviceUuid, &keyLen, (void**)&deviceCacheEntry);

        if (persist)
        {
            if (deviceCacheEntry->dirty)
            {
                if (!saveDevice(deviceCacheEntry->device))
                {
                    icLogError(LOG_TAG, "%s: device save for %s failed!", deviceCacheEntry->device->uri, __FUNCTION__);
                }
            }
        }
    }
    hashMapIteratorDestroy(iter);
    hashMapDestroy(devices, freeDevicesItem);
    devices = NULL;
    // Should be nothing left, but to clean up the map itself
    hashMapDestroy(resourcesByUri, NULL);
    resourcesByUri = NULL;
}

/**
 * Close the jsonDatabase and release any related resources.
 *
 * @param persist Whether or not to persist everything while shutting down
 */
void jsonDatabaseCleanup(bool persist)
{
    pthread_mutex_lock(&mtx);
    jsonDatabaseCleanupNoLock(persist);
    pthread_mutex_unlock(&mtx);
}

/**
 * Reload the database from storage without flushing the current contents.  Basically
 * the equivalent of calling jsonDatabaseCleanup(false), and then jsonDatabaseInitialize(),
 * but this method is atomic under a lock to prevent races
 *
 * @return true on success
 */
bool jsonDatabaseReload()
{
    bool retval = false;
    pthread_mutex_lock(&mtx);
    // Cleanup without pushing contents out to stoarge
    jsonDatabaseCleanupNoLock(false);
    // Re-initialize based on what's in storage
    retval = jsonDatabaseInitializeNoLock();
    pthread_mutex_unlock(&mtx);

    return retval;
}

/**
 * Reload the database from storage without flushing the current contents.  Basically
 * the equivalent of calling jsonDatabaseCleanup(false), and then jsonDatabaseInitialize(),
 * but this method is atomic under a lock to prevent races
 *
 * @return true on success
 */
bool jsonDatabaseRestore(const char* tempRestoreDir, const char *dynamicConfigPath)
{
    bool retval = false;
    pthread_mutex_lock(&mtx);
    // Cleanup without pushing contents out to stoarge
    jsonDatabaseCleanupNoLock(false);

    // Restore the configuration. The current namespace will
    // be deleted automatically.
    retval = storageRestoreNamespace(STORAGE_NAMESPACE, tempRestoreDir);

    // Re-initialize based on what's in storage
    retval = (retval && jsonDatabaseInitializeNoLock());
    pthread_mutex_unlock(&mtx);

    (void) dynamicConfigPath;

    return retval;
}

/**
 * Retrieve a system property by name.
 *
 * @param key the name of the system property
 * @param value the property value or NULL
 *
 * @param caller frees value output
 *
 * @return true on success
 */
bool jsonDatabaseGetSystemProperty(const char *key, char **value)
{
    bool didGet = false;
    pthread_mutex_lock(&mtx);
    char *propertyValue = stringHashMapGet(systemProperties, key);
    if (propertyValue != NULL)
    {
        *value = strdup(propertyValue);
        didGet = true;
    }
    pthread_mutex_unlock(&mtx);

    return didGet;
}

/**
 * Private method to set a property, assumes caller holds the lock
 *
 * @param key the key
 * @param value the value
 * @return true on success
 */
static bool jsonDatabaseSetSystemPropertyNoLock(const char *key, const char *value)
{
    bool retval = false;

    // Clear out an existing value, if one exists
    stringHashMapDelete(systemProperties, key, NULL);
    // Put it in the map
    if (stringHashMapPutCopy(systemProperties, key, value))
    {
        retval = saveSystemProperties();
    }
    else
    {
        icLogWarn(LOG_TAG, "%s: Failed to write property %s", __func__, key);
    }

    return retval;
}

/**
 * Set a system property
 *
 * @param key the name of the system property
 * @param value the property value
 *
 * @return true on success
 */
bool jsonDatabaseSetSystemProperty(const char *key, const char *value)
{
    pthread_mutex_lock(&mtx);
    bool retval = jsonDatabaseSetSystemPropertyNoLock(key, value);
    pthread_mutex_unlock(&mtx);

    return retval;
}

/**
 * Load a device into our devices map cache, and create its URI entries.  This function takes ownership of the passed
 * device object and will destroy it on failure, or otherwise it will be owned by the devices map
 *
 * @param newDevice the device to add
 * @return true on success, false otherwise.
 */
static bool loadDeviceIntoCache(icDevice *newDevice)
{
    bool retval = false;

    if (!newDevice || !newDevice->uuid)
    {
        icLogError(LOG_TAG, "Unable to load a NULL device or a device with a NULL uuid!");
        return false;
    }

    DeviceCacheEntry *cacheEntry = createCacheEntry(newDevice);

    // Put it in the id lookup map and the uri lookup map
    if (hashMapPut(devices, newDevice->uuid, strlen(newDevice->uuid) + 1, cacheEntry))
    {
        if (addDeviceURIEntries(cacheEntry))
        {
            retval = true;
        }
        else
        {
            icLogError(LOG_TAG, "Failed to add device resources for device %s", newDevice->uuid);
            retval = false;
            hashMapDelete(devices, newDevice->uuid, strlen(newDevice->uuid)+1, freeDevicesItem);
        }
    }
    else
    {
        icLogError(LOG_TAG, "Failed to create device entry for device %s", newDevice->uuid);
        // If we failed go ahead and clean up
        destroyCacheEntry(cacheEntry);
    }

    return retval;
}

/**
 * Add a new device to the jsonDatabase.
 *
 * @param device the fully populated icDevice structure to insert into the jsonDatabase. Caller
 * still owns the device structure when this call completes and is responsible for destroying it
 *
 * @return true on success
 */
bool jsonDatabaseAddDevice(icDevice *device)
{
    bool retval = false;
    if (device != NULL && device->uuid != NULL)
    {
        icDevice *newDevice = deviceClone(device);
        pthread_mutex_lock(&mtx);
        if (loadDeviceIntoCache(newDevice))
        {
            if (!saveDevice(newDevice))
            {
                // TODO is this the right way to handle this?
                icLogError(LOG_TAG, "Failed to persist device, removing device");
                // Clean up
                hashMapDelete(devices, newDevice->uuid, strlen(newDevice->uuid) + 1, freeDevicesItem);
            }
            else
            {
                retval = true;
            }
        }
        // If loading the device into the cache fails it will clean up the device
        pthread_mutex_unlock(&mtx);
    }
    else
    {
        icLogWarn(LOG_TAG, "Cannot add NULL device or device with NULL uuid");
    }

    return retval;
}

bool jsonDatabaseAddEndpoint(icDeviceEndpoint *endpoint)
{
    bool retval = false;
    if (endpoint != NULL && endpoint->deviceUuid != NULL && endpoint->id != NULL)
    {
        icDeviceEndpoint *newEndpoint = endpointClone(endpoint);

        pthread_mutex_lock(&mtx);
        DeviceCacheEntry *deviceCacheEntry = (DeviceCacheEntry *)hashMapGet(devices,
                                                                            (void *)endpoint->deviceUuid,
                                                                            strlen(endpoint->deviceUuid)+1);
        if(deviceCacheEntry != NULL)
        {
            linkedListAppend(deviceCacheEntry->device->endpoints, newEndpoint);

            if (addEndpointURIEntries(deviceCacheEntry, newEndpoint))
            {
                if (saveDevice(deviceCacheEntry->device))
                {
                    retval = true;
                }
                else
                {
                    icLogError(LOG_TAG, "Failed to save device after adding endpoint");
                }
            }
            else
            {
                icLogError(LOG_TAG, "Failed to add endpoint resources for endpoint %s on device %s", endpoint->id,
                           endpoint->deviceUuid);
                retval = false;

                endpointDestroy(newEndpoint);
            }
        }
        else
        {
            endpointDestroy(newEndpoint);
            icLogError(LOG_TAG, "Failed to locate device entry for device %s", endpoint->deviceUuid);
        }

        pthread_mutex_unlock(&mtx);
    }
    else
    {
        icLogWarn(LOG_TAG, "Cannot add NULL endpoint or endpoint with NULL id");
    }

    return retval;
}

/**
 * Retrieve all devices in the jsonDatabase.
 *
 * @return linked list of all devices, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see deviceDestroy
 *
 */
icLinkedList *jsonDatabaseGetDevices()
{
    pthread_mutex_lock(&mtx);
    icLinkedList *devicesCopy = linkedListCreate();
    icHashMapIterator *iter = hashMapIteratorCreate(devices);
    while(hashMapIteratorHasNext(iter))
    {
        char *key;
        uint16_t keyLen;
        DeviceCacheEntry *deviceCacheEntry;
        hashMapIteratorGetNext(iter, (void**)&key, &keyLen, (void**)&deviceCacheEntry);
        linkedListAppend(devicesCopy, deviceClone(deviceCacheEntry->device));
    }
    hashMapIteratorDestroy(iter);
    pthread_mutex_unlock(&mtx);

    return devicesCopy;
}

/**
 * Retrieve all devices that have an endpoint with the given profile
 *
 * @param profileId the profile
 * @return linked list of devices, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see deviceDestroy
 */
icLinkedList *jsonDatabaseGetDevicesByEndpointProfile(const char *profileId)
{
    icLinkedList *foundDevices = linkedListCreate();
    if (profileId != NULL)
    {
        pthread_mutex_lock(&mtx);
        icHashMapIterator *iter = hashMapIteratorCreate(devices);
        while (hashMapIteratorHasNext(iter))
        {
            char *key;
            uint16_t keyLen;
            DeviceCacheEntry *deviceCacheEntry;
            hashMapIteratorGetNext(iter, (void **) &key, &keyLen, (void **) &deviceCacheEntry);
            icLinkedListIterator *deviceEndpointsIter = linkedListIteratorCreate(deviceCacheEntry->device->endpoints);
            while (linkedListIteratorHasNext(deviceEndpointsIter))
            {
                icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(deviceEndpointsIter);
                if (endpoint->profile != NULL && strcmp(endpoint->profile, profileId) == 0)
                {
                    linkedListAppend(foundDevices, deviceClone(deviceCacheEntry->device));
                    // Break out of endpoints and move on to the next device
                    break;
                }
            }
            linkedListIteratorDestroy(deviceEndpointsIter);
        }
        hashMapIteratorDestroy(iter);
        pthread_mutex_unlock(&mtx);
    }

    return foundDevices;

}

/**
 * Retrieve all devices that have an endpoint with the given device class
 *
 * @param deviceClass the device class
 * @return linked list of devices, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see deviceDestroy
 */
icLinkedList *jsonDatabaseGetDevicesByDeviceClass(const char *deviceClass)
{
    icLinkedList *foundDevices = linkedListCreate();
    if (deviceClass != NULL)
    {
        pthread_mutex_lock(&mtx);
        icHashMapIterator *iter = hashMapIteratorCreate(devices);
        while (hashMapIteratorHasNext(iter))
        {
            char *key;
            uint16_t keyLen;
            DeviceCacheEntry *deviceCacheEntry;
            hashMapIteratorGetNext(iter, (void **) &key, &keyLen, (void **) &deviceCacheEntry);
            if (deviceCacheEntry->device->deviceClass != NULL &&
                strcmp(deviceClass, deviceCacheEntry->device->deviceClass) == 0)
            {
                linkedListAppend(foundDevices, deviceClone(deviceCacheEntry->device));
            }
        }
        hashMapIteratorDestroy(iter);
        pthread_mutex_unlock(&mtx);
    }


    return foundDevices;
}

/**
 * Retrieve all devices that have an endpoint with the given device driver
 *
 * @param driverName the device driver name
 * @return linked list of devices, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see deviceDestroy
 */
icLinkedList *jsonDatabaseGetDevicesByDeviceDriver(const char *deviceDriverName)
{
    icLinkedList *foundDevices = linkedListCreate();
    if (deviceDriverName != NULL)
    {
        pthread_mutex_lock(&mtx);
        icHashMapIterator *iter = hashMapIteratorCreate(devices);
        while (hashMapIteratorHasNext(iter))
        {
            char *key;
            uint16_t keyLen;
            DeviceCacheEntry *deviceCacheEntry;
            hashMapIteratorGetNext(iter, (void **) &key, &keyLen, (void **) &deviceCacheEntry);
            if (deviceCacheEntry->device->managingDeviceDriver != NULL &&
                strcmp(deviceDriverName, deviceCacheEntry->device->managingDeviceDriver) == 0)
            {
                linkedListAppend(foundDevices, deviceClone(deviceCacheEntry->device));
            }
        }
        hashMapIteratorDestroy(iter);
        pthread_mutex_unlock(&mtx);
    }

    return foundDevices;
}

/**
 * Retrieve a device by its UUID
 *
 * @param uuid the uuid of the device
 * @return the device or NULL if not found.  Caller is responsible for destroying
 *
 * @see deviceDestroy
 */
icDevice *jsonDatabaseGetDeviceById(const char *uuid)
{
    icDevice *device = NULL;
    if (uuid != NULL)
    {
        pthread_mutex_lock(&mtx);
        DeviceCacheEntry *deviceCacheEntry = (DeviceCacheEntry *)hashMapGet(devices, (void *)uuid, strlen(uuid)+1);
        if (deviceCacheEntry != NULL)
        {
            device = deviceClone(deviceCacheEntry->device);
        }
        pthread_mutex_unlock(&mtx);
    }

    return device;
}

/**
 * Retrieve a device by its URI.  Can pass in an endpoint, resource, or metdata uri
 * and will return the owner device
 *
 * @param uri the uri of the device
 * @return the device or NULL if not found.  Caller is responsible for destroying
 *
 * @see deviceDestroy
 */
icDevice *jsonDatabaseGetDeviceByUri(const char *uri)
{
    icDevice *foundDevice = NULL;
    if (uri != NULL)
    {
        pthread_mutex_lock(&mtx);
        Locator *locator = (Locator *)hashMapGet(resourcesByUri, (void *)uri, strlen(uri)+1);
        if (locator != NULL)
        {
            if (locator->locatorType == LOCATOR_TYPE_DEVICE)
            {
                foundDevice = deviceClone(locator->locator.deviceCacheEntry->device);
            }
            else if (locator->locatorType == LOCATOR_TYPE_ENDPOINT)
            {
                foundDevice = deviceClone(locator->locator.endpointLocator.deviceCacheEntry->device);
            }
            else if (locator->locatorType == LOCATOR_TYPE_RESOURCE)
            {
                foundDevice = deviceClone(locator->locator.resourceLocator.deviceCacheEntry->device);
            }
            else if (locator->locatorType == LOCATOR_TYPE_METADATA)
            {
                foundDevice = deviceClone(locator->locator.metadataLocator.deviceCacheEntry->device);
            }
            else
            {
                icLogWarn(LOG_TAG, "%s: Unknown locator type found %d", __FUNCTION__, locator->locatorType);
            }
        }
        pthread_mutex_unlock(&mtx);
    }

    return foundDevice;
}

/**
 * Check if the provided device uuid is known to our database.
 *
 * @param uuid the uuid of the device
 * @return true if the device is known (in the database)
 */
bool jsonDatabaseIsDeviceKnown(const char *uuid)
{
    bool retval = false;
    if (uuid != NULL)
    {
        pthread_mutex_lock(&mtx);
        retval = hashMapContains(devices, (void *)uuid, strlen(uuid)+1);
        pthread_mutex_unlock(&mtx);
    }

    return retval;
}

/**
 * Remove a device
 *
 * @param uuid the uuid of the device
 * @return true if successful, false otherwise
 */
bool jsonDatabaseRemoveDeviceById(const char *uuid)
{
    bool retval = false;
    if (uuid != NULL)
    {
        pthread_mutex_lock(&mtx);
        DeviceCacheEntry *cacheEntry = hashMapGet(devices, (void *)uuid, strlen(uuid)+1);
        if (cacheEntry != NULL)
        {
            if (storageDelete(STORAGE_NAMESPACE, uuid))
            {
                // Clean out of id map, which will free all resources
                hashMapDelete(devices, (void *)uuid, strlen(uuid)+1, (hashMapFreeFunc)freeDevicesItem);
                retval = true;
            }
            else
            {
                icLogError(LOG_TAG, "Failed to remove storage for device %s", uuid);
            }
        }
        pthread_mutex_unlock(&mtx);
    }

    return retval;
}

//Endpoints
/**
 * Retrieve all endpoints  with the given device profile
 *
 * @param profileId the endpoint profile
 * @return linked list of endpoints, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see endpointDestroy
 */
icLinkedList *jsonDatabaseGetEndpointsByProfile(const char *profileId)
{
    icLinkedList *endpoints = linkedListCreate();
    if (profileId != NULL)
    {
        pthread_mutex_lock(&mtx);
        icHashMapIterator *iter = hashMapIteratorCreate(devices);
        while(hashMapIteratorHasNext(iter))
        {
            char *key;
            uint16_t keyLen;
            DeviceCacheEntry *deviceCacheEntry;
            hashMapIteratorGetNext(iter, (void**)&key, &keyLen, (void**)&deviceCacheEntry);
            icLinkedListIterator *endpointsIter = linkedListIteratorCreate(deviceCacheEntry->device->endpoints);
            while(linkedListIteratorHasNext(endpointsIter))
            {
                icDeviceEndpoint *endpoint = (icDeviceEndpoint *)linkedListIteratorGetNext(endpointsIter);
                if (endpoint->profile != NULL && strcmp(endpoint->profile, profileId) == 0)
                {
                    linkedListAppend(endpoints, endpointClone(endpoint));
                }
            }
            linkedListIteratorDestroy(endpointsIter);

        }
        hashMapIteratorDestroy(iter);
        pthread_mutex_unlock(&mtx);
    }

    return endpoints;
}

/**
 * Retrieve an endpoint by its id
 *
 * @param deviceUuid the device the endpoint belongs to
 * @param endpointId the id of the endpoint
 * @return the endpoint, or NULL if its not found.  Caller is responsible for destroying
 *
 * @see endpointDestroy
 */
icDeviceEndpoint *jsonDatabaseGetEndpointById(const char *deviceUuid, const char *endpointId)
{
    icDeviceEndpoint *foundEndpoint = NULL;
    if (endpointId != NULL)
    {
        pthread_mutex_lock(&mtx);
        DeviceCacheEntry *deviceCacheEntry = hashMapGet(devices, (void *)deviceUuid, strlen(deviceUuid)+1);
        if (deviceCacheEntry != NULL)
        {
            icLinkedListIterator *endpointsIter = linkedListIteratorCreate(deviceCacheEntry->device->endpoints);
            while (linkedListIteratorHasNext(endpointsIter))
            {
                icDeviceEndpoint *endpoint = (icDeviceEndpoint *) linkedListIteratorGetNext(endpointsIter);
                if (endpoint->id != NULL && strcmp(endpoint->id, endpointId) == 0)
                {
                    foundEndpoint = endpointClone(endpoint);
                    break;
                }
            }
            linkedListIteratorDestroy(endpointsIter);
        }
        pthread_mutex_unlock(&mtx);
    }

    return foundEndpoint;
}

/**
 * Retrieve an endpoint by its uri
 *
 * @param uri the uri of the endpoint
 * @return the endpoint, or NULL if its not found.  Caller is responsible for destroying
 *
 * @see endpointDestroy
 */
icDeviceEndpoint *jsonDatabaseGetEndpointByUri(const char *uri)
{
    icDeviceEndpoint *endpoint = NULL;
    if (uri != NULL)
    {
        pthread_mutex_lock(&mtx);
        Locator *locator = (Locator *)hashMapGet(resourcesByUri, (void *)uri, strlen(uri)+1);
        if (locator != NULL)
        {
            if (locator->locatorType == LOCATOR_TYPE_ENDPOINT)
            {
                endpoint = endpointClone(locator->locator.endpointLocator.endpoint);
            }
            else if (locator->locatorType == LOCATOR_TYPE_RESOURCE)
            {
                if (locator->locator.resourceLocator.endpoint != NULL)
                {
                    endpoint = endpointClone(locator->locator.resourceLocator.endpoint);
                }
            }
            else if (locator->locatorType == LOCATOR_TYPE_METADATA)
            {
                endpoint = endpointClone(locator->locator.metadataLocator.endpoint);
            }
            else
            {
                icLogWarn(LOG_TAG, "%s: Found invalid locator type %d when looking up endpoint by uri %s",
                          __FUNCTION__, locator->locatorType, uri);
            }
        }
        pthread_mutex_unlock(&mtx);
    }

    return endpoint;
}

/**
 * Update an endpoint in the database.  Currently only its enabled flag and resource set is updated.
 *
 * @param endpoint the endpoint to update
 * @return true if success, false otherwise
 */
bool jsonDatabaseSaveEndpoint(icDeviceEndpoint *endpoint)
{
    bool retval = false;

    if (endpoint != NULL && endpoint->uri != NULL)
    {
        pthread_mutex_lock(&mtx);
        // Take out the bits we can update and update our internal cache
        Locator *locator = (Locator *) hashMapGet(resourcesByUri, endpoint->uri,
                                                  strlen(endpoint->uri) + 1);
        if (locator != NULL)
        {
            if (locator->locatorType == LOCATOR_TYPE_ENDPOINT)
            {
                icDeviceEndpoint *dbEndpoint = locator->locator.endpointLocator.endpoint;
                // Update everything that makes sense(only enabled flag right now)
                dbEndpoint->enabled = endpoint->enabled;

                DeviceCacheEntry *entry = locator->locator.endpointLocator.deviceCacheEntry;

                // Replace resources when changing profile versions
                // TODO: This business logic shouldn't be here. It is purely defensive; a more general purpose
                // way to just tell the db to save an existing device is likely what we really want.
                //
                if (dbEndpoint->profileVersion != endpoint->profileVersion)
                {
                    replaceEndpointResources(&locator->locator.endpointLocator, endpoint);
                    dbEndpoint->profileVersion = endpoint->profileVersion;
                }

                // Write out
                entry->dirty = true;
                retval = saveDevice(entry->device);
                if (retval)
                {
                    entry->dirty = false;
                }

            }
        }
        pthread_mutex_unlock(&mtx);
    }

    return retval;
}


//Resources
/**
 * Retrieve a resource by its uri
 *
 * @param uri the uri of the resource
 * @return the resource, or NULL if its not found.  Caller is responsible for destroying
 *
 * @see resourceDestroy
 */
icDeviceResource *jsonDatabaseGetResourceByUri(const char *uri)
{
    icDeviceResource *resource = NULL;
    if (uri != NULL)
    {
        pthread_mutex_lock(&mtx);
        Locator *locator = (Locator *)hashMapGet(resourcesByUri, (void *)uri, strlen(uri)+1);
        if (locator != NULL && locator->locatorType == LOCATOR_TYPE_RESOURCE)
        {
            resource = resourceClone(locator->locator.resourceLocator.resource);
        }
        pthread_mutex_unlock(&mtx);
    }

    return resource;
}

/**
 * Update a resource in the database.  The following properties can be updated:
 * value
 * cachingPolicy
 * mode
 * dateOfLastSyncMillis
 *
 * @param endpoint the resource to update
 * @return true if success, false otherwise
 */
bool jsonDatabaseSaveResource(icDeviceResource *resource)
{
    bool retval = false;
    if (resource != NULL && resource->uri != NULL)
    {
        pthread_mutex_lock(&mtx);
        // Take out the bits we can update and update our internal cache
        Locator *locator = (Locator *)hashMapGet(resourcesByUri, resource->uri, strlen(resource->uri)+1);
        if (locator != NULL && locator->locatorType == LOCATOR_TYPE_RESOURCE)
        {
            icDeviceResource *dbResource = locator->locator.resourceLocator.resource;
            // Update everything that makes sense
            free(dbResource->value);
            dbResource->value = resource->value != NULL ? strdup(resource->value) : NULL;
            dbResource->cachingPolicy = resource->cachingPolicy;
            dbResource->mode = resource->mode;
            dbResource->dateOfLastSyncMillis = resource->dateOfLastSyncMillis;
            // Write out
            DeviceCacheEntry *entry = locator->locator.resourceLocator.deviceCacheEntry;
            entry->dirty = true;

            //If this is a lazy save resource, dont flush to storage yet
            if((resource->mode & RESOURCE_MODE_LAZY_SAVE_NEXT) == 0)
            {
                retval = saveDevice(entry->device);
                if (retval)
                {
                    entry->dirty = false;
                }
            }
            else
            {
                //a lazy save is successful
                retval = true;
            }
        }
        pthread_mutex_unlock(&mtx);
    }

    return retval;
}

/**
 * Update the dateOfLastSyncMillis of a resource.  Note that this is a lazy save and does not write to storage
 * immediately.
 *
 * @param resource the resource to update
 * @return true if success, false otherwise
 */
bool jsonDatabaseUpdateDateOfLastSyncMillis(icDeviceResource *resource)
{
    bool retval = false;
    if (resource != NULL && resource->uri != NULL)
    {
        pthread_mutex_lock(&mtx);
        // Take out the bits we can update and update our internal cache
        Locator *locator = (Locator *)hashMapGet(resourcesByUri, resource->uri, strlen(resource->uri)+1);
        if (locator != NULL && locator->locatorType == LOCATOR_TYPE_RESOURCE)
        {
            icDeviceResource *dbResource = locator->locator.resourceLocator.resource;
            // Update the dateOfLastSyncMillis to 'now'
            dbResource->dateOfLastSyncMillis = getCurrentUnixTimeMillis();
            // Write out
            DeviceCacheEntry *entry = locator->locator.resourceLocator.deviceCacheEntry;
            entry->dirty = true;

            // This is a lazy save resource so dont flush to storage yet
            retval = true;
        }
        pthread_mutex_unlock(&mtx);
    }

    return retval;
}

static void replaceEndpointResources(EndpointLocator *dst, icDeviceEndpoint *src)
{
    icLinkedListIterator *resIt = linkedListIteratorCreate(dst->endpoint->resources);
    while (linkedListIteratorHasNext(resIt) == true)
    {
        icDeviceResource *res = linkedListIteratorGetNext(resIt);
        removeDeviceResourceURIEntry(res);
    }
    linkedListIteratorDestroy(resIt);
    linkedListDestroy(dst->endpoint->resources, (linkedListItemFreeFunc) resourceDestroy);

    dst->endpoint->resources = linkedListCreate();

    resIt = linkedListIteratorCreate(src->resources);
    while (linkedListIteratorHasNext(resIt) == true)
    {
        icDeviceResource *res = resourceClone(linkedListIteratorGetNext(resIt));
        linkedListAppend(dst->endpoint->resources, res);
        addDeviceResourceURIEntry(dst->deviceCacheEntry, dst->endpoint, res);
    }
    linkedListIteratorDestroy(resIt);
}

//Metadata
/**
 * Retrieve a metadata by its uri
 *
 * @param uri the uri of the metadata
 * @return the metadata, or NULL if its not found.  Caller is responsible for destroying
 *
 * @see metadataDestroy
 */
icDeviceMetadata *jsonDatabaseGetMetadataByUri(const char *uri)
{
    icDeviceMetadata *metadata = NULL;
    if (uri != NULL)
    {
        pthread_mutex_lock(&mtx);
        Locator *locator = (Locator *)hashMapGet(resourcesByUri, (void *)uri, strlen(uri)+1);
        if (locator != NULL && locator->locatorType == LOCATOR_TYPE_METADATA)
        {
            metadata = metadataClone(locator->locator.metadataLocator.metadata);
        }
        pthread_mutex_unlock(&mtx);
    }

    return metadata;
}

/**
 * Create or update a metadata in the database.  The value is the only property that can be updated.
 *
 * @param endpoint the metadata to update
 * @return true if success, false otherwise
 */
bool jsonDatabaseSaveMetadata(icDeviceMetadata *metadata)
{
    bool retval = false;
    if (metadata != NULL && metadata->uri != NULL)
    {
        pthread_mutex_lock(&mtx);
        DeviceCacheEntry *entry = NULL;
        // Copy over the pieces we can update in our internal cache
        Locator *locator = (Locator *)hashMapGet(resourcesByUri, metadata->uri, strlen(metadata->uri)+1);
        if (locator != NULL)
        {
            if (locator->locatorType == LOCATOR_TYPE_METADATA)
            {
                icDeviceMetadata *dbMetadata = locator->locator.metadataLocator.metadata;
                free(dbMetadata->value);
                dbMetadata->value = strdup(metadata->value);
                // Mark the device dirty and save
                entry = locator->locator.metadataLocator.deviceCacheEntry;

            }
        }
        else
        {
            // Create new metadata
            entry = hashMapGet(devices, metadata->deviceUuid,
                               strlen(metadata->deviceUuid) + 1);
            if (entry != NULL && metadata->endpointId != NULL)
            {
                // Add endpoint metadata
                icLinkedListIterator *iter = linkedListIteratorCreate(entry->device->endpoints);
                while(linkedListIteratorHasNext(iter))
                {
                    icDeviceEndpoint *endpoint = (icDeviceEndpoint *)linkedListIteratorGetNext(iter);
                    if (strcmp(endpoint->id, metadata->endpointId) == 0)
                    {
                        icDeviceMetadata *newMetadata = metadataClone(metadata);
                        linkedListAppend(endpoint->metadata, newMetadata);
                        if (!addDeviceMetadataURIEntry(entry, endpoint, newMetadata))
                        {
                            // Null out entry so we return false
                            entry = NULL;
                        }
                        break;
                    }
                }
                linkedListIteratorDestroy(iter);
            }
            else if (entry != NULL)
            {
                // Add device metadata
                icDeviceMetadata *newMetadata = metadataClone(metadata);
                linkedListAppend(entry->device->metadata, newMetadata);
                if (!addDeviceMetadataURIEntry(entry, NULL, newMetadata))
                {
                    // Null out entry so we return false
                    entry = NULL;
                }
            }
        }

        // Flush the change if we did something
        if (entry != NULL)
        {
            entry->dirty = true;
            retval = saveDevice(entry->device);
            if (retval)
            {
                entry->dirty = false;
            }
        }

        pthread_mutex_unlock(&mtx);
    }

    return retval;
}

/**
 * Get a list of items based on a uri regex of the specified type
 * @param uriRegex the regex to match
 * @param locatorType the type of items to get(devices, endpoints, resources, metadata)
 * @return the list of items
 */
static icLinkedList *getItemsByUriRegex(const char *uriRegex, LocatorType locatorType)
{
    icLinkedList *items = linkedListCreate();
    if (uriRegex != NULL)
    {
        regex_t regex;

        int ret = regcomp(&regex, uriRegex, 0);

        if (ret == 0)
        {
            pthread_mutex_lock(&mtx);
            icHashMapIterator *iter = hashMapIteratorCreate(resourcesByUri);
            while(hashMapIteratorHasNext(iter))
            {
                char *key;
                uint16_t keyLen;
                Locator *value;
                hashMapIteratorGetNext(iter, (void**)&key, &keyLen, (void**)&value);
                if (value->locatorType == locatorType)
                {
                    ret = regexec(&regex, key, 0, NULL, 0);
                    if (ret == 0)
                    {
                        switch (locatorType)
                        {

                            case LOCATOR_TYPE_DEVICE:
                                linkedListAppend(items, deviceClone(value->locator.deviceCacheEntry->device));
                                break;
                            case LOCATOR_TYPE_ENDPOINT:
                                linkedListAppend(items, endpointClone(value->locator.endpointLocator.endpoint));
                                break;
                            case LOCATOR_TYPE_RESOURCE:
                                linkedListAppend(items, resourceClone(value->locator.resourceLocator.resource));
                                break;
                            case LOCATOR_TYPE_METADATA:
                                linkedListAppend(items, metadataClone(value->locator.metadataLocator.metadata));
                                break;
                            default:
                                icLogError(LOG_TAG, "Unknown locator type value %d", (int)locatorType);
                                break;
                        }

                    }
                }
            }
            hashMapIteratorDestroy(iter);
            pthread_mutex_unlock(&mtx);

            // Cleanup
            regfree(&regex);
        }
        else
        {
            icLogDebug(LOG_TAG, "Got invalid regex for querying items %s", uriRegex);
        }
    }
    return items;
}

/**
 * Get a list of resources matching the given regex
 *
 * @param uriRegex the regex to match
 * @return the resources that match the regex
 */
icLinkedList *jsonDatabaseGetResourcesByUriRegex(const char *uriRegex)
{
    return getItemsByUriRegex(uriRegex, LOCATOR_TYPE_RESOURCE);
}

/**
 * Get a list of metadata matching the given regex
 *
 * @param uriRegex the regex to match
 * @return the metadata that match the regex
 */
icLinkedList *jsonDatabaseGetMetadataByUriRegex(const char *uriRegex)
{
    return getItemsByUriRegex(uriRegex, LOCATOR_TYPE_METADATA);
}

