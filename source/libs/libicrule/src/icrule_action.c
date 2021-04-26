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
// Created by Boyd, Weston on 4/28/18.
//

#include <string.h>

#include <icrule/icrule.h>
#include <errno.h>
#include <icTypes/icHashMap.h>
#include <icUtil/array.h>

#include "icrule_action.h"
#include "icrule_xml.h"
#include "icrule_internal.h"

#define DEFAULT_ACTION_LIST_DIR "."
#define ACTION_ATTACHMENT_KEY "attachment"

static char* action_list_dir = NULL;

static const char* action_dependency_enum2str[] = {
    "camera",
    "lighting",
    "doorLock",
    "temperature",
    "siren",
    "display",
    "alarm",
    "audio",
    "scene",
    "cloud",
};

static const char* action_type_enum2str[] = {
    "cameraID",
    "zoneID",
    "lightID",
    "doorLockID",
    "thermostatID",
    "time",
    "touchscreenStateEnum",
    "armTypeEnum",
    "panicStateEnum",
    "networkStateEnum",
    "doorLockStateEnum",
    "sensorStateEnum",
    "sensorTypeEnum",
    "message",
    "string",
};

// For some rules that are out there, there are parameters which aren't in the master action list.  There doesn't appear
// to be a master action list with these parameters, and so we don't know what all is possible.  For just these
// actions we will ignore any unknown parameters
static const int ignore_unknown_parameter_action_ids[] = {
    1,
    2,
    3
};

static int parse_action_parameter_definition(xmlNodePtr parent, icrule_action_parameter_t* parameter)
{
    xmlNodePtr node;

    parameter->type = ACTION_TYPE_INVALID;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "key") == 0) {
                parameter->key = icrule_get_xml_string(node, NULL, NULL);
            } else if (strcmp((const char*) node->name, "type") == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_CAMERAID]) == 0) {
                        parameter->type = ACTION_TYPE_CAMERAID;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_SENSORID]) == 0) {
                        parameter->type = ACTION_TYPE_SENSORID;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_LIGHTID]) == 0) {
                        parameter->type = ACTION_TYPE_LIGHTID;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_DOOR_LOCKID]) == 0) {
                        parameter->type = ACTION_TYPE_DOOR_LOCKID;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_THERMOSTATID]) == 0) {
                        parameter->type = ACTION_TYPE_THERMOSTATID;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_TIME]) == 0) {
                        parameter->type = ACTION_TYPE_TIME;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_TOUCHSCREEN_STATE]) == 0) {
                        parameter->type = ACTION_TYPE_TOUCHSCREEN_STATE;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_ARM_TYPE]) == 0) {
                        parameter->type = ACTION_TYPE_ARM_TYPE;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_PANIC_STATE]) == 0) {
                        parameter->type = ACTION_TYPE_PANIC_STATE;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_NETWORK_STATE]) == 0) {
                        parameter->type = ACTION_TYPE_NETWORK_STATE;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_DOOR_LOCK_STATE]) == 0) {
                        parameter->type = ACTION_TYPE_DOOR_LOCK_STATE;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_SENSOR_STATE]) == 0) {
                        parameter->type = ACTION_TYPE_SENSOR_STATE;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_SENSOR_TYPE]) == 0) {
                        parameter->type = ACTION_TYPE_SENSOR_TYPE;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_MESSAGE]) == 0) {
                        parameter->type = ACTION_TYPE_MESSAGE;
                    } else if (strcmp((const char*) value, action_type_enum2str[ACTION_TYPE_STRING]) == 0) {
                        parameter->type = ACTION_TYPE_STRING;
                    }

                    xmlFree(value);
                }
            } else if (strcmp((const char*) node->name, "required") == 0) {
                parameter->required = icrule_get_xml_bool(node, NULL, false);
            }
        }
    }

    return (parameter->key == NULL) ? -1 : 0;
}

