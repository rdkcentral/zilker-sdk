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
// Created by mkoch201 on 5/9/18.
//

#include <icLog/logging.h>
#include "database/jsonDatabase.h"
#include <device/icDevice.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>

#include <deviceModelHelper.h>
#include <commonDeviceDefs.h>
#include <resourceTypes.h>
#include <deviceHelper.h>
#include <deviceDriver.h>
#include <device/icDeviceEndpoint.h>
#include <device/icDeviceResource.h>
#include <icConfig/storage.h>


#define LOG_TAG "jsonDatabaseTest"

#define USE_DUMMY_STORAGE "___USE_DUMMY_STORAGE___"

static int counter = 0;

static cJSON *dummyMemoryStorage;

bool __wrap_storageLoad(const char *namespace, const char *key, char **value);
bool __wrap_storageParse(const char* namespace, const char* key, const StorageCallbacks *cb);
bool __wrap_storageSave(const char* namespace, const char* key, const char* value);
icLinkedList* __wrap_storageGetKeys(const char* namespace);
bool __wrap_storageDelete(const char* namespace, const char* key);
static icDevice *createDummyDevice();

static void assertDevicesEqual(icDevice *device1, icDevice *device2);
static void assertEndpointsEqual(icDeviceEndpoint *endpoint1, icDeviceEndpoint *endpoint2);
static void assertResourceEqual(icDeviceResource *resource1, icDeviceResource *resource2);
static void assertMetadataEqual(icDeviceMetadata *metadata1, icDeviceMetadata *metadata2);

// ******************************
// Tests
// ******************************

static icDevice *createDeviceWithSensitiveResourceData(const char *uuid, const char *adminUser,
                                                       const char *adminPassword)
{
    icDevice *camera = createDevice(uuid, "camera", 1, "openHomeCameraDeviceDriver", NULL);
    camera->uri = malloc(64);
    sprintf(camera->uri, "/%s", uuid);

    icInitialResourceValues *initialResourceValues = initialResourceValuesCreate();

    icDeviceEndpoint *cameraEndpoint = createEndpoint(camera, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID, CAMERA_PROFILE, true);
    cameraEndpoint->uri = malloc(64);
    sprintf(cameraEndpoint->uri, "/%s/ep/camera", uuid);

    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                          COMMON_ENDPOINT_RESOURCE_LABEL, "My Camera 1");

    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                          CAMERA_PROFILE_RESOURCE_ADMIN_USER_ID, adminUser);

    initialResourceValuesPutEndpointValue(initialResourceValues, CAMERA_DC_CAMERA_PROFILE_ENDPOINT_ID,
                                          CAMERA_PROFILE_RESOURCE_ADMIN_PASSWORD, adminPassword);

    icDeviceResource *labelResource = createEndpointResourceIfAvailable(cameraEndpoint,
                                                                        COMMON_ENDPOINT_RESOURCE_LABEL,
                                                                        initialResourceValues,
                                                                        RESOURCE_TYPE_LABEL,
                                                                        RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_DYNAMIC |RESOURCE_MODE_EMIT_EVENTS,
                                                                        CACHING_POLICY_ALWAYS);
    labelResource->uri = malloc(64);
    sprintf(labelResource->uri, "/%s/ep/camera/r/label", uuid);

    icDeviceResource *adminUserResource = createEndpointResourceIfAvailable(cameraEndpoint,
                                                                            CAMERA_PROFILE_RESOURCE_ADMIN_USER_ID,
                                                                            initialResourceValues,
                                                                            RESOURCE_TYPE_USER_ID,
                                                                            RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_SENSITIVE,
                                                                            CACHING_POLICY_ALWAYS);
    adminUserResource->uri = malloc(64);
    sprintf(adminUserResource->uri, "/%s/ep/camera/r/adminUserId", uuid);

    icDeviceResource *adminPasswordResource = createEndpointResourceIfAvailable(cameraEndpoint,
                                                                                CAMERA_PROFILE_RESOURCE_ADMIN_PASSWORD,
                                                                                initialResourceValues,
                                                                                RESOURCE_TYPE_PASSWORD,
                                                                                RESOURCE_MODE_READWRITEABLE | RESOURCE_MODE_EMIT_EVENTS | RESOURCE_MODE_SENSITIVE,
                                                                                CACHING_POLICY_ALWAYS);
    adminPasswordResource->uri = malloc(64);
    sprintf(adminPasswordResource->uri, "/%s/ep/camera/r/adminPassword", uuid);

    initialResourceValuesDestroy(initialResourceValues);

    return camera;
}

static void test_jsonDatabaseAddDeviceWithSensitiveResourceData(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);

    // mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDeviceWithSensitiveResourceData("944a0c1c0ae4", "AdminUserNameValue", "AdminPasswordValue");

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    // Verify admin user and password don't appear in storage data
    char *rawStorageData = cJSON_Print(dummyMemoryStorage);
    icLogDebug(LOG_TAG, "rawStorageData=%s", rawStorageData);

    // TODO: uncomment when the obfuscation is implemented
//    assert_null(strstr(rawStorageData, "AdminUserNameValue"));
//    assert_null(strstr(rawStorageData, "AdminPasswordValue"));

    cJSON_free(rawStorageData);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);

    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // Read system properties
    will_return(__wrap_storageLoad, USE_DUMMY_STORAGE);

    // Read device
    will_return(__wrap_storageGetKeys, USE_DUMMY_STORAGE);
    will_return(__wrap_storageLoad, USE_DUMMY_STORAGE);

    jsonDatabaseInitialize();

    // Test that we read the device back in
    icDevice *savedDevice = jsonDatabaseGetDeviceById(device->uuid);
    assert_non_null(savedDevice);

    // Should come back the same way it was
    assertDevicesEqual(savedDevice, device);

    // Cleanup
    deviceDestroy(savedDevice);

    // We still own devices after we call add
    deviceDestroy(device);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);

    jsonDatabaseCleanup(true);

    (void) state;
}

