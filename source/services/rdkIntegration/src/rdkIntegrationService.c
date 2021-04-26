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
 * Created by Thomas Lea on 2/5/20.
 */

#include <icBuildtime.h>

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef CONFIG_PRODUCT_ANGELSENVY
#include "XHFW_Ssp/ssp_global.h"
#endif

#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <propsMgr/propsHelper.h>
#include <pkiService/pkiService_ipc.h>
#include <commMgr/commService_eventAdapter.h>
#include <icConcurrent/timedWait.h>
#include <propsMgr/commonProperties.h>
#include <watchdog/watchdogService_ipc.h>
#include <icConcurrent/threadUtils.h>
#include <commMgr/commService_ipc.h>
#include <jsonHelper/jsonHelper.h>

#define AWS_ACTIVATION_TIMEOUT_SECS 120

#define AWS_CHANNEL_NAME "aws channel"

#define XBB_SERVICE_GROUP_NAME "xbb"
#define ONLINE_SERVICE_GROUP_NAME "online"

#define STATUS_PROP_NAME "xhfw-status"

#ifdef CONFIG_DEBUG_BREAKPAD
#include <breakpadHelper.h>
#endif

/*
 * External function declarations
 */

/*
 * Private types
 */
typedef enum
{
    ActivationStatusUnknown = 0,
    ActivationStatusNotActivated,
    ActivationStatusActivated,
    ActivationStatusActivationFailed,
    ActivationStatusActivationNotAllowed,
    ActivationStatusInvalidArguments
} ActivationStatusCode;

static const char *ActivationStatusDescriptions[] =
{
        "unknown",
        "not activated",
        "activated",
        "activation failed",
        "activation not allowed",
        "invalid activation arguments"
};

/*
 * Private function declarations
 */
static void printUsage(void);

static bool setActivateCallback(const char *blob);
static char * getStatusCallback(void);
static bool setSATCallback(const char *sat);
static bool resetToFactoryCallback(void);
static bool restartCallback(void);
static bool getAwsIotEnabledCallback(void);
static bool setAwsIotEnabledCallback(bool bEnabled);
static bool getUServerEnabledCallback(void);
static bool setUServerEnabledCallback(bool bEnabled);
static char * getWhitelistURLCallback(void);
static bool setWhitelistURLCallback(const char *url);
static char * getAWSEndpointCallback(void);
static bool setAWSEndpointCallback(const char *endpoint);
static char * getFirmwareDownloadURLCallback(void);
static bool setFirmwareDownloadURLCallback(const char *url);
static char * getxPKICertIssuerCANameCallback(void);
static bool setxPKICertIssuerCANameCallback(const char *name);

static bool isAwsChannelEnabled(void);
static bool isUserverChannelEnabled(void);
static bool startXbbServiceGroup(void);
static bool startOnlineServiceGroup(void);
static bool stopXbbServiceGroup(void);
static bool stopOnlineServiceGroup(void);
static void setStatus(const char *status);
static char *activateAws(const char *sat);

/*
 * Static variables
 */
static pthread_cond_t activateAwsCond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t activateAwsMtx = PTHREAD_MUTEX_INITIALIZER;
static bool awsActivationSuccessful = false;
static bool isRunning = false;

/**
 * Launch the rest of the system/services
 */
static void startServices(void)
{
    //tell watchdog service to start battery support since we always do that for gateways that support them
#ifdef CONFIG_SERVICE_DEVICE_ZIGBEE_XBB
    startXbbServiceGroup();
#endif

    // now check the TR-181 parameters for aws and userver channels.  If either of those are enabled,
    // start up the rest of our services for full online support
    if (isAwsChannelEnabled() == true || isUserverChannelEnabled() == true)
    {
        startOnlineServiceGroup();
    }
}

//TODO Dont duplicate this code here!
#ifndef CONFIG_PRODUCT_ANGELSENVY
#include "ccsp_psm_helper.h"
extern ANSC_HANDLE bus_handle;//lnt
extern char g_Subsystem[32];//lnt
#endif

static bool isAwsChannelEnabled(void)
{
    return getPropertyAsBool(AWS_ENABLED_PROP, false);
}

static bool isUserverChannelEnabled(void)
{
    return getPropertyAsBool(USERVER_ENABLED_PROP, false);
}

