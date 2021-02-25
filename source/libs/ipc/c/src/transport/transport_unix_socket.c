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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/un.h>
#include <arpa/inet.h>

#include <icTypes/icHashMap.h>

#include <icLog/logging.h>
#include <icIpc/ipcSender.h>
#include <icIpc/ipcMessage.h>
#include <pthread.h>
#include <icIpc/baseEvent.h>

#include "../serviceRegistry.h"
#include "transport.h"

/* Special "code" to notify a REP socket it is being shutdown. */
#define INTERNAL_SHUTDOWN_MSG "byebye"

/* THe number of bytes representing a socket for the hash map. */
#define SOCKET_KEY_LEN sizeof(int)

/* same MAX buffer size as in Java.  Due to the fact Strings are
 * sent as "short + char *" so that Java can use "readUTF"
 * and ensure compatibility between C & Java processes.
 */
#define SUB_RECV_BUFFER_SIZE (64 * 1024)

/* Control block that will may be passed to the response handler
 * when receiving a request. This way the response may be
 * associated with the proper request.
 */
typedef struct control_block {
    int client_sockfd;
} control_block_t;

/* Internal data that represents the socket
 * being used, and the file descriptors to shutdown
 * a socket gracefully.
 */
typedef struct transport_ipc
{
    int sockfd;

    int shutdown_sock_writefd;
    int shutdown_sock_readfd;
} transport_ipc_t;

typedef struct
{
    uint16_t localPort;
    uint16_t peerPort;
} ConnectionInfo;

static int ipcSenderShutdownWriteFd = -1;
static int ipcSenderShutdownReadFd = -1;
static icHashMap* socketMap = NULL;
static pthread_mutex_t map_mutex = PTHREAD_MUTEX_INITIALIZER;

/* One time allocation for the multicast socket
 * description. This will be reused over and over.
 */
static struct sockaddr_in pubsub_multicast_sockaddr;

/** Get the socket hash map, and create it if
 * it is not already there.
 *
 * We use the singleton as we do not have an official 'init'
 * routine. Thus we need to make sure we only initialize once.
 *
 * This routine is thread safe because all places that
 * access the map lock beforehand.
 *
 * @return The socket hash map.
 */
static icHashMap* get_map(void)
{
    if (socketMap == NULL) {
        socketMap = hashMapCreate();
    }

    return socketMap;
}

static void hashmap_free_handler(void* key, void* value)
{
    transport_ipc_t* transport = (transport_ipc_t*) value;

    /* cleanup sockets */
    if (transport->shutdown_sock_writefd > 0 &&
        transport->shutdown_sock_writefd != ipcSenderShutdownWriteFd) {
        close(transport->shutdown_sock_writefd);
        transport->shutdown_sock_writefd = -1;
    }

    /* Go ahead and shutdown any senders as well. */
    if (transport->shutdown_sock_readfd > 0 &&
        transport->shutdown_sock_readfd != ipcSenderShutdownReadFd) {
        close(transport->shutdown_sock_readfd);
        transport->shutdown_sock_readfd = -1;
    }

    if (transport->sockfd) {
        shutdown(transport->sockfd, SHUT_RDWR);
        close(transport->sockfd);
        transport->sockfd = -1;
    }

    free(transport);
}

static ConnectionInfo getConnectionInfo(int socket)
{
    ConnectionInfo info;
    memset(&info, 0, sizeof(info));

    if (socket != -1)
    {
        struct sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);

        if (getpeername(socket, (struct sockaddr *) &addr, &addrLen) == 0)
        {
            info.peerPort = ntohs(addr.sin_port);
        }

        if (getsockname(socket, (struct sockaddr *) &addr, &addrLen) == 0)
        {
            info.localPort = ntohs(addr.sin_port);
        }
    }

    return info;
}

void* transport_malloc_msg(size_t size)
{
    return malloc(size);
}

void* transport_realloc_msg(void* ptr, size_t size)
{
    return realloc(ptr, size);
}

