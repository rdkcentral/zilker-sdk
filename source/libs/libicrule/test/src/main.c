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
// Created by wboyd747 on 7/11/18.
//

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include <icrule/icrule.h>
#include <libxml2/libxml/tree.h>

#include "../../src/icrule_internal.h"
#include "../../src/icrule_action.h"
#include "../../src/icrule_constraint.h"
#include "../../src/icrule_trigger.h"

#define XML_HEADER "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"

static void load_xml(const char* xml, xmlDocPtr* doc, xmlNodePtr* top)
{
    *doc = xmlParseMemory(xml, (int) strlen(xml));
    assert_non_null(*doc);

    *top = xmlDocGetRootElement(*doc);
    assert_non_null(*top);
}

static icHashMap* load_action_map(void)
{
    char filename[255];
    icHashMap* actionMap = hashMapCreate();

    sprintf(filename, "%s/%s", icrule_get_action_list_dir(), "masterActionList.xml");

    if (icrule_action_list_load(filename, actionMap) < 0) {
        icrule_action_list_release(actionMap);
        return NULL;
    }

    sprintf(filename, "%s/%s", icrule_get_action_list_dir(), "internalActionList.xml");

    if (icrule_action_list_load(filename, actionMap) < 0) {
        icrule_action_list_release(actionMap);
        return NULL;
    }

    return actionMap;
}
static void test_parse_time(void** state)
{
    static const char* xml = XML_HEADER
                                  "<when>\n"
                                  "  <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT sunset</exactTime>\n"
                                  "</when>";

    xmlDocPtr doc;
    xmlNodePtr top;

    icrule_time_t rule_time;
    int ret;

    unused(state);

    doc = xmlParseMemory(xml, (int) strlen(xml));
    assert_non_null(doc);

    top = xmlDocGetRootElement(doc);
    assert_non_null(top);

    memset(&rule_time, 0, sizeof(icrule_time_t));

    ret = icrule_parse_time_slot(top, &rule_time);
    assert_int_equal(ret, 0);
    assert_int_equal(rule_time.day_of_week, 127);
    assert_int_equal(rule_time.time.sun_time, ICRULE_SUNTIME_SUNSET);
    assert_false(rule_time.use_exact_time);

    xmlFreeDoc(doc);
}

static void test_parse_constraint(void** state)
{
    static const char* xml = XML_HEADER
                             "<constraints>\n"
                             "    <and-expression>\n"
                             "        <timeConstraint>\n"
                             "            <start>\n"
                             "                <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 16:38</exactTime>\n"
                             "            </start>\n"
                             "            <end>\n"
                             "                <exactTime>SUN,MON,TUE,WED,THU,FRI,SAT 16:39</exactTime>\n"
                             "            </end>\n"
                             "        </timeConstraint>\n"
                             "    </and-expression>\n"
                             "</constraints>";

    xmlDocPtr doc;
    xmlNodePtr top;

    icLinkedList* constraints;
    icrule_constraint_t* constraint;

    int ret;

    unused(state);

    doc = xmlParseMemory(xml, (int) strlen(xml));
    assert_non_null(doc);

    top = xmlDocGetRootElement(doc);
    assert_non_null(top);

    constraints = linkedListCreate();
    assert_non_null(constraints);

    ret = icrule_parse_constraint(top, constraints, CONSTRAINT_LOGIC_AND);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(constraints), 1);

    constraint = linkedListGetElementAt(constraints, 0);
    assert_non_null(constraint);
    assert_int_equal(linkedListCount(constraint->child_constraints), 1);

    constraint = linkedListGetElementAt(constraint->child_constraints, 0);
    assert_non_null(constraint);
    assert_int_equal(linkedListCount(constraint->time_constraints), 1);

    linkedListDestroy(constraints, icrule_free_constraint);

    xmlFreeDoc(doc);
}

