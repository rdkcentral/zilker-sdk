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
 * Created by Thomas Lea on 8/13/15.
 */

#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
#include <curl/curl.h>
#include <pthread.h>
#include <inttypes.h>

#include <icBuildtime.h>
#include <icSystem/hardwareCapabilities.h>
#include <icTypes/icStringHashMap.h>
#include <icUtil/stringUtils.h>
#include <icLog/logging.h>
#include <propsMgr/paths.h>

#ifdef CONFIG_SERVICE_PKI
#include <pkiService/pkiService_ipc.h>
#include <pkiService/pkiService_eventAdapter.h>
#endif

#include <icConcurrent/delayedTask.h>
#include <icConcurrent/threadUtils.h>
#include "urlHelper/urlHelper.h"

#define LOG_TAG "urlHelper"

#define CONNECT_TIMEOUT          15
#define CELLULAR_CONNECT_TIMEOUT 30

pthread_mutex_t cancelMtx;
icStringHashMap *cancelUrls;

pthread_once_t initOnce = PTHREAD_ONCE_INIT;

#ifdef CONFIG_SERVICE_PKI

#define DEFAULT_SECRET_EXPIRE_S  60

/* This mtx is initialized by the one-time initializer 'urlHelperInit' */
static pthread_mutex_t pkiConfigMtx;
static char *keystorePassword;
static char *keystorePath;
static uint16_t secretsExpireSecs = DEFAULT_SECRET_EXPIRE_S;
static uint32_t secretsExpireHandle = 0;
static void clearKeystorePasswordLocked();
static void setKeystorePasswordLocked(const char *password);

static void setmTLSOptions(CURL *context);

#endif /* CONFIG_SERVICE_PKI */

typedef struct
{
    char *ptr;
    size_t len;
} ResponseBuffer;

typedef struct fileDownloadData
{
    FILE* fout;
    size_t size;
} fileDownloadData;

/**
  * Internal function for running the cURL request. When mTLS is enabled,
  * this will try again without mTLS automatically when the client certificate is unusable.
  * @param curl The cURL context used by the cURL library
  * @param reportOnFailure If true then report that a network connectivity issue may have
  *                        occurred to the system reporting component.
  * @return the HTTP response status code
  * @note the return value may be < 200 if no response is received or the connection is interrupted.
  */
static long performRequest(CURL *curl, bool reportOnFailure);
static inline CURL *urlHelperCreateCurl(void);
static inline void urlHelperDestroyCurl(CURL *ctx);

#ifdef CONFIG_SERVICE_PKI
/**
 * Erase and free the keystore password
 */
static void clearKeystorePasswordLocked()
{
    /* Keep the secret from floating around in process memory */
    if (keystorePassword != NULL)
    {
        memset(keystorePassword, 0, strlen(keystorePassword));
        free(keystorePassword);
        keystorePassword = NULL;
    }
}

static void secretsExpireTask(void *unused)
{
    LOCK_SCOPE(pkiConfigMtx);

    icLogInfo(LOG_TAG, "PKI passphrase timer expired. Unloading credential.");
    clearKeystorePasswordLocked();
    secretsExpireHandle = 0;
}

 /**
  * Set the mTLS keystore password and arm the self-destruct timer.
  * @param password
  */
static void setKeystorePasswordLocked(const char *password)
{
    clearKeystorePasswordLocked();

    if (password != NULL)
    {
        keystorePassword = strdup(password);
    }

    if (secretsExpireHandle == 0)
    {
        secretsExpireHandle = scheduleDelayTask(secretsExpireSecs, DELAY_SECS, secretsExpireTask, NULL);
    }
    else
    {
        rescheduleDelayTask(secretsExpireHandle, secretsExpireSecs, DELAY_SECS);
    }
}

static void loadPKIConfig(void)
{
    AUTO_CLEAN(pojo_destroy__auto) PKIConfig *config = create_PKIConfig();

    IPCCode rc = pkiService_request_GET_CONFIG(false, config);
    if (rc == IPC_SUCCESS)
    {
        LOCK_SCOPE(pkiConfigMtx);

        if (keystorePassword == NULL)
        {
            setKeystorePasswordLocked(config->password);
        }

        if (keystorePath == NULL)
        {
            keystorePath = strdup(config->keystore);
        }
    }
    else
    {
        icLogWarn(LOG_TAG, "Unable to get PKI configuration: %s", IPCCodeLabels[rc]);
    }
}

