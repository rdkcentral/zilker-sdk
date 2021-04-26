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
 * ohcmMotion.c
 *
 * implementation of "motion" functionality as defined in ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <string.h>
#include <inttypes.h>

#include <openHomeCamera/ohcm.h>
#include <xmlHelper/xmlHelper.h>
#include "ohcmBase.h"

#define MOTION_POLL_URI             "/OpenHome/System/Poll/notifications"
#define MOTION_DETECTION_VIDEO_URI  "/OpenHome/Event/MotionDetection/video"
#define MOTION_EVENT_URI            "/OpenHome/Event"

#define MOTION_DETECTION_NODE               "MotionDetection"
#define MOTION_DETECTION_ID_NODE                "id"
#define MOTION_DETECTION_ENABLED_NODE           "enabled"
#define MOTION_DETECTION_INPUTID_NODE           "inputID"
#define MOTION_DETECTION_SAMPLE_INTERVAL_NODE   "samplingInterval"
#define MOTION_DETECTION_START_TRIG_TIME_NODE   "startTriggerTime"
#define MOTION_DETECTION_END_TRIG_TIME_NODE     "endTriggerTime"
#define MOTION_DETECTION_DIRECTION_NODE         "directionSensitivity"
#define MOTION_DETECTION_REGION_TYPE_NODE       "regionType"
#define MOTION_DETECTION_MIN_OBJ_SIZE_NODE      "minObjectSize"
#define MOTION_DETECTION_MAX_OBJ_SIZE_NODE      "maxObjectSize"

#define MOTION_DETECTION_GRID_NODE      "Grid"
#define MOTION_DETECTION_ROW_GRAN_NODE      "rowGranularity"
#define MOTION_DETECTION_COL_GRAN_NODE      "columnGranularity"

#define MOTION_DETECTION_ROI_NODE           "ROI"
#define MOTION_DETECTION_MIN_HORIZ_RES_NODE     "minHorizontalResolution"
#define MOTION_DETECTION_MIN_VERT_RES_NODE      "minVerticalResolution"
#define MOTION_DETECTION_SRC_HORIZ_RES_NODE     "sourceHorizontalResolution"
#define MOTION_DETECTION_SRC_VERT_RES_NODE      "sourceVerticalResolution"

#define MOTION_DETECTION_REGION_LIST_NODE       "MotionDetectionRegionList"
#define MOTION_DETECTION_REGION_NODE                "MotionDetectionRegion"
#define MOTION_DETECTION_REGION_MASK_NODE           "maskEnabled"
#define MOTION_DETECTION_REGION_SENSITIVY_NODE      "sensitivityLevel"
#define MOTION_DETECTION_REGION_DETECT_NODE         "detectionThreshold"

#define MOTION_DETECTION_REGION_COORD_LIST_NODE "RegionCoordinatesList"
#define MOTION_DETECTION_REGION_COORD_NODE          "RegionCoordinates"
#define MOTION_DETECTION_REGION_COORD_POSX_NODE     "positionX"
#define MOTION_DETECTION_REGION_COORD_POSY_NODE     "positionY"

#define MOTION_EVENT_NOTIFICATION_TOP_NODE  "EventNotification"
#define MOTION_EVENT_TRIGGER_LIST_NODE      "EventTriggerList"
#define MOTION_EVENT_TRIGGER_NODE               "EventTrigger"
#define MOTION_EVENT_TRIGGER_TYPE_NODE          "eventType"
#define MOTION_EVENT_TRIGGER_TYPE_INPUTID_NODE  "eventTypeInputID"
#define MOTION_EVENT_TRIGGER_INTERVAL_NODE      "intervalBetweenEvents"
#define MOTION_EVENT_TRIGGER_DESC_NODE          "eventDescription"
#define MOTION_EVENT_TRIGGER_INPUT_PORT_NODE    "inputIOPortID"

#define MOTION_EVENT_NOTIF_LIST_NODE    "EventTriggerNotificationList"
#define MOTION_EVENT_NOTIF_NODE             "EventTriggerNotification"
#define MOTION_EVENT_NOTIF_ID_NODE          "notificationID"
#define MOTION_EVENT_NOTIF_METHOD_NODE      "notificationMethod"
#define MOTION_EVENT_NOTIF_RECURE_NODE      "notificationRecurrence"
#define MOTION_EVENT_NOTIF_INTERVAL_NODE    "notificationInterval"

#define MOTION_EVENT_NOTIF_METHODS_NODE     "EventNotificationMethods"

#define MOTION_EVENT_HOST_NOTIF_LIST_NODE   "HostNotificationList"
#define MOTION_EVENT_HOST_NOTIF_NODE            "HostNotification"
#define MOTION_EVENT_HOST_ID_NODE               "id"
#define MOTION_EVENT_HOST_URL_NODE              "url"
#define MOTION_EVENT_HOST_AUTH_NODE             "httpAuthenticationMethod"

#define MOTION_EVENT_NON_MEDIA_NODE         "NonMediaEvent"

/*
 * helper function to create a blank ohcmMotionDetection object
 */
ohcmMotionDetection *createOhcmMotionDetection()
{
    // make an empty object, then assign linked lists
    //
    ohcmMotionDetection *retVal = (ohcmMotionDetection *)calloc(1, sizeof(ohcmMotionDetection));
    retVal->regionList = linkedListCreate();
    return retVal;
}

/*
 * helper function to destroy the ohcmMotionDetection object
 */
