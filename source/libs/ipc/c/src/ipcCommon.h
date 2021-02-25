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
 * ipcCommon.h
 *
 * Set of private macros, defines, and functions used
 * internally as part of the IPC library implementation
 *
 * Author: jelderton
 *-----------------------------------------------*/

#ifndef IPCCOMMON_H
#define IPCCOMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <cjson/cJSON.h>
#include <icIpc/ipcMessage.h>

/* define the log category (regardless of CONFIG_DEBUG_IPC to allow warn/error messages) */
#define  API_LOG_CAT        "IPC"
#define  API_DEEP_LOG_CAT   "IPC-DEEP"

/* default local loopback IP address for services running on the same host as the client */
#define LOCAL_LOOPBACK  "127.0.0.1"

/* set of well-known IPC message codes */
#define PING_REQUEST    -75
#define PING_RESPONSE   -74

/* actual port used for events */
#define EVENT_BROADCAST_PORT    12575

/*
 * test to see if the socket is ready for reading
 */
bool canReadFromSocket(int32_t sockFD, time_t timeoutSecs);

/*
 * check if can read from either socket.  returns:
 * 0         - if 'serviceSock' is ready
 * ETIMEDOUT - if 'serviceSock' was not ready within the timeoutSecs
 * EINTR     - if 'shutdownSock' is ready
 * EAGAIN    - all other conditions
 */
int canReadFromServiceSocket(int32_t serviceSock, int32_t shutdownSock, time_t timeoutSecs);

/*
 * test to see if the socket is ready for writing
 */
bool canWriteToSocket(int32_t sockFD, time_t timeoutSecs);

/*
 * extract the "serviceIdNum" from a raw event (to find where it came from)
 */
uint32_t extractServiceIdFromRawEvent(cJSON *buffer);

/*
 * embed the "serviceIdNum" into a raw event (to indicate where the event originated from)
 */
void insertServiceIdToRawEvent(cJSON *buffer, uint32_t serviceIdNum);

#endif // IPCCOMMON_H
