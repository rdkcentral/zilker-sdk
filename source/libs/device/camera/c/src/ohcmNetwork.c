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
 * ohcmNetwork.c
 *
 * implementation of "network" functionality as defined in ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <libxml/parser.h>

#include <xmlHelper/xmlHelper.h>
#include <openHomeCamera/ohcm.h>
#include <icUtil/stringUtils.h>
#include "ohcmBase.h"
#include "ohcmPrivate.h"

/* network status strings */
#define DEVICE_NETIFACE_URI           "/OpenHome/System/Network/interfaces"
#define DEVICE_WIRELESS_STATUS_URI    "/wireless/status"

#define WIRELESS_STATUS_NODE_ENABLED            "enabled"
#define WIRELESS_STATUS_NODE_CHANNEL_NO         "channelNo"
#define WIRELESS_STATUS_NODE_SSID               "ssid"
#define WIRELESS_STATUS_NODE_BSSID              "bssid"
#define WIRELESS_STATUS_NODE_RSSIDB             "rssidB"
#define WIRELESS_STATUS_NODE_SIGNAL_STRENGTH    "signalStrength"
#define WIRELESS_STATUS_NODE_NOISE_DBM          "noiseIndB"
#define WIRELESS_STATUS_NODE_NUM_OF_AP          "numOfAPs"
#define WIRELESS_STATUS_NODE_AVAILABLE_AP_LIST  "AvailableAPList"

/* network interface strings */
#define NET_IFACE_NODE                  "NetworkInterface"
#define NET_IFACE_ID_NODE               "id"
#define NET_IFACE_ENABLED_NODE          "enabled"
#define NET_IFACE_IPADDR_NODE           "IPAddress"
#define NET_IFACE_WIRELESS_NODE         "Wireless"
#define NET_IFACE_DISCOVERY_NODE        "Discovery"
#define NET_IFACE_IP_VER_NODE           "ipVersion"
#define NET_IFACE_ADDR_TYPE_NODE        "addressingType"
#define NET_IFACE_IP_ADDRESS_NODE       "ipAddress"
#define NET_IFACE_SUBNET_NODE           "subnetMask"
#define NET_IFACE_GATEWAY_NODE          "DefaultGateway"
#define NET_IFACE_PRIMARY_DNS_NODE      "PrimaryDNS"
#define NET_IFACE_SECONDARY_DNS_NODE    "SecondaryDNS"
#define NET_IFACE_IPADDR_NODE           "IPAddress"
#define NET_IFACE_WIFI_MODE_NODE        "wirelessNetworkMode"
#define NET_IFACE_WIFI_PROFILE_NODE     "profile"
#define NET_IFACE_WIFI_CHANNEL_NODE     "channel"
#define NET_IFACE_WIFI_SSID_NODE        "ssid"
#define NET_IFACE_WIFI_WMM_NODE         "wmmEnabled"
#define NET_IFACE_WIFI_SEC_NODE         "WirelessSecurity"
#define NET_IFACE_WIFI_SEC_MODE_NODE    "securityMode"
#define NET_IFACE_WIFI_WPA_NODE         "WPA"
#define NET_IFACE_WIFI_WPA_ALGO_NODE    "algorithmType"
#define NET_IFACE_WIFI_WPA_KEY_NODE     "sharedKey"
#define NET_IFACE_REFRESH_INTERVAL      "statusRefreshInterval"
#define NET_IFACE_ROAMING_NODE          "AggressiveRoaming"
#define NET_IFACE_UPNP_NODE             "UPnP"

// NULL terminated array that correlates to the ohcmWifiSecurityType enum
static const char *ohcmWifiSecurityTypeLabels[] = {
        "disable",          // OHCM_SEC_DISABLED
        "wep",              // OHCM_SEC_WEP
        "WPA-personal",     // OHCM_SEC_WPA_PERSONAL
        "WPA2-personal",    // OHCM_SEC_WPA2_PERSONAL
        "WPA-RADIUS",       // OHCM_SEC_WPA_RADIUS
        "WPA-enterprise",   // OHCM_SEC_WPA_ENTRERPRISE
        "WPA2-enterprise",  // OHCM_SEC_WPA2_ENTERPRISE
        "WPA/WPA2-personal",// OHCM_SEC_WPA_WPA2_PERSONAL
        NULL
};
// NULL terminated array that correlates to the ohcmWpaEncrAlgoType enum
static const char *ohcmWpaEncrAlgoTypeLabels[] = {
        "none",     // OHCM_WPA_ENCR_ALGO_NONE
        "TKIP",     // OHCM_WPA_ENCR_ALGO_TKIP
        "AES",      // OHCM_WPA_ENCR_ALGO_AES
        "TKIP/AES", // OHCM_WPA_ENCR_ALGO_TKIP_AES
        NULL
};

