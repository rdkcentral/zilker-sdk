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
 * ohcmConfig.c
 *
 * implementation of "config" functionality as defined in ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <string.h>
#include <inttypes.h>

#include <openHomeCamera/ohcm.h>
#include <xmlHelper/xmlHelper.h>
#include <urlHelper/urlHelper.h>
#include <icUtil/stringUtils.h>
#include <icTypes/icStringBuffer.h>
#include <propsMgr/sslVerify.h>
#include "ohcmBase.h"
#include "ohcmPrivate.h"

#define CONFIG_FILE_URI         "/OpenHome/System/ConfigurationData/configFile"
#define CONFIG_TIMERS_URI       "/OpenHome/System/ConfigurationData/Timers"
#define TIMEZONE_SETTINGS_URI   "/OpenHome/System/time/timeZone"
#define SCOMM_CONFIG_URL        "https://%s/adm/set_group.cgi?group=SYSTEM"

#define CONFIG_FILE_TOP_NODE                "ConfigFile"

#define CONF_DEVICE_SECTION_NODE            "DeviceInfo"
#define CONF_TIMERS_SECTION_NODE            "ConfigTimers"
#define CONF_TIME_SECTION_NODE              "Time"
#define CONF_NTP_SECTION_NODE               "NTPServerList"
#define CONF_LOG_SECTION_NODE               "LoggingConfig"
#define CONF_HOST_SECTION_NODE              "HostServer"
#define CONF_HISTORY_SECTION_NODE           "HistoryConfiguration"
#define CONF_NETWORK_SECTION_NODE           "NetworkInterfaceList"
#define CONF_AUDIO_CHANNEL_SECTION_NODE     "AudioChannelList"
#define CONF_VIDEO_INPUT_SECTION_NODE       "VideoInput"
#define CONF_USERS_SECTION_NODE             "UserList"
#define CONF_AUTH_SECTION_NODE              "AuthorizationInfo"
#define CONF_STREAM_CHANNEL_SECTION_NODE    "StreamingChannelList"
#define CONF_MOTION_DETECT_SECTION_NODE     "MotionDetectionList"
#define CONF_SOUND_DETECT_SECTION_NODE      "SoundDetectionList"
#define CONF_EVENT_NOTIF_SECTION_NODE       "EventNotification"

// UserList
#define USER_ACCOUNT_NODE   "Account"
#define USER_ID_NODE        "id"
#define USER_NAME_NODE      "userName"
#define USER_PASSWORD_NODE  "password"
#define USER_RIGHTS_NODE    "accessRights"

// ConfigTimers
#define CONFIG_TIMERS_TUNNEL_NODE           "MediaTunnelReadyTimers"
#define CONFIG_TIMERS_TUNNEL_MAX_READY_NODE "maxMediaTunnelReadyWait"
#define CONFIG_TIMERS_MIN_NODE              "minWait"
#define CONFIG_TIMERS_MAX_NODE              "maxWait"
#define CONFIG_TIMERS_STEPSIZE_NODE         "stepsizeWait"
#define CONFIG_TIMERS_RETRIES_NODE          "retries"
#define CONFIG_TIMERS_UPLOAD_NODE           "MediaUploadTimers"
#define CONFIG_TIMERS_UPLOAD_TIMEOUT_NODE   "UploadTimeout"

// HostServer
#define HOST_SERVER_HTTPS_NODE          "https"
#define HOST_SERVER_HTTP_NODE           "http"
#define HOST_SERVER_POLL_NODE           "poll"
#define HOST_SERVER_ENABLED_NODE        "enabled"
#define HOST_SERVER_PORT_NODE           "port"
#define HOST_SERVER_VALIDATE_CERT_NODE  "validateCerts"
#define HOST_SERVER_DEFAULT_LINGER_NODE "defaultLinger"

/**
 * No security.
 */
#define SCOMM_TLS_VALIDATE_NONE   0U

/**
 * Validate server certificate chains (camera ---> server)
 */
#define SCOMM_TLS_VALIDATE_SERVER 1U << 0U

/**
 * Validate client certificate chains (touchscreen/gateway ---> camera)
 */
#define SCOMM_TLS_VALIDATE_CLIENT 1U << 1U

#define CONFIG_TIMEOUT_S 10

/*
 * helper function to create a blank ohcmSecurityAccount object
 */
ohcmSecurityAccount *createOhcmSecurityAccount()
{
    return (ohcmSecurityAccount *)calloc(1, sizeof(ohcmSecurityAccount));
}

/*
 * helper function to destroy the ohcmSecurityAccount object
 */
