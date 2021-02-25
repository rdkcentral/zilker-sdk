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

#include <icLog/logging.h>
#include <unistd.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <automationService.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <icConfig/storage.h>
#include <propsMgr/propsHelper.h>
#include <cjson/cJSON.h>

static bool loadFile(const char* filepath, char** value);
bool __wrap_storageSave(const char* namespace, const char* key, const char* value);
bool __wrap_storageLoad(const char* namespace, const char* key, char** value);
bool __wrap_storageDelete(const char* namespace, const char* key);
icLinkedList* __wrap_storageGetKeys(const char* namespace);

//
// Created by Thomas Lea on 4/2/18
//
static void test_init_cleanup(void** state)
{
    //this test starts without any automations already existing
    will_return(__wrap_storageGetKeys, linkedListCreate());

    //address sanitizer checks
    automationServiceInitPhase1();
    automationServiceInitPhase2();
//    usleep(10000); //yikes
    automationServiceCleanup();

    (void) state;
}

extern bool automationEnginePost(cJSON* message);

static void test_simple_automation(void** state)
{
    //this test starts without any automations already existing
    will_return(__wrap_storageGetKeys, linkedListCreate());

    automationServiceInitPhase1();
    automationServiceInitPhase2();

    char* spec = NULL;
    assert_true(loadFile("TestAutomation1.js", &spec));

    assert_true(automationServiceAddMachine("TestAutomation1", spec, true));

    //TestAutomation1 waits for this message: {"type":"test"}
    // then emits: {to: "test", requestType : "dummyRequest"}
    // which expects a response: {"type":"dummyResponse"}
    // then returns to the 'listen' state

    cJSON *msg = cJSON_Parse("{\"type\":\"test\"}");
    automationEnginePost(msg);
    cJSON_Delete(msg);

    usleep(1000); //terrible.... dont leave this in

    char* machineState = NULL;
    assert_true(getMachineState("TestAutomation1", &machineState));
    assert_string_equal(machineState, "waitForDummyResponse");
    free(machineState);

    free(spec);
    automationServiceCleanup();

    (void) state;
}

static void test_thermostat_schedule_automation(void** state)
{
    //this test starts without any automations already existing
    will_return(__wrap_storageGetKeys, linkedListCreate());

    automationServiceInitPhase1();
    automationServiceInitPhase2();

    char* spec = NULL;
    assert_true(loadFile("ThermostatSchedule.js", &spec));

    assert_true(automationServiceAddMachine("ThermostatSchedule", spec, true));

    // simulate a timer tick
    cJSON *msg = cJSON_Parse("{ \n"
                             "   \"_evCode\":499,\n"
                             "   \"_sunrise\":1569327600,\n"
                             "   \"_sunset\":1569371220,\n"
                             "   \"_systemStatus\":\"home\",\n"
                             "   \"_evTime\":1569360600\n"
                             "}");
    automationEnginePost(msg);
    cJSON_Delete(msg);

    usleep(1000); //terrible.... dont leave this in

    char* machineState = NULL;
    assert_true(getMachineState("ThermostatSchedule", &machineState));
    assert_string_equal(machineState, "start");
    free(machineState);

    free(spec);
    automationServiceCleanup();

    (void) state;
}

static void test_remove_automation(void** state)
{
    //this test starts without any automations already existing
    will_return(__wrap_storageGetKeys, linkedListCreate());

    automationServiceInitPhase1();
    automationServiceInitPhase2();

    char* spec = NULL;
    assert_true(loadFile("TestAutomation1.js", &spec));

    assert_true(automationServiceAddMachine("TestAutomation1", spec, true));

    icLinkedList* infos = automationServiceGetMachineInfos();
    assert_non_null(infos);
    assert_int_equal(linkedListCount(infos), 1);
    linkedListDestroy(infos, machineInfoFreeFunc);

    // the delete operation should succeed
    will_return(__wrap_storageDelete, true);
    expect_function_call(__wrap_storageDelete);
    assert_true(automationServiceRemoveMachine("TestAutomation1"));
    infos = automationServiceGetMachineInfos();
    assert_non_null(infos);
    assert_int_equal(linkedListCount(infos), 0);
    linkedListDestroy(infos, machineInfoFreeFunc);

    free(spec);
    automationServiceCleanup();
    (void) state;
}

static void test_load_automation(void** state)
{
    //this test starts with a single automation ready to be loaded
    icLinkedList* automationKeys = linkedListCreate();
    linkedListAppend(automationKeys, strdup("TestAutomation1"));
    will_return(__wrap_storageGetKeys, automationKeys);

    // the load operation should succeed
    will_return(__wrap_storageLoad, true);
    expect_function_call(__wrap_storageLoad);

    automationServiceInitPhase1();
    automationServiceInitPhase2();

    //The machine will be in start state, nothing has happened
    char* machineState = NULL;
    assert_true(getMachineState("TestAutomation1", &machineState));
    assert_string_equal(machineState, "start");

    free(machineState);
    automationServiceCleanup();
    (void) state;
}

