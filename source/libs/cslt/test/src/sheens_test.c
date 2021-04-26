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

#include <cslt/sheens.h>
#include <icTypes/icLinkedList.h>
#include <cslt/icrules.h>

#include "test_internal.h"

static const char* sheens_spec = "{\n"
        "  \"doc\": \"A machine that turns on a light when a sensor faults and turns it off when it restores.\",\n"
        "  \"sheensVersion\": \"1.0\",\n"
        "  \"name\": \"SampleAutomation1\",\n"
        "  \"nodes\": {\n"
        "    \"cleanup\": {\n"
        "      \"action\": {\n"
        "        \"interpreter\": \"goja\",\n"
        "        \"source\": \"delete _.bindings[\\\"?x\\\"];\\nreturn _.bindings;\"\n"
        "      },\n"
        "      \"branching\": {\n"
        "        \"branches\": [\n"
        "          {\n"
        "            \"target\": \"listen\"\n"
        "          }\n"
        "        ]\n"
        "      }\n"
        "    },\n"
        "    \"listen\": {\n"
        "      \"branching\": {\n"
        "        \"branches\": [\n"
        "          {\n"
        "            \"pattern\": \"{\\\"topic\\\":\\\"resourceUpdated\\\", \\\"payload\\\": {\\\"content\\\": {\\\"value\\\":true}, \\\"source\\\": {\\\"resource\\\" : \\\"/dev/sensor/000d6f0004a60511/1\\\"}}}\\n\",\n"
        "            \"target\": \"turnOn\"\n"
        "          },\n"
        "          {\n"
        "            \"pattern\": \"{\\\"topic\\\":\\\"resourceUpdated\\\", \\\"payload\\\": {\\\"content\\\": {\\\"value\\\":false}, \\\"source\\\": {\\\"resource\\\" : \\\"/dev/sensor/000d6f0004a60511/1\\\"}}}\\n\",\n"
        "            \"target\": \"turnOff\"\n"
        "          }\n"
        "        ],\n"
        "        \"type\": \"message\"\n"
        "      }\n"
        "    },\n"
        "    \"start\": {\n"
        "      \"branching\": {\n"
        "        \"branches\": [\n"
        "          {\n"
        "            \"target\": \"listen\"\n"
        "          }\n"
        "        ]\n"
        "      }\n"
        "    },\n"
        "    \"turnOff\": {\n"
        "      \"action\": {\n"
        "        \"interpreter\": \"goja\",\n"
        "        \"source\": \"_.out({to: \\\"module\\\", name : \\\"ocfModule\\\", requestType : \\\"updateResource\\\", requestBody: {uri: \\\"/dev/light/000d6f00023dc83d/1\\\", rep: {value: false}}});\\nreturn _.bindings;\"\n"
        "      },\n"
        "      \"branching\": {\n"
        "        \"branches\": [\n"
        "          {\n"
        "            \"target\": \"listen\"\n"
        "          }\n"
        "        ]\n"
        "      }\n"
        "    },\n"
        "    \"turnOn\": {\n"
        "      \"action\": {\n"
        "        \"interpreter\": \"goja\",\n"
        "        \"source\": \"_.out({to: \\\"module\\\", name : \\\"ocfModule\\\", requestType : \\\"updateResource\\\", requestBody: {uri: \\\"/dev/light/000d6f00023dc83d/1\\\", rep: {value: true}}});\\nreturn _.bindings;\"\n"
        "      },\n"
        "      \"branching\": {\n"
        "        \"branches\": [\n"
        "          {\n"
        "            \"target\": \"listen\"\n"
        "          }\n"
        "        ]\n"
        "      }\n"
        "    }\n"
        "  },\n"
        "  \"parsepatterns\": true\n"
        "}";