void destroyOhcmSecurityAccount(ohcmSecurityAccount *obj)
{
    if (obj != NULL)
    {
        free(obj->id);
        obj->id = NULL;

        free(obj->userName);
        obj->userName = NULL;

        free(obj->password);
        obj->password = NULL;

        free(obj);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmSecurityAccount from a linked list
 */
void destroyOhcmSecurityAccountFromList(void *item)
{
    ohcmSecurityAccount *sec = (ohcmSecurityAccount *)item;
    destroyOhcmSecurityAccount(sec);
}

/*
 * parse an XML node for information about a single user account
 * assumes 'funcArg' is an ohcmSecurityAccount object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
static bool parseAccountXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmSecurityAccount *account = (ohcmSecurityAccount *) funcArg;
    if (node == NULL)
    {
        return false;
    }

    // parse the XML and stuff into our object:
    //
    if (strcmp((const char *) node->name, USER_ID_NODE) == 0)
    {
        account->id = getXmlNodeContentsAsString(node, 0);
    }
    else if (strcmp((const char *) node->name, USER_NAME_NODE) == 0)
    {
        account->userName = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, USER_PASSWORD_NODE) == 0)
    {
        account->password = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, USER_RIGHTS_NODE) == 0)
    {
        char *tmp = getXmlNodeContentsAsString(node, NULL);
        if (tmp != NULL && strcmp(tmp, "admin") == 0)
        {
            account->accessRights = OHCM_ACCESS_ADMIN;
        }
        else
        {
            account->accessRights = OHCM_ACCESS_USER;
        }
        free(tmp);
    }

    return true;
}


/*
 * parse the XML node 'UserList', adding ohcmSecurityAccount objects to the list
 */
static void parseUserListXmlNode(xmlNodePtr node, icLinkedList *targetList)
{
    /*
     * parse XML similar to:
     *  <UserList version="1.0">
     *    <Account version="1.0">
     *      <id>0</id>
     *      <userName>administrator</userName>
     *      <password></password>
     *      <accessRights>admin</accessRights>
     *    </Account>
     *  </UserList>
     */
    xmlNodePtr currNode = NULL;
    xmlNodePtr loopNode = node->children;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, USER_ACCOUNT_NODE) == 0)
        {
            // create a new ohcmSecurityAccount object, parse it, then add to the list
            //
            ohcmSecurityAccount *acct = createOhcmSecurityAccount();
            ohcmParseXmlNodeChildren(currNode, parseAccountXmlNode, acct);
            linkedListAppend(targetList, acct);
        }
    }
}

/*
 * create an XML node with the ohcmSecurityAccount objects in the list
 */
static void appendUserListXml(xmlNodePtr rootNode, icLinkedList *accountList)
{
    icLinkedListIterator *loop = linkedListIteratorCreate(accountList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        ohcmSecurityAccount *account = (ohcmSecurityAccount *)linkedListIteratorGetNext(loop);
        if (account->id != NULL && account->userName != NULL)
        {
            // make the node for this account
            //
            xmlNodePtr node = xmlNewNode(NULL, BAD_CAST USER_ACCOUNT_NODE);
            xmlNewProp(node, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
            xmlAddChild(rootNode, node);

            xmlNewTextChild(node, NULL, BAD_CAST USER_ID_NODE, BAD_CAST account->id);
            xmlNewTextChild(node, NULL, BAD_CAST USER_NAME_NODE, BAD_CAST account->userName);
            if (account->password != NULL)
            {
                xmlNewTextChild(node, NULL, BAD_CAST USER_PASSWORD_NODE, BAD_CAST account->password);
            }
            switch(account->accessRights)
            {
                case OHCM_ACCESS_ADMIN:
                    xmlNewTextChild(node, NULL, BAD_CAST USER_RIGHTS_NODE, BAD_CAST "admin");
                    break;

                case OHCM_ACCESS_USER:
                default:
                    xmlNewTextChild(node, NULL, BAD_CAST USER_RIGHTS_NODE, BAD_CAST "user");
                    break;
            }
        }
    }
    linkedListIteratorDestroy(loop);
}

/*
 * parse the XML node 'HostServer' and populate the ohcmHostServer object.
 * assumes 'funcArg' is a ohcmHostServer.
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
static bool parseHostServerXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmHostServer *server = (ohcmHostServer *)funcArg;

    /*
     * parse XML similar to:
     * <HostServer version="1.0">
     *   <https>
     *       <enabled>true</enabled>
     *       <port>443</port>
     *       <validateCerts>true</validateCerts>
     *   </https>
     *   <http>
     *       <enabled>true</enabled>
     *       <port>80</port>
     *   </http>
     *   <poll>
     *       <enabled>true</enabled>
     *       <defaultLinger>10</defaultLinger>
     *   </poll>
     * </HostServer>
     */
    if (strcmp((const char *) node->name, HOST_SERVER_HTTPS_NODE) == 0)
    {
        xmlNodePtr currNode = NULL;
        xmlNodePtr loopNode = node->children;
        for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
        {
            // skip comments, blanks, etc
            //
            if (currNode->type != XML_ELEMENT_NODE)
            {
                continue;
            }
            else if (strcmp((const char *) currNode->name, HOST_SERVER_ENABLED_NODE) == 0)
            {
                server->httpsEnabled = getXmlNodeContentsAsBoolean(currNode, false);
            }
            else if (strcmp((const char *) currNode->name, HOST_SERVER_PORT_NODE) == 0)
            {
                server->httpsPort = getXmlNodeContentsAsUnsignedInt(currNode, 0);
            }
            else if (strcmp((const char *) currNode->name, HOST_SERVER_VALIDATE_CERT_NODE) == 0)
            {
                server->httpsValidateCerts = getXmlNodeContentsAsBoolean(currNode, false);
            }
        }
    }
    else if (strcmp((const char *) node->name, HOST_SERVER_HTTP_NODE) == 0)
    {
        xmlNodePtr currNode = NULL;
        xmlNodePtr loopNode = node->children;
        for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
        {
            // skip comments, blanks, etc
            //
            if (currNode->type != XML_ELEMENT_NODE)
            {
                continue;
            }
            else if (strcmp((const char *) currNode->name, HOST_SERVER_ENABLED_NODE) == 0)
            {
                server->httpEnabled = getXmlNodeContentsAsBoolean(currNode, false);
            }
            else if (strcmp((const char *) currNode->name, HOST_SERVER_PORT_NODE) == 0)
            {
                server->httpPort = getXmlNodeContentsAsUnsignedInt(currNode, 0);
            }
        }
    }
    else if (strcmp((const char *) node->name, HOST_SERVER_POLL_NODE) == 0)
    {
        xmlNodePtr currNode = NULL;
        xmlNodePtr loopNode = node->children;
        for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
        {
            // skip comments, blanks, etc
            //
            if (currNode->type != XML_ELEMENT_NODE)
            {
                continue;
            }
            else if (strcmp((const char *) currNode->name, HOST_SERVER_ENABLED_NODE) == 0)
            {
                server->pollEnabled = getXmlNodeContentsAsBoolean(currNode, false);
            }
            else if (strcmp((const char *) currNode->name, HOST_SERVER_DEFAULT_LINGER_NODE) == 0)
            {
                server->pollDefaultLinger = getXmlNodeContentsAsUnsignedInt(currNode, 0);
            }
        }
    }

    return true;
}

