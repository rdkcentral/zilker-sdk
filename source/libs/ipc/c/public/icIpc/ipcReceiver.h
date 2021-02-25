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
 * ipcReceiver.h
 *
 * Service-side functions that serve as the foundation
 * for receiving IPC requests, processing them, and
 * supplying a response.  Requires the Service to provide
 * a handler so that requests can be processed as they arrive.
 *
 * Services should be able to process requests from
 * both Native & Java clients and send back responses
 * that can be parsed by these clients.
 *
 * Note: this correlates closely to IPCReceiver.java
 *
 * Author: jelderton
 *-----------------------------------------------*/

#ifndef IPCRECEIVER_H
#define IPCRECEIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <icIpc/ipcMessage.h>
#include <icIpc/ipcStockMessagesPojo.h>

#define IpcReceiver void*

// standard values for the IPC threadPool.
// originally these were larger, but decided to reduce the min to 1 since
// we sit idle for a long time; wasting stack and memory space
//
#define IPC_DEFAULT_MIN_THREADS     1
#define IPC_DEFAULT_MAX_THREADS     8
#define IPC_DEFAULT_MAX_QUEUE_SIZE  25

/*
 * define the 'visibility' of the service
 */
typedef enum  {
    SERVICE_VISIBLE_LOCAL_PROCESS,
    SERVICE_VISIBLE_LOCAL_HOST,
    SERVICE_VISIBLE_ALL_HOSTS,
} serviceVisibility;

/*
 * Function signature of the Service Request Handler so that messages
 * can be processed as they are received.  Implementation should not
 * free the request. If the response is not NULL, the processing thread
 * will free it after sending to the Client.
 */
typedef IPCCode (*serviceRequestHandler)(IPCMessage *request, IPCMessage *response);

/*
 * Function signature for notifying handler implementations that the receiver had to be asynchronously shut down.
 * After this is called, a handler implementation should no longer reference their copy of the receiver.
 *
 * The receiver may be shut down asynchronously if a message handler function returned with a value of IPC_SHUT_DOWN
 * @see ipcMessage.h
 */
typedef void (*asyncShutdownNotifyFunc)(void);

/*
 * Creates a server-socket to listen on 'servicePortNum' and process incoming message
 * requests.  As each arrives, it will be placed into a processing thread and
 * forwarded to the 'serviceRequestHandler' for processing.
 *
 * This call does not return unless there was an error creating the server-socket OR
 * until a call to shutdownRequestHandler() is made.
 *
 * @param servicePortNum - the TCP port to listen on
 * @param serviceRequestHandler - the handler to utilize as messages arrive
 * @param scope - the visibility of the handler (used when registering with ServiceRegistry)
 * @param minThreads - the min number of threads for the 'threadPool'
 * @param maxThreads - the max number of threads for the 'threadPool'
 * @param maxQueueSize - the max number of requests to keep in the threadPool's queue
 * @param shutdownNotifyFunc - callback to be notified when the returned receiver is shutting down and should no longer be used
 *
 * @return IPC_SUCCESS when the handler exits cleanly
 * @see shutdownRequestHandler()
 */
extern IpcReceiver startRequestHandler(uint16_t servicePortNum, serviceRequestHandler handler, serviceVisibility scope,
                                       uint16_t minThreads, uint16_t maxThreads, uint32_t maxQueueSize,
                                       asyncShutdownNotifyFunc shutdownNotifyFunc);

/*
 * Closes the receive server-socket to halt processing of incoming requests.
 * Generally called during Service shutdown.
 */
extern void shutdownRequestHandler(IpcReceiver receiver);

/*
 * Suspends the calling thread until the receiver shuts down
 */
extern void waitForRequestHandlerToShutdown(IpcReceiver receiver);


/*
 * collect statistics about the IPC handlers, and populate
 * them into the supplied runtimeStatsPojo container
 */
extern void collectIpcStatistics(IpcReceiver receiver, runtimeStatsPojo *container, bool thenClear);

#endif // IPCRECEIVER_H