static void test_jsonDatabaseInitializeEmptyAndCleanup(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    char *version;
    jsonDatabaseGetSystemProperty(JSON_DATABASE_SCHEMA_VERSION_KEY,&version);

    assert_string_equal(version, JSON_DATABASE_CURRENT_SCHEMA_VERSION);
    free(version);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    jsonDatabaseCleanup(true);

    (void) state;
}

static void test_jsonDatabaseSetSystemPropertyAndCleanup(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    // Set a property, which will do an immediate save
    will_return(__wrap_storageSave, true);
    jsonDatabaseSetSystemProperty("dummyKey","dummyValue");

    // Mock writing system properties for cleanup
    will_return(__wrap_storageSave, true);
    // No devices to write
    jsonDatabaseCleanup(true);

    (void) state;
}

static void test_jsonDatabaseAddOneDeviceAndCleanup(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    deviceDestroy(device);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    (void) state;
}

static void test_jsonDatabaseSetSystemPropertyAndReadItBack(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    // Set a property, which will do an immediate save
    will_return(__wrap_storageSave, true);
    jsonDatabaseSetSystemProperty("dummyKey","dummyValue");

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // No devices to write
    jsonDatabaseCleanup(true);

    // Read system properties
    will_return(__wrap_storageLoad, USE_DUMMY_STORAGE);
    // Read device
    will_return(__wrap_storageGetKeys, USE_DUMMY_STORAGE);
    jsonDatabaseInitialize();

    char *value;
    // Should only read from memory at this point
    assert_true(jsonDatabaseGetSystemProperty("dummyKey", &value));
    assert_string_equal(value, "dummyValue");
    // Cleanup
    free(value);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // No devices, so device will not get written
    jsonDatabaseCleanup(true);

    (void) state;
}

static void test_jsonDatabaseAddOneDeviceAndReadItBack(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // Read system properties
    will_return(__wrap_storageLoad, USE_DUMMY_STORAGE);
    // Read device
    will_return(__wrap_storageGetKeys, USE_DUMMY_STORAGE);
    will_return(__wrap_storageLoad, USE_DUMMY_STORAGE);
    jsonDatabaseInitialize();

    // Test that we read the device back in
    icDevice *loadedDevice = jsonDatabaseGetDeviceById(device->uuid);

    assert_non_null(loadedDevice);
    // Should come back the same way it was
    assertDevicesEqual(loadedDevice, device);

    // Cleanup
    deviceDestroy(loadedDevice);
    // We still own devices after we call add
    deviceDestroy(device);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    jsonDatabaseCleanup(true);

    (void) state;
}

static void test_jsonDatabaseGetDevices(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDevice *device2 = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device2));

    // Should have both devices
    icLinkedList *devices = jsonDatabaseGetDevices();
    assert_int_equal(linkedListCount(devices), 2);

    // Should be the "same" as what we put in
    icLinkedListIterator *iter = linkedListIteratorCreate(devices);
    while(linkedListIteratorHasNext(iter))
    {
        icDevice *item = (icDevice *)linkedListIteratorGetNext(iter);
        if (strcmp(item->uuid, device->uuid) == 0)
        {
            assertDevicesEqual(item, device);
        }
        else
        {
            assertDevicesEqual(item, device2);
        }
    }
    linkedListIteratorDestroy(iter);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // Cleanup
    linkedListDestroy(devices, (linkedListItemFreeFunc)deviceDestroy);
    // We still own devices after we call add
    deviceDestroy(device);
    deviceDestroy(device2);

    (void) state;
}

static void test_jsonDatabaseGetDevicesByEndpointProfile(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDevice *device2 = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device2));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    icLinkedList *foundDevices = jsonDatabaseGetDevicesByEndpointProfile(endpoint->profile);

    assert_non_null(foundDevices);
    assert_int_equal(linkedListCount(foundDevices), 1);

    icDevice *foundDevice = linkedListGetElementAt(foundDevices, 0);

    // Check its the same device
    assertDevicesEqual(foundDevice, device);

    // Clean up what we found
    linkedListDestroy(foundDevices, (linkedListItemFreeFunc)deviceDestroy);

    // Now try finding multiple devices
    icDevice *device3 = createDummyDevice();
    icDeviceEndpoint *endpoint2 = linkedListGetElementAt(device2->endpoints, 0);
    icDeviceEndpoint *endpoint3 = linkedListGetElementAt(device3->endpoints, 0);
    free(endpoint3->profile);
    endpoint3->profile = strdup(endpoint2->profile);

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device3));

    foundDevices = jsonDatabaseGetDevicesByEndpointProfile(endpoint2->profile);

    // Should have found both device2 and device3
    assert_int_equal(linkedListCount(foundDevices), 2);
    icLinkedListIterator *iter = linkedListIteratorCreate(foundDevices);
    while(linkedListIteratorHasNext(iter))
    {
        icDevice *item = (icDevice *)linkedListIteratorGetNext(iter);
        if (strcmp(item->uuid, device2->uuid) == 0)
        {
            assertDevicesEqual(item, device2);
        }
        else
        {
            assertDevicesEqual(item, device3);
        }
    }
    linkedListIteratorDestroy(iter);


    // Clean up what we found
    linkedListDestroy(foundDevices, (linkedListItemFreeFunc)deviceDestroy);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);
    deviceDestroy(device2);
    deviceDestroy(device3);

    (void) state;
}

