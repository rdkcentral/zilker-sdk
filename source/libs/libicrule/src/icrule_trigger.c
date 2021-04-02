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

#include <icrule/icrule.h>
#include <errno.h>
#include <icrule/icrule_triggers.h>

#include "icrule_internal.h"
#include "icrule_trigger.h"
#include "icrule_xml.h"

#define TRIGGER_SENSOR_NODE "sensorTrigger"
#define TRIGGER_TOUCHSCREEN_NODE "touchscreenTrigger"
#define TRIGGER_PANIC_NODE "panicTrigger"
#define TRIGGER_SYSTEMSCENE_NODE "systemSceneTrigger"
#define TRIGGER_NETWORK_NODE "networkTrigger"
#define TRIGGER_LIGHTING_NODE "lightingTrigger"
#define TRIGGER_DOORLOCK_NODE "doorLockTrigger"
#define TRIGGER_TSTAT_THRESHOLD_NODE "thermostatThresholdTrigger"
#define TRIGGER_TSTAT_NODE "thermostatTrigger"
#define TRIGGER_TIME_NODE "timeTrigger"
#define TRIGGER_ZIGBEE_COMMSTATUS_NODE "zigbeeCommStatusTrigger"
#define TRIGGER_SWITCH_NODE "switchTrigger"
#define TRIGGER_RESOURCE_NODE "resourceTrigger"
#define TRIGGER_CLOUD_SERVICE_NODE "cloudServiceTrigger"
#define TRIGGER_CLOUD_NODE "cloudTrigger"

#define ELEMENT_CATEGORY "category"
#define ELEMENT_DESCRIPTION "description"

#define ELEMENT_SENSOR_STATE "sensorState"
#define ELEMENT_SENSOR_ID "sensorID"
#define ELEMENT_SENSOR_TYPE "sensorType"

typedef int (*parse_trigger_handler_t)(xmlNodePtr parent, icLinkedList* triggers);
typedef void(*trigger_free_t)(icrule_trigger_t* trigger);

typedef struct trigger_descriptor {
    const char* name;
    parse_trigger_handler_t handler;
    trigger_free_t free;
} trigger_descriptor_t;

static const char* trigger_category_enum2str[] = {
    "sensor",
    "touchscreen",
    "scene",
    "light",
    "doorLock",
    "thermostat",
    "network",
    "panic",
    "time",
    "switch",
    "cloud",
    "cloudService",
    "resource"
};

static const char* trigger_sensor_state_enum2str[] = {
    "open",
    "close",
    "openOrClose",
    "trouble"
};

static icrule_trigger_t* icrule_alloc_trigger(xmlNodePtr parent, icrule_trigger_type_t type)
{
    icrule_trigger_t* trigger;
    xmlNodePtr node;

    trigger = malloc(sizeof(struct icrule_trigger));
    if (trigger == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    memset(trigger, 0, sizeof(struct icrule_trigger));

    trigger->type = type;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, ELEMENT_CATEGORY) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_SENSOR]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_SENSOR;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_TOUCHSCREEN]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_TOUCHSCREEN;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_SCENE]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_SCENE;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_LIGHT]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_LIGHT;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_DOOR_LOCK]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_DOOR_LOCK;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_THERMOSTAT]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_THERMOSTAT;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_NETWORK]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_NETWORK;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_PANIC]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_PANIC;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_TIME]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_TIME;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_SWITCH]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_SWITCH;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_CLOUD]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_CLOUD;
                    } else if (strcmp((const char*) value, trigger_category_enum2str[TRIGGER_CATEGORY_RESOURCE]) == 0) {
                        trigger->category = TRIGGER_CATEGORY_RESOURCE;
                    }

                    xmlFree(value);
                }
            } else if (strcmp((const char*) node->name, ELEMENT_DESCRIPTION) == 0) {
                trigger->desc = icrule_get_xml_string(node, NULL, NULL);
            }
        }
    }

    return trigger;
}

static void free_multitrigger_map(void* key, void* value)
{
    unused(key);

    if (value) {
        linkedListDestroy(value, NULL);
    }
}

