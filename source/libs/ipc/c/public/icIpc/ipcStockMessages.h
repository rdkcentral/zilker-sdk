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
 * ipcStockMessages.h
 *
 * Define the standard/stock set of IPC messages that
 * ALL services should handle.  Over time this may grow,
 * but serves the purpose of creating well-known IPC
 * messages without linking in superfluous API libraries
 * or relying on convention.
 *
 * Author: jelderton - 3/2/16
 *-----------------------------------------------*/

#ifndef ZILKER_IPCSTOCKMESSAGES_H
#define ZILKER_IPCSTOCKMESSAGES_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <cjson/cJSON.h>
#include <icTypes/icLinkedList.h>
#include <icTypes/icHashMap.h>
#include <icIpc/ipcMessage.h>
#include <icIpc/ipcSender.h>
#include <icIpc/ipcStockMessagesPojo.h>

/*
 * stock message codes.  all are negative so we do not
 * conflict with other messages a service may have
 */
#define STOCK_IPC_SHUTDOWN_SERVICE     -5
#define STOCK_IPC_GET_SERVICE_STATUS   -10
#define STOCK_IPC_GET_RUNTIME_STATS    -15
#define STOCK_IPC_CONFIG_RESTORED      -20
#define STOCK_IPC_START_INIT           -25

/**
 * request the service to shutdown.  requires the caller to have the magic
 * token, or else this request will fail.
 *   credentials - ensure caller has permission to request a shutdown
 *   timeoutSecs - number of seconds to wait for a reply before timing out.
 *                 if 0, there is no timeout on the read
 **/
IPCCode requestServiceShutdown(uint16_t servicePortNum, char *credentials, time_t timeoutSecs);

/**
 * obtain the current runtime statistics of the service
 *   thenClear - if true, clear stats after collecting them
 *   output - map of string/[string,date,int,long] to use for getting statistics
 *   readTimeoutSecs - number of seconds to wait for a reply before timing out.
 *                     if 0, there is no timeout on the read
 **/
IPCCode getServiceRuntimeStats(uint16_t servicePortNum, bool thenClear, runtimeStatsPojo *output, time_t readTimeoutSecs);

/**
 * obtain the current status of the service as a set of string/string values
 *   output - map of string/string to use for getting status
 *   readTimeoutSecs - number of seconds to wait for a reply before timing out.
 *                     if 0, there is no timeout on the read
 **/
IPCCode getServiceStatus(uint16_t servicePortNum, serviceStatusPojo *output, time_t readTimeoutSecs);

/**
 * inform a service that the configuration data was restored, into 'restoreDir'.
 * allows the service an opportunity to import files from the restore dir into the
 * normal storage area.  only happens during RMA situations.
 *   restoreDir - temp dir the configuration was extracted to
 *   readTimeoutSecs - number of seconds to wait for a reply before timing out.
 *                     if 0, there is no timeout waiting on a reply
 **/
IPCCode configRestored(uint16_t servicePortNum, configRestoredInput *input, configRestoredOutput *output, time_t readTimeoutSecs);

/**
 * called by watchdog during system startup.
 * TODO: fill this in with meaningful details
 *   timeoutSecs - number of seconds to wait for a reply before timing out.
 **/
IPCCode startInitialization(uint16_t servicePortNum, bool *output, time_t timeoutSecs);

#endif //ZILKER_IPCSTOCKMESSAGES_H