/*
 * Creates an ohcmWirelessStatus
 */
ohcmWirelessStatus *createOhcmWirelessStatus()
{
    ohcmWirelessStatus *status = calloc(1, sizeof(ohcmWirelessStatus));
    //status->accessPoints = linkedListCreate();    //TODO Wireless APs not added/implemented yet
    return status;
}

/*
 * Cleans up an ohcmWirelessStatus
 */
void destroyOhcmWirelessStatus(ohcmWirelessStatus *status)
{
    if (status != NULL)
    {
        free(status->bssid);
        status->bssid = NULL;
        free(status->ssid);
        status->ssid = NULL;
        free(status->channel);
        status->channel = NULL;
        //linkedListDestroy(status->accessPoints, accessPointFreeFunction); //TODO Wireless APs not added/implemented yet
        free(status);
    }
}

/*
 * parse an XML node for information about the wireless status of a network interface.
 * assumes 'funcArg' is an ohcmWirelessStatus object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmWirelessStatusXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmWirelessStatus *status = (ohcmWirelessStatus *) funcArg;
    if (node == NULL || status == NULL)
    {
        return false;
    }

    if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_ENABLED) == 0)
    {
        status->enabled = getXmlNodeContentsAsBoolean(node, false);
    }
    else if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_CHANNEL_NO) == 0)
    {
        status->channel = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_SSID) == 0)
    {
        status->ssid = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_BSSID) == 0)
    {
        status->bssid = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_RSSIDB) == 0)
    {
        status->rssidB = getXmlNodeContentsAsInt(node, 0);
    }
    else if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_SIGNAL_STRENGTH) == 0)
    {
        status->signalStrength = getXmlNodeContentsAsInt(node, 0);
    }
    else if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_NOISE_DBM) == 0)
    {
        status->noiseIndB = getXmlNodeContentsAsInt(node, 0);
    }
    else if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_NUM_OF_AP) == 0)
    {
        status->numAPs = getXmlNodeContentsAsInt(node, 0);
    }
    else if (strcmp((const char *) node->name, WIRELESS_STATUS_NODE_AVAILABLE_AP_LIST) == 0)
    {
        //TODO
        //Currently don't do anything with APs
    }
    else
    {
        icLogWarn(OHCM_LOG, "Unrecognized wireless status xml node: %s", node->name);
    }

    return true;
}

/*
 * attempts to get the wireless status of a camera
 */
ohcmResultCode getWirlessStatusOhcmCamera(ohcmCameraInfo *cam, const char* ifaceUid, ohcmWirelessStatus *target, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    int lenReal = snprintf(realUrl, MAX_URL_LENGTH, "https://%s:%s@%s%s/%s%s", cam->userName, cam->password, cam->cameraIP, DEVICE_NETIFACE_URI,
                           ifaceUid, DEVICE_WIRELESS_STATUS_URI);
    snprintf(debugUrl, MAX_URL_LENGTH, "https://%s%s/%s%s", cam->cameraIP, DEVICE_NETIFACE_URI, ifaceUid, DEVICE_WIRELESS_STATUS_URI);

    CURLcode rc = CURLE_URL_MALFORMAT;

    //If our URL wasn't truncated...
    if (lenReal < MAX_URL_LENGTH)
    {
        // create the output buffer
        //
        icFifoBuff *chunk = fifoBuffCreate(1024);

        // create our CURL context
        //
        CURL *curl = createOhcmCurlContext();
        if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
        }

        // perform the 'get' operation
        //
        rc = ohcmPerformCurlGet(curl, debugUrl, chunk, retryCounts);
        if (rc == CURLE_OK)
        {
            if (isIcLogPriorityTrace() == true && chunk != NULL && fifoBuffGetPullAvailable(chunk) > 0)
            {
                icLogTrace(OHCM_LOG, "camera get: %s\n%s", debugUrl, (char *)fifoBuffPullPointer(chunk, 0));
            }

            // success with the 'get', so parse the result
            //
            if (ohcmParseXmlHelper(chunk, parseOhcmWirelessStatusXmlNode, target) == false)
            {
                // unable to parse result from camera
                //
                icLogWarn(OHCM_LOG, "error parsing results of %s", debugUrl);
                rc = CURLE_CONV_FAILED;
            }
        }

        // cleanup
        //
        destroyOhcmCurlContext(curl);
        fifoBuffDestroy(chunk);
    }

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

/*
 * parse the <IPAddress> section of <NetworkInterface>
 */