static int parse_sensor_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    xmlNodePtr node;
    icrule_trigger_t* trigger;

    trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_SENSOR);
    if (trigger == NULL) return -1;

    // Init the conditional sensor variables to defaults.
    trigger->trigger.sensor.id = NULL;
    trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_INVALID;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, ELEMENT_SENSOR_STATE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    if (strcmp((const char*) value, trigger_sensor_state_enum2str[TRIGGER_SENSOR_STATE_EITHER]) == 0) {
                        trigger->trigger.sensor.state = TRIGGER_SENSOR_STATE_EITHER;
                    } else if (strcmp((const char*) value, trigger_sensor_state_enum2str[TRIGGER_SENSOR_STATE_OPEN]) == 0) {
                        trigger->trigger.sensor.state = TRIGGER_SENSOR_STATE_OPEN;
                    } else if (strcmp((const char*) value, trigger_sensor_state_enum2str[TRIGGER_SENSOR_STATE_CLOSED]) == 0) {
                        trigger->trigger.sensor.state = TRIGGER_SENSOR_STATE_CLOSED;
                    } else if (strcmp((const char*) value, trigger_sensor_state_enum2str[TRIGGER_SENSOR_STATE_TROUBLE]) == 0) {
                        trigger->trigger.sensor.state = TRIGGER_SENSOR_STATE_TROUBLE;
                    }

                    xmlFree(value);
                }
            } else if (strcmp((const char*) node->name, ELEMENT_SENSOR_ID) == 0) {
                xmlChar* value = xmlNodeGetContent(node);
                if (value != NULL)
                {
                    trigger->trigger.sensor.id = strdup((const char*)value);
                    xmlFree(value);
                }
            } else if (strcmp((const char*) node->name, ELEMENT_SENSOR_TYPE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    if (strcmp((const char*) value, "allSensors") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_ALL_SENSORS;
                    } else if (strcmp((const char*) value, "allNonMotionSensors") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_NONMOTION_SENSORS;
                    } else if (strcmp((const char*) value, "door") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_DOOR;
                    } else if (strcmp((const char*) value, "window") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_WINDOW;
                    } else if (strcmp((const char*) value, "motion") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_MOTION;
                    } else if (strcmp((const char*) value, "glassBreak") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_GLASS_BREAK;
                    } else if (strcmp((const char*) value, "smoke") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_SMOKE;
                    } else if (strcmp((const char*) value, "carbonMonoxide") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_CO;
                    } else if (strcmp((const char*) value, "water") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_WATER;
                    } else if (strcmp((const char*) value, "dryContact") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_DRY_CONTACT;
                    } else if (strcmp((const char*) value, "inertia") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_INERTIA;
                    } else if (strcmp((const char*) value, "lighting") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_LIGHTING;
                    } else if (strcmp((const char*) value, "temperature") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_TEMPERATURE;
                    } else if (strcmp((const char*) value, "doorLock") == 0) {
                        trigger->trigger.sensor.type = TRIGGER_SENSOR_TYPE_DOOR_LOCK;
                    }

                    xmlFree(value);
                }
            }
        }
    }

    linkedListAppend(triggers, trigger);

    return 0;
}

static int parse_touchscreen_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    xmlNodePtr node;
    icrule_trigger_t* trigger;

    trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_TOUCHSCREEN);
    if (trigger == NULL) return -1;

    trigger->trigger.touchscreen.state = TRIGGER_TOUCHSCREEN_STATE_INVALID;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "touchscreenState") == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    if (strcmp((const char*) value, "trouble") == 0) {
                        trigger->trigger.touchscreen.state = TRIGGER_TOUCHSCREEN_STATE_TROUBLE;
                    } else if (strcmp((const char*) value, "power_lost") == 0) {
                        trigger->trigger.touchscreen.state = TRIGGER_TOUCHSCREEN_STATE_POWER_LOST;
                    }

                    xmlFree(value);
                }
            }
        }
    }

    if (trigger->trigger.touchscreen.state == TRIGGER_TOUCHSCREEN_STATE_INVALID) {
        icrule_free_trigger(trigger);

        errno = EBADMSG;
        return -1;
    }

    linkedListAppend(triggers, trigger);

    return 0;
}

