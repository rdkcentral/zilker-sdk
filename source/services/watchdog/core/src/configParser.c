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

/*
 * configParser.c
 *
 *  Created on: Apr 4, 2011
 *      Author: gfaulkner
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <libxml/parser.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icTypes/icLinkedList.h>
#include <icUtil/stringUtils.h>
#include <icConfig/storage.h>
#include <xmlHelper/xmlHelper.h>
#include "common.h"
#include "configParser.h"
#include "procMgr.h"


// define the config file relative to our HOME_DIR (ex: /vendor/etc/watchdog.conf or /opt/icontrol/etc/watchdog.conf)
//
#define CONFIG_FILE_NAME       "/etc/watchdog.conf"

#define MANAGER_LIST_NODE           "managerList"           // top-level node
#define DEFAULTS_NODE               "defaults"              // defaults for all managers
#define MANAGER_NODE                "managerDef"            // define a service
#define MANAGER_NAME_NODE           "managerName"           // name of the service
#define MANAGER_PATH_NODE           "managerPath"           // path of the service executable
#define MANAGER_RESTART_ON_CRASH    "restartOnCrash"        // if this should be restarted when it dies
#define MANAGER_EXPECTS_ACK         "expectStartupAck"      // if this service will perform a 'startup ack'
#define MIN_RESTART_INTERVAL_NODE   "secondsBetweenRestarts"// pause between restarting a service after death
#define MAX_RESTARTS_PER_MIN_NODE   "maxRestartsPerMinute"  // max number of times a service can bounce before disabling
#define ACTION_ON_MAX_RESTARTS      "actionOnMaxRestarts"
#define AUTO_START_NODE             "autoStart"             // if this service should be launched at startup
#define WAIT_ON_SHUTDOWN_NODE       "waitOnShutdown"        // if 'shutdown' of this service should wait for the IPC response (no 'killl')
#define MANAGER_ARGLIST_NODE        "argList"
#define MANAGER_ARG_NODE            "arg"                   // optional arg to pass to service executable during launch
#define LOGICAL_GROUP_NODE          "logicalGroup"          // group services together (can only be part of 1 group)
#define SINGLE_PHASE_STARTUP_NODE   "singlePhaseStartup"    // start in a single phase before other services

#define REBOOT_ACTION_DEF           "reboot"
#define DONT_RESTART_ACTION         "stopRestarting"

#define JAVA_SERVICE_NODE           "javaDef"               // java service (not a lot we do with it)
#define JAVA_NAME_NODE              "managerName"
#define JAVA_IPC_NODE               "ipcPort"

#define CONF_DIR_MARKER             "CONF_DIR"
#define HOME_DIR_MARKER             "HOME_DIR"

// variables for saveMisbehavingService()
#define WATCHDOG_NAMESPACE          "watchdog"
#define MISBEHAVING_SERVICE_FILE    "badService"

/*
 * private functions
 */

static serviceDefinition *parseManagerNode(xmlNodePtr node);
static serviceDefinition *parseJavaNode(xmlNodePtr node);
static void parseDefaultsNode(xmlNodePtr node);
static char **parseArgList(xmlNodePtr node, uint8_t *argCount, char *progPath);
static char *substituteMarkers(char *input, const char *configDir, const char *homeDir);

// defaults from the defaults
static bool     defaultRestartOnFail = false;       // if true, this service will be restarted when death is detected
static bool     defaultExpectStartupAck = false;    // if true, this service should notify watchdog when it is online
static uint32_t defaultSecondsBetweenRestarts = 0;
static uint16_t defaultMaxRestartsPerMinute = 0;
static uint16_t defaultActionOnMaxRestarts = 0;
static bool     defaultAutoStart = true;            // if true, start this service during watchdog initialization
static uint32_t defaultWaitSecsOnShutdown = 0;      // number of seconds to wait during "shutdown" of this service
static bool     defaultSinglePhaseStartup = false;

/**
 * parses the XML file and build up the managerList
 * internal function, so assumes caller has CFG_MTX locked
 */
