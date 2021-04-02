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

/**
 * defining the IANA HTTP Status codes. This is based on the CSV found at
 * https://www.iana.org/assignments/http-status-codes/http-status-codes-1.csv
 *
 * This code was derived from IETF RFC 7231.
 * Copyright (c) <2014> IETF Trust and the persons identified as authors of the code. All rights reserved.
 * Licensed under the BSD-3 License
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

#ifndef COMCASTHTTPSTATUS_H
#define COMCASTHTTPSTATUS_H

#define HTTP_STATUS_CONTINUE                        100
#define HTTP_STATUS_SWITCHING_PROTOCOLS             101
#define HTTP_STATUS_PROCESSING                      102
#define HTTP_STATUS_EARLY_HINTS                     103
#define HTTP_STATUS_OK                              200
#define HTTP_STATUS_CREATED                         201
#define HTTP_STATUS_ACCEPTED                        202
#define HTTP_STATUS_NON_AUTHORITATIVE_INFORMATION   203
#define HTTP_STATUS_NO_CONTENT                      204
#define HTTP_STATUS_RESET_CONTENT                   205
#define HTTP_STATUS_PARTIAL_CONTENT                 206
#define HTTP_STATUS_MULTI_STATUS                    207
#define HTTP_STATUS_ALREADY_REPORTED                208
#define HTTP_STATUS_IM_USED                         226
#define HTTP_STATUS_MULTIPLE_CHOICES                300
#define HTTP_STATUS_MOVED_PERMANENTLY               301
#define HTTP_STATUS_FOUND                           302
#define HTTP_STATUS_SEE_OTHER                       303
#define HTTP_STATUS_NOT_MODIFIED                    304
#define HTTP_STATUS_USE_PROXY                       305
#define HTTP_STATUS_(UNUSED)                        306
#define HTTP_STATUS_TEMPORARY_REDIRECT              307
#define HTTP_STATUS_PERMANENT_REDIRECT              308
#define HTTP_STATUS_BAD_REQUEST                     400
#define HTTP_STATUS_UNAUTHORIZED                    401
#define HTTP_STATUS_PAYMENT_REQUIRED                402
#define HTTP_STATUS_FORBIDDEN                       403
#define HTTP_STATUS_NOT_FOUND                       404
#define HTTP_STATUS_METHOD_NOT_ALLOWED              405
#define HTTP_STATUS_NOT_ACCEPTABLE                  406
#define HTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED   407
#define HTTP_STATUS_REQUEST_TIMEOUT                 408
#define HTTP_STATUS_CONFLICT                        409
#define HTTP_STATUS_GONE                            410
#define HTTP_STATUS_LENGTH_REQUIRED                 411
#define HTTP_STATUS_PRECONDITION_FAILED             412
#define HTTP_STATUS_PAYLOAD_TOO_LARGE               413
#define HTTP_STATUS_URI_TOO_LONG                    414
#define HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE          415
#define HTTP_STATUS_RANGE_NOT_SATISFIABLE           416
#define HTTP_STATUS_EXPECTATION_FAILED              417
#define HTTP_STATUS_MISDIRECTED_REQUEST             421
#define HTTP_STATUS_UNPROCESSABLE_ENTITY            422
#define HTTP_STATUS_LOCKED                          423
#define HTTP_STATUS_FAILED_DEPENDENCY               424
#define HTTP_STATUS_TOO_EARLY                       425
#define HTTP_STATUS_UPGRADE_REQUIRED                426
#define HTTP_STATUS_PRECONDITION_REQUIRED           428
#define HTTP_STATUS_TOO_MANY_REQUESTS               429
#define HTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE 431
#define HTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS   451
#define HTTP_STATUS_INTERNAL_SERVER_ERROR           500
#define HTTP_STATUS_NOT_IMPLEMENTED                 501
#define HTTP_STATUS_BAD_GATEWAY                     502
#define HTTP_STATUS_SERVICE_UNAVAILABLE             503
#define HTTP_STATUS_GATEWAY_TIMEOUT                 504
#define HTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED      505
#define HTTP_STATUS_VARIANT_ALSO_NEGOTIATES         506
#define HTTP_STATUS_INSUFFICIENT_STORAGE            507
#define HTTP_STATUS_LOOP_DETECTED                   508
#define HTTP_STATUS_NOT_EXTENDED                    510
#define HTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED 511

#define HTTP_STATUS_IS_INFORMATIONAL(_stat_code)   ((_stat_code >= 100) && (_stat_code < 200))
#define HTTP_STATUS_IS_SUCCESSFUL(_stat_code)      ((_stat_code >= 200) && (_stat_code < 300))
#define HTTP_STATUS_IS_REDIRECTION(_stat_code)     ((_stat_code >= 300) && (_stat_code < 400))
#define HTTP_STATUS_IS_CLIENT_ERROR(_stat_code)    ((_stat_code >= 400) && (_stat_code < 500))
#define HTTP_STATUS_IS_SERVER_ERROR(_stat_code)    ((_stat_code >= 500) && (_stat_code < 600))
#define HTTP_STATUS_IS_ERROR(_stat_code)           (_stat_code >= 400)

typedef enum {
    HTTP_STATUS_IS_INFORMATIONAL,
    HTTP_STATUS_IS_SUCCESSFUL,
    HTTP_STATUS_IS_REDIRECTION,
    HTTP_STATUS_IS_CLIENT_ERROR,
    HTTP_STATUS_IS_SERVER_ERROR,
    HTTP_STATUS_IS_ERROR
} httpStatusClassifier;

/*
 * simple inline function to classify an http status code according to the httpStatusClassifier enum
 */
static inline httpStatusClassifier getHttpStatusClassifier(int stat_code)
{
    httpStatusClassifier retval = HTTP_STATUS_IS_ERROR;
    if ((stat_code >= 100) && (stat_code < 200))
    {
        retval = HTTP_STATUS_IS_INFORMATIONAL;
    }
    else if ((stat_code >= 200) && (stat_code < 300))
    {
        retval = HTTP_STATUS_IS_SUCCESSFUL;
    }
    else if ((stat_code >= 300) && (stat_code < 400))
    {
        retval = HTTP_STATUS_IS_REDIRECTION;
    }
    else if ((stat_code >= 400) && (stat_code < 500))
    {
        retval = HTTP_STATUS_IS_CLIENT_ERROR;
    }
    else if ((stat_code >= 500) && (stat_code < 600))
    {
        retval = HTTP_STATUS_IS_SERVER_ERROR;
    }
    return retval;
}

#endif /* COMCASTHTTPSTATUS_H */ 