static void handleCommOnlineChangedEvent(commOnlineChangedEvent *event)
{
    if (event == NULL ||
        event->channelStatusDetailedList == NULL ||
        event->channelStatusDetailedList->commStatusList == NULL)
    {
        return;
    }

    icLogInfo(LOG_TAG, "%s", __func__);

    sbIcLinkedListIterator *channelStatusIt = linkedListIteratorCreate(event->channelStatusDetailedList->commStatusList);
    while (linkedListIteratorHasNext(channelStatusIt) == true)
    {
        commChannelStatus *status = linkedListIteratorGetNext(channelStatusIt);

        icLogInfo(LOG_TAG, "%s: channel %s is %s", __func__, status->channelId, status->bbandOnline ? "online" : "offline");

        if (stringCompare(AWS_CHANNEL_NAME, status->channelId, false) == 0)
        {
            if (status->bbandOnline == true)
            {
                pthread_mutex_lock(&activateAwsMtx);
                awsActivationSuccessful = true;
                pthread_cond_signal(&activateAwsCond);
                pthread_mutex_unlock(&activateAwsMtx);

                break;
            }
        }
    }
}

int main(int argc, char **argv)
{
    int c = 0;
    char *dataFile = NULL;

#ifdef CONFIG_DEBUG_BREAKPAD
    // Setup breakpad support
    breakpadHelperSetup();
#endif

    isRunning = true;

    initIcLogger();

    icLogInfo(LOG_TAG, "Starting up");

    while ((c = getopt(argc, argv, "m:")) != -1)
    {
        switch (c)
        {
            case 'm':
            {
                dataFile = strdup(optarg);
                setenv("XHFW_DATAMODEL", dataFile, 1);
                break;
            }

            default:
            {
                fprintf(stderr, "Unexpected option '%c'\n", c);
                free(dataFile);
                printUsage();
                closeIcLogger();
                return EXIT_FAILURE;
            }
        }
    }

    //register callbacks
    register_commOnlineChangedEvent_eventListener(handleCommOnlineChangedEvent);

    {
        xhfw_ssp_callbacks_t xhfw_ssp;

        xhfw_ssp.pfnSetActivate         = setActivateCallback;
        xhfw_ssp.pfnGetStatus           = getStatusCallback;
        xhfw_ssp.pfnSetSAT              = setSATCallback;
        xhfw_ssp.pfnResetToFactory      = resetToFactoryCallback;
        xhfw_ssp.pfnRestart             = restartCallback;
        xhfw_ssp.pfnGetAwsIotEnabled    = getAwsIotEnabledCallback;
        xhfw_ssp.pfnSetAwsIotEnabled    = setAwsIotEnabledCallback;
        xhfw_ssp.pfnGetUServerEnabled   = getUServerEnabledCallback;
        xhfw_ssp.pfnSetUServerEnabled   = setUServerEnabledCallback;
        xhfw_ssp.pfnGetWhitelistURL     = getWhitelistURLCallback;
        xhfw_ssp.pfnSetWhitelistURL     = setWhitelistURLCallback;
        xhfw_ssp.pfnGetAWSEndpoint      = getAWSEndpointCallback;
        xhfw_ssp.pfnSetAWSEndpoint      = setAWSEndpointCallback;
        xhfw_ssp.pfnGetFirmwareDLURL    = getFirmwareDownloadURLCallback;
        xhfw_ssp.pfnSetFirmwareDLURL    = setFirmwareDownloadURLCallback;
        xhfw_ssp.pfnGetxPKICertIssuerCAName    = getxPKICertIssuerCANameCallback;
        xhfw_ssp.pfnSetxPKICertIssuerCAName    = setxPKICertIssuerCANameCallback;

#ifndef CONFIG_PRODUCT_ANGELSENVY
        msgBusInit(&xhfw_ssp);
#endif
    }

    startServices();

    while (isRunning == true)
    {
        sleep(30);
    }

    unregister_commOnlineChangedEvent_eventListener(handleCommOnlineChangedEvent);

    closeIcLogger();
    free(dataFile);

#ifdef CONFIG_DEBUG_BREAKPAD
    // Cleanup breakpad support
    breakpadHelperCleanup();
#endif

    return 0;
}

/*
 * XHFW CCSP callback functions
 */


/*
The payload to write to the Activate parameter includes the SAT, trace-id, and partner id.  An example is:

 {
   "sat": "<token string>",
   "trace-id": "<trace id string>",
   "partner": "<partner id string>"
  }

 */