static const char* icrule_spec = ""
    "<ns2:rule ruleID=\"1012718221\" "
    "xmlns=\"http://ucontrol.com/smap/v2\" "
    "xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" "
    "xmlns:ns3=\"http://icontrol.com/statreports/v1.0\">\n"
    "    <ns2:triggerList>\n"
    "        <ns2:sensorTrigger>\n"
    "            <ns2:description>Sensor Trigger</ns2:description>\n"
    "            <ns2:category>sensor</ns2:category>\n"
    "            <ns2:sensorState>openOrClose</ns2:sensorState>\n"
    "            <ns2:sensorType>door</ns2:sensorType>\n"
    "        </ns2:sensorTrigger>\n"
    "    </ns2:triggerList>\n"
    "    <ns2:action>\n"
    "        <ns2:actionID>70</ns2:actionID>\n"
    "        <ns2:parameter>\n"
    "            <ns2:key>lightID</ns2:key>\n"
    "            <ns2:value>000d6f0002a67cbe.1</ns2:value>\n"
    "        </ns2:parameter>\n"
    "        <ns2:parameter>\n"
    "            <ns2:key>duration</ns2:key>\n"
    "            <ns2:value>10</ns2:value>\n"
    "        </ns2:parameter>\n"
    "    </ns2:action>\n"
    "    <ns2:constraints>\n"
    "        <ns2:and-expression>\n"
    "            <ns2:timeConstraint>\n"
    "                <ns2:start>\n"
    "                    <ns2:exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 12:00</ns2:exactTime>\n"
    "                </ns2:start>\n"
    "                <ns2:end>\n"
    "                    <ns2:exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 11:59</ns2:exactTime>\n"
    "                </ns2:end>\n"
    "            </ns2:timeConstraint>\n"
    "        </ns2:and-expression>\n"
    "    </ns2:constraints>\n"
    "    <ns2:description>I want Window to send text message when it is opened </ns2:description>\n"
    "</ns2:rule>";

static const char* icrule_spec_tstat = ""
    "<ns2:rule ruleID=\"1001386770\" xmlns=\"http://ucontrol.com/smap/v2\" "
    "xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" "
    "xmlns:ns3=\"http://icontrol.com/statreports/v1.0\">\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SUN 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2333</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>MON 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>TUE 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>WED 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>THU 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>FRI 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SAT 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SUN 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2250</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>MON 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2944</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>TUE 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2944</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>WED 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2944</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>THU 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2944</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>FRI 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2944</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SAT 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2944</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SUN 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2388</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>MON 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>TUE 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>WED 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>THU 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>FRI 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SAT 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2555</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SUN 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2138</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>MON 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2777</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>TUE 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2777</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>WED 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2777</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>THU 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2777</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>FRI 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2777</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SAT 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>cool</ns2:mode>\n"
    "        <ns2:temperature>2777</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SUN 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>MON 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>TUE 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>WED 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>THU 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>FRI 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SAT 6:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SUN 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>MON 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>TUE 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>WED 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>THU 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>FRI 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SAT 8:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SUN 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2444</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>MON 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2444</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>TUE 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2444</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>WED 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2444</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>THU 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2444</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>FRI 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2444</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SAT 14:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2444</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SUN 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>MON 22:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>TUE 22:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>WED 22:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>THU 22:00</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>FRI 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:scheduleEntry thermostatID=\"244600000893D3.1\">\n"
    "        <ns2:timeSlot>\n"
    "            <ns2:exactTime>SAT 21:30</ns2:exactTime>\n"
    "        </ns2:timeSlot>\n"
    "        <ns2:mode>heat</ns2:mode>\n"
    "        <ns2:temperature>2222</ns2:temperature>\n"
    "    </ns2:scheduleEntry>\n"
    "    <ns2:description>M Bed Thermostat Schedule</ns2:description>\n"
    "</ns2:rule>";

static const char* icrule_spec_timertrigger_test = ""
    "<ns2:rule ruleID=\"1000775499\" xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" xmlns=\"http://ucontrol.com/smap/v2\" xmlns:ns3=\"http://icontrol.com/statreports/v1.0\">\n"
    "    <ns2:triggerList>\n"
    "        <ns2:timeTrigger>\n"
    "            <ns2:description>Time  Trigger</ns2:description>\n"
    "            <ns2:category>time</ns2:category>\n"
    "            <ns2:when>\n"
    "                <ns2:exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 21:00</ns2:exactTime>\n"
    "            </ns2:when>\n"
    "        </ns2:timeTrigger>\n"
    "    </ns2:triggerList>\n"
    "    <ns2:action>\n"
    "        <ns2:actionID>70</ns2:actionID>\n"
    "        <ns2:parameter>\n"
    "            <ns2:key>lightID</ns2:key>\n"
    "            <ns2:value>000d6f0002a67cbe.1</ns2:value>\n"
    "        </ns2:parameter>\n"
    "        <ns2:parameter>\n"
    "            <ns2:key>duration</ns2:key>\n"
    "            <ns2:value>7200</ns2:value>\n"
    "        </ns2:parameter>\n"
    "    </ns2:action>\n"
    "    <ns2:description>Office Light</ns2:description>\n"
    "</ns2:rule>";