/*
 * create an XML node with the contents of a ohcmHostServer object
 */
static void appendHostServerXml(xmlNodePtr rootNode, ohcmHostServer *server)
{
    char buff[128];

    // make "https" node
    //
    xmlNodePtr httpsNode = xmlNewNode(NULL, BAD_CAST HOST_SERVER_HTTPS_NODE);
    xmlAddChild(rootNode, httpsNode);
    xmlNewTextChild(httpsNode, NULL, BAD_CAST HOST_SERVER_ENABLED_NODE, BAD_CAST ((server->httpsEnabled == true) ? "true" : "false"));
    sprintf(buff, "%"PRIu32, server->httpsPort);
    xmlNewTextChild(httpsNode, NULL, BAD_CAST HOST_SERVER_PORT_NODE, BAD_CAST buff);
    xmlNewTextChild(httpsNode, NULL, BAD_CAST HOST_SERVER_VALIDATE_CERT_NODE, BAD_CAST ((server->httpsValidateCerts == true) ? "true" : "false"));

    // make "http" node
    //
    xmlNodePtr httpNode = xmlNewNode(NULL, BAD_CAST HOST_SERVER_HTTP_NODE);
    xmlAddChild(rootNode, httpNode);
    xmlNewTextChild(httpNode, NULL, BAD_CAST HOST_SERVER_ENABLED_NODE, BAD_CAST ((server->httpEnabled == true) ? "true" : "false"));
    sprintf(buff, "%"PRIu32, server->httpPort);
    xmlNewTextChild(httpNode, NULL, BAD_CAST HOST_SERVER_PORT_NODE, BAD_CAST buff);

    // make "poll" node
    //
    xmlNodePtr pollNode = xmlNewNode(NULL, BAD_CAST HOST_SERVER_POLL_NODE);
    xmlAddChild(rootNode, pollNode);
    xmlNewTextChild(pollNode, NULL, BAD_CAST HOST_SERVER_ENABLED_NODE, BAD_CAST ((server->pollEnabled == true) ? "true" : "false"));
    sprintf(buff, "%"PRIu32, server->pollDefaultLinger);
    xmlNewTextChild(pollNode, NULL, BAD_CAST HOST_SERVER_DEFAULT_LINGER_NODE, BAD_CAST buff);
}

/*
 * helper function to create a blank ohcmTimeConfig object
 */
ohcmTimeConfig *createOhcmTimeConfig()
{
    return (ohcmTimeConfig *)calloc(1, sizeof(ohcmTimeConfig));
}

/*
 * helper function to destroy the ohcmTimeConfig object
 */
void destroyOhcmTimeConfig(ohcmTimeConfig *obj)
{
    if (obj != NULL)
    {
        free(obj->timeMode);
        obj->timeMode = NULL;

        free(obj->timeZone);
        obj->timeZone = NULL;

        free(obj);
    }
}

ohcmResultCode getOhcmTimeZoneInfo(ohcmCameraInfo *cam, char *tz, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, TIMEZONE_SETTINGS_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, TIMEZONE_SETTINGS_URI);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(128);

    // create our CURL context
    //
    CURL *curl = createOhcmCurlContext();
    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
    }

    // perform the 'get' operation
    //
    CURLcode rc = ohcmPerformCurlGet(curl, debugUrl, chunk, retryCounts);
    if (rc == CURLE_OK)
    {
        // success with the 'get', so transfer the result from chunk to 'timezone' string
        // NOTE: this is not in XML format
        //
        uint32_t len = fifoBuffGetPullAvailable(chunk);
        fifoBuffPull(chunk, tz, len);
        tz[len] = '\0';
    }

    // cleanup
    //
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