static icLinkedList *parseConfiguration(const char *xmlFile, const char *configDir, const char *homeDir)
{
    icLogDebug(WDOG_LOG, "parsing configuration file %s", xmlFile);

    // open the file as an XML document
    //
    xmlDocPtr doc = xmlParseFile(xmlFile);
    if (doc == (xmlDocPtr)0)
    {
        icLogWarn(WDOG_LOG, "unable to parse configuration file %s", xmlFile);
        return NULL;
    }

    // extract the top-level node
    //
    xmlNodePtr head = xmlDocGetRootElement(doc);
    if (head == (xmlNodePtr)0)
    {
        xmlFreeDoc(doc);
        icLogWarn(WDOG_LOG, "configuration document is empty");
        return NULL;
    }

    // double-check that this is <managerList>
    //
    if (xmlStrcmp(head->name,(xmlChar *)MANAGER_LIST_NODE) != 0)
    {
        xmlFreeDoc(doc);
        icLogWarn(WDOG_LOG, "root configuration element is not %s; cannot parse", MANAGER_LIST_NODE);
        return NULL;
    }

    // parse the body of the file
    //
    icLinkedList *retVal = linkedListCreate();
    xmlNodePtr loopNode = head->children;
    xmlNodePtr currentNode = NULL;
    for (currentNode = loopNode ; currentNode != NULL ; currentNode = currentNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currentNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        // look for 'manager' node
        //
        if (strcmp((const char *) currentNode->name, MANAGER_NODE) == 0)
        {
            // parse the manager definition
            //
            serviceDefinition *mgr = parseManagerNode(currentNode);
            if (mgr != NULL)
            {
                // passed validation, so substitute out any HOME_DIR and CONFIG_DIR
                // strings that are defined within the XML
                //
                if (mgr->execPath != NULL)
                {
                    mgr->execPath = substituteMarkers(mgr->execPath, configDir, homeDir);
                    icLogDebug(WDOG_LOG, "%s path=%s", mgr->serviceName, mgr->execPath);
                }
                if (mgr->execArgCount > 0 && mgr->execArgs != NULL)
                {
                    int i;
                    for (i = 0 ; i < mgr->execArgCount ; i++)
                    {
                        mgr->execArgs[i] = substituteMarkers(mgr->execArgs[i], configDir, homeDir);
                        icLogDebug(WDOG_LOG, "%s arg[%d]=%s", mgr->serviceName, i, mgr->execArgs[i]);
                    }
                }

                // now add this definition to the list
                //
                linkedListAppend(retVal, mgr);
            }
        }
        else if (strcmp((const char *) currentNode->name, JAVA_SERVICE_NODE) == 0)
        {
            // parse the Java Service definition
            //
            serviceDefinition *mgr = parseJavaNode(currentNode);
            if (mgr != NULL)
            {
                // now add this definition to the list
                //
                linkedListAppend(retVal, mgr);
            }
        }
        else if (strcmp((const char *) currentNode->name, DEFAULTS_NODE) == 0)
        {
            // parse the defaults definition
            //
            parseDefaultsNode(currentNode);
        }
    }

    // mem cleanup
    //
    icLogDebug(WDOG_LOG, "parsing complete, total manager definitions is %d", linkedListCount(retVal));
    xmlFreeDoc(doc);

    return retVal;
}

/*
 * apply the global defaults to the object (used during parsing)
 */
static void applyDefaults(serviceDefinition *def)
{
    if (def == NULL)
    {
        return;
    }

    // pre-load the defaults
    def->restartOnFail = defaultRestartOnFail;
    def->expectStartupAck = defaultExpectStartupAck;
    def->secondsBetweenRestarts = defaultSecondsBetweenRestarts;
    def->maxRestartsPerMinute = defaultMaxRestartsPerMinute;
    def->actionOnMaxRestarts = defaultActionOnMaxRestarts;
    def->autoStart = defaultAutoStart;
    def->waitSecsOnShutdown = defaultWaitSecsOnShutdown;
    def->singlePhaseStartup = defaultSinglePhaseStartup;
}