void destroyOhcmMotionDetection(ohcmMotionDetection *obj)
{
    if (obj != NULL)
    {
        free(obj->id);
        obj->id = NULL;
        free(obj->inputID);
        obj->inputID = NULL;
        linkedListDestroy(obj->regionList, destroyOhcmMotionDetectRegionFromList);
        obj->regionList = NULL;

        free(obj);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmMotionDetection from a linked list
 */
void destroyOhcmMotionDetectionFromList(void *item)
{
    ohcmMotionDetection *obj = (ohcmMotionDetection *)item;
    destroyOhcmMotionDetection(obj);
}

/*
 * helper function to create a blank ohcmMotionDetectRegion object
 */
ohcmMotionDetectRegion *createOhcmMotionDetectRegion()
{
    ohcmMotionDetectRegion *retVal = (ohcmMotionDetectRegion *)calloc(1, sizeof(ohcmMotionDetectRegion));
    retVal->coordinatesList = linkedListCreate();
    return retVal;
}

/*
 * helper function to destroy the ohcmMotionDetectRegion object
 */
void destroyOhcmMotionDetectRegion(ohcmMotionDetectRegion *obj)
{
    if (obj != NULL)
    {
        linkedListDestroy(obj->coordinatesList, destroyOhcmRegionCoordinateFromList);
        obj->coordinatesList = NULL;
        free(obj->id);
        obj->id = NULL;
        free(obj);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmMotionDetectRegion from a linked list
 */
void destroyOhcmMotionDetectRegionFromList(void *item)
{
    ohcmMotionDetectRegion *obj = (ohcmMotionDetectRegion *)item;
    destroyOhcmMotionDetectRegion(obj);
}

/*
 * helper function to create a blank ohcmRegionCoordinate object
 */
ohcmRegionCoordinate *createOhcmRegionCoordinate()
{
    return (ohcmRegionCoordinate *)calloc(1, sizeof(ohcmRegionCoordinate));
}

/*
 * helper function to destroy the ohcmRegionCoordinate object
 */
void destroyOhcmRegionCoordinate(ohcmRegionCoordinate *obj)
{
    if (obj != NULL)
    {
        free(obj);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmRegionCoordinate from a linked list
 */
void destroyOhcmRegionCoordinateFromList(void *item)
{
    ohcmRegionCoordinate *obj = (ohcmRegionCoordinate *)item;
    destroyOhcmRegionCoordinate(obj);
}

/*
 * assume ohcmMotionDetectRegion
 */
static bool parseOhcmMotionDetectionRegionXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmMotionDetectRegion *region = (ohcmMotionDetectRegion *)funcArg;

    if (strcmp((const char *)node->name, MOTION_DETECTION_ID_NODE) == 0)
    {
        region->id = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_ENABLED_NODE) == 0)
    {
        region->enabled = getXmlNodeContentsAsBoolean(node, false);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_REGION_MASK_NODE) == 0)
    {
        region->maskEnabled = getXmlNodeContentsAsBoolean(node, false);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_REGION_SENSITIVY_NODE) == 0)
    {
        region->sensitivityLevel = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_REGION_DETECT_NODE) == 0)
    {
        region->detectionThreshold = getXmlNodeContentsAsUnsignedInt(node, 0);
    }

    else if (strcmp((const char *)node->name, MOTION_DETECTION_REGION_COORD_LIST_NODE) == 0)
    {
        // should be a list of "RegionCoordinates"
        //
        xmlNodePtr currNode = NULL;
        xmlNodePtr loopNode = node->children;
        for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
        {
            // skip comments, blanks, etc
            //
            if (currNode->type != XML_ELEMENT_NODE)
            {
                continue;
            }

            if (strcmp((const char *) currNode->name, MOTION_DETECTION_REGION_COORD_NODE) == 0)
            {
                // has 2 elements: x/y
                xmlNodePtr xNode = findChildNode(currNode, MOTION_DETECTION_REGION_COORD_POSX_NODE, false);
                xmlNodePtr yNode = findChildNode(currNode, MOTION_DETECTION_REGION_COORD_POSY_NODE, false);
                if (xNode != NULL && yNode != NULL)
                {
                    ohcmRegionCoordinate *coordinate = createOhcmRegionCoordinate();
                    coordinate->positionX = getXmlNodeContentsAsUnsignedInt(xNode, 0);
                    coordinate->positionY = getXmlNodeContentsAsUnsignedInt(yNode, 0);

                    linkedListAppend(region->coordinatesList, coordinate);
                }
            }
        }
    }

    return true;
}

/*
 * parse an XML node for information about a single motion detection setting.
 * assumes 'funcArg' is a ohcmMotionDetection object.
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
static bool parseOhcmMotionDetectionXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmMotionDetection *motion = (ohcmMotionDetection *)funcArg;

    if (strcmp((const char *)node->name, MOTION_DETECTION_ID_NODE) == 0)
    {
        motion->id = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_ENABLED_NODE) == 0)
    {
        motion->enabled = getXmlNodeContentsAsBoolean(node, false);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_INPUTID_NODE) == 0)
    {
        motion->inputID = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_SAMPLE_INTERVAL_NODE) == 0)
    {
        motion->samplingInterval = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_START_TRIG_TIME_NODE) == 0)
    {
        motion->startTriggerTime = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_END_TRIG_TIME_NODE) == 0)
    {
        motion->endTriggerTime = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_DIRECTION_NODE) == 0)
    {
        char *tmp = getXmlNodeContentsAsString(node, NULL);
        if (tmp != NULL)
        {
            if (strcmp(tmp, "left-right") == 0)
            {
                motion->directionSensitivity = OHCM_MOTION_DIR_LEFT_RIGHT;
            }
            else if (strcmp(tmp, "right-left") == 0)
            {
                motion->directionSensitivity = OHCM_MOTION_DIR_RIGHT_LEFT;
            }
            else if (strcmp(tmp, "up-down") == 0)
            {
                motion->directionSensitivity = OHCM_MOTION_DIR_UP_DOWN;
            }
            else if (strcmp(tmp, "down-up") == 0)
            {
                motion->directionSensitivity = OHCM_MOTION_DIR_DOWN_UP;
            }
            else
            {
                motion->directionSensitivity = OHCM_MOTION_DIR_ANY;
            }
        }
        free(tmp);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_REGION_TYPE_NODE) == 0)
    {
        char *tmp = getXmlNodeContentsAsString(node, NULL);
        if (tmp != NULL)
        {
            if (strcmp(tmp, "roi") == 0)
            {
                motion->regionType = OHCM_MOTION_REGION_ROI;
            }
            else
            {
                motion->regionType = OHCM_MOTION_REGION_GRID;
            }
        }
        free(tmp);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_MIN_OBJ_SIZE_NODE) == 0)
    {
        motion->minObjectSize = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_MAX_OBJ_SIZE_NODE) == 0)
    {
        motion->maxObjectSize = getXmlNodeContentsAsUnsignedInt(node, 0);
    }

    // Grid & ROI
    else if (strcmp((const char *)node->name, MOTION_DETECTION_GRID_NODE) == 0)
    {
        // has 2 elements: row/col
        xmlNodePtr rowNode = findChildNode(node, MOTION_DETECTION_ROW_GRAN_NODE, false);
        if (rowNode != NULL)
        {
            motion->rowGranularity = getXmlNodeContentsAsUnsignedInt(rowNode, 0);
        }
        xmlNodePtr colNode = findChildNode(node, MOTION_DETECTION_COL_GRAN_NODE, false);
        if (colNode != NULL)
        {
            motion->columnGranularity = getXmlNodeContentsAsUnsignedInt(colNode, 0);
        }
    }
    else if (strcmp((const char *)node->name, MOTION_DETECTION_ROI_NODE) == 0)
    {
        // has 4 elements: min horiz/vert & source horiz/vert
        xmlNodePtr tmpNode;
        if ((tmpNode = findChildNode(node, MOTION_DETECTION_MIN_HORIZ_RES_NODE, false)) != NULL)
        {
            motion->minHorizontalResolution = getXmlNodeContentsAsUnsignedInt(tmpNode, 0);
        }
        if ((tmpNode = findChildNode(node, MOTION_DETECTION_MIN_VERT_RES_NODE, false)) != NULL)
        {
            motion->minVerticalResolution = getXmlNodeContentsAsUnsignedInt(tmpNode, 0);
        }
        if ((tmpNode = findChildNode(node, MOTION_DETECTION_SRC_HORIZ_RES_NODE, false)) != NULL)
        {
            motion->sourceHorizontalResolution = getXmlNodeContentsAsUnsignedInt(tmpNode, 0);
        }
        if ((tmpNode = findChildNode(node, MOTION_DETECTION_SRC_VERT_RES_NODE, false)) != NULL)
        {
            motion->sourceVerticalResolution = getXmlNodeContentsAsUnsignedInt(tmpNode, 0);
        }
    }

    // region
    else if (strcmp((const char *)node->name, MOTION_DETECTION_REGION_LIST_NODE) == 0)
    {
        // should be a list of "MotionDetectionRegion"
        //
        xmlNodePtr currNode = NULL;
        xmlNodePtr loopNode = node->children;
        for (currNode = loopNode; currNode != NULL; currNode = currNode->next)
        {
            // skip comments, blanks, etc
            //
            if (currNode->type != XML_ELEMENT_NODE)
            {
                continue;
            }

            if (strcmp((const char *) currNode->name, MOTION_DETECTION_REGION_NODE) == 0)
            {
                // create a ohcmMotionDetectRegion, then populate it
                //
                ohcmMotionDetectRegion *region = createOhcmMotionDetectRegion();
                ohcmParseXmlNodeChildren(currNode, parseOhcmMotionDetectionRegionXmlNode, region);
                linkedListAppend(motion->regionList, region);
            }
        }
    }

    return true;
}

/*
 * parse an XML node for information about a list of motion detection settings.
 * assumes 'funcArg' is a linkedList (to hold ohcmMotionDetection objects).
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmMotionDetectionListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    /*
     * parse XML similar to:
     *   <MotionDetectionList version="1.0">
     *      <MotionDetection version="1.0">
     *          ....
     *      </MotionDetection>
     *  </MotionDetectionList>
     */
    icLinkedList *list = (icLinkedList *)funcArg;

    //Caller is iterating over the "MotionDetectionList" so 'node' should be a "MotionDetection" node
    if (strcmp((const char *) node->name, MOTION_DETECTION_NODE) == 0)
    {
        // parse the 'motion detection' node then add to our list
        //
        ohcmMotionDetection *motion = createOhcmMotionDetection();
        ohcmParseXmlNodeChildren(node, parseOhcmMotionDetectionXmlNode, motion);
        linkedListAppend(list, motion);
    }

    return true;
}