static void test_parse_action(void** state)
{
    static const char* video_xml = XML_HEADER
                                   "<action>\n"
                                   "  <actionID>22</actionID>\n"
                                   "  <parameter>\n"
                                   "    <key>cameraID</key>\n"
                                   "    <value>6051.944a0cfe4bc8</value>\n"
                                   "  </parameter>\n"
                                   "  <parameter>\n"
                                   "    <key/>\n"
                                   "    <value/>\n"
                                   "  </parameter>\n"
                                   "</action>";
    static const char* msg_xml = XML_HEADER
                                 "<action>\n"
                                 "  <actionID>1</actionID>\n"
                                 "</action>";

    xmlDocPtr doc;
    xmlNodePtr top;

    icrule_action_t* action;
    icrule_action_parameter_t* parameter;

    icLinkedList* actions = linkedListCreate();
    icHashMap* actionMap = load_action_map();

    unused(state);

    assert_non_null(actionMap);

    doc = xmlParseMemory(video_xml, (int) strlen(video_xml));
    assert_non_null(doc);

    top = xmlDocGetRootElement(doc);
    assert_non_null(top);

    assert_int_equal(icrule_parse_action(top, actions, actionMap), 0);
    assert_int_equal(linkedListCount(actions), 1);

    xmlFreeDoc(doc);

    action = linkedListGetElementAt(actions, 0);
    assert_non_null(action);

    assert_int_equal(action->id, 22);
    assert_int_equal(hashMapCount(action->parameters), 1);

    parameter = hashMapGet(action->parameters, "cameraID", (uint16_t) strlen("cameraID"));
    assert_non_null(parameter);

    assert_string_equal(parameter->key, "cameraID");
    assert_string_equal(parameter->value, "6051.944a0cfe4bc8");

    /* Now test the notification message attachment for
     * video/image and email/sms mechanism.
     */
    doc = xmlParseMemory(msg_xml, (int) strlen(msg_xml));
    assert_non_null(doc);

    top = xmlDocGetRootElement(doc);
    assert_non_null(top);

    assert_int_equal(icrule_parse_action(top, actions, actionMap), 0);

    xmlFreeDoc(doc);

    icrule_update_message_attachment(actions);

    action = linkedListGetElementAt(actions, 1);
    assert_non_null(action);

    assert_int_equal(action->id, 1);
    assert_int_equal(hashMapCount(action->parameters), 1);

    parameter = hashMapGet(action->parameters, "attachment", (uint16_t) strlen("attachment"));
    assert_non_null(parameter);

    assert_string_equal(parameter->key, "attachment");
    assert_string_equal(parameter->value, "video");

    linkedListDestroy(actions, icrule_free_action);
    icrule_action_list_release(actionMap);
}

static void verify_multiaction_entry(icrule_action_t* action,
                                     const char* id,
                                     const char* level,
                                     const char* duration)
{
    icrule_action_parameter_t* parameter;

    assert_int_equal(action->id, 70);
    assert_int_equal(hashMapCount(action->parameters), 3);

    parameter = hashMapGet(action->parameters, "lightID", (uint16_t) strlen("lightID"));
    assert_non_null(parameter);

    assert_string_equal(parameter->key, "lightID");
    assert_string_equal(parameter->value, id);

    parameter = hashMapGet(action->parameters, "level", (uint16_t) strlen("level"));
    assert_non_null(parameter);

    assert_string_equal(parameter->key, "level");
    assert_string_equal(parameter->value, level);

    parameter = hashMapGet(action->parameters, "duration", (uint16_t) strlen("duration"));
    assert_non_null(parameter);

    assert_string_equal(parameter->key, "duration");
    assert_string_equal(parameter->value, duration);
}