/*
 * parse the <manager> node, and place into a 'serviceDefinition' object
 * caller should free the returned object, or place it into the master list
 */
static serviceDefinition *parseManagerNode(xmlNodePtr node)
{
    // got a 'manager' node, so parse into a serviceDefinition
    //
    serviceDefinition *manager = (serviceDefinition *)calloc(1, sizeof(serviceDefinition));
    manager->isJavaService = false;

    //pre-load the defaults
    applyDefaults(manager);

    // lopo through the children of the 'manager' node
    //
    xmlNodePtr kidNodes = node->children;
    xmlNodePtr kid = NULL;
    for (kid = kidNodes ; kid != NULL ; kid = kid->next)
    {
        // skip comments, blanks, etc
        //
        if (kid->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) kid->name, MANAGER_NAME_NODE) == 0)
        {
            // name
            manager->serviceName = getXmlNodeContentsAsString(kid, NULL);
        }
        else if (strcmp((const char *) kid->name, LOGICAL_GROUP_NODE) == 0)
        {
            // logical group
            manager->logicalGroup = getXmlNodeContentsAsString(kid, NULL);
        }
        else if (strcmp((const char *) kid->name, MANAGER_PATH_NODE) == 0)
        {
            // path to the binary
            manager->execPath = getXmlNodeContentsAsString(kid, NULL);
        }
        else if (strcmp((const char *) kid->name, MANAGER_RESTART_ON_CRASH) == 0)
        {
            // restart flag
            manager->restartOnFail = getXmlNodeContentsAsBoolean(kid, defaultRestartOnFail);
        }
        else if (strcmp((const char *) kid->name, MANAGER_EXPECTS_ACK) == 0)
        {
            // this service will acknowledge when it is ready
            manager->expectStartupAck = getXmlNodeContentsAsBoolean(kid, defaultExpectStartupAck);
        }
        else if (strcmp((const char *) kid->name, MIN_RESTART_INTERVAL_NODE) == 0)
        {
            // min restart value
            manager->secondsBetweenRestarts = getXmlNodeContentsAsUnsignedInt(kid, defaultSecondsBetweenRestarts);
        }
        else if (strcmp((const char *) kid->name, MAX_RESTARTS_PER_MIN_NODE) == 0)
        {
            // max restart value
            manager->maxRestartsPerMinute = (uint16_t)getXmlNodeContentsAsUnsignedInt(kid, defaultMaxRestartsPerMinute);
        }
        else if (strcmp((const char *) kid->name, ACTION_ON_MAX_RESTARTS) == 0)
        {
            // action on max restart
            char *action = getXmlNodeContentsAsString(kid, NULL);
            if (action != NULL)
            {
                if (strcmp(action, REBOOT_ACTION_DEF) == 0)
                {
#ifdef CONFIG_LIB_SHUTDOWN
                    manager->actionOnMaxRestarts = REBOOT_ACTION;
#else
                    manager->actionOnMaxRestarts = STOP_RESTARTING_ACTION;
                    icLogError(WDOG_LOG, "configuration error!  cannot set action to REBOOT when local rebooting is not supported! assigning to STOP.");
#endif
                }
                else if (strcmp(action, DONT_RESTART_ACTION) == 0)
                {
                    manager->actionOnMaxRestarts = STOP_RESTARTING_ACTION;
                }
                else
                {
                    icLogWarn(WDOG_LOG, "Unexpected action found: '%s'", action);
                }
                free(action);
            }
        }
        else if (strcmp((const char *) kid->name, AUTO_START_NODE) == 0)
        {
            // auto start mode
            manager->autoStart = getXmlNodeContentsAsBoolean(kid, defaultAutoStart);
        }
        else if (strcmp((const char *) kid->name, WAIT_ON_SHUTDOWN_NODE) == 0)
        {
            // seconds to wait for IPC shutdown
            manager->waitSecsOnShutdown = getXmlNodeContentsAsUnsignedInt(kid, defaultWaitSecsOnShutdown);
        }
        else if (strcmp((const char *) kid->name, SINGLE_PHASE_STARTUP_NODE) == 0)
        {
            // single phase startup
            manager->singlePhaseStartup = getXmlNodeContentsAsBoolean(kid, defaultSinglePhaseStartup);
        }
        else if (strcmp((const char *) kid->name, MANAGER_ARGLIST_NODE) == 0)
        {
            // CLI arguments
            manager->execArgs = parseArgList(kid, &(manager->execArgCount), manager->execPath);
        }
    }

    // validate the manager object before returning it
    //
    if (stringIsEmpty(manager->serviceName) == true ||
        stringIsEmpty(manager->execPath) == true)
    {
        // name or path are not defined, so no way to id and launch this definition
        //
        destroyServiceDefinition(manager);
        manager = NULL;
    }

    /* Conventional argv[0] is the program path. parseArgList will fill it in if the list node exists */
    if (manager != NULL && manager->execArgs == NULL)
    {
        manager->execArgs = calloc(2, sizeof(char *));
        manager->execArgs[0] = strdup(manager->execPath);
    }

    return manager;
}