#define LIGHT_STATE_NODE "lightState"
#define LIGHT_ID_NODE "lightID"

static int parse_lighting_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    uint32_t i, maxEntries = 0;

    xmlNodePtr node;
    icHashMap* map = hashMapCreate();

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, LIGHT_STATE_NODE) == 0) {
                char* value = icrule_get_xml_string(node, NULL, "");

                if (value) {
                    icLinkedList* list = strtok2list(value, ",");
                    free(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               LIGHT_STATE_NODE,
                               hashmap_string_sizeof(LIGHT_STATE_NODE),
                               list);
                }
            } else if (strcmp((const char*) node->name, LIGHT_ID_NODE) == 0) {
                char* value = icrule_get_xml_string(node, NULL, "");

                if (value) {
                    icLinkedList* list = strtok2list(value, ",");
                    free(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               LIGHT_ID_NODE,
                               hashmap_string_sizeof(LIGHT_ID_NODE),
                               list);
                }
            }
        }
    }

    for (i = 0; i < maxEntries; i++) {
        icHashMapIterator* iterator;
        icrule_trigger_t* trigger;

        trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_LIGHTING);
        if (trigger == NULL) {
            hashMapDestroy(map, free_multitrigger_map);

            return -1;
        }

        iterator = hashMapIteratorCreate(map);
        while (hashMapIteratorHasNext(iterator)) {
            void* key;
            uint16_t keyLen;
            icLinkedList* list;
            uint32_t index;

            if (hashMapIteratorGetNext(iterator, &key, &keyLen, (void**) &list)) {
                index = linkedListCount(list);
                index = (i < index) ? i : index - 1;

                if (strncmp(key, LIGHT_ID_NODE, keyLen) == 0) {
                    trigger->trigger.lighting.id = strdup(linkedListGetElementAt(list, index));
                } else if (strncmp(key, LIGHT_STATE_NODE, keyLen) == 0) {
                    trigger->trigger.lighting.enabled = (strcmp(linkedListGetElementAt(list, index), "true") == 0);
                }
            }
        }
        hashMapIteratorDestroy(iterator);

        if ((trigger->trigger.lighting.id == NULL) || (strlen(trigger->trigger.lighting.id) == 0)) {
            hashMapDestroy(map, free_multitrigger_map);
            icrule_free_trigger(trigger);

            errno = EBADMSG;
            return -1;
        }

        linkedListAppend(triggers, trigger);
    }

    hashMapDestroy(map, free_multitrigger_map);

    return 0;
}

#define DOORLOCK_STATE_NODE "doorLockState"
#define DOORLOCK_ID_NODE "doorLockID"

static int parse_doorlock_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    uint32_t i, maxEntries = 0;

    xmlNodePtr node;
    icHashMap* map = hashMapCreate();

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, DOORLOCK_STATE_NODE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    icLinkedList* list = strtok2list((const char*) value, ",");
                    xmlFree(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               DOORLOCK_STATE_NODE,
                               hashmap_string_sizeof(DOORLOCK_STATE_NODE),
                               list);
                }
            } else if (strcmp((const char*) node->name, DOORLOCK_ID_NODE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    icLinkedList* list = strtok2list((const char*) value, ",");
                    xmlFree(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               DOORLOCK_ID_NODE,
                               hashmap_string_sizeof(DOORLOCK_ID_NODE),
                               list);
                }
            }
        }
    }

    for (i = 0; i < maxEntries; i++) {
        icHashMapIterator* iterator;
        icrule_trigger_t* trigger;

        trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_DOOR_LOCK);
        if (trigger == NULL) {
            hashMapDestroy(map, free_multitrigger_map);

            return -1;
        }

        iterator = hashMapIteratorCreate(map);
        while (hashMapIteratorHasNext(iterator)) {
            void* key;
            uint16_t keyLen;
            icLinkedList* list;
            uint32_t index;

            if (hashMapIteratorGetNext(iterator, &key, &keyLen, (void**) &list)) {
                index = linkedListCount(list);
                index = (i < index) ? i : index - 1;

                if (strncmp(key, DOORLOCK_ID_NODE, keyLen) == 0) {
                    trigger->trigger.doorlock.id  = strdup(linkedListGetElementAt(list, index));
                } else if (strncmp(key, DOORLOCK_STATE_NODE, keyLen) == 0) {
                    const char* item = linkedListGetElementAt(list, index);

                    if (strcmp(item, "lock") == 0) {
                        trigger->trigger.doorlock.state = TRIGGER_DOORLOCK_STATE_LOCKED;
                    } else if (strcmp(item, "unlock") == 0) {
                        trigger->trigger.doorlock.state = TRIGGER_DOORLOCK_STATE_UNLOCKED;
                    } else if (strcmp(item, "trouble") == 0) {
                        trigger->trigger.doorlock.state = TRIGGER_DOORLOCK_STATE_TROUBLE;
                    }
                }
            }
        }
        hashMapIteratorDestroy(iterator);

        if ((trigger->trigger.doorlock.id == NULL) || (strlen(trigger->trigger.doorlock.id) == 0)) {
            hashMapDestroy(map, free_multitrigger_map);
            icrule_free_trigger(trigger);

            errno = EBADMSG;
            return -1;
        }

        linkedListAppend(triggers, trigger);
    }

    hashMapDestroy(map, free_multitrigger_map);

    return 0;
}