static void onPKIConfigChanged(const PKIConfigChangedEvent *event)
{
    icLogTrace(LOG_TAG,
               "Received PKIConfigChangedEvent; code=[%"PRIi32 "]; reason=%s",
               event->baseEvent.eventCode,
               PKIConfigChangeReasonLabels[event->changeReason]);

    LOCK_SCOPE(pkiConfigMtx);

    switch(event->changeReason)
    {
        case PKI_CONFIG_CHANGE_LOADED:
        case PKI_CONFIG_CHANGE_ISSUED:
            setKeystorePasswordLocked(event->PKIConfig->password);

            free(keystorePath);
            keystorePath = NULL;
            if (event->PKIConfig->keystore != NULL)
            {
                keystorePath = strdup(event->PKIConfig->keystore);
            }
            break;

        case PKI_CONFIG_CHANGE_UNLOADED:
            free(keystorePath);
            keystorePath = NULL;
            clearKeystorePasswordLocked();
            break;

        case PKI_CONFIG_CHANGE_RENEWED:
            /* This implies no configuration changes are needed */
            break;

        default:
            icLogWarn(LOG_TAG, "%s: PKI change reason %d not supported!", __func__, event->changeReason);
            break;
    }
}

#endif /* CONFIG_SERVICE_PKI */

/**
 * Auto initializer: called by pthread_once
 */
static void urlHelperInit(void)
{
    mutexInitWithType(&cancelMtx, PTHREAD_MUTEX_ERRORCHECK);
    cancelUrls = stringHashMapCreate();
    curl_global_init(CURL_GLOBAL_ALL);
    atexit(curl_global_cleanup);
#ifdef CONFIG_SERVICE_PKI
    /*
     * Enabling reentrant locking simplifies on-demand PKI config
     * loading from within applyStandardCurlOptions
     */
    mutexInitWithType(&pkiConfigMtx, PTHREAD_MUTEX_RECURSIVE);

    /*
     * Because there's no 'shutdown' for this auto-init, this will not be unregistered. It's used throughout the
     * process lifecycle and is harmless to leave.
     */
    register_PKIConfigChangedEvent_eventListener((handleEvent_PKIConfigChangedEvent) onPKIConfigChanged);

    loadPKIConfig();
#endif /* CONFIG_SERVICE_PKI */
}

/**
 * Set the client certificate for mTLS when PKI service is enabled and configuration is available.
 * @param context a valid cURL context
 */
static void setmTLSOptions(CURL *context)
{
#ifdef CONFIG_SERVICE_PKI
    LOCK_SCOPE(pkiConfigMtx);

    if (keystorePath != NULL && keystorePassword == NULL)
    {
        loadPKIConfig();
    }

    if (keystorePassword != NULL && keystorePath != NULL)
    {
        icLogInfo(LOG_TAG, "Using mTLS certificate at %s", keystorePath);
        CURLcode err = CURLE_OK;
        if ((err = curl_easy_setopt(context, CURLOPT_SSLCERT, keystorePath)) != CURLE_OK)
        {
            icLogWarn(LOG_TAG, "%s: could not set keystore path: %s", __func__, curl_easy_strerror(err));
        }
        if ((err = curl_easy_setopt(context, CURLOPT_SSLCERTTYPE, "P12")) != CURLE_OK)
        {
            icLogWarn(LOG_TAG, "%s: could not set keystore type to P12: %s", __func__, curl_easy_strerror(err));
        }
        if ((err = curl_easy_setopt(context, CURLOPT_KEYPASSWD, keystorePassword)) != CURLE_OK)
        {
            icLogWarn(LOG_TAG, "%s: could not set keystore password: %s", __func__, curl_easy_strerror(err));
        }
    }
#endif /* CONFIG_SERVICE_PKI */
}

static int curlDebugCallback(CURL *handle,
                             curl_infotype type,
                             char *data,
                             size_t size,
                             void *debugCtx);

static bool initResponseBuffer(ResponseBuffer *buff)
{
    bool result = false;
    buff->len = 0;
    buff->ptr = malloc(buff->len + 1);
    if (buff->ptr == NULL)
    {
        icLogError(LOG_TAG, "failed to allocate response buffer");
    }
    else
    {
        buff->ptr[0] = '\0';
        result = true;
    }

    return result;
}

/* implement the 'curl_write_callback' prototype 
typedef size_t (*curl_write_callback)(char *buffer,
                                      size_t size,
                                      size_t nitems,
                                      void *outstream);
*/
static size_t writefunc(char *ptr, size_t size, size_t nmemb, void *outstream)
{
    ResponseBuffer *buff = (ResponseBuffer *)outstream;
    size_t new_len = buff->len + size * nmemb;
    buff->ptr = realloc(buff->ptr, new_len + 1);
    if (buff->ptr == NULL)
    {
        icLogError(LOG_TAG, "failed to reallocate response buffer");
    }
    else
    {
        memcpy(buff->ptr + buff->len, ptr, size * nmemb);
        buff->ptr[new_len] = '\0';
        buff->len = new_len;
    }

    return size * nmemb;
}

/*
 * Core logic for performing HTTP Request. Allows other callers in the library to presupply a curl context, if
 * additional options are needed (such as in the case of multipart)
 */
