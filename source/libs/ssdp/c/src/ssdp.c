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
* ssdp.c
*
* SSDP functions supported:
*     Perform an SSDP discovery to locate UPnP-ready devices
*
* Author: jgleason
*-----------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>

#include <icBuildtime.h>
#include <ssdp/ssdp.h>
#include <icUtil/macAddrUtils.h>
#include <icLog/logging.h>
#include "discoverSearch.h"
#include <icConcurrent/threadPool.h>
#include <icTypes/icHashMap.h>
#include <icConcurrent/threadUtils.h>
#include <icConcurrent/timedWait.h>
#include <icUtil/stringUtils.h>
#include <errno.h>


/* SSDP broadcast address & port (IPV4) */
#define UPNP_MCAST_ADDR "239.255.255.250"
#define PORT 1900

/* UPnP Device Search Target (ST) definitions */
#define WIRELESS_NETWORK_CAMERA_ST  "urn:schemas-upnp-org:device:Wireless Network Camera:1"
#define OPENHOME_CAMERA_ST          "urn:schemas-upnp-org:device:OpenHome Camera:1"
#define WIFI_ST                     "urn:schemas-upnp-org:device:InternetGatewayDevice:1"
#define ROUTER_ST                   "urn:schemas-upnp-org:service:WANIPConnection:1"
#define PHILIPSHUE_ST               "libhue:idl"
#define SONOS_ST                    "urn:smartspeaker-audio:service:SpeakerGroup:1"

/* SSDP M-Search Maximum Time To Wait For Host Response */
#define M_SEARCH_MAX_WAIT_SECONDS   1

/* broadcast message */
#define BROADCAST_SSDP   "M-SEARCH * HTTP/1.1\r\nHOST: %s:%u\r\nST: %s\r\nMAN: \"ssdp:discover\"\r\nMX: %u\r\n\r\n"
#define BROADCAST_MARVELL_TEMPLATE "TYPE: WM-DISCOVER\r\nVERSION: 1.0\r\n\r\nservices: %s\r\n\r\n"
#define BROADCAST_ALL "M-SEARCH * HTTP/1.1\r\nHOST: %s:%u\r\nMAN: \"ssdp:discover\"\r\nMX: 1\r\nST: ssdp:all\r\n\r\n"

/* The time in second to wait between beacons */
#define BEACON_INTERVAL_SECS 5

/* The time to wait each iteration for a response */
#define RESPONSE_READ_TIMEOUT_SECS 1

#define DISCOVER_DEVICE_CALLBACK_POOL_NAME "discoverDeviceCallbackPool"
#define DISCOVER_DEVICE_CALLBACK_MIN_POOL_SIZE 1
#define DISCOVER_DEVICE_CALLBACK_MAX_POOL_SIZE 5
#define DISCOVER_DEVICE_CALLBACK_MAX_QUEUE_SIZE 20

/*
 * enumerated list of possible return codes from ssdp
 */
typedef enum
{
    SSDP_CODE_DISCOVER_SUCCESS = 0,
    SSDP_CODE_NOT_SUPPORTED_ERROR,
    SSDP_CODE_SOCKET_CONFIGURE_ERROR,
    SSDP_CODE_SOCKET_SETTING_ERROR,
    SSDP_CODE_SOCKET_BINDING_ERROR,
    SSDP_CODE_SOCKET_BROADCAST_ERROR,
    SSDP_CODE_SOCKET_READ_ERROR,

    /* the final entry is just for looping over the enum */
            SSDP_CODE_LAST
} SsdpCode;

/*
 * A struct to be used as the argument for a task in a thread pool
 */
typedef struct _SsdpThreadPoolTaskArg
{
    SsdpDiscoverCallback  callback;    //used to access the callback function for a discovered device
    SsdpDevice      *device;    //information about the discovered device
} SsdpThreadPoolTaskArg;


typedef struct {
    bool sendFailure;
    icLinkedList *sentList;
} SsdpBeaconArg;

/*
 * forward declare internal functions
 */
static int readResponse(int socket, char *data, size_t length, int timeoutMillis);
static SsdpDevice *parseResponse(char *data, int length);
static char *restOfLine(char *line, int start);
static void printDiscoveredDevice(SsdpDevice *device);
static bool ipSearch(void *searchVal, void *item);
static void ssdpDeviceFree(void *item);
static void callbackPoolTask(void *arg);

static void* beaconThreadProc(void *arg);
static void* listenThreadProc(void *arg);
static void callbackPoolTaskArgFreeFunc(void *arg);

/* private variables */
static pthread_mutex_t controlMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t controlCond;
static bool controlCondInited = false;
static pthread_t beaconThread;
static bool beaconRunning = false;
static pthread_t listenThread;
static bool listenRunning = false;

