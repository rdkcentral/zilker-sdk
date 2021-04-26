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
 * ohcmDevice.c
 *
 * implementation of "device" functionality as defined in ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <string.h>

#include <libxml/parser.h>
#include <openHomeCamera/ohcm.h>
#include <xmlHelper/xmlHelper.h>
#include "ohcmBase.h"

#define DEVICE_INFO_URI         "/OpenHome/System/deviceInfo"
#define DEVICE_NAME_NODE              "deviceName"
#define DEVICE_ID_NODE                "deviceID"
#define DEVICE_MANUFACTURER_NODE      "manufacturer"
#define DEVICE_MODEL_NODE             "model"
#define DEVICE_SERIAL_NUM_NODE        "serialNumber"
#define DEVICE_MAC_ADDR_NODE          "macAddress"
#define DEVICE_FW_VERSION_NODE        "firmwareVersion"
#define DEVICE_FW_RELEASE_DATE_NODE   "firmwareReleasedDate"
#define DEVICE_BOOT_VERSION_NODE      "bootVersion"
#define DEVICE_BOOT_RELEASE_DATE_NODE "bootReleasedDate"
#define DEVICE_RESCUE_VERSION_NODE    "rescueVersion"
#define DEVICE_HW_VERSION_NODE        "hardwareVersion"
#define DEVICE_API_VERSION_NODE       "apiVersion"

#define DEVICE_PING_URI               "/OpenHome/System/Ping"
#define DEVICE_REBOOT_URI             "/OpenHome/System/reboot"
#define DEVICE_FACTORYRESET_URI       "/OpenHome/System/factoryReset"


/*
 * helper function to create a new ohcmCameraInfo object (with NULL values)
 */
ohcmCameraInfo *createOhcmCameraInfo()
{
    return (ohcmCameraInfo *)calloc(1, sizeof(ohcmCameraInfo));
}

/*
 * helper function to destroy the ohcmCameraInfo object, along with it's contents
 */
void destroyOhcmCameraInfo(ohcmCameraInfo *cam)
{
    if (cam != NULL)
    {
        free(cam->cameraIP);
        cam->cameraIP = NULL;
        free(cam->macAddress);
        cam->macAddress = NULL;
        free(cam->password);
        cam->password = NULL;
        free(cam->userName);
        cam->userName = NULL;

        free(cam);
    }
}

/*
 * helper function to create a new ohcmDeviceInfo object (with NULL values)
 */
ohcmDeviceInfo *createOhcmDeviceInfo()
{
    return (ohcmDeviceInfo *)calloc(1, sizeof(ohcmDeviceInfo));
}

/*
 * helper function to destroy the ohcmDeviceInfo object, along with it's contents
 */
void destroyOhcmDeviceInfo(ohcmDeviceInfo *device)
{
    if (device != NULL)
    {
        free(device->deviceName);
        device->deviceName = NULL;
        free(device->deviceID);
        device->deviceID = NULL;
        free(device->manufacturer);
        device->manufacturer = NULL;
        free(device->model);
        device->model = NULL;
        free(device->serialNumber);
        device->serialNumber = NULL;
        free(device->macAddress);
        device->macAddress = NULL;
        free(device->firmwareVersion);
        device->firmwareVersion = NULL;
        free(device->firmwareReleasedDate);
        device->firmwareReleasedDate = NULL;
        free(device->bootVersion);
        device->bootVersion = NULL;
        free(device->bootReleasedDate);
        device->bootReleasedDate = NULL;
        free(device->rescueVersion);
        device->rescueVersion = NULL;
        free(device->hardwareVersion);
        device->hardwareVersion = NULL;
        free(device->apiVersion);
        device->apiVersion = NULL;

        free(device);
    }
}

/*
 * debug print the contents
 */
void printDeviceInfo(ohcmDeviceInfo *device)
{
    icLogDebug(OHCM_LOG, "==================");
    icLogDebug(OHCM_LOG, "DEVICE INFORMATION");
    icLogDebug(OHCM_LOG, "==================");
    if(device->deviceName != NULL)
    {
        icLogDebug(OHCM_LOG, "deviceName : %s",device->deviceName);
    }
    if(device->deviceID != NULL)
    {
        icLogDebug(OHCM_LOG, "deviceID : %s",device->deviceID);
    }
    if(device->manufacturer != NULL)
    {
        icLogDebug(OHCM_LOG, "manufacturer : %s",device->manufacturer);
    }
    if(device->model != NULL)
    {
        icLogDebug(OHCM_LOG, "model : %s",device->model);
    }
    if(device->serialNumber != NULL)
    {
        icLogDebug(OHCM_LOG, "serialNumber : %s",device->serialNumber);
    }
    if(device->macAddress != NULL)
    {
        icLogDebug(OHCM_LOG, "macAddress : %s",device->macAddress);
    }
    if(device->firmwareVersion != NULL)
    {
        icLogDebug(OHCM_LOG, "firmwareVersion : %s",device->firmwareVersion);
    }
    if(device->firmwareReleasedDate != NULL)
    {
        icLogDebug(OHCM_LOG, "firmwareReleasedDate : %s",device->firmwareReleasedDate);
    }
    if(device->bootVersion != NULL)
    {
        icLogDebug(OHCM_LOG, "bootVersion : %s",device->bootVersion);
    }
    if(device->bootReleasedDate != NULL)
    {
        icLogDebug(OHCM_LOG, "bootReleasedDate : %s",device->bootReleasedDate);
    }
    if(device->rescueVersion != NULL)
    {
        icLogDebug(OHCM_LOG, "rescueVersion : %s",device->rescueVersion);
    }
    if(device->hardwareVersion != NULL)
    {
        icLogDebug(OHCM_LOG, "hardwareVersion : %s",device->hardwareVersion);
    }
    if(device->apiVersion != NULL)
    {
        icLogDebug(OHCM_LOG, "apiVersion : %s",device->apiVersion);
    }
}

