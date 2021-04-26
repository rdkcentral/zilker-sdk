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
 * ohcmBase.c
 *
 * set of common functions that are not public via ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <curl/curlver.h>
#include <libxml/xmlversion.h>
#include <libxml/tree.h>

#include <openHomeCamera/ohcm.h>
#include <xmlHelper/xmlHelper.h>
#include <urlHelper/urlHelper.h>
#include <propsMgr/sslVerify.h>
#include "ohcmBase.h"

#define CURL_RETRY_SLEEP_MICROSECONDS   500000

// local variables
static char *clientCertFilename = NULL;         // for Mutual TLS support (must be client + intermediates in PEM)
static char *clientPrivKeyFilename = NULL;
static sslVerify tlsVerify = SSL_VERIFY_NONE;
static pthread_mutex_t MUTUAL_TLS_MTX = PTHREAD_MUTEX_INITIALIZER;

// local functions
static size_t curlWriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userPtr);
static size_t curlReadMemoryCallback(void *ptr, size_t size, size_t nmemb, void *userPtr);

/*
 * initialize the OpenHome Camera library.  must be called at least once
 * prior to other function calls.
 */
void initOhcm()
{
    // init curl and libxml2.  more then likely this was done
    // in our 'main', but is supposedly safe to do this again.
    //
    LIBXML_TEST_VERSION
}

/*
 * cleanup internal resources created during the initOhcm() function
 */
void cleanupOhcm()
{
    // cleanup curl
    //
//    curl_global_cleanup();
}

void setOhcmMutualTlsMode(const char *certFilename, const char *privKeyFilename)
{
    pthread_mutex_lock(&MUTUAL_TLS_MTX);

    // cleanup previous values (regardless of what is passed)
    //
    if (clientCertFilename != NULL)
    {
        free(clientCertFilename);
        clientCertFilename = NULL;
    }
    if (clientPrivKeyFilename != NULL)
    {
        free(clientPrivKeyFilename);
        clientPrivKeyFilename = NULL;
    }

    // save new filenames
    //
    if (certFilename != NULL)
    {
        clientCertFilename = strdup(certFilename);
    }
    if (privKeyFilename != NULL)
    {
        clientPrivKeyFilename = strdup(privKeyFilename);
    }
    pthread_mutex_unlock(&MUTUAL_TLS_MTX);
}

bool ohcmIsMtlsCapable(void)
{
    bool mtlsCapable = false;

    pthread_mutex_lock(&MUTUAL_TLS_MTX);

    if (clientCertFilename != NULL && access(clientCertFilename, R_OK) == 0 &&
        clientPrivKeyFilename != NULL && access(clientPrivKeyFilename, R_OK) == 0)
    {
        mtlsCapable = true;
    }

    pthread_mutex_unlock(&MUTUAL_TLS_MTX);

    return mtlsCapable;
}

void ohcmSetTLSVerify(sslVerify level)
{
    /* Cameras always use ipAddr, but urlHelper can't determine this as used by/with createOhcmCurlContext */
    switch (level)
    {
        case SSL_VERIFY_BOTH:
            level = SSL_VERIFY_PEER;
            break;

        case SSL_VERIFY_HOST:
            level = SSL_VERIFY_NONE;
            break;

        case SSL_VERIFY_PEER:
        case SSL_VERIFY_NONE:
            break;

        case SSL_VERIFY_INVALID:
        default:
            icLogError(OHCM_LOG, "Can not set TLS verify level to [%d]: not supported. Using SSL_VERIFY_NONE", level);
            level = SSL_VERIFY_NONE;
            break;
    }

    pthread_mutex_lock(&MUTUAL_TLS_MTX);
    tlsVerify = level;
    pthread_mutex_unlock(&MUTUAL_TLS_MTX);
}

sslVerify ohcmGetTLSVerify(void)
{
    sslVerify level;
    pthread_mutex_lock(&MUTUAL_TLS_MTX);
    level = tlsVerify;
    pthread_mutex_unlock(&MUTUAL_TLS_MTX);

    return level;
}

/*
 * internal function to apply mutual TLS options if applicable
 */