/*
 * query the camera for the current 'motion detection configuration'.
 * if successful, will populate the supplied 'outputList' with ohcmMotionDetection
 * objects (which should be released by caller).
 *
 * @param cam - device to contact
 * @param outputList - linked likst to populate with ohcmMotionDetection objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmMotionDetection(ohcmCameraInfo *cam, icLinkedList *outputList, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, MOTION_DETECTION_VIDEO_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, MOTION_DETECTION_VIDEO_URI);

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
    CURLcode rc = ohcmPerformCurlGet(curl, debugUrl, chunk, retryCounts);
    if (rc == CURLE_OK)
    {
        if (isIcLogPriorityTrace() == true && chunk != NULL && fifoBuffGetPullAvailable(chunk) > 0)
        {
            icLogTrace(OHCM_LOG, "camera get: %s\n%s", debugUrl, (char *)fifoBuffPullPointer(chunk, 0));
        }

        // success with the 'get', so parse the result
        //
        if (ohcmParseXmlHelper(chunk, parseOhcmMotionDetectionListXmlNode, outputList) == false)
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

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

/*
 *
 */
static void appendMotionDetectionRegionXml(xmlNodePtr rootNode, ohcmMotionDetectRegion *region)
{
    // make the outer node
    //
    xmlNodePtr regionNode = xmlNewNode(NULL, BAD_CAST MOTION_DETECTION_REGION_NODE);
    xmlAddChild(rootNode, regionNode);

    // add the children
    //
    char tmp[128];
    xmlNewTextChild(regionNode, NULL, BAD_CAST MOTION_DETECTION_ID_NODE, BAD_CAST region->id);
    xmlNewTextChild(regionNode, NULL, BAD_CAST MOTION_DETECTION_ENABLED_NODE, BAD_CAST ((region->enabled) ? "true" : "false"));
    xmlNewTextChild(regionNode, NULL, BAD_CAST MOTION_DETECTION_REGION_MASK_NODE, BAD_CAST ((region->maskEnabled) ? "true" : "false"));
    sprintf(tmp, "%"PRIu32, region->sensitivityLevel);
    xmlNewTextChild(regionNode, NULL, BAD_CAST MOTION_DETECTION_REGION_SENSITIVY_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, region->detectionThreshold);
    xmlNewTextChild(regionNode, NULL, BAD_CAST MOTION_DETECTION_REGION_DETECT_NODE, BAD_CAST tmp);

    // add coordinates
    if (linkedListCount(region->coordinatesList) > 0)
    {
        // need RegionCoordinatesList
        xmlNodePtr coordListNode = xmlNewNode(NULL, BAD_CAST MOTION_DETECTION_REGION_COORD_LIST_NODE);
        xmlAddChild(regionNode, coordListNode);

        icLinkedListIterator *loop = linkedListIteratorCreate(region->coordinatesList);
        while (linkedListIteratorHasNext(loop) == true)
        {
            ohcmRegionCoordinate *coord = (ohcmRegionCoordinate *)linkedListIteratorGetNext(loop);

            xmlNodePtr coordNode = xmlNewNode(NULL, BAD_CAST MOTION_DETECTION_REGION_COORD_NODE);
            xmlAddChild(coordListNode, coordNode);
            sprintf(tmp, "%"PRIu32, coord->positionX);
            xmlNewTextChild(coordNode, NULL, BAD_CAST MOTION_DETECTION_REGION_COORD_POSX_NODE, BAD_CAST tmp);
            sprintf(tmp, "%"PRIu32, coord->positionY);
            xmlNewTextChild(coordNode, NULL, BAD_CAST MOTION_DETECTION_REGION_COORD_POSY_NODE, BAD_CAST tmp);
        }
        linkedListIteratorDestroy(loop);
    }
}

