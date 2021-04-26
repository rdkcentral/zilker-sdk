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
 * ipcSender.h
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

#ifndef IPCSENDER_H
#define IPCSENDER_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <icIpc/ipcMessage.h>

/*
 * Checks to see if the Service (on the local host)
 * is taking requests. (i.e. is it alive and running)
 *
 * @param servicePortNum  - the port (on the local host) of the service to send the message to
 */
extern bool isServiceAvailable(uint16_t servicePortNum);

/*
 * Wait for the Service (on the local host) to start responding.
 * Same as calling "isServiceAvailable" over and over until success.
 * After 'timeoutSecs' seconds, this will give up.
 *
 * @param servicePortNum  - the port (on the local host) of the service to send the message to
 * @param timeooutSecs  - if > 0, number of seconds to wait before giving up.  If 0, will wait indefinitely
 */
extern bool waitForServiceAvailable(uint16_t servicePortNum, time_t timeoutSecs);

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
 * @param response - Response from the Service.  Can be NULL, but if needed should be allocated by caller (except for payload, which needs to be freed by the caller)
 *
 * @return if the message was successfully sent, not if the actual request was successful in the Service
 */
extern IPCCode sendServiceRequest(uint16_t servicePortNum, IPCMessage *request, IPCMessage *response);

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
extern IPCCode sendServiceRequestTimeout(uint16_t servicePortNum, IPCMessage *request, IPCMessage *response, time_t readTimeoutSecs);

/*
 * Tell any pending IPC senders to give up and shutdown
 */
void ipcSenderShutdown();

#endif // IPCSENDER_H
