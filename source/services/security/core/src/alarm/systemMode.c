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
 * systemMode.c
 *
 * Define the state machine for the 'system mode'.
 * Keeps a config file to track the mode, so requires
 * an initialization call during startup.
 *
 * NOTE: this has it's own mutex and is not part
 *       of the shared SECURITY_MTX.
 *
 * Author: jelderton - 7/9/15
 *-----------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <libxml/tree.h>

#include <icLog/logging.h>
#include <icConcurrent/delayedTask.h>
#include <backup/backupRestoreService_ipc.h>
#include <xmlHelper/xmlHelper.h>
#include <propsMgr/paths.h>
#include <icConfig/backupUtils.h>
#include <icConcurrent/threadUtils.h>
#include "systemMode.h"
#include "../broadcastEvent.h"
#include "../common.h"

#define SCENE_CONFIG_FILENAME   "/scenes.conf"
#define CONFIG_BACKUP_FILE      "/scenes.bak"     // the backup file
#define CONFIG_TMP_FILE         "/scenes.tmp"     // the temporary file

#define MAX_PATH_LEN            1024

// XML parsing tags (note that we use the same tags as before
// to maintain backward compatibility and RMA scenarios)
//
#define ROOT_NODE           "sceneConfig"
#define VERSION_NODE        "versionNumber"
#define ACTIVE_NODE         "active"
#define SCENE_NODE          "scene"     // legacy supoprt, not really used
#define SCENE_NAME_NODE     "name"
#define SCENE_SCOPE_NODE    "readOnly"

/*
 * private variables
 */
static bool didInit = false;                    // not true until we have loaded the config file
static char configFilename[MAX_PATH_LEN];       // xml config file where current mode is stored
static char configBackupFilename[MAX_PATH_LEN]; // backup of the config file
static char configTmpFilename[MAX_PATH_LEN];    // the temp copy of config file
static systemModeSet currentSystemMode = SYSTEM_MODE_HOME;
static uint64_t confVersion = 0;                // xml file version
static pthread_mutex_t MODE_MTX = PTHREAD_MUTEX_INITIALIZER;

/*
 * private function declarations
 */
static bool readConfigFile(const char *path);
static bool writeConfigFile(bool sendEvent);
static void *notifyBackupService(void *arg);

/*
 * one-time init to load the config.  since this makes
 * requests to propsService, should not be called until
 * all of the services are available
 */
void initSystemMode()
{
    pthread_mutex_lock(&MODE_MTX);

    // bail if already did the initialization
    //
    if (didInit == true)
    {
        pthread_mutex_unlock(&MODE_MTX);
        return;
    }

    // load the XML file from our config dir.
    //
    char *configDir = getDynamicConfigPath();
    sprintf(configFilename, "%s%s", configDir, SCENE_CONFIG_FILENAME);
    sprintf(configBackupFilename, "%s%s", configDir, CONFIG_BACKUP_FILE);
    sprintf(configTmpFilename, "%s%s", configDir, CONFIG_TMP_FILE);

    // check for file or a backup
    //
    fileToRead whichFile = chooseFileToRead(configFilename, configBackupFilename, configDir);

    switch (whichFile)
    {
        case ORIGINAL_FILE:                         // original file exists ... read it
        {
            readConfigFile(configFilename);
            break;
        }
        case BACKUP_FILE:                           // backup file exists ... read it
        {
            readConfigFile(configBackupFilename);
            break;
        }
        default:                                    // no file to read ... create one
        {
            // use HOME as the default
            //
            currentSystemMode = SYSTEM_MODE_HOME;
        }
    }

    // cleanup
    //
    free(configDir);

    // set the init flag
    //
    didInit = true;
    pthread_mutex_unlock(&MODE_MTX);
}

/*
 * called during RMA/Restore
 */
bool restoreSystemModeConfig(char *tempDir, char *destDir)
{
    bool retVal = false;

    // if our config file is located in 'tempDir', parse it -
    // effectively overwriting all of the values we have in mem
    //
    char oldFile[MAX_PATH_LEN];
    sprintf(oldFile, "%s/%s", tempDir, SCENE_CONFIG_FILENAME);
    struct stat fileInfo;
    if ((stat(oldFile, &fileInfo) == 0) && (fileInfo.st_size > 5))
    {
        // file exists with at least 5 bytes, so parse it
        //
        icLogDebug(SECURITY_LOG, "loading 'restored config' file %s", oldFile);
        readConfigFile(oldFile);

        // now re-save
        //
        writeConfigFile(false);

        // should be good-to-go
        //
        retVal = true;
    }
    else
    {
        icLogWarn(SECURITY_LOG, "error loading 'restored config' file %s", oldFile);
    }

    return retVal;
}