/*
 * parse the <javaDef> node, and place into a 'serviceDefinition' object
 * caller should free the returned object, or place it into the master list
 */
static serviceDefinition *parseJavaNode(xmlNodePtr node)
{
    // got a 'manager' node, so parse into a serviceDefinition
    //
    serviceDefinition *manager = (serviceDefinition *)calloc(1, sizeof(serviceDefinition));
    manager->isJavaService = true;

    // pre-load the defaults, then clear the ones not applicable for Java services
    // (since we are not the ones to launch them)
    //
    applyDefaults(manager);
    manager->autoStart = false;
    manager->restartOnFail = false;
    manager->expectStartupAck = false;
    manager->actionOnMaxRestarts = STOP_RESTARTING_ACTION;

    // lopo through the children of the 'manager' node
    //
    xmlNodePtr kidNodes = node->children;
    xmlNodePtr kid = NULL;
    for (kid = kidNodes ; kid != NULL ; kid = kid->next)
    {
        // skip comments, blanks, etc
        //
        if (kid->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) kid->name, JAVA_NAME_NODE) == 0)
        {
            // name
            manager->serviceName = getXmlNodeContentsAsString(kid, NULL);
        }
        else if (strcmp((const char *) kid->name, JAVA_IPC_NODE) == 0)
        {
            // ipcPort
            manager->serviceIpcPort = getXmlNodeContentsAsUnsignedInt(kid, 0);
        }
        else if (strcmp((const char *) kid->name, MANAGER_EXPECTS_ACK) == 0)
        {
            // this service will acknowledge when it is ready
            manager->expectStartupAck = getXmlNodeContentsAsBoolean(kid, defaultExpectStartupAck);
        }
    }

    // validate the manager object before returning it
    //
    if (stringIsEmpty(manager->serviceName) == true || manager->serviceIpcPort == 0)
    {
        // name or ipc are not defined, so no way to communicate with this service
        //
        destroyServiceDefinition(manager);
        manager = NULL;
    }

    return manager;
}

/*
 * parse the <defaults> node, and save off our default values to use on
 * subsequent loads of <manager> definitions.
 */