ohcmResultCode ohcmConfigSetMutualTLS(ohcmCameraInfo *camInfo, const char *allowedSubjects[])
{
    /*
     * # enable "mutual tls" on the camera
     * curl -v -u "adminUser:adminPasswd" 'http://172.16.12.9/adm/set_group.cgi?group=SYSTEM&ssl_cert_validation=2'
     *
     * ssl_cert_validation: Validate which peers:
     * 0 : none
     * 1 : server (camera--->server)
     * 2 : client (client--->camera)
     * 3 : both
     *
     * # set the acceptable CN for the certificate
     * curl -v -u "adminUser:adminPasswd" 'http://172.16.12.9/adm/set_group.cgi?group=SYSTEM&ssl_cert_server_cn_list=*.xcal.tv;*.xfinityhome.com'
     */
    ohcmResultCode rc = OHCM_GENERAL_FAIL;
#ifndef CONFIG_PLATFORM_RDK
    if (ohcmIsMtlsCapable() == true)
    {
        icLogInfo(OHCM_LOG, "mTLS capability present; attempting to enable mTLS on camera %s", camInfo->macAddress);
    }
    else
    {
        return OHCM_NOT_SUPPORTED;
    }

    uint8_t validationMode = SCOMM_TLS_VALIDATE_SERVER;
    sslVerify tlsToDevice = ohcmGetTLSVerify();
    long httpCode = 0;

    if (allowedSubjects != NULL)
    {
        AUTO_CLEAN(stringBufferDestroy__auto) icStringBuffer *tmp = stringBufferCreate(32);
        bool first = true;
        for (; *allowedSubjects != NULL; allowedSubjects++)
        {
            if (first == false)
            {
                stringBufferAppend(tmp, ";");
            }
            else
            {
                first = false;
            }
            stringBufferAppend(tmp, *allowedSubjects);
        }

        if (stringBufferLength(tmp) > 0)
        {
            AUTO_CLEAN(free_generic__auto) char *subjectsParam = stringBufferToString(tmp);
            icLogInfo(OHCM_LOG, "%s: Setting allowed subject CNs: '%s' ", __func__, subjectsParam);
            AUTO_CLEAN(free_generic__auto) char *setSubjectsUrl = stringBuilder(SCOMM_CONFIG_URL"&ssl_cert_server_cn_list=%s",
                                                                                camInfo->cameraIP,
                                                                                subjectsParam);

            AUTO_CLEAN(free_generic__auto) char *tmpResponse = urlHelperExecuteRequest(setSubjectsUrl,
                                                                                       &httpCode,
                                                                                       NULL,
                                                                                       camInfo->userName,
                                                                                       camInfo->password,
                                                                                       CONFIG_TIMEOUT_S,
                                                                                       tlsToDevice,
                                                                                       false);
            /* Response doesn't matter - use result code */
            (void) tmpResponse;

            rc = ohcmTranslateCurlCode(ohcmTranslateHttpCode(httpCode));
            if (rc == OHCM_SUCCESS)
            {
                /*
                 * Find out what the "server validation" TLS property is to find out if we should tell the camera
                 * to set its own "verify server" bit to match. There is no way to control how it does this,
                 * only if it does so or not.
                 */
                sslVerify verifyServers = getSslVerifyProperty(SSL_VERIFY_HTTP_FOR_SERVER);
                validationMode = SCOMM_TLS_VALIDATE_CLIENT;
                if (verifyServers == SSL_VERIFY_PEER || verifyServers == SSL_VERIFY_BOTH)
                {
                    icLogInfo(OHCM_LOG, "Enabling TLS server validation on camera %s", camInfo->macAddress);
                    validationMode |= SCOMM_TLS_VALIDATE_SERVER;
                }
            }
            else if (httpCode == 404)
            {
                return OHCM_NOT_SUPPORTED;
            }
            else
            {
                icLogError(OHCM_LOG, "%s: failed to configure camera for mTLS: httpCode [%ld]", __func__, httpCode);
                return OHCM_GENERAL_FAIL;
            }
        }
    }

    httpCode = 0;
    icLogInfo(OHCM_LOG, "%s: Setting TLS validation mode to %u", __func__, validationMode);
    AUTO_CLEAN(free_generic__auto) char *setTLSUrl = stringBuilder(SCOMM_CONFIG_URL"&ssl_cert_validation=%u",
                                                                   camInfo->cameraIP,
                                                                   validationMode);

    AUTO_CLEAN(free_generic__auto) char *tmp = urlHelperExecuteRequest(setTLSUrl,
                                                                       &httpCode,
                                                                       NULL,
                                                                       camInfo->userName,
                                                                       camInfo->password,
                                                                       CONFIG_TIMEOUT_S,
                                                                       tlsToDevice,
                                                                       false);

    /* Response doesn't matter - look at result code */
    (void) tmp;

    rc = ohcmTranslateCurlCode(ohcmTranslateHttpCode(httpCode));
#else
    /* On XBs, this is supposedly set up externally. Pretend everything is OK */
    rc = OHCM_SUCCESS;
#endif

    return rc;
}

/*
 * requests the camera set timezone
 *
 * @param cam - device to contact
 * @param timezone - timezone to use
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmTimeZoneInfo(ohcmCameraInfo *cam, const char *tz, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, TIMEZONE_SETTINGS_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, TIMEZONE_SETTINGS_URI);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(128);

    // create the payload, note that this is NOT XML
    //
    size_t len = strlen(tz);
    icFifoBuff *payload = fifoBuffCreate(1024);
    fifoBuffPush(payload, (char *) tz, (uint32_t)len);

    // create our CURL context
    //
    CURL *curl = createOhcmCurlContext();
    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(curl, CURLOPT_POST, 1) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POST, 1) failed at %s(%d)",__FILE__,__LINE__);
    }

    // add HTTP headers
    //
    struct curl_slist *header = NULL;
    header = curl_slist_append(header, OHCM_CONTENT_TYPE_HEADER);
    header = curl_slist_append(header, OHCM_CONN_CLOSE_HEADER);
    header = curl_slist_append(header, OHCM_SERVER_HEADER);
    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header) failed at %s(%d)",__FILE__,__LINE__);
    }

    // perform the 'post' operation.
    //
    CURLcode rc = ohcmPerformCurlPost(curl, debugUrl, payload, chunk, retryCounts);
    if (rc == CURLE_OK)
    {
        // success with the 'post', so parse the result
        //
        ohcmBasicResponse result;
        memset(&result, 0, sizeof(ohcmBasicResponse));
        if (ohcmParseBasicResponse(chunk, &result) == false)
        {
            // error parsing, force a failure
            //
            icLogWarn(OHCM_LOG, "error parsing results of %s", debugUrl);
            rc = CURLE_CONV_FAILED;
        }
        else
        {
            // look at the result code to see if it was successful.
            //
            rc = ohcmTranslateOhcmResponseCodeToCurl(result.statusCode);
            if (rc != CURLE_OK && result.statusMessage != NULL)
            {
                icLogWarn(OHCM_LOG, "result of %s contained error: %s - %s",
                          debugUrl, ohcmResponseCodeLabels[result.statusCode], result.statusMessage);
            }
        }

        free(result.statusMessage);
    }

    // cleanup
    //
    curl_slist_free_all(header);
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);
    fifoBuffDestroy(payload);   // also free's xmlBuff

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

/*
 * helper function to create a blank ohcmConfigFile object
 */