static char *urlHelperPerformRequestInternal(CURL* curl,
                                             const char* url,
                                             long* httpCode,
                                             const char* postData,
                                             icLinkedList* headerStrings,
                                             const char* username,
                                             const char* password,
                                             uint32_t timeoutSecs,
                                             sslVerify verifyFlag,
                                             bool allowCellular)
{
    char *result = NULL;

    if (curl)
    {
        ResponseBuffer buff;
        ResponseBuffer *pBuff = &buff;
        if (initResponseBuffer(&buff) == true)
        {
            // apply standard options
            applyStandardCurlOptions(curl, url, timeoutSecs, verifyFlag, allowCellular);

            // set additional options
            if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_URL, url) failed at %s(%d)",__FILE__,__LINE__);
            }
            if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc) failed at %s(%d)",__FILE__,__LINE__);
            }
            if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, pBuff) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_WRITEDATA, pBuff) failed at %s(%d)",__FILE__,__LINE__);
            }

            // apply POST data if supplied
            if (postData != NULL)
            {
                if (curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData) != CURLE_OK)
                {
                    icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData) failed at %s(%d)",__FILE__,__LINE__);
                }
            }

            // apply credentials if supplied
            char *userpass = NULL;
            if(username != NULL && password != NULL)
            {
                userpass = (char*)malloc(strlen(username) + 1 + strlen(password) + 1);
                if(userpass == NULL)
                {
                    icLogError(LOG_TAG, "Failed to allocate userpass");
                    free(buff.ptr);
                    return NULL;
                }
                sprintf(userpass, "%s:%s", username, password);
                if (curl_easy_setopt(curl, CURLOPT_USERPWD, userpass) != CURLE_OK)
                {
                    icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_USERPWD, userpass) failed at %s(%d)",__FILE__,__LINE__);
                }
            }

            // apply HTTP headers if supplied
            struct curl_slist *header = NULL;
            if (linkedListCount(headerStrings) > 0)
            {
                // add each string
                //
                icLinkedListIterator *loop = linkedListIteratorCreate(headerStrings);
                while (linkedListIteratorHasNext(loop) == true)
                {
                    char *string = (char *)linkedListIteratorGetNext(loop);
                    if (string != NULL)
                    {
                        header = curl_slist_append(header, string);
                    }
                }
                linkedListIteratorDestroy(loop);
                if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header) != CURLE_OK)
                {
                    icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header) failed at %s(%d)",__FILE__,__LINE__);
                }
            }

            *httpCode = performRequest(curl, allowCellular);

            if (header != NULL)
            {
                curl_slist_free_all(header);
            }
            result = buff.ptr;

            // cleanup
            if(userpass != NULL)
            {
                free(userpass);
            }
        }

    }
    return result;
}

static long performRequest(CURL *curl, bool reportOnFailure)
{
    long httpCode = 0;
    if (curl == NULL)
    {
        return 0;
    }

    CURLcode curlcode = curl_easy_perform(curl);

    /* This code is only emitted when mTLS is enabled and the client cert is invalid */
    if (curlcode == CURLE_SSL_CERTPROBLEM)
    {
        if (curl_easy_setopt(curl, CURLOPT_SSLCERT, NULL) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_SSLCERT, NULL) failed at %s(%d)",__FILE__,__LINE__);
        }
        icLogWarn(LOG_TAG, "cURL could not use mTLS certificate; attempting unsigned request");
        curlcode = curl_easy_perform(curl);
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    if (curlcode != CURLE_OK)
    {
        icLogDebug(LOG_TAG,
                   "Error performing HTTP request. HTTP status: [%ld]; Error code: [%d][%s]",
                   httpCode,
                   curlcode,
                   curl_easy_strerror(curlcode));

        if (reportOnFailure) {
            switch (curlcode) {
                case CURLE_COULDNT_CONNECT:
                case CURLE_COULDNT_RESOLVE_HOST:
                case CURLE_COULDNT_RESOLVE_PROXY:
                case CURLE_BAD_DOWNLOAD_RESUME:
                case CURLE_INTERFACE_FAILED:
                case CURLE_GOT_NOTHING:
                case CURLE_NO_CONNECTION_AVAILABLE:
                case CURLE_OPERATION_TIMEDOUT:
                case CURLE_PARTIAL_FILE:
                case CURLE_READ_ERROR:
                case CURLE_RECV_ERROR:
                case CURLE_SEND_ERROR:
                case CURLE_SEND_FAIL_REWIND:
                case CURLE_SSL_CONNECT_ERROR:
                case CURLE_UPLOAD_FAILED:
                case CURLE_WRITE_ERROR:
                    icLogDebug(LOG_TAG, "Reporting network connectivity concerns.");
//                    reportNetworkConnectivity();
                    break;
                default:
                    break;
            }
        }
    }

    return httpCode;
}

/*
 * Execute a request to a web server and return the resulting body.
 * Caller should free the return string (if not NULL)
 *
 * @param requestUrl - the URL for the request that can contain variables
 * @param httpCode - the result code from the server for the HTTP request
 * @param postData - text to use for an HTTP POST operation or NULL to use a standard HTTP GET
 * @param variableMap - the string map of variable names to values used to substitute in the requestUri
 * @param username - the username to use for basic authentication or NULL for none
 * @param password - the password to use for basic authentication or NULL for none
 * @param timeoutSecs - number of seconds to timeout (0 means no timeout set)
 */