static void parseIPAddrNode(xmlNodePtr parent, ohcmNetworkInterface *target)
{
    // parse XML that looks similar to:
    //   <IPAddress version="1.0">
    //     <ipVersion>v4</ipVersion>
    //     <addressingType>dynamic</addressingType>
    //     <ipAddress>172.16.12.100</ipAddress>
    //     <subnetMask>255.255.255.0</subnetMask>
    //     <DefaultGateway>
    //       <ipAddress>172.16.12.1</ipAddress>
    //     </DefaultGateway>
    //     <PrimaryDNS>
    //       <ipAddress>172.16.12.1</ipAddress>
    //     </PrimaryDNS>
    //   </IPAddress>

    xmlNodePtr currNode = NULL;
    xmlNodePtr loopNode = parent->children;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, NET_IFACE_IP_VER_NODE) == 0)
        {
            // v4 or v6
            char *tmp = getXmlNodeContentsAsString(currNode, NULL);
            if (tmp != NULL && strcmp(tmp, "v6") == 0)
            {
                target->ipVersion = OHCM_IPV6;
            }
            else
            {
                target->ipVersion = OHCM_IPV4;
            }
            free(tmp);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_ADDR_TYPE_NODE) == 0)
        {
            // dynamic or static
            char *tmp = getXmlNodeContentsAsString(currNode, NULL);
            if (tmp != NULL && strcmp(tmp, "static") == 0)
            {
                target->addressingType = OHCM_NET_ADDRESS_STATIC;
            }
            else
            {
                target->addressingType = OHCM_NET_ADDRESS_DYNAMIC;
            }
            free(tmp);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_IP_ADDRESS_NODE) == 0)
        {
            target->ipAddress = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_SUBNET_NODE) == 0)
        {
            target->subnetMask = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_GATEWAY_NODE) == 0)
        {
            // look for the child: <ipAddress>
            xmlNodePtr ipAddr = findChildNode(currNode, NET_IFACE_IP_ADDRESS_NODE, false);
            if (ipAddr != NULL)
            {
                target->gatewayIpAddress = getXmlNodeContentsAsString(ipAddr, NULL);
            }
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_PRIMARY_DNS_NODE) == 0)
        {
            // look for the child: <ipAddress>
            xmlNodePtr ipAddr = findChildNode(currNode, NET_IFACE_IP_ADDRESS_NODE, false);
            if (ipAddr != NULL)
            {
                target->primaryDNSIpAddress = getXmlNodeContentsAsString(ipAddr, NULL);
            }
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_SECONDARY_DNS_NODE) == 0)
        {
            // look for the child: <ipAddress>
            xmlNodePtr ipAddr = findChildNode(currNode, NET_IFACE_IP_ADDRESS_NODE, false);
            if (ipAddr != NULL)
            {
                target->secondaryDNSIpAddress = getXmlNodeContentsAsString(ipAddr, NULL);
            }
        }
    }
}

/*
 * parse the <Wireless> section of <WirelessSecurity>
 */
static void parseWirelessSecurityNode(xmlNodePtr parent, ohcmNetworkInterface *target)
{
    // parse XML that looks similar to:
    //       <WirelessSecurity>
    //         <securityMode>WPA/WPA2-personal</securityMode>
    //         <WPA>
    //           <algorithmType>TKIP/AES</algorithmType>
    //           <sharedKey>MyPSK</sharedKey>
    //         </WPA>
    //       </WirelessSecurity>

    xmlNodePtr currNode = NULL;
    xmlNodePtr loopNode = parent->children;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, NET_IFACE_WIFI_SEC_MODE_NODE) == 0)
        {
            // it should be one of:
            //  "disable"
            //  "wep"
            //  "WPA-personal"
            //  "WPA2-personal"
            //  "WPA-RADIUS"
            //  "WPA-enterprise"
            //  "WPA2-enterprise"
            //  "WPA/WPA2-personal"
            //
            char *tmp = getXmlNodeContentsAsString(currNode, NULL);
            if (tmp != NULL)
            {
                // loop through the array to find the correlating enum value
                //
                target->profileSecurityMode = OHCM_SEC_DISABLED;
                int offset = 0;
                for (offset = 0; ohcmWifiSecurityTypeLabels[offset] != NULL; offset++)
                {
                    if (strcmp(tmp, ohcmWifiSecurityTypeLabels[offset]) == 0)
                    {
                        // got a match!
                        target->profileSecurityMode = (ohcmWifiSecurityType) offset;
                        break;
                    }
                }
                free(tmp);
            }
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_WIFI_WPA_NODE) == 0)
        {
             // should have 2 children:
             //    <algorithmType>TKIP/AES</algorithmType>
             //    <sharedKey>MyPSK</sharedKey>
             //
            xmlNodePtr algoNode = findChildNode(currNode, NET_IFACE_WIFI_WPA_ALGO_NODE, false);
            if (algoNode != NULL)
            {
                char *tmp = getXmlNodeContentsAsString(algoNode, NULL);
                if (tmp != NULL)
                {
                    // could be one of:
                    //  "TKIP"
                    //  "AES"
                    //  "TKIP/AES"
                    //
                    // loop through the array to find the correlating enum value
                    //
                    target->profileAlgorithmType = OHCM_WPA_ENCR_ALGO_NONE;
                    int offset = 0;
                    for (offset = 0; ohcmWpaEncrAlgoTypeLabels[offset] != NULL; offset++)
                    {
                        if (strcmp(tmp, ohcmWpaEncrAlgoTypeLabels[offset]) == 0)
                        {
                            // got a match!
                            target->profileAlgorithmType = (ohcmWpaEncrAlgoType) offset;
                            break;
                        }
                    }
                    free(tmp);
                }
            }
            xmlNodePtr keyNode = findChildNode(currNode, NET_IFACE_WIFI_WPA_KEY_NODE, false);
            if (keyNode != NULL)
            {
                target->profileSharedKey = getXmlNodeContentsAsString(keyNode, NULL);
            }
        }
    }
}