static void test_jsonDatabaseGetDevicesByDeviceClass(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDevice *device2 = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device2));

    icLinkedList *foundDevices = jsonDatabaseGetDevicesByDeviceClass(device->deviceClass);

    assert_non_null(foundDevices);
    assert_int_equal(linkedListCount(foundDevices), 1);

    icDevice *foundDevice = linkedListGetElementAt(foundDevices, 0);

    // Check its the same device
    assertDevicesEqual(foundDevice, device);

    // Clean up what we found
    linkedListDestroy(foundDevices, (linkedListItemFreeFunc)deviceDestroy);

    // Now try finding multiple devices
    icDevice *device3 = createDummyDevice();
    free(device3->deviceClass);
    device3->deviceClass = strdup(device2->deviceClass);

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device3));

    foundDevices = jsonDatabaseGetDevicesByDeviceClass(device2->deviceClass);

    // Should have found both device2 and device3
    assert_int_equal(linkedListCount(foundDevices), 2);
    icLinkedListIterator *iter = linkedListIteratorCreate(foundDevices);
    while(linkedListIteratorHasNext(iter))
    {
        icDevice *item = (icDevice *)linkedListIteratorGetNext(iter);
        if (strcmp(item->uuid, device2->uuid) == 0)
        {
            assertDevicesEqual(item, device2);
        }
        else
        {
            assertDevicesEqual(item, device3);
        }
    }
    linkedListIteratorDestroy(iter);


    // Clean up what we found
    linkedListDestroy(foundDevices, (linkedListItemFreeFunc)deviceDestroy);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);
    deviceDestroy(device2);
    deviceDestroy(device3);

    (void) state;
}

static void test_jsonDatabaseGetDevicesByDeviceDriver(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDevice *device2 = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device2));

    icLinkedList *foundDevices = jsonDatabaseGetDevicesByDeviceDriver(device->managingDeviceDriver);

    assert_non_null(foundDevices);
    assert_int_equal(linkedListCount(foundDevices), 1);

    icDevice *foundDevice = linkedListGetElementAt(foundDevices, 0);

    // Check its the same device
    assertDevicesEqual(foundDevice, device);

    // Clean up what we found
    linkedListDestroy(foundDevices, (linkedListItemFreeFunc)deviceDestroy);

    // Now try finding multiple devices
    icDevice *device3 = createDummyDevice();
    free(device3->managingDeviceDriver);
    device3->managingDeviceDriver = strdup(device2->managingDeviceDriver);

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device3));

    foundDevices = jsonDatabaseGetDevicesByDeviceDriver(device2->managingDeviceDriver);

    // Should have found both device2 and device3
    assert_int_equal(linkedListCount(foundDevices), 2);
    icLinkedListIterator *iter = linkedListIteratorCreate(foundDevices);
    while(linkedListIteratorHasNext(iter))
    {
        icDevice *item = (icDevice *)linkedListIteratorGetNext(iter);
        if (strcmp(item->uuid, device2->uuid) == 0)
        {
            assertDevicesEqual(item, device2);
        }
        else
        {
            assertDevicesEqual(item, device3);
        }
    }
    linkedListIteratorDestroy(iter);


    // Clean up what we found
    linkedListDestroy(foundDevices, (linkedListItemFreeFunc)deviceDestroy);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);
    deviceDestroy(device2);
    deviceDestroy(device3);

    (void) state;
}

static void test_jsonDatabaseGetDeviceById(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDevice *device2 = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device2));

    icDevice *foundDevice = jsonDatabaseGetDeviceById(device->uuid);

    assert_non_null(foundDevice);

    // Check its the same device
    assertDevicesEqual(foundDevice, device);

    // Clean up what we found
    deviceDestroy(foundDevice);

    foundDevice = jsonDatabaseGetDeviceById(device2->uuid);

    assert_non_null(foundDevice);

    // Check its the same device
    assertDevicesEqual(foundDevice, device2);

    // Clean up what we found
    deviceDestroy(foundDevice);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);
    deviceDestroy(device2);

    (void) state;
}

static void test_jsonDatabaseGetDeviceByUri(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDevice *device2 = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device2));

    icDevice *foundDevice = jsonDatabaseGetDeviceByUri(device->uri);

    assert_non_null(foundDevice);

    // Check its the same device
    assertDevicesEqual(foundDevice, device);

    // Clean up what we found
    deviceDestroy(foundDevice);

    foundDevice = jsonDatabaseGetDeviceByUri(device2->uri);

    assert_non_null(foundDevice);

    // Check its the same device
    assertDevicesEqual(foundDevice, device2);

    // Clean up what we found
    deviceDestroy(foundDevice);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);
    deviceDestroy(device2);

    (void) state;
}

