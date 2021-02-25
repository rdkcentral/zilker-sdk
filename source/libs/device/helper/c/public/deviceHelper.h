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
 * This library provides a higher level interface to device service.  It has some knowledge
 * about well known first class device types.  It also knows how to assemble URIs to access
 * various elements on devices.
 *
 * Created by Thomas Lea on 9/30/15.
 */

#ifndef ZILKER_DEVICEHELPER_H
#define ZILKER_DEVICEHELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <deviceService/deviceService_ipc.h>

/*
 * Helper to create a URI to a device.
 *
 * Caller frees result.
 */
char* createDeviceUri(const char *deviceUuid);

/*
 * Helper to create a URI to an endpoint on a device.
 *
 * Caller frees result.
 */
char* createEndpointUri(const char *deviceUuid, const char *endpointId);

/*
 * Helper to create a URI to an resource on a device.
 *
 * Caller frees result.
 */
char* createDeviceResourceUri(const char *deviceUuid, const char *resourceId);

/*
 * Helper to create a URI to an metadata on a device
 *
 * Caller frees result.
 */
char *createDeviceMetadataUri(const char *deviceUuid, const char *metadataId);

/*
 * Helper to create a URI to an resource on an endpoint.
 *
 * Caller frees result.
 */
char* createEndpointResourceUri(const char *deviceUuid, const char *endpointId, const char *resourceId);

/*
 * Helper to create a URI to a metadata item on an endpoint.
 *
 * Caller frees result.
 */
char* createEndpointMetadataUri(const char *deviceUuid, const char *endpointId, const char *metadataId);

/*
 * Read an resource on a device's endpoint.
 *
 * Caller frees value output on success.
 */
bool deviceHelperReadEndpointResource(const char *deviceUuid,
                                       const char *endpointId,
                                       const char *resourceId,
                                       char **value);

/*
 * Write a resource on a device's endpoint.
 */
bool deviceHelperWriteEndpointResource(const char *deviceUuid,
                                       const char *endpointId,
                                       const char *resourceId,
                                       char *value);
/*
 * Read a resource by ID
 *
 * Caller frees value output on success.
 */
bool deviceHelperReadDeviceResource(const char *deviceUuid,
                                    const char *resourceId,
                                    char **value);
/*
 * Get a resource, including its description/definition, by URI
 *
 * Caller must pass in a DSResource pointer created with create_DSResource and then destroy it with destroy_DSResource
 */
bool deviceHelperGetResourceByUri(const char *uri,
                                  DSResource *resource);

/*
 * Read a resource by URI
 *
 * Caller frees value output on success.
 */
bool deviceHelperReadResourceByUri(const char *uri,
                                   char **value);

/*
 * Read metadata by URI
 *
 * Caller frees value output on success.
 */
bool deviceHelperReadMetadataByUri(const char *uri,
                                   char **value);

/*
 * Write metadata by URI
 */
bool deviceHelperWriteMetadataByUri(const char *uri,
                                    const char *value);

/*
 * Read metadata by owner URI and ID
 *
 * Caller frees value output on success.
 */
bool deviceHelperReadMetadataByOwner(const char *ownerUri,
                                     const char *metadataId,
                                     char **value);

/*
 * Write metadata by owner URI and ID
 */
bool deviceHelperWriteMetadataByOwner(const char *ownerUri,
                                      const char *metadataId,
                                      const char *value);

//TODO get rid of many of the above helpers that separate devices from endpoints

/*
 * Helper to create a URI to an resource (on either a device or an endpoint)
 *
 * Caller frees result.
 */
char* createResourceUri(const char *ownerUri, const char *resourceId);

/*
 * Extract the device UUID from the given URI, caller must free result
 */
char* getDeviceUuidFromUri(const char *uri);

/*
 * Extract the endpoint ID from the given URI, caller must free result
 */
char* getEndpointIdFromUri(const char *uri);

/**
 * Get the value of a resource from the device
 * @param endpoint the endpoint to fetch from
 * @param resourceId the resourceId to fetch
 * @param defaultValue the default value to use if value is empty or cannot be retrieved
 * @return the value, which should NOT be freed
 */
const char *deviceHelperGetDeviceResourceValue(DSDevice *device, const char *resourceId, const char *defaultValue);

/**
 * Get the value of a resource endpoint from the endpoint
 * @param endpoint the endpoint to fetch from
 * @param resourceId the resourceId to fetch
 * @param defaultValue the default value to use if value is empty or cannot be retrieved
 * @return the value, which should NOT be freed
 */
const char *deviceHelperGetEndpointResourceValue(DSEndpoint *endpoint, const char *resourceId, const char *defaultValue);

/**
 * Find the first endpoint on the given device with the given profile
 * @param device the device
 * @param endpointProfile the profile
 * @return the endpoint or NULL if no endpoint found
 */
DSEndpoint *deviceHelperGetEndpointByProfile(DSDevice *device, const char *endpointProfile);

/**
 * Determine if a device is multi-endpoint capable
 * @param device
 * @return
 */
bool deviceHelperIsMultiEndpointCapable(const DSDevice *device);

#endif //ZILKER_DEVICEHELPER_H