void transport_free_msg(void* ptr)
{
    if (ptr) free(ptr);
}

int transport_connect(const char* host, uint16_t servicePortNum)
{
    int sockfd;
    struct sockaddr_in addr;
    transport_ipc_t* transport;

    // fill in the socket structure with target port,
    // assuming this is the LOCAL_LOOPBACK
    //
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(servicePortNum);
    inet_aton(host, &(addr.sin_addr));

    // create a bidirectional TCP "stream" socket
    //
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        icLogError(API_LOG_CAT, "error opening socket on servicePort %"PRIu16": %s", servicePortNum, strerror(errno));
        return -1;
    }

#ifdef SO_NOSIGPIPE
    // set socket option to not SIGPIPE on write errors.  not available on some platforms.
    // hopefully if not, they support MSG_NOSIGNAL - which should be used on the send() call
    //
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable)) != 0)
    {
        icLogWarn(API_LOG_CAT, "unable to set SO_NOSIGPIPE flag on socket to servicePort %d", servicePortNum);
    }
#endif

    // connect to servicePortNum on LOCAL_LOOPBACK
    //
#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "connecting to %s:%" PRIu16, host, servicePortNum);
#endif
    if (connect(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1)
    {
        // failed to connect to service, close up the socket
        //
        icLogWarn(API_LOG_CAT, "error connecting to socket on servicePort %d: %s", servicePortNum, strerror(errno));
//        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return -1;
    }

    transport = malloc(sizeof(transport_ipc_t));
    if (transport == NULL) {
        icLogWarn(API_LOG_CAT, "Error reserving memory for socket");
        close(sockfd);
        return -1;
    }

    transport->sockfd = sockfd;

    pthread_mutex_lock(&map_mutex);
    if (ipcSenderShutdownWriteFd == -1)
    {
        int32_t cancelPipe[2];
        if (pipe(cancelPipe) == 0)
        {
            ipcSenderShutdownReadFd = cancelPipe[0];
            ipcSenderShutdownWriteFd = cancelPipe[1];
        }
    }
    transport->shutdown_sock_readfd = ipcSenderShutdownReadFd;
    transport->shutdown_sock_writefd = ipcSenderShutdownWriteFd;
    hashMapPut(get_map(), &transport->sockfd, SOCKET_KEY_LEN, transport);
    pthread_mutex_unlock(&map_mutex);

    return sockfd;
}

int transport_establish(const char* host, uint16_t servicePortNum)
{
    transport_ipc_t* transport;

    // create the socket for the supplied port
    //
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        icLogWarn(API_LOG_CAT, "unable to create listening socket: %s", strerror(errno));
        return -1;
    }

    // set socket options to reuse the address & port
    //
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0)
    {
        icLogWarn(API_LOG_CAT, "unable to set SO_REUSEADDR flag on service socket");
    }
#ifdef SO_NOSIGPIPE
    // not available on some platforms.  hopefully if not, they support MSG_NOSIGNAL -
    // which should be used on the send() calls
    if (setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &enable, sizeof(enable)) != 0)
    {
        icLogWarn(API_LOG_CAT, "unable to set SO_NOSIGPIPE flag on service socket");
    }
#endif
#ifdef SO_REUSEPORT
    // not available on some platforms like ngHub and xb3
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) != 0)
    {
        icLogWarn(API_LOG_CAT, "unable to set SO_REUSEPORT flag on service socket");
    }