char* urlHelperExecuteVariableRequest(char* requestUrl,
                                      icStringHashMap* variableMap,
                                      long* httpCode,
                                      char* postData,
                                      const char* username,
                                      const char* password,
                                      uint32_t timeoutSecs,
                                      sslVerify verifyFlag,
                                      bool allowCellular)
{
    char *result = NULL;

    if (requestUrl == NULL || variableMap == NULL)
    {
        icLogError(LOG_TAG, "executeVariableRequest: invalid args");
        return NULL;
    }

    char *updatedUri = requestUrl;
    char *updatedPostData = postData;

    //for each variable in the variable map, search and replace all occurrences in the requestUri and the postData (if it was provided)
    icStringHashMapIterator *iterator = stringHashMapIteratorCreate(variableMap);
    while (stringHashMapIteratorHasNext(iterator))
    {
        char *key;
        char *value;
        stringHashMapIteratorGetNext(iterator, &key, &value);
        char *tmp = stringReplace(updatedUri, key, value);
        if(updatedUri != requestUrl)
        {
            free(updatedUri);
        }
        updatedUri = tmp;

        if(postData != NULL)
        {
            tmp = stringReplace(updatedPostData, key, value);
            if(updatedPostData != postData)
            {
                free(updatedPostData);
            }
            updatedPostData = tmp;
        }
    }
    stringHashMapIteratorDestroy(iterator);

    result = urlHelperExecuteRequest(updatedUri,
                                     httpCode,
                                     updatedPostData,
                                     username,
                                     password,
                                     timeoutSecs,
                                     verifyFlag,
                                     allowCellular);
    if(updatedUri != requestUrl)
    {
        free(updatedUri);
    }
    if(postData != NULL && updatedPostData != postData)
    {
        free(updatedPostData);
    }

    return result;
}

/*
 * Execute a simple request to the provided URL and return the body. If username and password
 * are provided, basic authentication will be used.
 * Caller should free the return string (if not NULL)
 *
 * @param url - the URL to retrieve
 * @param httpCode - the result code from the server for the HTTP request
 * @param postData - text to use for an HTTP POST operation or NULL to use a standard HTTP GET
 * @param username - the username to use for basic authentication or NULL for none
 * @param password - the password to use for basic authentication or NULL for none
 * @param timeoutSecs - number of seconds to timeout (0 means no timeout set)
 *
 * @returns the body of the response.  Caller frees.
 */
char* urlHelperExecuteRequest(const char* url,
                              long* httpCode,
                              const char* postData,
                              const char* username,
                              const char* password,
                              uint32_t timeoutSecs,
                              sslVerify verifyFlag,
                              bool allowCellular)
{
    return urlHelperExecuteRequestHeaders(url,
                                          httpCode,
                                          postData,
                                          NULL,
                                          username,
                                          password,
                                          timeoutSecs,
                                          verifyFlag,
                                          allowCellular);
}

/*
 * same as urlHelperExecuteRequest, but allow for assigning HTTP headers in the request.
 * For ex:  Accept: application/json
 *
 * @param headers - list of strings that define header values.  ex: "Content-Type: text/xml; charset=utf-8".  ignored if NULL
 */
char* urlHelperExecuteRequestHeaders(const char* url,
                                     long* httpCode,
                                     const char* postData,
                                     icLinkedList* headerStrings,
                                     const char* username,
                                     const char* password,
                                     uint32_t timeoutSecs,
                                     sslVerify verifyFlag,
                                     bool allowCellular)
{
    CURL *ctx = urlHelperCreateCurl();
    char *result = urlHelperPerformRequestInternal(ctx,
                                           url,
                                           httpCode,
                                           postData,
                                           headerStrings,
                                           username,
                                           password,
                                           timeoutSecs,
                                           verifyFlag,
                                           allowCellular);
    urlHelperDestroyCurl(ctx);

    return result;
}

/*
 * same as urlHelperExecuteMultipartRequestHeaders, but without headers
 */
char* urlHelperExecuteMultipartRequest(const char* url,
                                       long* httpCode,
                                       icLinkedList* plainParts,
                                       icLinkedList* fileInfo,
                                       const char* username,
                                       const char* password,
                                       uint32_t timeoutSecs,
                                       sslVerify verifyFlag,
                                       bool allowCellular)
{
    return urlHelperExecuteMultipartRequestHeaders(url,
                                                   httpCode,
                                                   plainParts,
                                                   fileInfo,
                                                   NULL,
                                                   username,
                                                   password,
                                                   timeoutSecs,
                                                   verifyFlag,
                                                   allowCellular);
}

/*
 * Performs a multipart POST request using the information passed. It is up to the caller to free all memory passed.
 *
 * @param url               The url to perform the POST request to
 * @param httpCode          A pointer to the httpCode returned by the request.
 * @param plainParts        A list of MimePartInfo types containing key/value string part information.
 * @param fileInfo          A list of MimeFileInfo types containing file information for local files. Each entry in
*                           the list will be a separate part in the HTTP request, with the file data being the
*                           part's body.
 * @param headerStrings     A list of header strings for the request
 * @param username          A username to provide the server for authentication
 * @param password          A password to provide the server for authentication
 * @param timeoutSecs       Number of seconds to wait before timeout
 * @param verifyFlag        SSL verification flag
 * @param allowCellular     A flag to indicate if this request should be sent out over cell iff it cannot be sent
 *                          over bband
 * @return The body of the response from the server.
 */