/*
 * returns the current systemMode value
 *
 * NOTE: should only be called if "supportSystemMode() == true"
 */
systemModeSet getCurrentSystemMode()
{
    systemModeSet retVal;

    // should be fine even if not initialized
    //
    pthread_mutex_lock(&MODE_MTX);
    retVal = currentSystemMode;
    pthread_mutex_unlock(&MODE_MTX);

    return retVal;
}

/*
 * set the current systemMode value
 * returns false if the mode didn't change
 * (generally because it matches the current mode)
 *
 * NOTE: should only be called if "supportSystemMode() == true"
 */
bool setCurrentSystemMode(systemModeSet newMode, uint64_t requestId)
{
    pthread_mutex_lock(&MODE_MTX);

    // bail if didInit is not set.  we don't want to accept
    // changes until we've loaded our configuration and can
    // property emmit events
    //
    if (didInit == false)
    {
        icLogWarn(SECURITY_LOG, "unable to 'setCurrentSystemMode'; state machine was not initialized.");
        pthread_mutex_unlock(&MODE_MTX);
        return false;
    }

    // save off current mode so we can include it in the event
    //
    systemModeSet oldMode = currentSystemMode;

    // compare to ensure this is a true 'change'
    //
    if (newMode == currentSystemMode)
    {
        // fall through so we re-broadcast the mode change in case this request is coming from the server
        //
        icLogDebug(SECURITY_LOG, "ignoring request to 'setCurrentSystemMode'; already at state %s", systemModeNames[newMode]);
    }
    else
    {
        // save the new mode
        //
        currentSystemMode = newMode;

        // write our config file
        //
        writeConfigFile(true);
    }

    // get config version (for the event)
    //
    uint64_t version = confVersion;
    pthread_mutex_unlock(&MODE_MTX);

    // broadcast the event (oldMode & newMode)
    //
    broadcastSystemModeChangedEvent ((char *)systemModeNames[oldMode],
                                     (char *)systemModeNames[newMode],
                                     version, requestId);

    return true;
}

/**
 * gets the version of the storage file
 */
uint64_t getSystemModeConfigFileVersion()
{
    pthread_mutex_lock(&MODE_MTX);
    uint64_t retVal = confVersion;
    pthread_mutex_unlock(&MODE_MTX);

    return retVal;
}

/**
 * populate head with vales read from the XML file
 * internal, so assumes the PROP_MTX is held
 */
static bool readConfigFile(const char *path)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr topNode = NULL;

    // parse the simple config file, which should look something like:
    /*
     * <sceneConfig>
     *     <versionNumber>2</versionNumber>
     *     <active>home</active>
     *     <scene>
     *         <name>home</name>
     *         <readOnly>true</readOnly>
     *     </scene>
     *     <scene>
     *         <name>away</name>
     *         <readOnly>true</readOnly>
     *     </scene>
     *     <scene>
     *         <name>night</name>
     *         <readOnly>true</readOnly>
     *     </scene>
     *     <scene>
     *         <name>vacation</name>
     *         <readOnly>true</readOnly>
     *     </scene>
     * </sceneConfig>
     */

    // assume our linked list is empty, or wanted to be appended to
    //
    icLogDebug(SECURITY_LOG, "reading systemMode configuration file");

    // open/parse the XML file
    //
    doc = xmlParseFile(path);
    if (doc == (xmlDocPtr)0)
    {
        icLogWarn(SECURITY_LOG, "Unable to parse %s", path);
        return false;
    }

    topNode = xmlDocGetRootElement(doc);
    if (topNode == (xmlNodePtr)0)
    {
        icLogWarn(SECURITY_LOG, "Unable to find contents of %s", ROOT_NODE);
        xmlFreeDoc(doc);
        return false;
    }

    // assign defaults
    //
    confVersion = 0;
    currentSystemMode = SYSTEM_MODE_HOME;

    // loop through the children of ROOT
    //
    xmlNodePtr loopNode = topNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode ; currNode != NULL ; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char*)currNode->name, VERSION_NODE) == 0)
        {
            // extract last version
            //
            confVersion = getXmlNodeContentsAsUnsignedLongLong(currNode, 0);
        }
        else if (strcmp((const char*)currNode->name, ACTIVE_NODE) == 0)
        {
            // get the 'current mode'
            //
            char *tmp = getXmlNodeContentsAsString(currNode, NULL);
            if (tmp != NULL)
            {
                // find the systemModeSet string that mathes
                //
                systemModeSet t;
                for (t = SYSTEM_MODE_HOME ; t <= SYSTEM_MODE_VACATION ; t++)
                {
                    if (strcmp(systemModeNames[t], tmp) == 0)
                    {
                        // found match
                        //
                        currentSystemMode = t;
                        break;
                    }
                }

                free(tmp);
            }
        }

        // ignore the <scene> nodes that are in the file.
        // that was from the original design that allowed
        // for custom scenes - which never happened so I'm
        // not going to waste time adding that in now
        //
    }

    icLogDebug(SECURITY_LOG, "done reading systemMode configuration file");

    xmlFreeDoc(doc);
    return true;
}

