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
 * properties.c
 *
 * Command line utility to get, set, delete properties
 * that are stored within the propsService.  Access
 * the service via IPC calls.
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/stat.h>

#include <icLog/logging.h>
#include <icIpc/ipcMessage.h>
#include <propsMgr/propsService_pojo.h>
#include <propsMgr/propsService_ipc.h>
#include <libxml2/libxml/tree.h>
#include <xmlHelper/xmlHelper.h>
#include <icUtil/stringUtils.h>

/*
 * local variables & types
 */
typedef enum {
    NO_MODE_YET,
    GET_MODE,
    SET_MODE,
    DEL_MODE,
    LIST_MODE
} modeEnum;

/*
 * private function declarations
 */
static void printUsage();
static void doCleanup(char *key, char *value, char *filename);
static char *searchConfigFile(const char *path, char *search);
static char *getResultErrorMessage(propSetResult result);

/*
 * main entry point
 */
int main(int argc, char *argv[])
{
    int c;
    char *key = (char *)0;
    char *value = (char *)0;
    char *filename = NULL;
    char *source = NULL;
    propSource sourceVal = PROPERTY_SRC_DEFAULT;
    modeEnum mode = NO_MODE_YET;
    bool getVerbose = false;
    bool listFull = false;
    bool waitForService = false;
    bool overwrite = false;

    // init logger in case libraries we use attempt to log
    // and prevent debug crud from showing on the console
    //
    initIcLogger();
    setIcLogPriorityFilter(IC_LOG_WARN);

    // parse CLI args
    //
    while ((c = getopt(argc,argv,"gGsdk:v:f:S:hlLwo")) != -1)
    {
        switch (c)
        {
            case 'g':       // GET mode option
            {
                mode = GET_MODE;
                getVerbose = false;
                break;
            }
            case 'G':       // GET mode option
            {
                mode = GET_MODE;
                getVerbose = true;
                break;
            }
            case 's':       // SET mode option
            {
                mode = SET_MODE;
                break;
            }
            case 'd':       // DEL mode option
            {
                mode = DEL_MODE;
                break;
            }
            case 'l':       // LIST mode option
            {
                mode = LIST_MODE;
                listFull = false;
                break;
            }
            case 'L':       // LIST ALL option
            {
                mode = LIST_MODE;
                listFull = true;
                break;
            }

            case 'k':
            {
                if (key != NULL)
                {
                    fprintf(stderr,"Can only specify one key (-k)\n  Use -h option for usage\n");
                    doCleanup(key, value, filename);
                    closeIcLogger();
                    return EXIT_FAILURE;
                }
                key = strdup(optarg);
                break;
            }
            case 'v':
            {
                if (value != NULL)
                {
                    fprintf(stderr,"Can only specify one value (-v)\n  Use -h option for usage\n");
                    doCleanup(key, value, filename);
                    closeIcLogger();
                    return EXIT_FAILURE;
                }
                value = strdup(optarg);
                break;
            }
            case 'S':
            {
                uint64_t tmpSourceVal;

                if (source != NULL)
                {
                    fprintf(stderr, "Can only specify one source (-S)\n  Use -h option for usage\n");
                    doCleanup(key, value, filename);
                    closeIcLogger();
                    return EXIT_FAILURE;
                }
                if (!stringToUnsignedNumberWithinRange(optarg, &tmpSourceVal, 10, PROPERTY_SRC_DEFAULT, PROPERTY_SRC_DEVICE))
                {
                    fprintf(stderr, "Invalid source value specified (valid values include: 0, 1, 2, 3)\n  Use -h option for usage\n");
                    doCleanup(key, value, filename);
                    closeIcLogger();
                    return EXIT_FAILURE;
                }
                source = strdup(optarg);
                sourceVal = tmpSourceVal;
                break;
            }
            case 'f':
            {
                if (filename == NULL)
                {
                    filename = strdup(optarg);
                }
                break;
            }
            case 'w':
            {
                waitForService = true;
                break;
            }
            case 'o':
            {
                overwrite = true;
                break;
            }
            case 'h':       // help option
            {
                printUsage();
                doCleanup(key, value, filename);
                closeIcLogger();
                return EXIT_SUCCESS;
            }
            default:
            {
                fprintf(stderr,"Unknown option '%c'\n",c);
                doCleanup(key, value, filename);
                closeIcLogger();
                return EXIT_FAILURE;
            }
        }
    }

    // look to see that we have a mode set
    //
    if (mode == NO_MODE_YET)
    {
        fprintf(stderr, "No mode defined.  Use -h option for usage\n");
        doCleanup(key, value, filename);
        closeIcLogger();
        return EXIT_FAILURE;
    }

    // most modes require a key, so check that now
    //
    if (key == NULL && mode != LIST_MODE)
    {
        fprintf(stderr,"Must supply the key to use (-k)\n  Use -h option for usage\n");
        doCleanup(key, value, filename);
        closeIcLogger();
        return EXIT_FAILURE;
    }

    // if told to wait, do that before we contact the service
    //
    if (waitForService == true)
    {
        waitForServiceAvailable(PROPSSERVICE_IPC_PORT_NUM, 30);
    }

    // create property container
    //
    property *object = create_property();

    // handle each mode
    //
    int retVal = EXIT_SUCCESS;
    switch (mode)
    {
        case GET_MODE:
        {
            // go direct to file if -f option was given
            //
            if (filename != NULL)
            {
                char *search = searchConfigFile((const char *)filename, key);
                if (search != NULL)
                {
//                    fprintf(stdout, "Property %s=%s\n", key, search);
                    fprintf(stdout, "%s\n", search);
                    free(search);
                }
                else
                {
                    fprintf(stderr, "Property '%s' is not set\n", key);
                    retVal = EXIT_FAILURE;
                }
            }

            // try to get the property with this 'key'
            //
            else if (propsService_request_GET_CPE_PROPERTY(key, object) == IPC_SUCCESS)
            {
                // see if we got a property back
                //
                if (object->value != NULL)
                {
                    if (getVerbose == false)
                    {
                        fprintf(stdout, "%s\n", object->value);
                    }
                    else
                    {
                        fprintf(stdout, "Property %s=%s source=%s\n", key, object->value, propSourceLabels[object->source]);
                    }
                }
                else
                {
                    // not there, exit with an error to aid scripting
                    //
                    fprintf(stderr, "Property '%s' is not set\n", key);
                    retVal = EXIT_FAILURE;
                }
            }
            else
            {
                fprintf(stderr, "Unable to communicate with propsService\n");
            }
            break;
        }

        case SET_MODE:
        {
            // make sure we have a 'value'
            //
            if (value == NULL)
            {
                fprintf(stderr,"Must supply the value to use (-v)\n  Use -h option for usage\n");
                retVal = EXIT_FAILURE;
            }
            else
            {
                propSource savedSource = PROPERTY_SRC_DEFAULT;

                // try to get the property with this 'key' to honor existing source if new source not specified
                //
                if (propsService_request_GET_CPE_PROPERTY(key, object) == IPC_SUCCESS)
                {
                    // see if we got a property back
                    //
                    if (object->value != NULL)
                    {
                        savedSource = object->source;
                    }
                }
                free(object->key);
                free(object->value);

                // fill in 'object', then make the call to the service
                //
                object->key = strdup(key);
                object->value = strdup(value);
                object->source = (source != NULL) ? (sourceVal) : (savedSource);
                propertySetResult *setResult = create_propertySetResult();
                if (overwrite)
                {
                    if (propsService_request_SET_CPE_PROPERTY_OVERWRITE(object,setResult) != IPC_SUCCESS)
                    {
                        fprintf(stderr, "Unable to overwrite property using propsService\n");
                        retVal = EXIT_FAILURE;
                    }
                    else if (setResult->result != PROPERTY_SET_OK)
                    {
                        fprintf(stderr, "Unable to overwrite property using propsService: %s\n",getResultErrorMessage(setResult->result));
                        retVal = EXIT_FAILURE;
                    }
                }
                else
                {
                    if (propsService_request_SET_CPE_PROPERTY(object,setResult) != IPC_SUCCESS)
                    {
                        fprintf(stderr, "Unable to set property using propsService\n");
                        retVal = EXIT_FAILURE;
                    }
                    else if (setResult->result != PROPERTY_SET_OK)
                    {
                        fprintf(stderr, "Unable to set property using propsService: %s\n",getResultErrorMessage(setResult->result));
                        retVal = EXIT_FAILURE;
                    }
                }
                destroy_propertySetResult(setResult);
            }
            break;
        }

        case DEL_MODE:
        {
            // try to remove the property with this 'key'
            //
            if (propsService_request_DEL_CPE_PROPERTY(key) != IPC_SUCCESS)
            {
                fprintf(stderr, "Unable to communicate with propsService\n");
                retVal = EXIT_FAILURE;
            }
            break;
        }

        case LIST_MODE:
        {
            if (listFull == false)
            {
                // get the set of all keys, then print them to the screen
                //
                propertyKeys *keys = create_propertyKeys();
                if (propsService_request_GET_ALL_KEYS(keys) == IPC_SUCCESS)
                {
                    fprintf(stdout, "Key Count: %d\n", linkedListCount(keys->list));

                    // use the linked list iterator
                    //
                    icLinkedListIterator *loop = linkedListIteratorCreate(keys->list);
                    while (linkedListIteratorHasNext(loop) == true)
                    {
                        char *str = (char *) linkedListIteratorGetNext(loop);
                        fprintf(stdout, "  %s\n", str);
                    }
                    linkedListIteratorDestroy(loop);
                }
                else
                {
                    fprintf(stderr, "Unable to communicate with propsService\n");
                }
                destroy_propertyKeys(keys);
            }
            else
            {
                // get the list of all keys & values
                //
                propertyValues *values = create_propertyValues();
                if (propsService_request_GET_ALL_KEY_VALUES(values) == IPC_SUCCESS)
                {
                    fprintf(stdout, "Property Count: %d\n", hashMapCount(values->setValuesMap));

                    // use a hash-map iterator
                    //
                    icHashMapIterator *loop = hashMapIteratorCreate(values->setValuesMap);
                    while (hashMapIteratorHasNext(loop) == true)
                    {
                        void *mapKey;
                        void *mapVal;
                        uint16_t mapKeyLen = 0;

                        // get the key & value
                        //
                        hashMapIteratorGetNext(loop, &mapKey, &mapKeyLen, &mapVal);
                        if (mapKey == NULL || mapVal == NULL)
                        {
                            continue;
                        }

                        // print out the key & value on the same line
                        //
                        property *prop = (property *)mapVal;
                        fprintf(stdout, "  %-30s = %-30s source=%s\n", prop->key, prop->value, propSourceLabels[prop->source]);
                    }
                    hashMapIteratorDestroy(loop);
                }
                else
                {
                    fprintf(stderr, "Unable to communicate with propsService\n");
                }
                destroy_propertyValues(values);
            }
            break;
        }

        // not really necessary, but make the compile warnings go away
//        NO_MODE_YET:
        default:
            fprintf(stderr, "No mode defined.  Use -h option for usage\n");
            break;
    }

    // memory cleanup
    //
    free(source);
    destroy_property(object);
    doCleanup(key, value, filename);
    closeIcLogger();
    return retVal;
}

