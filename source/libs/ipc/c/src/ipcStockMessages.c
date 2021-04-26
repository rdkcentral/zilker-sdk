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
 * ipcStockMessages.c
 *
 * Define the standard/stock set of IPC messages that
 * ALL services should handle.  Over time this may grow,
 * but serves the purpose of creating well-known IPC
 * messages without linking in superfluous API libraries
 * or relying on convention.
 *
 * Author: jelderton - 3/2/16
 *-----------------------------------------------*/

#include <string.h>
#include "icIpc/ipcStockMessages.h"

/**
 * request the service to shutdown.  requires the caller to have the magic
 * token, or else this request will fail.
 *   credentials - ensure caller has permission to request a shutdown
 *   timeoutSecs - number of seconds to wait for a reply before timing out.
 *                 if 0, there is no timeout on the read
 **/
IPCCode requestServiceShutdown(uint16_t servicePortNum, char *credentials, time_t timeoutSecs)
{
    // fast fail if no 'credentials'
    //
    if (credentials == NULL || strlen(credentials) == 0)
    {
        return IPC_INVALID_ERROR;
    }

    IPCCode rc;
    IPCMessage	*ipcInParm = createIPCMessage();
    IPCMessage  *ipcOutParm = createIPCMessage();

    // set message code for request GET_RUNTIME_STATS
    //
    ipcInParm->msgCode = STOCK_IPC_SHUTDOWN_SERVICE;

    // encode input parameter
    //
    cJSON *inRoot = cJSON_CreateObject();
    cJSON_AddStringToObject(inRoot, "s", credentials);
#ifdef CONFIG_DEBUG_IPC
    char *encodeStr = cJSON_Print(inRoot);
#else
    char *encodeStr = cJSON_PrintUnformatted(inRoot);
#endif
    populateIPCMessageWithJSON(ipcInParm, encodeStr);

    // mem cleanup
    //
    free(encodeStr);
    cJSON_Delete(inRoot);

    // send request
    //
    rc = sendServiceRequestTimeout(servicePortNum, ipcInParm, ipcOutParm, timeoutSecs);

    // mem cleanup and return
    //
    freeIPCMessage(ipcInParm);
    freeIPCMessage(ipcOutParm);
    return rc;
}

/**
 * obtain the current runtime statistics of the service
 *   thenClear - if true, clear stats after collecting them
 *   output - map of string/[string,date,int,long] to use for getting statistics
 *   readTimeoutSecs - number of seconds to wait for a reply before timing out.
 *                     if 0, there is no timeout on the read
 **/
IPCCode getServiceRuntimeStats(uint16_t servicePortNum, bool thenClear, runtimeStatsPojo *output, time_t readTimeoutSecs)
{
    IPCCode rc = IPC_SUCCESS;
    IPCMessage	*ipcInParm = createIPCMessage();
    IPCMessage  *ipcOutParm = createIPCMessage();

    // set message code for request GET_RUNTIME_STATS
    //
    ipcInParm->msgCode = STOCK_IPC_GET_RUNTIME_STATS;

    // encode input parameter
    //
    cJSON *inRoot = cJSON_CreateObject();
    if (thenClear == true)
        cJSON_AddTrueToObject(inRoot, "b");
    else
        cJSON_AddFalseToObject(inRoot, "b");
#ifdef CONFIG_DEBUG_IPC
    char *encodeStr = cJSON_Print(inRoot);
#else
    char *encodeStr = cJSON_PrintUnformatted(inRoot);
#endif
    populateIPCMessageWithJSON(ipcInParm, encodeStr);

    // mem cleanup
    //
    free(encodeStr);
    cJSON_Delete(inRoot);

    // send request
    //
    rc = sendServiceRequestTimeout(servicePortNum, ipcInParm, ipcOutParm, readTimeoutSecs);
    if (rc != IPC_SUCCESS)
    {
        // request failure
        //
        freeIPCMessage(ipcInParm);
        freeIPCMessage(ipcOutParm);
        return rc;
    }

    // parse output
    //
    if (ipcOutParm->payloadLen > 0)
    {
        cJSON *outRoot = cJSON_Parse((const char *)ipcOutParm->payload);
        if (outRoot != NULL)
        {
            decode_runtimeStatsPojo_fromJSON(output, outRoot);
            cJSON_Delete(outRoot);
        }
    }

    // mem cleanup
    //
    freeIPCMessage(ipcInParm);
    freeIPCMessage(ipcOutParm);

    return rc;
}

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 *   readTimeoutSecs - number of seconds to wait for a reply before timing out.
 *                     if 0, there is no timeout on the read
 **/