ohcmConfigFile *createOhcmConfigFile()
{
    // allocate the object, then all of the linked lists
    //
    ohcmConfigFile *retVal = (ohcmConfigFile *)calloc(1, sizeof(ohcmConfigFile));
    retVal->streamChannelsList = linkedListCreate();
    retVal->securityAccountList = linkedListCreate();
    retVal->audioChannelList = linkedListCreate();
    retVal->videoInputList = linkedListCreate();
    retVal->configTimerList = linkedListCreate();
    retVal->networkInterfaceList = linkedListCreate();
    retVal->motionDetectionList = linkedListCreate();

    return retVal;
}

/*
 * helper function to destroy the ohcmConfigFile object
 */
void destroyOhcmConfigFile(ohcmConfigFile *obj)
{
    if (obj != NULL)
    {
        destroyOhcmDeviceInfo(obj->device);
        obj->device = NULL;

        linkedListDestroy(obj->streamChannelsList, destroyOhcmStreamChannelFromList);
        obj->streamChannelsList = NULL;
        linkedListDestroy(obj->securityAccountList, destroyOhcmSecurityAccountFromList);
        obj->securityAccountList = NULL;
        linkedListDestroy(obj->audioChannelList, destroyOhcmAudioChannelFromList);
        obj->audioChannelList = NULL;
        linkedListDestroy(obj->videoInputList, destroyOhcmVideoInputFromList);
        obj->videoInputList = NULL;
        linkedListDestroy(obj->configTimerList, NULL);  // ohcmConfigTimers is just a struct of numbers
        obj->configTimerList = NULL;
        linkedListDestroy(obj->networkInterfaceList, destroyOhcmNetworkInterfaceFromList);
        obj->networkInterfaceList = NULL;
        linkedListDestroy(obj->motionDetectionList, destroyOhcmMotionDetectionFromList);
        obj->motionDetectionList = NULL;
        destroyOhcmTimeConfig(obj->time);
        obj->time = NULL;

        // TODO: free loggingConfig
        // TODO: free ntpServerPtr
        // TODO: free historyConfig
        // TODO: free networkInterfaceList

        free(obj);
    }
}

/*
 * parse the 'ConfigTimers' node from the massive config file.
 * adheres to the 'ohcmParseXmlHelper' signature so this can
 * operate via ohcmParseXmlNodeChildren
 */
static bool parseConfigTimersXmlNode(const xmlChar *topName, xmlNodePtr node, void *arg)
{
    ohcmConfigTimers *timers = (ohcmConfigTimers *)arg;

    /*
     * parse XML similar to:
     *  <ConfigTimers version="1.0">
     *     <MediaTunnelReadyTimers>
     *         <maxMediaTunnelReadyWait>60000</maxMediaTunnelReadyWait>
     *         <minWait>0</minWait>
     *         <maxWait>5000</maxWait>
     *         <stepsizeWait>500</stepsizeWait>
     *         <retries>10</retries>
     *     </MediaTunnelReadyTimers>
     *     <MediaUploadTimers>
     *         <minWait>1000</minWait>
     *         <maxWait>5000</maxWait>
     *         <stepsizeWait>500</stepsizeWait>
     *         <retries>5</retries>
     *         <UploadTimeout>1800000</UploadTimeout>
     *     </MediaUploadTimers>
     *  </ConfigTimers>
     */

    // our 'node' should be "MediaTunnelReadyTimers" or "MediaUploadTimers"
    //
    bool forReady = false;
    if (strcmp((const char *) node->name, CONFIG_TIMERS_TUNNEL_NODE) == 0)
    {
        forReady = true;
    }
    else if (strcmp((const char *) node->name, CONFIG_TIMERS_UPLOAD_NODE) == 0)
    {
        forReady = false;
    }
    else
    {
        // unexpected node
        //
        return true;
    }

    // loop through the children of this node, saving data were appropriate
    //
    xmlNodePtr currNode = NULL;
    xmlNodePtr loopNode = node->children;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, CONFIG_TIMERS_TUNNEL_MAX_READY_NODE) == 0)
        {
            timers->maxMediaTunnelReadyWait = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, CONFIG_TIMERS_UPLOAD_TIMEOUT_NODE) == 0)
        {
            timers->mediaUploadTimersUploadTimeout = getXmlNodeContentsAsUnsignedLongLong(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, CONFIG_TIMERS_MIN_NODE) == 0)
        {
            uint32_t val = getXmlNodeContentsAsUnsignedInt(currNode, 0);
            if (forReady == true)
            {
                timers->mediaTunnelReadyTimersMinWait = val;
            }
            else
            {
                timers->mediaUploadTimersMinWait = val;
            }
        }
        else if (strcmp((const char *) currNode->name, CONFIG_TIMERS_MAX_NODE) == 0)
        {
            uint32_t val = getXmlNodeContentsAsUnsignedInt(currNode, 0);
            if (forReady == true)
            {
                timers->mediaTunnelReadyTimersMaxWait = val;
            }
            else
            {
                timers->mediaUploadTimersMaxWait = val;
            }
        }
        else if (strcmp((const char *) currNode->name, CONFIG_TIMERS_STEPSIZE_NODE) == 0)
        {
            uint32_t val = getXmlNodeContentsAsUnsignedInt(currNode, 0);
            if (forReady == true)
            {
                timers->mediaTunnelReadyTimersStepsizeWait = val;
            }
            else
            {
                timers->mediaUploadTimersStepsizeWait = val;
            }
        }
        else if (strcmp((const char *) currNode->name, CONFIG_TIMERS_RETRIES_NODE) == 0)
        {
            uint32_t val = getXmlNodeContentsAsUnsignedInt(currNode, 0);
            if (forReady == true)
            {
                timers->mediaTunnelReadyTimersRetries = val;
            }
            else
            {
                timers->mediaUploadTimersRetries = val;
            }
        }
    }

    return true;
}