static int parse_action_dependency(xmlNodePtr parent, icrule_action_t* action)
{
    xmlNodePtr node;

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "type") == 0) {
                xmlChar* value = xmlNodeGetContent(node);

                if (value) {
                    if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_CAMERA]) == 0) {
                        action->dependency = ACTION_DEP_CAMERA;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_LIGHTING]) == 0) {
                        action->dependency = ACTION_DEP_LIGHTING;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_DOOR_LOCK]) == 0) {
                        action->dependency = ACTION_DEP_DOOR_LOCK;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_TEMPERATURE]) == 0) {
                        action->dependency = ACTION_DEP_TEMPERATURE;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_SIREN]) == 0) {
                        action->dependency = ACTION_DEP_SIREN;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_DISPLAY]) == 0) {
                        action->dependency = ACTION_DEP_DISPLAY;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_ALARM]) == 0) {
                        action->dependency = ACTION_DEP_ALARM;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_AUDIO]) == 0) {
                        action->dependency = ACTION_DEP_AUDIO;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_SCENE]) == 0) {
                        action->dependency = ACTION_DEP_SCENE;
                    } else if (strcmp((const char*) value, action_dependency_enum2str[ACTION_DEP_CLOUD]) == 0) {
                        action->dependency = ACTION_DEP_CLOUD;
                    }

                    xmlFree(value);
                }
            }
        }
    }

    return (action->dependency == ACTION_DEP_INVALID) ? -1 : 0;
}

static int parse_action_definition(xmlNodePtr parent, icrule_action_t* action)
{
    xmlNodePtr node;

    action->id = icrule_get_xml_uint64(parent, "actionID", ULLONG_MAX);

    if (action->id == ULLONG_MAX) return -1;

    action->dependency = ACTION_DEP_INVALID;
    action->parameters = hashMapCreate();

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "description") == 0) {
                action->desc = icrule_get_xml_string(node, NULL, NULL);
            } else if (strcmp((const char*) node->name, "target") == 0) {
                action->target = icrule_get_xml_string(node, NULL, NULL);
            } else if (strcmp((const char*) node->name, "dependency") == 0) {
                if (parse_action_dependency(node, action) < 0) {
                    return -1;
                }
            } else if (strcmp((const char*) node->name, "parameterDef") == 0) {
                icrule_action_parameter_t* parameter;

                parameter = malloc(sizeof(icrule_action_parameter_t));
                if (parameter == NULL) return -1;

                memset(parameter, 0, sizeof(icrule_action_parameter_t));

                if (parse_action_parameter_definition(node, parameter) < 0) {
                    icrule_free_parameter(NULL, parameter);
                    return -1;
                }

                hashMapPut(action->parameters,
                           parameter->key,
                           (uint16_t) strlen(parameter->key),
                           parameter);
            }
        }
    }

    return 0;
}

int icrule_action_list_load(const char* filename, icHashMap* map)
{
    xmlDocPtr doc;
    xmlNodePtr top, node;

    doc = xmlParseFile(filename);
    if (doc == NULL) {
        errno = EINVAL;
        return -1;
    }

    top = xmlDocGetRootElement(doc);

    for (node = top->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "action") == 0) {
                icrule_action_t* action;

                action = malloc(sizeof(icrule_action_t));
                if (action == NULL) {
                    xmlFreeDoc(doc);
                    return -1;
                }

                memset(action, 0, sizeof(icrule_action_t));

                if (parse_action_definition(node, action) < 0) {
                    icrule_free_action(action);
                    xmlFreeDoc(doc);
                    return -1;
                }

                hashMapPut(map, &action->id, sizeof(uint64_t), action);
            }
        }
    }

    xmlFreeDoc(doc);

    return 0;
}

static void icrule_free_action_definitions(void* key, void* value)
{
    unused(key);

    if (value) icrule_free_action(value);
}

void icrule_action_list_release(icHashMap* map)
{
    if (map) hashMapDestroy(map, icrule_free_action_definitions);
}

const char* icrule_get_action_list_dir(void)
{
    return (action_list_dir == NULL) ? DEFAULT_ACTION_LIST_DIR : action_list_dir;
}

