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

#include <icBuildtime.h>
#include <icUtil/regexUtils.h>
#include <stddef.h>
#include <icLog/logging.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <icTypes/sbrm.h>
#include <string.h>

#define LOG_TAG "regexTests"

static void test_credentialsReplacer(void **state)
{
    REGEX_SIMPLE_REPLACER(usernameReplacer, "<username>\\([^<>]*\\)</username>", NULL, "xxx-username-xxx");
    REGEX_REPLFLAGS_REPLACER(passwordReplacer, REGEX_GLOBAL, "<password>\\([^<>]*\\)</password>", NULL, "xxx-password-xxx");
    REGEX_SIMPLE_REPLACER(adminUserReplacer, "<adminUsername>\\([^<>]*\\)</adminUsername>", NULL, "xxx-adminUsername-xxx");
    REGEX_SIMPLE_REPLACER(adminPasswordReplacer, "<adminPassword>\\([^<>]*\\)</adminPassword>", NULL, "xxx-adminPassword-xxx");

    RegexReplacer *credentialsReplacers[] = {
            &adminUserReplacer,
            &adminPasswordReplacer,
            &usernameReplacer,
            &passwordReplacer,
            NULL
    };

    /* The extra password node is a global replacer sanity check. It is not real smap. */
    const char *cameraAdded = "<iq uri='cameraAdded'>\n"
                              "<smap xmlns=\"http://ucontrol.com/smap/v2\">\n"
                              " <cameraAddedEvent>\n"
                              "  <cpeGenId>361.88736</cpeGenId>\n"
                              "  <time>2015-09-29T15:22:23.526Z</time>\n"
                              "  <version>43</version>\n"
                              "  <source>cpeKeypad</source>\n"
                              "  <camera>\n"
                              "   <cameraCpeId>361.4</cameraCpeId>\n"
                              "   <manufacturer>iControl</manufacturer>\n"
                              "   <model>iCamera2</model>\n"
                              "   <macAddress>D4:21:22:C9:B4:33</macAddress>\n"
                              "   <serialNumber>D42122C9B433</serialNumber>\n"
                              "   <ipAddress>172.16.12.154</ipAddress>\n"
                              "   <label>My Camera 1</label>\n"
                              "   <adminUsername>testAdmin</adminUsername>\n"
                              "   <adminPassword>testPassword</adminPassword>\n"
                              "   <username>myUsername</username>\n"
                              "   <password>myPassword</password>\n"
                              "   <displayOrder>4</displayOrder>\n"
                              "   <firmwareVersion>3.0.01.32</firmwareVersion>\n"
                              "   <videoRecordable>true</videoRecordable>\n"
                              "   <videoFormat>MJPEG</videoFormat>\n"
                              "   <videoFormat>FLV</videoFormat>\n"
                              "   <videoFormat>RTSP</videoFormat>\n"
                              "   <videoCodec>H264</videoCodec>\n"
                              "   <videoCodec>MPEG4</videoCodec>\n"
                              "   <apiVersion>3.3</apiVersion>\n"
                              "   <motionCapable>true</motionCapable>\n"
                              "   <motionSensitivity>low</motionSensitivity>\n"
                              "   <inMotion>false</inMotion>\n"
                              "   <resolution>1280:720</resolution>\n"
                              "   <aspectRatio>16:9</aspectRatio>\n"
                              "   <password>somePassword</password>\n"
                              "  </camera>\n"
                              " </cameraAddedEvent>\n"
                              "</smap>\n"
                              "</iq>";

    regexInitReplacers(credentialsReplacers);
    AUTO_CLEAN(free_generic__auto) char *edited = regexReplace(cameraAdded, credentialsReplacers);

    printf("edited: %s", edited);

    assert_false(strstr(edited, "myPassword") ||
                 strstr(edited, "myUsername")    ||
                 strstr(edited, "testAdmin")     ||
                 strstr(edited, "testPassword")  ||
                 strstr(edited, "somePassword"));
}

static void test_subExpressionReplace(void **state)
{
    REGEX_REPLFLAGS_REPLACER(multiReplacer, REGEX_GLOBAL, "Test \\(\\(foo\\)*\\|\\(bar\\)\\)", NULL, NULL, "bz", "bifffff");

    RegexReplacer *replacers[] = {
            &multiReplacer,
            NULL
    };

    regexInitReplacers(replacers);

    const char *fixture = "Test foo bar Test bar";
    AUTO_CLEAN(free_generic__auto) char *edited = regexReplace(fixture, replacers);

    assert_string_equal("Test bz bar Test bifffff", edited);
}

static void test_zeroMatchReplacementIsNotInfinite(void **state)
{
    /* This replacer will match the zero-length subexpression 1, without matching any actual chars. */
    REGEX_REPLFLAGS_REPLACER(zeroWidthMatchReplacer, REGEX_GLOBAL, "\\(Test\\)*", NULL, "Test");
    RegexReplacer *replacers[] = {
            &zeroWidthMatchReplacer,
            NULL
    };

    regexInitReplacers(replacers);

    const char *fixture = "teSt testTestTest";
    AUTO_CLEAN(free_generic__auto) char *edited = regexReplace(fixture, replacers);
    assert_string_equal(fixture, edited);
}

int main(int argc, const char **argv)
{
    initIcLogger();

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_credentialsReplacer),
                    cmocka_unit_test(test_subExpressionReplace),
                    cmocka_unit_test(test_zeroMatchReplacementIsNotInfinite)
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    closeIcLogger();

    return retval;
}