static icThreadPool *deviceDiscoverCallbackPool = NULL;

static int sockfd = -1; //the socket used to send/receive on
static icLinkedList *searchList = NULL;
static struct sockaddr_in targetAddress; //the address we will send to


/*---------------------------------------------------------------------------------------
 * ssdpDiscoverStart : Start SSDP discovery
 *
 * This will start background discovery of specific devices.  As devices are found, the
 * provided callback will be invoked.
 *
 * Caller must stop discovery with ssdpDiscoverStop.
 *
 * @return handle that can be used for the 'stop' call.  if <= 0, then the discover failed to start.
 *---------------------------------------------------------------------------------------*/
uint32_t ssdpDiscoverStart(SearchType type, SsdpDiscoverCallback callback)
{
    icLogTrace(SSDP_LOG_TAG, "%s", __FUNCTION__);

    // the idea is to have 2 threads for sending & listening while SSDP is active.
    // to allow concurrent searches, keep a list of "search" directives for the
    // 2 threads to iterate through.  what we need here is to potentially create the list,
    // add a search-item for this request, and potentially start the threads
    //

    // ensure we have a list of 'searches'
    //
    pthread_mutex_lock(&controlMutex);
    if (controlCondInited == false)
    {
        initTimedWaitCond(&controlCond);
        controlCondInited = true;
    }
    if (searchList == NULL)
    {
        searchList = linkedListCreate();
    }

    // create the 'discoverSearch' object
    //
    discoverSearch *search = createDiscoverSearch();
    search->searchType = type;
    search->callback = callback;
    switch (type)
    {
        case CAMERA:
            //
            linkedListAppend(search->stList, strdup(OPENHOME_CAMERA_ST));
            linkedListAppend(search->stList, strdup(WIRELESS_NETWORK_CAMERA_ST));
            search->searchCategory = SSDP_SEARCH_TYPE_STANDARD;
            break;

        case PHILIPSHUE:
            // add search target for Philips Hue lights
            linkedListAppend(search->stList, strdup(PHILIPSHUE_ST));
            search->searchCategory = SSDP_SEARCH_TYPE_STANDARD;
            break;

        case RTCOA:
            // these Thermostats are 'marvell', so save the "service name" to the st list
            linkedListAppend(search->stList, strdup("com.rtcoa.tstat*"));
            search->searchCategory = SSDP_SEARCH_TYPE_MARVELL;
            break;

        case SONOS:
            linkedListAppend(search->stList, strdup(SONOS_ST));
            search->searchCategory = SSDP_SEARCH_TYPE_STANDARD;
            break;

        default:
            /* NOT SUPPORTED... */
            icLogError(SSDP_LOG_TAG, "Error - SSDP discover for type = %d not yet supported", type);
            destroyDiscoverSearch(search);
            pthread_mutex_unlock(&controlMutex);
            return 0;
    }

    // see if our socket is valid
    //
    if (sockfd <= 0)
    {
        struct sockaddr_in localAddress;
        int opt = 1;

        // set up our target address that we will send beacons to
        //
        memset(&targetAddress, 0, sizeof(struct sockaddr_in));
        targetAddress.sin_family = AF_INET;
        targetAddress.sin_port = htons(PORT);
        targetAddress.sin_addr.s_addr = inet_addr(UPNP_MCAST_ADDR);

        // setup the UDP multicast socket
        //
        sockfd = socket(PF_INET, SOCK_DGRAM, 0);
        if(sockfd < 0)
        {
            icLogError(SSDP_LOG_TAG, "error configuring socket for SSDP");
            destroyDiscoverSearch(search);
            pthread_mutex_unlock(&controlMutex);
            return 0;
        }

        memset(&localAddress, 0, sizeof(struct sockaddr_in));
        localAddress.sin_family = AF_INET;
        localAddress.sin_port = htons(PORT);
        localAddress.sin_addr.s_addr = INADDR_ANY;

        /* set socket option to allow re-use of local addresses */
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt)) < 0)
        {
            icLogError(SSDP_LOG_TAG, "error setting socket option 'SO_REUSEADDR' for SSDP");
            close(sockfd);
            sockfd = -1;
            destroyDiscoverSearch(search);
            pthread_mutex_unlock(&controlMutex);
            return 0;
        }

        /* bind the socket */
        if (bind(sockfd, (struct sockaddr *)&localAddress, sizeof(struct sockaddr_in)) != 0)
        {
            icLogError(SSDP_LOG_TAG, "error binding to address 'INADDR_ANY'");
            close(sockfd);
            sockfd = -1;
            destroyDiscoverSearch(search);
            pthread_mutex_unlock(&controlMutex);
            return 0;
        }
    }

    // now that we're setup, assign a handle to this node
    //
    uint32_t handle = 0;
    uint32_t size = linkedListCount(searchList);
    if (size > 0)
    {
        // grab the handle from the last one
        //
        discoverSearch *last = (discoverSearch *)linkedListGetElementAt(searchList, size - 1);
        handle = last->handle;
    }
    handle++;
    search->handle = handle;

    // add this search directive to the list
    //
    linkedListAppend(searchList, search);

    // start threads if necessary
    //
    if (listenRunning == false)
    {
        //Start the listen thread
        listenRunning = true;
        createThread(&listenThread, listenThreadProc, NULL, "ssdpListen");
    }
    if (beaconRunning == false)
    {
        //Start the beacon thread
        beaconRunning = true;
        createThread(&beaconThread, beaconThreadProc, NULL, "ssdpBeacon");
    }

    // release lock and return the handle
    //
    pthread_mutex_unlock(&controlMutex);
    return handle;
}

