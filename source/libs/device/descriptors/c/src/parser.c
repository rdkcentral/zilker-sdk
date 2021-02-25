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
// Created by Thomas Lea on 7/31/15.
//

#include <libxml/tree.h>
#include <icLog/logging.h>
#include <xmlHelper/xmlHelper.h>
#include <string.h>
#include <deviceDescriptor.h>
#include <icTypes/icStringHashMap.h>
#include <inttypes.h>
#include "parser.h"
#include <icUtil/stringUtils.h>
#include <icUtil/fileUtils.h>

#define LOG_TAG "libdeviceDescriptorParser"

#define DDL_ROOT_NODE               "DeviceDescriptorList"
#define CAMERA_DD_NODE              "CameraDeviceDescriptor"
#define ZIGBEE_DD_NODE              "DeviceDescriptor"
#define UUID_NODE                   "uuid"
#define DESCRIPTION_NODE            "description"
#define MANUFACTURER_NODE           "manufacturer"
#define MODEL_NODE                  "model"
#define HARDWARE_VERSIONS_NODE      "hardwareVersions"
#define FIRMWARE_VERSIONS_NODE      "firmwareVersions"
#define MIN_FIRMWARE_VERSIONS_NODE  "minSupportedFirmwareVersion"
#define METADATA_LIST_NODE          "metadataList"
#define METADATA_NODE               "metadata"
#define NAME_NODE                   "name"
#define VALUE_NODE                  "value"
#define LATEST_FIRMWARE_NODE        "latestFirmware"
#define LATEST_FIRMWARE_VERSION_NODE "version"
#define LATEST_FIRMWARE_FILENAME_NODE "filename"
#define LATEST_FIRMWARE_TYPE_NODE   "type"
#define LATEST_FIRMWARE_TYPE_ZIGBEE_OTA    "ota"
#define LATEST_FIRMWARE_TYPE_ZIGBEE_LEGACY "legacy"
#define LATEST_FIRMWARE_CHECKSUM_ATTRIBUTE "checksum"
#define VERSION_LIST_FORMAT         "format"
#define PROTOCOL_NODE               "protocol"
#define MOTION_NODE                 "motion"
#define LIST_NODE                   "list"
#define ANY_NODE                    "any"
#define RANGE_NODE                  "range"
#define ENABLED_NODE                "enabled"
#define SENSITIVITY_NODE            "sensitivityLevel"
#define LOW_NODE                    "low"
#define MEDIUM_NODE                 "med"
#define HIGH_NODE                   "high"
#define DETECTION_NODE              "detectionThreshold"
#define REGION_OF_INTEREST_NODE     "regionOfInterest"
#define WIDTH_NODE                  "width"
#define HEIGHT_NODE                 "height"
#define BOTTOM_NODE                 "bottomCoord"
#define TOP_NODE                    "topCoord"
#define LEFT_NODE                   "leftCoord"
#define RIGHT_NODE                  "rightCoord"
#define FROM_NODE                   "from"
#define TO_NODE                     "to"

static char *getTrimmedXmlNodeContentsAsString(xmlNodePtr node);

static void parseDeviceVersionList(xmlNodePtr node, DeviceVersionList *list)
{
    list->format = getXmlNodeAttributeAsString(node, VERSION_LIST_FORMAT, NULL);
    xmlNodePtr loopNode = node->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, LIST_NODE) == 0)
        {
            list->listType = DEVICE_VERSION_LIST_TYPE_LIST;
            list->list.versionList = linkedListCreate();
            xmlNodePtr listNode = currNode->children;
            xmlNodePtr listItemNode = NULL;
            for (listItemNode = listNode; listItemNode != NULL; listItemNode = listItemNode->next)
            {
                // skip comments, blanks, etc
                //
                if (listItemNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                linkedListAppend(list->list.versionList, getTrimmedXmlNodeContentsAsString(listItemNode));
            }
            break;
        }
        else if (strcmp((const char *) currNode->name, ANY_NODE) == 0)
        {
            list->listType = DEVICE_VERSION_LIST_TYPE_WILDCARD;
            break;
        }
        else if (strcmp((const char *) currNode->name, RANGE_NODE) == 0)
        {
            list->listType = DEVICE_VERSION_LIST_TYPE_RANGE;
            xmlNodePtr rangeNode = currNode->children;
            xmlNodePtr rangeItemNode = NULL;
            for (rangeItemNode = rangeNode; rangeItemNode != NULL; rangeItemNode = rangeItemNode->next)
            {
                // skip comments, blanks, etc
                //
                if (rangeItemNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }


                char *version = getTrimmedXmlNodeContentsAsString(rangeItemNode);
                if (version != NULL)
                {
                    if (strcmp((const char *) rangeItemNode->name, FROM_NODE) == 0)
                    {
                        list->list.versionRange.from = version;
                    }
                    else if (strcmp((const char *) rangeItemNode->name, TO_NODE) == 0)
                    {
                        list->list.versionRange.to = version;
                    }
                    else
                    {
                        //unused
                        free(version);
                    }
                }
            }
            break;
        }
        else
        {
            icLogError(LOG_TAG, "Unexpected device version list type");
            break;
        }
    }
}

