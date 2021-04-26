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
 * Database of properties stored locally and kept
 * in memory via a HashMap
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <libxml/tree.h>

#include <icBuildtime.h>
#include <icTypes/icHashMap.h>
#include <icLog/logging.h>
#include <icConcurrent/delayedTask.h>
#include <icUtil/parsePropFile.h>
#include <xmlHelper/xmlHelper.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/paths.h>
#include <propsMgr/timezone.h>
#include <backup/backupRestoreService_ipc.h>
#include <icConfig/backupUtils.h>
#include <icUtil/stringUtils.h>
#include <libgen.h>
#include <icUtil/fileUtils.h>
#include "common.h"
#include "properties.h"
#include "broadcastEvent.h"

#define CONFIG_FILE         "genericProps.xml"     // backward compatible with Android & Touchstone
#define CONFIG_BACKUP_FILE  "genericProps.bak"     // the backup file
#define CONFIG_TMP_FILE     "genericProps.tmp"     // the temporary file
#define XHUI_VERSION_FILE   "xfinityhome.apk.ver"  // if xhui is installed, this file exists
#define ROOT_NODE           "properties"
#define VERSION_NODE        "version"
#define SCHEMA_VER_ATTR     "schema"
#define PROP_NODE           "property"
#define KEY_NODE            "key"
#define VALUE_NODE          "value"
#define SOURCE_NODE         "src"

#define MAX_PATHNAME_LEN    1024
#define CURR_SCHEMA_VER     1           // increments when file structure is altered


/*
 * local variables
 */
static char configFilename[MAX_PATHNAME_LEN];       // xml config file where property objects are stored
static char configBackupFilename[MAX_PATHNAME_LEN]; // backup of the config file
static char configTmpFilename[MAX_PATHNAME_LEN];    // the temp copy of config file
static icHashMap *propertyMap = NULL;               // hash of property objects
static uint64_t confVersion = 0;      // xml file version
static pthread_mutex_t    PROP_MTX = PTHREAD_MUTEX_INITIALIZER;
static int backupTask = 0;

/*
 * define the set of properties (keys & values)
 * we must maintain, even when deleted or creating
 * a new config file.
 */
static const char *DEFAULT_PROPERTIES[] = {
        IC_DYNAMIC_DIR_PROP,                      DEFAULT_DYNAMIC_PATH,
        IC_STATIC_DIR_PROP,                       DEFAULT_STATIC_PATH,
        CAMERA_FW_UPGRADE_DELAY_SECONDS_PROPERTY, "0",
        CONFIG_FASTBACKUP_BOOL_PROPERTY,          "false",
        DISCOVER_DISABLED_DEVICES_BOOL_PROPERTY,  "false",
        NO_CAMERA_UPGRADE_BOOL_PROPERTY,          "false",
        TELEMETRY_FAST_UPLOAD_TIMER_BOOL_PROPERTY,"false",
        ZIGBEE_FW_UPGRADE_NO_DELAY_BOOL_PROPERTY, "false",
        PAN_ID_CONFLICT_ENABLED_PROPERTY_NAME,    "false",
        CPE_BLACKLISTED_DEVICES_PROPERTY_NAME,    "",
        XCONF_TELEMETRY_MAX_FILE_ROLL_SIZE,       "200000",
        NULL,                NULL        // must be last to terminate the array
};

/*
 * private function declarations
 */
static bool ensureDefaults();
static property *getPropertyWhenLocked(char *key);
static setPropRc setPropertyWhenLocked(property *prop, bool sendEvent, bool overwrite);
static bool readConfigFile(const char *path);
static bool writeConfigFile(bool sendEvent);
static void notifyBackupService(void *arg);

/**
 * initializes the properties settings
 */