/*
 * parse the <Wireless> section of <profile>
 */
static void parseWirelessProfileNode(xmlNodePtr parent, ohcmNetworkInterface *target)
{
    // parse XML that looks similar to:
    //        <profile>
    //          <channel>11</channel>
    //          <ssid>MySSID</ssid>
    //          <wmmEnabled>true</wmmEnabled>
    //          <WirelessSecurity>
    //            <securityMode>WPA/WPA2-personal</securityMode>
    //            <WPA>
    //              <algorithmType>TKIP/AES</algorithmType>
    //              <sharedKey>MyPSK</sharedKey>
    //            </WPA>
    //          </WirelessSecurity>
    //        </profile>

    xmlNodePtr currNode = NULL;
    xmlNodePtr loopNode = parent->children;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, NET_IFACE_WIFI_CHANNEL_NODE) == 0)
        {
            // TODO: should we make this an enumeration??
            target->profileChannel = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_WIFI_SSID_NODE) == 0)
        {
            target->profileSsid = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_WIFI_WMM_NODE) == 0)
        {
            target->profileWmmEnabled = getXmlNodeContentsAsBoolean(currNode, false);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_WIFI_SEC_NODE) == 0)
        {
            // parse the <WirelessSecurity> node
            parseWirelessSecurityNode(currNode, target);
        }
    }
}


/*
 * parse the <Wireless> section of <Wireless>
 */
static void parseWirelessNode(xmlNodePtr parent, ohcmNetworkInterface *target)
{
    // parse XML that looks similar to:
    //      <Wireless version="1.0">
    //        <enabled>true</enabled>
    //        <wirelessNetworkMode>infrastructure</wirelessNetworkMode>
    //        <profile>
    //          <channel>11</channel>
    //          <ssid>MySSID</ssid>
    //          <wmmEnabled>true</wmmEnabled>
    //          <WirelessSecurity>
    //            <securityMode>WPA/WPA2-personal</securityMode>
    //            <WPA>
    //              <algorithmType>TKIP/AES</algorithmType>
    //              <sharedKey>MyPSK</sharedKey>
    //            </WPA>
    //          </WirelessSecurity>
    //        </profile>
    //        <statusRefreshInterval>0</statusRefreshInterval>
    //        <AggressiveRoaming>
    //          <enabled>true</enabled>
    //        </AggressiveRoaming>
    //      </Wireless>

    xmlNodePtr currNode = NULL;
    xmlNodePtr loopNode = parent->children;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, NET_IFACE_ENABLED_NODE) == 0)
        {
            target->wirelessEnabled = getXmlNodeContentsAsBoolean(currNode, false);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_WIFI_MODE_NODE) == 0)
        {
            target->wirelessNetworkMode = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_WIFI_PROFILE_NODE) == 0)
        {
            // parse <profile>
            parseWirelessProfileNode(currNode, target);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_REFRESH_INTERVAL) == 0)
        {
            target->statusRefreshInterval = getXmlNodeContentsAsInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, NET_IFACE_ROAMING_NODE) == 0)
        {
            // look for the child: <enabled>
            xmlNodePtr enabled = findChildNode(currNode, NET_IFACE_ENABLED_NODE, false);
            if (enabled != NULL)
            {
                target->aggressiveRoamingEnabled = getXmlNodeContentsAsBoolean(enabled, false);
            }
        }
    }
}