static bool setActivateCallback(const char *blob)
{
    ActivationStatusCode code = ActivationStatusActivationFailed;

    icLogInfo(LOG_TAG, "%s", __func__);

    AUTO_CLEAN(cJSON_Delete__auto) cJSON *partner = NULL;
    AUTO_CLEAN(cJSON_Delete__auto) cJSON *traceId = NULL;
    AUTO_CLEAN(free_generic__auto) char *xboAccountId = NULL;

    AUTO_CLEAN(cJSON_Delete__auto) cJSON *activateObject = cJSON_Parse(blob);
    if (activateObject != NULL)
    {
        cJSON *sat = cJSON_GetObjectItem(activateObject, "sat");
        traceId = cJSON_DetachItemFromObject(activateObject, "trace-id");
        partner = cJSON_DetachItemFromObject(activateObject, "partner");

        if (sat != NULL && traceId != NULL && partner != NULL)
        {
            if (isAwsChannelEnabled() == false)
            {
                icLogError(LOG_TAG, "%s: ignoring activation request -- services are not enabled", __func__);
                code = ActivationStatusActivationNotAllowed;
            }
            else
            {
                xboAccountId = activateAws(sat->valuestring);
                if (xboAccountId != NULL)
                {
                    code = ActivationStatusActivated;
                }
            }
        }
        else
        {
            code = ActivationStatusInvalidArguments;
        }
    }

#ifndef CONFIG_PRODUCT_ANGELSENVY
    Send_Notification_Task(code,
                           ActivationStatusDescriptions[code],
                           partner != NULL ? partner->valuestring : NULL,
                           traceId != NULL ? traceId->valuestring : NULL,
                           xboAccountId);
#endif

    setStatus(ActivationStatusDescriptions[code]);

    return code == ActivationStatusActivated;
}

static char * getStatusCallback(void)
{
    char *result = getPropertyAsString(STATUS_PROP_NAME, ActivationStatusDescriptions[ActivationStatusNotActivated]);

    icLogInfo(LOG_TAG, "getStatus() = %s", result);

    return result;
}


//TODO remove support for the SAT parameter for production.  Leaving in now for backward compatibility during dev
static bool setSATCallback(const char *sat)
{
    ActivationStatusCode code = ActivationStatusActivationFailed;

    icLogInfo(LOG_TAG, "%s", __func__);

    if (isAwsChannelEnabled() == false)
    {
        icLogError(LOG_TAG, "%s: ignoring SAT -- services are not enabled", __func__);
        code = ActivationStatusInvalidArguments;
    }
    else
    {
        char *xboAccountId = activateAws(sat);
        if (xboAccountId != NULL)
        {
            code = ActivationStatusActivated;
            free(xboAccountId); //this activation method does not use this.
        }
    }

#ifndef CONFIG_PRODUCT_ANGELSENVY
    Send_Notification_Task(-1,
                           ActivationStatusDescriptions[code],
                           NULL, NULL, NULL);
#endif

    setStatus(ActivationStatusDescriptions[code]);

    return code == ActivationStatusActivated;
}

static void *shutdownBackgroundTask(void *arg)
{
    bool forReset = *((bool*)arg);
    free(arg);

    //this is hokey, but we are trying to let the response to the TR-181 property set request complete before we
    // terminate this process (indirectly via watchdog).  Nothing really bad happens if we fail to return a
    // response, so this is just a minimum effort attempt.
    sleep(1);

    shutdownOptions *opts = create_shutdownOptions();
    opts->forReset = forReset;
    opts->exit = true; //have watchdog exit when its done so systemd restarts us
    watchdogService_request_SHUTDOWN_AND_RESET_TO_FACTORY(opts);
    destroy_shutdownOptions(opts);

    return NULL;
}

static bool resetToFactoryCallback(void)
{
    icLogInfo(LOG_TAG, "resetToFactory()");

    // do this in the background so we have a chance of returning successful response for the TR-181 property set
    bool *forReset = malloc(sizeof(bool));
    *forReset = true;
    createDetachedThread(shutdownBackgroundTask, forReset, "backgroundReset");

    return true;
}

static bool restartCallback(void)
{
    icLogInfo(LOG_TAG, "restart()");

    // do this in the background so we have a chance of returning successful response for the TR-181 property set
    bool *forReset = malloc(sizeof(bool));
    *forReset = false;
    createDetachedThread(shutdownBackgroundTask, forReset, "backgroundRestart");

    return true;
}

static bool getAwsIotEnabledCallback(void)
{
    icLogInfo(LOG_TAG, "getAwsIotEnabled()");
    return isAwsChannelEnabled();
}

