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
 * ohcmAudio.c
 *
 * implementation of "audio" functionality as defined in ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <string.h>

#include <openHomeCamera/ohcm.h>
#include <xmlHelper/xmlHelper.h>
#include "ohcmBase.h"
#include "ohcmPrivate.h"

#define AUDIO_CHANNELS_URI      "/OpenHome/System/Audio/channels"
#define AUDIO_ID_NODE           "id"
#define AUDIO_ENABLED_NODE      "enabled"
#define AUDIO_MODE_NODE         "audioMode"
#define AUDIO_MIC_ENAB_NODE     "microphoneEnabled"
#define AUDIO_SET_TOP_NODE      "AudioChannel"

#define AUDIO_MODE_LISTENONLY_VAL       "listenonly"
#define AUDIO_MODE_TALKONLY_VAL         "talkonly"
#define AUDIO_MODE_TALKORLISTEN_VAL     "talkorlisten"
#define AUDIO_MODE_TALKANDLISTEN_VAL    "talkandlisten"


/*
 * helper function to create a blank ohcmAudioChannel object
 */
ohcmAudioChannel *createOhcmAudioChannel()
{
    return (ohcmAudioChannel *)calloc(1, sizeof(ohcmAudioChannel));
}

/*
 * helper function to destroy the ohcmAudioChannel object
 */
