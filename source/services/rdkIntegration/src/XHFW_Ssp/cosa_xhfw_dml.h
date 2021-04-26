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

/**************************************************************************

    module: cosa_xhfw_dml.h

        For COSA Data Model Library Development

    -------------------------------------------------------------------

    description:

        This file defines the apis for XHFW object to support Data Model Library.

    -------------------------------------------------------------------


    author:

        COSA XML TOOL CODE GENERATOR 1.0

    -------------------------------------------------------------------

    revision:

        12/02/2010    initial revision.

**************************************************************************/


#ifndef  _COSA_XHFW_DML_H
#define  _COSA_XHFW_DML_H

#include "slap_definitions.h"

/***********************************************************************

 APIs for Object:

    X_RDKCENTAL-COM_XHFW.

    *  XHFW_GetParamBoolValue
    *  XHFW_GetParamStringValue
    *  XHFW_SetParamBoolValue
    *  XHFW_SetParamStringValue
    *  XHFW_Validate
    *  XHFW_Commit
    *  XHFW_Rollback

***********************************************************************/
BOOL
XHFW_GetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        char*                       pValue,
        ULONG*                      pUlSize
    );

BOOL
XHFW_GetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       ParamName,
        BOOL*                       pBool
    );

BOOL
XHFW_GetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        ULONG*                      puLong
    );

BOOL
XHFW_GetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        int*                        pInt
    );

BOOL
XHFW_SetParamStringValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        char*                       pString
    );

BOOL
XHFW_SetParamBoolValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        BOOL                        bValue
    );

BOOL
XHFW_SetParamUlongValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        ULONG                       uValue
    );

BOOL
XHFW_SetParamIntValue
    (
        ANSC_HANDLE                 hInsContext,
        char*                       pParamName,
        int                         iValue
    );

ULONG
XHFW_Commit
    (
        ANSC_HANDLE                 hInsContext
    );

#endif //_COSA_XHFW_DML_H
