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
// Created by gfaulk200 on 3/12/20.
//
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <inttypes.h>
#include <stdint.h>
#include <icUtil/stringUtils.h>
#include <libgen.h>
#include <icTypes/sbrm.h>
#include <icLog/logging.h>
#include <icUtil/fileUtils.h>
#include "propertyTypeDefinitions.h"
#include "propsService_ipc_handler.h"
#include "properties.h"

static const char *staticConfigDir;
static const char *dynamicConfigDir;
static const char *badStaticConfigDir;

char *__wrap_getStaticConfigPath()
{
    return mock_type(char *);
}

/**
 * this tests that we successfully parse a known good propertyTypesDefinition.json file;
 * it is also covered by the props set tests, but wanted an explicit test here
 */
void test_successfulPropDefsParse(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    assert_int_not_equal(getPropertyTypeDefsCount(),0);
    destroyPropertyTypeDefs();
}

/**
 * this tests that we do not crash when parsing a badly formed
 * propertyTypesDefinition.json file
 */
void test_badPropDefsParse(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(badStaticConfigDir));
    initPropertyTypeDefs();
    assert_int_equal(getPropertyTypeDefsCount(),0);
    destroyPropertyTypeDefs();
}

/**
 * this tests that when we set a property to a known good value, that it works
 */
void test_goodBooleanPropsSet(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    initProperties((char *)dynamicConfigDir, (char *)staticConfigDir);

    // setup the actual test
    AUTO_CLEAN(free_generic__auto) property *object = calloc(1,sizeof(property));
    object->key = "coredumps.save";
    object->value = "false";
    object->source = PROPERTY_SRC_SERVER;
    propertySetResult result;
    result.errorMessage = NULL;

    // make the request
    IPCCode ipcResult = handle_SET_CPE_PROPERTY_request(object,&result);

    // validate the expected results
    assert_int_equal(ipcResult,IPC_SUCCESS);
    assert_int_equal(result.result,PROPERTY_SET_OK);
    assert_null(result.errorMessage);

    //cleanup for ASAN
    destroyProperties();
    destroyPropertyTypeDefs();

    // cleanup the config dir we wrote config to
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);
}

/**
 * this tests that when we set a property to a known good value, that it works
 */
void test_goodInt32PropsSet(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    initProperties((char *)dynamicConfigDir, (char *)staticConfigDir);

    // setup the actual test
    AUTO_CLEAN(free_generic__auto) property *object = calloc(1,sizeof(property));
    object->key = "cpe.gatewaySync.retryMaxAttempts";
    object->value = "10";
    object->source = PROPERTY_SRC_SERVER;
    propertySetResult result;
    result.errorMessage = NULL;

    // make the request
    IPCCode ipcResult = handle_SET_CPE_PROPERTY_request(object,&result);

    // validate the expected results
    assert_int_equal(ipcResult,IPC_SUCCESS);
    assert_int_equal(result.result,PROPERTY_SET_OK);
    assert_null(result.errorMessage);

    // cleanup for ASAN
    destroyProperties();
    destroyPropertyTypeDefs();

    // cleanup the config dir we wrote config to
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);
}

/**
 * this tests that when we set a property to a known good value, that it works
 */
void test_goodUint32PropsSet(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    initProperties((char *)dynamicConfigDir, (char *)staticConfigDir);

    // setup the actual test
    AUTO_CLEAN(free_generic__auto) property *object = calloc(1,sizeof(property));
    object->key = "cpe.account.maxAllowedDaysWithInactiveAccount";
    object->value = "10";
    object->source = PROPERTY_SRC_SERVER;
    propertySetResult result;
    result.errorMessage = NULL;

    // send the request
    IPCCode ipcResult = handle_SET_CPE_PROPERTY_request(object,&result);

    // validate the expected results
    assert_int_equal(ipcResult,IPC_SUCCESS);
    assert_int_equal(result.result,PROPERTY_SET_OK);
    assert_null(result.errorMessage);

    // cleanup for ASAN
    destroyProperties();
    destroyPropertyTypeDefs();

    // cleanup the config dir we wrote config to
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);
}

/**
 * this tests that when we set a boolean property to a bad value, it fails correctly
 */
void test_badBooleanPropsSet(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    initProperties((char *)dynamicConfigDir, (char *)staticConfigDir);

    // setup the actual test
    AUTO_CLEAN(free_generic__auto) property *object = calloc(1,sizeof(property));
    object->key = "coredumps.save";
    object->value = "badValue";
    object->source = PROPERTY_SRC_SERVER;
    propertySetResult result;
    result.errorMessage = NULL;

    // make the request
    handle_SET_CPE_PROPERTY_request(object,&result);

    // validate the expected results
    assert_int_equal(result.result,PROPERTY_SET_VALUE_NOT_ALLOWED);
    assert_non_null(result.errorMessage);

    // cleanup for ASAN
    if (result.errorMessage != NULL)
    {
        free(result.errorMessage);
    }
    destroyProperties();
    destroyPropertyTypeDefs();

    // cleanup the config dir we wrote config too
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);
}

/**
 * this tests that when we set an int32 property to a bad value, it fails correctly
 */