/*
 * create the XML to send to the camera, enabling motion detection
 */
static void appendMotionDetectionXml(xmlNodePtr rootNode, ohcmMotionDetection *detect)
{
    char tmp[128];

    xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_ID_NODE, BAD_CAST detect->id);
    xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_ENABLED_NODE, BAD_CAST ((detect->enabled) ? "true" : "false"));
    if (detect->inputID != NULL)
    {
        xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_INPUTID_NODE, BAD_CAST detect->inputID);
    }

/* not used and causes problems
    sprintf(tmp, "%"PRIu32, detect->samplingInterval);
    xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_SAMPLE_INTERVAL_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, detect->startTriggerTime);
    xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_START_TRIG_TIME_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, detect->endTriggerTime);
    xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_END_TRIG_TIME_NODE, BAD_CAST tmp);
 */

    switch(detect->directionSensitivity)
    {
        case OHCM_MOTION_DIR_LEFT_RIGHT:
            xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_DIRECTION_NODE, BAD_CAST "left-right");
            break;
        case OHCM_MOTION_DIR_RIGHT_LEFT:
            xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_DIRECTION_NODE, BAD_CAST "right-left");
            break;
        case OHCM_MOTION_DIR_UP_DOWN:
            xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_DIRECTION_NODE, BAD_CAST "up-down");
            break;
        case OHCM_MOTION_DIR_DOWN_UP:
            xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_DIRECTION_NODE, BAD_CAST "down-up");
            break;
        case OHCM_MOTION_DIR_ANY:
            xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_DIRECTION_NODE, BAD_CAST "any");
            break;
    }
    switch(detect->regionType)
    {
        case OHCM_MOTION_REGION_ROI:
            xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_REGION_TYPE_NODE, BAD_CAST "roi");
            break;
        case OHCM_MOTION_REGION_GRID:
            xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_REGION_TYPE_NODE, BAD_CAST "grid");
            break;
    }

    // cannot have min == max
    if (detect->minObjectSize != detect->maxObjectSize)
    {
        sprintf(tmp, "%"PRIu32, detect->minObjectSize);
        xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_MIN_OBJ_SIZE_NODE, BAD_CAST tmp);
        sprintf(tmp, "%"PRIu32, detect->maxObjectSize);
        xmlNewTextChild(rootNode, NULL, BAD_CAST MOTION_DETECTION_MAX_OBJ_SIZE_NODE, BAD_CAST tmp);
    }

    // ROI & Grid
    if (detect->regionType == OHCM_MOTION_REGION_GRID)
    {
        xmlNodePtr gridNode = xmlNewNode(NULL, BAD_CAST MOTION_DETECTION_GRID_NODE);
        xmlAddChild(rootNode, gridNode);
        sprintf(tmp, "%"PRIu32, detect->rowGranularity);
        xmlNewTextChild(gridNode, NULL, BAD_CAST MOTION_DETECTION_ROW_GRAN_NODE, BAD_CAST tmp);
        sprintf(tmp, "%"PRIu32, detect->columnGranularity);
        xmlNewTextChild(gridNode, NULL, BAD_CAST MOTION_DETECTION_COL_GRAN_NODE, BAD_CAST tmp);
    }
    else    // ROI
    {
        xmlNodePtr roiNode = xmlNewNode(NULL, BAD_CAST MOTION_DETECTION_ROI_NODE);
        xmlAddChild(rootNode, roiNode);
        sprintf(tmp, "%"PRIu32, detect->minHorizontalResolution);
        xmlNewTextChild(roiNode, NULL, BAD_CAST MOTION_DETECTION_MIN_HORIZ_RES_NODE, BAD_CAST tmp);
        sprintf(tmp, "%"PRIu32, detect->minVerticalResolution);
        xmlNewTextChild(roiNode, NULL, BAD_CAST MOTION_DETECTION_MIN_VERT_RES_NODE, BAD_CAST tmp);
        sprintf(tmp, "%"PRIu32, detect->sourceHorizontalResolution);
        xmlNewTextChild(roiNode, NULL, BAD_CAST MOTION_DETECTION_SRC_HORIZ_RES_NODE, BAD_CAST tmp);
        sprintf(tmp, "%"PRIu32, detect->sourceVerticalResolution);
        xmlNewTextChild(roiNode, NULL, BAD_CAST MOTION_DETECTION_SRC_VERT_RES_NODE, BAD_CAST tmp);
    }

    // regions
    xmlNodePtr regionsNode = xmlNewNode(NULL, BAD_CAST MOTION_DETECTION_REGION_LIST_NODE);
    xmlNewProp(regionsNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(rootNode, regionsNode);
    icLinkedListIterator *loop = linkedListIteratorCreate(detect->regionList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // add XML for this region and it's coordinates
        //
        ohcmMotionDetectRegion *region = (ohcmMotionDetectRegion *)linkedListIteratorGetNext(loop);
        appendMotionDetectionRegionXml(regionsNode, region);
    }
    linkedListIteratorDestroy(loop);
}