/*
 * mem cleanup
 */
static void doCleanup(char *key, char *value, char *filename)
{
    if (key != NULL)
    {
        free(key);
    }
    if (value != NULL)
    {
        free(value);
    }
    if (filename != NULL)
    {
        free(filename);
    }
}

/*
 * show user available options
 */
static void printUsage()
{
    fprintf(stderr, "iControl Properties Utility\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  properties <-g|-s|-d|-l> <-k key> [-v value] [-w]\n");
    fprintf(stderr, "    -g : get property value for 'key'\n");
    fprintf(stderr, "    -G : get property value for 'key' and show additional details\n");
    fprintf(stderr, "    -s : set property 'key' with 'value'\n");
    fprintf(stderr, "    -d : delete property for 'key'\n");
    fprintf(stderr, "    -l : list all property keys\n");
    fprintf(stderr, "    -L : list all property keys and values\n");
    fprintf(stderr, "    -k - key to use for get,set,del\n");
    fprintf(stderr, "    -v - value to use when 'set'\n");
    fprintf(stderr, "    -S - source/priority to use when 'set' (0 - 3). default is 3\n");
    fprintf(stderr, "    -w - if necessary, wait for service to be available\n");
    fprintf(stderr, "    -o - overwrite existing key-value pair\n");
    fprintf(stderr, "\n");
}

