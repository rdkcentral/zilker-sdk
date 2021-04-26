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
// Created by Thomas Lea on 7/30/15.
//

#include <deviceDescriptors.h>
#include <stdio.h>
#include <setjmp.h>
#include <cmocka.h>
#include <icLog/logging.h>
#include <versionUtils.h>
#include "../../src/parser.h"

/*
 * This tests the internal parser to ensure it parses all device descriptors from WhiteList.xml
 */
static void test_load_all_descriptors(void **state)
{
    icLinkedList* descriptors = parseDeviceDescriptors("data/WhiteList.xml", NULL);

    assert_non_null(descriptors);

    printf("loaded %d descriptors\n", linkedListCount(descriptors));
    linkedListIterate(descriptors, (linkedListIterateFunc)deviceDescriptorPrint, NULL);
    assert_int_equal(linkedListCount(descriptors), 129);

    linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);

    (void)state; //unused
}

/*
 * This tests the internal parser to ensure it parses all 9 camera device descriptors from WhiteList-9CameraDDs.xml
 */
static void test_load_camera_descriptors(void **state)
{
    icLinkedList* descriptors = parseDeviceDescriptors("data/WhiteList-9CameraDDs.xml", NULL);

    assert_non_null(descriptors);

    printf("loaded %d descriptors\n", linkedListCount(descriptors));
    linkedListIterate(descriptors, (linkedListIterateFunc)deviceDescriptorPrint, NULL);
    assert_int_equal(linkedListCount(descriptors), 9);

    linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);

    (void)state; //unused
}

/*
 * This tests the internal parser to ensure it parses all 120 zigbee device descriptors from WhiteList-ZigbeeDDs.xml
 */
static void test_load_zigbee_descriptors(void **state)
{
    icLinkedList* descriptors = parseDeviceDescriptors("data/WhiteList-ZigbeeDDs.xml", NULL);

    assert_non_null(descriptors);

    printf("loaded %d descriptors\n", linkedListCount(descriptors));
    linkedListIterate(descriptors, (linkedListIterateFunc)deviceDescriptorPrint, NULL);
    assert_int_equal(linkedListCount(descriptors), 120);

    linkedListDestroy(descriptors, (linkedListItemFreeFunc) deviceDescriptorFree);

    (void)state; //unused
}

/*
 * use the proper public interface to the library and locate a camera DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *list* of supported firmware versions.
 */
