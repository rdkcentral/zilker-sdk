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
// Created by Boyd, Weston on 4/18/18.
//

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <cslt/cslt.h>

const char* icrule_spec = "<ns2:rule ruleID=\"1002152\" xmlns=\"http://ucontrol.com/smap/v2\" xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" xmlns:ns3=\"http://icontrol.com/statreports/v1.0\">\n"
        "    <ns2:triggerList>\n"
        "        <ns2:zoneTrigger>\n"
        "            <ns2:description>Zone Trigger</ns2:description>\n"
        "            <ns2:category>sensor</ns2:category>\n"
        "            <ns2:zoneState>open</ns2:zoneState>\n"
        "            <ns2:zoneID>49</ns2:zoneID>\n"
        "        </ns2:zoneTrigger>\n"
        "    </ns2:triggerList>\n"
        "    <ns2:action>\n"
        "        <ns2:actionID>70</ns2:actionID>\n"
        "        <ns2:parameter>\n"
        "            <ns2:key>lightID</ns2:key>\n"
        "            <ns2:value>3781220515476107</ns2:value>\n"
        "        </ns2:parameter>\n"
        "        <ns2:parameter>\n"
        "            <ns2:key>level</ns2:key>\n"
        "            <ns2:value>-1</ns2:value>\n"
        "        </ns2:parameter>\n"
        "        <ns2:parameter>\n"
        "            <ns2:key>duration</ns2:key>\n"
        "            <ns2:value>300</ns2:value>\n"
        "        </ns2:parameter>\n"
        "    </ns2:action>\n"
        "    <ns2:description>Turn On Master Closet Light</ns2:description>\n"
        "</ns2:rule>";

int icrule_test(void)
{
    return EXIT_SUCCESS;
}
