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
 * ohcmBase.h
 *
 * set of common functions that are not public via ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#ifndef IC_OHCM_BASE_H
#define IC_OHCM_BASE_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#include <curl/curl.h>
#include <libxml/tree.h>

#include <openHomeCamera/ohcm.h>
#include <icBuildtime.h>
#include <icLog/logging.h>
#include <icTypes/icFifoBuffer.h>

// log prefix
#define OHCM_LOG    "ohcm"

// max URL string size
#define MAX_URL_LENGTH  256

// common XML values
#define OHCM_XML_VERSION            "1.0"
#define OHCM_XML_VERSION_ATTRIB     "version"
#define OHCM_XML_UTF8               "UTF-8"
#define OHCM_CONTENT_TYPE_HEADER    "Content-Type: text/xml; charset=utf-8"
#define OHCM_CONN_CLOSE_HEADER      "Connection: close"
#define OHCM_SERVER_HEADER          "Server: ip-camera"

// XML nodes for the basic response
#define BASIC_STATUS_CODE_NODE          "statusCode"
#define BASIC_STATUS_MSG_NODE           "statusString"

/*
 * translate a CURLCode to a ohcmResultCode value
 */
ohcmResultCode ohcmTranslateCurlCode(CURLcode code);

/*
 * translate an HTTP code to a CURLcode
 */
CURLcode ohcmTranslateHttpCode(long httpCode);

/*
 * create a default CURL context, applying the standard set of
 * options used for all OHCM calls (timeout, TLS, etc).
 */
CURL *createOhcmCurlContext();

/**
 * Clean up a CURL context
 * @param curl
 */
void destroyOhcmCurlContext(CURL *curl);

/*
 * helper function to perform a CURL "get".  assumes the supplied
 * CURL context is setup and ready for the "get" operation.  if
 * successful, will place the returned data within result->memory
 * and set the result->size appropriately.
 *
 * caller is responsible for releasing the result->memory pointer
 *
 * @param ctx - CURL context
 * @param url - used for debugging
 * @param result - output from the "get" operation
 * @param retryAttempts - if > 0, number of times to retry if the operation fails
 */
CURLcode ohcmPerformCurlGet(CURL *ctx, char *url, icFifoBuff *result, uint32_t retryAttempts);

/*
 * helper function to perform a CURL "post".  assumes the supplied
 * CURL context is setup and ready for the "post" operation.  if the
 * 'payload' is not empty, then it's contents will be sent as part
 * of the post operation.  if successful, will place the returned
 * data within result->memory and set the result->size appropriately.
 *
 * caller is responsible for releasing the result->memory pointer
 *
 * @param ctx - CURL context
 * @param url - used for debugging
 * @param payload - data to send as part of the "post" operation
 * @param result - output from the "post" operation
 * @param retryAttempts - if > 0, number of times to retry if the operation fails
 */
CURLcode ohcmPerformCurlPost(CURL *ctx, char *url, icFifoBuff *payload, icFifoBuff *result, uint32_t retryAttempts);

/*
 * utility to export an XML Document to a String, then
 * append to a fifo buffer
 */
void ohcmExportXmlToBuffer(xmlDoc *doc, icFifoBuff *buffer);

/*
 * comes direct from open home spec:
 *   O=1-OK, 2-Device Busy, 3-Device Error, 4-Invalid Operation, 5-Invalid XML Format, 6-Invalid XML Content
 */
typedef enum {
    OHCM_RESP_OK,
    OHCM_RESP_SUCCESS,
    OHCM_RESP_DEVICE_BUSY,
    OHCM_RESP_DEVICE_ERROR,
    OHCM_RESP_INVALID_OP,
    OHCM_RESP_INVALID_XML_FORMAT,
    OHCM_RESP_INVALID_XML_CONTENT,
    OHCM_RESP_REBOOT_REQ,
} ohcmResponseCode;

/* string representations of ohcmResponseCode (mainly used for debugging) */
static char *ohcmResponseCodeLabels[] =
{
    "OK",                   // OHCM_RESP_OK
    "OK",                   // OHCM_RESP_SUCCESS
    "Device Busy",          // OHCM_RESP_DEVICE_BUSY
    "Device Error",         // OHCM_RESP_DEVICE_ERROR
    "Invalid Operation",    // OHCM_RESP_INVALID_OP
    "Invalid XML Format",   // OHCM_RESP_INVALID_XML_FORMAT
    "Invalid XML Content",  // OHCM_RESP_INVALID_XML_CONTENT
    "Reboot Required",      // OHCM_RESP_REBOOT_REQ
};

/*
 * basic info we get from the camera for most POST commands
 */
typedef struct {
    ohcmResponseCode statusCode;
    char *statusMessage;
} ohcmBasicResponse;

/*
 * parse the basic response from the camera for post POST commands.
 * caller must free 'result->statusMessage'
 */
bool ohcmParseBasicResponse(icFifoBuff *result, ohcmBasicResponse *parsed);

/*
 * translate a ohcmResponseCode to a ohcmResultCode
 */
ohcmResultCode ohcmTranslateOhcmResponseCode(ohcmResponseCode code);

/*
 * translate a ohcmResponseCode to a CURLCode
 */
CURLcode ohcmTranslateOhcmResponseCodeToCurl(ohcmResponseCode code);

/*
 * function signature for the ohcmParseXmlHelper.  if the function
 * implementation returns "false", the parsing will stop.
 */
typedef bool (*ohcmParseXmlNodeCallback)(const xmlChar *topNodeName, xmlNodePtr node, void *funcArg);

/*
 * Helper function to parse the chunk as an XML document, then
 * iterate through the children of the top-level node, calling
 * 'func' for each XML node so it can be examined.
 *
 * @param xmlBuffer - XML received from the camera to be parsed as a doc
 * @param func - function to invoke for each node (child of top-level node) within the doc
 * @param funcArg - optional data to pass into 'func'
 */
bool ohcmParseXmlHelper(icFifoBuff *xmlBuffer, ohcmParseXmlNodeCallback func, void *funcArg);

/*
 * Helper function to loop through the children of 'node' and call
 * 'func' for each child XML node so it can be examined.
 *
 * @param node - top-level node to iterate through
 * @param func - function to invoke for each node (child of top-level node) within the doc
 * @param funcArg - optional data to pass into 'func'
 */
void ohcmParseXmlNodeChildren(xmlNodePtr node, ohcmParseXmlNodeCallback func, void *funcArg);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // IC_OHCM_BASE_H
