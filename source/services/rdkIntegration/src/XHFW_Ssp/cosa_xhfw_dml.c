/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
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
*/

#include "common.h"

#include "ansc_platform.h"
#include "cosa_xhfw_dml.h"
#include "ccsp_trace.h"
#include "ccsp_syslog.h"
#include "ccsp_psm_helper.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

extern ANSC_HANDLE bus_handle;//lnt
extern char g_Subsystem[32];//lnt
extern xhfw_ssp_callbacks_t g_xhfw_ssp;

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        XHFW_GetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pParamName,
                char*                       pValue,
                ULONG*                      pUlSize
            );

    description:

        This function is called to retrieve string parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pParamName,
                The parameter name;

                char*                       pValue,
                The string value buffer;

                ULONG*                      pUlSize
                The buffer of length of string value;
                Usually size of 1023 will be used.
                If it's not big enough, put required size here and return 1;

    return:     0 if succeeded;
                1 if short of buffer size; (*pUlSize = required size)
                -1 if not supported.

**********************************************************************/
BOOL
XHFW_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    )
{
    if( AnscEqualString(pParamName, XHFW_PARAM_NAME_STATUS, TRUE))
    {
        if (g_xhfw_ssp.pfnGetStatus != NULL)
        {
            char * status = g_xhfw_ssp.pfnGetStatus();
            if (status != NULL)
            {
                strcpy(pValue, status);
                free(status);
                *pUlSize = AnscSizeOfString(pValue);
                return TRUE;
            }
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_WHITELIST_URL, TRUE))
    {
        if (g_xhfw_ssp.pfnGetWhitelistURL != NULL)
        {
            char * val = g_xhfw_ssp.pfnGetWhitelistURL();
            if (val != NULL)
            {
                strcpy(pValue, val);
                free(val);
                *pUlSize = AnscSizeOfString(pValue);
                return TRUE;
            }
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_AWS_ENDPOINT, TRUE))
    {
        if (g_xhfw_ssp.pfnGetAWSEndpoint != NULL)
        {
            char * val = g_xhfw_ssp.pfnGetAWSEndpoint();
            if (val != NULL)
            {
                strcpy(pValue, val);
                free(val);
                *pUlSize = AnscSizeOfString(pValue);
                return TRUE;
            }
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_FW_DOWNLOAD_URL, TRUE))
    {
        if (g_xhfw_ssp.pfnGetFirmwareDLURL != NULL)
        {
            char * val = g_xhfw_ssp.pfnGetFirmwareDLURL();
            if (val != NULL)
            {
                strcpy(pValue, val);
                free(val);
                *pUlSize = AnscSizeOfString(pValue);
                return TRUE;
            }
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_XPKI_CERT_ISSUER_CA_NAME, TRUE))
    {
        if (g_xhfw_ssp.pfnGetxPKICertIssuerCAName != NULL)
        {
            char * val = g_xhfw_ssp.pfnGetxPKICertIssuerCAName();
            if (val != NULL)
            {
                strcpy(pValue, val);
                free(val);
                *pUlSize = AnscSizeOfString(pValue);
                return TRUE;
            }
        }
    }

    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        XHFW_GetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pParamName,
                BOOL*                       pBool
            );

    description:

        This function is called to retrieve Boolean parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pParamName,
                The parameter name;

                BOOL*                       pBool
                The buffer of returned boolean value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
XHFW_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        BOOL*                       pBool
    )
{
    if( AnscEqualString(pParamName, XHFW_PARAM_NAME_AWS_ENABLED, TRUE))
    {
        if (g_xhfw_ssp.pfnGetAwsIotEnabled != NULL)
        {
            *pBool = g_xhfw_ssp.pfnGetAwsIotEnabled();
            return TRUE;
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_USERVER_ENABLED, TRUE))
    {
        if (g_xhfw_ssp.pfnGetUServerEnabled != NULL)
        {
            *pBool = g_xhfw_ssp.pfnGetUServerEnabled();
            return TRUE;
        }
    }

    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        XHFW_GetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pParamName,
                ULONG*                      puLong
            );

    description:

        This function is called to retrieve ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pParamName,
                The parameter name;

                ULONG*                      puLong
                The buffer of returned ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
XHFW_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        ULONG*                      puLong
    )
{
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        XHFW_GetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pParamName,
                int*                        pInt
            );

    description:

        This function is called to retrieve integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pParamName,
                The parameter name;

                int*                        pInt
                The buffer of returned integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