#endif

    // now, create our addr, and bind to it
    //
    struct sockaddr_in addr;
    memset(&addr,0,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(servicePortNum);
    inet_aton(host, &(addr.sin_addr));
    int rc = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0)
    {
        icLogWarn(API_LOG_CAT, "Unable to bind to listening port: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    // establish the listen
    //
    rc = listen(sockfd, 50);
    if (rc < 0)
    {
        icLogWarn(API_LOG_CAT, "Unable to establish listen on socket: %s", strerror(errno));
        close(sockfd);
        return -1;
    }

    /* Now that we know we have successfully created the socket go ahead and setup the transport layer. */
    transport = malloc(sizeof(transport_ipc_t));
    if (transport == NULL) {
        icLogWarn(API_LOG_CAT, "Error reserving memory for socket");
        close(sockfd);
        return -1;
    }

    transport->sockfd = sockfd;

    /* Special pipe to gracefully shutdown a request handler. */
    int32_t cancelPipe[2];
    if (pipe(cancelPipe) == 0) {
        transport->shutdown_sock_readfd = cancelPipe[0];
        transport->shutdown_sock_writefd = cancelPipe[1];
    } else {
        transport->shutdown_sock_readfd = transport->shutdown_sock_writefd = -1;
    }

    pthread_mutex_lock(&map_mutex);
    hashMapPut(get_map(), &transport->sockfd, SOCKET_KEY_LEN, transport);
    pthread_mutex_unlock(&map_mutex);

    return sockfd;
}

int transport_pub_register(const char* channel)
{
    int sockfd;
    int enable = 1;

    transport_ipc_t* transport;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        icLogError(API_LOG_CAT, "failed to create event producer socket: %s", strerror(errno));
        return -1;
    }

    // set socket options to reuse the address & port
    //
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) != 0)
    {
        icLogWarn(API_LOG_CAT, "unable to set SO_REUSEADDR flag on event socket");
    }

#ifdef SO_REUSEPORT
    // not available on some platforms like ngHub and xb3
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) != 0)
    {
        icLogWarn(API_LOG_CAT, "unable to set SO_REUSEPORT flag on event socket");
    }
#endif

    /* Setup the Multicast socket information. This is
     * *always* the same. Thus we only set it on register, and
     * we don't really care if it gets set multiple times.
     * By setting it up here we save a few (very few) cycles
     * during the broadcast.
     */
    pubsub_multicast_sockaddr.sin_family = AF_INET;
    pubsub_multicast_sockaddr.sin_port = htons(EVENT_BROADCAST_PORT);
    pubsub_multicast_sockaddr.sin_addr.s_addr = inet_addr(IC_EVENT_MULTICAST_GROUP);

    // limit broadcasting to the local-loopback interface
    //
    struct in_addr loopback;
    loopback.s_addr = inet_addr(LOCAL_LOOPBACK);
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, &loopback, sizeof(loopback)) != 0)
    {
        icLogWarn(API_LOG_CAT, "unable to set IP_MULTICAST_IF flag on event socket");
    }

    transport = malloc(sizeof(transport_ipc_t));
    if (transport == NULL) {
        icLogWarn(API_LOG_CAT, "Error reserving memory for socket");
        close(sockfd);
        return -1;
    }

    transport->sockfd = sockfd;
    transport->shutdown_sock_writefd = transport->shutdown_sock_readfd = -1;

    pthread_mutex_lock(&map_mutex);
    hashMapPut(get_map(), &transport->sockfd, SOCKET_KEY_LEN, transport);
    pthread_mutex_unlock(&map_mutex);

    return sockfd;
}

int transport_sub_register(const char* channel)
{
    int sockfd;
    int reuse = 1;

    transport_ipc_t* transport;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        icLogError(API_LOG_CAT, "unable to create event listening socket : %s", strerror(errno));
        return -1;
    }

    // allow multiple sockets/receivers to bind to this port number
    //
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*) &reuse, sizeof(reuse)) < 0)
    {
        icLogError(API_LOG_CAT, "unable to set SO_REUSEPORT for event listener : %s", strerror(errno));
//        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return -1;
    }

#ifdef SO_REUSEPORT
    // not available on some platforms like ngHub and xb3
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) != 0)
    {
        icLogWarn(API_LOG_CAT, "unable to set SO_REUSEPORT for event listener : %s", strerror(errno));
    }