//given a version string that could be decimal or hex, convert the string to decimal
static char * versionStringToDecimalString(const char* version)
{
    int value = (int)strtol(version, NULL, 0);
    char buf[20];
    sprintf(buf, "%" PRIu32, value);
    return trimString(buf);
}

static void parseZigbeeDeviceVersion(xmlNodePtr node, DeviceVersionList *list, bool forceDecimal)
{
    char * versions = getXmlNodeContentsAsString(node, NULL);

    list->format = strdup("Zigbee");

    if (strlen(versions) > 0 && versions[0] == '*')
    {
        list->listType = DEVICE_VERSION_LIST_TYPE_WILDCARD;
    }
    else if (strstr(versions, ",") != NULL)
    {
        list->listType = DEVICE_VERSION_LIST_TYPE_LIST;
        list->list.versionList = linkedListCreate();
        char * it = versions;
        char * token;
        while((token = strsep(&it, ",")) != NULL)
        {
            char * v = forceDecimal ? versionStringToDecimalString(token) : trimString(token);
            if (!forceDecimal)
            {
                stringToLowerCase(v);
            }
            linkedListAppend(list->list.versionList, v);
        }
    }
    else if (strstr(versions, "-") != NULL)
    {
        char * it = versions;
        char * first = strsep(&it, "-");
        char * second = strsep(&it, "-");
        list->listType = DEVICE_VERSION_LIST_TYPE_RANGE;

        char * v1 = forceDecimal ? versionStringToDecimalString(first) : trimString(first);
        if (!forceDecimal)
        {
            stringToLowerCase(v1);
        }
        list->list.versionRange.from = v1;

        char * v2 = forceDecimal ? versionStringToDecimalString(second) : trimString(second);
        if (!forceDecimal)
        {
            stringToLowerCase(v2);
        }
        list->list.versionRange.to = v2;
    }
    else
    {
        list->listType = DEVICE_VERSION_LIST_TYPE_LIST;
        list->list.versionList = linkedListCreate();

        char * v = forceDecimal ? versionStringToDecimalString(versions) : trimString(versions);
        linkedListAppend(list->list.versionList, v);
    }

    free(versions);
}

static bool parseMetadataList(xmlNodePtr metadataNode, icStringHashMap *map)
{
    bool result = true;

    xmlNodePtr loopNode = metadataNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, METADATA_NODE) == 0)
        {
            char *name = getXmlNodeAttributeAsString(currNode, NAME_NODE, NULL);
            char *value = getXmlNodeAttributeAsString(currNode, VALUE_NODE, NULL);

            if(name != NULL && value != NULL)
            {
                stringHashMapPut(map, name, value);
            }
            else
            {
                if(name != NULL)
                {
                    free(name);
                }
                if(value != NULL)
                {
                    free(value);
                }
            }
        }
    }

    return result;
}