static int parse_thermostat_bounds(xmlNodePtr parent, icrule_thermostat_bounds_t* bounds)
{
    xmlNodePtr node;

    bounds->lower = -1000;
    bounds->upper = -1000;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "lowTemperature") == 0) {
                bounds->lower = icrule_get_xml_int(node, NULL, -1000);
            } else if (strcmp((const char*) node->name, "highTemperature") == 0) {
                bounds->upper = icrule_get_xml_int(node, NULL, -1000);
            }
        }
    }

    return 0;
}

static int parse_thermostat_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    xmlNodePtr node;
    icrule_thermostat_bounds_t bounds;
    bool trouble = false;
    icLinkedList* ids = NULL;
    uint32_t i;

    memset(&bounds, 0, sizeof(icrule_thermostat_bounds_t));

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "thermostatStateEval") == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    trouble = (strcmp((const char*) value, "trouble") == 0);
                    xmlFree(value);
                }
            } else if (strcmp((const char*) node->name, "thermostatThresholdEval") == 0) {
                if (parse_thermostat_bounds(node, &bounds) < 0) {
                    if (ids) {
                        linkedListDestroy(ids, NULL);
                    }

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, "thermostatID") == 0) {
                xmlChar* value = NULL;
                if (ids) {
                    linkedListDestroy(ids, NULL);

                    errno = EBADMSG;
                    return -1;
                }
                value = xmlNodeGetContent(node);

                if (value) {
                    ids = strtok2list((const char*) value, ",");
                    xmlFree(value);
                }
            }
        }
    }

    if (ids == NULL) {
        errno = EBADMSG;
        return -1;
    }

    for (i = 0; i < linkedListCount(ids); i++) {
        icrule_trigger_t* trigger;

        trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_THERMOSTAT);
        if (trigger == NULL) {
            linkedListDestroy(ids, NULL);

            return -1;
        }

        trigger->trigger.thermostat.id = strdup(linkedListGetElementAt(ids, i));
        trigger->trigger.thermostat.trouble = trouble;

        memcpy(&trigger->trigger.thermostat.bounds, &bounds, sizeof(icrule_thermostat_bounds_t));

        linkedListAppend(triggers, trigger);
    }

    linkedListDestroy(ids, NULL);

    return 0;
}

#define TSTAT_THRESHOLD_ID_NODE "thermostatID"
#define TSTAT_THRESHOLD_LOW_NODE "lowTemperature"
#define TSTAT_THRESHOLD_HIGH_NODE "highTemperature"

