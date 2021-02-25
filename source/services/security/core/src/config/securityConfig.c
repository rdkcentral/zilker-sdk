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
 * securityConfig.c
 *
 * Main configuration for the security (alarm panel) layer.
 * Persists the user codes and alarm panel options.
 *
 * NOTE: this does NOT contain current alarm panel status information.
 *
 * Author: jelderton -  10/30/18.
 *-----------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <errno.h>
#include <libxml/tree.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <icLog/logging.h>
#include <icConfig/backupUtils.h>
#include <icConfig/protectedConfig.h>
#include <icConfig/obfuscation.h>
#include <icUtil/base64.h>
#include <icUtil/stringUtils.h>
#include <icConcurrent/delayedTask.h>
#include <icConcurrent/threadUtils.h>
#include <propsMgr/paths.h>
#include <propsMgr/commonProperties.h>
#include <propsMgr/propsHelper.h>
#include <backup/backupRestoreService_ipc.h>
#include <xmlHelper/xmlHelper.h>
#include "config/securityConfig.h"
#include "securityProps.h"
#include "common.h"

// defaults
#define CONFIG_FILE             "security.conf"
#define CONFIG_BACKUP_FILE      "security.bak"    // the backup file
#define CONFIG_TMP_FILE         "security.tmp"    // the temporary file
#define CONFIG_BRANDING_FILE    "defaults/security.conf.default"
#define MASTER_USER_UUID        0                 // legacy code used 0, but I think that's risky because a blank object assigns a uuid of 0.  tried -10, but the server doesn't like it
#define DURESS_USER_UUID        INT32_MAX         // max of int32_t == 2.4m
#define DEFAULT_INSTALLER_CODE  "4321"
#define DEFAULT_MASTER_CODE     "1234"
#define DEFAULT_DURESS_CODE     "DDDD"
#define DEFAULT_MAX_USER_UUID   1                 // try to avoid a user UUID of 0
#define DEFAULT_ENTRY_DELAY     30
#define DEFAULT_EXIT_DELAY      60
#define DEFAULT_DIALER_DELAY    30
#define DEFAULT_SOUND_DURATION  4
#define DEFAULT_SWINGER_FLAG    true
#define DEFAULT_SWINGER_MAX     2
#define DEFAULT_FIRE_FLAG       false
#define DEFAULT_TEST_SEND_FLAG  true                // default to On for Activation
#define DEFAULT_PIEZO_VOLUME    50                  // Maximum volume
#define DEFAULT_DEFER_TROUBLE_ENABLED_FLAG          true    // default to true
#define DEFAULT_DEFER_TROUBLES_START_HOUR   20      // Start hour in 24 hour time
#define DEFAULT_DEFER_TROUBLES_START_MINUTE 0
#define DEFAULT_DEFER_TROUBLES_DURATION     12      // Duration in hours

// XML nodes & attributes (same as Legacy Java code for compatibility)
#define ROOT_NODE                       "securityConf"
#define OBFUSCATED_KEY_NODE             "e2_sig"                // 'key' for encode/decode 'e2_*' nodes
#define OBFUSCATE_KEY                   "security"              // simple yet not out of place.  need to make this better
#define E2_INSTALLER_CODE_NODE          "e2_installerCode"      // newer encrpytion
#define E2_MASTER_CODE_NODE             "e2_masterCode"         // newer encryption
#define E2_DURESS_CODE_NODE             "e2_duressCode"         // newer encryption
#define USER_CODE_NODE                  "userCode"
#define GUEST_CODE_NODE                 "guestCode"
#define ARM_ONLY_CODE_NODE              "armOnlyCode"
#define E2_CODE_NODE                    "e2_code"               // newer encryption
#define FRIENDLY_NAME_NODE              "friendlyName"
#define UID_NODE                        "uid"
#define ENTRY_DELAY_NODE                "entryDelay"
#define EXIT_DELAY_NODE                 "exitDelay"
#define DIALER_DELAY_NODE               "dialerDelay"
#define SWINGER_SHUTDOWN_ENABLED_NODE   "swingerShutdownEnabled"
#define SWINGER_SHUTDOWN_MAX_TRIPS_NODE "swingerShutdownMaxTrips"
#define VERSION_NUMBER_NODE             "versionNumber"
#define VALID_SUNDAY_NODE               "validSunday"
#define VALID_MONDAY_NODE               "validMonday"
#define VALID_TUESDAY_NODE              "validTuesday"
#define VALID_WEDNESDAY_NODE            "validWednesday"
#define VALID_THURSDAY_NODE             "validThursday"
#define VALID_FRIDAY_NODE               "validFriday"
#define VALID_SATURDAY_NODE             "validSaturday"
#define ALARM_SOUND_DURATION_NODE       "alarmSoundDuration"
#define FIRE_ALARM_VERIFICATION_NODE    "fireAlarmVerification"
#define TEST_ALARM_SEND_CODES_NODE      "testAlarmSendCodes"

#define DEFER_TROUBLES_PROP_KEY                      "cpe.dnd.default.flag"
#define DEFER_TROUBLES_SLEEP_HOURS_ENABLED_NODE      "deferTroublesSleepHoursEnabled"
#define DEFER_TROUBLES_SLEEP_HOURS_START_HOUR_NODE   "deferTroublesSleepHoursStartHour"
#define DEFER_TROUBLES_SLEEP_HOURS_START_MINUTE_NODE "deferTroublesSleepHoursStartMinute"
#define DEFER_TROUBLES_SLEEP_HOURS_DURATION_NODE     "deferTroublesSleepHoursDuration"

// forward declarations
static void setupDefaultValues(bool loadBranding);
bool readSecurityConfigFile(const char *path);   // not static so it can be used by zilkerDecryptor
static bool writeSecurityConfigFile(bool sendEvent);

// private variables
static pthread_mutex_t CFG_MTX = PTHREAD_MUTEX_INITIALIZER;
static uint64_t     configVersion = 0;
static char         *configFilename = NULL;
static char         *configBackupFilename = NULL;
static char         *configTmpFilename = NULL;
static pcData       *xmlCryptKey = NULL;        // key to encrypt/decrypt our 'e2_*' nodes
static char         *installerCode = NULL;
static char         *masterCode = NULL;
static char         *duressCode = NULL;
static icLinkedList *userCodesList = NULL;
static int32_t      maxUuid = DEFAULT_MAX_USER_UUID;    // highest known uuid
static uint16_t     entryDelay = 0;
static uint16_t     exitDelay = 0;
static uint16_t     dialerDelay = 0;
static uint16_t     alarmSoundDuration = 0;
static bool         swingerShutdownEnabled = false;
static uint8_t      swingerShutdownMaxTrips = 0;
static bool         fireAlarmVerificationEnabled = false;
static bool         testAlarmSendCodesEnabled = false;

// support deferring troubles during "sleep hours"
static bool    deferTroublesEnabled        = DEFAULT_DEFER_TROUBLE_ENABLED_FLAG;
static uint8_t deferTroublesDurationHours  = DEFAULT_DEFER_TROUBLES_DURATION;
static uint8_t deferTroublesStartHour      = DEFAULT_DEFER_TROUBLES_START_HOUR;
static uint8_t deferTroublesStartMinute    = DEFAULT_DEFER_TROUBLES_START_MINUTE;

/*
 * one time initialization of the security configuration.
 * will attempt to read the $IC_CONF/etc/securityConf file.
 */