void icrule_set_action_list_dir(const char* dir)
{
    if ((dir != NULL) && (strlen(dir) != 0)) {
        if (action_list_dir != NULL) {
            free(action_list_dir);
        }

        action_list_dir = strdup(dir);
    }
}

void icrule_update_message_attachment(icLinkedList* actions)
{
    if (actions) {
        icLinkedListIterator* iterator;
        icrule_action_t* message_action = NULL;
        char* attachment = NULL;

        iterator = linkedListIteratorCreate(actions);
        while (linkedListIteratorHasNext(iterator)) {
            icrule_action_t* action = linkedListIteratorGetNext(iterator);

            switch (action->id) {
                case 1:
                case 2:
                    message_action = action;
                    break;
                case 21:
                    if (attachment) free(attachment);

                    attachment = strdup("picture");
                    break;
                case 22:
                    if (attachment) free(attachment);

                    attachment = strdup("video");
                    break;
                default:
                    break;
            }
        }
        linkedListIteratorDestroy(iterator);

        if (message_action && attachment) {
            icrule_action_parameter_t* parameter;

            parameter = malloc(sizeof(icrule_action_parameter_t));
            if (parameter) {
                parameter->key = strdup(ACTION_ATTACHMENT_KEY);
                parameter->value = attachment;

                hashMapPut(message_action->parameters,
                           parameter->key,
                           (uint16_t) strlen(parameter->key),
                           parameter);
            }
        } else {
            if (attachment) free(attachment);
        }
    }
}

void icrule_free_parameter(void* key, void* value)
{
    unused(key);

    if (value) {
        icrule_action_parameter_t* parameter = value;

        if (parameter->key) free(parameter->key);
        if (parameter->value) free(parameter->value);

        free(parameter);
    }
}

void icrule_free_action(void* alloc)
{
    if (alloc) {
        icrule_action_t* action = alloc;

        if (action->desc) free(action->desc);
        if (action->target) free(action->target);

        if (action->parameters) hashMapDestroy(action->parameters, icrule_free_parameter);

        free(action);
    }
}

static bool ignore_unknown_action_parameter(uint64_t actionId)
{
    bool ignore = false;
    for(int i = 0; i < ARRAY_LENGTH(ignore_unknown_parameter_action_ids); ++i) {
        if (ignore_unknown_parameter_action_ids[i] == actionId) {
            ignore = true;
            break;
        }
    }

    return ignore;
}

static int parse_action_parameter(xmlNodePtr parent, icrule_action_t* action)
{
    xmlNodePtr node;
    char* value = NULL;

    /* First pull the value as we will use the key later to do the
     * map lookup. If the key is not in the map then we have an
     * error condition. If the key is in the map then we just
     * move the value into the parameter entry.
     */
    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "value") == 0) {
                value = icrule_get_xml_string(node, NULL, NULL);
                break;
            }
        }
    }

    // We did not find any value parameter.
    if (value == NULL) return -1;

    /* We found a valid value but it is blank. This is
     * due to an empty key/value being sent down. To
     * preserve backward compatability we will allow
     * this to pass through. The action will be
     * removed later as the value will never have been
     * set.
     */
    if (strlen(value) == 0) {
        free(value);
        return 0;
    }

    /* Now find the key itself */
    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "key") == 0) {
                xmlChar* key = xmlNodeGetContent(node);

                if (key) {
                    icrule_action_parameter_t* parameter;

                    parameter = hashMapGet(action->parameters, key, (uint16_t) strlen((const char*) key));
                    if (parameter == NULL) {
                        free(value);
                        xmlFree(key);
                        if (ignore_unknown_action_parameter(action->id) == true) {
                            return 0;
                        } else {
                            return -1;
                        }
                    }

                    parameter->value = value;
                    value = NULL;

                    xmlFree(key);
                }
            }
        }
    }

    free(value);

    return 0;
}