#endif

    // bind to the port all services broadcast events on
    //
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
//    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
//    inet_aton(LOCAL_LOOPBACK, &(addr.sin_addr));
    addr.sin_port = htons(EVENT_BROADCAST_PORT);

    if (bind(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0)
    {
        icLogError(API_LOG_CAT, "unable to bind listener : %s", strerror(errno));
//        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return -1;
    }

    // join the multicast group, but only listen on local-loopback
    //
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(IC_EVENT_MULTICAST_GROUP);
//    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    mreq.imr_interface.s_addr = htonl(INADDR_LOOPBACK);

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
    {
        icLogError(API_LOG_CAT, "unable to join multicast group for listener : %s", strerror(errno));
//        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return -1;
    }

    transport = malloc(sizeof(transport_ipc_t));
    if (transport == NULL) {
        icLogWarn(API_LOG_CAT, "Error reserving memory for socket");
        close(sockfd);
        return -1;
    }

    transport->sockfd = sockfd;
    int32_t cancelPipe[2];
    if (pipe(cancelPipe) == 0)
    {
        transport->shutdown_sock_readfd = cancelPipe[0];
        transport->shutdown_sock_writefd = cancelPipe[1];
    } else {
        transport->shutdown_sock_readfd = transport->shutdown_sock_writefd = -1;
    }

    pthread_mutex_lock(&map_mutex);
    hashMapPut(get_map(), &transport->sockfd, SOCKET_KEY_LEN, transport);
    pthread_mutex_unlock(&map_mutex);

    return sockfd;
}

int transport_get_shutdown_sock_readfd(int sockfd)
{
    int shutdown_sock_readfd = -1;
    if (sockfd > 0) {
        pthread_mutex_lock(&map_mutex);
        // find our map entry for this socket
        //
        transport_ipc_t *transport = (transport_ipc_t *) hashMapGet(get_map(), &sockfd, SOCKET_KEY_LEN);
        if (transport != NULL) {
            shutdown_sock_readfd = transport->shutdown_sock_readfd;
        }
        pthread_mutex_unlock(&map_mutex);
    }


    return shutdown_sock_readfd;
}

/*
 * attempt to close the transport associated with 'sockfd'.  if this
 * is a 'server socket' (has a corresponding shutdown pipe), then will
 * send the internal 'shutdown' message to the pipe so that it will stop
 * listening to incoming requests and exit the thread.  once called,
 * the approach should be to wait for the thread to exit (via thread_join)
 */
void transport_close(int sockfd)
{
    if (sockfd > 0) {
        pthread_mutex_lock(&map_mutex);

        // find our map entry for this socket, then send the "INTERNAL_SHUTDOWN_MSG"
        // message to break the transport_recvmsg() and ultimately exit the thread.
        //
        transport_ipc_t *transport = (transport_ipc_t*) hashMapGet(get_map(), &sockfd, SOCKET_KEY_LEN);
        if (transport != NULL)
        {
            // see if this has the corresponding pipe
            //
            if (transport->shutdown_sock_writefd > 0 &&
                transport->shutdown_sock_writefd != ipcSenderShutdownWriteFd) {
                /* Tell a waiting request handler that we are shutting down. */
                write(transport->shutdown_sock_writefd,
                      INTERNAL_SHUTDOWN_MSG,
                      strlen(INTERNAL_SHUTDOWN_MSG));
                // now let the thread do the cleanup of our 'transport'
            }
            else
            {
                // unable to send the shutdown message, so just cleanup
                hashMapDelete(get_map(), &sockfd, SOCKET_KEY_LEN, hashmap_free_handler);
            }
        }
        pthread_mutex_unlock(&map_mutex);
    }
}

void transport_abortmsg(transport_control_t *control)
{
    if(control != NULL && control->control_block != NULL)
    {
        control_block_t *controlBlock = control->control_block;

        if (controlBlock->client_sockfd != -1)
        {
            shutdown(controlBlock->client_sockfd, SHUT_RDWR);
            close(controlBlock->client_sockfd);

            free(controlBlock);
            control->control_block = NULL;
        }
    }
}