#define PROP_NODE           "property"
#define KEY_NODE            "key"
#define VALUE_NODE          "value"

/*
 * used to destroy the property object from the hash
 */
//static void cleanupPropertyFunc(void *key, void *value)
//{
//    // 'key' is a pointer to the prop->key
//    // so just remove the "property" (the value)
//    //
//    if (value != NULL)
//    {
//        property *p = (property *)value;
//        free(p->value);
//        free(value);
//    }
//}

/**
 * read the XML file, looking for the value of the provided 'search' key
 */
static char *searchConfigFile(const char *path, char *search)
{
    xmlDocPtr doc;
    xmlNodePtr topNode, loopNode, currentNode, propLoopNode, propNode;

    struct stat info;
    if (stat(path, &info) != 0)
    {
        // file doesn't exist
        //
        return NULL;
    }

    // open/parse the XML file
    //
    doc = xmlParseFile(path);
    if (doc == (xmlDocPtr)0)
    {
//        icLogWarn(PROP_LOG, "Unable to parse %s", path);
        return NULL;
    }

    char *retVal = NULL;
    topNode = xmlDocGetRootElement(doc);
    if (topNode == (xmlNodePtr)0)
    {
//        icLogWarn(PROP_LOG, "Unable to find contents of %s", ROOT_NODE);
        xmlFreeDoc(doc);
        return NULL;
    }

    // loop through the children of ROOT
    //
    loopNode = topNode->children;
    for (currentNode = loopNode ; currentNode != NULL ; currentNode = currentNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currentNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        else if (strcmp((const char*)currentNode->name, PROP_NODE) == 0)
        {
            // have something like:
            //  <property>
            //    <key>CPE_TZ</key>
            //    <value>US/Central</value>
            //    <src>0</src>
            //  </property>
            //
            char *key = NULL;
            char *value = NULL;
            propLoopNode = currentNode->children;
            for (propNode = propLoopNode; propNode != NULL; propNode = propNode->next)
            {
                if (propNode->type == XML_ELEMENT_NODE)
                {
                    // look for key, value, or source
                    //
                    if (strcmp((const char *) propNode->name, KEY_NODE) == 0)
                    {
                        if (key != NULL)
                        {
                            free(key);
                        }
                        key = getXmlNodeContentsAsString(propNode, NULL);
                    }
                    else if (strcmp((const char *) propNode->name, VALUE_NODE) == 0)
                    {
                        if (value != NULL)
                        {
                            free(value);
                        }
                        value = getXmlNodeContentsAsString(propNode, NULL);
                    }
                }
            }

            // see if the property we're looking for
            //
            if (key != NULL && strcmp(key, search) == 0 && value != NULL)
            {
                // found match, save the 'value'
                //
                retVal = strdup(value);
            }

            // cleanup
            //
            if (key != NULL)
            {
                free(key);
            }
            if (value != NULL)
            {
                free(value);
            }

            if (retVal != NULL)
            {
                // found our match, break from "outer" loop
                break;
            }
        }
    }

    xmlFreeDoc(doc);
    return retVal;
}

static char *getResultErrorMessage(propSetResult result)
{
    switch (result)
    {
        case PROPERTY_SET_IPC_ERROR:
        {
            return "Unable to communicate with property service";
        }

        case PROPERTY_SET_ALREADY_EXISTS:
        {
            return "Property already exists";
        }

        case PROPERTY_SET_INVALID_REQUEST:
        {
            return "Request to property service was not valid";
        }

        case PROPERTY_SET_GENERAL_ERROR:
        {
            return "General error setting property with property service; see logs for details";
        }

        case PROPERTY_SET_VALUE_NOT_ALLOWED:
        {
            return "The property cannot be set to the value requested";
        }

        case PROPERTY_SET_OK:
        {
            return "Property successful set";
        }

        default:
        {
            return "Unexpected error code from property service";
        }
    }
}