static bool parseDescriptorBase(xmlNodePtr ddNode, DeviceDescriptor *dd)
{
    bool result = true;

    xmlNodePtr loopNode = ddNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, UUID_NODE) == 0)
        {
            dd->uuid = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, DESCRIPTION_NODE) == 0)
        {
            dd->description = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, MANUFACTURER_NODE) == 0)
        {
            dd->manufacturer = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, MODEL_NODE) == 0)
        {
            dd->model = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, HARDWARE_VERSIONS_NODE) == 0)
        {
            dd->hardwareVersions = (DeviceVersionList *) calloc(1, sizeof(DeviceVersionList));
            if (dd->hardwareVersions == NULL)
            {
                icLogError(LOG_TAG, "Failed to allocate DeviceVersionList");
                break;
            }
            if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
            {
                parseDeviceVersionList(currNode, dd->hardwareVersions);
            }
            else
            {
                parseZigbeeDeviceVersion(currNode, dd->hardwareVersions, true);
            }
        }
        else if (strcmp((const char *) currNode->name, FIRMWARE_VERSIONS_NODE) == 0)
        {
            dd->firmwareVersions = (DeviceVersionList *) calloc(1, sizeof(DeviceVersionList));
            if (dd->firmwareVersions == NULL)
            {
                icLogError(LOG_TAG, "Failed to allocate DeviceVersionList");
                break;
            }
            if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
            {
                parseDeviceVersionList(currNode, dd->firmwareVersions);
            }
            else
            {
                parseZigbeeDeviceVersion(currNode, dd->firmwareVersions, false);
            }
        }
        else if (strcmp((const char *) currNode->name, MIN_FIRMWARE_VERSIONS_NODE) == 0)
        {
            dd->minSupportedFirmwareVersion = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, METADATA_LIST_NODE) == 0)
        {
            dd->metadata = stringHashMapCreate();
            parseMetadataList(currNode, dd->metadata);
        }
        else if (strcmp((const char *) currNode->name, LATEST_FIRMWARE_NODE) == 0)
        {
            dd->latestFirmware = (DeviceFirmware *) calloc(1, sizeof(DeviceFirmware));
            if (dd->latestFirmware == NULL)
            {
                icLogError(LOG_TAG, "Failed to allocate DeviceFirmware");
            }
            else
            {
                if (dd->deviceDescriptorType == DEVICE_DESCRIPTOR_TYPE_CAMERA)
                {
                    dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_CAMERA;
                }
                else
                {
                    dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_UNKNOWN;
                }
                xmlNodePtr latestFirmwareLoopNode = currNode->children;
                xmlNodePtr latestFirmwareCurrNode = NULL;
                for (latestFirmwareCurrNode = latestFirmwareLoopNode;
                     latestFirmwareCurrNode != NULL; latestFirmwareCurrNode = latestFirmwareCurrNode->next)
                {
                    if (strcmp((const char *) latestFirmwareCurrNode->name, LATEST_FIRMWARE_VERSION_NODE) == 0)
                    {
                        dd->latestFirmware->version = getTrimmedXmlNodeContentsAsString(latestFirmwareCurrNode);
                    }
                    else if (strcmp((const char *) latestFirmwareCurrNode->name, LATEST_FIRMWARE_FILENAME_NODE) == 0)
                    {
                        if (dd->latestFirmware->filenames == NULL)
                        {
                            dd->latestFirmware->filenames = linkedListCreate();
                        }

                        linkedListAppend(dd->latestFirmware->filenames, getTrimmedXmlNodeContentsAsString(latestFirmwareCurrNode));
                    }
                    else if(strcmp((const char*) latestFirmwareCurrNode->name, LATEST_FIRMWARE_TYPE_NODE) == 0)
                    {
                        char *firmwareType =  getXmlNodeContentsAsString(latestFirmwareCurrNode, NULL);
                        // Java code uses case insensitive comparison here
                        if (strcasecmp(firmwareType, LATEST_FIRMWARE_TYPE_ZIGBEE_OTA) == 0)
                        {
                            dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_ZIGBEE_OTA;
                        }
                        else if (strcasecmp(firmwareType, LATEST_FIRMWARE_TYPE_ZIGBEE_LEGACY) == 0)
                        {
                            dd->latestFirmware->type = DEVICE_FIRMWARE_TYPE_ZIGBEE_LEGACY;
                        }
                        free(firmwareType);
                    }

                    dd->latestFirmware->checksum = getXmlNodeAttributeAsString(latestFirmwareCurrNode,
                                                                               LATEST_FIRMWARE_CHECKSUM_ATTRIBUTE,
                                                                               NULL);
                }
            }
        }
    }

    return result;
}