static bool setAwsIotEnabledCallback(bool bEnabled)
{
    int result = false;

    bool currentEnabled = isAwsChannelEnabled();

    icLogInfo(LOG_TAG, "setAwsIotEnabled(%s)", bEnabled ? "true" : "false");
    propSetResult setRc;
    if ((setRc = setPropertyBool(AWS_ENABLED_PROP, bEnabled, true, PROPERTY_SRC_XCONF)) != PROPERTY_SET_OK)
    {
        icLogError(LOG_TAG, "%s: failed to set property: %s", __func__, propSetResultLabels[setRc]);
    }
    else
    {
        if (bEnabled == true)
        {
            if (currentEnabled == false)
            {
                // we need to start things up
                startOnlineServiceGroup();
            }
        }
        else
        {
            if (currentEnabled == true)
            {
                // we need to stop online services
                stopOnlineServiceGroup();
            }
        }

        result = true;
    }

    return result;
}

static bool getUServerEnabledCallback(void)
{
    icLogInfo(LOG_TAG, "getUServerEnabled()");
    return isUserverChannelEnabled();
}

static bool setUServerEnabledCallback(bool bEnabled)
{
    bool result = false;

    bool currentEnabled = isUserverChannelEnabled();

    icLogInfo(LOG_TAG, "setUServerEnabled(%s)", bEnabled ? "true" : "false");
    propSetResult setRc;
    if ((setRc = setPropertyBool(USERVER_ENABLED_PROP, bEnabled, true, PROPERTY_SRC_XCONF)) != PROPERTY_SET_OK)
    {
        icLogError(LOG_TAG, "%s: failed to set property: %s", __func__, propSetResultLabels[setRc]);
    }
    else
    {
        if (bEnabled == true)
        {
            if (currentEnabled == false)
            {
                // we need to start things up
                startOnlineServiceGroup();
            }
        }
        else
        {
            if (currentEnabled == true)
            {
                // we need to stop online services
                stopOnlineServiceGroup();
            }
        }

        result = true;
    }

    return result;
}

static char * getWhitelistURLCallback(void)
{
    return getPropertyAsString(DEVICE_DESCRIPTOR_LIST, NULL);
}

static bool setWhitelistURLCallback(const char *url)
{
    bool result = true;

    propSetResult setRc;
    if ((setRc = setPropertyValue(DEVICE_DESCRIPTOR_LIST, url, true, PROPERTY_SRC_SERVER)) != PROPERTY_SET_OK)
    {
        icLogError(LOG_TAG, "%s: failed to set property: %s", __func__, propSetResultLabels[setRc]);
        result = false;
    }

    return result;
}

static char * getAWSEndpointCallback(void)
{
    return getPropertyAsString(AWS_HOST_PROP, NULL);
}

static bool setAWSEndpointCallback(const char *endpoint)
{
    bool result = true;

    propSetResult setRc;
    if ((setRc = setPropertyValue(AWS_HOST_PROP, endpoint, true, PROPERTY_SRC_SERVER)) != PROPERTY_SET_OK)
    {
        icLogError(LOG_TAG, "%s: failed to set property: %s", __func__, propSetResultLabels[setRc]);
        result = false;
    }

    return result;
}

static char * getFirmwareDownloadURLCallback(void)
{
    return getPropertyAsString(DEVICE_FIRMWARE_URL_NODE, NULL);
}

static bool setFirmwareDownloadURLCallback(const char *url)
{
    bool result = true;

    propSetResult setRc;
    if ((setRc = setPropertyValue(DEVICE_FIRMWARE_URL_NODE, url, true, PROPERTY_SRC_SERVER)) != PROPERTY_SET_OK)
    {
        icLogError(LOG_TAG, "%s: failed to set property: %s", __func__, propSetResultLabels[setRc]);
        result = false;
    }

    return result;
}

static char * getxPKICertIssuerCANameCallback(void)
{
    return getPropertyAsString(PKI_CERT_CA_NAME, NULL);
}

static bool setxPKICertIssuerCANameCallback(const char *name)
{
    bool result = true;

    propSetResult setRc;
    if ((setRc = setPropertyValue(PKI_CERT_CA_NAME, name, true, PROPERTY_SRC_SERVER)) != PROPERTY_SET_OK)
    {
        icLogError(LOG_TAG, "%s: failed to set property: %s", __func__, propSetResultLabels[setRc]);
        result = false;
    }

    return result;
}

static bool startXbbServiceGroup(void)
{
    bool result = true;

    IPCCode ipcCode;
    if ((ipcCode = watchdogService_request_START_GROUP(XBB_SERVICE_GROUP_NAME, &result)) != IPC_SUCCESS ||
        result == false)
    {
        icLogError(LOG_TAG, "%s: failed to start xbb service group: %s", __func__, IPCCodeLabels[ipcCode]);
        result = false;
    }

    return result;
}