static const char* icrule_spec_timetrigger_repeat_test = ""
    "<ns2:rule ruleID=\"1012673665\" xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" xmlns=\"http://ucontrol.com/smap/v2\" xmlns:ns3=\"http://icontrol.com/statreports/v1.0\">\n"
    "    <ns2:triggerList>\n"
    "        <ns2:timeTrigger>\n"
    "            <ns2:description>Time  Trigger</ns2:description>\n"
    "            <ns2:category>time</ns2:category>\n"
    "            <ns2:when>\n"
    "                <ns2:exactTime>SUN,MON,TUE,WED,THU,FRI,SAT sunset</ns2:exactTime>\n"
    "            </ns2:when>\n"
    "            <ns2:end>\n"
    "                <ns2:exactTime>SUN,MON,TUE,WED,THU,FRI,SAT sunrise</ns2:exactTime>\n"
    "            </ns2:end>\n"
    "            <ns2:repeat>5</ns2:repeat>\n"
    "            <ns2:randomize>false</ns2:randomize>\n"
    "        </ns2:timeTrigger>\n"
    "    </ns2:triggerList>\n"
    "    <ns2:action>\n"
    "        <ns2:actionID>70</ns2:actionID>\n"
    "        <ns2:parameter>\n"
    "            <ns2:key>lightID</ns2:key>\n"
    "            <ns2:value>000d6f0002a67cbe.1</ns2:value>\n"
    "        </ns2:parameter>\n"
    "        <ns2:parameter>\n"
    "            <ns2:key>duration</ns2:key>\n"
    "            <ns2:value>7200</ns2:value>\n"
    "        </ns2:parameter>\n"
    "    </ns2:action>\n"
    "    <ns2:description>Take Video Clip from Driveway Camera</ns2:description>\n"
    "</ns2:rule>";

static const char* icrule_spec_timetrigger_test = "<ns2:rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"32030\">\n"
                                                  "    <ns2:triggerList>\n"
                                                  "        <ns2:timeTrigger>\n"
                                                  "            <ns2:description>Time Trigger</ns2:description>\n"
                                                  "            <ns2:category>time</ns2:category>\n"
                                                  "            <ns2:when>\n"
                                                  "                <ns2:exactTime>SUN,MON,TUE,WED,THU,FRI,SAT sunset</ns2:exactTime>\n"
                                                  "            </ns2:when>\n"
                                                  "        </ns2:timeTrigger>\n"
                                                  "    </ns2:triggerList>\n"
                                                  "    <ns2:action>\n"
                                                  "        <ns2:actionID>70</ns2:actionID>\n"
                                                  "        <ns2:parameter>\n"
                                                  "            <ns2:key>lightID</ns2:key>\n"
                                                  "            <ns2:value>6251.000d6f00035fc532</ns2:value>\n"
                                                  "        </ns2:parameter>\n"
                                                  "        <ns2:parameter>\n"
                                                  "            <ns2:key>level</ns2:key>\n"
                                                  "            <ns2:value>-1</ns2:value>\n"
                                                  "        </ns2:parameter>\n"
                                                  "    </ns2:action>\n"
                                                  "    <ns2:description>When Any date Sunset, Turn On Light 1</ns2:description>\n"
                                                  "</ns2:rule>";