static void test_parse_multiaction(void** state)
{
    static const char* xml = XML_HEADER
                             "<action>\n"
                             "    <actionID>70</actionID>\n"
                             "    <parameter>\n"
                             "        <key>lightID</key>\n"
                             "        <value>000d6f000ad9cffe.1,000d6f000ae5dd94.1,000d6f000ad9e2e1.1</value>\n"
                             "    </parameter>\n"
                             "    <parameter>\n"
                             "        <key>level</key>\n"
                             "        <value>-1,20,40</value>\n"
                             "    </parameter>\n"
                             "    <parameter>\n"
                             "        <key>duration</key>\n"
                             "        <value>10,15,20</value>\n"
                             "    </parameter>\n"
                             "</action>";

    xmlDocPtr doc;
    xmlNodePtr top;

    icrule_action_t* action;

    icLinkedList* actions = linkedListCreate();
    icHashMap* actionMap = load_action_map();

    unused(state);

    load_xml(xml, &doc, &top);

    assert_int_equal(icrule_parse_action(top, actions, actionMap), 0);
    assert_int_equal(linkedListCount(actions), 3);

    action = linkedListGetElementAt(actions, 0);
    assert_non_null(action);

    verify_multiaction_entry(action, "000d6f000ad9cffe.1", "-1", "10");

    action = linkedListGetElementAt(actions, 1);
    assert_non_null(action);

    verify_multiaction_entry(action, "000d6f000ae5dd94.1", "20", "15");

    action = linkedListGetElementAt(actions, 2);
    assert_non_null(action);

    verify_multiaction_entry(action, "000d6f000ad9e2e1.1", "40", "20");

    linkedListDestroy(actions, icrule_free_action);
    icrule_action_list_release(actionMap);

    xmlFreeDoc(doc);
}

static void test_parse_sensor_trouble_state(void** state)
{
    static const char* xml = XML_HEADER
                             "<triggerList>\n"
                             "    <sensorTrigger>\n"
                             "        <description>Sensor Trigger</description>\n"
                             "        <category>sensor</category>\n"
                             "        <sensorState>trouble</sensorState>\n"
                             "        <sensorType>allNonMotionSensors</sensorType>\n"
                             "    </sensorTrigger>\n"
                             "</triggerList>\n";

    xmlDocPtr doc;
    xmlNodePtr top;

    int ret;

    icrule_trigger_list_t trigger_list;
    icrule_trigger_t* trigger;

    unused(state);

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 1);

    trigger = linkedListGetElementAt(trigger_list.triggers, 0);
    assert_non_null(trigger);
    assert_string_equal(trigger->desc, "Sensor Trigger");
    assert_int_equal(trigger->type, TRIGGER_TYPE_SENSOR);
    assert_int_equal(trigger->category, TRIGGER_CATEGORY_SENSOR);
    assert_int_equal(trigger->trigger.sensor.type, TRIGGER_SENSOR_TYPE_NONMOTION_SENSORS);
    assert_int_equal(trigger->trigger.sensor.state, TRIGGER_SENSOR_STATE_TROUBLE);

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);
}

static void test_parse_light_trigger(void** state)
{
    const char* xml;

    xmlDocPtr doc;
    xmlNodePtr top;

    int ret;

    icrule_trigger_list_t trigger_list;
    icrule_trigger_t* trigger;

    unused(state);

    // Test Single scene
    xml = XML_HEADER
          "<triggerList>\n"
          "        <lightingTrigger>\n"
          "            <description>Lighting Trigger</description>\n"
          "            <category>light</category>\n"
          "            <lightState>true</lightState>\n"
          "            <lightID>000d6f0002a67cba.1</lightID>\n"
          "        </lightingTrigger>"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 1);

    trigger = linkedListGetElementAt(trigger_list.triggers, 0);
    assert_non_null(trigger);
    assert_string_equal(trigger->desc, "Lighting Trigger");
    assert_int_equal(trigger->type, TRIGGER_TYPE_LIGHTING);
    assert_int_equal(trigger->category, TRIGGER_CATEGORY_LIGHT);
    assert_true(trigger->trigger.lighting.enabled);
    assert_string_equal(trigger->trigger.lighting.id, "000d6f0002a67cba.1");

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);

    // Test Multiple Scenes
    xml = XML_HEADER
          "<triggerList>\n"
          "        <lightingTrigger>\n"
          "            <description>Lighting Trigger</description>\n"
          "            <category>light</category>\n"
          "            <lightState>true,false,true</lightState>\n"
          "            <lightID>000d6f0002a67cba.1,000d6f0002a67cbb.1,000d6f0002a67cbc.1</lightID>\n"
          "        </lightingTrigger>"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 3);

    int i;
    const char* ids[3] = {
            "000d6f0002a67cba.1",
            "000d6f0002a67cbb.1",
            "000d6f0002a67cbc.1",
    };
    const bool states[3] = { true, false, true };

    for (i = 0; i < 3; i++) {
        trigger = linkedListGetElementAt(trigger_list.triggers, i);
        assert_non_null(trigger);
        assert_string_equal(trigger->desc, "Lighting Trigger");
        assert_int_equal(trigger->type, TRIGGER_TYPE_LIGHTING);
        assert_int_equal(trigger->category, TRIGGER_CATEGORY_LIGHT);
        assert_string_equal(trigger->trigger.lighting.id, ids[i]);

        if (states[i]) {
            assert_true(trigger->trigger.lighting.enabled);
        } else {
            assert_false(trigger->trigger.lighting.enabled);
        }
    }

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);
}