static void parseDefaultsNode(xmlNodePtr node)
{
    // got the 'defaults' node, so parse into static default variables
    //

    // loop through the children of the 'defaults' node
    //
    xmlNodePtr kidNodes = node->children;
    xmlNodePtr kid = NULL;
    for (kid = kidNodes ; kid != NULL ; kid = kid->next)
    {
        // skip comments, blanks, etc
        //
        if (kid->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) kid->name, MANAGER_RESTART_ON_CRASH) == 0)
        {
            // restart flag (default to NO)
            defaultRestartOnFail = getXmlNodeContentsAsBoolean(kid, false);
        }
        else if (strcmp((const char *) kid->name, MANAGER_EXPECTS_ACK) == 0)
        {
            // services will acknowledge when it is ready (default to NO)
            defaultExpectStartupAck = getXmlNodeContentsAsBoolean(kid, false);
        }
        else if (strcmp((const char *) kid->name, MIN_RESTART_INTERVAL_NODE) == 0)
        {
            // min restart value
            defaultSecondsBetweenRestarts = getXmlNodeContentsAsUnsignedInt(kid, 0);
        }
        else if (strcmp((const char *) kid->name, MAX_RESTARTS_PER_MIN_NODE) == 0)
        {
            // max restart value
            defaultMaxRestartsPerMinute = (uint16_t)getXmlNodeContentsAsUnsignedInt(kid, 0);
        }
        else if (strcmp((const char *) kid->name, ACTION_ON_MAX_RESTARTS) == 0)
        {
            // action on max restart
            char *action = getXmlNodeContentsAsString(kid, NULL);
            if (action != NULL)
            {
                if (strcmp(action, REBOOT_ACTION_DEF) == 0)
                {
                    defaultActionOnMaxRestarts = REBOOT_ACTION;
                }
                else if (strcmp(action, DONT_RESTART_ACTION) == 0)
                {
                    defaultActionOnMaxRestarts = STOP_RESTARTING_ACTION;
                }
                else
                {
                    icLogWarn(WDOG_LOG, "Unexpected action found: '%s'", action);
                }
                free(action);
            }
        }
        else if (strcmp((const char *) kid->name, AUTO_START_NODE) == 0)
        {
            // auto start mode (default to YES)
            defaultAutoStart = getXmlNodeContentsAsBoolean(kid, true);
        }
        else if (strcmp((const char *) kid->name, WAIT_ON_SHUTDOWN_NODE) == 0)
        {
            // seconds to wait for IPC shutdown
            defaultWaitSecsOnShutdown = getXmlNodeContentsAsUnsignedInt(kid, 0);
        }
    }
}

/*
 * parse the <argList> nodes
 */
static char **parseArgList(xmlNodePtr node, uint8_t *argCount, char *progPath)
{
    // lopo through the children of the 'argList' node
    // note that we loop through twice so that we can calculate
    // the number of arguments ahead of time, reducing the need
    // for realloc
    //
    uint8_t count = 0;
    xmlNodePtr kidNodes = node->children;
    xmlNodePtr kid = NULL;
    for (kid = kidNodes ; kid != NULL ; kid = kid->next)
    {
        // skip comments, blanks, etc
        //
        if (kid->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) kid->name, MANAGER_ARG_NODE) == 0)
        {
            count++;
        }
    }

    // save the count into the return parameter and add 1 for the arg0 - the program path
    //
    *argCount = ++count;

    // allocate memory for the total list
    //
    char **retVal = (char **)calloc(count+1, sizeof(char *));

    // assign [0] to be the program itself
    //
    retVal[0] = strdup(progPath);

    // loop through again, this time saving the arguments into the array
    //
    count = 1;
    kidNodes = node->children;
    for (kid = kidNodes ; kid != NULL ; kid = kid->next)
    {
        // skip comments, blanks, etc
        //
        if (kid->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) kid->name, MANAGER_ARG_NODE) == 0)
        {
            retVal[count] = getXmlNodeContentsAsString(kid, "");
            count++;
        }
    }

    return retVal;
}

/*
 * substitute all 'searchStr' found in 'input' with 'replaceStr'
 * potentially can free and reallocate 'input', so caller should
 * save the returned string
 */