int transport_subscribe(int sockfd, int id)
{
    /* Currently we only support the ALL subscription. */
    return (id == TRANSPORT_SUBSCRIBE_ALL) ? 0 : -1;
}

int transport_sub_recv(int sockfd, cJSON** event)
{
    char* buffer = NULL;

    // follow the same parse steps as our Java counterpart:
    //  1.  read from socket until something arrives
    //  2.  convert read bytes into string
    //  3.  place into JSON buffer, forward to handler
    //
    struct sockaddr_in remoteAddr;
    unsigned int remoteAddrLen = sizeof(struct sockaddr_in);

//    buffer = malloc(SUB_RECV_BUFFER_SIZE);
    buffer = calloc(SUB_RECV_BUFFER_SIZE, sizeof(char));
    if (buffer == NULL) {
        icLogError(API_LOG_CAT, "Error allocating sub buffer: %s", strerror(errno));
        return -1;
    }

    /* Make sure first byte is a null terminator. If we fail to
     * read then this check will return a zero length string. If
     * we do read then we will pick up the null terminator in the
     * transmit.
     */
//    buffer[0] = '\0';

    ssize_t nbytes = recvfrom(sockfd,
                              buffer,
                              SUB_RECV_BUFFER_SIZE,
                              0,
                              (struct sockaddr*) &remoteAddr,
                              &remoteAddrLen);
    if(nbytes < 0) {
        // read error, loop back around to check if we should cancel
        //
        if (errno != EBADF) {
            icLogError(API_LOG_CAT, "failed to receive event: %s", strerror(errno));
        }

        free(buffer);
        return -1;
    }

    if(nbytes < sizeof(short)) {
        // read too short, loop back around to check if we should cancel
        //
        icLogWarn(API_LOG_CAT, "received incomplete event header; %ld bytes", nbytes);
        free(buffer);
        return -1;
    }

//icLogDebug(API_LOG_CAT, "read event '%s'", buffer);
    *event = cJSON_Parse(buffer);

    free(buffer);

    if (*event == NULL) {
        icLogError(API_LOG_CAT, "error parsing incoming subscription event.");
        return -1;
    }

    return 0;
}

void transport_publish(int sockfd, const cJSON* event)
{
    char *encodeStr = cJSON_PrintUnformatted(event);

    if (encodeStr) {
        if (sendto(sockfd,
                   encodeStr,
                   strlen(encodeStr) + 1,
                   0,
                   (struct sockaddr*) &pubsub_multicast_sockaddr,
                   sizeof(struct sockaddr_in)) < 0) {
            icLogWarn(API_LOG_CAT, "failed to send event: %d - %s", errno, strerror(errno));
        }

        free(encodeStr);
    }
}

IPCCode transport_send(int sockfd, const IPCMessage* msg, time_t timeoutSecs)
{
    return transport_sendmsg(sockfd, NULL, msg, timeoutSecs);
}