static int parse_thermostat_threshold_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    uint32_t i, maxEntries = 0;

    xmlNodePtr node;
    icHashMap* map = hashMapCreate();

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, TSTAT_THRESHOLD_ID_NODE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    icLinkedList* list = strtok2list((const char*) value, ",");
                    xmlFree(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               TSTAT_THRESHOLD_ID_NODE,
                               hashmap_string_sizeof(TSTAT_THRESHOLD_ID_NODE),
                               list);
                }
            } else if (strcmp((const char*) node->name, TSTAT_THRESHOLD_LOW_NODE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    icLinkedList* list = strtok2list((const char*) value, ",");
                    xmlFree(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               TSTAT_THRESHOLD_LOW_NODE,
                               hashmap_string_sizeof(TSTAT_THRESHOLD_LOW_NODE),
                               list);
                }
            } else if (strcmp((const char*) node->name, TSTAT_THRESHOLD_HIGH_NODE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    icLinkedList* list = strtok2list((const char*) value, ",");
                    xmlFree(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               TSTAT_THRESHOLD_HIGH_NODE,
                               hashmap_string_sizeof(TSTAT_THRESHOLD_HIGH_NODE),
                               list);
                }
            }
        }
    }

    for (i = 0; i < maxEntries; i++) {
        icHashMapIterator* iterator;
        icrule_trigger_t* trigger;

        trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_THERMOSTAT_THRESHOLD);
        if (trigger == NULL) {
            hashMapDestroy(map, free_multitrigger_map);

            return -1;
        }

        trigger->trigger.thermostat.trouble = false;
        trigger->trigger.thermostat.bounds.lower = -1000;
        trigger->trigger.thermostat.bounds.upper = -1000;

        iterator = hashMapIteratorCreate(map);
        while (hashMapIteratorHasNext(iterator)) {
            void* key;
            uint16_t keyLen;
            icLinkedList* list;
            uint32_t index;

            if (hashMapIteratorGetNext(iterator, &key, &keyLen, (void**) &list)) {
                index = linkedListCount(list);
                index = (i < index) ? i : index - 1;

                if (strncmp(key, TSTAT_THRESHOLD_ID_NODE, keyLen) == 0) {
                    trigger->trigger.thermostat.id  = strdup(linkedListGetElementAt(list, index));
                } else if (strncmp(key, TSTAT_THRESHOLD_LOW_NODE, keyLen) == 0) {
                    const char* item = linkedListGetElementAt(list, index);

                    errno = 0;
                    trigger->trigger.thermostat.bounds.lower = (int) strtol(item, NULL, 10);
                    if (errno == ERANGE) {
                        trigger->trigger.thermostat.bounds.lower = -1000;
                    }
                } else if (strncmp(key, TSTAT_THRESHOLD_HIGH_NODE, keyLen) == 0) {
                    const char* item = linkedListGetElementAt(list, index);

                    errno = 0;
                    trigger->trigger.thermostat.bounds.upper = (int) strtol(item, NULL, 10);
                    if (errno == ERANGE) {
                        trigger->trigger.thermostat.bounds.upper = -1000;
                    }
                }
            }
        }
        hashMapIteratorDestroy(iterator);

        if ((trigger->trigger.thermostat.id == NULL) || (strlen(trigger->trigger.thermostat.id) == 0)) {
            hashMapDestroy(map, free_multitrigger_map);
            icrule_free_trigger(trigger);

            errno = EBADMSG;
            return -1;
        }

        linkedListAppend(triggers, trigger);
    }

    hashMapDestroy(map, free_multitrigger_map);

    return 0;
}

static int parse_time_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    xmlNodePtr node;
    icrule_trigger_t* trigger;

    trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_TIME);
    if (trigger == NULL) return -1;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "when") == 0) {
                if (icrule_parse_time_slot(node, &trigger->trigger.time.when) < 0) {
                    icrule_free_trigger(trigger);

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, "end") == 0) {
                if (icrule_parse_time_slot(node, &trigger->trigger.time.end) < 0) {
                    icrule_free_trigger(trigger);

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, "repeat") == 0) {
                trigger->trigger.time.repeat_interval = icrule_get_xml_int(node, NULL, 0);

                if (trigger->trigger.time.repeat_interval > 0) {
                    trigger->trigger.time.repeat_interval *= 60; // convert to seconds
                }
            } else if (strcmp((const char*) node->name, "randomize") == 0) {
                trigger->trigger.time.randomize = icrule_get_xml_bool(node, NULL, false);
            }
        }
    }

    linkedListAppend(triggers, trigger);

    return 0;
}