static void test_jsonDatabaseRemoveDeviceById(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    will_return(__wrap_storageDelete, USE_DUMMY_STORAGE);
    assert_true(jsonDatabaseRemoveDeviceById(device->uuid));

    // Make sure the device is gone
    icDevice *foundDevice = jsonDatabaseGetDeviceById(device->uuid);

    assert_null(foundDevice);

    foundDevice = jsonDatabaseGetDeviceByUri(device->uri);

    assert_null(foundDevice);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // Reopen database
    will_return(__wrap_storageLoad, USE_DUMMY_STORAGE);
    will_return(__wrap_storageGetKeys, USE_DUMMY_STORAGE);
    assert_true(jsonDatabaseInitialize());

    // Should still be gone
    foundDevice = jsonDatabaseGetDeviceById(device->uuid);

    assert_null(foundDevice);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseGetEndpointsByEndpointProfile(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDevice *device2 = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device2));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    icLinkedList *foundEndpoints = jsonDatabaseGetEndpointsByProfile(endpoint->profile);

    assert_non_null(foundEndpoints);
    assert_int_equal(linkedListCount(foundEndpoints), 1);

    icDeviceEndpoint *foundEndpoint = linkedListGetElementAt(foundEndpoints, 0);

    // Check its the same endpoint
    assertEndpointsEqual(foundEndpoint, endpoint);

    // Clean up what we found
    linkedListDestroy(foundEndpoints, (linkedListItemFreeFunc)endpointDestroy);

    // Now try finding multiple endpoints
    icDevice *device3 = createDummyDevice();
    icDeviceEndpoint *endpoint2 = linkedListGetElementAt(device2->endpoints, 0);
    icDeviceEndpoint *endpoint3 = linkedListGetElementAt(device3->endpoints, 0);
    free(endpoint3->profile);
    endpoint3->profile = strdup(endpoint2->profile);

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device3));

    foundEndpoints = jsonDatabaseGetEndpointsByProfile(endpoint2->profile);

    // Should have found both endpoint2 and endpoint3
    assert_int_equal(linkedListCount(foundEndpoints), 2);
    icLinkedListIterator *iter = linkedListIteratorCreate(foundEndpoints);
    while(linkedListIteratorHasNext(iter))
    {
        icDeviceEndpoint *item = (icDeviceEndpoint *)linkedListIteratorGetNext(iter);
        if (strcmp(item->id, endpoint2->id) == 0)
        {
            assertEndpointsEqual(item, endpoint2);
        }
        else
        {
            assertEndpointsEqual(item, endpoint3);
        }
    }
    linkedListIteratorDestroy(iter);


    // Clean up what we found
    linkedListDestroy(foundEndpoints, (linkedListItemFreeFunc)endpointDestroy);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);
    deviceDestroy(device2);
    deviceDestroy(device3);

    (void) state;
}


static void test_jsonDatabaseGetEndpointById(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDevice *device2 = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device2));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    icDeviceEndpoint *foundEndpoint = jsonDatabaseGetEndpointById(device->uuid, endpoint->id);

    assert_non_null(foundEndpoint);

    // Check its the same endpoint
    assertEndpointsEqual(foundEndpoint, endpoint);

    // Clean up what we found
    endpointDestroy(foundEndpoint);

    // Look for non-existant endpoint
    foundEndpoint = jsonDatabaseGetEndpointById(device2->uuid, endpoint->id);

    assert_null(foundEndpoint);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);
    deviceDestroy(device2);

    (void) state;
}