static void test_enable_disable_automation(void** state)
{
    //this test starts with a single automation ready to be loaded
    icLinkedList* automationKeys = linkedListCreate();
    linkedListAppend(automationKeys, strdup("TestAutomation1"));
    will_return(__wrap_storageGetKeys, automationKeys);

    // the load operation should succeed
    will_return(__wrap_storageLoad, true);
    expect_function_call(__wrap_storageLoad);

    automationServiceInitPhase1();
    automationServiceInitPhase2();

    //The machine will be in start state, nothing has happened
    char* machineState = NULL;
    assert_true(getMachineState("TestAutomation1", &machineState));
    assert_string_equal(machineState, "start");
    free(machineState);

    icLinkedList* infos = automationServiceGetMachineInfos();
    assert_non_null(infos);
    assert_int_equal(linkedListCount(infos), 1);
    MachineInfo* info = (MachineInfo*)linkedListGetElementAt(infos,0);
    assert_true(info->enabled);
    linkedListDestroy(infos, machineInfoFreeFunc);

    assert_true(automationServiceSetMachineEnabled("TestAutomation1", false));

    infos = automationServiceGetMachineInfos();
    assert_non_null(infos);
    assert_int_equal(linkedListCount(infos), 1);
    info = (MachineInfo*)linkedListGetElementAt(infos,0);
    assert_false(info->enabled);
    linkedListDestroy(infos, machineInfoFreeFunc);

    assert_true(automationServiceSetMachineEnabled("TestAutomation1", true));

    infos = automationServiceGetMachineInfos();
    assert_non_null(infos);
    assert_int_equal(linkedListCount(infos), 1);
    info = (MachineInfo*)linkedListGetElementAt(infos,0);
    assert_true(info->enabled);
    linkedListDestroy(infos, machineInfoFreeFunc);

    assert_true(getMachineState("TestAutomation1", &machineState));
    assert_string_equal(machineState, "start");
    free(machineState);

    automationServiceCleanup();
    (void) state;
}

int main(int argc, const char** argv)
{
    initIcLogger();

    setPropertyValue("IC_DYNAMIC_DIR", "/tmp/test", true, PROPERTY_SRC_DEVICE);

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_init_cleanup),
                    cmocka_unit_test(test_simple_automation),
                    cmocka_unit_test(test_remove_automation),
                    cmocka_unit_test(test_load_automation),
                    cmocka_unit_test(test_enable_disable_automation),
                    cmocka_unit_test(test_thermostat_schedule_automation),
            };

    return cmocka_run_group_tests(tests, NULL, NULL);

    closeIcLogger();

    return 0;
}


static bool loadFile(const char* filepath, char** value)
{
    bool result = true;

    *value = 0;
    long length;
    FILE* f = fopen(filepath, "r");

    if (f)
    {
        //determine the length of the receiving buffer
        if (fseek(f, 0, SEEK_END) != 0)
        {
            icLogError(LOG_TAG, "loadFile: failed to fseek %s: %s", filepath, strerror(errno));
            result = false;
            goto exit;
        }

        length = ftell(f);
        if (length == -1)
        {
            icLogError(LOG_TAG, "loadFile: failed to ftell %s: %s", filepath, strerror(errno));
            result = false;
            goto exit;
        }

        if (fseek(f, 0, SEEK_SET) != 0)
        {
            icLogError(LOG_TAG, "loadFile: failed to fseek %s: %s", filepath, strerror(errno));
            result = false;
            goto exit;
        }

        *value = malloc((size_t) length + 1);
        if (*value)
        {
            fread(*value, 1, (size_t) length, f);
            (*value)[length] = 0;
        }
        else
        {
            icLogError(LOG_TAG, "loadFile: failed to malloc for %s: %s", filepath, strerror(errno));
            result = false;
            goto exit;
        }
    }
    else
    {
        icLogError(LOG_TAG, "loadFile: failed to open %s: %s", filepath, strerror(errno));
        result = false;
        goto exit;
    }

    exit:
    if (f) fclose(f);
    return result;
}

// Mock out calls to storage for our tests
bool __wrap_storageSave(const char* namespace, const char* key, const char* value)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s, key=%s, value=%s", __FUNCTION__, namespace, key, value);
    return true;
}

bool __wrap_storageLoad(const char* namespace, const char* key, char** value)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s, key=%s", __FUNCTION__, namespace, key);
    function_called();

    bool shouldSucceed = (bool)mock();

    if(shouldSucceed)
    {
        icLogDebug(LOG_TAG, "%s: loading", __FUNCTION__);

        char* spec = NULL;
        assert_true(loadFile("TestAutomation1.js", &spec));

        cJSON* automation = cJSON_CreateObject();
        cJSON* specJson = cJSON_CreateString(spec);
        cJSON_AddItemToObject(automation, "spec", specJson);
        free(spec);
        cJSON_AddBoolToObject(automation, "enabled", cJSON_True);

        *value = cJSON_Print(automation);
        cJSON_Delete(automation);
    }
    else
    {
        icLogDebug(LOG_TAG, "%s: NOT loading", __FUNCTION__);
    }

    return shouldSucceed;
}

bool __wrap_storageDelete(const char* namespace, const char* key)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s, key=%s", __FUNCTION__, namespace, key);
    function_called();

    bool shouldSucceed = (bool)mock();

    if(shouldSucceed)
    {
        icLogDebug(LOG_TAG, "%s: deleting", __FUNCTION__);
    }
    else
    {
        icLogDebug(LOG_TAG, "%s: NOT deleting", __FUNCTION__);
    }

    return shouldSucceed;
}

icLinkedList* __wrap_storageGetKeys(const char* namespace)
{
    icLogDebug(LOG_TAG, "%s: namespace=%s", __FUNCTION__, namespace);

    icLinkedList* result = (icLinkedList*)mock();
    return result;
}