/*
 * request the camera apply a 'motion detection configuration' for a UID
 * (uses the settings->uid).
 *
 * @param cam - device to contact
 * @param settings - the motion detection settings to apply
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmMotionDetectionForUid(ohcmCameraInfo *cam, ohcmMotionDetection *settings, uint32_t retryCounts)
{
    // build up the URL to hit
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl, "https://%s:%s@%s%s/%s", cam->userName, cam->password, cam->cameraIP, MOTION_DETECTION_VIDEO_URI, settings->id);
    sprintf(debugUrl, "https://%s%s/%s", cam->cameraIP, MOTION_DETECTION_VIDEO_URI, settings->id);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

    // create the payload
    // first, build up the XML doc
    //
    icFifoBuff *payload = fifoBuffCreate(2048);
    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST MOTION_DETECTION_NODE);
    xmlNewProp(root, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, root);
    appendMotionDetectionXml(root, settings);

    // convert XML to a string, then cleanup
    //
    ohcmExportXmlToBuffer(doc, payload);
    xmlFreeDoc(doc);

    // create our CURL context.  Note setting "UPLOAD=yes"
    // because the camera want's this to be received as a PUT
    //
    CURL *curl = createOhcmCurlContext();
    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
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

    // perform the 'post' operation.
    //
    CURLcode rc = ohcmPerformCurlPost(curl, debugUrl, payload, chunk, retryCounts);
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
            icLogWarn(OHCM_LOG, "error parsing results of %s", debugUrl);
            rc = CURLE_CONV_FAILED;
        }
        else
        {
            // look at the result code to see if it was successful.
            //
            rc = ohcmTranslateOhcmResponseCodeToCurl(result.statusCode);
            if (rc != CURLE_OK && result.statusMessage != NULL)
            {
                icLogWarn(OHCM_LOG, "result of %s contained error: %s - %s",
                          debugUrl, ohcmResponseCodeLabels[result.statusCode], result.statusMessage);
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

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

/*
 * helper function to create a blank ohcmHostNotif object
 */
ohcmHostNotif *createOhcmHostNotif()
{
    return (ohcmHostNotif *)calloc(1, sizeof(ohcmHostNotif));
}

/*
 * helper function to destroy the ohcmHostNotif object
 */
