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
// Created by mkoch201 on 2/20/19.
//

// cmocka & it's dependencies
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>
#include <stdio.h>

#include <icLog/logging.h>
#include <xhCron/cronEventRegistrar.h>
#include <xhCron/cron_eventAdapter.h>

#define DUMMY_ENTRY_NAME "DUMMY"
#define DUMMY_ENTRY_SCHEDULE "0 1 * * *"
#define DUMMY_ENTRY_SCHEDULE2 "0 2 * * *"
#define DUMMY_ENTRY2_NAME "DUMMY2"
#define STATIC_PATH "/tmp"
#define ENTRY_LINE_FORMAT "%s "STATIC_PATH"/bin/xhCronEventUtil -b -n \"%s\""

static handleEvent_cronEvent eventListener = NULL;

/*
 * Wrapped functions
 */
int __wrap_addOrUpdatePreformattedCrontabEntry(const char *entryLine, const char *entryName)
{
    check_expected(entryLine);
    check_expected(entryName);

    return mock_type(int);
}

int __wrap_removeCrontabEntry(const char *entryName)
{
    check_expected(entryName);

    return mock_type(int);
}

void __wrap_register_cronEvent_eventListener(handleEvent_cronEvent listener)
{
    eventListener = listener;

    function_called();
}

void __wrap_unregister_cronEvent_eventListener(handleEvent_cronEvent listener)
{
    assert_ptr_equal(listener, eventListener);
    eventListener = NULL;

    function_called();
}

char *__wrap_getStaticPath()
{
    return strdup(STATIC_PATH);
}

/*
 * setup before event registrar tests
 */
static int test_testSetup(void **state)
{
    return 0;
}

/*
 * teardown after event registrar tests
 */
static int test_testTeardown(void **state)
{
    return 0;
}

/*
 * Callback function used for test
 */
static bool dummyCallback(const char *name)
{
    assert_string_equal(name, DUMMY_ENTRY_NAME);
    function_called();
    return false;
}

/*
 * Callback function used for test
 */
static bool dummy2Callback(const char *name)
{
    assert_string_equal(name, DUMMY_ENTRY2_NAME);
    function_called();
    return false;
}

/**
 * Helper to determine the expected entry line
 * @param name the name
 * @param schedule the schedule
 * @return the entry line, caller frees
 */
static char *getEntryLine(const char *name, const char *schedule)
{
    char *entryLine = NULL;
    int size = snprintf(entryLine, 0, ENTRY_LINE_FORMAT, schedule, name);
    entryLine = (char *)malloc(size+1);
    snprintf(entryLine, size+1, ENTRY_LINE_FORMAT, schedule, name);

    return entryLine;
}

/**
 * Helper to register a entry and assert we got what expected
 * @param name the name
 * @param handler the handle
 * @param expectRegister whether we expect this to regsiter a cron listener
 */
static void doRegister(const char *name, cronEventHandler handler, bool expectRegister)
{
    char *entryLine = getEntryLine(name, DUMMY_ENTRY_SCHEDULE);
    expect_string(__wrap_addOrUpdatePreformattedCrontabEntry, entryName, name);
    expect_string(__wrap_addOrUpdatePreformattedCrontabEntry, entryLine, entryLine);
    will_return(__wrap_addOrUpdatePreformattedCrontabEntry, 0);
    if (expectRegister)
    {
        expect_function_call(__wrap_register_cronEvent_eventListener);
    }
    assert_true(registerForCronEvent(name, DUMMY_ENTRY_SCHEDULE, handler));

    assert_non_null(eventListener);

    free(entryLine);
}

/**
 * Helper to simulate a sent cron event
 * @param name the name in the event
 * @return the event sent
 */
static cronEvent *sendEvent(const char *name)
{
    cronEvent *event = create_cronEvent();
    event->name = strdup(name);
    eventListener(event);
    return event;
}


static void test_registerForCronEvent(void **state)
{
    doRegister(DUMMY_ENTRY_NAME, dummyCallback, true);

    // Simulate a cron event
    expect_function_call(dummyCallback);
    cronEvent *event = sendEvent(DUMMY_ENTRY_NAME);

    // Cleanup
    destroy_cronEvent(event);
    expect_function_call(__wrap_unregister_cronEvent_eventListener);
    unregisterForCronEvent(DUMMY_ENTRY_NAME, false);
    assert_null(eventListener);
}

static void test_unregisterAndRemove(void **state)
{
    doRegister(DUMMY_ENTRY_NAME, dummyCallback, true);

    // Test the unregister with remove
    expect_string(__wrap_removeCrontabEntry, entryName, DUMMY_ENTRY_NAME);
    will_return(__wrap_removeCrontabEntry, 0);
    expect_function_call(__wrap_unregister_cronEvent_eventListener);
    unregisterForCronEvent(DUMMY_ENTRY_NAME, true);
    assert_null(eventListener);
}

static void test_registerForMultipleCronEvents(void **state)
{
    doRegister(DUMMY_ENTRY_NAME, dummyCallback, true);
    doRegister(DUMMY_ENTRY2_NAME, dummy2Callback, false);

    // Simulate a cron event
    expect_function_call(dummyCallback);
    cronEvent *event = sendEvent(DUMMY_ENTRY_NAME);
    expect_function_call(dummy2Callback);
    cronEvent *event2 = sendEvent(DUMMY_ENTRY2_NAME);

    // Cleanup
    destroy_cronEvent(event);
    destroy_cronEvent(event2);
    unregisterForCronEvent(DUMMY_ENTRY2_NAME, false);
    expect_function_call(__wrap_unregister_cronEvent_eventListener);
    unregisterForCronEvent(DUMMY_ENTRY_NAME, false);
    assert_null(eventListener);
}

static void test_updateCronEventSchedule(void **state)
{
    doRegister(DUMMY_ENTRY_NAME, dummyCallback, true);

    char *entryLine = getEntryLine(DUMMY_ENTRY_NAME, DUMMY_ENTRY_SCHEDULE2);
    expect_string(__wrap_addOrUpdatePreformattedCrontabEntry, entryName, DUMMY_ENTRY_NAME);
    expect_string(__wrap_addOrUpdatePreformattedCrontabEntry, entryLine, entryLine);
    will_return(__wrap_addOrUpdatePreformattedCrontabEntry, 0);
    updateCronEventSchedule(DUMMY_ENTRY_NAME, DUMMY_ENTRY_SCHEDULE2);

    // Cleanup
    expect_function_call(__wrap_unregister_cronEvent_eventListener);
    unregisterForCronEvent(DUMMY_ENTRY_NAME, false);
    assert_null(eventListener);
    free(entryLine);
}



/*
 * main
 */
int main(int argc, const char **argv)
{
    // init logging and set to ERROR so we don't output a ton of log lines
    // NOTE: feel free to make this INFO or DEBUG when debugging
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_DEBUG);

    // make our array of tests to run
    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test_setup_teardown(test_registerForCronEvent, test_testSetup, test_testTeardown),
                    cmocka_unit_test_setup_teardown(test_unregisterAndRemove, test_testSetup, test_testTeardown),
                    cmocka_unit_test_setup_teardown(test_registerForMultipleCronEvents, test_testSetup, test_testTeardown),
                    cmocka_unit_test_setup_teardown(test_updateCronEventSchedule, test_testSetup, test_testTeardown),
            };

    // fire off the suite of tests
    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    // shutdown
    closeIcLogger();

    return retval;
}