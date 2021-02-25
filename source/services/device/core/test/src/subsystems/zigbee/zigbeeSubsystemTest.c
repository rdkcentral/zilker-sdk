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
// Created by Micah Koch on 5/02/18.
//
#include <icLog/logging.h>
#include "subsystems/zigbee/zigbeeSubsystem.h"
#include <device/icDevice.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <deviceModelHelper.h>
#include <commonDeviceDefs.h>
#include <resourceTypes.h>
#include <stdio.h>
#include <string.h>
#include <deviceDriver.h>
#include <sys/stat.h>
#include <errno.h>
#include <subsystems/zigbee/zigbeeSubsystem.h>
#include <zhal/zhal.h>


#define LOG_TAG "zigbeeSubsystemTest"
#define DUMMY_OTA_FIRMWARE_FILE "dummy.ota"
#define LEGACY_FIRMWARE_FILE "dummy.ebl"

static int counter = 0;
static char templateTempDir[255] = "/tmp/testDirXXXXXX";
static char *dynamicDir;

icLinkedList *__wrap_deviceServiceGetDevicesBySubsystem(const char *subsystem);
DeviceDescriptor *__wrap_deviceServiceGetDeviceDescriptorForDevice(icDevice *device);
char *__wrap_getDynamicPath();
static icDevice *createDummyDevice(const char *manufacturer, const char *model, const char *hardwareVersion, const char *firmwareVersion);
static DeviceDescriptor *createDeviceDescriptor(const char *manufacturer, const char *model, const char *harwareVersion, const char *firmareVersion);
static void createDummyFirmwareFile(DeviceFirmwareType);
static char *getDummyFirmwareFilePath(DeviceFirmwareType firmwareType);

// ******************************
// Tests
// ******************************

static void test_zigbeeSubsystemCleanupFirmwareFiles(void **state)
{
    // Setup dummy device and cooresponding device descriptor
    icLinkedList *devices = linkedListCreate();
    linkedListAppend(devices, createDummyDevice("dummy","dummy","1","0x00000001"));
    will_return(__wrap_deviceServiceGetDevicesBySubsystem, devices);
    DeviceDescriptor *dd = createDeviceDescriptor("dummy","dummy","1","0x00000001");
    will_return(__wrap_deviceServiceGetDeviceDescriptorForDevice, dd);
    // Create some dummy firmware files to remove
    createDummyFirmwareFile(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);
    createDummyFirmwareFile(DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY);

    // Make the call
    zigbeeSubsystemCleanupFirmwareFiles();

    // Check if the files are gone like expected
    char *otaPath = getDummyFirmwareFilePath(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);
    struct stat statBuf;
    assert_int_not_equal(stat(otaPath, &statBuf), 0);
    assert_int_equal(errno, ENOENT);
    free(otaPath);

    char *legacyPath = getDummyFirmwareFilePath(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);
    assert_int_not_equal(stat(legacyPath, &statBuf), 0);
    assert_int_equal(errno, ENOENT);
    free(legacyPath);

    (void) state;
}

static void test_zigbeeSubsystemCleanupFirmwareFilesDoNothingIfFirmwareNeeded(void **state)
{
    // Setup dummy device and cooresponding device descriptor
    icLinkedList *devices = linkedListCreate();
    linkedListAppend(devices, createDummyDevice("dummy","dummy","1","0x00000001"));
    will_return(__wrap_deviceServiceGetDevicesBySubsystem, devices);
    DeviceDescriptor *dd = createDeviceDescriptor("dummy","dummy","1","0x00000001");
    // Add a newer version as latest with the dummy firmware file
    dd->latestFirmware = (DeviceFirmware *)calloc(1, sizeof(DeviceFirmware));
    dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA;
    dd->latestFirmware->version = strdup("0x00000002");
    dd->latestFirmware->filenames = linkedListCreate();
    linkedListAppend(dd->latestFirmware->filenames, strdup(DUMMY_OTA_FIRMWARE_FILE));
    // Add the firmware file
    will_return(__wrap_deviceServiceGetDeviceDescriptorForDevice, dd);
    // Create some dummy firmware files
    createDummyFirmwareFile(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);

    // Make the call
    zigbeeSubsystemCleanupFirmwareFiles();

    // Check if the files are still there
    char *otaPath = getDummyFirmwareFilePath(DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA);
    struct stat statBuf;
    assert_return_code(stat(otaPath, &statBuf), errno);
    free(otaPath);

    (void) state;
}