/*
 * look for 'handle' in 'searchList'
 */
static bool findHandleInSearchList(void *searchVal, void *item)
{
    discoverSearch *search = (discoverSearch *)item;
    uint32_t *handle = (uint32_t *)searchVal;

    if (search->handle == *handle)
    {
        return true;
    }
    return false;
}

/*
 * destroy a discoverSearch element from our searchList
 */
static void deleteSearchFromSearchList(void *item)
{
    discoverSearch *search = (discoverSearch *)item;
    destroyDiscoverSearch(search);
}

/*---------------------------------------------------------------------------------------
 * ssdpDiscoverStop : Stop SSDP discovery
 *
 * This must be called after a successful discovery start with ssdpDiscoverStart.
 *---------------------------------------------------------------------------------------*/
void ssdpDiscoverStop(uint32_t handle)
{
    // simple check of the whole thing down
    //
    pthread_mutex_lock(&controlMutex);
    if(sockfd == -1)
    {
        icLogDebug(SSDP_LOG_TAG, "ssdpDiscoverStop: discover not running!");
        pthread_mutex_unlock(&controlMutex);
        return;
    }

    // find this handle in the 'searchList' so we can remove it
    //
    if (linkedListDelete(searchList, &handle, findHandleInSearchList, deleteSearchFromSearchList) == true)
    {
        // check the size of our list.  if empty, close everything down
        //
        int count = linkedListCount(searchList);
        if (count == 0)
        {
            icLogDebug(SSDP_LOG_TAG, "ssdpDiscoverStop: no more discover searches, shutting down threads");
            // Tell them to stop running
            listenRunning = false;
            beaconRunning = false;
            // Wake up!
            pthread_cond_broadcast(&controlCond);
            pthread_mutex_unlock(&controlMutex);
            // Wait for them to terminate
            pthread_join(listenThread, NULL);
            pthread_join(beaconThread, NULL);
            pthread_mutex_lock(&controlMutex);
            // Cleanup our sock
            close(sockfd);
            sockfd = -1;

            linkedListDestroy(searchList, deleteSearchFromSearchList);
            searchList = NULL;
        }
    }

    pthread_mutex_unlock(&controlMutex);
}

/*---------------------------------------------------------------------------------------
* ipSearch : PRIVATE function make sure device does not get added to 'found' list
* more than once. Uses the IP address of the device to check it.
*---------------------------------------------------------------------------------------*/
static bool ipSearch(void *ipString, void *ssdpDeviceNode)
{
    /* typecast to search items */
    SsdpDevice *curr = (SsdpDevice *)ssdpDeviceNode;
    char *search = (char *)ipString;

    /* looking for matching 'ip'*/
    if (strcmp(curr->ipAddress, search) == 0)
    {
        return true;
    }

    return false;
}

/*---------------------------------------------------------------------------------------
 * ssdpDeviceClone : private function to deep clone the object
 *---------------------------------------------------------------------------------------*/
static SsdpDevice *ssdpDeviceClone(SsdpDevice *old)
{
    SsdpDevice *retVal = NULL;
    if (old != NULL)
    {
        retVal = (SsdpDevice *)calloc(1, sizeof(SsdpDevice));
        if (old->upnpURL != NULL)
        {
            retVal->upnpURL = strdup(old->upnpURL);
        }
        if (old->upnpST != NULL)
        {
            retVal->upnpST = strdup(old->upnpST);
        }
        if (old->upnpUSN != NULL)
        {
            retVal->upnpUSN = strdup(old->upnpUSN);
        }
        if (old->upnpServer != NULL)
        {
            retVal->upnpServer = strdup(old->upnpServer);
        }
        if (old->marvellServiceName != NULL)
        {
            retVal->marvellServiceName = strdup(old->marvellServiceName);
        }

        strcpy(retVal->ipAddress, old->ipAddress);
        strcpy(retVal->macAddress, old->macAddress);
        retVal->port = old->port;
        retVal->type = old->type;
    }
    return retVal;
}

