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
 * ohcmUpgrade.c
 *
 * implementation of "firmware upgrade" functionality as defined in ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <string.h>
#include <openHomeCamera/ohcm.h>
#include <xmlHelper/xmlHelper.h>
#include "ohcmBase.h"

#define UPDATE_FIRMWARE_URI     "/OpenHome/System/updateFirmware"
#define UPDATE_TOP_NODE         "FirmwareDownload"
#define UPDATE_FWARE_VER_NODE   "fwVersion"
#define UPDATE_MD5SUM_NODE      "md5checksum"

#define UPDATE_SUCCESS_NODE     "updateSuccess"
#define UPDATE_STATE_NODE       "updateState"
#define UPDATE_URL_NODE         "url"
#define UPDATE_TIME_NODE        "updateTime"
#define UPDATE_PROGRESS_NODE    "downloadPercentage"

/*
 * helper function to create a blank ohcmUpdateFirmwareRequest object
 */
ohcmUpdateFirmwareRequest *createOhcmUpdateFirmwareRequest()
{
    return (ohcmUpdateFirmwareRequest *)calloc(1, sizeof(ohcmUpdateFirmwareRequest));
}

/*
 * helper function to destroy the ohcmUpdateFirmwareRequest object
 */
void destroyOhcmUpdateFirmwareRequest(ohcmUpdateFirmwareRequest *obj)
{
    if (obj != NULL)
    {
        free(obj->url);
        obj->url = NULL;
        free(obj->fwVersion);
        obj->fwVersion = NULL;
        free(obj->md5checksum);
        obj->md5checksum = NULL;

        free(obj);
    }
}



/*
 * ask the camera to start a firmware update.  if successful, it will be
 * possible to get update state via ohcmUpdateFirmareStatus
 *
 * @param cam - device to contact
 * @param req - details for the firmware update
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode startOhcmUpdateFirmwareRequest(ohcmCameraInfo *cam, ohcmUpdateFirmwareRequest *conf, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, UPDATE_FIRMWARE_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, UPDATE_FIRMWARE_URI);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

    // create the payload
    // first, build up the XML doc
    //
    icFifoBuff *payload = fifoBuffCreate(1024);
    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST UPDATE_TOP_NODE);
    xmlNewProp(root, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, root);
    if (conf->url != NULL)
    {
        xmlNewTextChild(root, NULL, BAD_CAST UPDATE_URL_NODE, BAD_CAST conf->url);
    }
    if (conf->fwVersion != NULL)
    {
        xmlNewTextChild(root, NULL, BAD_CAST UPDATE_FWARE_VER_NODE, BAD_CAST conf->fwVersion);
    }

    xmlNewTextChild(root, NULL, BAD_CAST UPDATE_MD5SUM_NODE, BAD_CAST conf->md5checksum);

    // convert XML to a string, then cleanup
    //
    ohcmExportXmlToBuffer(doc, payload);
    xmlFreeDoc(doc);
    uint32_t payLen = fifoBuffGetPullAvailable(payload);

    // create our CURL context
    //
    CURL *curl = createOhcmCurlContext();
    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
    }
//    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
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
 * helper function to create a blank ohcmMediaTunnelRequest object
 */
ohcmUpdateFirmwareStatus *createOhcmUpdateFirmwareStatus()
{
    return (ohcmUpdateFirmwareStatus *)calloc(1, sizeof(ohcmUpdateFirmwareStatus));
}

/*
 * helper function to destroy the ohcmUpdateFirmwareStatus object
 */
void destroyOhcmUpdateFirmwareStatus(ohcmUpdateFirmwareStatus *obj)
{
    if (obj != NULL)
    {
        free(obj->url);
        obj->url = NULL;
        free(obj->updateState);
        obj->updateState = NULL;

        free(obj);
    }
}


/*
 * parse an XML node for information about an in-progress upgrade.
 * assumes 'funcArg' is an ohcmUpdateFirmwareStatus object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
static bool parseUpdateFirmwareStatusXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmUpdateFirmwareStatus *status = (ohcmUpdateFirmwareStatus *) funcArg;

    if (strcmp((const char *) node->name, UPDATE_SUCCESS_NODE) == 0)
    {
        status->updateSuccess = getXmlNodeContentsAsBoolean(node, false);
    }
    else if (strcmp((const char *) node->name, UPDATE_STATE_NODE) == 0)
    {
        status->updateState = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, UPDATE_TIME_NODE) == 0)
    {
        // TODO: not used, so not worried about this
    }
    else if (strcmp((const char *) node->name, UPDATE_URL_NODE) == 0)
    {
        status->url = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, UPDATE_PROGRESS_NODE) == 0)
    {
        status->downloadPercentage = getXmlNodeContentsAsUnsignedInt(node, 0);
    }

    return true;
}

/*
 * ask the camera to retrieve the status of the 'update firmware' request
 *
 * @param cam - device to contact
 * @param status - target object to populate with details
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmUpdateFirmwareStatus(ohcmCameraInfo *cam, ohcmUpdateFirmwareStatus *status, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s/status", cam->userName, cam->password, cam->cameraIP, UPDATE_FIRMWARE_URI);
    sprintf(debugUrl,"https://%s%s/status", cam->cameraIP, UPDATE_FIRMWARE_URI);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

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
        if (ohcmParseXmlHelper(chunk, parseUpdateFirmwareStatusXmlNode, status) == false)
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