bool initProperties(char *configDir, char *homeDir)
{
    pthread_mutex_lock(&PROP_MTX);

    // setup variables
    //
    confVersion = 0;
    propertyMap = hashMapCreate();


    // load the XML file from our config dir.
    //
    sprintf(configFilename, "%s%s/%s", configDir, CONFIG_SUBDIR, CONFIG_FILE);
    sprintf(configBackupFilename, "%s%s/%s", configDir, CONFIG_SUBDIR, CONFIG_BACKUP_FILE);
    sprintf(configTmpFilename, "%s%s/%s", configDir, CONFIG_SUBDIR, CONFIG_TMP_FILE);

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
            // create the directory so that 'write' works later on
            //
            char dir[MAX_PATHNAME_LEN];
            sprintf(dir, "%s%s", configDir, CONFIG_SUBDIR);
            if (mkdir(dir, 0755) != 0)
            {
                icLogWarn(PROP_LOG, "error creating directory %s - %d %s", dir, errno, strerror(errno));
            }
        }
    }

    // ensure we have 'paths' defined using the values we
    // received via CLI arguments at startup.  note that we ONLY
    // apply these when they are not set (not when they are different)
    //
    bool doSave = false;
    property *tmp = NULL;
    if ((tmp = getPropertyWhenLocked(IC_DYNAMIC_DIR_PROP)) == NULL)
    {
        // not set, so set it
        //
        tmp = createProperty(IC_DYNAMIC_DIR_PROP, configDir, PROPERTY_SRC_DEVICE);
        icLogDebug(PROP_LOG, "setting default %s=%s", IC_DYNAMIC_DIR_PROP, configDir);
        if (setPropertyWhenLocked(tmp, false, false) != SET_PROP_NEW)
        {
            destroy_property(tmp);
        }
        // Also go ahead and setup our env variable, just in case an IPC call to get this is made before our IPC is up
        setenv("IC_CONF", configDir, 1);
        doSave = true;
    }
    if ((tmp = getPropertyWhenLocked(IC_STATIC_DIR_PROP)) == NULL)
    {
        // not set, so set it
        //
        tmp = createProperty(IC_STATIC_DIR_PROP, homeDir, PROPERTY_SRC_DEVICE);
        icLogDebug(PROP_LOG, "setting default %s=%s", IC_STATIC_DIR_PROP, homeDir);
        if (setPropertyWhenLocked(tmp, false, false) != SET_PROP_NEW)
        {
            destroy_property(tmp);
        }
        // Also go ahead and setup our env variable, just in case an IPC call to get this is made before our IPC is up
        setenv("IC_HOME", homeDir, 1);
        doSave = true;
    }

    // ensure we have the other default values loaded
    //
    if (ensureDefaults() == true)
    {
        doSave = true;
    }

    // save our config file if needed
    //
    if (doSave == true)
    {
        // note that we don't send the event to backup/restore
        // since this was most likely a new file or adjustment
        // that should not cause a backup
        //
        writeConfigFile(false);
    }
    pthread_mutex_unlock(&PROP_MTX);

    return true;
}

/*
 * Cleanup properties
 */
void destroyProperties()
{
    pthread_mutex_lock(&PROP_MTX);

    hashMapDestroy(propertyMap, pojoMapDestroyHelper);
    propertyMap = NULL;
    if (backupTask != 0)
    {
        cancelDelayTask(backupTask);
        backupTask = 0;
    }

    pthread_mutex_unlock(&PROP_MTX);
}


/*
 * called during RMA/Restore
 */