char* urlHelperExecuteMultipartRequestHeaders(const char* url,
                                              long* httpCode,
                                              icLinkedList* plainParts,
                                              icLinkedList* fileInfo,
                                              icLinkedList* headerStrings,
                                              const char* username,
                                              const char* password,
                                              uint32_t timeoutSecs,
                                              sslVerify verifyFlag,
                                              bool allowCellular)
{
    char *result = NULL;

    //We are going to construct the curl context so we can fill it with our multipart data.
    CURL *ctx = urlHelperCreateCurl();
    if (ctx)
    {
        //Init mime
        curl_mime *requestBody = curl_mime_init(ctx);

        if (plainParts != NULL)
        {
            //Iterate over parts and add each one to the mime part
            icLinkedListIterator *partIter = linkedListIteratorCreate(plainParts);
            while (linkedListIteratorHasNext(partIter))
            {
                MimePartInfo *partInfo = (MimePartInfo *) linkedListIteratorGetNext(partIter);

                curl_mimepart *part = curl_mime_addpart(requestBody);
                curl_mime_name(part, partInfo->partName);
                curl_mime_data(part, partInfo->partData, partInfo->dataLength);
            }

            linkedListIteratorDestroy(partIter);
        }

        if (fileInfo != NULL)
        {
            //Iterate over the file parts and add each one to the mime part
            icLinkedListIterator *partIter = linkedListIteratorCreate(fileInfo);
            while (linkedListIteratorHasNext(partIter))
            {
                MimeFileInfo *file = (MimeFileInfo*) linkedListIteratorGetNext(partIter);

                //Make sure they supplied the bare mimimum information
                if (file->partName != NULL && file->localFilePath != NULL)
                {
                    curl_mimepart *part = curl_mime_addpart(requestBody);

                    curl_mime_name(part, file->partName);
                    curl_mime_filedata(part, file->localFilePath);

                    //Add a content-type if it was specified
                    if (file->contentType != NULL)
                    {
                        curl_mime_type(part, file->contentType);
                    }

                    //See if the caller wants to have a custom remote filename
                    if (file->remoteFileName != NULL)
                    {
                        curl_mime_filename(part, file->remoteFileName);
                    }
                }
            }

            linkedListIteratorDestroy(partIter);
        }

        if (curl_easy_setopt(ctx, CURLOPT_MIMEPOST, requestBody) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(ctx, CURLOPT_MIMEPOST, requestBody) failed at %s(%d)",__FILE__,__LINE__);
        }
        //Since multipart can be a large request, setting this bit will cause libcurl to handle exepct: 100 scenarios properly.
        if (curl_easy_setopt(ctx, CURLOPT_FAILONERROR, 1L) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(ctx, CURLOPT_FAILONERROR, 1L) failed at %s(%d)",__FILE__,__LINE__);
        }

        //Perform the operation and get the result
        result = urlHelperPerformRequestInternal(ctx,
                                                 url,
                                                 httpCode,
                                                 NULL,
                                                 headerStrings,
                                                 username,
                                                 password,
                                                 timeoutSecs,
                                                 verifyFlag,
                                                 allowCellular);

        //Cleanup
        curl_mime_free(requestBody);
        urlHelperDestroyCurl(ctx);
    }

    return result;
}

static size_t download_func(void* ptr, size_t size, size_t nmemb, void* stream)
{
    fileDownloadData* data = stream;
    size_t written;

    written = fwrite(ptr, size, nmemb, data->fout);

    /* fwrite returns the number of "items" (as in the number of 'nmemb').
     * Thus to get the correct number of bytes we must multiply by the size.
     */
    data->size += written * size;

    return written;
}

size_t urlHelperDownloadFile(const char* url,
                             long* httpCode,
                             const char* username,
                             const char* password,
                             uint32_t timeoutSecs,
                             sslVerify verifyFlag,
                             bool allowCellular,
                             const char* pathname)
{
    CURL *curl = urlHelperCreateCurl();
    fileDownloadData data;
    fileDownloadData *pData = &data;

    data.size = 0;

    if (curl)
    {
        data.fout = fopen(pathname, "w");
        if (data.fout != NULL)
        {
            // apply standard options
            applyStandardCurlOptions(curl, url, timeoutSecs, verifyFlag, allowCellular);

            // set additional options
            if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_URL, url) failed at %s(%d)",__FILE__,__LINE__);
            }
            if (curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_func) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_func) failed at %s(%d)",__FILE__,__LINE__);
            }
            if (curl_easy_setopt(curl, CURLOPT_WRITEDATA, pData) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_WRITEDATA, pData) failed at %s(%d)",__FILE__,__LINE__);
            }

            // apply credentials if supplied
            char *userpass = NULL;
            if(username != NULL && password != NULL)
            {
                userpass = (char*)malloc(strlen(username) + 1 + strlen(password) + 1);
                if(userpass == NULL)
                {
                    icLogError(LOG_TAG, "Failed to allocate userpass");
                    urlHelperDestroyCurl(curl);
                    return 0;
                }
                sprintf(userpass, "%s:%s", username, password);
                if (curl_easy_setopt(curl, CURLOPT_USERPWD, userpass) != CURLE_OK)
                {
                    icLogError(LOG_TAG,"curl_easy_setopt(curl, CURLOPT_USERPWD, userpass) failed at %s(%d)",__FILE__,__LINE__);
                }
            }

            *httpCode = performRequest(curl, allowCellular);

            free(userpass);

            fflush(data.fout);
            fclose(data.fout);
        }

        urlHelperDestroyCurl(curl);
    }

    return data.size;
}


