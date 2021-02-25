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
 * touchstone.c
 *
 * integration library to allow basic adjustments to
 * the Touchstone system prior to execution and/or activation.
 * intended to be utilized externally to integrate with control
 * systems (without requiring manual intervention via console).
 * utilizes many of the Touchstone/Zilker libraries, so ensure
 * the "rpath" is setup properly when linking against this library.
 *
 * Author: jelderton - 7/1/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <icSystem/runtimeAttributes.h>
#include <icIpc/ipcSender.h>
#include <propsMgr/paths.h>
#include <commMgr/commService_ipc.h>
#include <icReset/factoryReset.h>
#include <deviceService/deviceService_ipc_codes.h>
#include <watchdog/watchdogService_ipc.h>
#include "touchstone.h"
#include "fileHelper.h"


/*
 * private variables
 */
#define LOG_TAG                     "touchstone"
#define COMM_CONF_RUNTIME_FILE      "communication.conf"
#define COMM_CONF_DEFAULT_FILE      "defaults/communication.conf.default"
#define COMM_MARKER_FILE            "/tmp/server.txt"

// for development/debugging this can be set to IC_LOG_DEBUG.  Otherwise no logging will come from this process.
//
#define OVERALL_LOG_PRIORITY IC_LOG_NONE

// copied from commService userver/configuration.c
#define USERVER_CHANNEL_NAME        "userver channel"

// private function declarations
static bool findClsCommHostConfig(void *searchVal, void *item);
static bool findPrimaryCommHostConfig(void *searchVal, void *item);

/*
 * simplify the check for "is enabled"
 */
static bool isTouchstoneEnabled()
{
    bool retVal = false;
    struct stat fileInfo;

    // look for the existence of our 'enable touchstone' marker file
    // TODO: need better way to do this
    //
    if (stat("/nvram/icontrol/enabletouchstone", &fileInfo) == 0)
    {
        retVal = true;
    }
    return retVal;
}

/*
 * returns if Touchstone is currently activated.
 */
bool touchstoneIsActivated()
{
    // init logging subsystem if necessary
    //
    initIcLogger();

    setIcLogPriorityFilter(OVERALL_LOG_PRIORITY);

    // forward call to commService
    //
    bool retVal = false;
    icLogDebug(LOG_TAG, "checking if system is activated");

    cloudAssociationValue *result = create_cloudAssociationValue();
    IPCCode rc = commService_request_GET_CLOUD_ASSOCIATION_STATE(result);
    if (rc == IPC_SUCCESS && result->cloudAssState == CLOUD_ACCOC_AUTHENTICATED)
    {
        retVal = true;
    }
    destroy_cloudAssociationValue(result);
    return retVal;
}

/*
 * returns if Touchstone is currently running
 */
bool touchstoneIsRunning()
{
    // init logging subsystem if necessary
    //
    initIcLogger();

    // see if our key services are running
    //
    icLogDebug(LOG_TAG, "checking if commService is running");
    if (isServiceAvailable(COMMSERVICE_IPC_PORT_NUM) == true)
    {
        return true;
    }

    /* disabling the check against deviceService...  this call is intended to check
     * for "touchstone" running, and on XB6 deviceService is always running in case
     * there is a battery installed
     *
    icLogDebug(LOG_TAG, "checking if deviceService is running");
    if (isServiceAvailable(DEVICESERVICE_IPC_PORT_NUM) == true)
    {
        return true;
    }
     */

    icLogWarn(LOG_TAG, "commService is down; touchstone is not running");
    return false;
}

/*
 * return the hostname of the server Touchstone would activate
 * and communicate with.
 *
 * @return - hostname; caller must release via free() if not NULL
 */