/*
 * parse an XML node for information about a single network interfaces on the camera.
 * assumes 'funcArg' is a ohcmNetworkInterface object to contain the parsed data.
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmNetworkXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmNetworkInterface *target = (ohcmNetworkInterface *)funcArg;

    // parse XML that looks similar to:
    //
    //    <NetworkInterface version="1.0">
    //      <id>0</id>
    //      <enabled>true</enabled>
    //      <IPAddress version="1.0">
    //        <ipVersion>v4</ipVersion>
    //        <addressingType>dynamic</addressingType>
    //        <ipAddress>172.16.12.100</ipAddress>
    //        <subnetMask>255.255.255.0</subnetMask>
    //        <DefaultGateway>
    //          <ipAddress>172.16.12.1</ipAddress>
    //        </DefaultGateway>
    //        <PrimaryDNS>
    //          <ipAddress>172.16.12.1</ipAddress>
    //        </PrimaryDNS>
    //      </IPAddress>
    //      <Wireless version="1.0">
    //        <enabled>true</enabled>
    //        <wirelessNetworkMode>infrastructure</wirelessNetworkMode>
    //        <profile>
    //          <channel>11</channel>
    //          <ssid>MySSID</ssid>
    //          <wmmEnabled>true</wmmEnabled>
    //          <WirelessSecurity>
    //            <securityMode>WPA/WPA2-personal</securityMode>
    //            <WPA>
    //              <algorithmType>TKIP/AES</algorithmType>
    //              <sharedKey>MyPSK</sharedKey>
    //            </WPA>
    //          </WirelessSecurity>
    //        </profile>
    //        <statusRefreshInterval>0</statusRefreshInterval>
    //        <AggressiveRoaming>
    //          <enabled>true</enabled>
    //        </AggressiveRoaming>
    //      </Wireless>
    //      <Discovery version="1.0">
    //        <UPnP>
    //          <enabled>true</enabled>
    //        </UPnP>
    //      </Discovery>
    //    </NetworkInterface>
    //

    // this is called for each node within the parent node <NetworkInterface>
    //
    if (node == NULL || target == NULL)
    {
        return false;
    }

    if (strcmp((const char *) node->name, NET_IFACE_ID_NODE) == 0)
    {
        target->id = getXmlNodeContentsAsInt(node, 0);
    }
    else if (strcmp((const char *) node->name, NET_IFACE_ENABLED_NODE) == 0)
    {
        target->enabled = getXmlNodeContentsAsBoolean(node, false);
    }
    else if (strcmp((const char *) node->name, NET_IFACE_IPADDR_NODE) == 0)
    {
        parseIPAddrNode(node, target);
    }
    else if (strcmp((const char *) node->name, NET_IFACE_WIRELESS_NODE) == 0)
    {
        parseWirelessNode(node, target);
    }
    else if (strcmp((const char *) node->name, NET_IFACE_DISCOVERY_NODE) == 0)
    {
        // TODO: discovery
    }
    else
    {
        icLogWarn(OHCM_LOG, "Unrecognized NetworkInterface xml node: %s", node->name);
    }

    return true;
}

/*
 * parse an XML node for information about the network interfaces on the cameras.
 * assumes 'funcArg' is a linkedList (to hold ohcmNetworkInterface objects).
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmNetworkListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    // parse XML that looks similar to:
    //
    //  <NetworkInterfaceList version="1.0">
    //    <NetworkInterface version="1.0">
    //      ....
    //    </NetworkInterface>
    //  </NetworkInterfaceList>
    //
    icLinkedList *list = (icLinkedList *) funcArg;

    if (strcmp(node->name, NET_IFACE_NODE) == 0)
    {
        /*
         * When we are parsing just the network list then the XML helper
         * will send us the network interface directly rather than the
         * list itself.
         */

        // create a new NetworkInterface object, parse it, then add to the list
        //
        ohcmNetworkInterface *interface = createOhcmNetworkInterface();
        ohcmParseXmlNodeChildren(node, parseOhcmNetworkXmlNode, interface);
        linkedListAppend(list, interface);
    }
    else
    {
        /*
         * When we are parsing the config file then the XML helper
         * will send us the network list directly rather than
         * the individual interfaces.
         */

        // should be a set of "NetworkInterface" nodes
        //
        xmlNodePtr currNode;

        for (currNode = node->children; currNode != NULL; currNode = currNode->next)
        {
            // skip comments, blanks, etc
            //
            if (currNode->type == XML_ELEMENT_NODE)
            {
                icLogDebug(OHCM_LOG, "Current node name: [%s]", currNode->name);
                if (strcmp((const char *) currNode->name, NET_IFACE_NODE) == 0)
                {
                    // create a new NetworkInterface object, parse it, then add to the list
                    //
                    ohcmNetworkInterface *interface = createOhcmNetworkInterface();
                    ohcmParseXmlNodeChildren(currNode, parseOhcmNetworkXmlNode, interface);
                    linkedListAppend(list, interface);
                }
            }
        }
    }

    return true;
}