static int parse_cloud_simple_eval(xmlNodePtr parent, icrule_cloud_comparison_simple_t* eval)
{
    xmlNodePtr node;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "eventName") == 0) {
                eval->event_name = icrule_get_xml_string(node, NULL, NULL);
            }
        }
    }

    return 0;
}

static int parse_cloud_complex_eval(xmlNodePtr parent, icrule_cloud_comparison_complex_t* eval)
{
    xmlNodePtr node;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "attributeName") == 0) {
                eval->attribute_name = icrule_get_xml_string(node, NULL, NULL);
            } else if (strcmp((const char*) node->name, "comparisonMethod") == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    if (strcmp((const char*) value, "eq") == 0) {
                        eval->operator = ICRULE_CLOUD_OPERATOR_EQ;
                    } else if (strcmp((const char*) value, "lt") == 0) {
                        eval->operator = ICRULE_CLOUD_OPERATOR_LT;
                    } else if (strcmp((const char*) value, "le") == 0) {
                        eval->operator = ICRULE_CLOUD_OPERATOR_LE;
                    } else if (strcmp((const char*) value, "gt") == 0) {
                        eval->operator = ICRULE_CLOUD_OPERATOR_GT;
                    } else if (strcmp((const char*) value, "ge") == 0) {
                        eval->operator = ICRULE_CLOUD_OPERATOR_GE;
                    }

                    xmlFree(value);
                }
            } else if (strcmp((const char*) node->name, "comparisonValue") == 0) {
                eval->value = icrule_get_xml_double(node, NULL, 0);
            }
        }
    }

    return 0;
}

static int parse_cloud_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    xmlNodePtr node;
    icrule_trigger_t* trigger;

    trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_CLOUD);
    if (trigger == NULL) return -1;

    trigger->trigger.cloud.comparison_type = ICRULE_CLOUD_CMP_TYPE_INVALID;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "cloudObjectID") == 0) {
                trigger->trigger.cloud.id = icrule_get_xml_string(node, NULL, NULL);
            } else if (strcmp((const char*) node->name, "simpleEval") == 0) {
                trigger->trigger.cloud.comparison_type = ICRULE_CLOUD_CMP_TYPE_SIMPLE;

                if (parse_cloud_simple_eval(node, &trigger->trigger.cloud.comparison.simple) < 0) {
                    icrule_free_trigger(trigger);

                    errno = EBADMSG;
                    return -1;
                }
            } else if (strcmp((const char*) node->name, "comparisonEval") == 0) {
                trigger->trigger.cloud.comparison_type = ICRULE_CLOUD_CMP_TYPE_COMPLEX;

                if (parse_cloud_complex_eval(node, &trigger->trigger.cloud.comparison.complex) < 0) {
                    icrule_free_trigger(trigger);

                    errno = EBADMSG;
                    return -1;
                }
            }
        }
    }

    if ((trigger->trigger.cloud.id == NULL) || (strlen(trigger->trigger.cloud.id) == 0)) {
        icrule_free_trigger(trigger);

        errno = EBADMSG;
        return -1;
    }

    linkedListAppend(triggers, trigger);

    return 0;
}

static int parse_cloud_service_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    xmlNodePtr node;
    icrule_trigger_t* trigger;

    trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_CLOUD_SERVICE);
    if (trigger == NULL) return -1;

    trigger->trigger.cloud.comparison_type = ICRULE_CLOUD_CMP_TYPE_SIMPLE;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "cloudServiceName") == 0) {
                trigger->trigger.cloud.id = icrule_get_xml_string(node, NULL, NULL);
            } else if (strcmp((const char*) node->name, "simpleEval") == 0) {
                if (parse_cloud_simple_eval(node, &trigger->trigger.cloud.comparison.simple) < 0) {
                    icrule_free_trigger(trigger);

                    errno = EBADMSG;
                    return -1;
                }
            }
        }
    }

    if ((trigger->trigger.cloud.id == NULL) || (strlen(trigger->trigger.cloud.id) == 0)) {
        icrule_free_trigger(trigger);

        errno = EBADMSG;
        return -1;
    }

    linkedListAppend(triggers, trigger);

    return 0;
}