IPCCode transport_sendmsg(int sockfd, transport_control_t* control, const IPCMessage* msg, time_t timeoutSecs)
{
    transport_ipc_t* transport;
    control_block_t* cblock = NULL;
    int socket;

    pthread_mutex_lock(&map_mutex);
    transport = (transport_ipc_t*) hashMapGet(get_map(), &sockfd, SOCKET_KEY_LEN);
    pthread_mutex_unlock(&map_mutex);

    if (transport == NULL) {
        icLogWarn(API_LOG_CAT, "send: failed to send message could not find socket %" PRIi32, msg->msgCode);
        return IPC_INVALID_ERROR;
    }

    if ((control != NULL) && (control->control_block != NULL)) {
        cblock = (control_block_t*) control->control_block;
        socket = cblock->client_sockfd;
    } else {
        socket = sockfd;
    }

    // first, make sure the socket is ready for communication
    //
    if (timeoutSecs > 0)
    {
        if (canWriteToSocket(socket, timeoutSecs) == false)
        {
            icLogWarn(API_LOG_CAT, "send: socket is not valid for writing; bailing on sending msgCode=%" PRIi32, msg->msgCode);
            return IPC_TIMEOUT;
        }
    }

    // send the request message, one part at a time.  although inefficient,
    // it allows us to not be concerned if the target Service is Java or Native.
    // To keep the protocol the same across all clients (Java & C)
    // and with all services (Java & C), send the message in the following order:
    //  1. message code
    //  2. payload length
    //  if (payload len > 0)
    //     3. payload
    //
#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "send: sending message code %" PRIi32, msg->msgCode);
#endif

    int sendFlags = 0;
#ifdef MSG_NOSIGNAL
    // compilers that support the MSG_NOSIGNAL are probably the ones that do NOT
    // support the SO_NOSIGPIPE socket option.  just trying to make sure we don't
    // cause a SIGPIPE from within this library
    //
    sendFlags = MSG_NOSIGNAL;
#endif

    // first the "message code"
    //
    int32_t msgCode = htonl((uint32_t)msg->msgCode);
    if (send(socket, &msgCode, sizeof(int32_t), sendFlags) < 0)
    {
        icLogWarn(API_LOG_CAT, "send: failed to send message code %" PRIi32, msg->msgCode);
        return IPC_SEND_ERROR;
    }

#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "send: sending message len %" PRIu16, msg->payloadLen);
#endif

    // now send the "payload length" (even if 0 since the Service needs to know)
    // looks odd, but we need to send as uint16_t in case the target Service is Java,
    // which reads the string as a java-short + characters.  This gives us a maximum
    // possible payload of 65535 characters
    //
    uint32_t msgLen = htonl(msg->payloadLen);
    if (send(socket, &msgLen, sizeof(uint32_t), sendFlags) < 0)
    {
        icLogWarn(API_LOG_CAT, "send: failed to send message length, msgCode=%" PRIi32, msg->msgCode);
        return IPC_SEND_ERROR;
    }

    // finally send the message payload (if there is one)
    //
    if (msg->payloadLen > 0)
    {
#ifdef CONFIG_DEBUG_IPC
        icLogDebug(API_LOG_CAT, "send: sending message body %s", (char *)msg->payload);
#endif
        // send payload (synchronous)
        //
        if (send(socket, msg->payload, sizeof(char) * msg->payloadLen, sendFlags) < 0)
        {
            icLogWarn(API_LOG_CAT, "send: failed to send message payload, msgCode=%" PRIi32, msg->msgCode);
            return IPC_SEND_ERROR;
        }
    }

    if (cblock) {
        /* We must shutdown the client socket. This mimics other RPC mechanisms that
         * hide the fact that they are dealing with "client" sockets.
         */
        close(cblock->client_sockfd);
        free(cblock);

        control->control_block = NULL;
        control->length = 0;
    }

    // no problems, return success
    //
    return IPC_SUCCESS;
}

IPCCode transport_recv(int sockfd, IPCMessage** msg, time_t timeoutSecs)
{
    return transport_recvmsg(sockfd, NULL, msg, timeoutSecs);
}