static void test_sensor_trigger(void** state)
{
    const char* xml;

    const cslt_transcoder_t* transcoder;
    char* buffer = NULL;
    char* str;
    int ret;

    unused(state);

    transcoder = cslt_get_transcoder_by_name(TRANSCODER_NAME_ICRULES, TRANSCODER_NAME_SHEENS);
    assert_non_null(transcoder);

    /*
     * Trigger: sensor Open
     * Action: Light On
     */
    xml = XML_HEADER
          "<ns2:rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"32509\">\n"
          "    <ns2:triggerList>\n"
          "        <ns2:sensorTrigger>\n"
          "            <ns2:description>Sensor Trigger</ns2:description>\n"
          "            <ns2:category>sensor</ns2:category>\n"
          "            <ns2:sensorState>open</ns2:sensorState>\n"
          "            <ns2:sensorID>1</ns2:sensorID>\n"
          "        </ns2:sensorTrigger>\n"
          "    </ns2:triggerList>\n"
          "    <ns2:action>\n"
          "        <ns2:actionID>70</ns2:actionID>\n"
          "        <ns2:parameter>\n"
          "            <ns2:key>lightID</ns2:key>\n"
          "            <ns2:value>6251.b0ce181403060f7c</ns2:value>\n"
          "        </ns2:parameter>\n"
          "        <ns2:parameter>\n"
          "            <ns2:key>level</ns2:key>\n"
          "            <ns2:value>100</ns2:value>\n"
          "        </ns2:parameter>\n"
          "    </ns2:action>\n"
          "    <ns2:description>When Door/Window Sensor 1 Open, Turn On Light 1</ns2:description>\n"
          "</ns2:rule>";

    ret = cslt_transcode(transcoder, xml, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);
    free(buffer);

    /*
     * Trigger: motion
     * Action: Email
     */
    xml = XML_HEADER
          "<rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" "
          "xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"1052597\">\n"
          "    <triggerList>\n"
          "        <sensorTrigger>\n"
          "            <description>Sensor Trigger</description>\n"
          "            <category>sensor</category>\n"
          "            <sensorState>trouble</sensorState>\n"
          "            <sensorType>allNonMotionZones</sensorType>\n"
          "        </sensorTrigger>\n"
          "    </triggerList>\n"
          "    <action>\n"
          "        <actionID>1</actionID>\n"
          "    </action>\n"
          "    <description>When Any Non-Motion Sensor Trouble, Send Email</description>\n"
          "</rule>";

    buffer = NULL;
    ret = cslt_transcode(transcoder, xml, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);

    str = strstr(buffer, ":255");
    assert_non_null(str);
    free(buffer);

    /*
     * Trigger: sensor open or closed
     * Action: Email
     */
    xml = XML_HEADER
          "<rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" "
          "xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"32393\">\n"
          "    <triggerList>\n"
          "        <sensorTrigger>\n"
          "            <description>Sensor Trigger</description>\n"
          "            <category>sensor</category>\n"
          "            <sensorState>openOrClose</sensorState>\n"
          "            <sensorType>door</sensorType>\n"
          "        </sensorTrigger>\n"
          "    </triggerList>\n"
          "    <action>\n"
          "        <actionID>21</actionID>\n"
          "        <parameter>\n"
          "            <key>cameraID</key>\n"
          "            <value>6251.7894b4e751bc</value>\n"
          "        </parameter>\n"
          "        <parameter>\n"
          "            <key/>\n"
          "            <value/>\n"
          "        </parameter>\n"
          "    </action>\n"
          "    <constraints>\n"
          "        <timeConstraint>\n"
          "            <start>\n"
          "                <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 17:10</exactTime>\n"
          "            </start>\n"
          "            <end>\n"
          "                <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 17:18</exactTime>\n"
          "            </end>\n"
          "        </timeConstraint>\n"
          "    </constraints>\n"
          "    <description>When Any Door Open or Close, Take Picture with Camera 1</description>\n"
          "</rule>";

    buffer = NULL;
    ret = cslt_transcode(transcoder, xml, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);
    free(buffer);

    /*
     * Trigger: Door sensor Open/Close
     * Action: Multi-Light On
     */
    xml = XML_HEADER
          "<ns2:rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"32592\">\n"
          "    <ns2:triggerList>\n"
          "        <ns2:sensorTrigger>\n"
          "            <ns2:description>Sensor Trigger</ns2:description>\n"
          "            <ns2:category>sensor</ns2:category>\n"
          "            <ns2:sensorState>openOrClose</ns2:sensorState>\n"
          "            <ns2:sensorType>door</ns2:sensorType>\n"
          "        </ns2:sensorTrigger>\n"
          "    </ns2:triggerList>\n"
          "    <ns2:action>\n"
          "        <ns2:actionID>70</ns2:actionID>\n"
          "        <ns2:parameter>\n"
          "            <ns2:key>lightID</ns2:key>\n"
          "            <ns2:value>6251.b0ce181403060de3,6251.b0ce181403060f7c</ns2:value>\n"
          "        </ns2:parameter>\n"
          "        <ns2:parameter>\n"
          "            <ns2:key>level</ns2:key>\n"
          "            <ns2:value>100,100</ns2:value>\n"
          "        </ns2:parameter>\n"
          "    </ns2:action>\n"
          "    <ns2:description>When Any Door Open or Close, Turn On Light 1,Light 2</ns2:description>\n"
          "</ns2:rule>";

    buffer = NULL;
    ret = cslt_transcode(transcoder, xml, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);
    free(buffer);
}

