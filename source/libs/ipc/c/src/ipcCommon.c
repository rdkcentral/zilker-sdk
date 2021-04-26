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
 * ipcCommon.c
 *
 * Set of private macros, defines, and functions used
 * internally as part of the IPC library implementation
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/un.h>
#include <icLog/logging.h>
#include "ipcCommon.h"

#define SERVICE_ID_JSON_KEY "_svcIdNum"

/*
 * test to see if the socket is ready for reading
 */
bool canReadFromSocket(int32_t sockFD, time_t timeoutSecs)
{
    fd_set readFds, writeFds, exceptFds;
    int rc = 0;
    struct timeval timeout;

    // setup the descriptors
    //
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    // coverity[uninit_use]
    FD_SET(sockFD, &readFds);
    // coverity[uninit_use]
    FD_SET(sockFD, &exceptFds);

    // set a timeout
    //
    if (timeoutSecs <= 0)
    {
        // never happens, but if for some reason it does - use default of 10 secs
        // really just did this to make Coverity happy, since we never call this
        // unless the timeoutSecs > 0
        //
        timeoutSecs = 10;
    }
    timeout.tv_sec = timeoutSecs;
    timeout.tv_usec = 0;

    rc = select(sockFD+1, &readFds, &writeFds, &exceptFds, &timeout);
    if (rc > 0)
    {
        // see if our socket is ready to read
        //
        if (FD_ISSET(sockFD, &readFds) != 0)
        {
            return true;
        }
    }
#ifdef CONFIG_DEBUG_IPC
    else if (rc < 0)
    {
        // error running select
        //
        icLogError(API_LOG_CAT, "--- error testing socket %s", strerror(errno));
    }
#endif

    return false;
}

/*
 * check if can read from either socket.  returns:
 * 0         - if 'serviceSock' is ready
 * ETIMEDOUT - if 'serviceSock' was not ready within the timeoutSecs
 * EINTR     - if 'shutdownSock' is ready
 * EAGAIN    - all other conditions
 */
int canReadFromServiceSocket(int32_t serviceSock, int32_t shutdownSock, time_t timeoutSecs)
{
    fd_set readFds, writeFds, exceptFds;
    int rc = 0;
    int maxFD = serviceSock;
    struct timeval timeout;

    // setup the descriptors
    //
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    // coverity[uninit_use]
    FD_SET(serviceSock, &readFds);
    // coverity[uninit_use]
    FD_SET(serviceSock, &exceptFds);

    if (shutdownSock > 0)
    {
        // add the 'shutdown pipe' to the select so we can un-block if necessary
        //
        FD_SET(shutdownSock, &readFds);
        FD_SET(shutdownSock, &exceptFds);
        if (shutdownSock > maxFD)
        {
            maxFD = shutdownSock;
        }
    }

    // set a timeout
    //
    if (timeoutSecs <= 0)
    {
        // never happens, but if for some reason it does - use default of 10 secs
        // really just did this to make Coverity happy, since we never call this
        // unless the timeoutSecs > 0
        //
        timeoutSecs = 10;
    }
    timeout.tv_sec = timeoutSecs;
    timeout.tv_usec = 0;

    // wait up to 'timout seconds' for something to appear on the 'serviceSock'
    //
    rc = select(maxFD+1, &readFds, &writeFds, &exceptFds, &timeout);
    if (rc == 0)
    {
        return ETIMEDOUT;
    }
    else if (rc > 0)
    {
        if (shutdownSock >= 0 && FD_ISSET(shutdownSock, &readFds))
        {
            return EINTR;
        }
        else if (serviceSock >= 0 && FD_ISSET(serviceSock, &readFds))
        {
            return 0;
        }
    }
    else
    {
#ifdef CONFIG_DEBUG_IPC
        if (errno == EBADF)
            return EINTR;

        // error running select
        //
        icLogError(API_LOG_CAT, "--- error testing socket %s", strerror(errno));
#endif
        return errno;
    }

    return EAGAIN;
}

/*
 * test to see if the socket is ready for writing
 */
bool canWriteToSocket(int32_t sockFD, time_t timeoutSecs)
{
    fd_set readFds, writeFds, exceptFds;
    int rc = 0;
    struct timeval timeout;

    // setup the descriptors
    //
    FD_ZERO(&readFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&exceptFds);
    // coverity[uninit_use]
    FD_SET(sockFD, &writeFds);
    // coverity[uninit_use]
    FD_SET(sockFD, &exceptFds);

    // set a timeout
    //
    if (timeoutSecs <= 0)
    {
        // never happens, but if for some reason it does - use default of 5 secs
        // really just did this to make Coverity happy.
        //
        timeoutSecs = 5;
    }
    timeout.tv_sec = timeoutSecs;
    timeout.tv_usec = 0;

    rc = select(sockFD+1, &readFds, &writeFds, &exceptFds, &timeout);
    if (rc > 0)
    {
        // see if our socket is set
        //
        if (FD_ISSET(sockFD, &writeFds) != 0)
        {
            return true;
        }
    }
#ifdef CONFIG_DEBUG_IPC
    else if (rc < 0)
    {
        // error running select
        //
        icLogError(API_LOG_CAT, "--- error testing socket %s", strerror(errno));
    }
#endif

    return false;
}

/*
 * extract the "serviceIdNum" from a raw event (to find where it came from)
 */
uint32_t extractServiceIdFromRawEvent(cJSON *buffer)
{
    uint32_t retVal = 0;

    // look for the SERVICE_ID_JSON_KEY in the buffer
    //
    cJSON *item = cJSON_GetObjectItem(buffer, SERVICE_ID_JSON_KEY);
    if (item != NULL)
    {
        retVal = (uint32_t)item->valuedouble;
    }

    return retVal;
}

/*
 * embed the "serviceIdNum" into a raw event (to indicate where the event originated from)
 */
void insertServiceIdToRawEvent(cJSON *buffer, uint32_t serviceIdNum)
{
    cJSON_AddItemToObjectCS(buffer, SERVICE_ID_JSON_KEY, cJSON_CreateNumber(serviceIdNum));
}


