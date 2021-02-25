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
 * ServiceRegistry.h
 *
 * Registry of known services.  Primarily it will
 * contain each running  on the local host, but
 * attempts to keep in sync with other services
 * running within the local premise.
 *
 * Used by IPC and Event-listeners to determine the
 * targeted service location  (same host, same network, etc)
 * to optimize the calls and skip marshaling if possible.
 *
 * Note: this correlates closely to ServiceRegistry.java
 *
 * Author: jelderton - 6/19/15
 *-----------------------------------------------*/

#ifndef IC_SERVICEREGISTRY_H
#define IC_SERVICEREGISTRY_H

#include <stdint.h>
#include <icIpc/ipcReceiver.h>
#include "ipcCommon.h"


typedef struct _serviceHandle {
    char        serviceName[128];       // name of the service.  used only for debugging
    char        serviceAddress[128];    // IP address of the service.  Probably set to LOCAL_LOOPBACK
    uint16_t    ipcPort;                // accessible TCP port for IPC communication to/from the service
    uint16_t    eventPort;              // UDP port used for broadcasting events from the service
    serviceVisibility  visibility;      // visible scope of the service
} serviceHandle;

/*
 * convenience function to create and clear a
 * serviceHandle instance (as well as default the
 * address to LOCAL_LOOPBACK
 */
serviceHandle *createServiceHandle();

/*
 * convience function to destroy a serviceHandle
 */
void destroyServiceHandle(serviceHandle *handle);

/*
 * Called by a Service to register it's information
 * to the registry.  Used to track the available services
 * and their address.
 */
void registerService(serviceHandle *handle);

/*
 * Return the Service that is registered for this IPC port.
 * Can return NULL if the service is not known.  Note that the
 * caller should NOT free this memory.
 */
serviceHandle *getServiceForIpcPort(uint16_t ipcPort);

/*
 * Return the Service that is registered for this Event port.
 * Can return NULL if the service is not known.  Note that the
 * caller should NOT free this memory.
 */
serviceHandle *getServiceForEventPort(uint16_t eventPort);

/*
 * Return the serviceAddress to use for a particular IPC port
 * If the service is not registered, assumes LOCAL_LOOPBACK
 */
const char *getServiceAddressForIpcPort(uint16_t ipcPort);

/*
 * Return the serviceAddress to use for a particular Event port
 * If the service is not registered, assumes LOCAL_LOOPBACK
 */
const char *getServiceAddressForEventPort(uint16_t eventPort);


#endif // IC_SERVICEREGISTRY_H