#define ZIGBEE_COMM_STATUS_DEVICE_ID_NODE "zigbeeCommStatusDeviceId"
#define ZIGBEE_COMM_STATUS_STATE_NODE "zigbeeCommStatusState"

static int parse_zigbee_comm_status_trigger(xmlNodePtr parent, icLinkedList* triggers)
{
    uint32_t i, maxEntries = 0;

    xmlNodePtr node;
    icHashMap* map = hashMapCreate();

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, ZIGBEE_COMM_STATUS_STATE_NODE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    icLinkedList* list = strtok2list((const char*) value, ",");
                    xmlFree(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               ZIGBEE_COMM_STATUS_STATE_NODE,
                               hashmap_string_sizeof(ZIGBEE_COMM_STATUS_STATE_NODE),
                               list);
                }
            } else if (strcmp((const char*) node->name, ZIGBEE_COMM_STATUS_DEVICE_ID_NODE) == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    icLinkedList* list = strtok2list((const char*) value, ",");
                    xmlFree(value);

                    maxEntries = (maxEntries > linkedListCount(list)) ? maxEntries : linkedListCount(list);

                    hashMapPut(map,
                               ZIGBEE_COMM_STATUS_DEVICE_ID_NODE,
                               hashmap_string_sizeof(ZIGBEE_COMM_STATUS_DEVICE_ID_NODE),
                               list);
                }
            }
        }
    }

    for (i = 0; i < maxEntries; i++) {
        icHashMapIterator* iterator;
        icrule_trigger_t* trigger;

        trigger = icrule_alloc_trigger(parent, TRIGGER_TYPE_ZIGBEE_COMM_STATUS);
        if (trigger == NULL) {
            hashMapDestroy(map, free_multitrigger_map);

            return -1;
        }

        iterator = hashMapIteratorCreate(map);
        while (hashMapIteratorHasNext(iterator)) {
            void* key;
            uint16_t keyLen;
            icLinkedList* list;
            uint32_t index;

            if (hashMapIteratorGetNext(iterator, &key, &keyLen, (void**) &list)) {
                index = linkedListCount(list);
                index = (i < index) ? i : index - 1;

                if (strncmp(key, ZIGBEE_COMM_STATUS_DEVICE_ID_NODE, keyLen) == 0) {
                    trigger->trigger.zigbeecomm.id  = strdup(linkedListGetElementAt(list, index));
                } else if (strncmp(key, ZIGBEE_COMM_STATUS_STATE_NODE, keyLen) == 0) {
                    const char* item = linkedListGetElementAt(list, index);

                    if (strcmp(item, "lost") == 0) {
                        trigger->trigger.zigbeecomm.state = TRIGGER_ZIGBEE_COMM_STATE_LOST;
                    } else if (strcmp(item, "restored") == 0) {
                        trigger->trigger.zigbeecomm.state = TRIGGER_ZIGBEE_COMM_STATE_RESTORED;
                    }
                }
            }
        }
        hashMapIteratorDestroy(iterator);

        if ((trigger->trigger.zigbeecomm.id == NULL) || (strlen(trigger->trigger.zigbeecomm.id) == 0)) {
            hashMapDestroy(map, free_multitrigger_map);
            icrule_free_trigger(trigger);

            errno = EBADMSG;
            return -1;
        }

        linkedListAppend(triggers, trigger);
    }

    hashMapDestroy(map, free_multitrigger_map);

    return 0;
}

// Free routines
static void trigger_lighting_free(icrule_trigger_t* trigger)
{
    if (trigger->trigger.lighting.id) {
        free((void*) trigger->trigger.lighting.id);
    }
}

static void trigger_doorlock_free(icrule_trigger_t* trigger)
{
    if (trigger->trigger.doorlock.id) {
        free((void*) trigger->trigger.doorlock.id);
    }
}

static void trigger_thermostat_free(icrule_trigger_t* trigger)
{
    if (trigger->trigger.thermostat.id) {
        free((void*) trigger->trigger.thermostat.id);
    }
}