static icrule_action_t* copy_action(icrule_action_t* dst, const icrule_action_t* src)
{
    icHashMapIterator* hash_iterator;

    if (src == NULL) return NULL;

    dst->id = src->id;

    if (src->desc != NULL) {
        dst->desc = strdup(src->desc);
    }

    if (src->target != NULL) {
        dst->target = strdup(src->target);
    }

    dst->dependency = src->dependency;
    dst->parameters = hashMapCreate();

    hash_iterator = hashMapIteratorCreate(src->parameters);
    while (hashMapIteratorHasNext(hash_iterator)) {
        void *key;
        uint16_t keyLen;
        icrule_action_parameter_t* parameter = NULL;

        if (hashMapIteratorGetNext(hash_iterator, &key, &keyLen, (void**) &parameter)) {
            icrule_action_parameter_t* new_parameter;

            new_parameter = malloc(sizeof(icrule_action_parameter_t));
            if (new_parameter == NULL) {
                hashMapIteratorDestroy(hash_iterator);
                hashMapDestroy(dst->parameters, icrule_free_parameter);

                return NULL;
            }

            new_parameter->type = parameter->type;
            new_parameter->required = parameter->required;

            new_parameter->key = (parameter->key == NULL) ? NULL :  strdup(parameter->key);
            new_parameter->value = (parameter->value == NULL) ? NULL : strdup(parameter->value);

            if (new_parameter->key != NULL)
            {
                hashMapPut(dst->parameters,
                           new_parameter->key,
                           (uint16_t) strlen(new_parameter->key),
                           new_parameter);
            }
            else
            {
                icrule_free_parameter(NULL, new_parameter);
            }
        }
    }

    hashMapIteratorDestroy(hash_iterator);

    return dst;
}

static int action_cleanup_invalid(icHashMap* map)
{
    icHashMapIterator* iterator;

    iterator = hashMapIteratorCreate(map);
    while (hashMapIteratorHasNext(iterator)) {
        void *key;
        uint16_t keyLen;
        icrule_action_parameter_t* parameter = NULL;

        if (hashMapIteratorGetNext(iterator, &key, &keyLen, (void**) &parameter)) {
            if (parameter->value == NULL) {
                if (parameter->required) {
                    hashMapIteratorDestroy(iterator);
                    return -1;
                } else {
                    hashMapDelete(map, key, keyLen, icrule_free_parameter);
                }
            }
        }
    }

    hashMapIteratorDestroy(iterator);

    return 0;
}

void free_multiaction_map(void* key, void* value)
{
    unused(key);

    if (value) {
        linkedListDestroy(value, NULL);
    }
}