bool restorePropConfig(char *tempDir, char *destDir)
{
    bool retVal = false;

    // clear out all known properties
    //
    pthread_mutex_lock(&PROP_MTX);
    hashMapClear(propertyMap, pojoMapDestroyHelper);

    // if our config file is located in 'tempDir', parse it
    //
    char oldFile[MAX_PATHNAME_LEN];
    sprintf(oldFile, "%s/%s", tempDir, CONFIG_FILE);
    struct stat fileInfo;
    if ((stat(oldFile, &fileInfo) == 0) && (fileInfo.st_size > 5))
    {
        // file exists with at least 5 bytes, so parse it
        //
        icLogDebug(PROP_LOG, "loading 'restored config' file %s", oldFile);
        readConfigFile(oldFile);

        // make sure we have the dynamic config dir set
        //
        property *tmp = NULL;
        if ((tmp = getPropertyWhenLocked(IC_DYNAMIC_DIR_PROP)) == NULL)
        {
            // If it wasn't there, we have to default.  The path passed to us is the dynamic CONFIG path, we just want
            // the dynamic path, so get the parent directory
            char *dynamicDir = dirname(destDir);

            // not set, so set it
            //
            tmp = createProperty(IC_DYNAMIC_DIR_PROP, dynamicDir, PROPERTY_SRC_DEVICE);
            icLogDebug(PROP_LOG, "setting default %s=%s", IC_DYNAMIC_DIR_PROP, dynamicDir);
            if (setPropertyWhenLocked(tmp, false, true) != SET_PROP_NEW)
            {
                destroy_property(tmp);
            }
        }

        // ensure we have the other default values loaded
        //
        ensureDefaults();

        // save our config file
        //
        writeConfigFile(false);
        retVal = true;
    }
    else
    {
        icLogWarn(PROP_LOG, "error loading 'restored config' file %s", oldFile);
    }

    // cleanup & return
    //
    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/*
 * helper function to allocate and clear a new property object
 * if 'key' and/or 'value' are not NULL, they will be copied
 * into the new object
 */
property *createProperty(const char *key, const char *val, propSource source)
{
    // allocate & clear memory
    //
    property *retVal = create_property();

    // copy key and/or value
    //
    if (key != NULL)
    {
        retVal->key = strdup(key);
    }
    if (val != NULL)
    {
        retVal->value = strdup(val);
    }
    retVal->source = source;

    return retVal;
}

/**
 * internal function (when the mutex lock is held) to
 * retrieve the property for the given key. Will return
 * NULL if not found
 */
static property *getPropertyWhenLocked(char *key)
{
    // search through linked list looking for this 'key'
    //
    return (property *) hashMapGet(propertyMap, key, (uint16_t)strlen(key));
}

/**
 * retrieve the property for the given key, or NULL if not found.
 * caller MUST NOT free a non-NULL result.
 */
property *getProperty(char *key)
{
    property *retVal = NULL;
    if (key == NULL)
    {
        return retVal;
    }

    // get the lock, then search for this 'key'
    //
    pthread_mutex_lock(&PROP_MTX);
    retVal = getPropertyWhenLocked(key);
    pthread_mutex_unlock(&PROP_MTX);

    return retVal;
}

/**
 * internal function (when the mutex lock is held) to
 * create or update a property value in the list.
 *
 * the caller MUST examine the return value so that it knows
 * if it's save to free "prop" or not:
 *   SET_PROP_NEW        - prop was taken as-is; do NOT free 'prop'
 *   SET_PROP_OVERWRITE, - prop was duplicated; DO FREE 'prop'
 *   SET_PROP_FAILED     - failure; DO FREE 'prop'
 */
static setPropRc setPropertyWhenLocked(property *prop, bool sendEvent, bool overwrite)
{
    // see if we have a property with the same key
    //
    setPropRc retVal = SET_PROP_FAILED;
    property *exists = getPropertyWhenLocked(prop->key);
    if (exists != NULL)
    {
        bool different = false;
        if (stringCompare(prop->value, exists->value, false) != 0)
        {
            different = true;
        }

        // only update if:
        // - told to overwrite
        // - the value is different
        // - increasing the priority (source)
        //
        if (overwrite == true || prop->source > exists->source || (prop->source == exists->source && different == true))
        {
            // setting the property to a "different" value then what we have now,
            // so swap the 'value' of the existing object with the new value
            //
            if (exists->value != NULL)
            {
                // cleanup old value
                //
                free(exists->value);
                exists->value = NULL;
            }
            if (prop->value != NULL)
            {
                // apply new non-null value
                //
                exists->value = strdup(prop->value);
            }
            exists->source = prop->source;

            // broadcast an UPDATE event if sendEvent is true
            //
            if (sendEvent == true)
            {
                broadcastPropertyEvent(GENERIC_PROP_UPDATED, prop->key, prop->value, confVersion + 1, prop->source);
            }

            icLogDebug(PROPS_SERVICE_NAME,"%s: updating property; key=%s value=%s priority=%d",
                       __FUNCTION__, prop->key, stringCoalesce(prop->value), prop->source);
            retVal = SET_PROP_OVERWRITE;
        }
        else
        {
            icLogWarn(PROPS_SERVICE_NAME,"%s: NOT updating property; key=%s value different=%s priority new=%d old=%d",
                      __FUNCTION__, prop->key, stringValueOfBool(different), prop->source, exists->source);
            retVal = SET_PROP_DROPPED;
        }
    }
    else
    {
        // property does not exist, so add this property to the list
        //
        hashMapPut(propertyMap, prop->key, (uint16_t)strlen(prop->key), prop);
        retVal = SET_PROP_NEW;

        // broadcast an ADD event if sendEvent is true
        //
        if (sendEvent == true)
        {
            broadcastPropertyEvent(GENERIC_PROP_ADDED, prop->key, prop->value, confVersion + 1, prop->source);
        }
    }

    return retVal;
}


/**
 * creates or updates a cpe property value
 *
 * the caller MUST examine the return value so that it knows if it's safe to free "prop" or not:
 *   SET_PROP_NEW        - prop was taken as-is; do NOT free 'prop'
 *   SET_PROP_OVERWRITE, - prop was duplicated; DO FREE 'prop'
 *   SET_PROP_FAILED     - failure; DO FREE 'prop'
 */
setPropRc setProperty(property *prop)
{
    // get the lock, then perform the 'set'
    //
    pthread_mutex_lock(&PROP_MTX);
    setPropRc retVal = setPropertyWhenLocked(prop, true, false);
    if (retVal != SET_PROP_FAILED && retVal != SET_PROP_DROPPED)
    {
        // save file
        //
        writeConfigFile(true);
    }
    pthread_mutex_unlock(&PROP_MTX);

    return retVal;
}

/**
 * creates or updates a cpe property value, overwriting even if the values match;
 * forcing the GENERIC_PROP_UPDATED to be broadcast
 *
 * the caller MUST examine the return value so that it knows if it's save to free "prop" or not:
 *   SET_PROP_NEW        - prop was taken as-is; do NOT free 'prop'
 *   SET_PROP_OVERWRITE, - prop was duplicated; DO FREE 'prop'
 *   SET_PROP_FAILED     - failure; DO FREE 'prop'
 */
setPropRc setPropertyOverwrite(property *prop)
{
    // get the lock, then perform the 'set'
    //
    pthread_mutex_lock(&PROP_MTX);
    setPropRc retVal = setPropertyWhenLocked(prop, true, true);
    if (retVal != SET_PROP_FAILED)
    {
        // save file
        //
        writeConfigFile(true);
    }
    pthread_mutex_unlock(&PROP_MTX);

    return retVal;
}

/*
 * create/update a set of properties, but only applies to ones that are new or different.
 * this is more efficient when applying several because it does just a single write
 */
bool setPropertiesBulk(propertyValues *group)
{
    // make sure we have stuff to iterate through
    //
    if (get_count_of_propertyValues_set(group) == 0)
    {
        return false;
    }

    bool doSave = false;
    pthread_mutex_lock(&PROP_MTX);

    // use a hash-map iterator
    //
    icHashMapIterator *loop = hashMapIteratorCreate(group->setValuesMap);
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

        // need to clone the property (in case we keep it), then apply
        //
        property *prop = (property *)mapVal;
        property *copy = createProperty(prop->key, prop->value, prop->source);
        copy->source = prop->source;

        setPropRc worked = setPropertyWhenLocked(copy, true, false);
        if (worked != SET_PROP_FAILED)
        {
            doSave = true;
        }
        if (worked != SET_PROP_NEW)
        {
            // mem cleanup since we didn't take the memory (did a copy or nothing)
            destroy_property(copy);
        }
    }
    hashMapIteratorDestroy(loop);
    if (doSave == true)
    {
        writeConfigFile(true);
    }
    pthread_mutex_unlock(&PROP_MTX);

    return true;
}

