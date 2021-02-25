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
// Created by Boyd, Weston on 4/27/18.
//

#include <string.h>
#include <errno.h>

#include <libxml2/libxml/tree.h>
#include <icrule/icrule.h>
#include <icUtil/stringUtils.h>

#include "icrule_internal.h"
#include "icrule_trigger.h"
#include "icrule_xml.h"
#include "icrule_constraint.h"
#include "icrule_action.h"
#include "icrule_schedule.h"

#define TRIGGER_LIST_NODE "triggerList"
#define ACTION_NODE "action"
#define CONSTRAINTS_NODE "constraints"
#define SCHEDULE_ENTRY_NODE "scheduleEntry"
#define DESCRIPTION_NODE "description"

static int parse_xmldoc(xmlDocPtr doc, icrule_t* rule)
{
    char filename[255];
    xmlNodePtr top, node;
    icHashMap* actionMap;

    actionMap = hashMapCreate();

    sprintf(filename, "%s/%s", icrule_get_action_list_dir(), "masterActionList.xml");

    if (icrule_action_list_load(filename, actionMap) < 0) {
        icrule_action_list_release(actionMap);
        return -1;
    }

    sprintf(filename, "%s/%s", icrule_get_action_list_dir(), "internalActionList.xml");

    if (icrule_action_list_load(filename, actionMap) < 0) {
        icrule_action_list_release(actionMap);
        return -1;
    }

    // Initialize the rule
    memset(rule, 0, sizeof(icrule_t));

    rule->triggers.triggers = linkedListCreate();
    rule->schedule_entries = linkedListCreate();
    rule->constraints = linkedListCreate();
    rule->actions = linkedListCreate();

    top = xmlDocGetRootElement(doc);

    rule->id = icrule_get_xml_uint64(top, "ruleID", 0);

    for (node = top->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, TRIGGER_LIST_NODE) == 0) {
                if (icrule_parse_trigger_list(node, &rule->triggers) < 0) {
                    icrule_destroy(rule);
                    icrule_action_list_release(actionMap);

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, CONSTRAINTS_NODE) == 0) {
                if (icrule_parse_constraint(node, rule->constraints, CONSTRAINT_LOGIC_AND) < 0) {
                    icrule_destroy(rule);
                    icrule_action_list_release(actionMap);

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, ACTION_NODE) == 0) {
                if (icrule_parse_action(node, rule->actions, actionMap) < 0) {
                    icrule_destroy(rule);
                    icrule_action_list_release(actionMap);

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, SCHEDULE_ENTRY_NODE) == 0) {
                if (icrule_parse_schedule(node, rule->schedule_entries) < 0) {
                    icrule_destroy(rule);
                    icrule_action_list_release(actionMap);

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, DESCRIPTION_NODE) == 0) {
                xmlChar *contents = xmlNodeGetContent(node);
                rule->desc = strdup(contents);
                xmlFree(contents);
            }
        }
    }

    icrule_action_list_release(actionMap);

    /* We have parsed all the rules. Now we need to update
     * sms/email actions just in case there are video/picture
     * attachments that are required. This is necessary
     * because the legacy rules implicitly implied ordering
     * between video/pictures and sms/email attachments. That
     * does not mean however that other rule engines will do this.
     * Thus let's add in a new parameter key "attachment" with
     * value "video" | "picture".
     */
    icrule_update_message_attachment(rule->actions);

    return 0;
}

int icrule_parse(const char* xml, icrule_t* rule)
{
    int ret;
    xmlDocPtr doc;

    if ((xml == NULL) || (strlen(xml) == 0)) {
        errno = EINVAL;
        return -1;
    }

    if (rule == NULL) {
        errno = EINVAL;
        return -1;
    }

    doc = xmlParseMemory(xml, (int) strlen(xml));
    if (doc == NULL) {
        errno = EBADMSG;
        return -1;
    }

    ret = parse_xmldoc(doc, rule);

    xmlFreeDoc(doc);

    return ret;
}

int icrule_parse_file(const char* filename, icrule_t* rule)
{
    int ret;
    xmlDocPtr doc;

    if ((filename == NULL) || (strlen(filename) == 0)) {
        errno = EINVAL;
        return -1;
    }

    if (rule == NULL) {
        errno = EINVAL;
        return -1;
    }

    doc = xmlParseFile(filename);
    if (doc == NULL) {
        errno = EBADMSG;
        return -1;
    }

    ret = parse_xmldoc(doc, rule);

    xmlFreeDoc(doc);

    return ret;
}

