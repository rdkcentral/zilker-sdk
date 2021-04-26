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
/*********************************************************************************

    description:

        This is the template file of ssp_main.c for XxxxSsp.
        Please replace "XXXX" with your own ssp name with the same up/lower cases.

  ------------------------------------------------------------------------------

    revision:

        09/08/2011    initial revision.

**********************************************************************************/


#ifdef __GNUC__
#ifndef _BUILD_ANDROID
#include <execinfo.h>
#endif
#endif

#include "common.h"
#include <icLog/logging.h>

#include "ssp_global.h"
#include "stdlib.h"
#include "ccsp_dm_api.h"

/*----------------------------------------------------------------------------*/
/*                           Global scoped variables                          */
/*----------------------------------------------------------------------------*/
xhfw_ssp_callbacks_t                g_xhfw_ssp              = {};
char                                g_Subsystem[32]         = {0};
char*                               g_pComponentName        = NULL;


/*----------------------------------------------------------------------------*/
/*                             Function Prototypes                            */
/*----------------------------------------------------------------------------*/


/*----------------------------------------------------------------------------*/
/*                             External Functions                             */
/*----------------------------------------------------------------------------*/

/**
 * @brief Bus platform initialization to engage the component to CR(Component Registrar).
 */
int msgBusInit
    (
        xhfw_ssp_callbacks_t * pCallbacks
    )
{
    int                             rc                 = 0;
    extern ANSC_HANDLE              bus_handle;
    char *                          subSys             = NULL;  
    DmErr_t                         err;
    char                            CName[256];

    icLogDebug(LOG_TAG, "%s", __func__);
   
    /* Set the global g_pComponentName */
    g_pComponentName = CCSP_COMPONENT_NAME_XHFW;

    /* Set the callback functions */
    if (pCallbacks != NULL)
    {
        g_xhfw_ssp = *pCallbacks;
    }
    else
    {
        memset(&g_xhfw_ssp, 0, sizeof(xhfw_ssp_callbacks_t));
    }


    /* Use the eRT. subsystem */
    AnscCopyString(g_Subsystem, "eRT.");

    _ansc_sprintf(CName, "%s%s", g_Subsystem, CCSP_COMPONENT_ID_XHFW);

    ssp_Mbi_MessageBusEngage ( CName, CCSP_MSG_BUS_CFG, CCSP_COMPONENT_PATH_XHFW );
    ssp_create();
    ssp_engage();

#ifdef _COSA_SIM_
    subSys = "";        /* PC simu use empty string as subsystem */
#else
    subSys = NULL;      /* use default sub-system */
#endif

    err = Cdm_Init(bus_handle, subSys, NULL, NULL, g_pComponentName);
    if (err != CCSP_SUCCESS)
    {
        icLogError(LOG_TAG, "XHFW: Cdm_Init: %s\n", Cdm_StrError(err));
        rc = 1;
    }

    system("touch /tmp/xhfw_component_initialized");

    Init_Parodus_Task();

    icLogDebug(LOG_TAG, "%s: complete (rc=%d)", __func__, rc);

    return rc;
}

int msgBusTerm(void)
{
    int rc = 0;

    icLogDebug(LOG_TAG, "%s", __func__);

    DmErr_t err = Cdm_Term();
    if (err != CCSP_SUCCESS)
    {
        icLogError(LOG_TAG, "XHFW: Cdm_Term: %s\n", Cdm_StrError(err));
        rc = 1;
    }

    ssp_cancel();

    icLogDebug(LOG_TAG, "%s: complete (rc=%d)", __func__, rc);

    return rc;
}