/**
 * deletes a cpe property value
 */
bool deleteProperty(char *key)
{
    // delete the prop with a matching key (if found)
    // and free the node/property
    //
    pthread_mutex_lock(&PROP_MTX);
    bool retVal = hashMapDelete(propertyMap, key, (uint16_t) strlen(key), pojoMapDestroyHelper);
    if (retVal == true)
    {
        // send event that we've deleted this property
        // TODO: get the source of the property before deleting it so we can get our event correct
        //
        broadcastPropertyEvent(GENERIC_PROP_DELETED, key, NULL, confVersion+1, PROPERTY_SRC_DEVICE);

        // make sure we didn't delete a default property
        //
        ensureDefaults();

        // save file
        //
        writeConfigFile(true);
    }
    pthread_mutex_unlock(&PROP_MTX);

    return retVal;
}

/*
 * return the set of all known property keys as well as how many
 * are in the return array.  caller must free the returned array
 * as well as each string in the array.
 */
char **getAllPropertyKeys(int *countOut)
{
    // make the array large enough to hold all of our keys
    //
    pthread_mutex_lock(&PROP_MTX);
    int count = hashMapCount(propertyMap);
    char **retVal = (char **)malloc(sizeof(char *) * (count + 1));
    memset(retVal, 0, sizeof(char *) * (count + 1));

    // save the count
    //
    *countOut = count;

    // iterate through the property keys
    //
    count = 0;
    icHashMapIterator *loop = hashMapIteratorCreate(propertyMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        void *mapKey;
        void *mapValue;
        uint16_t mapKeyLen = 0;
        property *prop = NULL;

        // get the next property from the iterator
        //
        hashMapIteratorGetNext(loop, &mapKey, &mapKeyLen, &mapValue);
        prop = (property *) mapValue;

        // add it's key to the return array
        //
        retVal[count] = strdup(prop->key);
        count++;
    }
    hashMapIteratorDestroy(loop);

    pthread_mutex_unlock(&PROP_MTX);
    return retVal;
}