static int onCurlXferInfo(void *userData, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
    /* cURL < 7.68 does not support CURL_PROGRESSFUNC_CONTINUE */
#if LIBCURL_VERSION_NUM < 0x074400L
    int action = 0;
#else
    int action = CURL_PROGRESSFUNC_CONTINUE;
#endif


    LOCK_SCOPE(cancelMtx);
    if (stringHashMapCount(cancelUrls) > 0 &&
        stringHashMapDelete(cancelUrls, userData, NULL) == true)
    {
        /* Any nonzero will abort */
        action = 1;
    }

    return action;
}

void urlHelperCancel(const char *url)
{
    pthread_once(&initOnce, urlHelperInit);
    LOCK_SCOPE(cancelMtx);
    stringHashMapPutCopy(cancelUrls, url, NULL);
}

/*
 * apply standard options to a Curl Context.
 * if the 'url' is not null, this will add it to the context.
 * additionally, if the 'verifyFlag' includes VERIFY_HOST or VERIFY_BOTH, then the
 * 'url' will be checked for IP Addresses, and if so remove VERIFY_HOST from the mix
 */
void applyStandardCurlOptions(CURL* context, const char *url, uint32_t timeoutSecs, sslVerify verifyFlag, bool allowCellular)
{
    pthread_once(&initOnce, urlHelperInit);

    setmTLSOptions(context);

    // apply the 'verify' settings based on the flag
    //
    if (verifyFlag == SSL_VERIFY_HOST || verifyFlag == SSL_VERIFY_BOTH)
    {
        // before applying the VERIFY_HOST, check the url to see if it is a hostname or IP address
        //
        if (url != NULL && urlHelperCanVerifyHost(url) == false)
        {
            icLogInfo(LOG_TAG, "Disabling SSL_VERIFY_HOST, url %s appears to be an IP address", url);
            if (curl_easy_setopt(context, CURLOPT_SSL_VERIFYHOST, 0L) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_SSL_VERIFYHOST, 0L) failed at %s(%d)",__FILE__,__LINE__);
            }
        }
        else
        {
            // 2 is necessary to enable host verify.
            // 1 is a deprecated argument that used to be used for debugging but now does nothing.
            if (curl_easy_setopt(context, CURLOPT_SSL_VERIFYHOST, 2L) != CURLE_OK)
            {
                icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_SSL_VERIFYHOST, 2L) failed at %s(%d)",__FILE__,__LINE__);
            }
        }
    }
    else
    {
        if (curl_easy_setopt(context, CURLOPT_SSL_VERIFYHOST, 0L) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_SSL_VERIFYHOST, 0L) failed at %s(%d)",__FILE__,__LINE__);
        }
    }
    if (verifyFlag == SSL_VERIFY_PEER || verifyFlag == SSL_VERIFY_BOTH)
    {
        if (curl_easy_setopt(context, CURLOPT_SSL_VERIFYPEER, 1L) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_SSL_VERIFYPEER, 1L) failed at %s(%d)",__FILE__,__LINE__);
        }
    }
    else
    {
        if (curl_easy_setopt(context, CURLOPT_SSL_VERIFYPEER, 0L) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_SSL_VERIFYPEER, 0L) failed at %s(%d)",__FILE__,__LINE__);
        }
    }

    if (allowCellular)
    {
        if (curl_easy_setopt(context, CURLOPT_SOCKOPTDATA, NULL) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_SOCKOPTDATA, NULL) failed at %s(%d)",__FILE__,__LINE__);
        }
    }

    // set the input URL
    //
    if (url != NULL)
    {
        if (curl_easy_setopt(context, CURLOPT_URL, url) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_URL, url) failed at %s(%d)",__FILE__,__LINE__);
        }
    }

    // follow any redirection (302?)
    //
    if (curl_easy_setopt(context, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK)
    {
        icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_FOLLOWLOCATION, 1L) failed at %s(%d)",__FILE__,__LINE__);
    }

    // bail if there's an error
    //
    if (curl_easy_setopt(context, CURLOPT_FAILONERROR, 1L) != CURLE_OK)
    {
        icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_FAILONERROR, 1L) failed at %s(%d)",__FILE__,__LINE__);
    }

    // prevent curl from calling SIGABRT if we are trying to communicate with
    // a device that won't let us negotiate SSL or login properly
    //
    if (curl_easy_setopt(context, CURLOPT_NOSIGNAL, 1) != CURLE_OK)
    {
        icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_NOSIGNAL, 1) failed at %s(%d)",__FILE__,__LINE__);
    }

    // disable the global DNS cache
    //
    if (curl_easy_setopt(context, CURLOPT_DNS_USE_GLOBAL_CACHE, 0L) != CURLE_OK)
    {
        icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_DNS_USE_GLOBAL_CACHE, 0L) failed at %s(%d)",__FILE__,__LINE__);
    }

    // disable DNS caching
    //
    if (curl_easy_setopt(context, CURLOPT_DNS_CACHE_TIMEOUT, 0) != CURLE_OK)
    {
        icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_DNS_CACHE_TIMEOUT, 0) failed at %s(%d)",__FILE__,__LINE__);
    }

    if (isIcLogPriorityTrace() == true)
    {
        // enable verbose output
        if (curl_easy_setopt(context, CURLOPT_VERBOSE, 1L) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_VERBOSE, 1L) failed at %s(%d)",__FILE__,__LINE__);
        }
        if (curl_easy_setopt(context, CURLOPT_DEBUGFUNCTION, curlDebugCallback) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_DEBUGFUNCTION, curlDebugCallback) failed at %s(%d)",__FILE__,__LINE__);
        }
    }
    else
    {
        // disable verbose output
        if (curl_easy_setopt(context, CURLOPT_VERBOSE, 0L) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_VERBOSE, 0L) failed at %s(%d)",__FILE__,__LINE__);
        }
    }