static void applyOhcmMutualTls(CURL *curl)
{
    // apply the cert/key filenames if they exist.  we perform this
    // check each time as the files could be added/removed without
    // our knowledge - and don't want ot rely on a subsequent call
    // to set/check the filenames.
    //
    pthread_mutex_lock(&MUTUAL_TLS_MTX);
    if (clientCertFilename != NULL && clientPrivKeyFilename != NULL)
    {
        // see if files exist and have size
        //
        struct stat fileInfo;
        if ((stat(clientCertFilename, &fileInfo) == 0) && (fileInfo.st_size > 5))
        {
            // got the cert, check the private key
            //
            if ((stat(clientPrivKeyFilename, &fileInfo) == 0) && (fileInfo.st_size > 5))
            {
                icLogTrace(OHCM_LOG, "using 'mutual TLS' filenames %s %s", clientCertFilename, clientPrivKeyFilename);

                // both files there, good to go with the feature
                // first the certificate.  PEM is the default, but go ahead and force it
                //
                if (curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "PEM") != CURLE_OK)
                {
                    icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, \"PEM\") failed at %s(%d)",__FILE__,__LINE__);
                }
                if (curl_easy_setopt(curl, CURLOPT_SSLCERT, clientCertFilename) != CURLE_OK)
                {
                    icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_SSLCERT, clientCertFilename) failed at %s(%d)",__FILE__,__LINE__);
                }

                // now the private key
                //
                if (curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, "PEM") != CURLE_OK)
                {
                    icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_SSLKEYTYPE, \"PEM\") failed at %s(%d)",__FILE__,__LINE__);
                }
                if (curl_easy_setopt(curl, CURLOPT_SSLKEY, clientPrivKeyFilename) != CURLE_OK)
                {
                    icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_SSLKEY, clientPrivKeyFilename) failed at %s(%d)",__FILE__,__LINE__);
                }
            }
        }
    }
    pthread_mutex_unlock(&MUTUAL_TLS_MTX);
}

/*
 * translate a CURLCode to a ohcmResultCode value
 */
ohcmResultCode ohcmTranslateCurlCode(CURLcode code)
{
    switch(code)
    {
        case CURLE_OK:
            return OHCM_SUCCESS;

        case CURLE_COULDNT_RESOLVE_HOST:
        case CURLE_COULDNT_CONNECT:
        case CURLE_NO_CONNECTION_AVAILABLE:
        case CURLE_SEND_ERROR:
        case CURLE_RECV_ERROR:
            return OHCM_COMM_FAIL;

        case CURLE_OPERATION_TIMEDOUT:
            return OHCM_COMM_TIMEOUT;

        case CURLE_LOGIN_DENIED:
        case CURLE_REMOTE_ACCESS_DENIED:
            return OHCM_LOGIN_FAIL;

        case CURLE_USE_SSL_FAILED:
        case CURLE_SSL_ENGINE_NOTFOUND:
        case CURLE_SSL_ENGINE_SETFAILED:
        case CURLE_SSL_CERTPROBLEM:
        case CURLE_SSL_CIPHER:
        case CURLE_SSL_CACERT:
        case CURLE_SSL_CONNECT_ERROR:
        case CURLE_SSL_CACERT_BADFILE:
        case CURLE_SSL_SHUTDOWN_FAILED:
        case CURLE_SSL_CRL_BADFILE:
        case CURLE_SSL_ISSUER_ERROR:
        case CURLE_SSL_ENGINE_INITFAILED:
#if LIBCURL_VERSION_NUM >= 0x072900
        case CURLE_SSL_INVALIDCERTSTATUS:   // available on Curl 7.41 and higher (RDK-B uses 7.35)
#endif
            return OHCM_SSL_FAIL;

        case CURLE_BAD_CONTENT_ENCODING:    // not ideal, but works
        case CURLE_CONV_FAILED:
            return OHCM_INVALID_CONTENT;

        case CURLE_LDAP_CANNOT_BIND:
            // generally via ohcmTranslateOhcmResponseCodeToCurl
            return OHCM_REBOOT_REQ;

        default:
            return OHCM_GENERAL_FAIL;
    }
}