char *touchstoneGetServerHostname()
{
    char *retVal = NULL;

    // init logging subsystem if necessary
    //
    initIcLogger();

    // if commService is available, ask via IPC
    //
    icLogDebug(LOG_TAG, "asking commService for server hostname");
    commHostConfigList *configList = create_commHostConfigList();
    if (commService_request_GET_HOSTNAME_CONFIG_LIST(configList) == IPC_SUCCESS)
    {
        // got the list, find our server in question
        //
        commHostConfig *primary = linkedListFind(configList->hostConfigList, NULL, findPrimaryCommHostConfig);
        if (primary != NULL && primary->hostname != NULL)
        {
            // steal from the object
            //
            retVal = primary->hostname;
            primary->hostname = NULL;
        }
    }

    if (retVal == NULL)
    {
        // server not running, so need to see if we can extract
        // from one of the configuration files.
        // first  - check $IC_CONF/communication.conf (ran once, file ready once we start again)
        // second - check /tmp/server.txt             (never ran, however could be stagged from previous call to touchstoneSetServerHostname)
        // third  - check $IC_HOME/etc/defaults/communication.conf.default
        //

        // first  - check $IC_CONF/communication.conf
        //
        char *confDir = getDynamicConfigPath();
        if (confDir != NULL)
        {
            // build path to $IC_CONF/communication.conf
            //
            char *path = (char *)malloc(sizeof(char) * (strlen(confDir) + strlen(COMM_CONF_RUNTIME_FILE) + 2));
            sprintf(path, "%s/%s", confDir, COMM_CONF_RUNTIME_FILE);

            // see if hostname can be extracted
            //
            icLogDebug(LOG_TAG, "extracting server hostname from %s", path);
            retVal = extractHostnameFromCommConf(path);

            // cleanup
            //
            free(confDir);
            free(path);
        }

        // second - check /tmp/server.txt
        //
        if (retVal == NULL)
        {
            icLogDebug(LOG_TAG, "extracting server hostname from %s", COMM_MARKER_FILE);
            retVal = extractHostnameFromMarker(COMM_MARKER_FILE);
        }

        // third  - check $IC_HOME/etc/defaults/communication.conf.default
        //
        if (retVal == NULL)
        {
            char *defaultsDir = getStaticConfigPath();
            if (defaultsDir != NULL)
            {
                // build path to $IC_HOME/etc/defaults/communication.conf.default
                //
                char *path = (char *)malloc(sizeof(char) * (strlen(defaultsDir) + strlen(COMM_CONF_DEFAULT_FILE) + 2));
                sprintf(path, "%s/%s", defaultsDir, COMM_CONF_DEFAULT_FILE);

                // see if hostname can be extracted
                //
                icLogDebug(LOG_TAG, "extracting server hostname from %s", path);
                retVal = extractHostnameFromCommConf(path);

                // cleanup
                //
                free(defaultsDir);
                free(path);
            }
        }
    }

    // cleanup and return
    //
    destroy_commHostConfigList(configList);
    return retVal;
}

/*
 * attempts to adjust the hostname of the server Touchstone
 * will activate against.  will be ignored if the system is
 * already activated.
 *
 * @return - true if successful
 */
bool touchstoneSetServerHostname(const char *hostname)
{
    // init logging subsystem if necessary
    //
    initIcLogger();

    // sanity check
    //
    if (hostname == NULL || strlen(hostname) == 0)
    {
        icLogWarn(LOG_TAG, "unable to set server hostname.  input is empty");
        return false;
    }

    // see if activated or not.  regardless we want to apply a hostname change,
    // but may restrict that to "only CLS"
    //
    bool isActivated = touchstoneIsActivated();

    // get the current config from commService
    //
    bool retVal = false;
    icLogDebug(LOG_TAG, "asking commService to set server hostname to %s", hostname);
    commHostConfigList *configList = create_commHostConfigList();
    if (commService_request_GET_HOSTNAME_CONFIG_LIST(configList) == IPC_SUCCESS)
    {
        // need to update CLS and/or primary hostnames.
        // if we are NOT activated, update both CLS and primary
        // if we ARE activated, update only CLS for the next activation attempt (after being reset to factory)
        // first, get the CLS host from the list
        //
        bool doSave = false;
        commHostConfig *clsHost = linkedListFind(configList->hostConfigList, NULL, findClsCommHostConfig);
        if (clsHost != NULL)
        {
            // replace the hostname value
            //
            free(clsHost->hostname);
            clsHost->hostname = strdup(hostname);
            icLogDebug(LOG_TAG, "updating CLS to host=%s", hostname);
            doSave = true;
        }

        // now (optionally) find the primary host
        //
        if (isActivated == false)
        {
            commHostConfig *primary = linkedListFind(configList->hostConfigList, NULL, findPrimaryCommHostConfig);
            if (primary != NULL)
            {
                // replace the hostname value
                //
                free(primary->hostname);
                primary->hostname = strdup(hostname);
                icLogDebug(LOG_TAG, "updating primary to host=%s", hostname);
                doSave = true;
            }
        }
        else
        {
            icLogInfo(LOG_TAG, "NOT updating primary since device is already activated!");
        }

        if (doSave == true)
        {
            // now ask commService to apply the new hostname (give it a 60 second
            // timeout since this could take a few seconds while it restarts activation)
            //
            if (commService_request_SET_HOSTNAME_CONFIG_LIST_timeout(configList, 60) == IPC_SUCCESS)
            {
                retVal = true;
            }
        }
    }
    destroy_commHostConfigList(configList);

    if (retVal == false)
    {
        // unable to get commService to accept the change, or it's not running.
        // therefore, create a /tmp/server.txt file that it should pickup
        // next time it runs
        //
        icLogDebug(LOG_TAG, "saving server hostname %s to %s", hostname, COMM_MARKER_FILE);
        retVal = saveHostnameToMarker(COMM_MARKER_FILE, hostname);
    }

    return retVal;
}