static bool parseCameraMotionNode(xmlNodePtr motionNode, CameraDeviceDescriptor *dd)
{
    bool result = true;
    xmlNodePtr loopNode = motionNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, ENABLED_NODE) == 0)
        {
            char *enabled = getXmlNodeContentsAsString(currNode, "false");
            if (strcmp(enabled, "true") == 0)
            {
                dd->defaultMotionSettings.enabled = true;
            }
            else
            {
                dd->defaultMotionSettings.enabled = false;
            }
            free(enabled);
        } else if (strcmp((const char *) currNode->name, SENSITIVITY_NODE) == 0)
        {
            xmlNodePtr innerCurrNode = NULL;
            for (innerCurrNode = currNode->children; innerCurrNode != NULL; innerCurrNode = innerCurrNode->next)
            {
                if (innerCurrNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                if (strcmp((const char *) innerCurrNode->name, LOW_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.sensitivity.low = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid sensitivityLevel value for low");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, MEDIUM_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.sensitivity.medium = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid sensitivityLevel value for medium");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, HIGH_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.sensitivity.high = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid sensitivityLevel value for high");
                    }
                }
            }
        } else if (strcmp((const char *) currNode->name, DETECTION_NODE) == 0)
        {
            xmlNodePtr innerCurrNode = NULL;
            for (innerCurrNode = currNode->children; innerCurrNode != NULL; innerCurrNode = innerCurrNode->next)
            {
                if (innerCurrNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                if (strcmp((const char *) innerCurrNode->name, LOW_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.detectionThreshold.low = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid detectionThreshold value for low");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, MEDIUM_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.detectionThreshold.medium = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid detectionThreshold value for medium");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, HIGH_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.detectionThreshold.high = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid detectionThreshold value for high");
                    }
                }
            }
        } else if (strcmp((const char *) currNode->name, REGION_OF_INTEREST_NODE) == 0)
        {
            xmlNodePtr innerCurrNode = NULL;
            for (innerCurrNode = currNode->children; innerCurrNode != NULL; innerCurrNode = innerCurrNode->next)
            {
                if (innerCurrNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                if (strcmp((const char *) innerCurrNode->name, WIDTH_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.width = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for width");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, HEIGHT_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.height = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for height");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, BOTTOM_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.bottom = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for bottom");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, TOP_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.top = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for top");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, LEFT_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.left = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for left");
                    }
                } else if (strcmp((const char *) innerCurrNode->name, RIGHT_NODE) == 0)
                {
                    char *tmp = getXmlNodeContentsAsString(innerCurrNode, NULL);
                    if (tmp != NULL)
                    {
                        dd->defaultMotionSettings.regionOfInterest.right = atoi(tmp);
                        free(tmp);
                    }
                    else
                    {
                        icLogError(LOG_TAG, "Invalid regionOfInterest value for right");
                    }
                }
            }
        }
    }

    return result;
}

static bool parseCameraDescriptor(xmlNodePtr cameraNode, CameraDeviceDescriptor *dd)
{
    bool result = true;
    xmlNodePtr loopNode = cameraNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        // skip comments, blanks, etc
        //
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, PROTOCOL_NODE) == 0)
        {
            char *protocol = getXmlNodeContentsAsString(currNode, NULL);
            if (protocol != NULL)
            {
                if (strcmp(protocol, "legacy") == 0)
                {
                    dd->protocol = CAMERA_PROTOCOL_LEGACY;
                }
                else if (strcmp(protocol, "openHome") == 0)
                {
                    dd->protocol = CAMERA_PROTOCOL_OPENHOME;
                }
                else
                {
                    dd->protocol = CAMERA_PROTOCOL_UNKNOWN;
                }

                free(protocol);
            }
        }
        else if (strcmp((const char *) currNode->name, MOTION_NODE) == 0)
        {
            result = parseCameraMotionNode(currNode, dd);
        }
    }

    return result;
}

icStringHashMap *getBlacklistedUuids(const char *blacklistPath)
{
    icStringHashMap *result = NULL;

    if(doesNonEmptyFileExist(blacklistPath))
    {
        xmlDocPtr doc = NULL;
        xmlNodePtr topNode = NULL;

        if ((doc = xmlParseFile(blacklistPath)) == NULL)
        {
            // log line used for Telemetry do not edit/delete
            //
            icLogError(LOG_TAG, "Blacklist Failed to parse, for file %s", blacklistPath);

            return result;
        }

        if ((topNode = xmlDocGetRootElement(doc)) == NULL)
        {
            // log line used for Telemetry do not edit/delete
            //
            icLogWarn(LOG_TAG, "Blacklist Failed to parse, unable to find contents of %s", DDL_ROOT_NODE);

            xmlFreeDoc(doc);
            return result;
        }

        result = stringHashMapCreate();

        // loop through the children of ROOT
        xmlNodePtr loopNode = topNode->children;
        xmlNodePtr currNode = NULL;
        for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
        {
            if (currNode->type == XML_ELEMENT_NODE && strcmp((const char *) currNode->name, UUID_NODE) == 0)
            {
                if (currNode->children == NULL)
                {
                    icLogWarn(LOG_TAG, "%s: ignoring empty uuid node", __FUNCTION__);
                    continue;
                }
                char *uuid = strdup(currNode->children->content);
                if (stringHashMapPut(result, uuid, NULL) == false)
                {
                    icLogWarn(LOG_TAG, "%s: failed to add %s", __FUNCTION__, uuid);
                    free(uuid);
                }
            }
        }

        xmlFreeDoc(doc);
    }

    return result;
}