static void test_can_locate_camera_descriptor_from_version_list(void **state)
{
    deviceDescriptorsInit("data/WhiteList-9CameraDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("iControl", "RC8026", "1", "3.0.01.28");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void)state; //unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *list* of supported firmware versions.
 */
static void test_can_locate_zigbee_descriptor_from_version_list(void **state)
{
    deviceDescriptorsInit("data/WhiteList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("Bosch", "ISW-ZPR1-WP13", "1", "0x02030201");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    //zigbee descriptors can have hardware versions in decimal or hexidecimal.
    //  The library handles this by internally converting to decimal strings
    //  if required.
    dd = deviceDescriptorsGet("Sercomm Corp.", "SZ-DWS04", "18", "0x23005121");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void)state; //unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *range* of supported firmware versions.
 */
static void test_can_locate_zigbee_descriptor_from_range(void **state)
{
    deviceDescriptorsInit("data/WhiteList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x00750546");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void)state; //unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in a *range* of supported firmware versions.
 */
static void test_cant_locate_zigbee_descriptor_from_outside_range(void **state)
{
    deviceDescriptorsInit("data/WhiteList-ZigbeeDDs.xml", NULL);

    // Uses range 0x00750545-0x00840850

    // Just one after
    DeviceDescriptor *dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x00840851");

    assert_null(dd);

    // Well outside with a hex digit
    dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x0084085a");

    assert_null(dd);

    // Well outside with a hex digit
    dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x0084a850");

    assert_null(dd);

    // Just one before
    dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x00750544");

    assert_null(dd);

    // Well before
    dd = deviceDescriptorsGet("Yale", "YRD210 PB DB", "17", "0x0065a544");

    assert_null(dd);

    deviceDescriptorsCleanup();

    (void)state; //unused
}

/*
 * use the proper public interface to the library and locate a zigbee DD by
 * model, manufacturer, hardware version, and a firmware version that is a lookup
 * in wildcard of supported firmware versions.
 */
static void test_can_locate_zigbee_descriptor_with_wildcard(void **state)
{
    deviceDescriptorsInit("data/WhiteList-ZigbeeDDs.xml", NULL);

    DeviceDescriptor *dd = deviceDescriptorsGet("ACCTON", "SMCDW30-Z", "1", "0x00");

    assert_non_null(dd);

    deviceDescriptorFree(dd);

    deviceDescriptorsCleanup();

    (void)state; //unused
}

static void printArray(uint32_t *array, uint16_t len)
{
    int i = 0;
    for (i = 0 ; i < len ; i++)
    {
        printf("array[%d]=%d\n", i, array[i]);
    }
}

/*
 * test version parsing
 */
static void test_version_parsing(void **state)
{
    uint16_t arrayLen = 0;
    uint32_t *array = NULL;

    // simple version "1.2.3.4"
    //
    printf("parsing '1.2.3.4'\n");
    array = versionStringToInt("1.2.3.4", &arrayLen);
    assert_int_equal(arrayLen, 4);
    printArray(array, arrayLen);
    free(array);

    // version with alpha "1.2.R34"
    //
    printf("parsing '1.2.R34'\n");
    array = versionStringToInt("1.2.R34", &arrayLen);
    assert_int_equal(arrayLen, 3);
    printArray(array, arrayLen);
    free(array);

    // version with leading zeros "1.2.034"
    //
    printf("parsing '1.2.034'\n");
    array = versionStringToInt("1.2.034", &arrayLen);
    assert_int_equal(arrayLen, 3);
    printArray(array, arrayLen);
    free(array);

    // version with multiple crap chars "1.R2.X3__4010231_"
    //
    printf("parsing '1.R2.X3__4010231'\n");
    array = versionStringToInt("1.R2.X3__4010231", &arrayLen);
    assert_int_equal(arrayLen, 4);
    printArray(array, arrayLen);
    free(array);

    // version with no digits "RXJABC"
    //
    printf("parsing crap 'RXJABC'\n");
    array = versionStringToInt("RXJABC", &arrayLen);
    assert_int_equal(arrayLen, 0);
    free(array);

    // zigbee versions
    printf("parsing '0x00000001'\n");
    array = versionStringToInt("0x00000001", &arrayLen);
    assert_int_equal(arrayLen, 1);
    printArray(array, arrayLen);
    assert_int_equal(array[0], 1);
    free(array);

    printf("parsing '0X00000001'\n");
    array = versionStringToInt("0X00000001", &arrayLen);
    assert_int_equal(arrayLen, 1);
    printArray(array, arrayLen);
    assert_int_equal(array[0], 1);
    free(array);

    (void)state; //unused
}

/*
 * test version compare
 */
static void test_version_compare(void **state)
{
    int8_t val = 0;

    // check equal "1.2.3 == 1.2.3"
    //
    printf("comparing '1.2.3' vs '1.2.3'\n");
    val = compareVersionStrings("1.2.3", "1.2.3");
    assert_int_equal(val, 0);

    // left is greater "1.2.4 == 1.2.3"
    //
    printf("comparing '1.2.4' vs '1.2.3'\n");
    val = compareVersionStrings("1.2.4", "1.2.3");
    assert_int_equal(val, -1);

    // right is greater "1.2.3 == 1.2.4"
    //
    printf("comparing '1.2.3' vs '1.2.4'\n");
    val = compareVersionStrings("1.2.3", "1.2.4");
    assert_int_equal(val, 1);

    // special cases:  left longer
    //
    printf("comparing '1.2.3.4' vs '1.2.3'\n");
    val = compareVersionStrings("1.2.3.4", "1.2.3");
    assert_int_equal(val, -1);

    // special cases:  right longer
    //
    printf("comparing '1.2.3' vs '1.2.3.4'\n");
    val = compareVersionStrings("1.2.3", "1.2.3.4");
    assert_int_equal(val, 1);

    // special cases:  right longer with 0
    //
    printf("comparing '1.2.3' vs '1.2.3.0.00'\n");
    val = compareVersionStrings("1.2.3", "1.2.3.0.00");
    assert_int_equal(val, 0);

    // zigbee versions
    //
    printf("comparing '0x00750545' vs '0x00750546'\n");
    val = compareVersionStrings("0x00750545", "0x00750546");
    assert_int_equal(val, 1);

    printf("comparing '0x00840851' vs '0x00840850'\n");
    val = compareVersionStrings("0x00840851", "0x00840850");
    assert_int_equal(val, -1);

    printf("comparing '0x0084089a' vs '0x00840899'\n");
    val = compareVersionStrings("0x0084089a", "0x00840899");
    assert_int_equal(val, -1);

    (void)state; //unused
}

/*
 * Verify that a device descriptor can be found without a blacklist, but cannot be found when one is used that
 * references the device's descriptor.
 */
static void test_blacklist(void **state)
{
    //first confirm that we can find it without a blacklist
    deviceDescriptorsInit("data/WhiteList.xml", NULL);
    DeviceDescriptor *dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", "1");
    assert_non_null(dd);
    deviceDescriptorFree(dd);
    deviceDescriptorsCleanup();

    //now confirm that our blacklist, which excludes this device, prevents us from finding the same descriptor
    deviceDescriptorsInit("data/WhiteList.xml", "data/BlackList.xml");
    dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", "1");
    assert_null(dd);
    deviceDescriptorsCleanup();

    (void)state; //unused
}

/*
 * Verify proper handling of empty uuid nodes in blacklist.
 */
static void test_blacklist_empty_uuid(void **state)
{
    deviceDescriptorsInit("data/WhiteList.xml", "data/BlackList-EmptyUUID.xml");
    DeviceDescriptor *dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", "1");
    // don't care about the descriptors, just that parsing didn't crash the program
    deviceDescriptorFree(dd);
    deviceDescriptorsCleanup();

    (void)state; //unused
}

/*
 * Verify proper handling of missing uuid nodes in blacklist.
 */
static void test_blacklist_missing_uuid(void **state)
{
    deviceDescriptorsInit("data/WhiteList.xml", "data/BlackList-MissingUUID.xml");
    DeviceDescriptor *dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", "1");
    // don't care about the descriptors, just that parsing didn't crash the program
    deviceDescriptorFree(dd);
    deviceDescriptorsCleanup();

    (void)state; //unused
}

/*
 * Verify we don't crash when getting a device descriptor using null firmware/hardware versions.
 */
static void test_null_version_compare(void **state)
{
    // Get a basic whitelist/blacklist
    deviceDescriptorsInit("data/WhiteList.xml", "data/BlackList.xml");
    // Fetch a descriptor with null firmware version
    DeviceDescriptor *dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", "1", NULL);
    assert_null(dd);
    // Fetch a descriptor with null hardware version
    dd = deviceDescriptorsGet("SMC", "SMCCO10-Z", NULL, "1");
    assert_null(dd);

    deviceDescriptorsCleanup();

    (void) state;
}

int main(int argc, char ** argv)
{
    initIcLogger();

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_load_camera_descriptors),
                    cmocka_unit_test(test_load_zigbee_descriptors),
                    cmocka_unit_test(test_load_all_descriptors),
                    cmocka_unit_test(test_can_locate_camera_descriptor_from_version_list),
                    cmocka_unit_test(test_can_locate_zigbee_descriptor_from_version_list),
                    cmocka_unit_test(test_can_locate_zigbee_descriptor_with_wildcard),
                    cmocka_unit_test(test_can_locate_zigbee_descriptor_from_range),
                    cmocka_unit_test(test_cant_locate_zigbee_descriptor_from_outside_range),
                    cmocka_unit_test(test_version_parsing),
                    cmocka_unit_test(test_version_compare),
                    cmocka_unit_test(test_blacklist),
                    cmocka_unit_test(test_blacklist_empty_uuid),
                    cmocka_unit_test(test_blacklist_missing_uuid),
                    cmocka_unit_test(test_null_version_compare)
            };

    int rc = cmocka_run_group_tests(tests, NULL, NULL);
    closeIcLogger();
    return rc;
}