static int action_parse_multivalue(icrule_action_t* action, icLinkedList* actions)
{
    icHashMapIterator* iterator;
    icrule_action_parameter_t* parameter;
    void *id = NULL;

    /*
     * First loop.
     *
     * We need to know if this action contains a parameter than may have
     * multiple entries per "value".
     */
    iterator = hashMapIteratorCreate(action->parameters);
    while ((id == NULL) && hashMapIteratorHasNext(iterator)) {
        uint16_t keyLen;

        if (hashMapIteratorGetNext(iterator, &id, &keyLen, (void**) &parameter)) {
            switch (parameter->type) {
                case ACTION_TYPE_CAMERAID:
                case ACTION_TYPE_SENSORID:
                case ACTION_TYPE_LIGHTID:
                case ACTION_TYPE_DOOR_LOCKID:
                case ACTION_TYPE_THERMOSTATID:
                    break;
                default:
                    id = NULL;
                    break;
            }
        }
    }
    hashMapIteratorDestroy(iterator);

    /*
     * We have a valid action parameter type in this action.
     * Thus we need to make sure that we support comma-delim
     * entries in ALL parameters for the action.
     */
    if (id) {
        icHashMap* parameterMap = hashMapCreate();
        uint16_t i, maxEntries = 0;

        /*
         * Second loop.
         *
         * Create the lists and puts the
         * lists into a hash table with the key as the
         * parameter key.
         */
        iterator = hashMapIteratorCreate(action->parameters);
        while (hashMapIteratorHasNext(iterator)) {
            void* key;
            uint16_t keyLen;

            if (hashMapIteratorGetNext(iterator, &key, &keyLen, (void**) &parameter)) {
                icLinkedList* list;

                list = strtok2list(parameter->value, ",");
                if (list) {
                    uint16_t listSize = linkedListCount(list);
                    if (listSize == 0) {
                        linkedListDestroy(list, NULL);
                    } else {
                        hashMapPut(parameterMap, key, keyLen, list);

                        /*
                         * We need to make sure we know how many entries there are in the
                         * largest list.
                         */
                        maxEntries = (listSize > maxEntries) ? listSize : maxEntries;
                    }
                }
            }
        }
        hashMapIteratorDestroy(iterator);

        if (maxEntries == 1) {
            /*
             * We don't need to actually *do* anything here.
             * Thus we can just attach the original action and move on.
             */

            linkedListAppend(actions, action);
        } else {
            /*
             * Third loop.
             *
             * Create a new action for each entry.
             */
            for (i = 0; i < maxEntries; i++) {
                icrule_action_t* new_action;

                new_action = malloc(sizeof(icrule_action_t));
                if (new_action == NULL) {
                    hashMapDestroy(parameterMap, free_multiaction_map);

                    errno = ENOMEM;
                    return -1;
                }

                if (copy_action(new_action, action) == NULL) {
                    hashMapDestroy(parameterMap, free_multiaction_map);
                    icrule_free_action(new_action);

                    return -1;
                }

                /*
                 * Fourth, and final, loop. Yeah...
                 *
                 * Go through all the original parameters and pull
                 * out their respective value lists. Assign
                 * the new value based on the current entry.
                 */
                iterator = hashMapIteratorCreate(parameterMap);
                while (hashMapIteratorHasNext(iterator)) {
                    void* key;
                    uint16_t keyLen;

                    icLinkedList* list;

                    if (hashMapIteratorGetNext(iterator, &key, &keyLen, (void**) &list)) {
                        uint32_t listSize = linkedListCount(list);

                        parameter = hashMapGet(new_action->parameters, key, keyLen);

                        free(parameter->value);

                        /*
                         * If this parameter does not contain the same number of
                         * comma-delim entries then resuse the last available entry.
                         */
                        if (i >= listSize) {
                            parameter->value = strdup(linkedListGetElementAt(list, listSize - 1));
                        } else {
                            parameter->value = strdup(linkedListGetElementAt(list, i));
                        }
                    }
                }
                hashMapIteratorDestroy(iterator);

                linkedListAppend(actions, new_action);
            }

            icrule_free_action(action);
        }

        hashMapDestroy(parameterMap, free_multiaction_map);
    } else {
        linkedListAppend(actions, action);
    }

    return 0;
}

int icrule_parse_action(xmlNodePtr parent, icLinkedList* actions, icHashMap* actionMap)
{
    xmlNodePtr node;
    icrule_action_t *action, *action_definition;

    action = malloc(sizeof(icrule_action_t));
    if (action == NULL) return -1;

    memset(action, 0, sizeof(icrule_action_t));

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "actionID") == 0) {
                action->id = icrule_get_xml_uint64(node, NULL, ULLONG_MAX);
                if (action->id == ULLONG_MAX) {
                    icrule_free_action(action);
                    return -1;
                }
            }
        }
    }

    action_definition = hashMapGet(actionMap, &action->id, sizeof(uint64_t));
    if (action_definition == NULL) {
        icrule_free_action(action);
        return -1;
    }

    copy_action(action, action_definition);

    for (node = parent->children; node != NULL; node = node->next) {
        if (node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) node->name, "parameter") == 0) {
                if (parse_action_parameter(node, action) < 0) {
                    icrule_free_action(action);
                    return -1;
                }
            }
        }
    }

    if (action_cleanup_invalid(action->parameters) < 0) {
        icrule_free_action(action);
        return -1;
    }

    if (action_parse_multivalue(action, actions) < 0) {
        icrule_free_action(action);
        return -1;
    }

    return 0;
}