icLinkedList *parseDeviceDescriptors(const char *whitelistPath, const char *blacklistPath)
{
    icLinkedList *result = NULL;
    xmlDocPtr doc = NULL;
    xmlNodePtr topNode = NULL;

    if (doesNonEmptyFileExist(whitelistPath))
    {
        icLogDebug(LOG_TAG, "Parsing device descriptor list at %s", whitelistPath);
    }
    else
    {
        icLogWarn(LOG_TAG, "Invalid/missing device descriptor list at %s", whitelistPath);
        return result;
    }

    if ((doc = xmlParseFile(whitelistPath)) == NULL)
    {
        // log line used for Telemetry do not edit/delete
        //
        icLogError(LOG_TAG, "Whitelist Failed to parse, for file %s", whitelistPath);

        return result;
    }

    if ((topNode = xmlDocGetRootElement(doc)) == NULL)
    {
        // log line used for Telemetry do not edit/delete
        //
        icLogWarn(LOG_TAG, "Whitelist Failed to parse, unable to find contents of %s", DDL_ROOT_NODE);

        xmlFreeDoc(doc);
        return result;
    }

    //if we have a blacklist, go ahead and parse it into a set of uuids
    icStringHashMap *blacklistedUuids = getBlacklistedUuids(blacklistPath);

    //we got this far, so the input looks like something we can work with... allocate our result
    result = linkedListCreate();

    // loop through the children of ROOT
    xmlNodePtr loopNode = topNode->children;
    xmlNodePtr currNode = NULL;
    for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
    {
        DeviceDescriptor *dd = NULL;

        // skip comments, blanks, etc
        if (currNode->type != XML_ELEMENT_NODE)
        {
            continue;
        }

        if (strcmp((const char *) currNode->name, CAMERA_DD_NODE) == 0)
        {
            dd = calloc(1, sizeof(CameraDeviceDescriptor));
            dd->deviceDescriptorType = DEVICE_DESCRIPTOR_TYPE_CAMERA;

            if (parseDescriptorBase(currNode, (DeviceDescriptor *) dd) == false ||
                parseCameraDescriptor(currNode, (CameraDeviceDescriptor*)dd) == false)
            {
                // log line used for Telemetry do not edit/delete
                //
                icLogError(LOG_TAG, "Whitelist Failed to parse, Camera device descriptor problem");

                deviceDescriptorFree((DeviceDescriptor *) dd);
                dd = NULL;
            }
        }
        else if (strcmp((const char *) currNode->name, ZIGBEE_DD_NODE) == 0)
        {
            dd = calloc(1, sizeof(ZigbeeDeviceDescriptor));
            dd->deviceDescriptorType = DEVICE_DESCRIPTOR_TYPE_ZIGBEE;

            if (parseDescriptorBase(currNode, (DeviceDescriptor *) dd) == false)
            {
                // log line used for Telemetry do not edit/delete
                //
                icLogError(LOG_TAG, "Whitelist Failed to parse, Zigbee device descriptor problem");

                deviceDescriptorFree((DeviceDescriptor *) dd);
                dd = NULL;
            }
        }

        if (dd != NULL)
        {
            if (blacklistedUuids != NULL && stringHashMapContains(blacklistedUuids, dd->uuid) == true)
            {
                icLogInfo(LOG_TAG, "%s: descriptor %s blacklisted", __FUNCTION__, dd->uuid);
                deviceDescriptorFree((DeviceDescriptor *) dd);
            }
            else
            {
                linkedListAppend(result, dd);
            }
        }
    }

    stringHashMapDestroy(blacklistedUuids, NULL);
    xmlFreeDoc(doc);
    return result;
}

static char *getTrimmedXmlNodeContentsAsString(xmlNodePtr node)
{
    char *result = NULL;

    char *contents = getXmlNodeContentsAsString(node, NULL);
    if (contents != NULL)
    {
        result = trimString(contents);
        free(contents);
    }

    return result;
}