IPCCode transport_recvmsg(int sockfd, transport_control_t* control, IPCMessage** msg, long timeoutSecs)
{
    transport_ipc_t* transport;
    control_block_t* cblock = NULL;

    IPCMessage* internal_msg;
    int socket;

    pthread_mutex_lock(&map_mutex);
    transport = (transport_ipc_t*) hashMapGet(get_map(), &sockfd, SOCKET_KEY_LEN);
    int shutdownSockReadFd = -1;
    if (transport != NULL)
    {
        shutdownSockReadFd = transport->shutdown_sock_readfd;
    }
    pthread_mutex_unlock(&map_mutex);

    if (transport == NULL) {
        icLogWarn(API_LOG_CAT, "error: invalid socket requested.");
        return IPC_INVALID_ERROR;
    }

    if (control) {
        struct sockaddr_un clientAddr;
        int addrSize = sizeof(struct sockaddr_un);
        int ret;

        ret = canReadFromServiceSocket(sockfd, shutdownSockReadFd, timeoutSecs);
        switch (ret) {
            case 0:
                break;
            case EAGAIN:
            case ETIMEDOUT:
                return IPC_TIMEOUT;
            case EINTR:
                // told via 'transport_close' to shutdown, so cleanup our transport object and bail
                icLogDebug(API_LOG_CAT, "closing transport due to shutdown request");
                pthread_mutex_lock(&map_mutex);
                hashMapDelete(get_map(), &sockfd, SOCKET_KEY_LEN, hashmap_free_handler);
                pthread_mutex_unlock(&map_mutex);
                return IPC_SERVICE_DISABLED;
            default:
                icLogError(API_LOG_CAT, "error: Failed waiting for client. [%s]", strerror(ret));
                return IPC_READ_ERROR;
        }

        memset(&clientAddr, 0, sizeof(clientAddr));
        ret = accept(sockfd, (struct sockaddr *)&clientAddr, (socklen_t *)&addrSize);
        if (ret < 0) {
            switch (errno) {
                case EBADF:
                    // told via 'transport_close' to shutdown, so cleanup our transport object and bail
                    icLogDebug(API_LOG_CAT, "closing transport due to I/O error");
                    pthread_mutex_lock(&map_mutex);
                    hashMapDelete(get_map(), &sockfd, SOCKET_KEY_LEN, hashmap_free_handler);
                    pthread_mutex_unlock(&map_mutex);
                    return IPC_SERVICE_DISABLED;

                case EINVAL:
                case ENOMEM:
                case ENOBUFS:
                case EMFILE:
                case ENFILE:
                    icLogError(API_LOG_CAT, "error: Failed accepting client. [%s]", strerror(errno));
                    return IPC_READ_ERROR;

                default:
                    return IPC_TIMEOUT;
            }
        }

        cblock = malloc(sizeof(control_block_t));
        socket = cblock->client_sockfd = ret;
    } else {
        socket = sockfd;
    }

    // make sure the socket is valid and has data to read
    //
    if (timeoutSecs > 0)
    {
        int ret = canReadFromServiceSocket(socket, shutdownSockReadFd, timeoutSecs);
        switch (ret) {
            case 0:
                break;
            case EAGAIN:
            case ETIMEDOUT:
            {
                ConnectionInfo info = getConnectionInfo(socket);
                icLogWarn(API_LOG_CAT,
                          "read: message read timeout after %ld seconds (peer=[%d] <=> local=[%d])",
                          timeoutSecs,
                          info.peerPort,
                          info.localPort);

                if (cblock) free(cblock);
                return IPC_TIMEOUT;
            }
            case EINTR:
                // Shutdown got tickled, abort
                icLogDebug(API_LOG_CAT, "read: interrupted");
                if (cblock) free(cblock);
                return IPC_SERVICE_DISABLED;
            default:
                icLogError(API_LOG_CAT, "error: Failed waiting for client. [%s]", strerror(ret));
                if (cblock) free(cblock);
                return IPC_READ_ERROR;
        }
    }

    // read the request message, one part at a time.  although inefficient,
    // it allows us to not be concerned if the target Service is Java or Native.
    // To keep the protocol the same across all clients (Java & C)
    // and with all services (Java & C), read the message in the following order:
    //  1. message code
    //  2. payload length
    //  if (payload len > 0)
    //     3. payload
    //

    // first get the response code
    //
    int32_t msgCode = 0;
    ssize_t len = recv(socket, &msgCode, sizeof(int32_t), MSG_WAITALL);
    if (len == 0)
    {
        icLogWarn(API_LOG_CAT, "read: client closed connection");
        free(cblock);
        return IPC_READ_ERROR;
    }
    else if (len < 0)
    {
        icLogWarn(API_LOG_CAT, "read: failed to read message code");
        if (cblock) free(cblock);
        return IPC_READ_ERROR;
    }

    msgCode = (int32_t) ntohl((uint32_t)msgCode);

#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "read: received message code %" PRIi32, msgCode);
#endif

    // now length of response payload.  Like the send above, use uint16_t
    // in case the sender is Java which sends strings as java-int + characters
    //
    uint32_t msgLen = 0;
    if (recv(socket, &msgLen, sizeof(uint32_t), MSG_WAITALL) <= 0)
    {
        icLogWarn(API_LOG_CAT, "read: failed to read message size, msgCode=%" PRIi32, msgCode);
        if (cblock) free(cblock);
        return IPC_READ_ERROR;
    }

    msgLen = ntohl(msgLen);

#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "read: received message length %" PRIu16, msgLen);
#endif

    // if there is a length to the payload, get the rest
    //
    void* payload = NULL;

    if (msgLen > 0)
    {
        // lastly read the payload, but first need to allocate memory for it
        // NOTE: add one char for the NULL at the end of the payload string
        //       (in case it's JSON).  Chances are the 'send' routine already
        //       accounted for that, but doesn't hurt to allow for it here
        //
        payload = calloc((msgLen + 1), sizeof(char));
        errno = 0;
        len = recv(socket, payload, sizeof(char) * msgLen, MSG_WAITALL);
        if (len <= 0)
        {
            const char *err = len == 0 ? "client disconnected" : strerror(errno);
            if (errno != EBADF) {
                // cleanup memory we allocated
                //
                icLogWarn(API_LOG_CAT,
                          "read: failed to read message payload; msgCode=%"PRIi32 "; error=%s",
                          msgCode,
                          err);
            }

            free(payload);
            free(cblock);

            return IPC_READ_ERROR;
        }

#ifdef CONFIG_DEBUG_IPC
        icLogDebug(API_LOG_CAT, "read: received message body %s", (char*) payload);
#endif
    }

    internal_msg = (*msg == NULL) ? createIPCMessage() : *msg;
    internal_msg->msgCode = msgCode;
    internal_msg->payloadLen = msgLen;
    internal_msg->payload = payload;

    *msg = internal_msg;

    if (control) {
        control->control_block = cblock;
    }

    // no problems, return success
    //
    return IPC_SUCCESS;
}