void destroyOhcmHostNotif(ohcmHostNotif *obj)
{
    if (obj != NULL)
    {
        free(obj->id);
        obj->id = NULL;
        free(obj->url);
        obj->url = NULL;
        free(obj->httpAuthenticationMethod);
        obj->httpAuthenticationMethod = NULL;

        free(obj);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmHostNotif from a linked list
 */
void destroyOhcmHostNotifFromList(void *item)
{
    ohcmHostNotif *obj = (ohcmHostNotif *)item;
    destroyOhcmHostNotif(obj);
}

/*
 * helper function to create a blank ohcmEventNotifMethods object
 */
ohcmEventNotifMethods *createOhcmEventNotifMethods()
{
    // make an empty object, then assign linked lists
    //
    ohcmEventNotifMethods *retVal = (ohcmEventNotifMethods *)calloc(1, sizeof(ohcmEventNotifMethods));
    retVal->hostNotifList = linkedListCreate();
    return retVal;
}

/*
 * helper function to destroy the ohcmEventNotifMethods object
 */
void destroyOhcmEventNotifMethods(ohcmEventNotifMethods *obj)
{
    if (obj != NULL)
    {
        linkedListDestroy(obj->hostNotifList, destroyOhcmHostNotifFromList);
        obj->hostNotifList = NULL;
        free(obj);
    }
}

/*
 * helper function to create a blank ohcmEventTriggerNotif object
 */
ohcmEventTriggerNotif *createOhcmEventTriggerNotif()
{
    return (ohcmEventTriggerNotif *)calloc(1, sizeof(ohcmEventTriggerNotif));
}

/*
 * helper function to destroy the ohcmEventTriggerNotif object
 */
void destroyOhcmEventTriggerNotif(ohcmEventTriggerNotif *obj)
{
    if (obj != NULL)
    {
        free(obj->notificationID);
        obj->notificationID = NULL;
        free(obj->notificationMethod);
        obj->notificationMethod = NULL;
        free(obj->notificationRecurrence);
        obj->notificationRecurrence = NULL;

        free(obj);
    }
}

/*
 * helper function to create a blank ohcmEventTrigger object
 */
ohcmEventTrigger *createOhcmEventTrigger()
{
    ohcmEventTrigger *retVal = (ohcmEventTrigger *)calloc(1, sizeof(ohcmEventTrigger));
    retVal->notif = createOhcmEventTriggerNotif();
    return retVal;
}

/*
 * helper function to destroy the ohcmEventTrigger object
 */
void destroyOhcmEventTrigger(ohcmEventTrigger *obj)
{
    if (obj != NULL)
    {
        free(obj->id);
        obj->id = NULL;
        destroyOhcmEventTriggerNotif(obj->notif);
        obj->notif = NULL;

        free(obj);
    }
}

/*
 * create the XML to send to the camera, configuring the motion event trigger
 */
static void appendMotionEventTriggerXml(xmlNodePtr rootNode, ohcmEventTrigger *trigger)
{
    char tmp[128];

    // add the trigger
    xmlNodePtr triggerNode = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_TRIGGER_NODE);
    xmlNewProp(triggerNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(rootNode, triggerNode);

    xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_DETECTION_ID_NODE, BAD_CAST trigger->id);
    switch (trigger->eventType)
    {
        case OHCM_EVENT_TRIGGER_PIRMD:
            xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_EVENT_TRIGGER_TYPE_NODE, BAD_CAST "PirMD");
            break;
        case OHCM_EVENT_TRIGGER_VMD:
            xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_EVENT_TRIGGER_TYPE_NODE, BAD_CAST "VMD");
            break;
        case OHCM_EVENT_TRIGGER_SND:
            xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_EVENT_TRIGGER_TYPE_NODE, BAD_CAST "SndD");
            break;
        case OHCM_EVENT_TRIGGER_TMPD:
            xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_EVENT_TRIGGER_TYPE_NODE, BAD_CAST "TempD");
            break;
    }

    if (trigger->eventTypeInputID != NULL)
    {
        xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_EVENT_TRIGGER_TYPE_INPUTID_NODE, BAD_CAST trigger->eventTypeInputID);
    }
    sprintf(tmp, "%"PRIu32, trigger->intervalBetweenEvents);
    xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_EVENT_TRIGGER_INTERVAL_NODE, BAD_CAST tmp);

    if (trigger->eventDescription != NULL)
    {
        xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_EVENT_TRIGGER_DESC_NODE, BAD_CAST trigger->eventDescription);
    }
    if (trigger->inputIOPortID != NULL)
    {
        xmlNewTextChild(triggerNode, NULL, BAD_CAST MOTION_EVENT_TRIGGER_INPUT_PORT_NODE, BAD_CAST trigger->inputIOPortID);
    }

    // now the EventTriggerNotificationList
    if (trigger->notif != NULL)
    {
        // list wrapper
        xmlNodePtr listNode = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_NOTIF_LIST_NODE);
        xmlNewProp(listNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
        xmlAddChild(triggerNode, listNode);

        // notification wrapper
        xmlNodePtr notifNode = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_NOTIF_NODE);
        xmlNewProp(notifNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
        xmlAddChild(listNode, notifNode);

        if (trigger->notif->notificationID != NULL)
        {
            xmlNewTextChild(notifNode, NULL, BAD_CAST MOTION_EVENT_NOTIF_ID_NODE, BAD_CAST trigger->notif->notificationID);
        }
        if (trigger->notif->notificationMethod != NULL)
        {
            xmlNewTextChild(notifNode, NULL, BAD_CAST MOTION_EVENT_NOTIF_METHOD_NODE, BAD_CAST trigger->notif->notificationMethod);
        }
        if (trigger->notif->notificationRecurrence != NULL)
        {
            xmlNewTextChild(notifNode, NULL, BAD_CAST MOTION_EVENT_NOTIF_RECURE_NODE, BAD_CAST trigger->notif->notificationRecurrence);
        }
        if (trigger->notif->notificationInterval > 0)
        {
            sprintf(tmp, "%"PRIu32, trigger->notif->notificationInterval);
            xmlNewTextChild(notifNode, NULL, BAD_CAST MOTION_EVENT_NOTIF_INTERVAL_NODE, BAD_CAST tmp);
        }
    }
}