/**
 * gets the version of the storage file
 */
uint64_t getConfigFileVersion()
{
    uint64_t retVal = 0;

    pthread_mutex_lock(&PROP_MTX);
    retVal = confVersion;
    pthread_mutex_unlock(&PROP_MTX);

    return retVal;
}

/**
 * gets the number of cpe properties that are set
 */
int getPropertyCount()
{
    int retVal = 0;

    // if we have a property list, return the size
    //
    pthread_mutex_lock(&PROP_MTX);
    if (propertyMap != NULL)
    {
        retVal = hashMapCount(propertyMap);
    }
    pthread_mutex_unlock(&PROP_MTX);

    return retVal;
}

/*
 * called after init and delete to ensure the
 * default key/value properties are available
 *
 * internal function, so assume caller has PROP_MTX held
 */
static bool ensureDefaults()
{
    // ensure we have default values loaded
    //
    bool retVal = false;
    property *tmp = NULL;
    int i = 0;

    // loop through both key & value
    //
    while (DEFAULT_PROPERTIES[i] != NULL)
    {
        // see if the key is there
        //
        if ((tmp = getPropertyWhenLocked((char *)DEFAULT_PROPERTIES[i])) == NULL)
        {
            // not set, so set it
            //
            tmp = createProperty(DEFAULT_PROPERTIES[i], DEFAULT_PROPERTIES[i+1], PROPERTY_SRC_DEFAULT);
            icLogDebug(PROP_LOG, "setting missing default property %s=%s", DEFAULT_PROPERTIES[i], DEFAULT_PROPERTIES[i+1]);
            if (setPropertyWhenLocked(tmp, false, false) != SET_PROP_NEW)
            {
                destroy_property(tmp);
            }
            retVal = true;
        }

        // move to next key/value pair
        //
        i += 2;
    }
#ifdef CONFIG_CAP_SCREEN
    // make sure that we have the current XHUI version, if installed
    //
    property *staticDirProp = getPropertyWhenLocked(IC_STATIC_DIR_PROP);
    if (staticDirProp != NULL)
    {
        const char *homeDir = staticDirProp->value;
        AUTO_CLEAN(free_generic__auto) char *xhuiVersFile = stringBuilder("%s%s/%s", homeDir, CONFIG_SUBDIR,
                                                                          XHUI_VERSION_FILE);
        if (doesNonEmptyFileExist(xhuiVersFile))
        {
            AUTO_CLEAN(free_generic__auto) char *xhuiVersion = readFileContentsWithTrim(xhuiVersFile);
            if (xhuiVersion != NULL)
            {
                bool needsToSet = false;
                property *xhuiVersProp = getPropertyWhenLocked(CURRENT_XHUI_VERSION);
                if (xhuiVersProp != NULL)
                {
                    if (stringCompare(xhuiVersion,xhuiVersProp->value,false) != 0)
                    {
                        needsToSet = true;
                    }
                }
                else
                {
                    needsToSet = true;
                }
                if (needsToSet == true)
                {
                    icLogDebug(PROP_LOG,"Setting %s to %s",CURRENT_XHUI_VERSION,xhuiVersion);
                    property *newXHuiVersProp = createProperty(CURRENT_XHUI_VERSION,xhuiVersion,PROPERTY_SRC_DEVICE);
                    setPropRc setRc = setPropertyWhenLocked(newXHuiVersProp,false,true);
                    if (setRc != SET_PROP_NEW)
                    {
                        destroy_property(newXHuiVersProp);
                    }
                    if ((setRc != SET_PROP_FAILED) && (setRc != SET_PROP_DROPPED))
                    {
                        retVal = true;
                    }
                }
            }
        }
    }
#endif

    icLogDebug(PROP_LOG, "done filling in missing default properties, total count=%d", hashMapCount(propertyMap));

    return retVal;
}