static char *substitute(char *input, const char *searchStr, const char *replaceStr)
{
    // get the length of the search & replacement values
    //
    size_t searchLen = strlen(searchStr);
    size_t replaceLen = strlen(replaceStr);

    // look for the first occurance of 'search'
    //
    char *ptr = NULL;
    while ((ptr = strstr(input, searchStr)) != NULL)
    {
        // copy bytes from 'input' up-to 'ptr' into a temp buffer
        //
        size_t diff = ptr - input;
        size_t inputLen = strlen(input);

        // larger then we need, but ensure the temp buff can handle the whole
        // input string + the replacement string (even though we essentially
        // remove search string)
        //
        // first, grab all chars up-to 'ptr'
        //
        char *tmp = (char *)malloc(sizeof(char) * (inputLen + replaceLen));
        memset(tmp, 0, inputLen + replaceLen);
        memcpy(tmp, input, diff);

        // append the replacement
        //
        size_t tmpPos = diff;
        memcpy(tmp + tmpPos, replaceStr, replaceLen);
        tmpPos += replaceLen;

        // skip over 'search' in input, and append the rest
        //
        size_t inputPos = diff + searchLen;
        memcpy(tmp + tmpPos, input + inputPos, inputLen - inputPos);

        free (input);
        input = tmp;
    }

    return input;
}

/*
 * swap out CONFIG_DIR and HOME_DIR markers from 'input'.  Returns the new
 * string that should replace the existing one.  Note that if modified, 'input'
 * will be released since a new string will be allocated.
 */
static char *substituteMarkers(char *input, const char *configDir, const char *homeDir)
{
    // replace CONFIG_DIR with 'configDir'
    //
    input = substitute(input, CONF_DIR_MARKER, configDir);

    // replace HOME_DIR with 'homeDir'
    //
    input = substitute(input, HOME_DIR_MARKER, homeDir);

    return input;
}

/*
 * frees the memory associated with the supplied definition.
 */
void destroyServiceDefinition(serviceDefinition *def)
{
    free(def->serviceName);
    def->serviceName = NULL;

    free(def->logicalGroup);
    def->logicalGroup = NULL;

    free(def->execPath);
    def->execPath = NULL;

    if (def->execArgs != NULL)
    {
        int i = 0;
        for (i = 0 ; i < def->execArgCount ; i++)
        {
            free(def->execArgs[i]);
        }
        free(def->execArgs);
        def->execArgs = NULL;
    }

    free(def->shutdownToken);
    def->shutdownToken = NULL;
    free(def);
}

/**
 * parses the watchdog configuration file and returns a
 * linked list of serviceDefinition objects
 */
icLinkedList *loadServiceConfig(const char *configDir, const char *homeDir)
{
    // build up the path of where our configuration file should reside
    // note that this is a read-only file, so we only need to ensure its readable
    // unlike most services (/opt/etc), the watchdog config file should be
    // near our binaries (/vendor/etc)
    //
    char *path = stringBuilder("%s%s", homeDir, CONFIG_FILE_NAME);
    struct stat st;
    if (stat(path, &st) != 0)
    {
        icLogError(WDOG_LOG, "Configuration file %s does not exist", path);
        free(path);
        return NULL;
    }

    // parse the XML file
    //
    icLinkedList *cfg = parseConfiguration(path, configDir, homeDir);
    free(path);
    return cfg;
}

/*
 * potentially called before reboot to save off a problematic serviceName.
 * this will be loaded during our next loadServiceConfig so that we can tag
 * that process as a misbehaving and prevent it from causing an endless reboot cycle
 */
void saveMisbehavingService(const char *serviceName)
{
    if (serviceName != NULL)
    {
        // leverage the storage library to save the serviceName under the watchdog namespace.
        // we are assuming this call won't try to ask propsService as we're in a somewhat
        // poor state (about to reboot) and don't want to hang.
        //
        storageSave(WATCHDOG_NAMESPACE, MISBEHAVING_SERVICE_FILE, serviceName);
    }
}

/*
 * queried during startup to see if we saved a problematic serviceName prior to reboot.
 * after reading the file, it will be deleted.  caller MUST free the returned string.
 */
char *readMisbehavingService()
{
    // see if we have a misbehaving service file stored from before.
    // if we do, destroy it after reading the contents
    //
    char *badServiceName = NULL;
    if (storageLoad(WATCHDOG_NAMESPACE, MISBEHAVING_SERVICE_FILE, &badServiceName) == true)
    {
        storageDelete(WATCHDOG_NAMESPACE, MISBEHAVING_SERVICE_FILE);
    }

    return badServiceName;
}