/*
 * parse an XML node for device information
 * assumes 'funcArg' is an ohcmDeviceInfo object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmDeviceXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmDeviceInfo *info = (ohcmDeviceInfo *)funcArg;
    if (node == NULL)
    {
        return false;
    }

    if (strcmp((const char *) node->name, DEVICE_NAME_NODE) == 0)
    {
        info->deviceName = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_ID_NODE) == 0)
    {
        info->deviceID = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_MANUFACTURER_NODE) == 0)
    {
        info->manufacturer = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_MODEL_NODE) == 0)
    {
        info->model = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_SERIAL_NUM_NODE) == 0)
    {
        info->serialNumber = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_MAC_ADDR_NODE) == 0)
    {
        info->macAddress = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_FW_VERSION_NODE) == 0)
    {
        info->firmwareVersion = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_FW_RELEASE_DATE_NODE) == 0)
    {
        info->firmwareReleasedDate = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_BOOT_VERSION_NODE) == 0)
    {
        info->bootVersion = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_BOOT_RELEASE_DATE_NODE) == 0)
    {
        info->bootReleasedDate = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_RESCUE_VERSION_NODE) == 0)
    {
        info->rescueVersion = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_HW_VERSION_NODE) == 0)
    {
        info->hardwareVersion = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, DEVICE_API_VERSION_NODE) == 0)
    {
        info->apiVersion = getXmlNodeContentsAsString(node, NULL);
    }

    return true;
}

/*
 * obtain details about the camera
 *
 * @param cam - device to contact
 * @param info - object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmDeviceInfo(ohcmCameraInfo *cam, ohcmDeviceInfo *info, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, DEVICE_INFO_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, DEVICE_INFO_URI);

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
        if (ohcmParseXmlHelper(chunk, parseOhcmDeviceXmlNode, info) == false)
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
 * attempts to ping the camera using OpenHome API (not fork the ping command)
 *
 * @param cam - device to contact
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode isOhcmAlive(ohcmCameraInfo *cam, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, DEVICE_PING_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, DEVICE_PING_URI);

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

    // allow for a longer connect timeout, which we can hit during a heavily loaded CPU
    //
    if (curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L) failed at %s(%d)",__FILE__,__LINE__);
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
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

/*
 * attempts to reboot the camera using OpenHome API
 */
ohcmResultCode rebootOhcmCamera(ohcmCameraInfo *cam, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, DEVICE_REBOOT_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, DEVICE_REBOOT_URI);

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
    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDS, DEVICE_REBOOT_URI) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POSTFIELDS, DEVICE_REBOOT_URI) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(DEVICE_REBOOT_URI)) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(DEVICE_REBOOT_URI)) failed at %s(%d)",__FILE__,__LINE__);
    }

    // perform the 'post' operation.  note we'll use a NULL payload
    // since the data to send is set via POSTFIELDS option
    //
    CURLcode rc = ohcmPerformCurlPost(curl, debugUrl, NULL, chunk, retryCounts);
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
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

/*
 * attempts to reset the camera to factory
 */
ohcmResultCode factoryResetOhcmCamera(ohcmCameraInfo *cam, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, DEVICE_FACTORYRESET_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, DEVICE_FACTORYRESET_URI);

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
    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDS, DEVICE_FACTORYRESET_URI) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POSTFIELDS, DEVICE_FACTORYRESET_URI) failed at %s(%d)",__FILE__,__LINE__);
    }
    if (curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(DEVICE_FACTORYRESET_URI)) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(DEVICE_FACTORYRESET_URI)) failed at %s(%d)",__FILE__,__LINE__);
    }

    // perform the 'post' operation.  note we'll use an empty payload
    // since the data to send is set via POSTFIELDS option
    //
    icFifoBuff *payload = fifoBuffCreate(0); //LG Titan camera expects a payload, even an empty one
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
    destroyOhcmCurlContext(curl);
    fifoBuffDestroy(chunk);
    fifoBuffDestroy(payload);

    // convert to ohcmResultCode
    //
    return ohcmTranslateCurlCode(rc);
}