/*
 * extract the global default values assigned from branding
 * and apply each that is not defined.  cannot be called until
 * AFTER IPC is functional since this depends on assman, which
 * asks this service for the IC_HOME path.
 */
void loadGlobalDefaults()
{
    bool addedSomething = false;

    // extract our branded global default properties (if there)
    // first, locate where we store our defaults (IC_HOME/etc/defaults)
    // get location of our static config
    //
    char target[MAX_PATHNAME_LEN];
    char *homeDir = getStaticConfigPath();
    sprintf(target, "%s/defaults/globalSettings.properties", homeDir);
    free(homeDir);

    icLogInfo(PROP_LOG, "extracting branded file: %s", target);
    pthread_mutex_lock(&PROP_MTX);

    // open our icontrol/etc/timezone.properties file looking for a line
    // that matches the supplied 'timezone' (string should come from the server)
    //
    icPropertyIterator *loop = propIteratorCreate(target);
    while (propIteratorHasNext(loop) == true)
    {
        // for each property defined, see if we have it in our known props.
        // only set the ones we're missing so that we don't revert the values
        // to the default values
        //
        icProperty *tmp = propIteratorGetNext(loop);
        if (getPropertyWhenLocked(tmp->key) == NULL)
        {
            // transfer to 'property' object
            //
            property *prop = create_property();
            prop->key = tmp->key;
            prop->value = tmp->value;
            prop->source = PROPERTY_SRC_DEFAULT;
            tmp->key = NULL;
            tmp->value = NULL;

            // add this property to the list
            //
            icLogDebug(PROP_LOG, "adding branded property %s=%s", prop->key, prop->value);
            hashMapPut(propertyMap, prop->key, (uint16_t)strlen(prop->key), prop);
            addedSomething = true;
        }
        propertyDestroy(tmp);
    }

    // cleanup (including deleting the temp file)
    //
    propIteratorDestroy(loop);
    unlink("/tmnp/globalSettings.props");

    // if somethign was added, save our file
    //
    if (addedSomething == true)
    {
        writeConfigFile(false);
    }
    pthread_mutex_unlock(&PROP_MTX);
}

/*
 * called from readConfigFile when the file we parsed has the
 * schema version of 0 (or missing from the file).
 *
 * returns true if any properties were altered and a re-save is required
 */