/*---------------------------------------------------------------------------------------
 * ssdpDeviceFree : private function to clear elements of the linked list
 *---------------------------------------------------------------------------------------*/
static void ssdpDeviceFree(void *item)
{
    if(item != NULL)
    {
        SsdpDevice *device = (SsdpDevice *)item;
        free(device->upnpURL);
        device->upnpURL = NULL;
        free(device->upnpST);
        device->upnpST = NULL;
        free(device->upnpUSN);
        device->upnpUSN = NULL;
        free(device->upnpServer);
        device->upnpServer = NULL;
        free(device->marvellServiceName);
        device->marvellServiceName = NULL;

        free(device);
    }
}

/*---------------------------------------------------------------------------------------
 * printDiscoveredDevice : PRIVATE function to print discovered device information when
 * the search matches. Used for debug.
 *---------------------------------------------------------------------------------------*/
static void printDiscoveredDevice(SsdpDevice *device)
{
    /* print out what is there */
    if (device->upnpST)
    {
        icLogDebug(SSDP_LOG_TAG, "\tST:    %s", device->upnpST);
    }
    if (device->upnpURL)
    {
        icLogDebug(SSDP_LOG_TAG, "\tURL:   %s", device->upnpURL);
    }
    if (device->upnpUSN)
    {
        icLogDebug(SSDP_LOG_TAG, "\tUSN:    %s", device->upnpUSN);
    }
    if (device->upnpServer)
    {
        icLogDebug(SSDP_LOG_TAG, "\tSERVER:%s", device->upnpServer);
    }
    if (strlen(device->ipAddress) > 0)
    {
        icLogDebug(SSDP_LOG_TAG, "\tIP:    %s", device->ipAddress);
    }
    if (strlen(device->macAddress) > 0)
    {
        icLogDebug(SSDP_LOG_TAG, "\tMAC:   %s", device->macAddress);
    }
    if (device->marvellServiceName)
    {
        icLogDebug(SSDP_LOG_TAG, "\tMarvell Service Name:   %s", device->marvellServiceName);
    }
    icLogDebug(SSDP_LOG_TAG,       "\ttype  %d", device->type);
}

/*---------------------------------------------------------------------------------------
* readResponse : wait for data to appear on the socket, then read it (up to length amount)
*---------------------------------------------------------------------------------------*/
static int readResponse(int socket, char *data, size_t length, int timeoutMillis)
{
    ssize_t n;
    struct pollfd fds[1]; /* for the poll */

    /* wait for something to show up */
    fds[0].fd = socket;
    fds[0].events = POLLIN;
    n = poll(fds, 1, timeoutMillis);
    if(n < 0)
    {
        icLogError(SSDP_LOG_TAG, "error polling socket for data");
        return -1;
    }
    else if(n == 0)
    {
        return 0;
    }

    /* read the data */
    n = recv(socket, data, length, 0);
    if(n < 0)
    {
        icLogError(SSDP_LOG_TAG, "error reading socket for data");
    }
    return (int)n;
}