static bool startOnlineServiceGroup(void)
{
    bool result = true;

    IPCCode ipcCode;
    if ((ipcCode = watchdogService_request_START_GROUP(ONLINE_SERVICE_GROUP_NAME, &result)) != IPC_SUCCESS ||
        result == false)
    {
        icLogError(LOG_TAG, "%s: failed to start online service group: %s", __func__, IPCCodeLabels[ipcCode]);
        result = false;
    }

    return result;
}

static bool stopXbbServiceGroup(void)
{
    bool result = true;

    IPCCode ipcCode;
    if ((ipcCode = watchdogService_request_STOP_GROUP(XBB_SERVICE_GROUP_NAME, &result)) != IPC_SUCCESS ||
        result == false)
    {
        icLogError(LOG_TAG, "%s: failed to stop xbb service group: %s", __func__, IPCCodeLabels[ipcCode]);
        result = false;
    }

    return result;
}

static bool stopOnlineServiceGroup(void)
{
    bool result = true;

    // check to make sure that both channels are disabled before we stop online services
    // otherwise, we could get a change on one channel and affect both channels by shutting
    // down online services (at least until a reboot)
    //
    if(isAwsChannelEnabled() == false && isUserverChannelEnabled() == false)
    {
        IPCCode ipcCode;
        if ((ipcCode = watchdogService_request_STOP_GROUP(ONLINE_SERVICE_GROUP_NAME, &result)) != IPC_SUCCESS ||
            result == false)
        {
            icLogError(LOG_TAG, "%s: failed to stop online service group: %s", __func__, IPCCodeLabels[ipcCode]);
            result = false;
        }
    }
    else
    {
        result = false;
    }

    return result;
}

static void setStatus(const char *status)
{
    if (setPropertyValue(STATUS_PROP_NAME, status, true, PROPERTY_SRC_SERVER) != PROPERTY_SET_OK)
    {
        icLogError(LOG_TAG, "%s: unable to save status", __func__);
    }
}

static char *activateAws(const char *sat)
{
    char *xboAccountId = NULL;

    //TODO fix this later, but for now just return already activated if we are currently online.  Later
    //  we need to actually use the SAT to get new certificates, etc.
    bool alreadyActivated = false;
    commChannelStatusList *statusList = create_commChannelStatusList();
    IPCCode statRc = commService_request_GET_ONLINE_DETAILED_STATUS(statusList);
    if (statRc == IPC_SUCCESS)
    {
        icLinkedListIterator *it = linkedListIteratorCreate(statusList->commStatusList);
        while (linkedListIteratorHasNext(it))
        {
            commChannelStatus *status = linkedListIteratorGetNext(it);
            if (stringCompare(AWS_CHANNEL_NAME, status->channelId, false) == 0)
            {
                if (status->bbandOnline == true)
                {
                    alreadyActivated = true;

                    // fetch the xbo account id from pki service so we can return it
                    AUTO_CLEAN(pojo_destroy__auto) PKIConfig *pkiConfig = create_PKIConfig();
                    IPCCode rc = pkiService_request_GET_CONFIG(false, pkiConfig);
                    if (rc == IPC_SUCCESS && pkiConfig->accountId != NULL)
                    {
                        xboAccountId = strdup(pkiConfig->accountId);
                    }
                }
                break;
            }
        }
        linkedListIteratorDestroy(it);
    }
    destroy_commChannelStatusList(statusList);

    if (alreadyActivated == false)
    {
        initTimedWaitCond(&activateAwsCond);

        pthread_mutex_lock(&activateAwsMtx);
        awsActivationSuccessful = false;

        IPCCode rc = pkiService_request_SET_SAT((char *) sat, &xboAccountId);
        if (rc == IPC_SUCCESS && xboAccountId != NULL)
        {
            if (incrementalCondTimedWait(&activateAwsCond, &activateAwsMtx, AWS_ACTIVATION_TIMEOUT_SECS) != 0 ||
                awsActivationSuccessful == false)
            {
                //we failed.  Free the xboAccountId and set to NULL to indicate failure
                free(xboAccountId);
                xboAccountId = NULL;
            }
        }

        pthread_mutex_unlock(&activateAwsMtx);
    }

    return xboAccountId;
}

/*
 * show user available options
 */
static void printUsage()
{
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  xhRdkIntegrationService [-m datamodel-xml-file]\n");
#ifndef CONFIG_PRODUCT_ANGELSENVY
    fprintf(stderr, "    -m - set the 'Datamodel XML file'   (default: %s)\n", CCSP_DATAMODEL_XML_FILE);
#endif
    fprintf(stderr, "\n");
}