static xmlNodePtr getOhcmNetworkInterfaceXml(const ohcmNetworkInterface* net)
{
    // generation of the XML is far less then parsing.  at a min we need:
    //
    //    <NetworkInterface version="1.0">
    //      <id>0</id>
    //      <enabled>true</enabled>
    //      <IPAddress version="1.0">
    //        <ipVersion>v4</ipVersion>
    //        <addressingType>dynamic</addressingType>
    //      </IPAddress>
    //      <Wireless version="1.0">
    //        <enabled>true</enabled>
    //        <wirelessNetworkMode>infrastructure</wirelessNetworkMode>
    //        <profile>
    //          <channel>auto</channel>
    //          <ssid>MySSID</ssid>
    //          <wmmEnabled>true</wmmEnabled>
    //          <WirelessSecurity>
    //            <securityMode>WPA/WPA2-personal</securityMode>
    //            <WPA>
    //              <algorithmType>TKIP/AES</algorithmType>
    //              <sharedKey>MyPSK</sharedKey>
    //            </WPA>
    //          </WirelessSecurity>
    //        </profile>
    //        <AggressiveRoaming>
    //          <enabled>true</enabled>
    //        </AggressiveRoaming>
    //      </Wireless>
    //      <Discovery version="1.0">
    //        <UPnP>
    //          <enabled>true</enabled>
    //        </UPnP>
    //      </Discovery>
    //    </NetworkInterface>
    //
    char buffer[1024];

    // first, the top-level wrapper node
    //
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST NET_IFACE_NODE);
    xmlNewProp(node, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);

    //  1.  base info (id, enabled)
    sprintf(buffer, "%"PRIi32, net->id);
    xmlNewTextChild(node, NULL, BAD_CAST NET_IFACE_ID_NODE, BAD_CAST buffer);
    xmlNewTextChild(node, NULL, BAD_CAST NET_IFACE_ENABLED_NODE, (net->enabled == true) ? "true" : "false");

    //  2.  <IPAddress> section
    xmlNodePtr ipSection = xmlNewNode(NULL, BAD_CAST NET_IFACE_IPADDR_NODE);
    xmlNewProp(ipSection, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(node, ipSection);
    xmlNewTextChild(ipSection, NULL, BAD_CAST NET_IFACE_IP_VER_NODE, (net->ipVersion == OHCM_IPV4) ? "v4" : "dual");
    xmlNewTextChild(ipSection, NULL, BAD_CAST NET_IFACE_ADDR_TYPE_NODE, (net->addressingType == OHCM_NET_ADDRESS_STATIC) ? "static" : "dynamic");

    //  3.  <Wireless> section
    xmlNodePtr wifiSection = xmlNewNode(NULL, BAD_CAST NET_IFACE_WIRELESS_NODE);
    xmlNewProp(wifiSection, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(node, wifiSection);
    xmlNewTextChild(wifiSection, NULL, BAD_CAST NET_IFACE_ENABLED_NODE, (net->wirelessEnabled) ? "true" : "false");
    if (net->wirelessNetworkMode != NULL)
    {
        xmlNewTextChild(wifiSection, NULL, BAD_CAST NET_IFACE_WIFI_MODE_NODE, BAD_CAST net->wirelessNetworkMode);
    }

    sprintf(buffer, "%" PRIi32, net->statusRefreshInterval);
    xmlNewTextChild(wifiSection, NULL, BAD_CAST NET_IFACE_REFRESH_INTERVAL, BAD_CAST buffer);

    // <profile>
    xmlNodePtr wifiProfile = xmlNewNode(NULL, BAD_CAST NET_IFACE_WIFI_PROFILE_NODE);
    xmlAddChild(wifiSection, wifiProfile);
    if (net->profileChannel != NULL)
    {
        xmlNewTextChild(wifiProfile, NULL, BAD_CAST NET_IFACE_WIFI_CHANNEL_NODE, BAD_CAST net->profileChannel);
    }
    if (net->profileSsid != NULL)
    {
        xmlNewTextChild(wifiProfile, NULL, BAD_CAST NET_IFACE_WIFI_SSID_NODE, BAD_CAST net->profileSsid);
    }
    xmlNewTextChild(wifiProfile, NULL, BAD_CAST NET_IFACE_WIFI_WMM_NODE, (net->profileWmmEnabled == true) ? "true" : "false");

    // <WirelessSecurity>
    xmlNodePtr wifiSecSection = xmlNewNode(NULL, BAD_CAST NET_IFACE_WIFI_SEC_NODE);
    xmlAddChild(wifiProfile, wifiSecSection);
    xmlNewTextChild(wifiSecSection, NULL, BAD_CAST NET_IFACE_WIFI_SEC_MODE_NODE, ohcmWifiSecurityTypeLabels[net->profileSecurityMode]);

    switch (net->profileSecurityMode)
    {
        case OHCM_SEC_DISABLED:
        case OHCM_SEC_WEP:
            // nothing to add here
            break;

        default: // <WPA>
        {
            xmlNodePtr wifiWpaNode = xmlNewNode(NULL, BAD_CAST NET_IFACE_WIFI_WPA_NODE);
            xmlAddChild(wifiSecSection, wifiWpaNode);
            xmlNewTextChild(wifiWpaNode, NULL, BAD_CAST NET_IFACE_WIFI_WPA_ALGO_NODE, ohcmWpaEncrAlgoTypeLabels[net->profileAlgorithmType]);
            if (net->profileSharedKey != NULL)
            {
                xmlNewTextChild(wifiWpaNode, NULL, BAD_CAST NET_IFACE_WIFI_WPA_KEY_NODE, BAD_CAST net->profileSharedKey);
            }
            break;
        }
    }

    // TODO: <AggressiveRoaming> at the end of <Wireless> ??

    //  4.  <Discovery> section
    xmlNodePtr discoverSection = xmlNewNode(NULL, BAD_CAST NET_IFACE_DISCOVERY_NODE);
    xmlNewProp(discoverSection, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(node, discoverSection);

    xmlNodePtr upnpNode = xmlNewNode(NULL, BAD_CAST NET_IFACE_UPNP_NODE);
    xmlAddChild(discoverSection, upnpNode);
    xmlNewTextChild(upnpNode, NULL, BAD_CAST NET_IFACE_ENABLED_NODE, "true");

    return node;
}

/*
 * add the XML for a single ohcmNetworkInterface
 */
static void appendOhcmNetworkInterfaceXml(xmlNodePtr rootNode, const ohcmNetworkInterface *net)
{
    xmlAddChild(rootNode, getOhcmNetworkInterfaceXml(net));
}


/*
 * generates XML for a set of ohcmNetworkInterface objects,
 * adding as a child of 'rootNode'.
 */
void appendOhcmNetworkInterfaceListXml(xmlNodePtr rootNode, icLinkedList *netList)
{
    // add each ohcmNetworkInterface from the list
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(netList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // make the node for this ohcmNetworkInterface
        //
        ohcmNetworkInterface *currNet = (ohcmNetworkInterface *)linkedListIteratorGetNext(loop);
        appendOhcmNetworkInterfaceXml(rootNode, currNet);
    }
    linkedListIteratorDestroy(loop);
}

/*
 * helper function to create a blank ohcmNetworkInterface object
 */
ohcmNetworkInterface *createOhcmNetworkInterface()
{
    return (ohcmNetworkInterface *)calloc(1, sizeof(ohcmNetworkInterface));
}

/*
 * helper function to destroy the ohcmNetworkInterface object
 */
void destroyOhcmNetworkInterface(ohcmNetworkInterface *obj)
{
    if (obj != NULL)
    {
        free(obj->ipAddress);
        obj->ipAddress = NULL;
        free(obj->subnetMask);
        obj->subnetMask = NULL;
        free(obj->gatewayIpAddress);
        obj->gatewayIpAddress = NULL;
        free(obj->primaryDNSIpAddress);
        obj->primaryDNSIpAddress = NULL;
        free(obj->secondaryDNSIpAddress);
        obj->secondaryDNSIpAddress = NULL;
        free(obj->wirelessNetworkMode);
        obj->wirelessNetworkMode = NULL;
        free(obj->profileChannel);
        obj->profileChannel = NULL;
        free(obj->profileSsid);
        obj->profileSsid = NULL;
        free(obj->profileSharedKey);
        obj->profileSharedKey = NULL;
        free(obj);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmNetworkInterface from a linked list
 */
void destroyOhcmNetworkInterfaceFromList(void *item)
{
    ohcmNetworkInterface *obj = (ohcmNetworkInterface *)item;
    destroyOhcmNetworkInterface(obj);
}

ohcmResultCode getOhcmNetworkInterfaceList(const ohcmCameraInfo *cam, icLinkedList* output)
{
    char* url;
    CURLcode rc = CURLE_URL_MALFORMAT;

    url = stringBuilder("https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, DEVICE_NETIFACE_URI);
    if (url) {

        // create the output buffer
        //
        icFifoBuff* chunk = fifoBuffCreate(1024);

        // create our CURL context
        //
        CURL* curl = createOhcmCurlContext();
        if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, url) failed at %s(%d)",__FILE__,__LINE__);
        }

        // perform the 'get' operation
        //
        rc = ohcmPerformCurlGet(curl, DEVICE_NETIFACE_URI, chunk, 2);
        if (rc == CURLE_OK) {
            if (isIcLogPriorityTrace() == true && chunk != NULL && fifoBuffGetPullAvailable(chunk) > 0) {
                icLogTrace(OHCM_LOG, "camera get: %s\n%s", DEVICE_NETIFACE_URI, (char*) fifoBuffPullPointer(chunk, 0));
            }

            // success with the 'get', so parse the result
            //
            if (ohcmParseXmlHelper(chunk, parseOhcmNetworkListXmlNode, output) == false) {
                // unable to parse result from camera
                //
                icLogWarn(OHCM_LOG, "error parsing results of %s", DEVICE_NETIFACE_URI);
                rc = CURLE_CONV_FAILED;
            }
        }

        // cleanup
        //
        destroyOhcmCurlContext(curl);
        fifoBuffDestroy(chunk);

        free(url);
    }

    return ohcmTranslateCurlCode(rc);
}

ohcmResultCode setOhcmNetworkInterface(const ohcmCameraInfo* cam,
                                       const ohcmNetworkInterface* network)
{
    CURLcode rc = CURLE_URL_MALFORMAT;

    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, getOhcmNetworkInterfaceXml(network));

    if (doc)
    {
        // create the output buffer
        //
        icFifoBuff *chunk = fifoBuffCreate(1024);
        icFifoBuff *payload = fifoBuffCreate(4096);

        char* url = stringBuilder("https://%s:%s@%s%s/%d",
                                  cam->userName,
                                  cam->password,
                                  cam->cameraIP,
                                  DEVICE_NETIFACE_URI,
                                  network->id);

        // convert XML to a string, then cleanup
        //
        ohcmExportXmlToBuffer(doc, payload);
        xmlFreeDoc(doc);

        // create our CURL context.  Note setting "UPLOAD=yes"
        // because the camera want's this to be received as a PUT
        //
        CURL *curl = createOhcmCurlContext();
        if (curl_easy_setopt(curl, CURLOPT_URL, url) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, url) failed at %s(%d)",__FILE__,__LINE__);
        }
        if (curl_easy_setopt(curl, CURLOPT_POST, 1) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POST, 1) failed at %s(%d)",__FILE__,__LINE__);
        }
        if (curl_easy_setopt(curl, CURLOPT_UPLOAD, 1) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_UPLOAD, 1) failed at %s(%d)",__FILE__,__LINE__);
        }
        if (curl_easy_setopt(curl, CURLOPT_INFILESIZE, fifoBuffGetPullAvailable(payload)) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_INFILESIZE, fifoBuffGetPullAvailable(payload)) failed at %s(%d)",__FILE__,__LINE__);
        }

        // add HTTP headers
        //
        struct curl_slist *header = NULL;
        header = curl_slist_append(header, OHCM_CONTENT_TYPE_HEADER);
        header = curl_slist_append(header, OHCM_CONN_CLOSE_HEADER);
        header = curl_slist_append(header, OHCM_SERVER_HEADER);
        if (curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header) != CURLE_OK)
        {
            icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header) failed at %s(%d)",__FILE__,__LINE__);
        }

        rc = ohcmPerformCurlPost(curl, DEVICE_NETIFACE_URI, payload, chunk, 5);
        if (rc == CURLE_OK)
        {
            // success with the 'post', so parse the result
            //
            ohcmBasicResponse result;
            memset(&result, 0, sizeof(ohcmBasicResponse));
            if (ohcmParseBasicResponse(chunk, &result) == false)
            {
                // error parsing, force a failure
                //
                icLogWarn(OHCM_LOG, "error parsing results of %s/%d", DEVICE_NETIFACE_URI, network->id);
                rc = CURLE_CONV_FAILED;
            }
            else
            {
                // look at the result code to see if it was successful.
                //
                rc = ohcmTranslateOhcmResponseCodeToCurl(result.statusCode);
                if (rc == CURLE_OK)
                {
                    icLogDebug(OHCM_LOG, "set network interface was SUCCESSFUL");
                }
                else if (rc == CURLE_LDAP_CANNOT_BIND)
                {
                    icLogDebug(OHCM_LOG, "set network interface success, responded with 'Needs Reboot'");
                }
                else if (result.statusMessage != NULL)
                {
                    icLogWarn(OHCM_LOG,
                        "result of %s/%d contained error: %s - %s",
                              DEVICE_NETIFACE_URI,
                              network->id,
                              ohcmResponseCodeLabels[result.statusCode],
                              result.statusMessage);
                }
            }

            free(result.statusMessage);
        }

        // cleanup
        //
        curl_slist_free_all(header);
        destroyOhcmCurlContext(curl);

        fifoBuffDestroy(chunk);
        fifoBuffDestroy(payload);   // also free's xmlBuff

        free(url);
    }

    return ohcmTranslateCurlCode(rc);
}