void test_negative_sensor_trigger(void** state)
{
    const char* xml;

    const cslt_transcoder_t* transcoder;
    char* buffer = NULL;
    char* str;
    int ret;

    unused(state);

    transcoder = cslt_get_transcoder_by_name(TRANSCODER_NAME_ICRULES, TRANSCODER_NAME_SHEENS);
    assert_non_null(transcoder);

    /*
     * Trigger: Not Open/Close During Time
     * Action: Email
     */
    xml = XML_HEADER
          "<rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" "
          "xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"32316\">\n"
          "    <triggerList isNegative=\"true\">\n"
          "        <sensorTrigger>\n"
          "            <description>Sensor Trigger</description>\n"
          "            <category>sensor</category>\n"
          "            <sensorState>openOrClose</sensorState>\n"
          "            <sensorID>1</sensorID>\n"
          "        </sensorTrigger>\n"
          "    </triggerList>\n"
          "    <action>\n"
          "        <actionID>2</actionID>\n"
          "    </action>\n"
          "    <constraints>\n"
          "        <and-expression>\n"
          "            <timeConstraint>\n"
          "                <start>\n"
          "                    <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 16:38</exactTime>\n"
          "                </start>\n"
          "                <end>\n"
          "                    <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 16:39</exactTime>\n"
          "                </end>\n"
          "            </timeConstraint>\n"
          "        </and-expression>\n"
          "    </constraints>\n"
          "    <description>When Door/Window Sensor 1 Does Not Open or Close, "
          "Send Text Message</description>\n"
          "</rule>";

    ret = cslt_transcode(transcoder, xml, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);

//    save_file("/tmp/negative_sensor_with_constraints_email_sheens.json", buffer, strlen(buffer));

    free(buffer);

    /*
     * Trigger: Not Open/Close During Time
     * Contraint: None
     * Action: Email
     */
    xml = XML_HEADER
          "<rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" "
          "xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"32316\">\n"
          "    <triggerList isNegative=\"true\">\n"
          "        <sensorTrigger>\n"
          "            <description>Sensor Trigger</description>\n"
          "            <category>sensor</category>\n"
          "            <sensorState>openOrClose</sensorState>\n"
          "            <sensorID>1</sensorID>\n"
          "        </sensorTrigger>\n"
          "    </triggerList>\n"
          "    <action>\n"
          "        <actionID>2</actionID>\n"
          "    </action>\n"
          "    <constraints>\n"
          "        <timeConstraint>\n"
          "            <start>\n"
          "                <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 16:38</exactTime>\n"
          "            </start>\n"
          "            <end>\n"
          "                <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 16:39</exactTime>\n"
          "            </end>\n"
          "        </timeConstraint>\n"
          "    </constraints>\n"
          "    <description>When Door/Window Sensor 1 Does Not Open or Close, "
          "Send Text Message</description>\n"
          "</rule>";

    buffer = NULL;
    ret = cslt_transcode(transcoder, xml, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);

    //    save_file("/tmp/negative_sensor_without_constraints_email_sheens.json", buffer, strlen(buffer));

    free(buffer);

}

static void test_multiaction(void** state)
{
    static const char* rule = XML_HEADER
                              "<rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" "
                              "xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"1051632\">\n"
                              "    <triggerList>\n"
                              "        <sensorTrigger>\n"
                              "            <description>Sensor Trigger</description>\n"
                              "            <category>sensor</category>\n"
                              "            <sensorState>open</sensorState>\n"
                              "            <sensorID>42</sensorID>\n"
                              "        </sensorTrigger>\n"
                              "    </triggerList>\n"
                              "    <action>\n"
                              "        <actionID>70</actionID>\n"
                              "        <parameter>\n"
                              "            <key>lightID</key>\n"
                              "            <value>000d6f000ad9cffe.1,000d6f000ae5dd94.1,000d6f000ad9e2e1.1</value>\n"
                              "        </parameter>\n"
                              "        <parameter>\n"
                              "            <key>level</key>\n"
                              "            <value>-1,-1,-1</value>\n"
                              "        </parameter>\n"
                              "        <parameter>\n"
                              "            <key>duration</key>\n"
                              "            <value>15,15,15</value>\n"
                              "        </parameter>\n"
                              "    </action>\n"
                              "    <description>When Garage Lights Open, Turn On Light1,Light2,Light3</description>\n"
                              "</rule>";

    const cslt_transcoder_t* transcoder;
    char* buffer = NULL;
    int ret;

    unused(state);

    transcoder = cslt_get_transcoder_by_name(TRANSCODER_NAME_ICRULES, TRANSCODER_NAME_SHEENS);
    assert_non_null(transcoder);

    ret = cslt_transcode(transcoder, rule, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);

//    save_file("/tmp/test_multiaction_sheens.json", buffer, strlen(buffer));

    free(buffer);
}