void initSecurityConfig()
{
    // setup variables
    //
    configVersion = 0;
    userCodesList = linkedListCreate();

    // ask propsService for the configuration directory
    //
    char *configDir = getDynamicConfigPath();

    // define our filenames based on this config path
    //
    ssize_t configLen = strlen(configDir);
    configFilename = (char *)malloc(sizeof(char) * (strlen(CONFIG_FILE) + configLen + 2));
    sprintf(configFilename, "%s/%s", configDir, CONFIG_FILE);
    configBackupFilename = (char *)malloc(sizeof(char) * (strlen(CONFIG_BACKUP_FILE) + configLen + 2));
    sprintf(configBackupFilename, "%s/%s", configDir, CONFIG_BACKUP_FILE);
    configTmpFilename = (char *)malloc(sizeof(char) * (strlen(CONFIG_TMP_FILE) + configLen + 2));
    sprintf(configTmpFilename, "%s/%s", configDir, CONFIG_TMP_FILE);

    // check for file or a backup
    //
    fileToRead whichFile = chooseFileToRead(configFilename, configBackupFilename, configDir);
    switch (whichFile)
    {
        case ORIGINAL_FILE:
        {
            // original file exists ... read it
            readSecurityConfigFile(configFilename);
            break;
        }
        case BACKUP_FILE:
        {
            // backup file exists ... read it
            readSecurityConfigFile(configBackupFilename);
            break;
        }
        default:
        {
            // no file to read ... create one
            //
            setupDefaultValues(true);
            writeSecurityConfigFile(false);
        }
    }
    
    // cleanup
    //
    free(configDir);
}

/*
 * cleanup called during process shutdown
 */
void destroySecurityConfig()
{
    pthread_mutex_lock(&CFG_MTX);
    free(configFilename);
    configFilename = NULL;
    free(configBackupFilename);
    configBackupFilename = NULL;
    free(configTmpFilename);
    configTmpFilename = NULL;
    free(installerCode);
    installerCode = NULL;
    free(masterCode);
    masterCode = NULL;
    free(duressCode);
    duressCode = NULL;
    linkedListDestroy(userCodesList, freeKeypadUserCodeFromList);
    userCodesList = NULL;
    pthread_mutex_unlock(&CFG_MTX);
}

/*
 * called during RMA/Restore
 */
bool restoreSecurityConfig(char *tempDir, char *destDir)
{
    bool retVal = false;

    // if our config file is located in 'tempDir', parse it -
    // effectively overwriting all of the values we have in mem
    //
    char *oldFile = stringBuilder("%s/%s", tempDir, CONFIG_FILE);
    struct stat fileInfo;
    if ((stat(oldFile, &fileInfo) == 0) && (fileInfo.st_size > 5))
    {
        // file exists with at least 5 bytes, so parse it
        //
        pthread_mutex_lock(&CFG_MTX);
        icLogDebug(SECURITY_LOG, "loading 'restored config' file %s", oldFile);
        readSecurityConfigFile(oldFile);

        // now re-save
        //
        writeSecurityConfigFile(false);
        pthread_mutex_unlock(&CFG_MTX);

        // should be good-to-go
        //
        retVal = true;
    }
    else
    {
        icLogWarn(SECURITY_LOG, "error loading 'restored config' file %s", oldFile);
    }
    free(oldFile);

    return retVal;
}

/*
 * return the internal version of the config file.
 * here to support legacy SMAP communication to the server.
 */