/*
 * translate an HTTP code to a CURLcode
 */
CURLcode ohcmTranslateHttpCode(long httpCode)
{
    if (httpCode == 200 || httpCode == 100)
    {
        return CURLE_OK;
    }
    else if (httpCode >= 401 && httpCode <= 403)
    {
        // authorization problem
        //
        return CURLE_LOGIN_DENIED;
    }

    // general failure
    //
    return CURLE_GOT_NOTHING;
}

/*
 * create a default CURL context, applying the standard set of
 * options used for all OHCM calls (timeout, TLS, etc).
 */
CURL *createOhcmCurlContext()
{
    CURL *retVal = curl_easy_init();

    if (retVal != NULL)
    {
        // set standard options:

        pthread_mutex_lock(&MUTUAL_TLS_MTX);

        applyStandardCurlOptions(retVal, NULL, 60, tlsVerify, false);

        pthread_mutex_unlock(&MUTUAL_TLS_MTX);

        // setup 'mutual TLS' (if enabled)
        //
        applyOhcmMutualTls(retVal);

        if (curl_easy_setopt(retVal, CURLOPT_CONNECTTIMEOUT, 10L) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(retVal, CURLOPT_CONNECTTIMEOUT, 10L) failed at %s(%d)",__FILE__,__LINE__);
        }

        // mainly for POSTs, timeout waiting on the 100 response before sending
        //
//        curl_easy_setopt(retVal, CURLOPT_EXPECT_100_TIMEOUT_MS, 60L);
    }

    return retVal;
}