/**
 * save properties to an XML file
 * internal, so assumes the PROP_MTX is held
 */
static bool writeConfigFile(bool sendEvent)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr rootNode, versionNode, activeNode;
    xmlChar* xmlbuff;
    FILE* outputFile;
    int buffersize;

    char buffer[64];

    icLogDebug(SECURITY_LOG, "writing config file");
    
    // create the XML document structure
    //
    doc = xmlNewDoc(BAD_CAST "1.0");
    rootNode = xmlNewNode(NULL,BAD_CAST ROOT_NODE);

    // add version
    //
    versionNode = xmlNewNode(NULL, BAD_CAST VERSION_NODE);
    sprintf(buffer, "%" PRIu64, ++confVersion);
    xmlNodeSetContent(versionNode, BAD_CAST buffer);
    xmlAddChild(rootNode, versionNode);

    // add node for the 'currrent systemMode'
    //
    activeNode = xmlNewNode(NULL, BAD_CAST ACTIVE_NODE);
    xmlNodeSetContent(activeNode, BAD_CAST systemModeNames[currentSystemMode]);
    xmlAddChild(rootNode, activeNode);

    // at this point, fill in the remainder of the file with
    // the name of each mode, along with a "readOnly = true"
    // this is to keep backward compatibility with the legacy
    // code (which allowed for custom scenes)
    //
    xmlNodePtr currNode, nameNode, scopeNode;
    systemModeSet t;
    for (t = SYSTEM_MODE_HOME ; t <= SYSTEM_MODE_VACATION ; t++)
    {
//        const char *label = getSystemModeName(t);

        // make 'scene' container node
        currNode = xmlNewNode(NULL, BAD_CAST SCENE_NODE);
        xmlAddChild(rootNode, currNode);

        // add 'name'
        nameNode = xmlNewNode(NULL, BAD_CAST SCENE_NAME_NODE);
        xmlNodeSetContent(nameNode, BAD_CAST systemModeNames[t]);
        xmlAddChild(currNode, nameNode);

        // add 'readOnly'
        scopeNode = xmlNewNode(NULL, BAD_CAST SCENE_SCOPE_NODE);
        xmlNodeSetContent(scopeNode, BAD_CAST "true");
        xmlAddChild(currNode, scopeNode);
    }

    xmlDocSetRootElement(doc, rootNode);
    xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);

    // finally, try to write it out
    //
    xmlFreeDoc(doc);
    outputFile = fopen(configTmpFilename, "w");
    if (outputFile == (FILE *)0)
    {
        icLogWarn(SECURITY_LOG, "Unable to open '%s' for writing: %s", configFilename, strerror(errno));
        xmlFree(xmlbuff);

        return false;
    }
    else
    {
        fprintf(outputFile, "%s", xmlbuff);
        fflush(outputFile);
        fclose(outputFile);
        xmlFree(xmlbuff);

        // save the file in a safe way to avoid corruption
        //
        safeFileSave(configTmpFilename, configFilename, configBackupFilename);
    }

    if (sendEvent == true)
    {
        // let backup service know our file changed.  do this in a thread
        // since we don't need to keep the mutex locked while we wait on an IPC
        // to the backup service (could cause deadlocks)
        //
        createDetachedThread(notifyBackupService, NULL, "sysMdCnfChng");
    }
    return true;
}

/*
 * 'taskFunc' for the thread to inform
 * backup service that our config file
 * has recently changed.
 */
static void *notifyBackupService(void *arg)
{
    // let backup service know our file changed
    //
    backupRestoreService_request_CONFIG_UPDATED();
    return NULL;
}