static void test_time_trigger(void** state)
{
    static const char* rule = XML_HEADER
                              "<ns2:rule xmlns:ns2=\"http://ucontrol.com/rules/v1.0\" xmlns:ns3=\"http://icontrol.com/statreports/v1.0\" ruleID=\"32430\">\n"
                              "    <ns2:triggerList>\n"
                              "        <ns2:timeTrigger>\n"
                              "            <ns2:description>Time Trigger</ns2:description>\n"
                              "            <ns2:category>time</ns2:category>\n"
                              "            <ns2:when>\n"
                              "                <ns2:exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 12:59</ns2:exactTime>\n"
                              "            </ns2:when>\n"
                              "        </ns2:timeTrigger>\n"
                              "    </ns2:triggerList>\n"
                              "    <ns2:action>\n"
                              "        <ns2:actionID>3</ns2:actionID>\n"
                              "    </ns2:action>\n"
                              "    <ns2:description>When Any date 12:59 PM, Send Notification</ns2:description>\n"
                              "</ns2:rule>";

    const cslt_transcoder_t* transcoder;
    char* buffer = NULL;
    int ret;

    unused(state);

    transcoder = cslt_get_transcoder_by_name(TRANSCODER_NAME_ICRULES, TRANSCODER_NAME_SHEENS);
    assert_non_null(transcoder);

    ret = cslt_transcode(transcoder, rule, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);

//    save_file("/tmp/test_time_trigger_sheens.json", buffer, strlen(buffer));

    free(buffer);
}

static void test_doorlock_trigger(void** state)
{
    static const char* rule = XML_HEADER
                              "<ns2:rule xmlns:ns2='http://ucontrol.com/rules/v1.0' xmlns:ns3='http://icontrol.com/statreports/v1.0' ruleID='32954'>\n"
                              "    <ns2:triggerList>\n"
                              "        <ns2:doorLockTrigger>\n"
                              "            <ns2:description>DoorLock</ns2:description>\n"
                              "            <ns2:category>doorLock</ns2:category>\n"
                              "            <ns2:doorLockState>lock</ns2:doorLockState>\n"
                              "            <ns2:doorLockID>6051.000d6f000c4a4a56</ns2:doorLockID>\n"
                              "        </ns2:doorLockTrigger>\n"
                              "    </ns2:triggerList>\n"
                              "    <ns2:action>\n"
                              "        <ns2:actionID>1</ns2:actionID>\n"
                              "    </ns2:action>\n"
                              "    <ns2:description>When DoorLock 1 Lock, Send Email to Weston Boyd</ns2:description>\n"
                              "</ns2:rule>";

    const cslt_transcoder_t* transcoder;
    char* buffer = NULL;
    char* str;
    int ret;

    unused(state);

    transcoder = cslt_get_transcoder_by_name(TRANSCODER_NAME_ICRULES, TRANSCODER_NAME_SHEENS);
    assert_non_null(transcoder);

    ret = cslt_transcode(transcoder, rule, &buffer);
    assert_int_not_equal(ret, -1);
    assert_non_null(buffer);

    str = strstr(buffer, "\"rootDeviceId\":\"000d6f000c4a4a56\"");
    assert_non_null(str);

    str = strstr(buffer, "\"id\":\"locked\"");
    assert_non_null(str);

    str = strstr(buffer, "\"value\":\"true\"");
    assert_non_null(str);

    save_file("/tmp/test_doorlock_trigger_sheens.json", buffer, strlen(buffer));

    free(buffer);
}

int sheens_test(void)
{
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_sensor_trigger),
            cmocka_unit_test(test_negative_sensor_trigger),
            cmocka_unit_test(test_multiaction),
            cmocka_unit_test(test_time_trigger),
            cmocka_unit_test(test_doorlock_trigger),
    };

    return cmocka_run_group_tests_name("sheens", tests, NULL, NULL);
}