/*
 * create the XML to send to the camera, configuring the motion event delivery
 */
static void appendMotionEventNotificationXml(xmlNodePtr rootNode, ohcmEventNotifMethods *trigger)
{
    char tmp[128];

    // 2 sections:  HostNotificationList & NonMediaEvent
    //
    xmlNodePtr hostListNode = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_HOST_NOTIF_LIST_NODE);
    xmlNewProp(hostListNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(rootNode, hostListNode);
    icLinkedListIterator *loop = linkedListIteratorCreate(trigger->hostNotifList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        ohcmHostNotif *host = (ohcmHostNotif *)linkedListIteratorGetNext(loop);

        // HostNotification node
        xmlNodePtr hostNode = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_HOST_NOTIF_NODE);
        xmlNewProp(hostNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
        xmlAddChild(hostListNode, hostNode);

        if (host->id != NULL)
        {
            xmlNewTextChild(hostNode, NULL, BAD_CAST MOTION_EVENT_HOST_ID_NODE, BAD_CAST host->id);
        }
        if (host->url != NULL)
        {
            xmlNewTextChild(hostNode, NULL, BAD_CAST MOTION_EVENT_HOST_URL_NODE, BAD_CAST host->url);
        }
        if (host->httpAuthenticationMethod != NULL)
        {
            xmlNewTextChild(hostNode, NULL, BAD_CAST MOTION_EVENT_HOST_AUTH_NODE, BAD_CAST host->httpAuthenticationMethod);
        }
        else
        {
            xmlNewTextChild(hostNode, NULL, BAD_CAST MOTION_EVENT_HOST_AUTH_NODE, BAD_CAST "none");
        }
    }
    linkedListIteratorDestroy(loop);

    // non media
    xmlNodePtr nonMediaNode = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_NON_MEDIA_NODE);
    xmlAddChild(rootNode, nonMediaNode);
    xmlNewTextChild(nonMediaNode, NULL, BAD_CAST MOTION_DETECTION_ENABLED_NODE, BAD_CAST ((trigger->nonMediaEvent) ? "true" : "false"));
}