/*
 * generates XML for the 'config timers', adding as a child of 'rootNode'
 */
static void appendConfigTimersXml(xmlNodePtr rootNode, ohcmConfigTimers *timers)
{
    char tmp[256];

    /*
     * first the "MediaTunnelReadyTimers" node
     *    <MediaTunnelReadyTimers>
     *        <maxMediaTunnelReadyWait>60000</maxMediaTunnelReadyWait>
     *        <minWait>0</minWait>
     *        <maxWait>5000</maxWait>
     *        <stepsizeWait>500</stepsizeWait>
     *        <retries>10</retries>
     *    </MediaTunnelReadyTimers>
     */
    xmlNodePtr mediaNode = xmlNewNode(NULL, BAD_CAST CONFIG_TIMERS_TUNNEL_NODE);
    xmlAddChild(rootNode, mediaNode);
    sprintf(tmp, "%"PRIu32, timers->maxMediaTunnelReadyWait);
    xmlNewTextChild(mediaNode, NULL, BAD_CAST CONFIG_TIMERS_TUNNEL_MAX_READY_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, timers->mediaTunnelReadyTimersMinWait);
    xmlNewTextChild(mediaNode, NULL, BAD_CAST CONFIG_TIMERS_MIN_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, timers->mediaTunnelReadyTimersMaxWait);
    xmlNewTextChild(mediaNode, NULL, BAD_CAST CONFIG_TIMERS_MAX_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, timers->mediaTunnelReadyTimersStepsizeWait);
    xmlNewTextChild(mediaNode, NULL, BAD_CAST CONFIG_TIMERS_STEPSIZE_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, timers->mediaTunnelReadyTimersRetries);
    xmlNewTextChild(mediaNode, NULL, BAD_CAST CONFIG_TIMERS_RETRIES_NODE, BAD_CAST tmp);

    /*
     * now the "MediaUploadTimers" node
     *     <MediaUploadTimers>
     *         <minWait>1000</minWait>
     *         <maxWait>5000</maxWait>
     *         <stepsizeWait>500</stepsizeWait>
     *         <retries>5</retries>
     *         <UploadTimeout>1800000</UploadTimeout>
     *     </MediaUploadTimers>
     */
    xmlNodePtr uploadNode = xmlNewNode(NULL, BAD_CAST CONFIG_TIMERS_UPLOAD_NODE);
    xmlAddChild(rootNode, uploadNode);
    sprintf(tmp, "%"PRIu32, timers->mediaUploadTimersMinWait);
    xmlNewTextChild(uploadNode, NULL, BAD_CAST CONFIG_TIMERS_MIN_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, timers->mediaUploadTimersMaxWait);
    xmlNewTextChild(uploadNode, NULL, BAD_CAST CONFIG_TIMERS_MAX_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, timers->mediaUploadTimersStepsizeWait);
    xmlNewTextChild(uploadNode, NULL, BAD_CAST CONFIG_TIMERS_STEPSIZE_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, timers->mediaUploadTimersRetries);
    xmlNewTextChild(uploadNode, NULL, BAD_CAST CONFIG_TIMERS_RETRIES_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu64, timers->mediaUploadTimersUploadTimeout);
    xmlNewTextChild(uploadNode, NULL, BAD_CAST CONFIG_TIMERS_UPLOAD_TIMEOUT_NODE, BAD_CAST tmp);
}