static void test_parse_doorlock_trigger(void** state)
{
    const char* xml;

    xmlDocPtr doc;
    xmlNodePtr top;

    int ret;

    icrule_trigger_list_t trigger_list;
    icrule_trigger_t* trigger;

    unused(state);

    // Test Single scene
    xml = XML_HEADER
          "<triggerList>\n"
          "        <doorLockTrigger>\n"
          "            <description>DoorLock Trigger</description>\n"
          "            <category>doorLock</category>\n"
          "            <doorLockState>lock</doorLockState>\n"
          "            <doorLockID>000d6f0002a67cba.1</doorLockID>\n"
          "        </doorLockTrigger>"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 1);

    trigger = linkedListGetElementAt(trigger_list.triggers, 0);
    assert_non_null(trigger);
    assert_string_equal(trigger->desc, "DoorLock Trigger");
    assert_int_equal(trigger->type, TRIGGER_TYPE_DOOR_LOCK);
    assert_int_equal(trigger->category, TRIGGER_CATEGORY_DOOR_LOCK);
    assert_int_equal(trigger->trigger.doorlock.state, TRIGGER_DOORLOCK_STATE_LOCKED);
    assert_string_equal(trigger->trigger.doorlock.id, "000d6f0002a67cba.1");

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);

    // Test Multiple Scenes
    xml = XML_HEADER
          "<triggerList>\n"
          "        <doorLockTrigger>\n"
          "            <description>DoorLock Trigger</description>\n"
          "            <category>doorLock</category>\n"
          "            <doorLockState>lock,unlock,trouble</doorLockState>\n"
          "            <doorLockID>000d6f0002a67cba.1,000d6f0002a67cbb.1,000d6f0002a67cbc.1</doorLockID>\n"
          "        </doorLockTrigger>"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 3);

    int i;
    const char* ids[3] = {
            "000d6f0002a67cba.1",
            "000d6f0002a67cbb.1",
            "000d6f0002a67cbc.1",
    };
    const icrule_trigger_doorlock_state_t states[3] = {
            TRIGGER_DOORLOCK_STATE_LOCKED,
            TRIGGER_DOORLOCK_STATE_UNLOCKED,
            TRIGGER_DOORLOCK_STATE_TROUBLE,
    };

    for (i = 0; i < 3; i++) {
        trigger = linkedListGetElementAt(trigger_list.triggers, i);
        assert_non_null(trigger);
        assert_string_equal(trigger->desc, "DoorLock Trigger");
        assert_int_equal(trigger->type, TRIGGER_TYPE_DOOR_LOCK);
        assert_int_equal(trigger->category, TRIGGER_CATEGORY_DOOR_LOCK);
        assert_string_equal(trigger->trigger.doorlock.id, ids[i]);
        assert_int_equal(trigger->trigger.doorlock.state, states[i]);
    }

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);
}