void destroyOhcmCurlContext(CURL *curl)
{
    curl_easy_cleanup(curl);
}

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
CURLcode ohcmPerformCurlGet(CURL *ctx, char *url, icFifoBuff *result, uint32_t retryAttempts)
{
    CURLcode res = CURLE_GOT_NOTHING;
    int count = 0;

    // put the 'result' object into the context and assign the helper function
    // for CURL to call when we get a response back from the target host
    //
    if (curl_easy_setopt(ctx, CURLOPT_WRITEFUNCTION, curlWriteMemoryCallback) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_WRITEFUNCTION, curlWriteMemoryCallback) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(ctx, CURLOPT_WRITEDATA, (void *)result) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_WRITEDATA, (void *)result) failed at %s(%d)",__FILE__,__LINE__);
    }

    // loop up-to retryAttempts
    //
    do
    {
        if (isIcLogPriorityTrace() == true)
        {
            icLogTrace(OHCM_LOG, "camera get: %s", url);
        }

        // run the 'get' operation on the URL
        //
        res = curl_easy_perform(ctx);
        if (isIcLogPriorityTrace() == true)
        {
            icLogTrace(OHCM_LOG, "camera get: %s returned %d", url, res);
/*
            if (getFBuffSize(result) > 0)
            {
                // log the result, but ensure NULL terminated.  we'll also make a copy so that
                // we don't alter the actual result
                //
                icFifoBuff *copy = cloneFBuff(result);
                pushByteToFBuff(copy, '\0');
                icLogDebug(OHCM_LOG, "camera get: %s results\n%s", url, (char *) pullFromBuffDirect(copy, 0));
                destroyFBuff(copy);
            }
 */
        }

        // if success, break from the loop
        //
        if (res == CURLE_OK)
        {
            // ensure 'result' is NULL terminated
            //
            fifoBuffPushByte(result, '\0');
            break;
        }
        else if (res == CURLE_HTTP_RETURNED_ERROR)
        {
            // got an HTTP code from the server, so extract it from the context
            //
            long httpCode = 0;
            curl_easy_getinfo (ctx, CURLINFO_RESPONSE_CODE, &httpCode);
            if (isIcLogPriorityTrace() == true)
            {
                icLogTrace(OHCM_LOG, "camera get: %s returned HTTP code %ld", url, httpCode);
            }

            // no sense in retrying as something went wrong.  attempt to map
            // the http code to a return value
            //
            return ohcmTranslateHttpCode(httpCode);
        }

        // didn't work, pause a bit then try again
        //
        if ((count + 1) < retryAttempts)
        {
            usleep(CURL_RETRY_SLEEP_MICROSECONDS);
        }
        count++;

    } while (count < retryAttempts);

    // log the warning
    //
    if (res != CURLE_OK)
    {
        icLogWarn(OHCM_LOG, "camera get: '%s' failed with error '%s'", url, curl_easy_strerror(res));
    }
    return res;
}

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
CURLcode ohcmPerformCurlPost(CURL *ctx, char *url, icFifoBuff *payload, icFifoBuff *result, uint32_t retryAttempts)
{
    CURLcode res = CURLE_GOT_NOTHING;
    int count = 0;

    // put the 'result' object into the context and assign the helper function
    // for CURL to call when we get a response back from the target host
    //
    if (curl_easy_setopt(ctx, CURLOPT_WRITEFUNCTION, curlWriteMemoryCallback) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_WRITEFUNCTION, curlWriteMemoryCallback) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(ctx, CURLOPT_WRITEDATA, (void *)result) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_WRITEDATA, (void *)result) failed at %s(%d)",__FILE__,__LINE__);
    }

    // if we have a payload, assign that into the context along with the helper function
    // for CURL to use when performing the operation
    //
    if (payload != NULL)
    {
        // set the 'READDATA' option along with it's size
        // since POSTFIELDS isn't set, it should get content from our callback
        //
//        curl_easy_setopt(ctx, CURLOPT_UPLOAD, 1L);
        if (curl_easy_setopt(ctx, CURLOPT_READDATA, (void *)payload) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_READDATA, (void *)payload) failed at %s(%d)",__FILE__,__LINE__);
        }
        if (curl_easy_setopt(ctx, CURLOPT_READFUNCTION, curlReadMemoryCallback) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_READFUNCTION, curlReadMemoryCallback) failed at %s(%d)",__FILE__,__LINE__);
        }
        if (curl_easy_setopt(ctx, CURLOPT_POSTFIELDSIZE, (long)fifoBuffGetPullAvailable(payload)) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_POSTFIELDSIZE, (long)fifoBuffGetPullAvailable(payload)) failed at %s(%d)",__FILE__,__LINE__);
        }
    }

    // make sure setup for POST
    //
    if (curl_easy_setopt(ctx, CURLOPT_POST, 1L) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_POST, 1L) failed at %s(%d)",__FILE__,__LINE__);
    }

    // loop up-to retryAttempts
    //
    do
    {
        // create a clone of 'payload' to use for the actual POST.
        // this allows us to re-try because once the data is read
        // from the FIFO buffer, it's gone
        //
        icFifoBuff *copy = NULL;
        if (payload != NULL && fifoBuffGetPullAvailable(payload) > 0)
        {
            // copy and make sure NULL terminated (if anything just for the log print below)
            //
            copy = fifoBuffClone(payload);
            fifoBuffPushByte(copy, '\0');
            if (curl_easy_setopt(ctx, CURLOPT_READDATA, (void *) copy) != CURLE_OK)
            {
                icLogError(OHCM_LOG,"curl_easy_setopt(ctx, CURLOPT_READDATA, (void *) copy) failed at %s(%d)",__FILE__,__LINE__);
            }
            if (isIcLogPriorityTrace() == true)
            {
                icLogTrace(OHCM_LOG, "camera post: %s\n%s", url, (char *)fifoBuffPullPointer(copy, 0));
            }
        }
        else
        {
            if(isIcLogPriorityTrace() == true)
            {
                icLogTrace(OHCM_LOG, "camera post: %s", url);
            }
        }

        // run the 'post' operation on the URL
        //
        res = curl_easy_perform(ctx);
        if (isIcLogPriorityTrace() == true)
        {
            icLogTrace(OHCM_LOG, "camera post: %s returned %d", url, res);
        }

        // cleanup 'copy'
        //
        fifoBuffDestroy(copy);

        // if success, break from the loop
        //
        if (res == CURLE_OK)
        {
            // ensure 'result' is NULL terminated
            //
            fifoBuffPushByte(result, '\0');
            break;
        }
        else if (res == CURLE_HTTP_RETURNED_ERROR)
        {
            // got an HTTP code from the server, so extract it from the context
            //
            long httpCode = 0;
            curl_easy_getinfo (ctx, CURLINFO_RESPONSE_CODE, &httpCode);
            if (isIcLogPriorityTrace() == true)
            {
                icLogTrace(OHCM_LOG, "camera post: %s returned HTTP code %ld", url, httpCode);
            }

            // no sense in retrying as something went wrong.  attempt to map
            // the http code to a return value
            //
            return ohcmTranslateHttpCode(httpCode);
        }

        // didn't work, pause a bit then try again
        //
        if ((count + 1) < retryAttempts)
        {
            usleep(CURL_RETRY_SLEEP_MICROSECONDS);
        }
        count++;

    } while (count < retryAttempts);

    // log the warning
    //
    if (res != CURLE_OK)
    {
        icLogWarn(OHCM_LOG, "camera post: '%s' failed with error '%s'", url, curl_easy_strerror(res));
    }
    return res;
}

