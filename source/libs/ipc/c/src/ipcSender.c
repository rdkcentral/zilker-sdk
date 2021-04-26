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
 * IPCSender.c
 *
 * Client-side functions that serve as the framework
 * for making IPC calls to Services.  This supports both
 * Java and C Services as it utilizes IPCMessage objects
 * that contain JSON payloads.
 *
 * Note: this correlates closely to IPCSender.java
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
//#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
//#include <errno.h>
//#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/socket.h>
//#include <sys/select.h>
//#include <sys/un.h>
//#include <arpa/inet.h>

#include <icLog/logging.h>
#include <icIpc/ipcSender.h>
//#include <icIpc/ipcMessage.h>
#include "ipcCommon.h"
#include "serviceRegistry.h"
#include "transport/transport.h"

#define DEFAULT_WRITE_TIMEOUT_SECS  5
#define DEFAULT_READ_TIMEOUT_SECS   10      // match IPCSender.java

/* private function declarations */
static int32_t openServiceSocket(uint16_t servicePortNum);
static void closeServiceSocket(int32_t sockFD);

/*
 * Checks to see if the Service (on the local host)
 * is taking requests. (i.e. is it alive and running)
 *
 * @param servicePortNum  - the port (on the local host) of the service to send the message to
 */
bool isServiceAvailable(uint16_t servicePortNum)
{
    // assume failure
    //
    bool rc = false;

    // create a PING request and attempt to send to the service
    //
    IPCMessage input;
    IPCMessage output;

    // setup input/output containers
    //
    memset(&input, 0, sizeof(IPCMessage));
    memset(&output, 0, sizeof(IPCMessage));
    input.msgCode = PING_REQUEST;
    input.payloadLen = 0;

#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "sending PING to servicePort %" PRIu16, servicePortNum);
#endif

    // send the message and wait for a reply
    //
    if (sendServiceRequest(servicePortNum, &input, &output) == IPC_SUCCESS)
    {
        // before claiming success, check if the response code is correct
        //
        if (output.msgCode == PING_RESPONSE)
        {
            // successful, no more checking required
            //
            rc = true;
        }
#ifdef CONFIG_DEBUG_IPC
        else
        {
            icLogDebug(API_LOG_CAT, "got unexpected response=%" PRIi32 " to PING from servicePort %" PRIu16, output.msgCode, servicePortNum);
        }
#endif
        // check for small chance of memory leakage that could occur if the service
        // sent back a response payload.  We don't need it, so free if it set.
        //
        if (output.payload != NULL)
        {
            transport_free_msg(output.payload);
            output.payload = NULL;
        }
    }

    return rc;
}

/*
 * Wait for the Service (on the local host) to start responding.
 * Same as calling "isServiceAvailable" over and over until success.
 * After 'timeoutSecs' seconds, this will give up.
 *
 * @param servicePortNum  - the port (on the local host) of the service to send the message to
 * @param timeooutSecs  - if > 0, number of seconds to wait before giving up.  If 0, will wait indefinitely
 */
bool waitForServiceAvailable(uint16_t servicePortNum, time_t timeoutSecs)
{
    int count = 1;

    while (1)
    {
#ifdef CONFIG_DEBUG_IPC
        icLogDebug(API_LOG_CAT, "wait: testing to see if service port=%" PRIu16 " is alive", servicePortNum);
#endif

        // check if service response to PING requests
        //
        if (isServiceAvailable(servicePortNum) == true)
        {
            // good to go
            //
            return true;
        }

#ifdef CONFIG_DEBUG_IPC
        icLogDebug(API_LOG_CAT, "unable to PING servicePort %" PRIu16 ", attempt #%d", servicePortNum, count);
#endif

        // failed, sleep 1 second before trying again
        //
        count++;
        sleep(1);

        if (timeoutSecs > 0)
        {
            if (count > timeoutSecs)
            {
                // too many failures
                //
#ifdef CONFIG_DEBUG_IPC
                icLogWarn(API_LOG_CAT, "wait: bailing on servicePort=%" PRIu16 ".  Too many failures=%d", servicePortNum, count);
#endif
                break;
            }
        }
    }

    return false;
}

/*
 * Send an IPC request to the Service at 'servicePortNum'.
 *
 * Note that both the 'request' and 'response' should be created and
 * provided by the caller, with the exception of the 'response payload',
 * which should initially be NULL to handle undetermined response sizes.
 * If there is a  response, the 'response payload' will be allocated
 * during this function, so the caller will need to free that memory.
 *
 * There is a limitation of 64K for the encoded message payload (both
 * request and response).  This is due to the fact we send the length
 * of the message as a 'short' (to allow for Java Services).
 *
 * @param servicePortNum  - the port (on the local host) of the service to send the message to
 * @param request - IPCMessage to send to the service.  Should be allocated by caller and fully constructed as applicable
 * @param response - Response from the Service.  Can be NULL, but if needed should be allocated by calle
 * @return if the message was successfully sent, not if the actual request was successful in the Service
 */