/*
 * request the camera apply a 'motion event delivery' mechanism
 *
 * @param cam - device to contact
 * @param trigger - ???
 * @param method - mechanism to use for event delivery
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmMotionEvent(ohcmCameraInfo *cam, ohcmEventTrigger *trigger, ohcmEventNotifMethods *method, uint32_t retryCounts)
{
    // build up the URL to hit
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl, "https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, MOTION_EVENT_URI);
    sprintf(debugUrl, "https://%s%s", cam->cameraIP, MOTION_EVENT_URI);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

    // create the payload
    // first, build up the XML doc
    //
    icFifoBuff *payload = fifoBuffCreate(2048);
    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_NOTIFICATION_TOP_NODE);
    xmlNewProp(root, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, root);

    // broken into 2 sections:
    // 1. EventTriggerList
    //
    xmlNodePtr triggerNode = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_TRIGGER_LIST_NODE);
    xmlNewProp(triggerNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, triggerNode);
    appendMotionEventTriggerXml(triggerNode, trigger);

    // 2. EventNotificationMethods
    //
    xmlNodePtr notifNode = xmlNewNode(NULL, BAD_CAST MOTION_EVENT_NOTIF_METHODS_NODE);
    xmlNewProp(notifNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(root, notifNode);
    appendMotionEventNotificationXml(notifNode, method);

    // convert XML to a string, then cleanup
    //
    ohcmExportXmlToBuffer(doc, payload);
    xmlFreeDoc(doc);

    // create our CURL context.  Note setting "UPLOAD=yes"
    // because the camera want's this to be received as a PUT
    //
    CURL *curl = createOhcmCurlContext();
    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
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

    // perform the 'post' operation.
    //
    CURLcode rc = ohcmPerformCurlPost(curl, debugUrl, payload, chunk, retryCounts);
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
            icLogWarn(OHCM_LOG, "error parsing results of %s", debugUrl);
            rc = CURLE_CONV_FAILED;
        }
        else
        {
            // look at the result code to see if it was successful.
            //
            rc = ohcmTranslateOhcmResponseCodeToCurl(result.statusCode);
            if (rc != CURLE_OK && result.statusMessage != NULL)
            {
                icLogWarn(OHCM_LOG, "result of %s contained error: %s - %s",
                          debugUrl, ohcmResponseCodeLabels[result.statusCode], result.statusMessage);
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

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

/*
 * parse the XML results to the MOTION_POLL_URI request.
 * assumes 'funcArg' is an ohcmPollNotifResult enum.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
static bool parsePollNotificationXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmPollNotifResult *status = (ohcmPollNotifResult *) funcArg;

    // the root node of the XML response should be one of several possibilities.
    // 1. camera reporting motion, so embedded within a <NotificationWrapper> node:
    //  <NotificationWrapper version="1.0">
    //      <notificationURI>eventalertsystem</notificationURI>
    //      <notifyTime>2016-05-26T04:03:46.102-08:00</notifyTime>
    //      <notifyBody>
    //          <EventAlert version="1.0">
    //              <id>827568</id>
    //              <dateTime>2016-05-26T04:03:46.101-08:00</dateTime>
    //              <activePostCount>1</activePostCount>
    //              <eventType>VMD</eventType>
    //              <eventState>active</eventState>
    //              <DetectionRegionList>
    //                  <DetectionRegionEntry>
    //                      <regionID>f555f551-15e1-45a3-af9f-8fe856339c2c</regionID>
    //                      <sensitivityLevel>80</sensitivityLevel>
    //                      <detectionThreshold>0</detectionThreshold>
    //                  </DetectionRegionEntry>
    //              </DetectionRegionList>
    //          </EventAlert>
    //      </notifyBody>
    //  </NotificationWrapper>
    //
    // 2. Camera reporting a doorbell button press event (this is in progres... Sercomm is reporting 'Unknown' for this)
    //  <NotificationWrapper version="1.0">
    //   <notificationURI>eventalertsystem</notificationURI>
    //   <notifyTime>2016-10-17T05:17:45.479-08:00</notifyTime>
    //   <notifyBody>
    //      <EventAlert version="1.0">
    //         <id>1378</id>
    //         <dateTime>2016-10-17T05:17:45.478-08:00</dateTime>
    //         <activePostCount>1</activePostCount>
    //         <eventType>Unknown</eventType>
    //         <eventState>active</eventState>
    //      </EventAlert>
    //   </notifyBody>
    // </NotificationWrapper>
    //
    // 3. camera reporting error, placed within a <ResponseStatus> node:
    //  <ResponseStatus version="1.0">
    //      <requestURL>/OpenHome/System/Poll/notifications</requestURL>
    //      <statusCode>3</statusCode>
    //      <statusString>Device Error</statusString>
    //  </ResponseStatus>
    //
    //

    // for scenario #1 and #2, look for the 'notifyBody' node
    //
    if (strcmp((const char *)node->name, "notifyBody") == 0)
    {
        // see if there's an "eventType" node
        //
        xmlNodePtr eventTypeNode = findChildNode(node, "eventType", true);
        if (eventTypeNode != NULL)
        {
            // see if VMD or Unknown
            //
            char *tmp = getXmlNodeContentsAsString(eventTypeNode, "");
            if (strcmp(tmp, "VMD") == 0)
            {
                // got a motion event
                //
                *status = POLL_MOTION_EVENT;
            }
            else if (strcmp(tmp, "Unknown") == 0)
            {
                // got a button event
                //
                *status = POLL_BUTTON_EVENT;
            }
            free(tmp);
        }
        else
        {
            // must be NO MOTION
            //
            *status = POLL_NO_EVENT;
        }

        // no sense continuing the parsing
        //
        return false;
    }

    // possibly scenario #3 OR no event
    //
    else if (strcmp((const char *)top, "statusCode") == 0)
    {
        // something went wrong
        //
        icLogWarn(OHCM_LOG, "%s returned ERROR, bailing on XML parsing", MOTION_POLL_URI);
        *status = POLL_RESULT_ERROR;
        return false;
    }

    // return true so we can continue with the parsing
    //
    return true;
}

/*
 * Perform a blocking 'poll' of the camera to see if there are motion events to report.
 * This synchronous call will block for 'waitSecs' seconds for an event to occur.
 *
 * @param cam : The camera to poll
 * @param waitSecs : number of seconds to wait for an event before disconnecting
 *
 * @return POLL_NO_EVENT if 'waitSecs' elapsed with nothing to report;
 *         POLL_MOTION_EVENT if a motion event occurs;
 *         or POLL_ERROR if unable to connect to the camera device
 */
ohcmPollNotifResult getOhcmPollNotification(ohcmCameraInfo *cam, uint8_t waitSecs)
{
    ohcmPollNotifResult retVal = POLL_RESULT_ERROR;

    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, MOTION_POLL_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, MOTION_POLL_URI);

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
    // overload standard timeout
    if (curl_easy_setopt(curl, CURLOPT_TIMEOUT, waitSecs) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_TIMEOUT, waitSecs) failed at %s(%d)",__FILE__,__LINE__);
    }

    // perform the 'get' operation
    //
    CURLcode rc = ohcmPerformCurlGet(curl, debugUrl, chunk, 1);
    if (rc == CURLE_OK)
    {
        uint32_t len = fifoBuffGetPullAvailable(chunk);
        if (len > 1)
        {
            // got something worth parsing
            //
            if (isIcLogPriorityTrace() == true)
            {
                icLogTrace(OHCM_LOG, "camera get: %s\n%s", debugUrl, (char *) fifoBuffPullPointer(chunk, 0));
            }

            // success with the 'get', so parse the result
            //
            if (ohcmParseXmlHelper(chunk, parsePollNotificationXmlNode, &retVal) == false)
            {
                // unable to parse result from camera
                //
                icLogWarn(OHCM_LOG, "error parsing results of %s", debugUrl);
                rc = CURLE_CONV_FAILED;
            }
        }
        else
        {
            // empty result, no motion
            //
            retVal = POLL_NO_EVENT;
        }
    }
    else
    {
        // couldn't read the camera, see what happened
        //
        ohcmResultCode error = ohcmTranslateCurlCode(rc);
        switch (error)
        {
            case OHCM_COMM_FAIL:
            case OHCM_COMM_TIMEOUT:
            case OHCM_SSL_FAIL:
            case OHCM_LOGIN_FAIL:
                // network error
                //
                retVal = POLL_COMM_ERROR;
                break;

            default:
                // something wrong
                retVal = POLL_RESULT_ERROR;
                break;
        }
    }

    // cleanup
    //
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);

    return retVal;
}

