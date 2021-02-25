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
 * common.h
 *
 * Set of common values used throughout the
 * rdkIntegrationService source files.
 *
 *-----------------------------------------------*/

#ifndef RDK_INTEGRATION_COMMON_H
#define RDK_INTEGRATION_COMMON_H

#include <stdbool.h>

/*
 * logging "category name" used within the service
 */
#define LOG_TAG "rdkIntSvc"

/*
 * XHFW.xml parameter names
 */

#define XHFW_PARAM_NAME_ACTIVATE        "Activate"
#define XHFW_PARAM_NAME_STATUS          "Status"
#define XHFW_PARAM_NAME_SAT             "SAT"
#define XHFW_PARAM_NAME_RESET           "ResetToFactory"
#define XHFW_PARAM_NAME_RESTART         "Restart"
#define XHFW_PARAM_NAME_AWS_ENABLED     "AWSIoTEnabled"
#define XHFW_PARAM_NAME_USERVER_ENABLED "UServerEnabled"
#define XHFW_PARAM_NAME_WHITELIST_URL   "WhitelistURL"
#define XHFW_PARAM_NAME_AWS_ENDPOINT    "AWSEndpoint"
#define XHFW_PARAM_NAME_FW_DOWNLOAD_URL "FirmwareDownloadURL"
#define XHFW_PARAM_NAME_XPKI_CERT_ISSUER_CA_NAME "xPKICertIssuerCAName"
/*
 * Callback prototypes between RDK Integration Service and XHFW SSP
 */

typedef bool
(*PFN_GET_BOOL)
    (
        void
    );

typedef bool
(*PFN_SET_BOOL)
    (
        bool value
    );

typedef char *
(*PFN_GET_STRING)
    (
        void
    );

typedef bool
(*PFN_SET_STRING)
    (
        const char * value
    );

typedef bool
(*PFN_BOOL_TRIGGER)
    (
        void
    );

typedef struct _xhfw_ssp_callbacks_t
{
    PFN_SET_STRING          pfnSetActivate;
    PFN_GET_STRING          pfnGetStatus;
    PFN_SET_STRING          pfnSetSAT;
    PFN_BOOL_TRIGGER        pfnResetToFactory;
    PFN_BOOL_TRIGGER        pfnRestart;
    PFN_GET_BOOL            pfnGetAwsIotEnabled;
    PFN_SET_BOOL            pfnSetAwsIotEnabled;
    PFN_GET_BOOL            pfnGetUServerEnabled;
    PFN_SET_BOOL            pfnSetUServerEnabled;
    PFN_SET_STRING          pfnSetWhitelistURL;
    PFN_GET_STRING          pfnGetWhitelistURL;
    PFN_SET_STRING          pfnSetAWSEndpoint;
    PFN_GET_STRING          pfnGetAWSEndpoint;
    PFN_SET_STRING          pfnSetFirmwareDLURL;
    PFN_GET_STRING          pfnGetFirmwareDLURL;
    PFN_SET_STRING          pfnSetxPKICertIssuerCAName;
    PFN_GET_STRING          pfnGetxPKICertIssuerCAName;
} xhfw_ssp_callbacks_t;

/*
 * Register the XHFW component on the CCSP bus
 */
int msgBusInit
    (
        xhfw_ssp_callbacks_t * pCallbacks
    );

/*
 * Connect to parodus
 */
void
Init_Parodus_Task(void);

/*
 * Send notification to libparodus
 */
void
Send_Notification_Task
    (
            int activation_code,
            const char * activation_status,
            const char * partner_id,
            const char * trace_id,
            const char * xbo_account_id
    );

#endif //RDK_INTEGRATION_COMMON_H