IPCCode getServiceStatus(uint16_t servicePortNum, serviceStatusPojo *output, time_t readTimeoutSecs)
{
    IPCCode rc = IPC_SUCCESS;
    IPCMessage	*ipcInParm = createIPCMessage();
    IPCMessage  *ipcOutParm = createIPCMessage();

    // set message code for request GET_SERVICE_STATUS
    //
    ipcInParm->msgCode = STOCK_IPC_GET_SERVICE_STATUS;

    // send request
    //
    rc = sendServiceRequestTimeout(servicePortNum, ipcInParm, ipcOutParm, readTimeoutSecs);
    if (rc != IPC_SUCCESS)
    {
        // request failure
        //
        freeIPCMessage(ipcInParm);
        freeIPCMessage(ipcOutParm);
        return rc;
    }

    // parse output
    //
    if (ipcOutParm->payloadLen > 0)
    {
        cJSON *outRoot = cJSON_Parse((const char *)ipcOutParm->payload);
        if (outRoot != NULL)
        {
            decode_serviceStatusPojo_fromJSON(output, outRoot);
            cJSON_Delete(outRoot);
        }
    }

    // mem cleanup
    //
    freeIPCMessage(ipcInParm);
    freeIPCMessage(ipcOutParm);

    return rc;
}

/**
 * inform a service that the configuration data was restored, into 'restoreDir'.
 * allows the service an opportunity to import files from the restore dir into the
 * normal storage area.  only happens during RMA situations.
 *   restoreDir - temp dir the configuration was extracted to
 *   readTimeoutSecs - number of seconds to wait for a reply before timing out.
 *                     if 0, there is no timeout waiting on a reply
 **/
IPCCode configRestored(uint16_t servicePortNum, configRestoredInput *input, configRestoredOutput *output, time_t readTimeoutSecs)
{
    IPCCode rc = IPC_SUCCESS;
    IPCMessage	*ipcInParm = createIPCMessage();
    IPCMessage  *ipcOutParm = createIPCMessage();

    // set message code for request STOCK_IPC_CONFIG_RESTORED
    ipcInParm->msgCode = STOCK_IPC_CONFIG_RESTORED;

    if (input)
    {
        // encode input parameter
        cJSON* inRoot = encode_configRestoredInput_toJSON(input);
#ifdef CONFIG_DEBUG_IPC
        char *encodeStr = cJSON_Print(inRoot);
#else
        char* encodeStr = cJSON_PrintUnformatted(inRoot);
#endif
        populateIPCMessageWithJSON(ipcInParm, encodeStr);

        // mem cleanup
        free(encodeStr);
        cJSON_Delete(inRoot);
    }

    // send request
    //
    rc = sendServiceRequestTimeout(servicePortNum, ipcInParm, ipcOutParm, readTimeoutSecs);
    if (rc != IPC_SUCCESS)
    {
        // request failure
        //
        freeIPCMessage(ipcInParm);
        freeIPCMessage(ipcOutParm);
        return rc;
    }

    rc = IPC_GENERAL_ERROR;
    // parse output
    //
    if (ipcOutParm->payloadLen > 0)
    {
        cJSON *outRoot = cJSON_Parse((const char *)ipcOutParm->payload);
        if (outRoot != NULL)
        {
            rc = decode_configRestoredOutput_fromJSON(output, outRoot) == 0 ? IPC_SUCCESS : IPC_GENERAL_ERROR;
            cJSON_Delete(outRoot);
        }
    }


    // mem cleanup
    //
    freeIPCMessage(ipcInParm);
    freeIPCMessage(ipcOutParm);

    return rc;

}

/**
 * called by watchdog during system startup.
 * TODO: fill this in with meaningful details
 *   timeoutSecs - number of seconds to wait for a reply before timing out.
 **/
IPCCode startInitialization(uint16_t servicePortNum, bool *output, time_t timeoutSecs)
{
    IPCCode rc = IPC_SUCCESS;
    IPCMessage	*ipcInParm = createIPCMessage();
    IPCMessage  *ipcOutParm = createIPCMessage();

    // set message code for request START_INIT
    //
    ipcInParm->msgCode = STOCK_IPC_START_INIT;

    // send the request to the service
    //
    rc = sendServiceRequestTimeout(servicePortNum, ipcInParm, ipcOutParm, timeoutSecs);
    if (rc == IPC_SUCCESS)
    {

        // parse output
        //
        if (ipcOutParm->payloadLen > 0)
        {
            cJSON *item = NULL;
            cJSON *outRoot = cJSON_Parse((const char *)ipcOutParm->payload);
            if (outRoot != NULL)
            {
                item = cJSON_GetObjectItem(outRoot, "b");
                if (item != NULL)
                {
                    *output = cJSON_IsTrue(item) ? true : false;
                    rc = 0;
                }
                cJSON_Delete(outRoot);
            }
        }
    }

    // cleanup, then return
    //
    freeIPCMessage(ipcInParm);
    freeIPCMessage(ipcOutParm);
    return rc;
}