#ifdef CONFIG_PRODUCT_ANGELSENVY
    //Point to our built certs bundle if it exists
    AUTO_CLEAN(free_generic__auto) char *trustBundle = getCABundlePath();
    // So, libcurl will prioritize CAINFO file over CAPATH no matter what. This also means that it will try to use the
    // default CAINFO file (on linux, /etc/ssl/certs/ca-certificates.crt) EVEN IF you specify a CAPATH directory. If,
    // for some reason, that CAINFO file doesn't exist, it actually errors out rather than check the CAPATH. It seems to
    // only check the CAPATH iff the specified CAINFO file does not have the desired ca cert.
    if (trustBundle != NULL)
    {
        CURLcode err = curl_easy_setopt(context, CURLOPT_CAINFO, trustBundle);
        if (err != CURLE_OK)
        {
            icLogWarn(LOG_TAG, "%s: could not set CAINFO: %s", __func__, curl_easy_strerror(err));
        }
    }
#endif

    if (curl_easy_setopt(context, CURLOPT_NOPROGRESS, 0) != CURLE_OK)
    {
        icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_NOPROGRESS, 0) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(context, CURLOPT_XFERINFOFUNCTION, onCurlXferInfo) != CURLE_OK)
    {
        icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_XFERINFOFUNCTION, onCurlXferInfo) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(context, CURLOPT_XFERINFODATA, url) != CURLE_OK)
    {
        icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_XFERINFODATA, url) failed at %s(%d)",__FILE__,__LINE__);
    }

    if (timeoutSecs > 0)
    {
        uint32_t connectTimeout = (allowCellular) ? CELLULAR_CONNECT_TIMEOUT : CONNECT_TIMEOUT;

        if (connectTimeout > timeoutSecs)
        {
            connectTimeout = timeoutSecs;
        }

        // set the 'socket read' timeout
        //
        if (curl_easy_setopt(context, CURLOPT_TIMEOUT, timeoutSecs) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_TIMEOUT, timeoutSecs) failed at %s(%d)",__FILE__,__LINE__);
        }

        // set the 'socket connect' timeout
        //
        if (curl_easy_setopt(context, CURLOPT_CONNECTTIMEOUT, connectTimeout) != CURLE_OK)
        {
            icLogError(LOG_TAG,"curl_easy_setopt(context, CURLOPT_CONNECTTIMEOUT, connectTimeout) failed at %s(%d)",__FILE__,__LINE__);
        }

    }
}

/*
 * pull the hostname from the url string.
 * caller must free the string.
 */
static char *extractHostFromUrl(const char *urlStr)
{
    // sanity check
    //
    if (stringIsEmpty(urlStr) == true)
    {
        return NULL;
    }

    // in a perfect world, we could leverage "curl_url_get", but that requires libcurl 7.62
    // and we can't do that just yet (due to RDK using 7.60)
    //
    // therefore we'll do this via string manipulation by extracting the characters
    // between the // and the /
    //

    // find the leading '//'
    //
    char *start = strstr(urlStr, "//");
    if (start == NULL)
    {
        // bad url
        //
        return NULL;
    }
    start += 2;

    // find the next '/'
    //
    char *end = strchr(start, '/');
    if (end == NULL)
    {
        // format is probably "https://hostname"
        //
        return strdup(start);
    }

    // calculate the difference between start & end so we create the return string
    //
    int len = 0;
    char *ptr = start;
    while (ptr != end && ptr != NULL)
    {
        len++;
        ptr++;
    }
    char *retVal = calloc(len+1, sizeof(char));
    strncpy(retVal, start, len);
    return retVal;
}