/*---------------------------------------------------------------------------------------
* parseResponse : parse the buffer we read from a UPnP device.
* Creates a ssdpDevice with the values populated.
*---------------------------------------------------------------------------------------*/
static SsdpDevice *parseResponse(char *data, int length)
{
    char        *token = NULL;
    SsdpDevice  *retVal = NULL;
    bool isMarvellResponse = false;

    // the response read from the remote device will look something like:
    //    HTTP/1.1 200 OK
    //    CACHE-CONTROL: max-age=1800
    //    DATE: Fri, 05 Mar 2010 11:11:39 GMT
    //    EXT:
    //    LOCATION: http://172.16.12.1:49153/gatedesc.xml
    //    SERVER: Linux/2.4.30, UPnP/1.0, Intel SDK for UPnP devices/1.3.1
    //    ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1
    //    USN: uuid:75802409-bccb-40e7-8e6c-fa095ecce13e::urn:schemas-upnp-org:device:InternetGatewayDevice:1
    //
    //    HTTP/1.1 200 OK
    //	  CACHE-CONTROL: max-age=1800
    //	  DATE: Fri, 05 Mar 2010 14:17:17 GMT
    //	  EXT:
    //	  LOCATION: http://172.16.12.151:6789/device.xml
    //	  SERVER: Linux/2.4.19-pl1029 UPnP/1.0 Intel UPnP SDK/1.0
    //	  ST: urn:schemas-upnp-org:device:Wireless Network Camera:1
    //	  USN: uuid:upnp-Linksys_Wireless Network Camera-0021297e5a85::urn:schemas-upnp-org:device:Wireless Network Camera:1
    //

    /* null terminate buffer */
    data[length] = '\0';
    icLogTrace(SSDP_LOG_TAG, "%s", data);

    /* create our return struct */
    retVal = (SsdpDevice *)malloc(sizeof(SsdpDevice));
    memset(retVal, 0, sizeof(SsdpDevice));

    /* all we're looking for is the LOCATION, ST, and the USN */
    /* first tokenize on newline */
    char *ctx;
    token = strtok_r(data, "\n", &ctx);
    while (token != NULL)
    {
        /* look at the beginning of the token to see if it is a 'header' we care about */
        if (strncasecmp(token, "ST:", 3) == 0 || strncasecmp(token, "NT:", 3) == 0)
        {
            retVal->upnpST = restOfLine(token, 3);
        }
        else if (strncasecmp(token, "USN:", 4) == 0)
        {
            retVal->upnpUSN = restOfLine(token, 4);
        }
        else if (strncasecmp(token, "LOCATION:", 9) == 0 || strncasecmp(token, "URL:", 4) == 0)
        {
            retVal->upnpURL = restOfLine(token, 9);
        }
        else if (strncasecmp(token, "SERVER:", 7) == 0)
        {
            retVal->upnpServer = restOfLine(token, 7);
        }
        else if (strncasecmp(token, "SERVICE:", 8) == 0)
        {
            retVal->marvellServiceName = restOfLine(token, 8);
        }
        else if (strncasecmp(token, "TYPE: WM-NOTIFY", 15) == 0)
        {
            isMarvellResponse = true;
        }

        /* get next token */
        token = strtok_r(NULL, "\n", &ctx);
    }

    /*
     * now that we have extracted the information we care about
     * examine the string values to determine the IP, port, and type
     */

    /* First, the type */
    if (retVal->upnpST != NULL)
    {
        /* get the type based on this value */
        if (strcasecmp(retVal->upnpST, WIFI_ST) == 0)
        {
            retVal->type = WIFI;
        }
        else if (strcasecmp(retVal->upnpST, ROUTER_ST) == 0)
        {
            retVal->type = ROUTER;
        }
        else if (strcasecmp(retVal->upnpST, SONOS_ST) == 0)
        {
            retVal->type = SONOS;
        }
        else if ((strcasecmp(retVal->upnpST, WIRELESS_NETWORK_CAMERA_ST) == 0) ||
                 (strcasecmp(retVal->upnpST, OPENHOME_CAMERA_ST) == 0))
        {
            retVal->type = CAMERA;
        }
        else if(retVal->upnpServer != NULL && strstr(retVal->upnpServer, "IpBridge") != NULL)
        {
            //according to offical Philips Hue discovery guidelines, if the response has IpBridge
            // in it, it can be considered to be a Philips Hue bridge :-/
            retVal->type = PHILIPSHUE;
        }
        else if(isMarvellResponse == true)
        {
            //our only marvell search so far is rtcoa..
            retVal->type = RTCOA;
        }
    }

    /* now the IP and port */
    if (retVal->upnpURL != NULL)
    {
        uint16_t defaultPort = 80;
        int32_t skipChars = -1;
        if (strncasecmp(retVal->upnpURL, "http://", 7) == 0)
        {
            skipChars = 7;
        }
        else if (strncasecmp(retVal->upnpURL, "https://", 8) == 0)
        {
            skipChars = 8;
            defaultPort = 443;
        }
        /* skip the http:// */
        if (skipChars > 0)
        {
            /* read chars until we hit a ':', '/' or eoln */
            char *start = retVal->upnpURL + skipChars;
            int len = strlen(start);
            int portStart = strcspn(start, ":");
            int urlEnd = strcspn(start, "/");

            /* see if port was found */
            if (urlEnd > portStart && portStart < len)
            {
                char tmp[10];

                /*
                 * got something like 192.168.0.1:12345/HNAP
                 * ipAddress = from start to portStart
                 * port      = portStart to urlEnd
                 */
                strncpy(retVal->ipAddress, start, portStart);
                strncpy(tmp, start+portStart+1, urlEnd - (portStart + 1));
                tmp[urlEnd - (portStart + 1)] = '\0';
                retVal->port = (unsigned int) atoi(tmp);
            }
            else if (urlEnd > 0 && urlEnd < len)
            {
                /*
                 * got something like 192.168.0.1/HNAP
                 * (i.e. no port definition)
                 */
                strncpy(retVal->ipAddress, start, urlEnd);
                retVal->port = defaultPort;
            }
            else if (urlEnd == len && portStart == len)
            {
                /*
                 * got something like 192.168.0.1
                 */
                strncpy(retVal->ipAddress, start, 31);
                retVal->port = defaultPort;
            }
        }
    }

    /*
     * if we got an IP address, find the MAC from the ARP tables.  if we didnt get an ip and mac, free and return null
     */
    if (strlen(retVal->ipAddress) == 0)
    {
        icLogError(SSDP_LOG_TAG, "Failed to get ip address for discovered device.");
        ssdpDeviceFree(retVal);
        retVal = NULL;
    }
    else
    {
        // attempt to get the mac address of the device
        //
        lookupMacAddressByIpAddress(retVal->ipAddress, retVal->macAddress);
    }

    return retVal;
}