static bool migrateFromSchemaZero()
{
    bool retVal = false;

    // trying to reconcile some problems with Legacy and early Zilker configuration files.
    // Legacy had at least 2 issues related to the "source".
    //  1 - reading always assigned properties a source of 0
    //  2 - writing allowed 0, 1, 2, 3 where 1 & 2 were effectively the same
    // These values were not properly mapped into Zilker "source" values, and therefore the
    // best thing to do is just reset everything to 0 and let Server or XConf re-assign the source.
    //
    icHashMapIterator *loop = hashMapIteratorCreate(propertyMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        void *mapKey;
        void *mapValue;
        uint16_t mapKeyLen = 0;
        property *prop = NULL;

        // get the next property from the iterator
        //
        hashMapIteratorGetNext(loop, &mapKey, &mapKeyLen, &mapValue);
        prop = (property *) mapValue;

        // set the source to 0
        //
        prop->source = PROPERTY_SRC_DEFAULT;
        retVal = true;
    }
    hashMapIteratorDestroy(loop);

    return retVal;
}

/**
 * populate head with vales read from the XML file
 * internal, so assumes the PROP_MTX is held
 */
static bool readConfigFile(const char *path)
{
    xmlDocPtr doc;
    xmlNodePtr topNode, loopNode, currentNode, propLoopNode, propNode;

    // assume our linked list is empty, or wanted to be appended to
    //
    icLogDebug(PROP_LOG, "readConfiguration");

    // open/parse the XML file
    //
    doc = xmlParseFile(path);
    if (doc == (xmlDocPtr)0)
    {
        icLogWarn(PROP_LOG, "Unable to parse %s", path);
        return false;
    }

    topNode = xmlDocGetRootElement(doc);
    if (topNode == (xmlNodePtr)0)
    {
        icLogWarn(PROP_LOG, "Unable to find contents of %s", ROOT_NODE);
        xmlFreeDoc(doc);
        return false;
    }

    // look for the schema version attribute that should be on the root node
    //
    int32_t schemaVersion = getXmlNodeAttributeAsInt(topNode, SCHEMA_VER_ATTR, 0);

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

        if (strcmp((const char*)currentNode->name, VERSION_NODE) == 0)
        {
            // extract the version number
            //
            confVersion = getXmlNodeContentsAsUnsignedLongLong(currentNode, 0);
        }

        else if (strcmp((const char*)currentNode->name, PROP_NODE) == 0)
        {
            // create a new properties object container, then populate it
            //
            property *save = createProperty(NULL, NULL, PROPERTY_SRC_DEFAULT);

            // loop through the children to build up a struct to keep in our linked list
            //
            propLoopNode = currentNode->children;
            for (propNode = propLoopNode ; propNode != NULL ; propNode = propNode->next)
            {
                if (propNode->type == XML_ELEMENT_NODE)
                {
                    // look for key, value, or source
                    //
                    if (strcmp((const char*)propNode->name, KEY_NODE) == 0)
                    {
                        save->key = getXmlNodeContentsAsString(propNode, "");
                    }
                    else if (strcmp((const char*)propNode->name, VALUE_NODE) == 0)
                    {
                        save->value = getXmlNodeContentsAsString(propNode, "");
                    }
                    else if (strcmp((const char*)propNode->name, SOURCE_NODE) == 0)
                    {
                        // stored as a numeric string
                        save->source = (propSource)getXmlNodeContentsAsInt(propNode, PROPERTY_SRC_DEFAULT);
                    }
                }
            }

            // add to our list
            //
            if (save->key != NULL && strlen(save->key) > 0 &&
                save->value != NULL && strlen(save->value) > 0)
            {
                hashMapPut(propertyMap, save->key, (uint16_t)strlen(save->key), save);
            }
            else
            {
                // not complete, free it
                //
                destroy_property(save);
            }
        }
    }

    icLogDebug(PROP_LOG, "done reading configuration file; total count=%d", hashMapCount(propertyMap));
    xmlFreeDoc(doc);

    // now that we're done parsing, check the current schema against the one read from the file
    //
    if (schemaVersion != CURR_SCHEMA_VER)
    {
        // need to perform some form of migration.
        //
        if (schemaVersion == 0)
        {
            icLogInfo(PROP_LOG, "converting from schema %"PRIi32" to %"PRIi32, schemaVersion, CURR_SCHEMA_VER);
            if (migrateFromSchemaZero() == true)
            {
                // migration altered something, so re-save our file
                //
                writeConfigFile(false);
            }
        }
    }
    return true;
}