/*
 * parse an XML node from the 'massive config' doc.
 * assumes 'funcArg' is an ohcmConfigFile object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
static bool parseMassiveConfigFileXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmConfigFile *conf = (ohcmConfigFile *) funcArg;

    // NOTE: see 'sampleMassiveConfig.xml' for an example of what this looks like.
    // the XML document is broken in to multiple sections, which we'll leverage
    // other XML parsing functions spread across other source files:
    //   DeviceInfo
    //   ConfigTimers
    //   Time
    //   NTPServerList
    //   LoggingConfig
    //   HostServer
    //   HistoryConfiguration
    //   NetworkInterfaceList
    //   AudioChannelList
    //   VideoInput
    //   UserList
    //   AuthorizationInfo
    //   StreamingChannelList
    //   MotionDetectionList
    //   SoundDetectionList
    //   EventNotification
    //
    if (strcmp((const char *) node->name, CONF_DEVICE_SECTION_NODE) == 0)
    {
        // parse the section, placing the info into conf->device
        //
        conf->device = createOhcmDeviceInfo();
        ohcmParseXmlNodeChildren(node, parseOhcmDeviceXmlNode, conf->device);
    }
    else if (strcmp((const char *) node->name, CONF_TIMERS_SECTION_NODE) == 0)
    {
        // parse the section, saving into conf->timers
        //
        ohcmParseXmlNodeChildren(node, parseConfigTimersXmlNode, &conf->timers);
    }
    else if (strcmp((const char *) node->name, CONF_TIME_SECTION_NODE) == 0)
    {
        // TODO: parse "Time"
    }
    else if (strcmp((const char *) node->name, CONF_NTP_SECTION_NODE) == 0)
    {
        // TODO: parse "NTPServerList"
    }
    else if (strcmp((const char *) node->name, CONF_LOG_SECTION_NODE) == 0)
    {
        // TODO: parse "LoggingConfig"
    }
    else if (strcmp((const char *) node->name, CONF_HOST_SECTION_NODE) == 0)
    {
        // parse the "host server" section
        //
        ohcmParseXmlNodeChildren(node, parseHostServerXmlNode, &conf->hostServer);
    }
    else if (strcmp((const char *) node->name, CONF_HISTORY_SECTION_NODE) == 0)
    {
        // TODO: parse "HistoryConfiguration"
    }
    else if (strcmp((const char *) node->name, CONF_NETWORK_SECTION_NODE) == 0)
    {
        // parse the section, adding ohcmNetworkInterface objects to the conf->networkInterfaceList
        //
        parseOhcmNetworkListXmlNode(node->name, node, conf->networkInterfaceList);
    }
    else if (strcmp((const char *) node->name, CONF_AUDIO_CHANNEL_SECTION_NODE) == 0)
    {
        // parse the section, adding ohcmAudioChannel objects to the conf->audioChannelList
        //
        parseOhcmAudioListXmlNode(node->name, node, conf->audioChannelList);
    }
    else if (strcmp((const char *) node->name, CONF_VIDEO_INPUT_SECTION_NODE) == 0)
    {
        // parse the section, adding ohcmVideoInput objects to the conf->videoInputList
        //
        ohcmParseXmlNodeChildren(node, parseOhcmVideoInputChannelListXmlNode, conf->videoInputList);
    }
    else if (strcmp((const char *) node->name, CONF_USERS_SECTION_NODE) == 0)
    {
        // parse the section, adding ohcmSecurityAccount objects to the conf->securityAccountList
        //
        parseUserListXmlNode(node, conf->securityAccountList);
    }
    else if (strcmp((const char *) node->name, CONF_AUTH_SECTION_NODE) == 0)
    {
        // TODO: parse "AuthorizationInfo"
    }
    else if (strcmp((const char *) node->name, CONF_STREAM_CHANNEL_SECTION_NODE) == 0)
    {
        // parse the section, adding ohcmStreamChannel objects to the conf->streamChannelsList
        //
        ohcmParseXmlNodeChildren(node, parseOhcmStreamChannelListXmlNode, conf->streamChannelsList);
    }
    else if (strcmp((const char *) node->name, CONF_MOTION_DETECT_SECTION_NODE) == 0)
    {
        // parse the section, adding ohcmMotionDetection objects to the conf->motionDetectionList
        //
        ohcmParseXmlNodeChildren(node, parseOhcmMotionDetectionListXmlNode, conf->motionDetectionList);
    }
    else if (strcmp((const char *) node->name, CONF_SOUND_DETECT_SECTION_NODE) == 0)
    {
        // TODO: parse "SoundDetectionList"
    }
    else if (strcmp((const char *) node->name, CONF_EVENT_NOTIF_SECTION_NODE) == 0)
    {
        // TODO: parse "EventNotification"
    }

    return true;
}

/*
 * query the camera for the 'massive configuration' from the device
 *
 * @param cam - device to contact
 * @param conf - object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmConfigFile(ohcmCameraInfo *cam, ohcmConfigFile *conf, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, CONFIG_FILE_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, CONFIG_FILE_URI);

    // create the output buffer (start out large due to the size of the config)
    //
    icFifoBuff *chunk = fifoBuffCreate(4096);

    // create our CURL context
    //
    CURL *curl = createOhcmCurlContext();
    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
    }

    // perform the 'get' operation
    //
    CURLcode rc = ohcmPerformCurlGet(curl, debugUrl, chunk, retryCounts);
    if (rc == CURLE_OK)
    {
        if (isIcLogPriorityTrace() == true && chunk != NULL && fifoBuffGetPullAvailable(chunk) > 0)
        {
            icLogTrace(OHCM_LOG, "camera get: %s\n%s", debugUrl, (char *)fifoBuffPullPointer(chunk, 0));
        }

        // success with the 'get', so parse the result
        //
        if (ohcmParseXmlHelper(chunk, parseMassiveConfigFileXmlNode, conf) == false)
        {
            // unable to parse result from camera
            //
            icLogWarn(OHCM_LOG, "error parsing results of %s", debugUrl);
            rc = CURLE_CONV_FAILED;
        }
    }

    // cleanup
    //
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

/*
 * apply the 'massive configuration' settings on the camera
 *
 * @param cam - device to contact
 * @param conf - settings to apply
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmConfigFile(ohcmCameraInfo *cam, ohcmConfigFile *conf, uint32_t retryCounts)
{
    // build up the URL to hit
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    snprintf(realUrl, MAX_URL_LENGTH, "https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, CONFIG_FILE_URI);
    snprintf(debugUrl, MAX_URL_LENGTH, "https://%s%s", cam->cameraIP, CONFIG_FILE_URI);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

    // create the payload
    // first, build up the XML doc
    //
    icFifoBuff *payload = fifoBuffCreate(4096);
    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST CONFIG_FILE_TOP_NODE);
    xmlNewProp(root, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, root);

    // add each section:
    // ConfigTimers
    xmlNodePtr section = xmlNewNode(NULL, BAD_CAST CONF_TIMERS_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);
    appendConfigTimersXml(section, &conf->timers);

    // DeviceInfo (just the section header)
    section = xmlNewNode(NULL, BAD_CAST CONF_DEVICE_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);

    // TODO: Time
    section = xmlNewNode(NULL, BAD_CAST CONF_TIME_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);

    // TODO: NTPServerList
    section = xmlNewNode(NULL, BAD_CAST CONF_NTP_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);

    // TODO: LoggingConfig
/**
    section = xmlNewNode(NULL, BAD_CAST CONF_LOG_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);
 **/

    // HostServer
    section = xmlNewNode(NULL, BAD_CAST CONF_HOST_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);
    appendHostServerXml(section, &conf->hostServer);

    // TODO: HistoryConfiguration
    section = xmlNewNode(NULL, BAD_CAST CONF_HISTORY_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);

    // NetworkInterfaceList
    section = xmlNewNode(NULL, BAD_CAST CONF_NETWORK_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);
    appendOhcmNetworkInterfaceListXml(section, conf->networkInterfaceList);

    // AudioChannelList
    section = xmlNewNode(NULL, BAD_CAST CONF_AUDIO_CHANNEL_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);
    appendOhcmAudioChannelListXml(section, conf->audioChannelList);

    // VideoInput
    section = xmlNewNode(NULL, BAD_CAST CONF_VIDEO_INPUT_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);
    appendOhcmVideoInputChannelListXml(section, conf->videoInputList);

    // UserList
    section = xmlNewNode(NULL, BAD_CAST CONF_USERS_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);
    appendUserListXml(section, conf->securityAccountList);

    // StreamingChannelList
    section = xmlNewNode(NULL, BAD_CAST CONF_STREAM_CHANNEL_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);
