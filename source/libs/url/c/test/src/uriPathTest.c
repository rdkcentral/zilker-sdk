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

/*-----------------------------------------------
 * uriPathTest.c
 *
 * simple unit test to validate the 'uriDispatcher' code
 *
 * Author: jelderton -  9/1/16.
 *-----------------------------------------------*/

// cmocka & it's dependencies
#include <stddef.h>
#include <setjmp.h>
#include <stdarg.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icLog/logging.h>
#include <urlHelper/uriDispatcher.h>
#include <urlHelper/urlHelper.h>

/*
 * stub implementation of 'handleUriPath' function
 */
static void stubHandleUriPath(char *fullUri, icStringHashMap *variables, void *arg)
{
    // since unit test, just print out the variables from the map
    //
    icStringHashMapIterator *loop = stringHashMapIteratorCreate(variables);
    while (stringHashMapIteratorHasNext(loop) == true)
    {
        char *key = NULL;
        char *val = NULL;
        stringHashMapIteratorGetNext(loop, &key, &val);
        printf("uri=%s key=%s val='%s'\n", fullUri, key, val);
    }
    stringHashMapIteratorDestroy(loop);
}

/*
 * breakup "cpe id" for devices that are generally concatenated
 * together into a single string - of "premise . device".  for
 * example, a camera id: "1521.123" where 1521 is the premise
 * and 123 is the camera uid.
 *
 * returns if the parse was successful and relies on the caller
 * to free the output string
 */
static bool stripPremiseDirective(char *input, char **valueStr, void *context)
{
    if (input == NULL)
    {
        return false;
    }

    // find the '.' char in the string
    //
    char *ptr = strchr(input, '.');
    if (ptr == NULL)
    {
        // dot not found, return false
        //
        return false;
    }

    // right side is the value
    //
    *valueStr = strdup(ptr + 1);

    return true;
}

/*
 * This test is meant to be done with an instance of zilker running already (as it makes IPC requests). It is largely
 * meant to be a means for testing by inspection, not necessarily pass or fail.
 */
static void test_urlDispatcher(void **state)
{
    // make a dispatcher
    //
    uriDispatcher *disp = uriDispatcherCreate();

    uriDispatcherRegisterDirective(disp, "stripPremise", stripPremiseDirective, NULL);
    uriDispatcherRegisterDirective(disp, "anotherDirective", stripPremiseDirective, NULL);

    // add some paths that should work
    //
    uriDispatchAddResult rc;
    rc = registerUriHandler(disp, "/icontrol/sites/[siteId]/network/cameras/[cameraId]", "cam uri", stubHandleUriPath);
    rc = registerUriHandler(disp, "/icontrol/sites/[siteId]/network/rules/[ruleId]", "rule uri", stubHandleUriPath);
    rc = registerUriHandler(disp, "/icontrol/sites/[siteId]/network/zdif/discover", "zdif uri", stubHandleUriPath);
    rc = registerUriHandler(disp, "/icontrol/sites/[siteId]/testDirective/[cameraId#stripPremise]", "premise-strip-test", stubHandleUriPath);

    // should get a 'dup var' error
    rc = registerUriHandler(disp, "/icontrol/sites/[failure]/network", NULL, stubHandleUriPath);

    // should get a 'dup handler' error
    rc = registerUriHandler(disp, "/icontrol/sites/[siteId]/network/zdif/discover", NULL, stubHandleUriPath);

    // should get a unknown directive error
    rc = registerUriHandler(disp, "/icontrol/sites/[siteId]/invalidDirective/[cameraId#invalidDirective]", NULL, stubHandleUriPath);

    // should get a 'dup var' error
    rc = registerUriHandler(disp, "/icontrol/sites/[siteId]/testDirective/[cameraId#anotherDirective]", NULL, stubHandleUriPath);

    // now process a couple of URIs
    //
    uriHandlerContainer *search;
    icStringHashMap *values = stringHashMapCreate();
    if ((search = locateUriHandler(disp, "/icontrol/sites/1234/network/rules/1001", values)) != NULL)
    {
        search->handler("rules", values, NULL);
        uriHandlerContainerDestroy(search);
    }
    stringHashMapDestroy(values, NULL);

    values = stringHashMapCreate();
    if ((search = locateUriHandler(disp, "/icontrol/sites/4567/network/zdif/discover", values)) != NULL)
    {
        search->handler("discover", values, NULL);
        uriHandlerContainerDestroy(search);
    }
    stringHashMapDestroy(values, NULL);

    values = stringHashMapCreate();
    if ((search = locateUriHandler(disp, "/icontrol/sites/4567/testDirective/123.22334", values)) != NULL)
    {
        search->handler("discover", values, NULL);
        uriHandlerContainerDestroy(search);
    }
    stringHashMapDestroy(values, NULL);

    // should fail to locate a handle
    values = stringHashMapCreate();
    if ((search = locateUriHandler(disp, "/icontrol/sites/0000/network", values)) != NULL)
    {
        search->handler("FAIL", values, NULL);
        uriHandlerContainerDestroy(search);
    }
    stringHashMapDestroy(values, NULL);

    // cleanup
    //
    uriDispatcherDestroy(disp);
}

/*
 * This test is meant to be done with an instance of zilker running already (as it makes IPC requests). It is largely
 * meant to be a means for testing by inspection, not necessarily pass or fail.
 */
static void test_urlHelperExtractHost(void **state)
{
    // first make sure we can extract the hostname from the url string properly
    //
    printf("starting 'verify host' test\n");
    assert_true(urlHelperCanVerifyHost("http://testhost"));
    assert_true(urlHelperCanVerifyHost("https://testhost/"));
    assert_true(urlHelperCanVerifyHost("http://testhost:80"));
    assert_true(urlHelperCanVerifyHost("https://testhost:443/a/b/c"));
    assert_false(urlHelperCanVerifyHost("/testhost:443/no/workie"));
    assert_false(urlHelperCanVerifyHost(""));
    assert_false(urlHelperCanVerifyHost(NULL));

    // now see if we correctly fail urls with IPv4 and IPv6 addresses in the path
    //
    assert_false(urlHelperCanVerifyHost("https://72.13.22.5"));
    assert_false(urlHelperCanVerifyHost("https://72.13.22.5:443/test123/a/b/c"));
    assert_false(urlHelperCanVerifyHost("https://[fe80::a38c:fc6c:6e0d:87dd]/another/path"));
    assert_false(urlHelperCanVerifyHost("https://[fe80::a38c:fc6c:6e0d:87dd]:443/go/here?a=b"));

    // now some edge case urls
    //
    assert_true(urlHelperCanVerifyHost("https://7wishes.com/"));
}



void test_urlHelperCancel(void **state)
{
    urlHelperCancel("http://localhost:65535/fake");
    long httpCode;
    urlHelperDownloadFile("http://localhost:65535/fake",
                          &httpCode,
                          NULL,
                          NULL,
                          60,
                          SSL_VERIFY_NONE,
                          false,
                          "/dev/null");

    //TODO: this test can't detect failure in software -
    // urlHelper doesn't report what went wrong, which is important. It can be verified by log inspection.
}

/*
 * main
 */
int main(int argc, char *argv[])
{
    // init curl
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_TRACE);

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_urlDispatcher),
                    cmocka_unit_test(test_urlHelperExtractHost),
                    cmocka_unit_test(test_urlHelperCancel)
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    closeIcLogger();
    return retval;
}