void destroyOhcmAudioChannel(ohcmAudioChannel *obj)
{
    if (obj != NULL)
    {
        free(obj->id);
        obj->id = NULL;

        free(obj);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmAudioChannel from a linked list
 */
void destroyOhcmAudioChannelFromList(void *item)
{
    ohcmAudioChannel *channel = (ohcmAudioChannel *)item;
    destroyOhcmAudioChannel(channel);
}

/*
 * parse an XML node for information about a list of audio channels.
 * assumes 'funcArg' is a linkedList (to hold ohcmAudioChannel objects).
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmAudioListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    icLinkedList *list = (icLinkedList *) funcArg;

    // should be a set of "AudioChannel" nodes
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

        if (strcmp((const char *) currNode->name, "AudioChannel") == 0)
        {
            // create a new AudioChannel object, parse it, then add to the list
            //
            ohcmAudioChannel *channel = createOhcmAudioChannel();
            ohcmParseXmlNodeChildren(currNode, parseOhcmAudioXmlNode, channel);
            linkedListAppend(list, channel);
        }
    }

    return true;
}

/*
 * parse an XML node for information about a single audio channel.
 * assumes 'funcArg' is an ohcmAudioChannel object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmAudioXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmAudioChannel *channel = (ohcmAudioChannel *)funcArg;
    if (node == NULL)
    {
        return false;
    }

    // parse the XML and stuff into our object:
    //
    //   char *id;                   // An unique alphanumeric id
    //   bool enabled;               // true if Audio Channels is enabled
    //   ohcmAudioModeType audioMode;// currently only supports LISTENONLY
    //   bool microphoneEnabled;     // true if Microphone is enabled
    //
    if (strcmp((const char *) node->name, AUDIO_ID_NODE) == 0)
    {
        channel->id = getXmlNodeContentsAsString(node, 0);
    }
    else if (strcmp((const char *) node->name, AUDIO_ENABLED_NODE) == 0)
    {
        channel->enabled = getXmlNodeContentsAsBoolean(node, false);
    }
    else if (strcmp((const char *) node->name, AUDIO_MODE_NODE) == 0)
    {
        char *mode = getXmlNodeContentsAsString(node, NULL);
        if (mode != NULL)
        {
            if (strcasecmp(mode, AUDIO_MODE_LISTENONLY_VAL) == 0)
            {
                channel->audioMode = OHCM_AUDIO_LISTENONLY;
            }
            else if (strcasecmp(mode, AUDIO_MODE_TALKONLY_VAL) == 0)
            {
                channel->audioMode = OHCM_AUDIO_TALKONLY;
            }
            else if (strcasecmp(mode, AUDIO_MODE_TALKORLISTEN_VAL) == 0)
            {
                channel->audioMode = OHCM_AUDIO_TALKORLISTEN;
            }
            else if (strcasecmp(mode, AUDIO_MODE_TALKANDLISTEN_VAL) == 0)
            {
                channel->audioMode = OHCM_AUDIO_TALKANDLISTEN;
            }
            free(mode);
        }
    }
    else if (strcmp((const char *) node->name, AUDIO_MIC_ENAB_NODE) == 0)
    {
        channel->microphoneEnabled = getXmlNodeContentsAsBoolean(node, false);
    }

    return true;
}

/*
 * generates XML for the audio 'channel', adding as a child of 'rootNode'
 */
static void appendOhcmAudioChannelXml(xmlNodePtr rootNode, ohcmAudioChannel *channel)
{
    if (channel->id != NULL)
    {
        xmlNewTextChild(rootNode, NULL, BAD_CAST AUDIO_ID_NODE, BAD_CAST channel->id);
    }
    xmlNewTextChild(rootNode, NULL, BAD_CAST AUDIO_ENABLED_NODE, BAD_CAST ((channel->enabled) ? "true" : "false"));
    switch(channel->audioMode)
    {
        case OHCM_AUDIO_LISTENONLY:
            xmlNewTextChild(rootNode, NULL, BAD_CAST AUDIO_MODE_NODE, BAD_CAST AUDIO_MODE_LISTENONLY_VAL);
            break;
        case OHCM_AUDIO_TALKONLY:
            xmlNewTextChild(rootNode, NULL, BAD_CAST AUDIO_MODE_NODE, BAD_CAST AUDIO_MODE_TALKONLY_VAL);
            break;
        case OHCM_AUDIO_TALKORLISTEN:
            xmlNewTextChild(rootNode, NULL, BAD_CAST AUDIO_MODE_NODE, BAD_CAST AUDIO_MODE_TALKORLISTEN_VAL);
            break;
        case OHCM_AUDIO_TALKANDLISTEN:
            xmlNewTextChild(rootNode, NULL, BAD_CAST AUDIO_MODE_NODE, BAD_CAST AUDIO_MODE_TALKANDLISTEN_VAL);
            break;
    }
    xmlNewTextChild(rootNode, NULL, BAD_CAST AUDIO_MIC_ENAB_NODE, BAD_CAST ((channel->microphoneEnabled) ? "true" : "false"));
}

/*
 * generates XML for a set of ohcmAudioChannel objects,
 * adding as a child of 'rootNode'.
 */
void appendOhcmAudioChannelListXml(xmlNodePtr rootNode, icLinkedList *channelList)
{
    // iterate through the list
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(channelList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        ohcmAudioChannel *channel = (ohcmAudioChannel *)linkedListIteratorGetNext(loop);

        // make the node for this AudioChannel
        //
        xmlNodePtr node = xmlNewNode(NULL, BAD_CAST AUDIO_SET_TOP_NODE);
        xmlNewProp(node, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
        xmlAddChild(rootNode, node);
        appendOhcmAudioChannelXml(node, channel);
    }
    linkedListIteratorDestroy(loop);
}

/*
 * debug print the object
 */
void printOhcmAudioChannel(ohcmAudioChannel *channel)
{
    // TODO: print audio object
}

/*
 * query the camera for the current 'audio channel configuration'.
 * if successful, will populate the supplied 'outputList' with ohcmAudioChannel
 * objects (which should be released by caller).
 *
 * @param cam - device to contact
 * @param outputList - linked likst to populate with ohcmAudioChannel objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmAudioChannels(ohcmCameraInfo *cam, icLinkedList *outputList, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, AUDIO_CHANNELS_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, AUDIO_CHANNELS_URI);

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
        if (ohcmParseXmlHelper(chunk, parseOhcmAudioListXmlNode, outputList) == false)
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
 * query the camera for a specific 'audio channel configuration'.
 * if successful, will populate 'target' with details about the channel.
 *
 * @param cam - device to contact
 * @param audioUid - the name of the audio channel to get config for
 * @param target - ohcmAudioChannel object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmAudioChannelByID(ohcmCameraInfo *cam, char *audioUid, ohcmAudioChannel *target, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s/%s", cam->userName, cam->password, cam->cameraIP, AUDIO_CHANNELS_URI, audioUid);
    sprintf(debugUrl,"https://%s%s/%s", cam->cameraIP, AUDIO_CHANNELS_URI, audioUid);

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
        if (ohcmParseXmlHelper(chunk, parseOhcmAudioXmlNode, target) == false)
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
 * apply new 'audio channel configuration' to a camera.
 *
 * @param cam - device to contact
 * @param settings - channel settings to use
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmAudioChannel(ohcmCameraInfo *cam, ohcmAudioChannel *settings, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s/%s", cam->userName, cam->password, cam->cameraIP, AUDIO_CHANNELS_URI, settings->id);
    sprintf(debugUrl,"https://%s%s/%s", cam->cameraIP, AUDIO_CHANNELS_URI, settings->id);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

    // create the payload
    // first, build up the XML doc
    //
    icFifoBuff *payload = fifoBuffCreate(1024);
    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST AUDIO_SET_TOP_NODE);
    xmlNewProp(root, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, root);
    appendOhcmAudioChannelXml(root, settings);

    // convert XML to a string, then cleanup
    //
    ohcmExportXmlToBuffer(doc, payload);
    xmlFreeDoc(doc);

    // create our CURL context
    //
    CURL *curl = createOhcmCurlContext();
    if (curl_easy_setopt(curl, CURLOPT_URL, realUrl) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_URL, realUrl) failed at %s(%d)",__FILE__,__LINE__);
    }
//    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
    if (curl_easy_setopt(curl, CURLOPT_POST, 1) != CURLE_OK)
    {
        icLogError(OHCM_LOG,"curl_easy_setopt(curl, CURLOPT_POST, 1) failed at %s(%d)",__FILE__,__LINE__);
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

    // perform the 'post' operation
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

