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
// Created by mkoch201 on 5/4/18.
//

#ifndef ZILKER_JSONDATABASE_H
#define ZILKER_JSONDATABASE_H


#include <icTypes/icLinkedList.h>
#include <device/icDevice.h>
#include <device/icDeviceResource.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceMetadata.h>

#define JSON_DATABASE_CURRENT_SCHEMA_VERSION "1"
#define JSON_DATABASE_SCHEMA_VERSION_KEY "schemaVersion"

/**
 * Open or create our jsonDatabase.
 *
 * @return true on success (either opening existing or creating new)
 */
bool jsonDatabaseInitialize();

/**
 * Close the jsonDatabase and release any related resources.
 *
 * @param persist Whether or not to persist everything while shutting down
 */
void jsonDatabaseCleanup(bool persist);

/**
 * Reload the database from storage without flushing the current contents.  Basically
 * the equivalent of calling jsonDatabaseCleanup(false), and then jsonDatabaseInitialize(),
 * but this method is atomic under a lock to prevent races
 *
 * @return true on success
 */
bool jsonDatabaseReload();

/**
 * Restore a database from a previous backup without flushing the current contents.
 * Basically the equivalent of calling jsonDatabaseCleanup(false), and then jsonDatabaseInitialize(),
 * but this method is atomic under a lock to prevent races.
 *
 * @param tempRestoreDir The configuration directory to restore from.
 * @param dynamicConfigPath The configuration directory to restore to.
 * @return True on success.
 */
bool jsonDatabaseRestore(const char* tempRestoreDir, const char *dynamicConfigPath);

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
bool jsonDatabaseGetSystemProperty(const char *key, char **value);

/**
 * Set a system property
 *
 * @param key the name of the system property
 * @param value the property value
 *
 * @return true on success
 */
bool jsonDatabaseSetSystemProperty(const char *key, const char *value);

/**
 * Add a new device to the jsonDatabase.
 *
 * @param device the fully populated icDevice structure to insert into the jsonDatabase. Caller
 * still owns this structure when this call completes and is responsible for destroying it
 *
 * @return true on success
 */
bool jsonDatabaseAddDevice(icDevice *device);

/**
 * Add a new endpoint to the jsonDatabase.
 *
 * @param endpoint the fully populated icDeviceEndpoint structure to insert into the jsonDatabase. Caller
 * still owns this structure when this call completes and is responsible for destroying it
 *
 * @return true on success
 */
bool jsonDatabaseAddEndpoint(icDeviceEndpoint *endpoint);

/**
 * Retrieve all devices in the jsonDatabase.
 *
 * @return linked list of all devices, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see deviceDestroy
 *
 */
icLinkedList *jsonDatabaseGetDevices();

/**
 * Retrieve all devices that have an endpoint with the given profile
 *
 * @param profileId the profile
 * @return linked list of devices, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see deviceDestroy
 */
icLinkedList *jsonDatabaseGetDevicesByEndpointProfile(const char *profileId);

/**
 * Retrieve all devices that have an endpoint with the given device class
 *
 * @param deviceClass the device class
 * @return linked list of devices, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see deviceDestroy
 */
icLinkedList *jsonDatabaseGetDevicesByDeviceClass(const char *deviceClass);

/**
 * Retrieve all devices that have an endpoint with the given device driver
 *
 * @param driverName the device driver name
 * @return linked list of devices, caller is responsible for destroying
 *
 * @see linkedListDestroy
 * @see deviceDestroy
 */
icLinkedList *jsonDatabaseGetDevicesByDeviceDriver(const char *deviceDriverName);

/**
 * Retrieve a device by its UUID
 *
 * @param uuid the uuid of the device
 * @return the device or NULL if not found.  Caller is responsible for destroying
 *
 * @see deviceDestroy
 */
icDevice *jsonDatabaseGetDeviceById(const char *uuid);

/**
 * Retrieve a device by its URI
 *
 * @param uri the uri of the device
 * @return the device or NULL if not found.  Caller is responsible for destroying
 *
 * @see deviceDestroy
 */
icDevice *jsonDatabaseGetDeviceByUri(const char *uri);

/**
 * Check if the provided device uuid is known to our database.
 *
 * @param uuid the uuid of the device
 * @return true if the device is known (in the database)
 */
bool jsonDatabaseIsDeviceKnown(const char *uuid);

/**
 * Remove a device
 *
 * @param uuid the uuid of the device
 * @return true if successful, false otherwise
 */
bool jsonDatabaseRemoveDeviceById(const char *uuid);

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
icLinkedList *jsonDatabaseGetEndpointsByProfile(const char *profileId);

/**
 * Retrieve an endpoint by its id
 *
 * @param deviceUuid the device the endpoint belongs to
 * @param endpointId the id of the endpoint
 * @return the endpoint, or NULL if its not found.  Caller is responsible for destroying
 *
 * @see endpointDestroy
 */
icDeviceEndpoint *jsonDatabaseGetEndpointById(const char *deviceUuid, const char *endpointId);

/**
 * Retrieve an endpoint by its uri
 *
 * @param uri the uri of the endpoint
 * @return the endpoint, or NULL if its not found.  Caller is responsible for destroying
 *
 * @see endpointDestroy
 */
icDeviceEndpoint *jsonDatabaseGetEndpointByUri(const char *uri);

/**
 * Update an endpoint in the database.  Currently only its enabled flag is updated
 *
 * @param endpoint the endpoint to update
 * @return true if success, false otherwise
 */
bool jsonDatabaseSaveEndpoint(icDeviceEndpoint *endpoint);

//Resources
/**
 * Retrieve a resource by its uri
 *
 * @param uri the uri of the resource
 * @return the resource, or NULL if its not found.  Caller is responsible for destroying
 *
 * @see resourceDestroy
 */
icDeviceResource *jsonDatabaseGetResourceByUri(const char *uri);

/**
 * Update a resource in the database.  The following properties can be updated:
 * value
 * cachingPolicy
 * mode
 * dateOfLastSyncMillis
 *
 * @param resource the resource to update
 * @return true if success, false otherwise
 */
bool jsonDatabaseSaveResource(icDeviceResource *resource);

/**
 * Update the dateOfLastSyncMillis of a resource.  Note that this is a lazy save and does not write to storage
 * immediately.
 *
 * @param resource the resource to update
 * @return true if success, false otherwise
 */
bool jsonDatabaseUpdateDateOfLastSyncMillis(icDeviceResource *resource);

//Metadata
/**
 * Retrieve a metadata by its uri
 *
 * @param uri the uri of the metadata
 * @return the metadata, or NULL if its not found.  Caller is responsible for destroying
 *
 * @see metadataDestroy
 */
icDeviceMetadata *jsonDatabaseGetMetadataByUri(const char *uri);

/**
 * Create or update a metadata in the database.  The value is the only property that can be updated.
 *
 * @param endpoint the metadata to update
 * @return true if success, false otherwise
 */
bool jsonDatabaseSaveMetadata(icDeviceMetadata *metadata);

/**
 * Get a list of resources matching the given regex
 *
 * @param uriRegex the regex to match
 * @return the resources that match the regex
 */
icLinkedList *jsonDatabaseGetResourcesByUriRegex(const char *uriRegex);

/**
 * Get a list of metadata matching the given regex
 *
 * @param uriRegex the regex to match
 * @return the metadata that match the regex
 */
icLinkedList *jsonDatabaseGetMetadataByUriRegex(const char *uriRegex);

#endif //ZILKER_JSONDATABASE_H