// NOTE: setting the StreamingChannelList causes problems with Sercomm devices
//    appendOhcmStreamChannelListXml(section, &conf->timers);

    // TODO: MotionDetectionList
    section = xmlNewNode(NULL, BAD_CAST CONF_MOTION_DETECT_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);

    // TODO: SoundDetectionList
    section = xmlNewNode(NULL, BAD_CAST CONF_SOUND_DETECT_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);

    // TODO: EventNotification
    section = xmlNewNode(NULL, BAD_CAST CONF_EVENT_NOTIF_SECTION_NODE);
    xmlNewProp(section, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, section);

    // convert XML to a string, then cleanup
    //
    ohcmExportXmlToBuffer(doc, payload);
    xmlFreeDoc(doc);

    // create our CURL context.  Note setting "UPLOAD=yes"
    // because the camera want's this to be received as a PUT
    //
    CURL *curl = createOhcmCurlContext();
    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(curl, CURLOPT_POST, 1) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POST, 1) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(curl, CURLOPT_UPLOAD, 1) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_UPLOAD, 1) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(curl, CURLOPT_INFILESIZE, fifoBuffGetPullAvailable(payload)) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_INFILESIZE, fifoBuffGetPullAvailable(payload)) failed at %s(%d)",__FILE__,__LINE__);
    }

    // add HTTP headers
    //
    struct curl_slist *header = NULL;
    header = curl_slist_append(header, OHCM_CONTENT_TYPE_HEADER);
    header = curl_slist_append(header, OHCM_CONN_CLOSE_HEADER);
    header = curl_slist_append(header, OHCM_SERVER_HEADER);
    if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header) failed at %s(%d)",__FILE__,__LINE__);
    }

    // perform the 'post' operation.  Allow up to 2 iterations.
    //
    bool done = false;
    CURLcode rc = CURLE_URL_MALFORMAT;
    for(int i = 0; !done && i < 2; i++)
    {
        done = true;
        rc = ohcmPerformCurlPost(curl, debugUrl, payload, chunk, retryCounts);
        if (rc == CURLE_OK)
        {
            // success with the 'post', so parse the result
            //
            ohcmBasicResponse result;
            memset(&result, 0, sizeof(ohcmBasicResponse));
            if (ohcmParseBasicResponse(chunk, &result) == false)
            {
                // error parsing, force a failure
                //
                icLogWarn(OHCM_LOG, "error parsing results of %s", debugUrl);
                rc = CURLE_CONV_FAILED;
            }
            else
            {
                // look at the result code to see if it was successful.
                //
                rc = ohcmTranslateOhcmResponseCodeToCurl(result.statusCode);
                if (rc == CURLE_OK)
                {
                    icLogDebug(OHCM_LOG, "setConfig was SUCCESSFUL");
                }
                else if (rc == CURLE_LDAP_CANNOT_BIND)
                {
                    icLogDebug(OHCM_LOG, "setConfig success, responded with 'Needs Reboot'");
                }
                else if (result.statusMessage != NULL)
                {
                    icLogWarn(OHCM_LOG, "result of %s contained error: %s - %s",
                              debugUrl, ohcmResponseCodeLabels[result.statusCode], result.statusMessage);
                }
            }

            free(result.statusMessage);
        }
        else if (rc == CURLE_LOGIN_DENIED)
        {
            // some cameras apply the config then reboot by themselves.  This causes our retries to fail
            // on a 401 response since the new credentials have been applied.  Try one more time with the
            // new credentials
            icLogDebug(OHCM_LOG, "setConfig got login denied, trying one more time with updated credentials");

            done = false;
            retryCounts = 1;

            //find admin security account
            sbIcLinkedListIterator *it = linkedListIteratorCreate(conf->securityAccountList);
            while(linkedListIteratorHasNext(it))
            {
                ohcmSecurityAccount *account = linkedListIteratorGetNext(it);
                if(account->accessRights == OHCM_ACCESS_ADMIN)
                {
                    snprintf(realUrl, MAX_URL_LENGTH, "https://%s:%s@%s%s", account->userName, account->password, cam->cameraIP, CONFIG_FILE_URI);
                    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
                    {
                        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
                    }
                    break;
                }
            }
        }
    }

    // cleanup
    //
    curl_slist_free_all(header);
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);
    fifoBuffDestroy(payload);   // also free's xmlBuff

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