IPCCode sendServiceRequest(uint16_t servicePortNum, IPCMessage *request, IPCMessage *response)
{
    return sendServiceRequestTimeout(servicePortNum, request, response, DEFAULT_READ_TIMEOUT_SECS);
}

/*
 * Send an IPC request to the Service at 'servicePortNum', allowing for a timeout
 * while waiting on the response.
 *
 * Note that both the 'request' and 'response' should be created and
 * provided by the caller, with the exception of the 'response payload',
 * which should initially be NULL to handle undetermined response sizes.
 * If there is a  response, the 'response payload' will be allocated
 * during this function, so the caller will need to free that memory.
 *
 * There is a limitation of 64K for the encoded message payload (both
 * request and response).  This is due to the fact we send the length
 * of the message as a 'short' (to allow for Java Services).
 *
 * @param servicePortNum  - the port (on the local host) of the service to send the message to
 * @param request - IPCMessage to send to the service.  Should be allocated by caller and fully constructed as applicable
 * @param response - Response from the Service.  Can be NULL, but if needed should be allocated by calle
 * @param readTimeoutSecs - if == 0, block until a response. if > 0, use as a response timeout
 *
 * @return if the message was successfully sent, not if the actual request was successful in the Service
 */
IPCCode sendServiceRequestTimeout(uint16_t servicePortNum, IPCMessage *request, IPCMessage *response, time_t readTimeoutSecs)
{
    int32_t sock = 0;
    const char *serviceHost = getServiceAddressForIpcPort(servicePortNum);

    // There are 4 setps to sending an IPC request:
    // 1.  open socket to service
    // 2.  send the request (code, length, format, payload)
    // 3.  read a response (code, length, format, payload)
    // 4.  close socket
    //

    // make sure the message is available
    //
    if (request == NULL)
    {
        icLogWarn(API_LOG_CAT, "invalid 'request' to servicePort %" PRIu16, servicePortNum);
        return IPC_INVALID_ERROR;
    }

    // open the socket to this service
    //
    sock = transport_connect(serviceHost, servicePortNum);
    if (sock < 0)
    {
        icLogWarn(API_LOG_CAT, "unable to create socket to servicePort %" PRIu16, servicePortNum);
        return IPC_CONNECT_ERROR;
    }

    // send the request message to the service.  use a 'write'
    // timeout to handle scenarios where the server went
    // down right after creating the socket.
    //
    IPCCode rc = IPC_SUCCESS;
    if ((rc = transport_send(sock, request, DEFAULT_WRITE_TIMEOUT_SECS)) != IPC_SUCCESS)
    {
        // error sending, bail
        //
#ifdef CONFIG_DEBUG_IPC
        icLogWarn(API_LOG_CAT, "error sending service request to servicePort=%"PRIu16" msg=%" PRIi32" reason=%s",
                  servicePortNum, request->msgCode, IPCCodeLabels[rc]);
#endif
        transport_close(sock);
        return rc;
    }

    // if we got this far, then the message was successfully sent.
    // read the response
    //
    if (response != NULL)
    {
        // read the response
        //
        if ((rc = transport_recv(sock, &response, readTimeoutSecs)) != IPC_SUCCESS)
        {
#ifdef CONFIG_DEBUG_IPC
            icLogWarn(API_LOG_CAT, "error receiving service response from servicePort=%"PRIu16" msg=%" PRIi32" reason=%s",
                      servicePortNum, request->msgCode, IPCCodeLabels[rc]);
#endif
            transport_close(sock);
            return rc;
        }

        // look at the response code to see if it fits within IPCCode.
        // this way we can return failures from the server (IPC_GENERAL_ERROR)
        // and preserve returns outside of that bounds (ex: PING_RESPONSE)
        //
        if (response->msgCode > IPC_SUCCESS && response->msgCode <= IPC_GENERAL_ERROR)
        {
            // log the error code
#ifdef CONFIG_DEBUG_IPC
            icLogWarn(API_LOG_CAT, "service response from servicePort=%"PRIu16" returned failure; %"PRIi32" - %s; msg=%" PRIi32,
                      servicePortNum, response->msgCode, IPCCodeLabels[response->msgCode], request->msgCode);
#endif
            rc = (IPCCode)response->msgCode;
        }
    }

    // close up the socket and hope the caller frees the 'response'
    //
    transport_close(sock);

    return rc;
}

/*
 * Tell any pending IPC senders to give up and shutdown
 */
void ipcSenderShutdown()
{
    transport_shutdown();
}