static void trigger_cloud_free(icrule_trigger_t* trigger)
{
    icrule_trigger_cloud_t* cloud = &trigger->trigger.cloud;

    if (cloud->id) {
        free((void*) cloud->id);
    }

    switch (trigger->trigger.cloud.comparison_type) {
        case ICRULE_CLOUD_CMP_TYPE_INVALID:
            break;
        case ICRULE_CLOUD_CMP_TYPE_SIMPLE:
            if (cloud->comparison.simple.event_name) {
                free((void*) cloud->comparison.simple.event_name);
            }
            break;
        case ICRULE_CLOUD_CMP_TYPE_COMPLEX:
            if (cloud->comparison.complex.attribute_name) {
                free((void*) cloud->comparison.complex.attribute_name);
            }
            break;
    }
}

static void trigger_sensor_free(icrule_trigger_t* trigger)
{
    if (trigger->trigger.sensor.id) {
        free((void*) trigger->trigger.sensor.id);
    }
}

static void trigger_zigbee_comm_free(icrule_trigger_t* trigger)
{
    if (trigger->trigger.sensor.id) {
        free((void*) trigger->trigger.sensor.id);
    }
}

static trigger_descriptor_t trigger_descriptors[] = {
    { TRIGGER_SENSOR_NODE, parse_sensor_trigger, trigger_sensor_free },
    { TRIGGER_TOUCHSCREEN_NODE, parse_touchscreen_trigger, NULL },
    { TRIGGER_LIGHTING_NODE, parse_lighting_trigger, trigger_lighting_free },
    { TRIGGER_DOORLOCK_NODE, parse_doorlock_trigger, trigger_doorlock_free },
    { TRIGGER_TSTAT_NODE, parse_thermostat_trigger, trigger_thermostat_free },
    { TRIGGER_TSTAT_THRESHOLD_NODE, parse_thermostat_threshold_trigger, trigger_thermostat_free },
    { TRIGGER_TIME_NODE, parse_time_trigger, NULL },
    { TRIGGER_CLOUD_NODE, parse_cloud_trigger, trigger_cloud_free },
    { TRIGGER_CLOUD_SERVICE_NODE, parse_cloud_service_trigger, trigger_cloud_free },

    // These triggers are not used in production
    { TRIGGER_NETWORK_NODE, NULL, NULL },
    // Used by cool people
    { TRIGGER_ZIGBEE_COMMSTATUS_NODE, parse_zigbee_comm_status_trigger, trigger_zigbee_comm_free },
    { TRIGGER_SWITCH_NODE, NULL, NULL },
    { TRIGGER_RESOURCE_NODE, NULL, NULL },
    { TRIGGER_PANIC_NODE, NULL, NULL },
};

void icrule_free_trigger(void* alloc)
{
    if (alloc) {
        icrule_trigger_t* trigger = alloc;
        trigger_descriptor_t* descriptor = &trigger_descriptors[trigger->type];

        if (descriptor->free) {
            descriptor->free(trigger);
        }

        if (trigger->desc) {
            free(trigger->desc);
        }

        free(alloc);
    }
}

int icrule_parse_trigger_list(xmlNodePtr parent, icrule_trigger_list_t* triggers)
{
    xmlNodePtr node;

    if ((parent == NULL) || (triggers == NULL)) {
        errno = EINVAL;
        return -1;
    }

    triggers->negate = icrule_get_xml_bool(parent, "isNegative", false);
    triggers->delay = icrule_get_xml_int(parent, "delay", 0);

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            int i;

            for (i = 0; i < sizeof_array(trigger_descriptors); i++) {
                trigger_descriptor_t* descriptor = &trigger_descriptors[i];

                if (strcmp((const char*) node->name, descriptor->name) == 0) {
                    icrule_trigger_t* trigger;

                    if (descriptor->handler == NULL) {
                        errno = ENOTSUP;
                        return -1;
                    }

                    if (descriptor->handler(node, triggers->triggers) != 0) {
                        errno = EBADMSG;

                        return -1;
                    }
                }
            }
        }
    }

    return 0;
}