void icrule_destroy(icrule_t* rule)
{
    if (rule) {
        if (rule->triggers.triggers != NULL) {
            linkedListDestroy(rule->triggers.triggers, icrule_free_trigger);
        }

        if (rule->schedule_entries != NULL) {
            linkedListDestroy(rule->schedule_entries, icrule_free_schedule);
        }

        if (rule->constraints != NULL) {
            linkedListDestroy(rule->constraints, icrule_free_constraint);
        }

        if (rule->actions != NULL) {
            linkedListDestroy(rule->actions, icrule_free_action);
        }

        free(rule->desc);
    }
}

static uint32_t parse_exact_time(char* when)
{
    char* time_secs = strchr(when, ':');
    unsigned long tmp_time, total_time;

    if (time_secs == NULL) {
        return 0;
    }

    *time_secs = '\0';
    time_secs++;

    tmp_time = strtoul(when, NULL, 10);
    if (tmp_time == ULONG_MAX) {
        errno = EINVAL;
        return UINT32_MAX;
    }

    // Convert from hours to seconds
    total_time = (uint32_t) (tmp_time * 60 * 60);

    tmp_time = strtoul(time_secs, NULL, 10);
    if (tmp_time == ULONG_MAX) {
        errno = EINVAL;
        return UINT32_MAX;
    }

    // Convert from minutes to seconds
    total_time += (uint32_t) (tmp_time * 60);

    return (uint32_t) total_time;
}

static uint8_t parse_day_of_week(char* when)
{
    uint8_t day_of_week = ICRULE_TIME_INVALID;
    char *token, *saveptr;

    if ((when == NULL) || (strlen(when) == 0)) {
        errno = EINVAL;
        return ICRULE_TIME_INVALID;
    }

    token = strtok_r(when, ",", &saveptr);
    while (token != NULL) {
        if (stringCompare(token, "MON", true) == 0) {
            day_of_week |= ICRULE_TIME_MONDAY;
        } else if (stringCompare(token, "TUE", true) == 0) {
            day_of_week |= ICRULE_TIME_TUESDAY;
        } else if (stringCompare(token, "WED", true) == 0) {
            day_of_week |= ICRULE_TIME_WEDNESDAY;
        } else if (stringCompare(token, "THU", true) == 0) {
            day_of_week |= ICRULE_TIME_THURSDAY;
        } else if (stringCompare(token, "FRI", true) == 0) {
            day_of_week |= ICRULE_TIME_FRIDAY;
        } else if (stringCompare(token, "SAT", true) == 0) {
            day_of_week |= ICRULE_TIME_SATURDAY;
        } else if (stringCompare(token, "SUN", true) == 0) {
            day_of_week |= ICRULE_TIME_SUNDAY;
        }

        token = strtok_r(NULL, ",", &saveptr);
    }

    return day_of_week;
}

int icrule_parse_time_slot(xmlNodePtr parent, icrule_time_t* time)
{
    xmlNodePtr node;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "exactTime") == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    char* when = strchr((const char*) value, ' ');

                    if (when == NULL) {
                        xmlFree(value);
                        errno = EBADMSG;
                        return -1;
                    }

                    // Make sure that days of the week and the time
                    // are separated. Otherwise we will overrun the
                    // string tokenizer.
                    *when = '\0';
                    when++;

                    time->day_of_week = parse_day_of_week((char*) value);
                    if (time->day_of_week == ICRULE_TIME_INVALID) {
                        xmlFree(value);
                        errno = EBADMSG;
                        return -1;
                    }

                    if (stringCompare(when, "sunrise", true) == 0) {
                        time->time.sun_time = ICRULE_SUNTIME_SUNRISE;
                        time->use_exact_time = false;
                    } else if (stringCompare(when, "sunset", true) == 0) {
                        time->time.sun_time = ICRULE_SUNTIME_SUNSET;
                        time->use_exact_time = false;
                    } else {
                        time->use_exact_time = true;
                        time->time.seconds = parse_exact_time(when);

                        if (time->time.seconds == UINT32_MAX) {
                            xmlFree(value);
                            errno = EBADMSG;
                            return -1;
                        }
                    }

                    xmlFree(value);
                }
            }
        }
    }

    return 0;
}

icLinkedList* strtok2list(const char* value, const char* delim)
{
    char *orig, *token, *saveptr;
    icLinkedList* list;

    list = linkedListCreate();

    orig = strdup(value);
    token = strtok_r(orig, delim, &saveptr);

    while (token) {
        linkedListAppend(list, strdup(token));

        token = strtok_r(NULL, delim, &saveptr);
    }

    free(orig);

    return list;
}