/*---------------------------------------------------------------------------------------
* restOfLine : copies the rest of the line after the header string
*---------------------------------------------------------------------------------------*/
static char *restOfLine(char *line, int start)
{
    int len = strlen(line);
    int diff = 0;
    char *retVal = NULL;

    /* skip the first char if it's a space */
    if (line[start] == ' ')
    {
        start++;
    }
    diff = len - start;

    /* make the mem to hold a copy of the string */
    retVal = (char *)calloc(sizeof(char) * (diff+1), sizeof(char));

    /*
     * copy the string starting at the offset 'start'
     * and terminate at the end (and maybe the last char if \r)
     */
    strncpy(retVal, line+start, diff);
    retVal[diff] = '\0';
    if (diff > 0 && retVal[diff - 1] == '\r')
    {
        retVal[diff - 1] = '\0';
    }

    return retVal;
}

/*
 * linkedListCompareFunc implementation to search for a string
 * within our 'sentList'
 */
static bool findDupSearchTarget(void *searchVal, void *item)
{
    char *searchStr = (char *)searchVal;
    char *currStr = (char *)item;

    if (strcmp(searchStr, currStr) == 0)
    {
        return true;
    }
    return false;
}

/*
 * the 'linkedListIterateFunc' used by 'beaconThreadProc' to
 * broadcast a 'search string' on our socket.  if an error
 * occurs, will close our global socket and break from the
 * loop.
 */
static bool broadcastSearchTarget(void *item, void *arg)
{
    SsdpBeaconArg *beaconArg = (SsdpBeaconArg *)arg;
    icLinkedList *sentList = beaconArg->sentList;

    // item should be a 'discoverSearch' object
    //
    discoverSearch *lookFor = (discoverSearch *)item;
    char buffer[1024 * 2];

    // broadcast differently based on the category
    //
    if (lookFor->searchCategory == SSDP_SEARCH_TYPE_STANDARD)
    {
        // this 'search directive' could have multiple 'search targets', so broadcast each
        // note that we'll use the getElementAt mechanism vs creating an iterator to save memory
        //
        int i, count = linkedListCount(lookFor->stList);
        for (i = 0 ; i < count ; i++)
        {
            // don't send this target if it's in our 'sentList'.  no sense in broadcasting
            // the same ST over and over for a single iteration
            //
            char *target = (char *)linkedListGetElementAt(lookFor->stList, i);
            if (linkedListFind(sentList, target, findDupSearchTarget) != NULL)
            {
                // this 'target' was already sent this run, so skip it
                //
                continue;
            }

            // add this target to the list so we don't send a dup (no strdup, just the pointer)
            //
            linkedListAppend(sentList, target);

            // create the message to send, then send it (telling the device to delay for 2 seconds before replying)
            //
            ssize_t n = sprintf(buffer, BROADCAST_SSDP, UPNP_MCAST_ADDR, PORT, target, M_SEARCH_MAX_WAIT_SECONDS);
            if(n > 0)
            {
                // send the message over our UDP socket
                //
                icLogTrace(SSDP_LOG_TAG, "broadcasting:\n%s", buffer);
                n = sendto(sockfd, buffer, n, 0, (struct sockaddr *) &targetAddress, sizeof(struct sockaddr_in));
                if (n < 0)
                {
                    char *errStr = strerrorSafe(errno);
                    icLogError(SSDP_LOG_TAG, "error sending broadcast message for SSDP: %s. Stopping beacons.", errStr);
                    free(errStr);
                    beaconArg->sendFailure = true;
                    return false;
                }
            }
        }
    }
    else if (lookFor->searchCategory == SSDP_SEARCH_TYPE_MARVELL)
    {
        // only support one of these, so grab it from the directive
        //
        char *marvellName = (char *)linkedListGetElementAt(lookFor->stList, 0);
        if (marvellName != NULL)
        {
            ssize_t n = sprintf(buffer, BROADCAST_MARVELL_TEMPLATE, marvellName);
            if (n > 0)
            {
                icLogTrace(SSDP_LOG_TAG, "broadcasting:\n%s", buffer);
                n = sendto(sockfd, buffer, n, 0, (struct sockaddr *) &targetAddress, sizeof(struct sockaddr_in));
                if (n < 0)
                {
                    icLogError(SSDP_LOG_TAG, "error sending broadcast message for SSDP. Stopping beacons.");
                    beaconArg->sendFailure = true;
                    return false;
                }
            }
        }
    }

    // save to go to the next discoverSearch object in our searchList
    //
    return true;
}