static void test_parse_thermostat_trigger(void** state)
{
    const char* xml;

    xmlDocPtr doc;
    xmlNodePtr top;

    int ret;

    icrule_trigger_list_t trigger_list;
    icrule_trigger_t* trigger;

    unused(state);

    // Test Single scene
    xml = XML_HEADER
          "<triggerList>\n"
          "        <thermostatTrigger>\n"
          "            <description>Thermostat Trigger</description>\n"
          "            <category>thermostat</category>\n"
          "            <thermostatID>000d6f0002a67cba.1</thermostatID>\n"
          "            <thermostatStateEval>trouble</thermostatStateEval>"
          "            <thermostatThresholdEval>\n"
          "                <lowTemperature>1556</lowTemperature>\n"
          "                <highTemperature>3222</highTemperature>\n"
          "            </thermostatThresholdEval>\n"
          "        </thermostatTrigger>\n"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 1);

    trigger = linkedListGetElementAt(trigger_list.triggers, 0);
    assert_non_null(trigger);
    assert_string_equal(trigger->desc, "Thermostat Trigger");
    assert_int_equal(trigger->type, TRIGGER_TYPE_THERMOSTAT);
    assert_int_equal(trigger->category, TRIGGER_CATEGORY_THERMOSTAT);
    assert_string_equal(trigger->trigger.thermostat.id, "000d6f0002a67cba.1");
    assert_true(trigger->trigger.thermostat.trouble);
    assert_int_equal(trigger->trigger.thermostat.bounds.lower, 1556);
    assert_int_equal(trigger->trigger.thermostat.bounds.upper, 3222);

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);

    // Test Multiple Scenes
    xml = XML_HEADER
          "<triggerList>\n"
          "        <thermostatTrigger>\n"
          "            <description>Thermostat Trigger</description>\n"
          "            <category>thermostat</category>\n"
          "            <thermostatID>000d6f0002a67cba.1,000d6f0002a67cbb.1,000d6f0002a67cbc.1</thermostatID>\n"
          "            <thermostatStateEval>trouble</thermostatStateEval>"
          "            <thermostatThresholdEval>\n"
          "                <lowTemperature>1556</lowTemperature>\n"
          "                <highTemperature>3222</highTemperature>\n"
          "            </thermostatThresholdEval>\n"
          "        </thermostatTrigger>\n"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 3);

    uint32_t i;
    const char* ids[3] = {
            "000d6f0002a67cba.1",
            "000d6f0002a67cbb.1",
            "000d6f0002a67cbc.1",
    };

    for (i = 0; i < 3; i++) {
        trigger = linkedListGetElementAt(trigger_list.triggers, i);
        assert_non_null(trigger);
        assert_string_equal(trigger->desc, "Thermostat Trigger");
        assert_int_equal(trigger->type, TRIGGER_TYPE_THERMOSTAT);
        assert_int_equal(trigger->category, TRIGGER_CATEGORY_THERMOSTAT);
        assert_string_equal(trigger->trigger.thermostat.id, ids[i]);
        assert_true(trigger->trigger.thermostat.trouble);
        assert_int_equal(trigger->trigger.thermostat.bounds.lower, 1556);
        assert_int_equal(trigger->trigger.thermostat.bounds.upper, 3222);
    }

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);
}

