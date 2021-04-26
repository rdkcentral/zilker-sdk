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
/*********************************************************************** 
  
    module: plugin_main.c

        Implement COSA Data Model Library Init and Unload apis.
 
    ---------------------------------------------------------------

    author:

        COSA XML TOOL CODE GENERATOR 1.0

    ---------------------------------------------------------------

    revision:

        09/28/2011    initial revision.

**********************************************************************/

#include "ansc_platform.h"
#include "ansc_load_library.h"
#include "cosa_plugin_api.h"
#include "plugin_main.h"
#include "cosa_xhfw_dml.h"

#include "common.h"
#include <icLog/logging.h>

#define THIS_PLUGIN_VERSION                         1

int ANSC_EXPORT_API
COSA_Init
    (
        ULONG                       uMaxVersionSupported, 
        void*                       hCosaPlugInfo         /* PCOSA_PLUGIN_INFO passed in by the caller */
    )
{
    PCOSA_PLUGIN_INFO               pPlugInfo  = (PCOSA_PLUGIN_INFO)hCosaPlugInfo;

    icLogInfo(LOG_TAG, "COSA_Init(%p)", hCosaPlugInfo);

    if ( uMaxVersionSupported < THIS_PLUGIN_VERSION )
    {
      /* this version is not supported */
        return -1;
    }   
    
    pPlugInfo->uPluginVersion       = THIS_PLUGIN_VERSION;
    /* register the back-end apis for the data model */

    //TODO get rid of the ones we dont need?
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_GetParamUlongValue",  XHFW_GetParamUlongValue);
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_SetParamUlongValue",  XHFW_SetParamUlongValue);
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_GetParamStringValue",  XHFW_GetParamStringValue);
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_SetParamStringValue",  XHFW_SetParamStringValue);
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_GetParamBoolValue",  XHFW_GetParamBoolValue);
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_SetParamBoolValue",  XHFW_SetParamBoolValue);
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_GetParamIntValue",  XHFW_GetParamIntValue);
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_SetParamIntValue",  XHFW_SetParamIntValue);
    pPlugInfo->RegisterFunction(pPlugInfo->hContext, "XHFW_Commit",  XHFW_Commit);
    return  0;
}

BOOL ANSC_EXPORT_API
COSA_IsObjectSupported
    (
        char*                        pObjName
    )
{
    //TODO fill this in?
    return TRUE;
}

void ANSC_EXPORT_API
COSA_Unload
    (
        void
    )
{
    icLogInfo(LOG_TAG, "COSA_Unload()");

    /* unload the memory here */
}