/*
 * utility to export an XML Document to a String, then
 * append to a fifo buffer
 */
void ohcmExportXmlToBuffer(xmlDoc *doc, icFifoBuff *buffer)
{
    // convert XML to a string
    //
    xmlChar *xmlStr = NULL;
    int xmlSize;
    xmlDocDumpMemoryEnc(doc, &xmlStr, &xmlSize, OHCM_XML_UTF8);

    // append to the buffer
    //
    fifoBuffPush(buffer, xmlStr, (uint32_t)xmlSize);
//    fifoBuffPushByte(buffer, '\0');

    // cleanup
    //
    free(xmlStr);
    xmlMemoryDump();
}

/**
 * transfer results of a GET/POST from curl into user-space.
 * internal, therefore assume 'userPtr' is an icFifoBuff object
 */
static size_t curlWriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userPtr)
{
    size_t realsize = size * nmemb;
    icFifoBuff *chunk = (icFifoBuff *)userPtr;

    // enusre there's enough room to transfer from curl to the
    // 'mem' object and append 'content' to our buffer
    //
    fifoBuffPush(chunk, contents, (uint32_t)realsize);

    return realsize;
}

/**
 * transfer a payload that's part of a "POST" into the curl context.
 * internal, therefore assume 'userPtr' is an icFifoBuff object
 */
static size_t curlReadMemoryCallback(void *ptr, size_t size, size_t nmemb, void *userPtr)
{
    // transferring an XML document, so need to find the length of 'userp'
    // and copy that many bytes into 'ptr', so curl has the data to add
    // to the payload.
    //
    size_t xmlLen = 0;
    icFifoBuff *chunk = (icFifoBuff *)userPtr;
    if (chunk != NULL)
    {
        // should be a string since we don't upload binaries directly
        // note that 'size * nmemb' is the MAXIMUM amount we can copy,
        // NOT THE AMOUNT TO COPY
        //
        xmlLen = fifoBuffGetPullAvailable(chunk);
        if (xmlLen > (size * nmemb))
        {
            xmlLen = (size * nmemb);
        }

        // perform the transfer from 'chunk' into the pointer curl provided
        //
        fifoBuffPull(chunk, ptr, (uint32_t)xmlLen);
    }
    return xmlLen;
}

/*
 * ohcmParseXmlNodeCallback for the 'parse basic response'
 */
static bool parseBasicXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmBasicResponse *resp = (ohcmBasicResponse *)funcArg;

    if (strcmp((const char *) node->name, BASIC_STATUS_CODE_NODE) == 0)
    {
        resp->statusCode = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *) node->name, BASIC_STATUS_MSG_NODE) == 0)
    {
        resp->statusMessage = getXmlNodeContentsAsString(node, NULL);
    }

    return true;
}

/*
 * parse the basic response from the camera for post POST commands.
 * caller must free 'result->statusMessage'
 */
bool ohcmParseBasicResponse(icFifoBuff *result, ohcmBasicResponse *parsed)
{
    return ohcmParseXmlHelper(result, parseBasicXmlNode, parsed);
}