/**
 * save properties to an XML file
 * internal, so assumes the PROP_MTX is held
 */
static bool writeConfigFile(bool sendEvent)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr rootNode, versionNode, propsNode, currNode;
    xmlChar* xmlbuff;
    FILE* outputFile;
    int buffersize;

    char buffer[64];

    icLogDebug(PROP_LOG, "writing config file");

    // create the XML document structure
    //
    doc = xmlNewDoc(BAD_CAST "1.0");

    // create the root node and apply the current schema version
    //
    rootNode = xmlNewNode(NULL,BAD_CAST ROOT_NODE);
    setXmlNodeAttributeAsInt(rootNode, SCHEMA_VER_ATTR, CURR_SCHEMA_VER);

    // add version
    //
    versionNode = xmlNewNode(NULL, BAD_CAST VERSION_NODE);
    sprintf(buffer, "%" PRIu64, ++confVersion);
    xmlNodeSetContent(versionNode, BAD_CAST buffer);
    xmlAddChild(rootNode, versionNode);

    // loop through our linked list, adding a "properties" node for each
    //
    icHashMapIterator *loop = hashMapIteratorCreate(propertyMap);
    while (hashMapIteratorHasNext(loop) == true)
    {
        void *mapKey;
        void *mapValue;
        uint16_t mapKeyLen = 0;
        property *prop = NULL;

        // get the next property from the iterator
        //
        hashMapIteratorGetNext(loop, &mapKey, &mapKeyLen, &mapValue);
        prop = (property *)mapValue;

        // create the parent <property> node
        //
        propsNode = xmlNewNode(NULL, BAD_CAST PROP_NODE);
        xmlAddChild(rootNode, propsNode);

        // add Key
        currNode = xmlNewNode(NULL, BAD_CAST KEY_NODE);
        xmlNodeSetContent(currNode, BAD_CAST prop->key);
        xmlAddChild(propsNode, currNode);

        // add Value
        currNode = xmlNewNode(NULL, BAD_CAST VALUE_NODE);
        xmlNodeSetContent(currNode, BAD_CAST prop->value);
        xmlAddChild(propsNode, currNode);

        // add Source
        currNode = xmlNewNode(NULL, BAD_CAST SOURCE_NODE);
        sprintf(buffer, "%d", prop->source);
        xmlNodeSetContent(currNode, BAD_CAST buffer);
        xmlAddChild(propsNode, currNode);
    }
    hashMapIteratorDestroy(loop);

    xmlDocSetRootElement(doc, rootNode);
    xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);

    // finally, try to write it out
    //
    xmlFreeDoc(doc);
    outputFile = fopen(configTmpFilename, "w");
    if (outputFile == (FILE *)0)
    {
        icLogWarn(PROP_LOG, "Unable to open '%s' for writing: %s", configFilename, strerror(errno));
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
        // let backup service know our file changed.  do this in a "delayed task"
        // since we don't need to keep the mutex locked while we wait on an IPC
        // to the backup service (could cause deadlocks)
        //
        // Only schedule if we don't already have one pending
        if (backupTask == 0 || !isDelayTaskWaiting(backupTask))
        {
            backupTask = scheduleDelayTask(2, DELAY_SECS, notifyBackupService, NULL);
        }
    }
    return true;
}

/*
 * 'taskCallbackFunc' for the 'delayed task' of
 * information backup service that our config file
 * has recently changed.
 */
static void notifyBackupService(void *arg)
{
    // let backup service know our file changed
    //
    backupRestoreService_request_CONFIG_UPDATED();
}