uint64_t getSecurityConfigVersion()
{
    pthread_mutex_lock(&CFG_MTX);
    uint64_t retVal = configVersion;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * return the 'installer' code string.
 * caller is responsible for releasing the returned string.
 */
char *getInstallerUserCode()
{
    char *retVal = NULL;

    pthread_mutex_lock(&CFG_MTX);
    if (installerCode != NULL)
    {
        retVal = strdup(installerCode);
    }
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * change the 'installer' code.  returns true
 * if the change was applied (because it's different)
 */
bool setInstallerUserCode(const char *code)
{
    bool retVal = false;

    if (code != NULL)
    {
        pthread_mutex_lock(&CFG_MTX);
        if (installerCode == NULL || strcmp(code, installerCode) != 0)
        {
            // apply since they are different
            //
            free(installerCode);
            installerCode = strdup(code);
            writeSecurityConfigFile(true);
            retVal = true;
        }
        pthread_mutex_unlock(&CFG_MTX);
    }
    return retVal;
}

/*
 * return the 'master' code string.
 * caller is responsible for releasing the returned string.
 */
char *getMasterUserCode()
{
    char *retVal = NULL;

    pthread_mutex_lock(&CFG_MTX);
    if (masterCode != NULL)
    {
        retVal = strdup(masterCode);
    }
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * change the 'master' code.  returns true
 * if the change was applied (because it's different)
 */
bool setMasterUserCode(const char *code)
{
    bool retVal = false;

    if (code != NULL)
    {
        pthread_mutex_lock(&CFG_MTX);
        if (masterCode == NULL || strcmp(code, masterCode) != 0)
        {
            // apply since they are different
            //
            free(masterCode);
            masterCode = strdup(code);
            writeSecurityConfigFile(true);
            retVal = true;
        }
        pthread_mutex_unlock(&CFG_MTX);
    }
    return retVal;
}

/*
 * returns if use of a duress user is allowed; which is
 * dictated by a property value
 */
bool isDuressUserAllowed()
{
    if (getDuressCodeDisabledProp() == false)
    {
        // duress enabled
        return true;
    }
    return false;
}

/*
 * return the 'duress' code string.
 * caller is responsible for releasing the returned string.
 * note that the notion of a duress user may be disabled.
 * see DURESSCODE_DISABLED property.
 */
char *getDuressUserCode()
{
    char *retVal = NULL;

    pthread_mutex_lock(&CFG_MTX);
    if (duressCode != NULL)
    {
        retVal = strdup(duressCode);
    }
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * change the 'duress' code.  returns true
 * if the change was applied (because it's different).
 * note that the notion of a duress user may be disabled.
 * see DURESSCODE_DISABLED property.
 */
bool setDuressUserCode(const char *code)
{
    bool retVal = false;

    if (code != NULL)
    {
        pthread_mutex_lock(&CFG_MTX);
        if (duressCode == NULL || strcmp(code, duressCode) != 0)
        {
            // apply since they are different
            //
            free(duressCode);
            duressCode = strdup(code);
            writeSecurityConfigFile(true);
            retVal = true;
        }
        pthread_mutex_unlock(&CFG_MTX);
    }
    return retVal;
}

/*
 * used to clone the keypadUserCode objects during a
 * deep-clone of the master list
 */
static void *cloneKeypadUser(void *item, void *context)
{
    keypadUserCode *existing = (keypadUserCode *)item;
    return clone_keypadUserCode(existing);
}

/*
 * used by getAllUserCodes
 */
static bool cloneUserForExport(void *item, void *arg)
{
    // typecast input arguments
    keypadUserCode *user = (keypadUserCode *)item;
    icLinkedList *dest = (icLinkedList *)arg;

    // clone, then append to the output list
    linkedListAppend(dest, clone_keypadUserCode(user));
    return true;
}

/*
 * returns a linked list of all known keypadUserCode objects.
 * caller is responsible for releasing the returned list.
 */
icLinkedList *getAllUserCodes(bool includeInternal)
{
    // make the output list.  we want to somewhat sort
    // by adding the internal users first (otherwise master is at the bottom)
    //
    icLinkedList *retVal = NULL;

    // potentially add 'master' and 'duress'
    //
    if (includeInternal == true)
    {
        // make a blank list
        //
        retVal = linkedListCreate();

        // add master
        char *masterUserCode = getMasterUserCode();
        if (masterUserCode != NULL)
        {
            keypadUserCode *master = create_keypadUserCode();
            master->label = strdup("Master");
            master->uuid = MASTER_USER_UUID;
            master->authorityLevel = KEYPAD_USER_LEVEL_MASTER;
            master->code = masterUserCode;
            master->validSunday = true;
            master->validMonday = true;
            master->validTuesday = true;
            master->validWednesday = true;
            master->validThursday = true;
            master->validFriday = true;
            master->validSaturday = true;
            linkedListAppend(retVal, master);
        }

        // add duress if set and enabled
        char *duressUserCode = getDuressUserCode();
        if (duressUserCode != NULL && isDuressUserAllowed() == true)
        {
            keypadUserCode *duress = create_keypadUserCode();
            duress->label = strdup("Duress");
            duress->uuid = DURESS_USER_UUID;
            duress->authorityLevel = KEYPAD_USER_LEVEL_DURESS;
            duress->code = duressUserCode;
            duress->validSunday = true;
            duress->validMonday = true;
            duress->validTuesday = true;
            duress->validWednesday = true;
            duress->validThursday = true;
            duress->validFriday = true;
            duress->validSaturday = true;
            linkedListAppend(retVal, duress);
        }
        else
        {
            free(duressUserCode);
        }
    }

    // now append (or clone)
    //
    pthread_mutex_lock(&CFG_MTX);
    if (userCodesList != NULL)
    {
        if (retVal == NULL)
        {
            // not getting internal, so just clone what we have
            retVal = linkedListDeepClone(userCodesList, cloneKeypadUser, NULL);
        }
        else
        {
            // iterate & clone each element of the userCodeList (appending to the end of retVal)
            linkedListIterate(userCodesList, cloneUserForExport, retVal);
        }
    }
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * linkedListCompareFunc used to look for the keypadUserCode
 * with the matching uuid
 */
static bool searchListForUserByUuid(void *searchVal, void *item)
{
    int32_t *uuid = (int32_t *)searchVal;
    keypadUserCode *user = (keypadUserCode *)item;

    if (uuid != NULL && *uuid == user->uuid)
    {
        return true;
    }
    return false;
}

/*
 * linkedListCompareFunc used to look for the keypadUserCode
 * with the matching userCode (searchVal is the userCode string)
 */
static bool searchListForUserByCode(void *searchVal, void *item)
{
    char *newCode = (char *)searchVal;
    keypadUserCode *exist = (keypadUserCode *)item;

    // compare codes
    if (stringCompare(newCode, exist->code, false) == 0)
    {
        return true;
    }

    // not the same
    //
    return false;
}

/*
 * linkedListCompareFunc used to look for the keypadUserCode
 * with the matching userCode (searchVal is the userCode string)
 */
static bool searchListForUserByLabel(void *searchVal, void *item)
{
    char *newLabel = (char *)searchVal;
    keypadUserCode *exist = (keypadUserCode *)item;

    // compare labels
    if (stringCompare(newLabel, exist->label, false) == 0)
    {
        return true;
    }

    // not the same
    //
    return false;
}

/*
 * return 'true' if any of the days are set to be valid
 */
static bool anyValidDays(keypadUserCode *user)
{
    if (user->validSunday == true)
    {
        return true;
    }
    if (user->validMonday == true)
    {
        return true;
    }
    if (user->validTuesday == true)
    {
        return true;
    }
    if (user->validWednesday == true)
    {
        return true;
    }
    if (user->validThursday == true)
    {
        return true;
    }
    if (user->validFriday == true)
    {
        return true;
    }
    if (user->validSaturday == true)
    {
        return true;
    }

    return false;
}

/*
 * adds a new user code (does not send the "user added" event)
 */
bool addUserCode(keypadUserCode *user)
{
    // ensure that at least 1 day is enabled
    //
    if (anyValidDays(user) == false)
    {
        icLogWarn(SECURITY_LOG, "addUser: unable to create user; no days are marked 'valid'");
        return false;
    }

    // make sure the label is defined
    //
    if (stringIsEmpty(user->label) == true)   // check for 0 len without a strlen()
    {
        icLogWarn(SECURITY_LOG, "addUser: unable to create user; no label defined");
        return false;
    }

    // make sure the label is allowed
    //
    if (stringCompare(user->label, "master", true) == 0 ||
        stringCompare(user->label, "duress", true) == 0)
    {
        icLogWarn(SECURITY_LOG, "addUser: will not allow the label to be \"master\" or \"duress\"");
        return false;
    }

    // make sure the code is long enough
    // TODO: Needs implementation
    //

    // make sure this code is not used by master, installer, duress, or another user
    // TODO: Needs implementation
    //

    // don't let the user code be escalated to an internal auth level
    //
    switch(user->authorityLevel)
    {
        case KEYPAD_USER_LEVEL_MASTER:
        case KEYPAD_USER_LEVEL_DURESS:
        case KEYPAD_USER_LEVEL_INSTALLER:
        case KEYPAD_USER_LEVEL_INVALID:
            icLogWarn(SECURITY_LOG, "addUser: being asked to increase a user auth level;  not adding");
            return false;

        default:
            break;
    }

    // last check...make sure this label is not in use (code check happened during authenticateUser)
    //
    bool retVal = false;
    pthread_mutex_lock(&CFG_MTX);
    keypadUserCode *labelInUse = linkedListFind(userCodesList, user->label, searchListForUserByLabel);
    if (labelInUse == NULL)
    {
        // assign a valid uuid to the code
        //
        user->uuid = ++maxUuid;

        // clone the object, then add to our list
        //
        linkedListAppend(userCodesList, clone_keypadUserCode(user));
        writeSecurityConfigFile(true);
        retVal = true;
    }
    else
    {
        icLogWarn(SECURITY_LOG, "addUser: cannot add; label %s is in use", user->label);
    }
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * helper for updateUserCode.  assumes LOCK is held...
 * and that the input string is not NULL
 */
static userAuthLevelType isInternalUser(const char *code)
{
    // check master
    //
    if (masterCode != NULL && strcmp(code, masterCode) == 0)
    {
        return KEYPAD_USER_LEVEL_MASTER;
    }

    // check installer
    //
    if (installerCode != NULL && strcmp(code, installerCode) == 0)
    {
        return KEYPAD_USER_LEVEL_INSTALLER;
    }

    // check duress (if enabled)
    //
    if (duressCode != NULL && strcmp(code, duressCode) == 0)
    {
        // same as duress, but allow if duress is disabled
        //
        if (isDuressUserAllowed() == true)
        {
            // duress enabled
            return KEYPAD_USER_LEVEL_DURESS;
        }
    }

    return KEYPAD_USER_LEVEL_INVALID;
}

/*
 * updates an existing user code (does not send the "user modified" event)
 */
bool updateUserCode(keypadUserCode *user)
{
    // Master code is special, handle specially
    //
    if (user->uuid == MASTER_USER_UUID)
    {
        // before applying master, make sure this new code is not being used by another
        //
        icLogInfo(SECURITY_LOG, "modUser: being asked to update MASTER user");
        keypadUserCode *codeInUse = linkedListFind(userCodesList, user->code, searchListForUserByCode);
        if (codeInUse != NULL && codeInUse->uuid != user->uuid)
        {
            icLogWarn(SECURITY_LOG, "modUser: cannot update MASTER with supplied code; it's being used by another user");
            return false;
        }

        // make sure this isn't the same as current-INSTALLER or DURESS (if enabled)
        //
        userAuthLevelType internal = isInternalUser((const char *)user->code);
        if (internal != KEYPAD_USER_LEVEL_INVALID && internal != KEYPAD_USER_LEVEL_MASTER)
        {
            icLogWarn(SECURITY_LOG, "modUser: cannot update MASTER with supplied code; it's being used by another user");
            return false;
        }

        // safe to move forward and update the master code
        //
        return setMasterUserCode(user->code);
    }
    else if (user->uuid == DURESS_USER_UUID)
    {
        if (isDuressUserAllowed() == true)
        {
            // before applying duress, make sure this new code is not being used by another
            //
            icLogInfo(SECURITY_LOG, "modUser: being asked to update DURESS user");
            keypadUserCode *codeInUse = linkedListFind(userCodesList, user->code, searchListForUserByCode);
            if (codeInUse != NULL && codeInUse->uuid != user->uuid)
            {
                icLogWarn(SECURITY_LOG, "modUser: cannot update DURESS with supplied code; it's being used by another user");
                return false;
            }

            // make sure this isn't the same as current-INSTALLER or MASTER
            //
            userAuthLevelType internal = isInternalUser((const char *)user->code);
            if (internal != KEYPAD_USER_LEVEL_INVALID && internal != KEYPAD_USER_LEVEL_DURESS)
            {
                icLogWarn(SECURITY_LOG, "modUser: cannot update DURESS with supplied code; it's being used by another user");
                return false;
            }

            // safe to apply the duress code change
            //
            return setDuressUserCode(user->code);
        }

        icLogWarn(SECURITY_LOG, "modUser: being asked to update DURESS user, but duress is disabled");
        return false;
    }

    // don't let the user code be escalated to an internal auth level
    //
    switch(user->authorityLevel)
    {
        case KEYPAD_USER_LEVEL_MASTER:
        case KEYPAD_USER_LEVEL_DURESS:
        case KEYPAD_USER_LEVEL_INSTALLER:
        case KEYPAD_USER_LEVEL_INVALID:
            icLogWarn(SECURITY_LOG, "modUser: being asked to increase a user auth level;  not updating");
            return false;

        default:
            break;
    }

    // ensure that at least 1 day is enabled
    //
    if (anyValidDays(user) == false)
    {
        icLogWarn(SECURITY_LOG, "modUser: unable to update user; no days are marked 'valid'");
        return false;
    }

    // see if we can find the user to update from our list (compare by uuid)
    //
    bool retVal = false;
    pthread_mutex_lock(&CFG_MTX);
    keypadUserCode *existing = linkedListFind(userCodesList, &(user->uuid), searchListForUserByUuid);
    if (existing != NULL)
    {
        // found the one to modify, IF we are changing the code, make sure it's safe
        //
        bool allowMod = false;

        // first make sure this new code is not in use by any internal users (master, installer, duress)
        //
        if (isInternalUser((const char *) user->code) == KEYPAD_USER_LEVEL_INVALID)
        {
            // not a match to internal codes, so search our list to ensure this new code
            // is not being used by another user
            //
            keypadUserCode *codeInUse = linkedListFind(userCodesList, user->code, searchListForUserByCode);
            if (codeInUse == NULL || codeInUse == existing)
            {
                // last check for another's label
                //
                keypadUserCode *labelInUse = linkedListFind(userCodesList, user->label, searchListForUserByLabel);
                if (labelInUse == NULL || labelInUse == existing)
                {
                    // not attempting to steal someones code/label
                    //
                    allowMod = true;
                }
                else
                {
                    icLogWarn(SECURITY_LOG, "modUser: attempting to use a duplicate user label; not updating");
                }
            }
            else
            {
                icLogWarn(SECURITY_LOG, "modUser: attempting to use a duplicate user code; not updating");
            }
        }
        else
        {
            icLogWarn(SECURITY_LOG, "modUser: invalid user code; not updating");
        }

        if (allowMod == true)
        {
            // passed the validation checks, so update all information from 'user'
            // into the 'existing' (the one in our list).
            //
            if (user->code != NULL && stringCompare(existing->code, user->code, false) != 0)
            {
                // code changed
                free(existing->code);
                existing->code = strdup(user->code);
            }
            if (user->label != NULL && stringCompare(existing->label, user->label, false) != 0)
            {
                // label changed
                free(existing->label);
                existing->label = strdup(user->label);
            }
            existing->authorityLevel = user->authorityLevel;
            existing->validSunday = user->validSunday;
            existing->validMonday = user->validMonday;
            existing->validTuesday = user->validTuesday;
            existing->validWednesday = user->validWednesday;
            existing->validThursday = user->validThursday;
            existing->validFriday = user->validFriday;
            existing->validSaturday = user->validSaturday;
//            existing->validStartTimeHour = user->validStartTimeHour;
//            existing->validStartTimeMinute = user->validStartTimeMinute;
//            existing->validEndTimeHour = user->validEndTimeHour;
//            existing->validEndTimeMinute = user->validEndTimeMinute;

            icLogWarn(SECURITY_LOG, "modUser: updated user %"PRIu32, user->uuid);
            writeSecurityConfigFile(true);
            retVal = true;
        }
    }
    else
    {
        icLogWarn(SECURITY_LOG, "modUser: unable to locate user with uuid=%"PRIu32"; not updating", user->uuid);
    }
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * delete an existing user code (does not send the "user deleted" event)
 */
bool deleteUserCode(keypadUserCode *user)
{
    // skip if trying to delete master
    if (user->uuid == MASTER_USER_UUID)
    {
        icLogWarn(SECURITY_LOG, "delUser: being asked to delete MASTER user; ignoring");
        return false;
    }

    // delete the one that matches this user->uid
    //
    pthread_mutex_lock(&CFG_MTX);
    bool retVal = linkedListDelete(userCodesList, &(user->uuid), searchListForUserByUuid, freeKeypadUserCodeFromList);
    if (retVal == true)
    {
        icLogWarn(SECURITY_LOG, "delUser: removed user %"PRIu32, user->uuid);
        writeSecurityConfigFile(true);
    }
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * return the current "entry delay" setting (in seconds)
 */
uint16_t getEntryDelaySecsSetting()
{
    pthread_mutex_lock(&CFG_MTX);
    uint16_t retVal = entryDelay;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * change the current "entry delay" setting.  returns
 * true if value is within bounds, but won't save the value
 * if the value doesn't change. note that the valid range
 * is between ENTRY_DELAY_SEC_MIN and ENTRY_DELAY_SEC_MAX
 */
bool setEntryDelaySecsSetting(uint16_t value)
{
    bool retVal = false;

    if (value >= ENTRY_DELAY_SEC_MIN && value <= ENTRY_DELAY_SEC_MAX)
    {
        pthread_mutex_lock(&CFG_MTX);
        if (value != entryDelay)
        {
            // apply since they are different
            //
            entryDelay = value;
            writeSecurityConfigFile(true);
        }
        // Return true if value is within bounds
        retVal = true;
        pthread_mutex_unlock(&CFG_MTX);
    }
    return retVal;
}

/*
 * return the current "exit delay" setting (in seconds)
 */
uint16_t getExitDelaySecsSetting()
{
    pthread_mutex_lock(&CFG_MTX);
    uint16_t retVal = exitDelay;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * change the current "exit delay" setting.  returns
 * true if value is within bounds, but does not save value
 * if the value doesn't change.  note that the valid range
 * is between EXIT_DELAY_SEC_MIN and EXIT_DELAY_SEC_MAX
 */
bool setExitDelaySecsSetting(uint16_t value)
{
    bool retVal = false;

    if (value >= EXIT_DELAY_SEC_MIN && value <= EXIT_DELAY_SEC_MAX)
    {
        pthread_mutex_lock(&CFG_MTX);
        if (value != exitDelay)
        {
            // apply since they are different
            //
            exitDelay = value;
            writeSecurityConfigFile(true);
        }
        // return true if value is within range
        retVal = true;
        pthread_mutex_unlock(&CFG_MTX);
    }
    return retVal;
}

/*
 * return the current "dialer delay" setting (in seconds)
 */
uint16_t getDialerDelaySecsSetting()
{
    pthread_mutex_lock(&CFG_MTX);
    uint16_t retVal = dialerDelay;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * change the current "dialer delay" setting.  returns
 * true if value is within bounds, does not apply change if
 * value does not change.  note that the valid range
 * is between DIALER_DELAY_SEC_MIN and DIALER_DELAY_SEC_MAX
 */
bool setDialerDelaySecsSetting(uint16_t value)
{
    bool retVal = false;

    if (value >= DIALER_DELAY_SEC_MIN && value <= DIALER_DELAY_SEC_MAX)
    {
        pthread_mutex_lock(&CFG_MTX);
        if (value != dialerDelay)
        {
            // apply since they are different
            //
            dialerDelay = value;
            writeSecurityConfigFile(true);
        }
        retVal = true;
        pthread_mutex_unlock(&CFG_MTX);
    }
    return retVal;
}

/*
 * return the number of minutes that the alarm siren should alarm
 */
uint16_t getAlarmSirenDurationMinutes()
{
    pthread_mutex_lock(&CFG_MTX);
    uint16_t retVal = alarmSoundDuration;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * returns if "swinger shutdown" is enabled or not
 */
bool isSwingerShutdownSettingEnabled()
{
    pthread_mutex_lock(&CFG_MTX);
    bool retVal = swingerShutdownEnabled;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * enable/disable "swinger shutdown"
 * returns true if this was applied
 */
bool setSwingerShutdownSettingEnabled(bool flag)
{
    bool retVal = false;

    pthread_mutex_lock(&CFG_MTX);
    if (flag != swingerShutdownEnabled)
    {
        // apply since they are different
        //
        swingerShutdownEnabled = flag;
        writeSecurityConfigFile(true);
        retVal = true;
    }
    pthread_mutex_unlock(&CFG_MTX);
    return retVal;
}

/*
 * return the current "swinger shutdown" max trips setting.
 * note: should only be utilized if swinger shutdown is enabled.
 */
uint8_t getSwingerShutdownMaxTripsSetting()
{
    pthread_mutex_lock(&CFG_MTX);
    uint8_t retVal = swingerShutdownMaxTrips;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * change the current "swinger shutdown" max trips setting.
 * returns true if value was within bounds, but does not apply
 * update if value doesn't change.  note that the valid range
 * is between SWINGER_TRIPS_MIN and SWINGER_TRIPS_MAX
 */
bool setSwingerShutdownMaxTripsSetting(uint8_t value)
{
    bool retVal = false;

    if (value >= SWINGER_TRIPS_MIN && value <= SWINGER_TRIPS_MAX)
    {
        pthread_mutex_lock(&CFG_MTX);
        if (value != swingerShutdownMaxTrips)
        {
            // apply since they are different
            //
            swingerShutdownMaxTrips = value;
            writeSecurityConfigFile(true);
        }
        retVal = true;
        pthread_mutex_unlock(&CFG_MTX);
    }
    return retVal;
}

/*
 * returns if "fire alarm verification" is enabled
 */
bool isFireAlarmVerificationSettingEnabled()
{
    pthread_mutex_lock(&CFG_MTX);
    bool retVal = fireAlarmVerificationEnabled;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * enable/disable "fire alarm verification" setting
 * returns true if this was applied
 */
bool setFireAlarmVerificationSettingEnabled(bool flag)
{
    bool retVal = false;

    pthread_mutex_lock(&CFG_MTX);
    if (flag != fireAlarmVerificationEnabled)
    {
        // apply since they are different
        //
        fireAlarmVerificationEnabled = flag;
        writeSecurityConfigFile(true);
        retVal = true;
    }
    pthread_mutex_unlock(&CFG_MTX);
    return retVal;
}

/*
 * returns if sending "test alarm" codes is enabled
 */
bool isTestAlarmSendCodesSettingEnabled()
{
    pthread_mutex_lock(&CFG_MTX);
    bool retVal = testAlarmSendCodesEnabled;
    pthread_mutex_unlock(&CFG_MTX);

    return retVal;
}

/*
 * enable/disable sending "test alarm" codes setting
 * returns true if this was applied
 */
bool setTestAlarmSendCodesSettingEnabled(bool flag)
{
    bool retVal = false;

    pthread_mutex_lock(&CFG_MTX);
    if (flag != testAlarmSendCodesEnabled)
    {
        // apply since they are different
        //
        testAlarmSendCodesEnabled = flag;
        writeSecurityConfigFile(true);
        retVal = true;
    }
    pthread_mutex_unlock(&CFG_MTX);
    return retVal;
}

/*
 * return if the 'defer troubles during sleep hours' option is enabled
 */
bool isDeferTroublesEnabled()
{
    pthread_mutex_lock(&CFG_MTX);
    bool retVal = deferTroublesEnabled;
    pthread_mutex_unlock(&CFG_MTX);
    return retVal;
}

/*
 * enable/disable the 'defer troubles during sleep hours' option
 */
bool setDeferTroublesEnabled(bool flag)
{
    bool retVal = false;

    pthread_mutex_lock(&CFG_MTX);
    if (flag != deferTroublesEnabled)
    {
        // apply since they are different
        //
        deferTroublesEnabled = flag;
        writeSecurityConfigFile(true);
        retVal = true;
    }
    pthread_mutex_unlock(&CFG_MTX);
    return retVal;
}

/*
 * populate the 'defer troubles during sleep hours' configuration.
 */
void getDeferTroublesConfiguration(deferTroublesConfig *container)
{
    pthread_mutex_lock(&CFG_MTX);
    container->deferTroublesAtNight = deferTroublesEnabled;
    container->durationInHours = deferTroublesDurationHours;
    container->deferTroublesStartHour = deferTroublesStartHour;
    container->deferTroublesStartMinute = deferTroublesStartMinute;
    pthread_mutex_unlock(&CFG_MTX);
}

/*
 * update the 'defer troubles during sleep hours' configuration
 */
bool setDeferTroublesConfiguration(deferTroublesConfig *container)
{
    bool retVal = false;

    pthread_mutex_lock(&CFG_MTX);
    if (container->deferTroublesAtNight != deferTroublesEnabled ||
        container->durationInHours != deferTroublesDurationHours ||
        container->deferTroublesStartHour != deferTroublesStartHour ||
        container->deferTroublesStartMinute != deferTroublesStartMinute)
    {
        // apply since something was changed
        //
        deferTroublesEnabled = container->deferTroublesAtNight;
        deferTroublesDurationHours = (uint8_t)container->durationInHours;
        deferTroublesStartHour = (uint8_t)container->deferTroublesStartHour;
        deferTroublesStartMinute = (uint8_t)container->deferTroublesStartMinute;
        writeSecurityConfigFile(true);
        retVal = true;
    }
    pthread_mutex_unlock(&CFG_MTX);
    return retVal;
}

/*
 * linkedListItemFreeFunc to delete a keypadUserCode object
 */
void freeKeypadUserCodeFromList(void *item)
{
    if (item != NULL)
    {
        keypadUserCode *code = (keypadUserCode *)item;
        destroy_keypadUserCode(code);
    }
}

/*
 * return the default "on" or "off" for Do Not Disturb.
 * normally it's defaulted to "on", but need a way to overload
 * (unit tests, build tests, ZITH tests, etc).
 */
static bool getDefaultDeferTroublesFlag()
{
    return getPropertyAsBool(DEFER_TROUBLES_PROP_KEY, DEFAULT_DEFER_TROUBLE_ENABLED_FLAG);
}

/*
 * assign defaults and potentially load the default branding file
 */
static void setupDefaultValues(bool loadBranding)
{
    // reset variables
    //
    configVersion = 1;
    free(installerCode);
    installerCode = strdup(DEFAULT_INSTALLER_CODE);
    free(masterCode);
    masterCode = strdup(DEFAULT_MASTER_CODE);
    free(duressCode);
    duressCode = strdup(DEFAULT_DURESS_CODE);
    maxUuid = DEFAULT_MAX_USER_UUID;
    if (userCodesList == NULL)
    {
        userCodesList = linkedListCreate();
    }
    else
    {
        linkedListClear(userCodesList, freeKeypadUserCodeFromList);
    }
    entryDelay = DEFAULT_ENTRY_DELAY;
    exitDelay = DEFAULT_EXIT_DELAY;
    dialerDelay = DEFAULT_DIALER_DELAY;
    alarmSoundDuration = DEFAULT_SOUND_DURATION;
    swingerShutdownEnabled = DEFAULT_SWINGER_FLAG;
    swingerShutdownMaxTrips = DEFAULT_SWINGER_MAX;
    fireAlarmVerificationEnabled = DEFAULT_FIRE_FLAG;
    testAlarmSendCodesEnabled = DEFAULT_TEST_SEND_FLAG;
    deferTroublesEnabled = getDefaultDeferTroublesFlag();
    deferTroublesDurationHours = DEFAULT_DEFER_TROUBLES_DURATION;
    deferTroublesStartHour = DEFAULT_DEFER_TROUBLES_START_HOUR;
    deferTroublesStartMinute = DEFAULT_DEFER_TROUBLES_START_MINUTE;

    if (loadBranding == true)
    {
        // load our branded default configuration (if there)
        // first, locate where we store our defaults (IC_HOME/etc/defaults)
        // get location of our static config (i.e.  /opt/icontrol/etc)
        //
        char *homeDir = getStaticConfigPath();
        char *target = (char *)malloc(strlen(homeDir) + strlen(CONFIG_BRANDING_FILE) + 2);
        sprintf(target, "%s/%s", homeDir, CONFIG_BRANDING_FILE);

        // re-read our config to import the branded settings
        //
        icLogInfo(SECURITY_LOG, "extracting branded default file: %s", target);
        struct stat fileInfo;
        if ((stat(target, &fileInfo) == 0) && (fileInfo.st_size > 5))
        {
            // file exists with at least 5 bytes, so parse it
            //
            readSecurityConfigFile(target);
        }

        // cleanup
        //
        free(target);
        free(homeDir);
    }
}

/*
 * parse <e2_sig>
 */
static pcData *parseXmlKey(xmlNodePtr node)
{
    // get the string from the node
    //
    char *tmp = getXmlNodeContentsAsString(node, NULL);
    if (tmp == NULL)
    {
        // unable to read the node
        //
        icLogError(SECURITY_LOG, "error extracting from %s", OBFUSCATED_KEY_NODE);
        return NULL;
    }

    // base64 decode, then save as our xmlKey
    //
    pcData *retVal = NULL;
    uint8_t *decoded = NULL;
    uint16_t decodeLen = 0;
    if (icDecodeBase64((const char *)tmp, &decoded, &decodeLen) == true)
    {
        // successfully decoded, so un-obfuscate.  unfortunately we need to use a
        // hard-coded obfuscation key.  that is because we cannot wait for a domicileId
        // or something similar
        //
        uint32_t keyLen = 0;
        char *key = unobfuscate(OBFUSCATE_KEY, (uint32_t) strlen(OBFUSCATE_KEY), (const char *) decoded, decodeLen, &keyLen);
        if (key != NULL)
        {
            // save as our return
            //
            retVal = (pcData *)malloc(sizeof(pcData));
            retVal->data = (unsigned char *)key;
            retVal->length = keyLen;
        }
        free(decoded);
    }
    free(tmp);

    return retVal;
}

/*
 * read an XML node, then decrypt it, returning a strdup of the result
 */
static char *extractAndDecryptString(xmlNodePtr node)
{
    char *retVal = NULL;

    if (xmlCryptKey != NULL)
    {
        // get XML node contents
        //
        char *tmp = getXmlNodeContentsAsString(node, NULL);
        if (tmp != NULL)
        {
            // place 'input' (stuff to decrypt) into a pcData container
            //
            pcData input;
            input.data = (unsigned char *)tmp;
            input.length = (uint32_t)strlen(tmp);

            // decrypt via protectConfig.c
            //
            pcData *data = unprotectConfigData(&input, xmlCryptKey);
            if (data != NULL)
            {
                // move from 'data' to 'retVal'
                retVal = (char *)data->data;
                data->data = NULL;
                destroyProtectConfigData(data, false);
            }

            // cleanup
            free(input.data);
        }
    }
    return retVal;
}

/*
 * read the user keycode
 */
static keypadUserCode *readUser(xmlNodePtr node, userAuthLevelType level, bool *needsEncryption)
{
    // create the return object so we can fill it in
    //
    keypadUserCode *retVal = create_keypadUserCode();
    retVal->authorityLevel = level;
    *needsEncryption = false;

    // loop through the XML nodes
    //
    xmlNodePtr currentNode;
    xmlNodePtr loopNode = node->children;
    for (currentNode = loopNode; currentNode != NULL; currentNode = currentNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currentNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currentNode->name, UID_NODE) == 0)
        {
            retVal->uuid = getXmlNodeContentsAsUnsignedInt(currentNode, 0);
        }
        else if (strcmp((const char *) currentNode->name, E2_CODE_NODE) == 0)
        {
            free(retVal->code);
            retVal->code = extractAndDecryptString(currentNode);
        }
        else if (strcmp((const char *) currentNode->name, FRIENDLY_NAME_NODE) == 0)
        {
            free(retVal->label);
            retVal->label = getXmlNodeContentsAsString(currentNode, NULL);
        }
        else if (strcmp((const char *) currentNode->name, VALID_SUNDAY_NODE) == 0)
        {
            retVal->validSunday = getXmlNodeContentsAsBoolean(currentNode, false);
        }
        else if (strcmp((const char *) currentNode->name, VALID_MONDAY_NODE) == 0)
        {
            retVal->validMonday = getXmlNodeContentsAsBoolean(currentNode, false);
        }
        else if (strcmp((const char *) currentNode->name, VALID_TUESDAY_NODE) == 0)
        {
            retVal->validTuesday = getXmlNodeContentsAsBoolean(currentNode, false);
        }
        else if (strcmp((const char *) currentNode->name, VALID_WEDNESDAY_NODE) == 0)
        {
            retVal->validWednesday = getXmlNodeContentsAsBoolean(currentNode, false);
        }
        else if (strcmp((const char *) currentNode->name, VALID_THURSDAY_NODE) == 0)
        {
            retVal->validThursday = getXmlNodeContentsAsBoolean(currentNode, false);
        }
        else if (strcmp((const char *) currentNode->name, VALID_FRIDAY_NODE) == 0)
        {
            retVal->validFriday = getXmlNodeContentsAsBoolean(currentNode, false);
        }
        else if (strcmp((const char *) currentNode->name, VALID_SATURDAY_NODE) == 0)
        {
            retVal->validSaturday = getXmlNodeContentsAsBoolean(currentNode, false);
        }
    }

    // make sure this is a valid user before returning
    //
    if (retVal->uuid == 0 || retVal->code == NULL)
    {
        // invalid user
        //
        *needsEncryption = false;
        destroy_keypadUserCode(retVal);
        retVal = NULL;
    }

    // before returning, update our maxUuid value
    //
    else if (retVal->uuid > maxUuid)
    {
        maxUuid = retVal->uuid;
    }

    return retVal;
}

/*
 * populate variables with values read from the XML file
 * internal, so assumes the CFG_MTX is held
 *
 * NOT static so it can be used by the zilkerDecryptor tool
 */
bool readSecurityConfigFile(const char *path)
{
    xmlDocPtr doc;
    xmlNodePtr topNode, loopNode, currentNode;

    // assume our linked list is empty, or wanted to be appended to
    //
    icLogDebug(SECURITY_LOG, "readConfiguration");

    if (!openProtectConfigSession())
    {
        icLogError(SECURITY_LOG, "unable to open a protectConfigSession");
        return false;
    }
    // open/parse the XML file
    //
    doc = xmlParseFile(path);
    if (doc == (xmlDocPtr) 0)
    {
        icLogWarn(SECURITY_LOG, "Unable to parse %s", path);
        return false;
    }

    topNode = xmlDocGetRootElement(doc);
    if (topNode == (xmlNodePtr) 0)
    {
        icLogWarn(SECURITY_LOG, "Unable to find contents of %s", ROOT_NODE);
        xmlFreeDoc(doc);
        return false;
    }

    // setup default values.  NOTE: do NOT include branding or we'll end up in endless recursion
    //
    setupDefaultValues(false);
    bool needSave = false;      // set when we've read un-encrypted data and need to save to perform encryption

    // loop through the children of ROOT
    //
    loopNode = topNode->children;
    for (currentNode = loopNode; currentNode != NULL; currentNode = currentNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currentNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currentNode->name, VERSION_NUMBER_NODE) == 0)
        {
            configVersion = getXmlNodeContentsAsUnsignedLongLong(currentNode, 0);
        }
        else if (strcmp((const char *) currentNode->name, OBFUSCATED_KEY_NODE) == 0)
        {
            pcData *key = parseXmlKey(currentNode);
            if (key != NULL)
            {
                // save as our encrypt/decrypt key
                //
                if (xmlCryptKey != NULL)
                {
                    destroyProtectConfigData(xmlCryptKey, true);
                }
                xmlCryptKey = key;
            }
        }
        else if (strcmp((const char *) currentNode->name, E2_INSTALLER_CODE_NODE) == 0)
        {
            // encrypted string
            free(installerCode);
            installerCode = extractAndDecryptString(currentNode);
        }
        else if (strcmp((const char *) currentNode->name, E2_MASTER_CODE_NODE) == 0)
        {
            // encrypted string
            free(masterCode);
            masterCode = extractAndDecryptString(currentNode);
        }
        else if (strcmp((const char *) currentNode->name, E2_DURESS_CODE_NODE) == 0)
        {
            // encrypted string
            free(duressCode);
            duressCode = extractAndDecryptString(currentNode);
        }
        else if (strcmp((const char *) currentNode->name, ENTRY_DELAY_NODE) == 0)
        {
            entryDelay = (uint16_t)getXmlNodeContentsAsUnsignedInt(currentNode, DEFAULT_ENTRY_DELAY);
        }
        else if (strcmp((const char *) currentNode->name, EXIT_DELAY_NODE) == 0)
        {
            exitDelay = (uint16_t)getXmlNodeContentsAsUnsignedInt(currentNode, DEFAULT_EXIT_DELAY);
        }
        else if (strcmp((const char *) currentNode->name, DIALER_DELAY_NODE) == 0)
        {
            dialerDelay = (uint16_t)getXmlNodeContentsAsUnsignedInt(currentNode, DEFAULT_DIALER_DELAY);
        }
        else if (strcmp((const char *) currentNode->name, ALARM_SOUND_DURATION_NODE) == 0)
        {
            alarmSoundDuration = (uint16_t)getXmlNodeContentsAsUnsignedInt(currentNode, DEFAULT_SOUND_DURATION);
        }
        else if (strcmp((const char *) currentNode->name, SWINGER_SHUTDOWN_ENABLED_NODE) == 0)
        {
            swingerShutdownEnabled = getXmlNodeContentsAsBoolean(currentNode, DEFAULT_SWINGER_FLAG);
        }
        else if (strcmp((const char *) currentNode->name, SWINGER_SHUTDOWN_MAX_TRIPS_NODE) == 0)
        {
            swingerShutdownMaxTrips = (uint8_t)getXmlNodeContentsAsUnsignedInt(currentNode, DEFAULT_SWINGER_MAX);
        }
        else if (strcmp((const char *) currentNode->name, FIRE_ALARM_VERIFICATION_NODE) == 0)
        {
            fireAlarmVerificationEnabled = getXmlNodeContentsAsBoolean(currentNode, DEFAULT_FIRE_FLAG);
        }
        else if (strcmp((const char *) currentNode->name, TEST_ALARM_SEND_CODES_NODE) == 0)
        {
            testAlarmSendCodesEnabled = getXmlNodeContentsAsBoolean(currentNode, DEFAULT_TEST_SEND_FLAG);
        }
        else if (strcmp((const char *) currentNode->name, USER_CODE_NODE) == 0)
        {
            // parse the user, and potentially set our flag to re-save
            //
            bool flag = false;
            keypadUserCode *user = readUser(currentNode, KEYPAD_USER_LEVEL_STANDARD, &flag);
            if (user != NULL)
            {
                linkedListAppend(userCodesList, user);
                if (flag == true)
                {
                    needSave = true;
                }
            }
        }
        else if (strcmp((const char *) currentNode->name, ARM_ONLY_CODE_NODE) == 0)
        {
            bool flag = false;
            keypadUserCode *user = readUser(currentNode, KEYPAD_USER_LEVEL_ARMONLY, &flag);
            if (user != NULL)
            {
                linkedListAppend(userCodesList, user);
                if (flag == true)
                {
                    needSave = true;
                }
            }
        }
        else if (strcmp((const char *) currentNode->name, GUEST_CODE_NODE) == 0)
        {
            bool flag = false;
            keypadUserCode *user = readUser(currentNode, KEYPAD_USER_LEVEL_GUEST, &flag);
            if (user != NULL)
            {
                linkedListAppend(userCodesList, user);
                if (flag == true)
                {
                    needSave = true;
                }
            }
        }

        // do-not-disturb settings
        //
        else if (strcmp((const char *) currentNode->name, DEFER_TROUBLES_SLEEP_HOURS_ENABLED_NODE) == 0)
        {
            deferTroublesEnabled = getXmlNodeContentsAsBoolean(currentNode, getDefaultDeferTroublesFlag());
        }
        else if (strcmp((const char *) currentNode->name, DEFER_TROUBLES_SLEEP_HOURS_DURATION_NODE) == 0)
        {
            deferTroublesDurationHours = (uint8_t)getXmlNodeContentsAsUnsignedInt(currentNode, DEFAULT_DEFER_TROUBLES_DURATION);
        }
        else if (strcmp((const char *) currentNode->name, DEFER_TROUBLES_SLEEP_HOURS_START_HOUR_NODE) == 0)
        {
            deferTroublesStartHour = (uint8_t)getXmlNodeContentsAsUnsignedInt(currentNode, DEFAULT_DEFER_TROUBLES_START_HOUR);
        }
        else if (strcmp((const char *) currentNode->name, DEFER_TROUBLES_SLEEP_HOURS_START_MINUTE_NODE) == 0)
        {
            deferTroublesStartMinute = (uint8_t)getXmlNodeContentsAsUnsignedInt(currentNode, DEFAULT_DEFER_TROUBLES_START_MINUTE);
        }
    }

    // cleanup
    //
    icLogDebug(SECURITY_LOG, "done reading configuration file");
    closeProtectConfigSession();
    xmlFreeDoc(doc);

    // re-save the file if necessary
    //
    if (needSave == true)
    {
        writeSecurityConfigFile(false);
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

/*
 * read an XML node, then decrypt it, returning a strdup of the result
 */
static void encryptAndStoreString(xmlNodePtr parentNode, const char *nodeName, char *value)
{
    if (xmlCryptKey != NULL && value != NULL)
    {
        // encrypt
        //
        pcData in;
        in.data = (unsigned char *)value;
        in.length = (uint32_t)strlen(value);
        pcData *encr = protectConfigData(&in, xmlCryptKey);
        if (encr != NULL)
        {
            // append to the XML node
            appendNewStringNode(parentNode, nodeName, (char *)encr->data);
            destroyProtectConfigData(encr, true);
        }
    }
}

/*
 * write cached variables to our XML file
 * internal, so assumes the CFG_MTX is held
 */
static bool writeSecurityConfigFile(bool sendEvent)
{
    xmlDocPtr doc = NULL;
    xmlNodePtr rootNode, versionNode;
    xmlChar* xmlbuff;
    FILE* outputFile;
    int buffersize;
    char buffer[64];

    if (!openProtectConfigSession())
    {
        icLogError(SECURITY_LOG, "unable to open a protectConfigSession");
        return false;
    }
    // create the XML document structure
    //
    icLogDebug(SECURITY_LOG, "writing config file");
    doc = xmlNewDoc(BAD_CAST "1.0");
    rootNode = xmlNewNode(NULL,BAD_CAST ROOT_NODE);

    // add version
    //
    versionNode = xmlNewNode(NULL, BAD_CAST VERSION_NUMBER_NODE);
    sprintf(buffer, "%" PRIu64, ++configVersion);
    xmlNodeSetContent(versionNode, BAD_CAST buffer);
    xmlAddChild(rootNode, versionNode);

    // if needed, generate an encryption key and save in
    // obfuscated form so we can pull later
    //
    if (xmlCryptKey == NULL)
    {
        xmlCryptKey = generateProtectPassword();
    }
    if (xmlCryptKey != NULL)
    {
        // obfuscate our key.  unfortunately we need to use a hard-coded
        // key.  that is because we cannot wait for a domicileId or something similar
        //
        uint32_t obLen = 0;
        char *ob1 = obfuscate(OBFUSCATE_KEY, (uint32_t)strlen(OBFUSCATE_KEY), (const char *)xmlCryptKey->data, xmlCryptKey->length, &obLen);
        if (ob1 != NULL)
        {
            // base64 encode the key so we can save it in XML
            //
            char *ob2 = icEncodeBase64((uint8_t *)ob1, (uint16_t)obLen);
            if (ob2 != NULL)
            {
                appendNewStringNode(rootNode, OBFUSCATED_KEY_NODE, ob2);
                free(ob2);
            }
            free(ob1);
        }
    }

    encryptAndStoreString(rootNode, E2_INSTALLER_CODE_NODE, installerCode);
    encryptAndStoreString(rootNode, E2_MASTER_CODE_NODE, masterCode);
    encryptAndStoreString(rootNode, E2_DURESS_CODE_NODE, duressCode);
    if (entryDelay > 0)
    {
        sprintf(buffer, "%"PRIu16, entryDelay);
        appendNewStringNode(rootNode, ENTRY_DELAY_NODE, buffer);
    }
    if (exitDelay > 0)
    {
        sprintf(buffer, "%"PRIu16, exitDelay);
        appendNewStringNode(rootNode, EXIT_DELAY_NODE, buffer);
    }
    if (dialerDelay > 0)
    {
        sprintf(buffer, "%"PRIu16, dialerDelay);
        appendNewStringNode(rootNode, DIALER_DELAY_NODE, buffer);
    }
    if (alarmSoundDuration > 0)
    {
        sprintf(buffer, "%"PRIu16, alarmSoundDuration);
        appendNewStringNode(rootNode, ALARM_SOUND_DURATION_NODE, buffer);
    }
    sprintf(buffer, "%s", (swingerShutdownEnabled == true) ? "true" : "false");
    appendNewStringNode(rootNode, SWINGER_SHUTDOWN_ENABLED_NODE, buffer);
    if (swingerShutdownMaxTrips > 0)
    {
        sprintf(buffer, "%"PRIu8, swingerShutdownMaxTrips);
        appendNewStringNode(rootNode, SWINGER_SHUTDOWN_MAX_TRIPS_NODE, buffer);
    }
    sprintf(buffer, "%s", (fireAlarmVerificationEnabled == true) ? "true" : "false");
    appendNewStringNode(rootNode, FIRE_ALARM_VERIFICATION_NODE, buffer);
    sprintf(buffer, "%s", (testAlarmSendCodesEnabled == true) ? "true" : "false");
    appendNewStringNode(rootNode, TEST_ALARM_SEND_CODES_NODE, buffer);

    // user codes
    icLinkedListIterator *loop = linkedListIteratorCreate(userCodesList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // all user codes are formatted the same, just have a different top node
        // to indicate the level
        keypadUserCode *user = (keypadUserCode *)linkedListIteratorGetNext(loop);
        xmlNodePtr userNode = NULL;

        switch (user->authorityLevel)
        {
            case KEYPAD_USER_LEVEL_STANDARD:
                userNode = xmlNewNode(NULL, BAD_CAST USER_CODE_NODE);
                break;

            case KEYPAD_USER_LEVEL_ARMONLY:
                userNode = xmlNewNode(NULL, BAD_CAST ARM_ONLY_CODE_NODE);
                break;

            case KEYPAD_USER_LEVEL_GUEST:
                userNode = xmlNewNode(NULL, BAD_CAST GUEST_CODE_NODE);
                break;

            default:
                break;
        }
        if (userNode != NULL)
        {
            xmlAddChild(rootNode, userNode);
            encryptAndStoreString(userNode, E2_CODE_NODE, user->code);
            sprintf(buffer, "%"PRIu32, user->uuid);
            appendNewStringNode(userNode, UID_NODE, buffer);
            appendNewStringNode(userNode, FRIENDLY_NAME_NODE, user->label);

//            // keep in HH:MM format
//            sprintf(buffer, "%2"PRIu16":%2"PRIu16, user->validStartTimeHour, user->validStartTimeMinute);
//            appendNewStringNode(userNode, VALID_START_TIME_NODE, buffer);
//            sprintf(buffer, "%2"PRIu16":%2"PRIu16, user->validEndTimeHour, user->validEndTimeMinute);
//            appendNewStringNode(userNode, VALID_END_TIME_NODE, buffer);
            appendNewStringNode(userNode, VALID_SUNDAY_NODE, (user->validSunday == true) ? "true" : "false");
            appendNewStringNode(userNode, VALID_MONDAY_NODE, (user->validMonday == true) ? "true" : "false");
            appendNewStringNode(userNode, VALID_TUESDAY_NODE, (user->validTuesday == true) ? "true" : "false");
            appendNewStringNode(userNode, VALID_WEDNESDAY_NODE, (user->validWednesday == true) ? "true" : "false");
            appendNewStringNode(userNode, VALID_THURSDAY_NODE, (user->validThursday == true) ? "true" : "false");
            appendNewStringNode(userNode, VALID_FRIDAY_NODE, (user->validFriday == true) ? "true" : "false");
            appendNewStringNode(userNode, VALID_SATURDAY_NODE, (user->validSaturday == true) ? "true" : "false");
        }
    }
    linkedListIteratorDestroy(loop);

    // add do-not-disturb settings
    //
    sprintf(buffer, "%s", (deferTroublesEnabled == true) ? "true" : "false");
    appendNewStringNode(rootNode, DEFER_TROUBLES_SLEEP_HOURS_ENABLED_NODE, buffer);
    sprintf(buffer, "%"PRIu8, deferTroublesDurationHours);
    appendNewStringNode(rootNode, DEFER_TROUBLES_SLEEP_HOURS_DURATION_NODE, buffer);
    sprintf(buffer, "%"PRIu8, deferTroublesStartHour);
    appendNewStringNode(rootNode, DEFER_TROUBLES_SLEEP_HOURS_START_HOUR_NODE, buffer);
    sprintf(buffer, "%"PRIu8, deferTroublesStartMinute);
    appendNewStringNode(rootNode, DEFER_TROUBLES_SLEEP_HOURS_START_MINUTE_NODE, buffer);

    xmlDocSetRootElement(doc, rootNode);
    xmlDocDumpFormatMemory(doc, &xmlbuff, &buffersize, 1);

    // finally, try to write it out
    //
    closeProtectConfigSession();
    xmlFreeDoc(doc);

    // write to tmp file
    //
    outputFile = fopen(configTmpFilename, "w");

    if (outputFile == (FILE *)0)
    {
        icLogWarn(SECURITY_LOG, "Unable to open '%s' for writing: %s", configTmpFilename, strerror(errno));
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
        createDetachedThread(notifyBackupService, NULL, "secCnfChng");
    }

    return true;
}