/*
 * returns if VERIFY_HOST is possible on the supplied url string.  this is a simple check to
 * handle "IP Address" based url strings as those cannot be used in a VERIFY_HOST situation.
 */
bool urlHelperCanVerifyHost(const char *urlStr)
{
    // this needs to handle a variety of scenarios:
    //    https://hostname/
    //    https://hostname:port/
    //    https://ipv4/
    //    https://ipv4:port/
    //    https://ipv6/
    //    https://ipv6:port/
    //

    // first, extract the host from the url
    //
    char *hostname = extractHostFromUrl(urlStr);
    if (hostname == NULL)
    {
        return false;
    }

    // ignore the optional ":port" and see if this is an IP address.
    // the easy one is ipv6 because it will have more then 1 colon char in the hostname
    //
    icLogTrace(LOG_TAG, "checking if %s is an ip address", hostname);
    char *firstColon = strchr(hostname, ':');
    if (firstColon != NULL)
    {
        // it could just be the optional port, sp look for a second colon
        //
        char *endColon = strrchr(hostname, ':');
        if (endColon != firstColon)
        {
            // got more then 1 colon, so assume IPv6
            //
            icLogDebug(LOG_TAG, "it appears %s is an IPv6 address; unable to use SSL_VERIFY_HOST", hostname);
            free(hostname);
            return false;
        }
    }

    // if we got here, it's not IPv6, so run a simple regex check for [0-9]* and a '.'
    //
    int rc = 0;
    regex_t exp;

    // simple regex check to see if the url starts with 'https://' and numbers.  ex: 'https://12.'
    // compile the pattern into an regular expression
    //
    if ((rc = regcomp(&exp, "^[0-9]*\\.", REG_ICASE | REG_NOSUB)) != 0)
    {
        // log the reason the regex didn't compile
        //
        char buffer[256];
        regerror(rc, &exp, buffer, sizeof(buffer));
        icLogError(LOG_TAG, "unable to parse regex pattern %s", buffer);
        free(hostname);
        return false;
    }

    // do the comparison
    //
    bool retVal = true;
    if (regexec(&exp, hostname, 0, NULL, 0) == 0)
    {
        // matched
        //
        icLogDebug(LOG_TAG, "it appears %s is an IPv4 address; unable to use SSL_VERIFY_HOST", hostname);
        retVal = false;
    }

    // cleanup
    //
    regfree(&exp);
    free(hostname);
    return retVal;
}

static inline CURL *urlHelperCreateCurl(void)
{
    pthread_once(&initOnce, urlHelperInit);
    return curl_easy_init();
}

static inline void urlHelperDestroyCurl(CURL *ctx)
{
    curl_easy_cleanup(ctx);
}

static const char curlInfoTypes[CURLINFO_END][3] = {
        "* ", "< ", "> ", "{ ", "} ", "{ ", "} "
};

static int curlDebugCallback(CURL *handle,
                             curl_infotype type,
                             char *data,
                             size_t size,
                             void *debugCtx)
{
    switch(type)
    {
        case CURLINFO_TEXT:
        case CURLINFO_HEADER_IN:
        case CURLINFO_HEADER_OUT:
            /* cURL data will have a linefeed that icLog* will also write */
            icLogTrace(LOG_TAG, "cURL: %s%.*s", curlInfoTypes[type], (int) size - 1, data);
            break;

        default:
            break;
    }

    return 0;
}

MimeFileInfo* createMimeFileInfo()
{
    MimeFileInfo *retVal = (MimeFileInfo *) calloc(1, sizeof(MimeFileInfo));

    return retVal;
}

void destroyMimeFileInfo(MimeFileInfo *fileInfo)
{
    if (fileInfo == NULL)
    {
        return;
    }

    if (fileInfo->partName != NULL)
    {
        free(fileInfo->partName);
    }
    if (fileInfo->contentType != NULL)
    {
        free(fileInfo->contentType);
    }
    if (fileInfo->localFilePath != NULL)
    {
        free(fileInfo->localFilePath);
    }
    if (fileInfo->remoteFileName != NULL)
    {
        free(fileInfo->remoteFileName);
    }

    free(fileInfo);
}

MimePartInfo *createMimePartInfo()
{
    MimePartInfo *retVal = (MimePartInfo *) calloc(1, sizeof(MimePartInfo));

    return retVal;
}

void destroyMimePartInfo(MimePartInfo *partInfo)
{
    if (partInfo == NULL)
    {
        return;
    }

    if (partInfo->partName != NULL)
    {
        free(partInfo->partName);
    }
    if (partInfo->partData != NULL)
    {
        free(partInfo->partData);
    }

    free(partInfo);
}