/*
 * thread to send broadcast messages for devices to listen for and respond to
 */
static void* beaconThreadProc(void *arg)
{
    pthread_mutex_lock(&controlMutex);
    while(beaconRunning == true)
    {
        // round-robin through all items of our 'search list'
        // we'll do this the convoluted way to be more time & memory efficient
        // (since this loop runs fairly often).  at the same time, create
        // a 'sent' list so we don't duplicate the ST strings for this iteration.
        //
        SsdpBeaconArg beaconArg = {
                .sendFailure = false,
                .sentList = linkedListCreate()
        };
        linkedListIterate(searchList, broadcastSearchTarget, &beaconArg);
        linkedListDestroy(beaconArg.sentList, standardDoNotFreeFunc);

        // see if we got an error during the loop execution,
        // signifying something went wrong and we need to close up shop
        //
        if (beaconArg.sendFailure == true)
        {
            break;
        }

        // pause a few seconds before running through our search list again
        // this gives our listening thread some CPU time as well as a window
        // for it to grab the lock
        //
        incrementalCondTimedWait(&controlCond, &controlMutex, BEACON_INTERVAL_SECS);
    }
    pthread_mutex_unlock(&controlMutex);

    // probably told to stop
    //
    icLogDebug(SSDP_LOG_TAG, "beacon thread is exiting");
    pthread_mutex_lock(&controlMutex);
    beaconRunning = false;
    pthread_mutex_unlock(&controlMutex);
    pthread_exit(NULL);
    return NULL;
}

/*
 * the 'linkedListCompareFunc' used by 'listenThreadProc' to
 * locate a match of "what was parsed from the response" with
 * the list of things we're searching for
 */
static bool findSearchTarget(void *arg, void *item)
{
    // typecast arguments.  'item' should be the discoverSearch object
    // and 'arg' should be the SsdpDevice object from the parsed response
    //
    discoverSearch *search = (discoverSearch *)item;
    SsdpDevice *parsed = (SsdpDevice *)arg;

    if (search->searchCategory == SSDP_SEARCH_TYPE_STANDARD)
    {
        if (parsed->type == PHILIPSHUE && search->searchType == PHILIPSHUE)
        {
            //Philips hue is odd... they dont return the same ST that we searched for
            // trust our earlier code that identified it
            //
            return true;
        }
        else if (parsed->upnpST != NULL)
        {
            // compare the returned ST string to the one we broadcasted
            //
            int i, count = linkedListCount(search->stList);
            for (i = 0; i < count; i++)
            {
                /* create the message to send, then send it (telling the device to delay for 2 seconds before replying) */
                char *target = (char *) linkedListGetElementAt(search->stList, i);
                if (target != NULL && strcmp(parsed->upnpST, target) == 0)
                {
                    // found match
                    //
                    return true;
                }
            }
        }
    }
    else if (search->searchCategory == SSDP_SEARCH_TYPE_MARVELL)
    {
        // marvell search names end in an asterisk typically... dont use that part when we compare.
        // NOTE: this is fragile but we have also never encountered another marvell service device
        //
        char *target = (char *) linkedListGetElementAt(search->stList, 0);
        if (target != NULL && parsed->marvellServiceName != NULL && strncmp(target, parsed->marvellServiceName, strlen(target)-1) == 0)
        {
            return true;
        }
    }

    // no match, keep iterating
    //
    return false;
}

/*
 * thread to listen for device responses to our beacons
 */