/**
 * Shutdown any pending ipc sender messages
 */
void transport_shutdown()
{
    pthread_mutex_lock(&map_mutex);
    if (ipcSenderShutdownWriteFd != -1)
    {
        // Tickle the shutdown pipe to get them to abort
        write(ipcSenderShutdownWriteFd,
              INTERNAL_SHUTDOWN_MSG,
              strlen(INTERNAL_SHUTDOWN_MSG));
        close(ipcSenderShutdownWriteFd);
        close(ipcSenderShutdownReadFd);
        // Go through and clean up all the senders
        icHashMapIterator *iter = hashMapIteratorCreate(get_map());
        while(hashMapIteratorHasNext(iter))
        {
            int *key;
            uint16_t keyLen;
            transport_ipc_t *value;
            hashMapIteratorGetNext(iter, (void**)&key, &keyLen, (void**)&value);
            // Identify the senders by them using the global ipc sender
            // shutdown fds.  Not really ideal, but it will work
            if (value->shutdown_sock_readfd == ipcSenderShutdownReadFd)
            {
                hashMapIteratorDeleteCurrent(iter, hashmap_free_handler);
            }
        }
        hashMapIteratorDestroy(iter);
        // Must do this after the above cleanup
        ipcSenderShutdownWriteFd = -1;
        ipcSenderShutdownReadFd = -1;
    }
    pthread_mutex_unlock(&map_mutex);
}