static void test_parse_thermostat_threshold_trigger(void** state)
{
    const char* xml;

    xmlDocPtr doc;
    xmlNodePtr top;

    int ret;

    icrule_trigger_list_t trigger_list;
    icrule_trigger_t* trigger;

    unused(state);

    // Test Single scene
    xml = XML_HEADER
          "<triggerList>\n"
          "        <thermostatThresholdTrigger>\n"
          "            <description>Thermostat Trigger</description>\n"
          "            <category>thermostat</category>\n"
          "            <thermostatID>000d6f0002a67cba.1</thermostatID>\n"
          "            <lowTemperature>1556</lowTemperature>\n"
          "            <highTemperature>3222</highTemperature>\n"
          "        </thermostatThresholdTrigger>\n"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 1);

    trigger = linkedListGetElementAt(trigger_list.triggers, 0);
    assert_non_null(trigger);
    assert_string_equal(trigger->desc, "Thermostat Trigger");
    assert_int_equal(trigger->type, TRIGGER_TYPE_THERMOSTAT_THRESHOLD);
    assert_int_equal(trigger->category, TRIGGER_CATEGORY_THERMOSTAT);
    assert_string_equal(trigger->trigger.thermostat.id, "000d6f0002a67cba.1");
    assert_false(trigger->trigger.thermostat.trouble);
    assert_int_equal(trigger->trigger.thermostat.bounds.lower, 1556);
    assert_int_equal(trigger->trigger.thermostat.bounds.upper, 3222);

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);

    // Test Multiple Scenes
    xml = XML_HEADER
          "<triggerList>\n"
          "        <thermostatThresholdTrigger>\n"
          "            <description>Thermostat Trigger</description>\n"
          "            <category>thermostat</category>\n"
          "            <thermostatID>000d6f0002a67cba.1,000d6f0002a67cbb.1,000d6f0002a67cbc.1</thermostatID>\n"
          "            <lowTemperature>1556</lowTemperature>\n"
          "            <highTemperature>3222</highTemperature>\n"
          "        </thermostatThresholdTrigger>\n"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 3);

    uint32_t i;
    const char* ids[3] = {
            "000d6f0002a67cba.1",
            "000d6f0002a67cbb.1",
            "000d6f0002a67cbc.1",
    };

    for (i = 0; i < 3; i++) {
        trigger = linkedListGetElementAt(trigger_list.triggers, i);
        assert_non_null(trigger);
        assert_string_equal(trigger->desc, "Thermostat Trigger");
        assert_int_equal(trigger->type, TRIGGER_TYPE_THERMOSTAT_THRESHOLD);
        assert_int_equal(trigger->category, TRIGGER_CATEGORY_THERMOSTAT);
        assert_string_equal(trigger->trigger.thermostat.id, ids[i]);
        assert_false(trigger->trigger.thermostat.trouble);
        assert_int_equal(trigger->trigger.thermostat.bounds.lower, 1556);
        assert_int_equal(trigger->trigger.thermostat.bounds.upper, 3222);
    }

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);

    // Test Multiple Scenes
    xml = XML_HEADER
          "<triggerList>\n"
          "        <thermostatThresholdTrigger>\n"
          "            <description>Thermostat Trigger</description>\n"
          "            <category>thermostat</category>\n"
          "            <thermostatID>000d6f0002a67cba.1,000d6f0002a67cbb.1,000d6f0002a67cbc.1</thermostatID>\n"
          "            <lowTemperature>1556,1557,1558</lowTemperature>\n"
          "            <highTemperature>3222,3223,3224</highTemperature>\n"
          "        </thermostatThresholdTrigger>\n"
          "</triggerList>\n";

    load_xml(xml, &doc, &top);

    trigger_list.triggers = linkedListCreate();
    assert_non_null(trigger_list.triggers);

    ret = icrule_parse_trigger_list(top, &trigger_list);
    assert_int_equal(ret, 0);
    assert_int_equal(linkedListCount(trigger_list.triggers), 3);

    int lower_bounds[3] = {
            1556, 1557, 1558
    };
    int upper_bounds[3] = {
            3222, 3223, 3224
    };

    for (i = 0; i < 3; i++) {
        trigger = linkedListGetElementAt(trigger_list.triggers, i);
        assert_non_null(trigger);
        assert_string_equal(trigger->desc, "Thermostat Trigger");
        assert_int_equal(trigger->type, TRIGGER_TYPE_THERMOSTAT_THRESHOLD);
        assert_int_equal(trigger->category, TRIGGER_CATEGORY_THERMOSTAT);
        assert_string_equal(trigger->trigger.thermostat.id, ids[i]);
        assert_false(trigger->trigger.thermostat.trouble);
        assert_int_equal(trigger->trigger.thermostat.bounds.lower, lower_bounds[i]);
        assert_int_equal(trigger->trigger.thermostat.bounds.upper, upper_bounds[i]);
    }

    linkedListDestroy(trigger_list.triggers, icrule_free_trigger);

    xmlFreeDoc(doc);
}

static int test_setup(void** state)
{
    unused(state);

    icrule_set_action_list_dir(NULL);
    return 0;
}

int main(int argc, char* argv[])
{
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_parse_time),
            cmocka_unit_test(test_parse_constraint),
            cmocka_unit_test(test_parse_action),
            cmocka_unit_test(test_parse_sensor_trouble_state),
            cmocka_unit_test(test_parse_multiaction),
            cmocka_unit_test(test_parse_light_trigger),
            cmocka_unit_test(test_parse_doorlock_trigger),
            cmocka_unit_test(test_parse_thermostat_trigger),
//            cmocka_unit_test(test_parse_thermostat_threshold_trigger),
    };

    return cmocka_run_group_tests(tests, test_setup, NULL);
}
