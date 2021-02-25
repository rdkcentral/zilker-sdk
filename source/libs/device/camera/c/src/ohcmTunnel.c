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
 * ohcmTunnel.c
 *
 * implementation of "media tunnel" functionality as defined in ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <string.h>
#include <openHomeCamera/ohcm.h>
#include "ohcmBase.h"

#define CREATE_MEDIA_TUNNEL_URI     "/Openhome/Streaming/mediatunnel/create"
#define STREAMING_MEDIA_TUNNEL_URI  "/Openhome/Streaming/mediatunnel"

#define TUNNEL_CREATE_TOP_NODE      "CreateMediaTunnel"
#define TUNNEL_SESSION_NODE         "sessionID"
#define TUNNEL_GATEWAY_URL_NODE     "gatewayURL"
#define TUNNEL_FAILURE_URL_NODE     "failureURL"

/*
 * helper function to create a blank ohcmMediaTunnelRequest object
 */
ohcmMediaTunnelRequest *createOhcmMediaTunnelRequest()
{
    return (ohcmMediaTunnelRequest *)calloc(1, sizeof(ohcmMediaTunnelRequest));
}

/*
 * helper function to destroy the ohcmMediaTunnelRequest object
 */
void destroyOhcmMediaTunnelRequest(ohcmMediaTunnelRequest *obj)
{
    if (obj != NULL)
    {
        free(obj->sessionID);
        obj->sessionID = NULL;
        free(obj->gatewayURL);
        obj->gatewayURL = NULL;
        free(obj->failureURL);
        obj->failureURL = NULL;

        free(obj);
    }
}

/*
 * ask the camera to start a media tunnel session
 *
 * @param cam - device to contact
 * @param conf - details for the media tunnel request
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode startOhcmMediaTunnelRequest(ohcmCameraInfo *cam, ohcmMediaTunnelRequest *conf, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, CREATE_MEDIA_TUNNEL_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, CREATE_MEDIA_TUNNEL_URI);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

    // create the payload
    // first, build up the XML doc
    //
    icFifoBuff *payload = fifoBuffCreate(1024);
    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST TUNNEL_CREATE_TOP_NODE);
    xmlNewProp(root, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, root);
    if (conf->sessionID != NULL)
    {
        xmlNewTextChild(root, NULL, BAD_CAST TUNNEL_SESSION_NODE, BAD_CAST conf->sessionID);
    }
    if (conf->gatewayURL != NULL)
    {
        xmlNewTextChild(root, NULL, BAD_CAST TUNNEL_GATEWAY_URL_NODE, BAD_CAST conf->gatewayURL);
    }
    if (conf->failureURL != NULL)
    {
        xmlNewTextChild(root, NULL, BAD_CAST TUNNEL_FAILURE_URL_NODE, BAD_CAST conf->failureURL);
    }

    // convert XML to a string, then cleanup
    //
    ohcmExportXmlToBuffer(doc, payload);
    xmlFreeDoc(doc);

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
 * ask the camera to stop a media tunnel session
 *
 * @param cam - device to contact
 * @param sessionId - media tunnel session to stop
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode stopOhcmMediaTunnelRequest(ohcmCameraInfo *cam, char *sessionId, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s/%s/destroy", cam->userName, cam->password, cam->cameraIP, STREAMING_MEDIA_TUNNEL_URI, sessionId);
    sprintf(debugUrl,"https://%s%s/%s/destroy", cam->cameraIP, STREAMING_MEDIA_TUNNEL_URI, sessionId);

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
    // is this right?
    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDS, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POSTFIELDS, realUrl) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(realUrl)) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(realUrl)) failed at %s(%d)",__FILE__,__LINE__);
    }

    // perform the 'post' operation.  note we'll use a NULL payload
    // since the data to send is set via POSTFIELDS option
    //
    CURLcode rc = ohcmPerformCurlPost(curl, debugUrl, NULL, chunk, retryCounts);
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
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