static void restartTouchstoneGroup()
{
    // I hate doing this, but if the 'touchstone enabled' marker exists,
    // then start the 'touchstone' process group.  the resetToFactory()
    // call above will shutdown 'all' processes and won't know to restart
    // the touchstone specific processes.  see filesystem/scripts/start_xb6_fcore.sh
    //
    if (isTouchstoneEnabled() == true)
    {
        // marker is there, start the logical group "touchstone"
        //
        icLogDebug(LOG_TAG, "re-starting touchstone services (after resetToFactory)");
        bool flag;
        watchdogService_request_RESTART_GROUP("touchstone", &flag);
    }
}

/*
 * attempts to reset Touchstone to factory defaults.
 * will not work if Touchstone is currently activated.
 */
bool touchstoneResetToFactory()
{
    // init logging subsystem if necessary
    //
    initIcLogger();

    // call into libicReset to perform the reset to factory.
    // because we're on a Gateway, this will kill the processes
    // but NOT kill watchdog.  therefore, need to startup the core
    // services after this is complete.
    //
    resetToFactory();

    // ask watchdog to start our core services.  we don't have an API
    // for that, but asking it to "bounce all" will launch the ones that
    // are marked for "autostart".
    //
    touchstoneRestart();

    return true;
}

/*
 * restarts all of the Touchstone processes.  note that
 * this will fail if touchstone is not enabled.
 */
bool touchstoneRestart()
{
    bool worked = false;

    if (isTouchstoneEnabled() == true)
    {
        // restart all services
        //
        shutdownOptions *opt = create_shutdownOptions();
        opt->exit = false;
        opt->forReset = false;
        IPCCode ipcRc = watchdogService_request_RESTART_ALL_SERVICES(opt);
        if (ipcRc != IPC_SUCCESS)
        {
            icLogError(LOG_TAG, "Unable to restart all touchstone services : %d - %s", ipcRc, IPCCodeLabels[ipcRc]);
        }
        else
        {
            icLogDebug(LOG_TAG, "Successfully restarted ALL SERVICES via watchdog");
            worked = true;
        }
        destroy_shutdownOptions(opt);

#ifdef CONFIG_PRODUCT_XB6
        // if on XB6, make sure our touchstone group is launched if needed.
        // this handles the cruddy situation where it was down at the time
        // of the 'restart' but should have been up.
        //
        restartTouchstoneGroup();
#endif
    }

    return worked;
}


/*
 * linkedList compare function to locate a particular hostname object
 * (adheres to linkedListCompareFunc signature)
 */
static bool findClsCommHostConfig(void *searchVal, void *item)
{
    // search is null.  looking for a hard-coded host
    //
    commHostConfig *curr = (commHostConfig *)item;

    if ((curr->channelId != NULL && strcmp(curr->channelId, USERVER_CHANNEL_NAME) == 0) &&
        (curr->purpose == CHANNEL_HOST_PURPOSE_CLS) && (curr->primary == true))
    {
        // found our userver primary bband host
        //
        return true;
    }

    return false;
}

/*
 * linkedList compare function to locate a particular hostname object
 * (adheres to linkedListCompareFunc signature)
 */
static bool findPrimaryCommHostConfig(void *searchVal, void *item)
{
    // search is null.  looking for a hard-coded host
    //
    commHostConfig *curr = (commHostConfig *)item;

    if ((curr->channelId != NULL && strcmp(curr->channelId, USERVER_CHANNEL_NAME) == 0) &&
        (curr->purpose == CHANNEL_HOST_PURPOSE_BBAND) && (curr->primary == true))
    {
        // found our userver primary bband host
        //
        return true;
    }

    return false;
}