static void test_jsonDatabaseGetEndpointByUri(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    icDeviceEndpoint *foundEndpoint = jsonDatabaseGetEndpointByUri(endpoint->uri);

    assert_non_null(foundEndpoint);

    // Check its the same endpoint
    assertEndpointsEqual(foundEndpoint, endpoint);

    // Clean up what we found
    endpointDestroy(foundEndpoint);

    // Look for non-existant endpoint
    foundEndpoint = jsonDatabaseGetEndpointByUri("AABBCC");

    assert_null(foundEndpoint);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseSaveEndpoint(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    endpoint->enabled = false;
    // Mock writing this out to disk
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseSaveEndpoint(endpoint));

    // This should still be false
    assert_false(endpoint->enabled);

    // If we query it should also have been updated
    icDevice *foundDevice = jsonDatabaseGetDeviceById(device->uuid);
    icDeviceEndpoint *foundEndpoint = linkedListGetElementAt(foundDevice->endpoints, 0);
    assertEndpointsEqual(foundEndpoint, endpoint);

    // Cleanup the device
    deviceDestroy(foundDevice);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseGetResourceByUri(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceResource *resource = linkedListGetElementAt(device->resources, 0);

    icDeviceResource *foundResource = jsonDatabaseGetResourceByUri(resource->uri);

    assert_non_null(foundResource);

    // Check its the same endpoint
    assertResourceEqual(foundResource, resource);

    // Clean up what we found
    resourceDestroy(foundResource);

    // Look for non-existant endpoint
    foundResource = jsonDatabaseGetResourceByUri("AABBCC");

    assert_null(foundResource);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseSaveResource(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceResource *resource = linkedListGetElementAt(device->resources, 0);

    free(resource->value);
    resource->value = strdup("abc123");


    // Mock writing this out to disk
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseSaveResource(resource));

    // This should still be false
    assert_string_equal(resource->value, "abc123");

    // If we query it should also have been updated
    icDevice *foundDevice = jsonDatabaseGetDeviceById(device->uuid);
    icDeviceResource *foundResource = linkedListGetElementAt(foundDevice->resources, 0);
    assertResourceEqual(foundResource, resource);

    // Cleanup the device
    deviceDestroy(foundDevice);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseGetEndpointResourceByUri(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    icDeviceResource *resource = linkedListGetElementAt(endpoint->resources, 0);

    icDeviceResource *foundResource = jsonDatabaseGetResourceByUri(resource->uri);

    assert_non_null(foundResource);

    // Check its the same endpoint
    assertResourceEqual(foundResource, resource);

    // Clean up what we found
    resourceDestroy(foundResource);

    // Look for non-existant endpoint
    foundResource = jsonDatabaseGetResourceByUri("AABBCC");

    assert_null(foundResource);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseSaveEndpointResource(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    icDeviceResource *resource = linkedListGetElementAt(endpoint->resources, 0);

    free(resource->value);
    resource->value = strdup("abc123");


    // Mock writing this out to disk
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseSaveResource(resource));

    // This should still be abc123
    assert_string_equal(resource->value, "abc123");

    // If we query it should also have been updated
    icDevice *foundDevice = jsonDatabaseGetDeviceById(device->uuid);
    icDeviceEndpoint *foundEndpoint = linkedListGetElementAt(foundDevice->endpoints, 0);
    icDeviceResource *foundResource = linkedListGetElementAt(foundEndpoint->resources, 0);
    assertResourceEqual(foundResource, resource);

    // Cleanup the device
    deviceDestroy(foundDevice);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}


static void test_jsonDatabaseGetMetadataByUri(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    icDeviceMetadata *metadata = linkedListGetElementAt(endpoint->metadata, 0);

    icDeviceMetadata *foundMetadata = jsonDatabaseGetMetadataByUri(metadata->uri);

    assert_non_null(foundMetadata);

    // Check its the same endpoint
    assertMetadataEqual(foundMetadata, metadata);

    // Clean up what we found
    metadataDestroy(foundMetadata);

    // Look for non-existant endpoint
    foundMetadata = jsonDatabaseGetMetadataByUri("AABBCC");

    assert_null(foundMetadata);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseSaveMetadata(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceEndpoint *endpoint = linkedListGetElementAt(device->endpoints, 0);

    icDeviceMetadata *metadata = linkedListGetElementAt(endpoint->metadata, 0);

    free(metadata->value);
    metadata->value = strdup("abc123");


    // Mock writing this out to disk
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseSaveMetadata(metadata));

    // This should still be abc123
    assert_string_equal(metadata->value, "abc123");

    // If we query it should also have been updated
    icDevice *foundDevice = jsonDatabaseGetDeviceById(device->uuid);
    icDeviceEndpoint *foundEndpoint = linkedListGetElementAt(foundDevice->endpoints, 0);
    icDeviceMetadata *foundMetadata = linkedListGetElementAt(foundEndpoint->metadata, 0);
    assertMetadataEqual(foundMetadata, metadata);

    // Cleanup the device
    deviceDestroy(foundDevice);

    icDeviceMetadata *newMetadata = createEndpointMetadata(endpoint, "newMetadata", "newMetadataValue");
    // Mock writing this out to disk
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseSaveMetadata(newMetadata));

    // If we query it should also have been created
    foundMetadata = jsonDatabaseGetMetadataByUri(newMetadata->uri);
    assertMetadataEqual(foundMetadata, newMetadata);

    // Cleanup
    metadataDestroy(foundMetadata);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseGetDeviceByOtherUris(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceEndpoint *endpoint = (icDeviceEndpoint *)linkedListGetElementAt(device->endpoints, 0);
    icDeviceResource *resource = (icDeviceResource *)linkedListGetElementAt(device->resources, 0);
    icDeviceResource *endpointResource = (icDeviceResource *)linkedListGetElementAt(endpoint->resources, 0);
    icDeviceMetadata *metadata = (icDeviceMetadata *)linkedListGetElementAt(endpoint->metadata, 0);

    // Query by endpoint uri
    icDevice *foundDevice = jsonDatabaseGetDeviceByUri(endpoint->uri);
    assertDevicesEqual(foundDevice, device);
    // Cleanup the device
    deviceDestroy(foundDevice);
    // Query by resource uri
    foundDevice = jsonDatabaseGetDeviceByUri(resource->uri);
    assertDevicesEqual(foundDevice, device);
    // Cleanup the device
    deviceDestroy(foundDevice);
    // Query by endpoint resource uri
    foundDevice = jsonDatabaseGetDeviceByUri(endpointResource->uri);
    assertDevicesEqual(foundDevice, device);
    // Cleanup the device
    deviceDestroy(foundDevice);
    // Query by metadata uri
    foundDevice = jsonDatabaseGetDeviceByUri(metadata->uri);
    assertDevicesEqual(foundDevice, device);
    // Cleanup the device
    deviceDestroy(foundDevice);


    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}


static void test_jsonDatabaseGetEndpointByOtherUris(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    icDeviceEndpoint *endpoint = (icDeviceEndpoint *)linkedListGetElementAt(device->endpoints, 0);
    icDeviceResource *endpointResource = (icDeviceResource *)linkedListGetElementAt(endpoint->resources, 0);
    icDeviceMetadata *metadata = (icDeviceMetadata *)linkedListGetElementAt(endpoint->metadata, 0);

    // Query by resource uri
    icDeviceEndpoint *foundEndpoint = jsonDatabaseGetEndpointByUri(endpointResource->uri);
    assertEndpointsEqual(foundEndpoint, endpoint);
    // Cleanup the device
    endpointDestroy(foundEndpoint);
    // Query by endpoint resource uri
    foundEndpoint = jsonDatabaseGetEndpointByUri(metadata->uri);
    assertEndpointsEqual(foundEndpoint, endpoint);
    // Cleanup the device
    endpointDestroy(foundEndpoint);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseGetResourcesByUriRegex(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Create some additional resources to match
    icDeviceResource *resource = createDeviceResource(device, "bypassed", "false", RESOURCE_TYPE_STRING, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    resource->uri = createDeviceResourceUri(device->uuid, resource->id);
    resource = createDeviceResource(device, "Rssi", "-25", RESOURCE_TYPE_STRING, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    resource->uri = createDeviceResourceUri(device->uuid, resource->id);

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    // Query by regex
    icLinkedList *foundResources = jsonDatabaseGetResourcesByUriRegex(".*ssi");
    assert_int_equal(linkedListCount(foundResources), 1);

    // Clean up the findings
    linkedListDestroy(foundResources, (linkedListItemFreeFunc)resourceDestroy);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // We still own devices after we call add
    deviceDestroy(device);

    (void) state;
}

static void test_jsonDatabaseAddNewDeviceMetdata(void **state)
{
    // mock so no systemProperties database which equals no database
    will_return(__wrap_storageLoad, NULL);
    /// mock initialization of systemProperties database
    will_return(__wrap_storageSave, true);

    assert_true(jsonDatabaseInitialize());

    icDevice *device = createDummyDevice();

    // Mock saving the device
    will_return(__wrap_storageSave, true);
    assert_true(jsonDatabaseAddDevice(device));

    // Add some new regex
    icDeviceMetadata *metadata = createDeviceMetadata(device, "newMetadata", "newValue");
    // Mock saving the device
    will_return(__wrap_storageSave, true);
    jsonDatabaseSaveMetadata(metadata);

    // Mock writing system properties
    will_return(__wrap_storageSave, true);
    // Device should not be dirty, so device will not get written
    jsonDatabaseCleanup(true);

    // Read system properties
    will_return(__wrap_storageLoad, USE_DUMMY_STORAGE);
    // Read device
    will_return(__wrap_storageGetKeys, USE_DUMMY_STORAGE);
    will_return(__wrap_storageLoad, USE_DUMMY_STORAGE);
    jsonDatabaseInitialize();

    // Test that we read the device back in
    icDevice *loadedDevice = jsonDatabaseGetDeviceById(device->uuid);

    assert_non_null(loadedDevice);
    // Should come back the same way it was
    assertDevicesEqual(loadedDevice, device);

    // Cleanup
    deviceDestroy(loadedDevice);

    // We still own devices after we call add
    deviceDestroy(device);

    // Mock writing system properties for cleanup
    will_return(__wrap_storageSave, true);

    // No devices to write
    jsonDatabaseCleanup(true);

    (void) state;
}

// ******************************
// Setup/Teardown
// ******************************

static int dummyStorageSetup(void **state)
{
    dummyMemoryStorage = cJSON_CreateObject();

    (void)state;

    return 0;
}

static int dummyStorageTeardown(void **state)
{
    if (dummyMemoryStorage != NULL)
    {
        cJSON_Delete(dummyMemoryStorage);
        dummyMemoryStorage = NULL;
    }

    (void)state;

    return 0;
}

// ******************************
// Helpers
// ******************************


static char *dummyStorageGet(const char *namespace, const char *key)
{
    char *retval = NULL;
    cJSON *namespaceJson = cJSON_GetObjectItem(dummyMemoryStorage, namespace);
    if (namespaceJson != NULL)
    {
        cJSON *keyJson = cJSON_GetObjectItem(namespaceJson, key);
        if (keyJson != NULL)
        {
            retval = strdup(keyJson->valuestring);

            icLogDebug(LOG_TAG, "Read from dummy storage namespace=%s, key=%s, value=%s", namespace, key, retval);
        }
        else
        {
            icLogDebug(LOG_TAG, "Failed to ready from dummy storage namespace=%s, key=%s", namespace, key);
        }
    }
    else
    {
        icLogDebug(LOG_TAG, "Did not find namespace %s in dummy storage", namespace);
    }

    return retval;
}

static void dummyStoragePut(const char *namespace, const char *key, const char *value)
{
    cJSON *namespaceJson = cJSON_GetObjectItem(dummyMemoryStorage, namespace);
    if (namespaceJson == NULL)
    {
        icLogDebug(LOG_TAG, "Creating dummy storage namespace %s", namespace);
        namespaceJson = cJSON_CreateObject();
        cJSON_AddItemToObject(dummyMemoryStorage, namespace, namespaceJson);
    }

    if (cJSON_HasObjectItem(namespaceJson, key))
    {
        cJSON_DeleteItemFromObject(namespaceJson, key);
    }
    cJSON_AddStringToObject(namespaceJson, key, value);

    icLogDebug(LOG_TAG, "Saving to dummy storage namespace=%s, key=%s, value=%s", namespace, key, value);
}

static icLinkedList *dummyStorageGetKeys(const char *namespace)
{
    icLinkedList *keys = linkedListCreate();

    cJSON *keysJson = cJSON_GetObjectItem(dummyMemoryStorage, namespace);
    if (keysJson != NULL)
    {
        int count = cJSON_GetArraySize(keysJson);
        for(int i = 0; i < count; ++i)
        {
            cJSON *item = cJSON_GetArrayItem(keysJson, i);
            icLogDebug(LOG_TAG, "Found key %s for namespace %s", item->string, namespace);
            linkedListAppend(keys, strdup(item->string));
        }
    }


    return keys;
}

static bool dummyStorageDelete(const char* namespace, const char* key)
{
    bool retval = false;
    cJSON *keysJson = cJSON_GetObjectItem(dummyMemoryStorage, namespace);
    if (keysJson != NULL)
    {
        if (cJSON_HasObjectItem(keysJson, key))
        {
            icLogDebug(LOG_TAG, "Deleting key %s for namespace %s", key, namespace);
            cJSON_DeleteItemFromObject(keysJson, key);
            retval = true;
        }
    }

    return retval;
}

static icDevice *createDummyDevice()
{
    ++counter;
    char uuid[64];
    sprintf(uuid,"device%d", counter++);
    char class[64];
    sprintf(class,"dummyClass%d", counter++);
    char driver[64];
    sprintf(driver,"dummyDriver%d", counter++);
    icDevice *device = createDevice(uuid, class, 1, driver, NULL);
    char uri[64];
    sprintf(uri,"/%s", device->uuid);
    device->uri = strdup(uri);

    // Create resources for the device
    char deviceResourceId[64];
    sprintf(deviceResourceId,"dummyResource%d", counter++);
    char deviceResourceValue[64];
    sprintf(deviceResourceValue, "dummyResourceValue%d", counter++);
    icDeviceResource *resource = createDeviceResource(device, deviceResourceId, deviceResourceValue, RESOURCE_TYPE_STRING, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    resource->uri = createDeviceResourceUri(device->uuid, resource->id);

    // Create a metadata for the device
    char metadataId[64];
    sprintf(metadataId, "dummyMD%d", counter++);
    char metadataValue[64];
    sprintf(metadataValue, "dummyMDValue%d", counter++);
    createDeviceMetadata(device, metadataId, metadataValue);
    // For whatever reason metadata automatically gets its uri set, vs the others
    // Create some json metadata
    cJSON *jsonMetadataValue = cJSON_CreateObject();
    sprintf(metadataId, "dummyJSONMD%d", counter++);
    sprintf(metadataValue, "dummyMDValue%d", counter++);
    cJSON_AddStringToObject(jsonMetadataValue, metadataValue, "testVal");
    char *val = cJSON_Print(jsonMetadataValue);
    createDeviceMetadata(device, metadataId, val);
    free(val);
    cJSON_Delete(jsonMetadataValue);

    // Create an endpoint
    char endpointId[64];
    sprintf(endpointId, "endpoint%d", counter++);
    char profile[64];
    sprintf(profile, "dummyProfile%d", counter++);
    icDeviceEndpoint *endpoint = createEndpoint(device, endpointId, profile, true);
    endpoint->uri = createEndpointUri(device->uuid, endpointId);

    // Create a resource for the endpoint
    char endpointResourceId[64];
    sprintf(endpointResourceId,"dummyEndpointResource%d", counter++);
    char endpointResourceValue[64];
    sprintf(endpointResourceValue, "dummyEndpointResourceValue%d", counter++);
    resource = createEndpointResource(endpoint, endpointResourceId, endpointResourceValue, RESOURCE_TYPE_STRING, RESOURCE_MODE_READABLE | RESOURCE_MODE_SENSITIVE, CACHING_POLICY_ALWAYS);
    resource->uri = createEndpointResourceUri(device->uuid, endpoint->id, resource->id);

    // Create a metadata for the endpoint
    sprintf(metadataId, "dummyMD%d", counter++);
    sprintf(metadataValue, "dummyMDValue%d", counter++);
    createEndpointMetadata(endpoint, metadataId, metadataValue);
    // For whatever reason metadata automatically gets its uri set, vs the others

    return device;
}

#define assert_non_null_string_equal(a, b) \
    assert_non_null(a); \
    assert_non_null(b); \
    assert_string_equal(a,b);

static void assertResourceEqual(icDeviceResource *resource1, icDeviceResource *resource2)
{
    assert_non_null_string_equal(resource1->id, resource2->id);
    assert_non_null_string_equal(resource1->deviceUuid, resource2->deviceUuid);
    assert_non_null_string_equal(resource1->uri, resource2->uri);
    if (resource1->endpointId != NULL && resource2->endpointId != NULL)
    {
        assert_string_equal(resource1->endpointId, resource2->endpointId);
    }
    else
    {
        assert_null(resource1->endpointId);
        assert_null(resource2->endpointId);
    }
//    assert_non_null_string_equal(resource1->value, resource2->value);
    assert_non_null_string_equal(resource1->type, resource2->type);
    assert_int_equal(resource1->dateOfLastSyncMillis, resource2->dateOfLastSyncMillis);
    assert_int_equal(resource1->cachingPolicy, resource2->cachingPolicy);
    assert_int_equal(resource1->mode, resource2->mode);
}

static void assertResourcesEqual(icLinkedList *resources1, icLinkedList *resources2)
{
    assert_non_null(resources1);
    assert_non_null(resources2);
    assert_int_equal(linkedListCount(resources1), linkedListCount(resources2));
    icLinkedListIterator *iter1 = linkedListIteratorCreate(resources1);
    icLinkedListIterator *iter2 = linkedListIteratorCreate(resources2);
    while(linkedListIteratorHasNext(iter1) && linkedListIteratorHasNext(iter2))
    {
        icDeviceResource *resource1 = (icDeviceResource *)linkedListIteratorGetNext(iter1);
        icDeviceResource *resource2 = (icDeviceResource *)linkedListIteratorGetNext(iter2);
        assertResourceEqual(resource1, resource2);
    }
    linkedListIteratorDestroy(iter1);
    linkedListIteratorDestroy(iter2);
}

static void assertMetadataEqual(icDeviceMetadata *metadata1, icDeviceMetadata *metadata2)
{
    assert_non_null_string_equal(metadata1->id, metadata2->id);
    assert_non_null_string_equal(metadata1->deviceUuid, metadata2->deviceUuid);
    assert_non_null_string_equal(metadata1->uri, metadata2->uri);
    if (metadata1->endpointId != NULL && metadata2->endpointId != NULL)
    {
        assert_string_equal(metadata1->endpointId, metadata2->endpointId);
    }
    else
    {
        assert_null(metadata1->endpointId);
        assert_null(metadata2->endpointId);
    }
    assert_non_null_string_equal(metadata1->value, metadata2->value);
}

static void assertMetadatasEqual(icLinkedList *metadatas1, icLinkedList *metadatas2)
{
    assert_non_null(metadatas1);
    assert_non_null(metadatas2);
    assert_int_equal(linkedListCount(metadatas1), linkedListCount(metadatas2));
    icLinkedListIterator *iter1 = linkedListIteratorCreate(metadatas1);
    icLinkedListIterator *iter2 = linkedListIteratorCreate(metadatas2);
    while(linkedListIteratorHasNext(iter1) && linkedListIteratorHasNext(iter2))
    {
        icDeviceMetadata *metadata1 = (icDeviceMetadata *)linkedListIteratorGetNext(iter1);
        icDeviceMetadata *metadata2 = (icDeviceMetadata *)linkedListIteratorGetNext(iter2);
        assertMetadataEqual(metadata1, metadata2);
    }
    linkedListIteratorDestroy(iter1);
    linkedListIteratorDestroy(iter2);
}

static void assertEndpointsEqual(icDeviceEndpoint *endpoint1, icDeviceEndpoint *endpoint2)
{
    assert_non_null(endpoint1);
    assert_non_null(endpoint2);
    assert_non_null_string_equal(endpoint1->uri, endpoint2->uri);
    assert_non_null_string_equal(endpoint1->deviceUuid, endpoint2->deviceUuid);
    assert_non_null_string_equal(endpoint1->id, endpoint2->id);
    assert_non_null_string_equal(endpoint1->profile, endpoint2->profile);
    assert_int_equal(endpoint1->profileVersion, endpoint2->profileVersion);
    assert_true(endpoint1->enabled == endpoint2->enabled);

    assertResourcesEqual(endpoint1->resources, endpoint2->resources);
    assertMetadatasEqual(endpoint1->metadata, endpoint2->metadata);
}

static void assertDevicesEqual(icDevice *device1, icDevice *device2)
{
    assert_non_null(device1);
    assert_non_null(device2);
    assert_non_null_string_equal(device1->uuid, device2->uuid);
    assert_non_null_string_equal(device1->uri, device2->uri);
    assert_non_null_string_equal(device1->deviceClass, device2->deviceClass);
    assert_non_null_string_equal(device1->managingDeviceDriver, device2->managingDeviceDriver);
    assert_int_equal(device1->deviceClassVersion, device2->deviceClassVersion);
    assert_non_null(device1->endpoints);
    assert_non_null(device2->endpoints);
    assert_int_equal(linkedListCount(device1->endpoints), linkedListCount(device2->endpoints));

    icLinkedListIterator *iter1 = linkedListIteratorCreate(device1->endpoints);
    icLinkedListIterator *iter2 = linkedListIteratorCreate(device2->endpoints);
    while(linkedListIteratorHasNext(iter1) && linkedListIteratorHasNext(iter2))
    {
        icDeviceEndpoint *endpoint1 = (icDeviceEndpoint *)linkedListIteratorGetNext(iter1);
        icDeviceEndpoint *endpoint2 = (icDeviceEndpoint *)linkedListIteratorGetNext(iter2);
        assertEndpointsEqual(endpoint1, endpoint2);
    }
    linkedListIteratorDestroy(iter1);
    linkedListIteratorDestroy(iter2);

    assertMetadatasEqual(device1->metadata, device2->metadata);

    assertResourcesEqual(device1->resources, device2->resources);
}


// ******************************
// wrapped(mocked) functions
// ******************************

bool __wrap_storageLoad(const char *namespace, const char *key, char **value)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s, key=%s", __FUNCTION__, namespace, key);

    *value = mock_type(char *);
    if (*value == NULL)
    {
        return false;
    }
    else if (strcmp(*value,USE_DUMMY_STORAGE) == 0)
    {
        icLogDebug(LOG_TAG, "%s: namespace=%s, key=%s - reading from memory storage", __FUNCTION__, namespace, key);

        *value = dummyStorageGet(namespace, key);

        return true;
    }
    else
    {
        return true;
    }
}

/**
 * Test implementation of storageParse. Use will_return(__wrap_storageLoad, USE_DUMMY_STORAGE) to use test fixtures.
 * @param namespace
 * @param key
 * @param cb
 * @return true when data is valid
 */
bool __wrap_storageParse(const char* namespace, const char* key, const StorageCallbacks *cb)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s, key=%s", __FUNCTION__, namespace, key);

    AUTO_CLEAN(free_generic__auto) char *fileData = NULL;
    if (__wrap_storageLoad(namespace, key, &fileData) == true)
    {
        if (cb != NULL && cb->parse != NULL)
        {
            /* cb->parse is loadDevice */
            return cb->parse(fileData, cb->parserCtx);
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

bool __wrap_storageSave(const char* namespace, const char* key, const char* value)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s, key=%s, value=%s", __FUNCTION__, namespace, key, value);

    bool retval = mock_type(bool);

    if (retval)
    {
        dummyStoragePut(namespace, key, value);
    }

    return retval;
}

icLinkedList* __wrap_storageGetKeys(const char* namespace)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s", __FUNCTION__, namespace);

    void *mockReturn = (void *)mock();
    if (mockReturn != NULL && strcmp(mockReturn,USE_DUMMY_STORAGE) == 0)
    {
        icLogDebug(LOG_TAG, "%s: namespace=%s - getting keys from memory storage", __FUNCTION__, namespace);

        return dummyStorageGetKeys(namespace);
    }
    else
    {
        return (icLinkedList *)mockReturn;
    }
}

bool __wrap_storageDelete(const char* namespace, const char* key)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s, key=%s", __FUNCTION__, namespace, key);
    void *mockReturn = (void *)mock();
    if (mockReturn != NULL && strcmp(mockReturn,USE_DUMMY_STORAGE) == 0)
    {
        icLogDebug(LOG_TAG, "%s: namespace=%s - deleting from memory storage", __FUNCTION__, namespace);

        return dummyStorageDelete(namespace, key);
    }
    else
    {
        return (bool)mockReturn;
    }
}


int main(int argc, const char ** argv)
{
    initIcLogger();

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseInitializeEmptyAndCleanup, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseSetSystemPropertyAndCleanup, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseAddOneDeviceAndCleanup, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseSetSystemPropertyAndReadItBack, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseAddOneDeviceAndReadItBack, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetDevices, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetDevicesByEndpointProfile, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetDevicesByDeviceClass, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetDevicesByDeviceDriver, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetDeviceById, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetDeviceByUri, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseRemoveDeviceById, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetEndpointsByEndpointProfile, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetEndpointById, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetEndpointByUri, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseSaveEndpoint, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetResourceByUri, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseSaveResource, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetEndpointResourceByUri, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseSaveEndpointResource, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetMetadataByUri, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseSaveMetadata, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetDeviceByOtherUris, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetEndpointByOtherUris, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseGetResourcesByUriRegex, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseAddNewDeviceMetdata, dummyStorageSetup, dummyStorageTeardown),
                    cmocka_unit_test_setup_teardown(test_jsonDatabaseAddDeviceWithSensitiveResourceData, dummyStorageSetup, dummyStorageTeardown),
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    closeIcLogger();

    return retval;
}