static void* listenThreadProc(void *arg)
{
    char buffer[1024 * 2];

    pthread_mutex_lock(&controlMutex);
    bool localListenRunning = listenRunning;
    pthread_mutex_unlock(&controlMutex);

    while(localListenRunning == true)
    {
        /* waiting for SSDP REPLY packet to M-SEARCH */
        icLogTrace(SSDP_LOG_TAG, "reading SSDP responses ...");
        int n = readResponse(sockfd, buffer, sizeof(buffer), RESPONSE_READ_TIMEOUT_SECS * 1000);
        icLogTrace(SSDP_LOG_TAG, "SSDP got response, size = %d", n);
        if (n < 0)
        {
            /* read error, no data, or timeout */
            icLogError(SSDP_LOG_TAG, "readResponse returned %d, exiting the listen thread.", n);
            pthread_mutex_lock(&controlMutex);
            listenRunning = false;
            pthread_mutex_unlock(&controlMutex);
            pthread_exit(NULL);
            return NULL;
        }

        // Update our flag
        pthread_mutex_lock(&controlMutex);
        localListenRunning = listenRunning;
        pthread_mutex_unlock(&controlMutex);

        if(n == 0)
        {
            continue;
        }

        // parse the discovered information
        //
        SsdpDevice *tmp = parseResponse(buffer, n);
        if (tmp != NULL && strlen(tmp->ipAddress) > 0)
        {
            // see if what we received/parsed matches any of the items in our 'searchList'.
            // in the same spirit as our broadcast thread, don't use the Iterator mechanism
            // in an effort to reduce our memory usage on something that can loop often
            //
            pthread_mutex_lock(&controlMutex);
            bool processed = false;
            int count = linkedListCount(searchList);
            int offset = 0;
            for (offset = 0 ; offset < count ; offset++)
            {
                discoverSearch *next = linkedListGetElementAt(searchList, (uint32_t)offset);
                if (next == NULL)
                {
                    // list size changed on us
                    //
                    break;
                }

                // see if this 'search' is looking for a device similar to what we found.
                // note that we keep looping even if this is not a match, to allow for
                // more then 1 object searching for the same thing (i.e. discover new cameras
                // while locating a lost camera)
                //
                if (findSearchTarget(tmp, next) == true)
                {
                    // before forwarding to this callback, make sure it hasn't processed this IP already
                    //
                    if (discoverSearchDidProcessedIp(next, tmp->ipAddress) == false)
                    {
                        // show device found and save a flag so that we save AFTER
                        // going through all of the possibles (allows this discovered
                        // device to be visible to more then just the current search request).
                        //
                        icLogDebug(SSDP_LOG_TAG, "Adding discovered device %s to SSDP list and invoking callback", tmp->ipAddress);
                        printDiscoveredDevice(tmp);

                        // Create a thread pool if we don't have one already
                        if (deviceDiscoverCallbackPool == NULL)
                        {
                            deviceDiscoverCallbackPool = threadPoolCreate(DISCOVER_DEVICE_CALLBACK_POOL_NAME,
                                                                          DISCOVER_DEVICE_CALLBACK_MIN_POOL_SIZE,
                                                                          DISCOVER_DEVICE_CALLBACK_MAX_POOL_SIZE,
                                                                          DISCOVER_DEVICE_CALLBACK_MAX_QUEUE_SIZE);
                        }

                        // Create a task argument.  note that we clone tmp for each callback thread
                        //
                        SsdpThreadPoolTaskArg *arg2 = (SsdpThreadPoolTaskArg *)calloc(1, sizeof(SsdpThreadPoolTaskArg));
                        arg2->callback = next->callback;
                        arg2->device = ssdpDeviceClone(tmp);

                        //Add task to our pool that will invoke the discover callback
                        if (threadPoolAddTask(deviceDiscoverCallbackPool, callbackPoolTask, arg2, callbackPoolTaskArgFreeFunc) == false)
                        {
                            icLogWarn(SSDP_LOG_TAG, "Unable to enqueue discover callback! The task queue may be full");
                        }

                        // put into processedList
                        discoverSearchAddProcessedIp(next, tmp->ipAddress);
                        processed = true;
                    }
                }
            }

            if (processed == false)
            {
                icLogDebug(SSDP_LOG_TAG, "Skipping discovered device %s; all listeners have previously examined the device", tmp->ipAddress);
            }

            pthread_mutex_unlock(&controlMutex);
        }
        else
        {
            icLogWarn(SSDP_LOG_TAG, "unable to parse SSDP response");
        }

        // cleanup
        ssdpDeviceFree(tmp);
    }

    icLogDebug(SSDP_LOG_TAG, "listen thread exiting normally");
    pthread_mutex_lock(&controlMutex);
    listenRunning = false;
    pthread_mutex_unlock(&controlMutex);
    pthread_exit(NULL);
    return NULL;
}

static void callbackPoolTask(void *arg)
{
    SsdpThreadPoolTaskArg *taskArg = (SsdpThreadPoolTaskArg *) arg;

    //Perform our callback. This will probably take a long time (hence why we are putting this in a threadpool)
    taskArg->callback(taskArg->device);
}

static void callbackPoolTaskArgFreeFunc(void *arg)
{
    SsdpThreadPoolTaskArg *taskArg = (SsdpThreadPoolTaskArg *) arg;

    // cleanup
    ssdpDeviceFree(taskArg->device);
    taskArg->device = NULL;
    taskArg->callback = NULL;
    free(taskArg);
}
