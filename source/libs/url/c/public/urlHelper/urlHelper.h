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

#ifndef ZILKER_URLHELPER_H
#define ZILKER_URLHELPER_H

#include <stdint.h>
#include <curl/curl.h>

#include <icTypes/icStringHashMap.h>
#include <icTypes/icLinkedList.h>
#include <propsMgr/sslVerify.h>

//A struct for encapsulating information about file data for multipart http posts
typedef struct
{
    char *partName;         //The name of the part for this file
    char *localFilePath;    //The path to the local file
    char *remoteFileName;   //The name to use for the file on the remote side
    char *contentType;      //The content type of the file (ex. "text/plain", "application/x-tar-gz", etc.)
} MimeFileInfo;

MimeFileInfo *createMimeFileInfo();
void destroyMimeFileInfo(MimeFileInfo *fileInfo);

//A struct for encapsulating information about regular part data for multipart http posts
typedef struct
{
    char *partName;         //The name of a part
    char *partData;         //A string representation of data to be the body of a part
    size_t dataLength;      //The length of the data block (curl expects size_t)
} MimePartInfo;

MimePartInfo *createMimePartInfo();
void destroyMimePartInfo(MimePartInfo *partInfo);

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
                                      bool allowCellular);

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
                              bool allowCellular);

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
                                     bool allowCellular);

/*
 * same as urlHelperExecuteMultipartRequestHeaders, but without headers
 */
char *urlHelperExecuteMultipartRequest(const char* url,
                                       long* httpCode,
                                       icLinkedList* plainParts,
                                       icLinkedList* fileInfo,
                                       const char* username,
                                       const char* password,
                                       uint32_t timeoutSecs,
                                       sslVerify verifyFlag,
                                       bool allowCellular);

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
char *urlHelperExecuteMultipartRequestHeaders(const char* url,
                                              long* httpCode,
                                              icLinkedList* plainParts,
                                              icLinkedList* fileInfo,
                                              icLinkedList* headerStrings,
                                              const char* username,
                                              const char* password,
                                              uint32_t timeoutSecs,
                                              sslVerify verifyFlag,
                                              bool allowCellular);

/**
 * Helper routine to download a file into a specified
 * location.
 *
 * @param url The URL pointing to data to download
 * @param httpCode The returned HTTP response code
 * @param username The username to authorize against
 * @param password The password to use in authorization
 * @param timeoutSecs The number of seconds before timing out.
 * @param verifyFlag SSL verification turned on or off.
 * @param allowCellular Allow this download over a cellular connection
 * @param pathname The path and file name to store the file into.
 * @return The number of bytes written into the file.
 */
size_t urlHelperDownloadFile(const char* url,
                             long* httpCode,
                             const char* username,
                             const char* password,
                             uint32_t timeoutSecs,
                             sslVerify verifyFlag,
                             bool allowCellular,
                             const char* pathname);

/*
 * apply standard options to a Curl Context.
 * if the 'url' is not null, this will add it to the context.
 * additionally, if the 'verifyFlag' includes VERIFY_HOST or VERIFY_BOTH, then the
 * 'url' will be checked for IP Addresses, and if so remove VERIFY_HOST from the mix
 */
void applyStandardCurlOptions(CURL* context, const char *url, uint32_t timeoutSecs, sslVerify verifyFlag, bool allowCellular);

/*
 * returns if VERIFY_HOST is possible on the supplied url string.  this is a simple check to
 * handle "IP Address" based url strings as those cannot be used in a VERIFY_HOST situation.
 */
bool urlHelperCanVerifyHost(const char *urlStr);

/**
 * Cancel a transfer by URL. If the transfer is not active,
 * the next request will be aborted immediately.
 * @param enabled
 */
void urlHelperCancel(const char *url);

#endif //ZILKER_URLHELPER_H
