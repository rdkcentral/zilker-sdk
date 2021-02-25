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

//
// Created by Boyd, Weston on 3/23/18.
//

#ifndef ZILKER_TRANSPORT_H
#define ZILKER_TRANSPORT_H

#include <time.h>

#include <cjson/cJSON.h>
#include <icIpc/ipcMessage.h>

#define TRANSPORT_TIMEOUT_INFINITE -1

#define TRANSPORT_SUBSCRIBE_ALL -1
#define TRANSPORT_DEFAULT_PUBSUB NULL

/** Internal control block to associate
 * a response with a request asynchronously.
 */
typedef struct transport_control {
    void* control_block;
    size_t length;
} transport_control_t;

/** Allocate memory for a message.
 *
 * Some transports will require there own special buffers. Thus
 * 'malloc' may not be relied upon.
 *
 * @param size The number of bytes to allocate.
 * @return A pointer to memory allocated for use. NULL if the allocation failed.
 */
void* transport_malloc_msg(size_t size);

void* transport_realloc_msg(void* ptr, size_t size);

/** Free memory used by a message.
 *
 * Some transports will require there own special buffers. Thus
 * 'free' may not be relied upon.
 *
 * @param ptr Pointer to memory that is to be released.
 */
void transport_free_msg(void* ptr);

/** Connect an IPC channel that may be used to send a request and
 * receive a response on.
 *
 * The channel created is to be used to send a request and receive a
 * response on. A new channel <em>must</em> be created for each
 * request/response session.
 *
 * @param host The host to connect to.
 * @param servicePortNum The port number of the request handler.
 * @return A IPC connection file descriptor that may be zero or greater. -1 on error.
 */
int transport_connect(const char* host, uint16_t servicePortNum);

/** Establish an IPC channel to handle requests from clients.
 *
 * The channel created is to be used as a request handler (a.k.a server). Many clients
 * may send messages to a single server. The channel may be used repeatedly.
 *
 * @param host The host this server should bind to.
 * @param servicePortNum The port number this server should be bound to.
 * @return A IPC connection file descriptor that may be zero or greater. -1 on error.
 */
int transport_establish(const char* host, uint16_t servicePortNum);

/** Register as a publisher of events on a channel.
 *
 * A NULL, or empty, channel will use the default publish channel that
 * all messages are broadcast over.
 *
 * @param channel The channel used for publishing messages.
 * If NULL, or empty, then use the default channel.
 * @return A publisher file descriptor. -1 if an error occurred.
 */
int transport_pub_register(const char* channel);

/** Register as a subscriber for events on a channel.
 *
 * A NULL, or empty, channel will use the default subscriber channel that
 * all messages are broadcast over.
 *
 * @param channel The channel used for subscribing to messages.
 * If NULL, or empty, then use the default channel.
 * @return A subscriber file descriptor. -1 if an error occurred.
 */
int transport_sub_register(const char* channel);

/**
 * Get the shutdown readfd for the given descriptor
 *
 * @param sockfd the socket file descriptor
 * @return the stutdown fd, or -1 if none exists
 */
int transport_get_shutdown_sock_readfd(int sockfd);

/** Shutdown, and close, an IPC channel.
 *
 * Once closed down the IPC file descriptor may <em>not</em> be used again.
 *
 * @param sockfd The IPC connection file descriptor to close.
 */
void transport_close(int sockfd);

/** Subscribe only to specific events.
 *
 * A value of TRANSPORT_SUBSCRIBE_ALL, or -1, will subscribe
 * to all events.
 *
 * @param sockfd The subscriber file descriptor.
 * @param id The topic ID to filter events on.
 * @return 0 on success. -1 on error.
 */
int transport_subscribe(int sockfd, int id);

/** Receive a subscribed JSON event.
 *
 * @param sockfd The subscriber file descriptor.
 * @param event A JSON object will be allocated for the caller
 * and assigned to the pointed to value of event.
 * @return 0 on success. -1 on error.
 */
int transport_sub_recv(int sockfd, cJSON** event);

/** Publish a JSON event to all subscribers.
 *
 * @param sockfd The publisher file descriptor.
 * @param event A JSON object that will be serialized and
 * published to all subscribers.
 */
void transport_publish(int sockfd, const cJSON* event);

/** Send an IPC message through an IPC channel.
 *
 * @param sockfd The IPC file descriptor to send over.
 * @param msg An IPC message to be sent to a listener.
 * @param timeoutSecs Number of seconds before timing out. -1 for infinite.
 * @return IPC code describing success level.
 */
IPCCode transport_send(int sockfd, const IPCMessage* msg, long timeoutSecs);

/** Receive an IPC message through an IPC channel.
 *
 * The message pointer should point to a NULL pointer or a valid buffer.
 * If a valid buffer is provided then all data will be received into that buffer.
 * Otherwise a new buffer will be allocated for the caller, and returned back to the
 * caller.
 *
 * @param sockfd The IPC file descriptor to send over.
 * @param msg A pointer to a message buffer (pointer), or a pointer to a NULL pointer.
 * @param timeoutSecs Number of seconds before timing out. -1 for infinite.
 * @return IPC code describing success level.
 */
IPCCode transport_recv(int sockfd, IPCMessage** msg, long timeoutSecs);

/** Send an IPC message through an IPC channel.
 *
 * A special control block <em>may</em> be provided so that a response may
 * be associated with its originating request. If no control block is specified (NULL)
 * then the message will be sent using a generated control block.
 *
 * @param sockfd The IPC file descriptor to send over.
 * @param control A control block that allows a response to be associated with a request.
 * @param msg An IPC message to be sent to a listener.
 * @param timeoutSecs Number of seconds before timing out. -1 for infinite.
 * @return IPC code describing success level.
 */
IPCCode transport_sendmsg(int sockfd, transport_control_t* control, const IPCMessage* msg, time_t timeoutSecs);

/** Receive an IPC message through an IPC channel.
 *
 * A special control block <em>may</em> be provided so that a response may
 * be associated with its originating request. If no control block is specified (NULL)
 * then the message will be received using a generated control block that will not be passed
 * back to the caller.
 *
 * The message pointer should point to a NULL pointer or a valid buffer.
 * If a valid buffer is provided then all data will be received into that buffer.
 * Otherwise a new buffer will be allocated for the caller, and returned back to the
 * caller.
 *
 * @param sockfd The IPC file descriptor to send over.
 * @param control A control block that allows a response to be associated with a request.
 * @param msg A pointer to a message buffer (pointer), or a pointer to a NULL pointer.
 * @param timeoutSecs Number of seconds before timing out. -1 for infinite.
 * @return IPC code describing success level.
 */
IPCCode transport_recvmsg(int sockfd, transport_control_t* control, IPCMessage** msg, long timeoutSecs);

/**
 * Abort a transaction by shutting down and closing the client socket. To be used when a message cannot be processed after
 * receiving a request
 * @param control
 */
void transport_abortmsg(transport_control_t *control);

/**
 * Shutdown any pending ipc sender messages
 */
void transport_shutdown();

#endif //ZILKER_TRANSPORT_H
