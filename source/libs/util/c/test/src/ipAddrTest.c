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
 * ipAddrTest.c
 *
 * test some of the ipAddrUtil.c functions
 *
 * Author: jelderton -  6/20/18.
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <icUtil/ipAddrUtils.h>
#include "ipAddrTest.h"

#define TEST_LOG        "ipAddrTest"
#define NEXUS_HOST      "tx-nexus.icontrol.com"
#define LOCAL_HOST      "localhost"
#define BOGUS_HOST      "this.willfail.invalid"
#define LOOPBACK_IFACE  "lo"

static const char *GOOD_IP_LIST[] = {
        "127.0.0.1",
        "222.222.222.0",
        "::1",                              // IPv6 version of 127.0.0.1
        "fe80::dfd2:a829:55fe:1c54",        // test IPv6
        NULL
};

static const char *BAD_IP_LIST[] = {
        "127,0,0.1",
        "www.google.com",
        "x:y:1",                            // IPv6
        NULL
};

/*
 * test getInterfaceIpAddressV4()
 */
static bool testGetInterfaceIp()
{
    // try to get the IP for our LOOPBACK interface
    char *ip = getInterfaceIpAddressV4(LOOPBACK_IFACE);
    if (ip == NULL)
    {
        printf("%s: FAILED to get IP for interface '%s'\n", TEST_LOG, LOOPBACK_IFACE);
        return false;
    }
    free(ip);
    return true;
}

/*
 * test isHostnameResolvable()
 */
static bool testIsHostnameResolvable()
{
    // resolve localhost
    if (isHostnameResolvable(LOCAL_HOST) == false)
    {
        printf("%s: FAILED to resolve host '%s'\n", TEST_LOG, LOCAL_HOST);
        return false;
    }

    // resolve tx-nexus
    if (isHostnameResolvable(NEXUS_HOST) == false)
    {
        printf("%s: FAILED to resolve host '%s'\n", TEST_LOG, NEXUS_HOST);
        return false;
    }

    // ensure we fail to resolve a bogus hostname
    if (isHostnameResolvable(BOGUS_HOST) == true)
    {
        printf("%s: INCORRECTLY resolved host '%s'\n", TEST_LOG, BOGUS_HOST);
        return false;
    }

    return true;
}

/*
 * test resolveHostname()
 */
static bool testResolveHostname()
{
    char *ipAddr = NULL;

    // resolve localhost
    if ((ipAddr = resolveHostname(LOCAL_HOST)) == NULL)
    {
        printf("%s: FAILED to resolve host '%s'\n", TEST_LOG, LOCAL_HOST);
        return false;
    }
    printf("%s: successfully resolved host '%s' to '%s'\n", TEST_LOG, LOCAL_HOST, ipAddr);
    free(ipAddr);

    // resolve tx-nexus
    if ((ipAddr = resolveHostname(NEXUS_HOST)) == NULL)
    {
        printf("%s: FAILED to resolve host '%s'\n", TEST_LOG, NEXUS_HOST);
        return false;
    }
    printf("%s: successfully resolved host '%s' to '%s'\n", TEST_LOG, NEXUS_HOST, ipAddr);
    free(ipAddr);

    // ensure we fail to resolve a bogus hostname
    if ((ipAddr = resolveHostname(BOGUS_HOST)) != NULL)
    {
        printf("%s: INCORRECTLY resolved host '%s' to '%s'\n", TEST_LOG, BOGUS_HOST, ipAddr);
        free(ipAddr);
        return false;
    }

    return true;
}

/*
 * test isValidIpAddress()
 */
static bool testIsValidIpAddress()
{
    int i;

    // try some good IPs
    //
    for (i = 0 ; GOOD_IP_LIST[i] != NULL ; i++)
    {
        if (isValidIpAddress(GOOD_IP_LIST[i]) == false)
        {
            printf("%s: FAILED to validate good IP '%s''\n", TEST_LOG, GOOD_IP_LIST[i]);
            return false;
        }
    }

    // now some bad IPs
    //
    for (i = 0 ; BAD_IP_LIST[i] != NULL ; i++)
    {
        if (isValidIpAddress(BAD_IP_LIST[i]) == true)
        {
            printf("%s: INCORRECTLY validated IP '%s''\n", TEST_LOG, BAD_IP_LIST[i]);
            return false;
        }
    }

    return true;
}

/*
 * main entry point for these tests
 */
bool runIpAddrTests()
{
    printf("%s: testing ability to get IP addresses for network interfaces\n", TEST_LOG);
    if (testGetInterfaceIp() == false)
    {
        return false;
    }

    printf("%s: testing ability to resolve hostnames\n", TEST_LOG);
    if (testIsHostnameResolvable() == false)
    {
        return false;
    }

    printf("%s: testing ability to obtain IP addresses for hostnames\n", TEST_LOG);
    if (testResolveHostname() == false)
    {
        return false;
    }

    printf("%s: testing ability to validate IP addresses\n", TEST_LOG);
    if (testIsValidIpAddress() == false)
    {
        return false;
    }

    return true;
}