/*
 * translate a ohcmResponseCode to a ohcmResultCode
 */
ohcmResultCode ohcmTranslateOhcmResponseCode(ohcmResponseCode code)
{
    switch (code)
    {
        case OHCM_RESP_OK:
        case OHCM_RESP_SUCCESS:
            return OHCM_SUCCESS;

        case OHCM_RESP_DEVICE_BUSY:
        case OHCM_RESP_DEVICE_ERROR:
        default:
            return OHCM_GENERAL_FAIL;

        case OHCM_RESP_INVALID_OP:
        case OHCM_RESP_INVALID_XML_FORMAT:
        case OHCM_RESP_INVALID_XML_CONTENT:
            return OHCM_INVALID_CONTENT;

        case OHCM_RESP_REBOOT_REQ:
            return OHCM_REBOOT_REQ;
    }
}

/*
 * translate a ohcmResponseCode to a CURLCode
 */
CURLcode ohcmTranslateOhcmResponseCodeToCurl(ohcmResponseCode code)
{
    switch (code)
    {
        case OHCM_RESP_OK:
        case OHCM_RESP_SUCCESS:
            return CURLE_OK;

        case OHCM_RESP_DEVICE_BUSY:
        case OHCM_RESP_DEVICE_ERROR:
        default:
            return CURLE_SEND_ERROR;

        case OHCM_RESP_INVALID_OP:
        case OHCM_RESP_INVALID_XML_FORMAT:
        case OHCM_RESP_INVALID_XML_CONTENT:
            return CURLE_CONV_FAILED;

        case OHCM_RESP_REBOOT_REQ:
            // map to something we shouldn't ever see
            return CURLE_LDAP_CANNOT_BIND;
    }
}

/*
 * Helper function to parse the chunk as an XML document, then
 * iterate through the children of the top-level node, calling
 * 'func' for each XML node so it can be examined.
 *
 * @param xmlBuffer - XML received from the camera to be parsed as a doc
 * @param func - function to invoke for each node (child of top-level node) within the doc
 * @param funcArg - optional data to pass into 'func'
 */
bool ohcmParseXmlHelper(icFifoBuff *xmlBuffer, ohcmParseXmlNodeCallback func, void *funcArg)
{
    // get the raw pointer to the buffer
    //
    uint32_t len = fifoBuffGetPullAvailable(xmlBuffer);
    char *ptr = (char *)fifoBuffPullPointer(xmlBuffer, len);
    if (ptr == NULL || len == 0)
    {
        icLogError(OHCM_LOG, "Failed to get XML from buffer.");
        // nothing to parse
        //
        return false;
    }
    fifoBuffAfterPullPointer(xmlBuffer, (uint32_t)strlen(ptr));

    // parse the XML doc
    //
    xmlDocPtr doc = xmlParseMemory(ptr, len);
    if (doc == NULL)
    {
        icLogError(OHCM_LOG, "Failed to parse XML from memory\n [%s]", ptr);
        // no good
        //
        return false;
    }

    // get the top-level node
    //
    xmlNodePtr top = xmlDocGetRootElement(doc);
    if (top == NULL)
    {
        icLogError(OHCM_LOG, "Failed to get root element.");
        // invalid XML, missing root node
        //
        xmlFreeDoc(doc);
        return false;
    }

    // loop through the children of ROOT
    //
    ohcmParseXmlNodeChildren(top, func, funcArg);

    // cleanup and return
    //
    xmlFreeDoc(doc);
    return true;
}

/*
 * Helper function to loop through the children of 'node' and call
 * 'func' for each child XML node so it can be examined.
 *
 * @param node - top-level node to iterate through
 * @param func - function to invoke for each node (child of top-level node) within the doc
 * @param funcArg - optional data to pass into 'func'
 */
void ohcmParseXmlNodeChildren(xmlNodePtr node, ohcmParseXmlNodeCallback func, void *funcArg)
{
    if (node == NULL)
    {
        return;
    }

    // loop through the children of 'node'
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

        // forward to 'func'
        //
        if (func(node->name, currNode, funcArg) == false)
        {
            // told to stop
            //
            break;
        }
    }
}


