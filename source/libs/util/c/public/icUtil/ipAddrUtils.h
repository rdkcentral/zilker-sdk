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
 * ipAddrUtils.h
 *
 * set of functions to aid with IP Address needs
 *
 * Author: jelderton -  8/23/16.
 *-----------------------------------------------*/

#ifndef ZILKER_IPADDRUTILS_H
#define ZILKER_IPADDRUTILS_H

#include <stdbool.h>
#include <icBuildtime.h>

/*
 * return the local IPV4 address of a particular interface
 * (ex: eth0, wifi0, ppp0).  caller must free the returned
 * string (if not NULL).
 */
char *getInterfaceIpAddressV4(const char *ifname);

/*
 * returns if the supplied 'hostname' is resolvable
 */
bool isHostnameResolvable(const char *hostname);

/*
 * returns the IP address of the supplied 'hostname'.
 * caller must free the returned string.
 */
char *resolveHostname(const char *hostname);

/*
 * returns if the supplied string is an IP address.
 * note that this handles both IPv4 and IPv6 strings.
 */
bool isValidIpAddress(const char *ipAddr);

#endif // ZILKER_IPADDRUTILS_H