static void test_encodeDecodeIcDiscoveredDeviceDetails(void **state)
{
    IcDiscoveredDeviceDetails* details = (IcDiscoveredDeviceDetails*)calloc(1, sizeof(IcDiscoveredDeviceDetails));

    details->eui64 = 0x1234567887654321;
    details->manufacturer = strdup("acme");
    details->model = strdup("rocket");
    details->hardwareVersion = 0x722222222222;
    details->firmwareVersion = 0x333333333333;
    details->appVersion = 43;
    details->powerSource = powerSourceBattery;
    details->deviceType = deviceTypeEndDevice;
    details->numEndpoints = 1;
    details->endpointDetails = (IcDiscoveredEndpointDetails*)calloc(1, sizeof(IcDiscoveredEndpointDetails));
    details->endpointDetails[0].endpointId = 5;
    details->endpointDetails[0].appDeviceId = 4;
    details->endpointDetails[0].appDeviceVersion = 3;
    details->endpointDetails[0].appProfileId = 7;

    details->endpointDetails[0].numServerClusterDetails = 1;
    details->endpointDetails[0].serverClusterDetails = (IcDiscoveredClusterDetails*)calloc(1, sizeof(IcDiscoveredClusterDetails));
    details->endpointDetails[0].serverClusterDetails[0].isServer = true;
    details->endpointDetails[0].serverClusterDetails[0].clusterId = 0x0b05;
    details->endpointDetails[0].serverClusterDetails[0].numAttributeIds = 1;
    details->endpointDetails[0].serverClusterDetails[0].attributeIds = (uint16_t*)calloc(1, sizeof(uint16_t));
    details->endpointDetails[0].serverClusterDetails[0].attributeIds[0] = 3;

    details->endpointDetails[0].numClientClusterDetails = 1;
    details->endpointDetails[0].clientClusterDetails = (IcDiscoveredClusterDetails*)calloc(1, sizeof(IcDiscoveredClusterDetails));
    details->endpointDetails[0].clientClusterDetails[0].isServer = true;
    details->endpointDetails[0].clientClusterDetails[0].clusterId = 2;
    details->endpointDetails[0].clientClusterDetails[0].numAttributeIds = 1;
    details->endpointDetails[0].clientClusterDetails[0].attributeIds = (uint16_t*)calloc(1, sizeof(uint16_t));
    details->endpointDetails[0].clientClusterDetails[0].attributeIds[0] = 4;

    cJSON* detailsJson = icDiscoveredDeviceDetailsToJson(details);

    IcDiscoveredDeviceDetails* details2 = icDiscoveredDeviceDetailsFromJson(detailsJson);

    //now see if details1 and details2 are equal!
    assert_true(details->eui64 == details2->eui64);
    assert_string_equal(details->manufacturer, details2->manufacturer);
    assert_string_equal(details->model, details2->model);
    assert_true(details->hardwareVersion == details2->hardwareVersion);
    assert_true(details->firmwareVersion == details2->firmwareVersion);
    assert_true(details->appVersion == details2->appVersion);
    assert_true(details->powerSource == details2->powerSource);
    assert_true(details->deviceType == details2->deviceType);
    assert_true(details->numEndpoints == details2->numEndpoints);

    assert_true(details->endpointDetails[0].endpointId == details2->endpointDetails[0].endpointId);
    assert_true(details->endpointDetails[0].appProfileId == details2->endpointDetails[0].appProfileId);
    assert_true(details->endpointDetails[0].appDeviceVersion == details2->endpointDetails[0].appDeviceVersion);
    assert_true(details->endpointDetails[0].appDeviceId == details2->endpointDetails[0].appDeviceId);
    assert_true(details->endpointDetails[0].numServerClusterDetails == details2->endpointDetails[0].numServerClusterDetails);
    assert_true(details->endpointDetails[0].numClientClusterDetails == details2->endpointDetails[0].numClientClusterDetails);

    assert_true(details->endpointDetails[0].serverClusterDetails[0].clusterId == details2->endpointDetails[0].serverClusterDetails[0].clusterId);
    assert_true(details->endpointDetails[0].serverClusterDetails[0].isServer == details2->endpointDetails[0].serverClusterDetails[0].isServer);
    assert_true(details->endpointDetails[0].serverClusterDetails[0].attributeIds[0] == details2->endpointDetails[0].serverClusterDetails[0].attributeIds[0]);

    assert_true(details->endpointDetails[0].clientClusterDetails[0].clusterId == details2->endpointDetails[0].clientClusterDetails[0].clusterId);
    assert_true(details->endpointDetails[0].clientClusterDetails[0].isServer == details2->endpointDetails[0].clientClusterDetails[0].isServer);
    assert_true(details->endpointDetails[0].clientClusterDetails[0].attributeIds[0] == details2->endpointDetails[0].clientClusterDetails[0].attributeIds[0]);

    freeIcDiscoveredDeviceDetails(details);
    freeIcDiscoveredDeviceDetails(details2);
    cJSON_Delete(detailsJson);
}

// ******************************
// Setup/Teardown
// ******************************

static int dynamicDirSetup(void **state)
{
    dynamicDir = mkdtemp(templateTempDir);

    assert_non_null(dynamicDir);

    (void)state;

    return 0;
}

static int dynamicDirTeardown(void **state)
{
    if (dynamicDir != NULL)
    {
        char buf[255];
        sprintf(buf, "rm -rf %s", dynamicDir);
        system(buf);
        dynamicDir = NULL;
    }


    (void)state;

    return 0;
}