XHFW_GetParamIntValue
    (
         ANSC_HANDLE                 hInsContext,
        char*                        pParamName,
        int*                       pInt
    )
{
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

       BOOL
       XHFW_SetParamStringValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pParamArray,
                char*                       pString,
            );

    description:

        This function is called to set bulk parameter values; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pParamName,
                The parameter name array;

                char*                       pString,
                The size of the array;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
XHFW_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        char*                       pString
    )
{
    if( AnscEqualString(pParamName, XHFW_PARAM_NAME_SAT, TRUE))
    {
        if (g_xhfw_ssp.pfnSetSAT != NULL)
        {
            return g_xhfw_ssp.pfnSetSAT(pString);
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_ACTIVATE, TRUE))
    {
        if (g_xhfw_ssp.pfnSetActivate != NULL)
        {
            return g_xhfw_ssp.pfnSetActivate(pString);
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_WHITELIST_URL, TRUE))
    {
        if (g_xhfw_ssp.pfnSetWhitelistURL != NULL)
        {
            return g_xhfw_ssp.pfnSetWhitelistURL(pString);
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_AWS_ENDPOINT, TRUE))
    {
        if (g_xhfw_ssp.pfnSetAWSEndpoint != NULL)
        {
            return g_xhfw_ssp.pfnSetAWSEndpoint(pString);
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_FW_DOWNLOAD_URL, TRUE))
    {
        if (g_xhfw_ssp.pfnSetFirmwareDLURL != NULL)
        {
            return g_xhfw_ssp.pfnSetFirmwareDLURL(pString);
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_XPKI_CERT_ISSUER_CA_NAME, TRUE))
    {
        if (g_xhfw_ssp.pfnSetxPKICertIssuerCAName != NULL)
        {
            return g_xhfw_ssp.pfnSetxPKICertIssuerCAName(pString);
        }
    }

    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        XHFW_SetParamBoolValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pParamName,
                BOOL                        bValue
            );

    description:

        This function is called to set BOOL parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pParamName,
                The parameter name;

                BOOL                        bValue
                The updated BOOL value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
XHFW_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        BOOL                        bValue
    )
{
    if( AnscEqualString(pParamName, XHFW_PARAM_NAME_RESET, TRUE))
    {
        if (bValue && g_xhfw_ssp.pfnResetToFactory != NULL)
        {
            return g_xhfw_ssp.pfnResetToFactory();
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_RESTART, TRUE))
    {
        if (bValue && g_xhfw_ssp.pfnRestart != NULL)
        {
            return g_xhfw_ssp.pfnRestart();
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_AWS_ENABLED, TRUE))
    {
        if (g_xhfw_ssp.pfnSetAwsIotEnabled != NULL)
        {
            return g_xhfw_ssp.pfnSetAwsIotEnabled(bValue);
        }
    }
    else if( AnscEqualString(pParamName, XHFW_PARAM_NAME_USERVER_ENABLED, TRUE))
    {
        if (g_xhfw_ssp.pfnSetUServerEnabled != NULL)
        {
            return g_xhfw_ssp.pfnSetUServerEnabled(bValue);
        }
    }

    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        XHFW_SetParamUlongValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pParamName,
                ULONG                       uValue
            );

    description:

        This function is called to set ULONG parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pParamName,
                The parameter name;

                ULONG                       uValue
                The updated ULONG value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
XHFW_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        ULONG                       uValue
    )
{
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

        BOOL
        XHFW_SetParamIntValue
            (
                ANSC_HANDLE                 hInsContext,
                char*                       pParamName,
                int                         iValue
            );

    description:

        This function is called to set integer parameter value; 

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

                char*                       pParamName,
                The parameter name;

                int                         iValue
                The updated integer value;

    return:     TRUE if succeeded.

**********************************************************************/
BOOL
XHFW_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        int                         iValue
    )
{
    return FALSE;
}

/**********************************************************************  

    caller:     owner of this object 

    prototype: 

       ULONG
       XHFW_Commit
            (
                ANSC_HANDLE                 hInsContext
            );

    description:

        This function is called to finally commit all the update.

    argument:   ANSC_HANDLE                 hInsContext,
                The instance handle;

    return:     The status of the operation.

**********************************************************************/
ULONG
XHFW_Commit
    (
        ANSC_HANDLE                 hInsContext
    )
{
    return 0;
}

