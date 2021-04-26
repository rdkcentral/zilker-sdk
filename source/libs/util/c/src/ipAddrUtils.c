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
 * ipAddrUtils.c
 *
 * set of functions to aid with IP Address needs
 *
 * Author: jelderton -  8/23/16.
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <ifaddrs.h>

#include <icBuildtime.h>
#include "icUtil/ipAddrUtils.h"

//getifaddrs not available on Android platforms, but this API is only used in native network service anyway.

/*
 * return the local IPV4 address of a particular interface
 * (ex: eth0, wifi0, ppp0).  caller must free the returned
 * string (if not NULL).
 */
char *getInterfaceIpAddressV4(const char *ifname)
{
    struct ifaddrs *ifaddrList, *ptr;

    // sanity check
    //
    if (ifname == NULL || strlen(ifname) == 0)
    {
        return NULL;
    }

    // get all of the network interfaces, as a linked list
    //
    if (getifaddrs(&ifaddrList) == -1)
    {
        return NULL;
    }

    // walk the linked list until we find a match to 'ifname'
    //
    char *retVal = NULL;
    for (ptr = ifaddrList ; ptr != NULL ; ptr = ptr->ifa_next)
    {
        if (ptr->ifa_addr == NULL || ptr->ifa_name == NULL)
        {
            // incomplete, skip this one
            //
            continue;
        }

        // see if match
        //
        if (strcmp(ptr->ifa_name, ifname) == 0)
        {
            // got the interface we're looking for, make sure it supports IPv4
            //
            if (ptr->ifa_addr->sa_family == AF_INET)    // AF_INET6 for IPv6
            {
                // get the IP in human readable string
                //
                char host[NI_MAXHOST];
                if (getnameinfo(ptr->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0)
                {
                    // save and break our loop
                    //
                    retVal = strdup(host);
                    break;
                }
            }
        }
    }

    // cleanup and return
    //
    freeifaddrs(ifaddrList);
    return retVal;
}

/*
 * returns if the supplied 'hostname' is resolvable
 */
bool isHostnameResolvable(const char *hostname)
{
    bool retVal = false;

    // prepare objects for the lookup call
    //
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;        // ip v4 and v6

    // perform the lookup
    //
    int rc = getaddrinfo(hostname, NULL, &hints, &results);
    if (rc == 0)
    {
        // success
        freeaddrinfo(results);
        retVal = true;
    }

    return retVal;
}

/*
 * returns the IP address of the supplied 'hostname'.
 * caller must free the returned string.
 */
char *resolveHostname(const char *hostname)
{
    char *retVal = NULL;

    // prepare objects for the lookup call
    //
    struct addrinfo hints;
    struct addrinfo *results = NULL;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;        // ip v4 and v6

    // perform the lookup
    //
    int rc = getaddrinfo(hostname, NULL, &hints, &results);
    if (rc == 0)
    {
        // convert to IP address string
        //
        char buffer[1024];
        struct addrinfo *ptr = results;
        if (getnameinfo(ptr->ai_addr, ptr->ai_addrlen, buffer, 1023, NULL, 0, NI_NUMERICHOST) == 0)
        {
            retVal = strdup(buffer);
        }
        freeaddrinfo(results);
    }

    return retVal;


#if 0
    // TODO: add 'forCell' parm and use DnsClient on Android to resolve for cellular

    struct hostent entry;
    struct hostent *hp;
    char *temp = NULL;
    int tempLen = 1024;
    int rc;

    // to allow for thread safety, use the '_r' version of gethostbyname.
    // it requires a block of 'temp' memory which may need to be increased
    //
    temp = (char *)malloc(sizeof(char) * tempLen);

    // make the call in a loop so we can increase the temp space if needed
    //
    while (gethostbyname_r(hostname, &entry, temp, (size_t)(tempLen-1), &hp, &rc) == ERANGE)
    {
        // double the space of 'temp' and try again
        //
        tempLen *= 2;
        temp = (char *)realloc(temp, sizeof(char) * tempLen);
        if (temp == NULL)
        {
            // out of mem?
            return false;
        }
    }

    // see if we got back a valid structure
    //
    if (hp != NULL && hp->h_addr_list != NULL)
    {
        char buffer[256];

        // convert to IP
        //
        if (hp->h_addrtype == AF_INET)
        {
            // IPv4 format
            struct in_addr **addressList = (struct in_addr **) hp->h_addr_list;
            char *h = inet_ntoa(*addressList[0]);
            snprintf(buffer, 255, "%s", h);
        }
        else if (hp->h_addrtype == AF_INET6)
        {
            // IPv6 format
            struct in6_addr **addressList = (struct in6_addr **)hp->h_addr_list;
            inet_ntop(AF_INET6, addressList[0], buffer, 255);
        }

        // cleanup and return
        //
        free(temp);
        return strdup(buffer);
    }

    // return NULL so caller doesn't think this was successful
    //
    free(temp);
    return NULL;
#endif
}

/*
 * returns if the supplied string is an IP address.
 * note that this handles both IPv4 and IPv6 strings.
 */
bool isValidIpAddress(const char *ipAddr)
{
    // create a buffer to store the result into.
    // we're not going to process this output, so just
    // use a char buffer
    //
    unsigned char buf[sizeof(struct in6_addr)];

    // first try IPv4
    if (inet_pton(AF_INET, ipAddr, &buf) == 1)
    {
        // is valid IPv4 address
        return true;
    }

    // now try IPv6
    if (inet_pton(AF_INET6, ipAddr, &buf) == 1)
    {
        // is valid IPv6 address
        return true;
    }

    return false;
}