// ******************************
// Helpers
// ******************************

static icDevice *createDummyDevice(const char *manufacturer, const char *model, const char *hardwareVersion, const char *firmwareVersion)
{
    ++counter;
    char uuid[255];
    sprintf(uuid,"device%d", counter);
    icDevice *device = createDevice(uuid, "dummy", 1, "dummy", NULL);
    createDeviceResource(device, COMMON_DEVICE_RESOURCE_MANUFACTURER, manufacturer, RESOURCE_TYPE_STRING, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createDeviceResource(device, COMMON_DEVICE_RESOURCE_MODEL, model, RESOURCE_TYPE_STRING, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createDeviceResource(device, COMMON_DEVICE_RESOURCE_HARDWARE_VERSION, hardwareVersion, RESOURCE_TYPE_VERSION, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    createDeviceResource(device, COMMON_DEVICE_RESOURCE_FIRMWARE_VERSION, firmwareVersion, RESOURCE_TYPE_VERSION, RESOURCE_MODE_READABLE, CACHING_POLICY_ALWAYS);
    return device;
}

static DeviceDescriptor *createDeviceDescriptor(const char *manufacturer, const char *model, const char *hardwareVersion, const char *firmwareVersion)
{
    ++counter;
    char uuid[255];
    sprintf(uuid,"dd%d", counter);
    DeviceDescriptor *dd = (DeviceDescriptor *)calloc(1, sizeof(DeviceDescriptor));
    dd->uuid = strdup(uuid);
    dd->model = strdup(model);
    dd->manufacturer = strdup(manufacturer);
    dd->firmwareVersions = (DeviceVersionList *)calloc(1, sizeof(DeviceVersionList));
    dd->firmwareVersions->listType = DEVICE_VERSION_LIST_TYPE_LIST;
    dd->firmwareVersions->list.versionList = linkedListCreate();
    linkedListAppend(dd->firmwareVersions->list.versionList, strdup(firmwareVersion));
    dd->hardwareVersions = (DeviceVersionList *)calloc(1, sizeof(DeviceVersionList));
    dd->hardwareVersions->listType = DEVICE_VERSION_LIST_TYPE_LIST;
    dd->hardwareVersions->list.versionList = linkedListCreate();
    linkedListAppend(dd->hardwareVersions->list.versionList, strdup(hardwareVersion));

    return dd;
}

static char *getDummyFirmwareFilePath(DeviceFirmwareType firmwareType)
{
    char pathBuf[255];
    char * dir = zigbeeSubsystemGetAndCreateFirmwareFileDirectory(firmwareType);
    if (firmwareType == DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA)
    {
        sprintf(pathBuf, "%s/" DUMMY_OTA_FIRMWARE_FILE, dir);
    }
    else if (firmwareType == DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY)
    {
        sprintf(pathBuf, "%s/" LEGACY_FIRMWARE_FILE, dir);
    }
    else
    {
        // Failure
        assert_false(true);
    }
    free(dir);

    return strdup(pathBuf);
}

static void createDummyFirmwareFile(DeviceFirmwareType firmwareType)
{
    assert_non_null(dynamicDir);
    char cmdBuf[255];

    char * path = getDummyFirmwareFilePath(firmwareType);
    sprintf(cmdBuf, "touch %s", path);
    system(cmdBuf);
    free(path);
}

// ******************************
// wrapped(mocked) functions
// ******************************

icLinkedList *__wrap_deviceServiceGetDevicesBySubsystem(const char *subsystem)
{
    icLogDebug(LOG_TAG, "%s: subsystem=%s", __FUNCTION__, subsystem);

    icLinkedList* result = (icLinkedList*)mock();
    return result;
}

DeviceDescriptor *__wrap_deviceServiceGetDeviceDescriptorForDevice(icDevice *device)
{
    icLogDebug(LOG_TAG, "%s: device UUID=%s", __FUNCTION__, device->uuid);

    DeviceDescriptor* result = (DeviceDescriptor*)mock();
    return result;
}

char *__wrap_getDynamicPath()
{
    assert_non_null(dynamicDir);

    icLogDebug(LOG_TAG, "%s = %s", __FUNCTION__, dynamicDir);

    return strdup(dynamicDir);
}

int main(int argc, const char ** argv)
{
    initIcLogger();

    const struct CMUnitTest tests[] =
        {
                cmocka_unit_test(test_zigbeeSubsystemCleanupFirmwareFiles),
                cmocka_unit_test(test_zigbeeSubsystemCleanupFirmwareFilesDoNothingIfFirmwareNeeded),
                cmocka_unit_test(test_encodeDecodeIcDiscoveredDeviceDetails)
        };

    int retval = cmocka_run_group_tests(tests, dynamicDirSetup, dynamicDirTeardown);

    closeIcLogger();

    return retval;
}