void test_badInt32PropsSet(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    initProperties((char *)dynamicConfigDir, (char *)staticConfigDir);

    // setup the actual test
    AUTO_CLEAN(free_generic__auto) property *object = calloc(1,sizeof(property));
    object->key = "cpe.trouble.preLowBatteryDays";
    object->value = "valueBad";
    object->source = PROPERTY_SRC_SERVER;
    propertySetResult result;
    result.errorMessage = NULL;

    // make the request
    handle_SET_CPE_PROPERTY_request(object,&result);

    // validate the expected results
    assert_int_equal(result.result,PROPERTY_SET_VALUE_NOT_ALLOWED);
    assert_non_null(result.errorMessage);

    // cleanup for ASAN
    if (result.errorMessage != NULL)
    {
        free(result.errorMessage);
    }
    destroyProperties();
    destroyPropertyTypeDefs();

    // cleanup the config dir we wrote config too
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);
}

/**
 * this tests that when we set a uint32 property to a bad value, it fails correctly
 */
void test_badUInt32PropsSet(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    initProperties((char *)dynamicConfigDir, (char *)staticConfigDir);

    // setup the actual test
    AUTO_CLEAN(free_generic__auto) property *object = calloc(1,sizeof(property));
    object->key = "cpe.account.maxAllowedDaysWithInactiveAccount";
    object->value = "valueBad";
    object->source = PROPERTY_SRC_SERVER;
    propertySetResult result;
    result.errorMessage = NULL;

    // make the request
    handle_SET_CPE_PROPERTY_request(object,&result);

    // validate the expected results
    assert_int_equal(result.result,PROPERTY_SET_VALUE_NOT_ALLOWED);
    assert_non_null(result.errorMessage);

    // cleanup for ASAN
    if (result.errorMessage != NULL)
    {
        free(result.errorMessage);
    }
    destroyProperties();
    destroyPropertyTypeDefs();

    // cleanup the config dir we wrote config too
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);
}

/**
 * this tests that when we set a uint8 property to a bad value, it fails correctly
 */
void test_badUInt8PropsSet(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    initProperties((char *)dynamicConfigDir, (char *)staticConfigDir);

    // setup the actual test
    AUTO_CLEAN(free_generic__auto) property *object = calloc(1,sizeof(property));
    object->key = "cpe.zigbee.defender.panIdChangeThreshold";
    object->value = "valueBad";
    object->source = PROPERTY_SRC_SERVER;
    propertySetResult result;
    result.errorMessage = NULL;

    // make the request
    handle_SET_CPE_PROPERTY_request(object,&result);

    // validate the expected results
    assert_int_equal(result.result,PROPERTY_SET_VALUE_NOT_ALLOWED);
    assert_non_null(result.errorMessage);

    // cleanup for ASAN
    if (result.errorMessage != NULL)
    {
        free(result.errorMessage);
    }
    destroyProperties();
    destroyPropertyTypeDefs();

    // cleanup the config dir we wrote config too
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);
}

/**
 * this tests that when we set a property to a known good value, that it works
 */
void test_goodUInt8PropsSet(void **state)
{
    // because getStaticConfigPath() is called multiple times, and when property used
    // the caller is supposed to free the results, we need a dup copy here
    will_return(__wrap_getStaticConfigPath, strdup(staticConfigDir));
    initPropertyTypeDefs();
    initProperties((char *)dynamicConfigDir, (char *)staticConfigDir);

    // setup the actual test
    AUTO_CLEAN(free_generic__auto) property *object = calloc(1,sizeof(property));
    object->key = "cpe.zigbee.defender.panIdChangeThreshold";
    object->value = "10";
    object->source = PROPERTY_SRC_SERVER;
    propertySetResult result;
    result.errorMessage = NULL;

    // make the request
    IPCCode ipcResult = handle_SET_CPE_PROPERTY_request(object,&result);

    // validate the expected results
    assert_int_equal(ipcResult,IPC_SUCCESS);
    assert_int_equal(result.result,PROPERTY_SET_OK);
    assert_null(result.errorMessage);

    // cleanup for ASAN
    destroyProperties();
    destroyPropertyTypeDefs();

    // cleanup the config dir we wrote config to
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);
}

int main(int argc, char *argv[])
{
    initIcLogger();

    AUTO_CLEAN(free_generic__auto) char *file = strdup(__FILE__);
    const char *dir = dirname(file);
    AUTO_CLEAN(free_generic__auto) char *wd = stringBuilder("%s/..", dir);
    staticConfigDir = stringBuilder("%s/resources", wd);
    dynamicConfigDir = stringBuilder("%s/resources", wd);
    badStaticConfigDir = stringBuilder("%s/bad", staticConfigDir);
    // cleanup from any previous test failure
    AUTO_CLEAN(free_generic__auto) char *configDir = stringBuilder("%s/etc",dynamicConfigDir);
    deleteDirectory(configDir);

    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_successfulPropDefsParse),
            cmocka_unit_test(test_badPropDefsParse),
            cmocka_unit_test(test_goodBooleanPropsSet),
            cmocka_unit_test(test_badBooleanPropsSet),
            cmocka_unit_test(test_badInt32PropsSet),
            cmocka_unit_test(test_goodInt32PropsSet),
            cmocka_unit_test(test_badUInt32PropsSet),
            cmocka_unit_test(test_goodUint32PropsSet),
            cmocka_unit_test(test_badUInt8PropsSet),
            cmocka_unit_test(test_goodUInt8PropsSet)
    };

    int rc = cmocka_run_group_tests(tests, NULL, NULL);

    closeIcLogger();

    return rc;
}
