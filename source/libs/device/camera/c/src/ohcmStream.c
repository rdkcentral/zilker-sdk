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
 * ohcmStream.c
 *
 * implementation of "stream" functionality as defined in ohcm.h
 *
 * Author: jelderton (refactored from original one created by karan)
 *-----------------------------------------------*/

#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <xmlHelper/xmlHelper.h>
#include <openHomeCamera/ohcm.h>
#include <inttypes.h>
#include "ohcmBase.h"
#include "ohcmPrivate.h"

#define STREAMING_CHANNELS_URI      "/OpenHome/Streaming/channels"
#define STREAMING_STATUS_URI        "/OpenHome/Streaming/status"

#define STREAMS_ROOT_NODE       "StreamingChannelList"
#define STREAM_TOP_NODE         "StreamingChannel"
#define STREAM_ID_NODE          "id"
#define STREAM_NAME_NODE        "channelName"
#define STREAM_ENABLED_NODE     "enabled"

#define STREAM_TRANSPORT_NODE               "Transport"
#define STREAM_TRANS_RTSP_PORT_NODE         "rtspPortNo"
#define STREAM_TRANS_CONT_LIST_PROT_NODE    "ControlProtocolList"
#define STREAM_TRANS_CONT_NODE              "ControlProtocol"
#define STREAM_TRANS_STREAM_NODE            "streamingTransport"
#define STREAM_TRANS_UNICAST_NODE           "Unicast"
#define STREAM_TRANS_MULTICAST_NODE         "Multicast"
#define STREAM_TRANS_DEST_IP_ADDR_NODE      "destIPAddress"
#define STREAM_TRANS_VIDEO_DEST_PORT_NODE   "videoDestPortNo"
#define STREAM_TRANS_AUDIO_DEST_PORT_NODE   "audioDestPortNo"
#define STREAM_TRANS_TTL_NODE               "ttl"
#define STREAM_TRANS_SECURITY_NODE          "Security"

#define STREAM_VIDEO_NODE                   "Video"
#define STREAM_VIDEO_IN_CHAN_IDNODE         "videoInputChannelID"
#define STREAM_VIDEO_CODEC_TYPE_NODE        "videoCodecType"
#define STREAM_VIDEO_H264_CODEC_NODE        "h.264"
#define STREAM_VIDEO_MPEG4_CODEC_NODE       "mpeg4"
#define STREAM_VIDEO_MJPEG_CODEC_NODE       "mjpeg"
#define STREAM_VIDEO_CODEC_PROFILE_NODE     "profile"
#define STREAM_VIDEO_CODEC_LEVEL_NODE       "level"
#define STREAM_VIDEO_SCAN_TYPE_NODE         "videoScanType"
#define STREAM_VIDEO_RESO_W_NODE            "videoResolutionWidth"
#define STREAM_VIDEO_RESO_H_NODE            "videoResolutionHeight"
#define STREAM_VIDEO_CONTROL_NODE           "videoQualityControlType"
#define STREAM_VIDEO_FIXED_QUAL_NODE        "fixedQuality"
#define STREAM_VIDEO_VBR_MIN_NODE           "vbrLowerCap"
#define STREAM_VIDEO_VBR_MAX_NODE           "vbrUpperCap"
#define STREAM_VIDEO_CONSTANT_BIT_RATE_NODE "constantBitRate"
#define STREAM_VIDEO_MAX_FRAME_RATE_NODE    "maxFrameRate"
#define STREAM_VIDEO_KEY_FRAME_INTERV_NODE  "keyFrameInterval"
#define STREAM_VIDEO_MIRROR_ENABLED_NODE    "mirrorEnabled"
#define STREAM_VIDEO_SNAPSHOT_TYPE_NODE     "snapShotImageType"

#define STREAM_AUDIO_NODE               "Audio"
#define STREAM_AUDIO_IN_CHAN_ID_NODE    "audioInputChannelID"
#define STREAM_AUDIO_COMP_TYPE_NODE     "audioCompressionType"

#define STREAM_MEDIA_CAP_NODE           "MediaCapture"
#define STREAM_MEDIA_CAP_PRE_NODE       "preCaptureLength"
#define STREAM_MEDIA_CAP_POST_NODE      "postCaptureLength"

#define VIDEO_UPLOAD_TOP_NODE           "MediaUpload"
#define VIDEO_CLIP_FORMAT_TYPE_NODE     "videoClipFormatType"
#define VIDEO_UPLOAD_SHOULD_BLOCK_NODE  "blockUploadComplete"
#define VIDEO_UPLOAD_GATEWAY_URL_NODE   "gatewayUrl"
#define VIDEO_UPLOAD_EVENT_URL_NODE     "eventUrl"

#define VIDEO_CHANNEL_LIST_NODE         "VideoInputChannelList"
#define VIDEO_CHANNEL_NODE              "VideoInputChannel"
#define VIDEO_POWER_FREQ_NODE           "powerLineFrequencyMode"
#define VIDEO_WHITE_BAL_NODE            "whiteBalanceMode"
#define VIDEO_BRIGHTNESS_LEVEL_NODE     "brightnessLevel"
#define VIDEO_CONTRAST_LEVEL_NODE       "contrastLevel"
#define VIDEO_SHARPNESS_LEVEL_NODE      "sharpnessLevel"
#define VIDEO_SATURATION_LEVEL_NODE     "saturationLevel"
#define VIDEO_DAYNIGHT_FILTER_NODE      "DayNightFilter"
#define VIDEO_DAYNIGHT_FILTER_TYPE_NODE "dayNightFilterType"
#define VIDEO_MIRROR_ENAB_NODE          "mirrorEnabled"

// private functions
static void parseStreamChannelTransportNode(xmlNodePtr node, ohcmStreamChannel *channel);
static void parseStreamChannelAudioNode(xmlNodePtr node, ohcmStreamChannel *channel);
static void parseStreamChannelVideoNode(xmlNodePtr node, ohcmStreamChannel *channel);
static void parseStreamChannelMediaCapNode(xmlNodePtr node, ohcmStreamChannel *channel);
static void parseStreamCapabilitiesVideoNode(xmlNodePtr node, ohcmVideoStreamCapabilities* caps);
static void parseStreamCapabilitiesAudioNode(xmlNodePtr node, ohcmAudioStreamCapabilities* caps);
static void parseStreamCapabilitiesMediaNode(xmlNodePtr node, ohcmMediaStreamCapabilities* caps);

/*
 * helper function to create a blank ohcmStreamChannel object
 */
ohcmStreamChannel *createOhcmStreamChannel()
{
    return (ohcmStreamChannel *)calloc(1, sizeof(ohcmStreamChannel));
}

/*
 * helper function to destroy the ohcmStreamAccount object
 */
void destroyOhcmStreamChannel(ohcmStreamChannel *obj)
{
    if (obj != NULL)
    {
        free(obj->id);
        obj->id = NULL;
        free(obj->name);
        obj->name = NULL;
        free(obj->streamingTransport);
        obj->streamingTransport = NULL;
        free(obj->destIPAddress);
        obj->destIPAddress = NULL;
        free(obj->videoInputChannelID);
        obj->videoInputChannelID = NULL;
        free(obj->h264Profile);
        obj->h264Profile = NULL;
        free(obj->h264Level);
        obj->h264Level = NULL;
        free(obj->mpeg4Profile);
        obj->mpeg4Profile = NULL;
        free(obj->mjpegProfile);
        obj->mjpegProfile = NULL;
        free(obj->videoScanType);
        obj->videoScanType = NULL;
        free(obj->videoQualityControlType);
        obj->videoQualityControlType = NULL;
        free(obj->snapShotImageType);
        obj->snapShotImageType = NULL;
        free(obj->audioInputChannelID);
        obj->audioInputChannelID = NULL;
        free(obj->audioCompressionType);
        obj->audioCompressionType = NULL;

        free(obj);
    }
}

/*
 * 'linkedListItemFreeFunc' implementation for destroying the
 * ohcmStreamChannel from a linked list
 */
void destroyOhcmStreamChannelFromList(void *item)
{
    ohcmStreamChannel *channel = (ohcmStreamChannel *)item;
    destroyOhcmStreamChannel(channel);
}

/*
 * parse an XML node for a single 'stream' object.
 * assumes 'funcArg' is an ohcmDeviceInfo object.  adheres to
 * ohcmStreamChannel signature so can be used for ohcmParseXmlHelper()
 */
static bool parseStreamChannelXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmStreamChannel *channel = (ohcmStreamChannel *) funcArg;

    /* parse the individual rows of the "StreamingChannel" node.  the doc
     * we're parsing should look similar to:
     *
     *     <StreamingChannel version="1.0">
     *        <id>0</id>
     *        <channelName>rtsp channel 1</channelName>
     *        <enabled>true</enabled>
     *        <Transport>
     *            <rtspPortNo>554</rtspPortNo>
     *            <ControlProtocolList>
     *                <ControlProtocol>
     *                    <streamingTransport>HTTP,RTSP</streamingTransport>
     *                </ControlProtocol>
     *            </ControlProtocolList>
     *            <Unicast>
     *                <enabled>true</enabled>
     *            </Unicast>
     *            <Multicast>
     *                <enabled>false</enabled>
     *                <destIPAddress>224.2.0.1</destIPAddress>
     *                <videoDestPortNo>2240</videoDestPortNo>
     *                <audioDestPortNo>2242</audioDestPortNo>
     *                <ttl>16</ttl>
     *            </Multicast>
     *            <Security>
     *                <enabled>false</enabled>
     *            </Security>
     *        </Transport>
     *        <Video>
     *            <enabled>true</enabled>
     *            <videoInputChannelID>0</videoInputChannelID>
     *            <videoCodecType>
     *                <h.264>
     *                    <profile>main</profile>
     *                    <level>3.1</level>
     *                </h.264>
     *            </videoCodecType>
     *            <videoScanType>interlaced</videoScanType>
     *            <videoResolutionWidth>1280</videoResolutionWidth>
     *            <videoResolutionHeight>720</videoResolutionHeight>
     *            <videoQualityControlType>VBR</videoQualityControlType>
     *            <fixedQuality>60</fixedQuality>
     *            <maxFrameRate>30</maxFrameRate>
     *            <keyFrameInterval>30</keyFrameInterval>
     *            <mirrorEnabled>false</mirrorEnabled>
     *            <snapShotImageType>JPEG</snapShotImageType>
     *        </Video>
     *        <Audio>
     *            <enabled>true</enabled>
     *            <audioInputChannelID>0</audioInputChannelID>
     *            <audioCompressionType>G.711ulaw</audioCompressionType>
     *        </Audio>
     *        <MediaCapture>
     *            <preCaptureLength>5000</preCaptureLength>
     *            <postCaptureLength>10000</postCaptureLength>
     *        </MediaCapture>
     *    </StreamingChannel>
     */

    if (strcmp((const char *) node->name, STREAM_ID_NODE) == 0)
    {
        channel->id = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, STREAM_NAME_NODE) == 0)
    {
        channel->name = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, STREAM_ENABLED_NODE) == 0)
    {
        channel->enabled = getXmlNodeContentsAsBoolean(node, false);
    }
    else if (strcmp((const char *) node->name, STREAM_TRANSPORT_NODE) == 0)
    {
        // transport section
        parseStreamChannelTransportNode(node, channel);
    }
    else if (strcmp((const char *) node->name, STREAM_AUDIO_NODE) == 0)
    {
        // audio section
        parseStreamChannelAudioNode(node, channel);
    }
    else if (strcmp((const char *) node->name, STREAM_VIDEO_NODE) == 0)
    {
        // video section
        parseStreamChannelVideoNode(node, channel);
    }
    else if (strcmp((const char *) node->name, STREAM_MEDIA_CAP_NODE) == 0)
    {
        // media capture section
        parseStreamChannelMediaCapNode(node, channel);
    }

    return true;
}

/*
 * parse the STREAM_TRANSPORT_NODE from a "StreamingChannel" section of the XML doc
 */
static void parseStreamChannelTransportNode(xmlNodePtr node, ohcmStreamChannel *channel)
{
    /* parse XML section similar to:
     *        <Transport>
     *            <rtspPortNo>554</rtspPortNo>
     *            <ControlProtocolList>
     *                <ControlProtocol>
     *                    <streamingTransport>HTTP,RTSP</streamingTransport>
     *                </ControlProtocol>
     *            </ControlProtocolList>
     *            <Unicast>
     *                <enabled>true</enabled>
     *            </Unicast>
     *            <Multicast>
     *                <enabled>false</enabled>
     *                <destIPAddress>224.2.0.1</destIPAddress>
     *                <videoDestPortNo>2240</videoDestPortNo>
     *                <audioDestPortNo>2242</audioDestPortNo>
     *                <ttl>16</ttl>
     *            </Multicast>
     *            <Security>
     *                <enabled>false</enabled>
     *            </Security>
     *        </Transport>
     */
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

        if (strcmp((const char *) currNode->name, STREAM_TRANS_RTSP_PORT_NODE) == 0)
        {
            channel->rtspPortNo = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_TRANS_CONT_LIST_PROT_NODE) == 0)
        {
            if (currNode->children != NULL && currNode->children->children != NULL &&
                strcmp((const char *) currNode->children->children->name, STREAM_TRANS_STREAM_NODE) == 0)
            {
                // <streamingTransport>HTTP,RTSP</streamingTransport>
                channel->streamingTransport = getXmlNodeContentsAsString(currNode->children->children, NULL);
            }
        }
        else if (strcmp((const char *) currNode->name, STREAM_TRANS_UNICAST_NODE) == 0)
        {
            if (currNode->children != NULL &&
                strcmp((const char *) currNode->children->name, STREAM_ENABLED_NODE) == 0)
            {
                channel->unicastEnabled = getXmlNodeContentsAsBoolean(currNode->children, false);
            }
        }
        else if (strcmp((const char *) currNode->name, STREAM_TRANS_MULTICAST_NODE) == 0)
        {
            /*
             * <Multicast>
             *    <enabled>false</enabled>
             *    <destIPAddress>224.2.0.1</destIPAddress>
             *    <videoDestPortNo>2240</videoDestPortNo>
             *    <audioDestPortNo>2242</audioDestPortNo>
             *    <ttl>16</ttl>
             * </Multicast>
             */

            xmlNodePtr innerNode = NULL;
            xmlNodePtr innerLoop = currNode->children;
            for (innerNode = innerLoop; innerNode != NULL; innerNode = innerNode->next)
            {
                // skip comments, blanks, etc
                //
                if (innerNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                if (strcmp((const char *) innerNode->name, STREAM_ENABLED_NODE) == 0)
                {
                    channel->multicastEnabled = getXmlNodeContentsAsBoolean(innerNode, false);
                }
                else if (strcmp((const char *) innerNode->name, STREAM_TRANS_DEST_IP_ADDR_NODE) == 0)
                {
                    channel->destIPAddress = getXmlNodeContentsAsString(innerNode, NULL);
                }
                else if (strcmp((const char *) innerNode->name, STREAM_TRANS_VIDEO_DEST_PORT_NODE) == 0)
                {
                    channel->videoDestPortNo = getXmlNodeContentsAsUnsignedInt(innerNode, 0);
                }
                else if (strcmp((const char *) innerNode->name, STREAM_TRANS_AUDIO_DEST_PORT_NODE) == 0)
                {
                    channel->audioDestPortNo = getXmlNodeContentsAsUnsignedInt(innerNode, 0);
                }
                else if (strcmp((const char *) innerNode->name, STREAM_TRANS_TTL_NODE) == 0)
                {
                    channel->ttl = getXmlNodeContentsAsUnsignedInt(innerNode, 0);
                }
            }
        }
        else if (strcmp((const char *) currNode->name, STREAM_TRANS_SECURITY_NODE) == 0)
        {
            if (currNode->children != NULL &&
                strcmp((const char *) currNode->children->name, STREAM_ENABLED_NODE) == 0)
            {
                channel->securityEnabled = getXmlNodeContentsAsBoolean(currNode->children, false);
            }
        }
    }
}

/*
 * parse the STREAM_AUDIO_NODE from a "StreamingChannel" section of the XML doc
 */
static void parseStreamChannelAudioNode(xmlNodePtr node, ohcmStreamChannel *channel)
{
    /* parse XML section similar to:
     *        <Audio>
     *            <enabled>true</enabled>
     *            <audioInputChannelID>0</audioInputChannelID>
     *            <audioCompressionType>G.711ulaw</audioCompressionType>
     *        </Audio>
     */
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

        if (strcmp((const char *) currNode->name, STREAM_ENABLED_NODE) == 0)
        {
            channel->audioEnabled = getXmlNodeContentsAsBoolean(currNode, false);
        }
        else if (strcmp((const char *) currNode->name, STREAM_AUDIO_IN_CHAN_ID_NODE) == 0)
        {
            channel->audioInputChannelID = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, STREAM_AUDIO_COMP_TYPE_NODE) == 0)
        {
            channel->audioCompressionType = getXmlNodeContentsAsString(currNode, NULL);
        }
    }
}

/*
 * parse the STREAM_VIDEO_NODE from a "StreamingChannel" section of the XML doc
 */
static void parseStreamChannelVideoNode(xmlNodePtr node, ohcmStreamChannel *channel)
{
    /* parse XML section similar to:
     *        <Video>
     *            <enabled>true</enabled>
     *            <videoInputChannelID>0</videoInputChannelID>
     *            <videoCodecType>
     *                <h.264>
     *                    <profile>main</profile>
     *                    <level>3.1</level>
     *                </h.264>
     *            </videoCodecType>
     *            <videoScanType>interlaced</videoScanType>
     *            <videoResolutionWidth>1280</videoResolutionWidth>
     *            <videoResolutionHeight>720</videoResolutionHeight>
     *            <videoQualityControlType>VBR</videoQualityControlType>
     *            <fixedQuality>60</fixedQuality>
     *            <maxFrameRate>30</maxFrameRate>
     *            <keyFrameInterval>30</keyFrameInterval>
     *            <mirrorEnabled>false</mirrorEnabled>
     *            <snapShotImageType>JPEG</snapShotImageType>
     *        </Video>
     */
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

        if (strcmp((const char *) currNode->name, STREAM_ENABLED_NODE) == 0)
        {
            channel->videoEnabled = getXmlNodeContentsAsBoolean(currNode, false);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_IN_CHAN_IDNODE) == 0)
        {
            channel->videoInputChannelID = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_CODEC_TYPE_NODE) == 0)
        {
            // kinda sucks, but looking for 3 different profiles:
            //    h.264 (profile & level)
            //    mpeg4 (profile)
            //    mjpeg (profile)
            //
            xmlNodePtr innerNode = NULL;
            xmlNodePtr innerLoop = currNode->children;
            for (innerNode = innerLoop; innerNode != NULL; innerNode = innerNode->next)
            {
                // skip comments, blanks, etc
                //
                if (innerNode->type != XML_ELEMENT_NODE)
                {
                    continue;
                }

                if (strcmp((const char *) innerNode->name, STREAM_VIDEO_H264_CODEC_NODE) == 0)
                {
                    /*
                     * <h.264>
                     *    <profile>main</profile>
                     *    <level>3.1</level>
                     * </h.264>
                     */
                    xmlNodePtr h264Node = NULL;
                    xmlNodePtr h264Loop = innerNode->children;
                    for (h264Node = h264Loop; h264Node != NULL; h264Node = h264Node->next)
                    {
                        if (strcmp((const char *) h264Node->name, STREAM_VIDEO_CODEC_PROFILE_NODE) == 0)
                        {
                            channel->h264Profile = getXmlNodeContentsAsString(h264Node, NULL);
                        }
                        else if (strcmp((const char *) h264Node->name, STREAM_VIDEO_CODEC_LEVEL_NODE) == 0)
                        {
                            channel->h264Level = getXmlNodeContentsAsString(h264Node, NULL);
                        }
                    }
                }
                else if (strcmp((const char *) innerNode->name, STREAM_VIDEO_MPEG4_CODEC_NODE) == 0)
                {
                    /*
                     * <mpeg4>
                     *    <profile>simple</profile>
                     * </mpeg4>
                     */
                    if (innerNode->children != NULL && strcmp((const char *)innerNode->children->name, STREAM_VIDEO_CODEC_PROFILE_NODE) == 0)
                    {
                        channel->mpeg4Profile = getXmlNodeContentsAsString(innerNode->children, NULL);
                    }
                }
                else if (strcmp((const char *) innerNode->name, STREAM_VIDEO_MJPEG_CODEC_NODE) == 0)
                {
                    /*
                     * <mjpeg>
                     *    <profile>simple</profile>
                     * </mjpeg>
                     */
                    if (innerNode->children != NULL && strcmp((const char *)innerNode->children->name, STREAM_VIDEO_CODEC_PROFILE_NODE) == 0)
                    {
                        channel->mjpegProfile = getXmlNodeContentsAsString(innerNode->children, NULL);
                    }
                }
            }
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_SCAN_TYPE_NODE) == 0)
        {
            channel->videoScanType = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_RESO_W_NODE) == 0)
        {
            channel->videoResolutionWidth = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_RESO_H_NODE) == 0)
        {
            channel->videoResolutionHeight = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_CONTROL_NODE) == 0)
        {
            channel->videoQualityControlType = getXmlNodeContentsAsString(currNode, NULL);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_FIXED_QUAL_NODE) == 0)
        {
            channel->fixedQuality = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_VBR_MIN_NODE) == 0)
        {
            channel->vbrMinRate = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_VBR_MAX_NODE) == 0)
        {
            channel->vbrMaxRate = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_MAX_FRAME_RATE_NODE) == 0)
        {
            channel->maxFrameRate = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_KEY_FRAME_INTERV_NODE) == 0)
        {
            channel->keyFrameInterval = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_MIRROR_ENABLED_NODE) == 0)
        {
            channel->mirrorEnabled = getXmlNodeContentsAsBoolean(currNode, false);
        }
        else if (strcmp((const char *) currNode->name, STREAM_VIDEO_SNAPSHOT_TYPE_NODE) == 0)
        {
            channel->snapShotImageType = getXmlNodeContentsAsString(currNode, NULL);
        }
    }
}

/*
 * parse the STREAM_MEDIA_CAP_NODE from a "StreamingChannel" section of the XML doc
 */
static void parseStreamChannelMediaCapNode(xmlNodePtr node, ohcmStreamChannel *channel)
{
    /* parse XML section similar to:
     *   <MediaCapture>
     *       <preCaptureLength>5000</preCaptureLength>
     *       <postCaptureLength>10000</postCaptureLength>
     *   </MediaCapture>
     */
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

        if (strcmp((const char *) currNode->name, STREAM_MEDIA_CAP_PRE_NODE) == 0)
        {
            channel->preCaptureLength = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
        else if (strcmp((const char *) currNode->name, STREAM_MEDIA_CAP_POST_NODE) == 0)
        {
            channel->postCaptureLength = getXmlNodeContentsAsUnsignedInt(currNode, 0);
        }
    }
}

/*
 * parse an XML node for a set of 'stream' objects.
 * assumes 'funcArg' is an icLinkedList object.  adheres to
 * ohcmStreamChannel signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmStreamChannelListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    icLinkedList *list = (icLinkedList *) funcArg;

    // the doc should be a list of 'StreamingChannel' nodes and look
    // similar to:
    //
    //  <StreamingChannelList version="1.0">
    //     <StreamingChannel version="1.0">
    //        ...bunch-of-stuff-here....
    //     </StreamingChannel>
    //     <StreamingChannel version="1.0">
    //        ...bunch-of-stuff-here....
    //     </StreamingChannel>
    //  </StreamingChannelList>
    //
    if (strcmp((const char *) node->name, STREAM_TOP_NODE) == 0)
    {
        // create a new ohcmStreamChannel object, then populate it
        // with the children of this node
        //
        ohcmStreamChannel *channel = createOhcmStreamChannel();
        linkedListAppend(list, channel);

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

            // forward to 'parseStreamChannelXmlNode'
            //
            parseStreamChannelXmlNode(node->name, currNode, channel);
        }
    }

    return true;
}

/*
 * generate <Transport> XML for a single ohcmStreamChannel object
 */
static void appendOhcmStreamChannelTransportXml(xmlNodePtr rootNode, ohcmStreamChannel *channel)
{
    /*
     * generate XML similar to:
     *  <Transport>
     *      <rtspPortNo>554</rtspPortNo>
     *      <ControlProtocolList>
     *          <ControlProtocol>
     *              <streamingTransport>HTTP,RTSP</streamingTransport>
     *          </ControlProtocol>
     *      </ControlProtocolList>
     *      <Unicast>
     *          <enabled>true</enabled>
     *      </Unicast>
     *      <Multicast>
     *          <enabled>false</enabled>
     *          <destIPAddress>224.2.0.1</destIPAddress>
     *          <videoDestPortNo>2240</videoDestPortNo>
     *          <audioDestPortNo>2242</audioDestPortNo>
     *          <ttl>16</ttl>
     *      </Multicast>
     *      <Security>
     *          <enabled>false</enabled>
     *      </Security>
     *  </Transport>
     */

    // first, the top-level wrapper node
    //
    char tmp[256];
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST STREAM_TRANSPORT_NODE);
    xmlAddChild(rootNode, node);

    sprintf(tmp, "%"PRIu32, channel->rtspPortNo);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_TRANS_RTSP_PORT_NODE, BAD_CAST tmp);

    // <ControlProtocolList>
    if (channel->streamingTransport != NULL)
    {
        xmlNodePtr protoListNode = xmlNewNode(NULL, BAD_CAST STREAM_TRANS_CONT_LIST_PROT_NODE);
        xmlAddChild(node, protoListNode);
        xmlNodePtr protoNode = xmlNewNode(NULL, BAD_CAST STREAM_TRANS_CONT_NODE);
        xmlAddChild(protoListNode, protoNode);
        xmlNewTextChild(protoNode, NULL, BAD_CAST STREAM_TRANS_STREAM_NODE, BAD_CAST channel->streamingTransport);
    }

    // <Unicast>
    xmlNodePtr unicastNode = xmlNewNode(NULL, BAD_CAST STREAM_TRANS_UNICAST_NODE);
    xmlAddChild(node, unicastNode);
    xmlNewTextChild(unicastNode, NULL, BAD_CAST STREAM_ENABLED_NODE, BAD_CAST ((channel->unicastEnabled) ? "true" : "false"));

    // <Multicast>
    xmlNodePtr multicastNode = xmlNewNode(NULL, BAD_CAST STREAM_TRANS_MULTICAST_NODE);
    xmlAddChild(node, multicastNode);
    xmlNewTextChild(multicastNode, NULL, BAD_CAST STREAM_ENABLED_NODE, BAD_CAST ((channel->multicastEnabled) ? "true" : "false"));
    if (channel->destIPAddress != NULL)
    {
        xmlNewTextChild(multicastNode, NULL, BAD_CAST STREAM_TRANS_DEST_IP_ADDR_NODE, BAD_CAST channel->destIPAddress);
    }
    sprintf(tmp, "%"PRIu32, channel->videoDestPortNo);
    xmlNewTextChild(multicastNode, NULL, BAD_CAST STREAM_TRANS_VIDEO_DEST_PORT_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->audioDestPortNo);
    xmlNewTextChild(multicastNode, NULL, BAD_CAST STREAM_TRANS_AUDIO_DEST_PORT_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->ttl);
    xmlNewTextChild(multicastNode, NULL, BAD_CAST STREAM_TRANS_TTL_NODE, BAD_CAST tmp);

    // <Security>
    xmlNodePtr securityNode = xmlNewNode(NULL, BAD_CAST STREAM_TRANS_SECURITY_NODE);
    xmlAddChild(node, securityNode);
    xmlNewTextChild(securityNode, NULL, BAD_CAST STREAM_ENABLED_NODE, BAD_CAST ((channel->securityEnabled) ? "true" : "false"));
}

/*
 * generate <Video> XML for a single ohcmStreamChannel object
 */
static void appendOhcmStreamChannelVideoXml(xmlNodePtr rootNode, ohcmStreamChannel *channel)
{
    /*
     * generate XML similar to:
     *  <Video>
     *      <enabled>true</enabled>
     *      <videoInputChannelID>0</videoInputChannelID>
     *      <videoCodecType>
     *          <h.264>
     *              <profile>main</profile>
     *              <level>3.1</level>
     *          </h.264>
     *      </videoCodecType>
     *      <videoScanType>interlaced</videoScanType>
     *      <videoResolutionWidth>1280</videoResolutionWidth>
     *      <videoResolutionHeight>720</videoResolutionHeight>
     *      <videoQualityControlType>VBR</videoQualityControlType>
     *      <fixedQuality>60</fixedQuality>
     *      <maxFrameRate>30</maxFrameRate>
     *      <keyFrameInterval>30</keyFrameInterval>
     *      <mirrorEnabled>false</mirrorEnabled>
     *      <snapShotImageType>JPEG</snapShotImageType>
     *  </Video>
     */

    // first, the top-level wrapper node
    //
    char tmp[256];
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST STREAM_VIDEO_NODE);
    xmlAddChild(rootNode, node);

    // basic values
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_ENABLED_NODE, BAD_CAST ((channel->videoEnabled) ? "true" : "false"));
    if (channel->videoInputChannelID != NULL)
    {
        xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_IN_CHAN_IDNODE, BAD_CAST channel->videoInputChannelID);
    }

    // <videoCodecType>
    // will contain one of 3 different profiles:
    //    h.264 (profile & level)
    //    mpeg4 (profile)
    //    mjpeg (profile)
    //
    xmlNodePtr codecTypeNode = xmlNewNode(NULL, BAD_CAST STREAM_VIDEO_CODEC_TYPE_NODE);
    xmlAddChild(node, codecTypeNode);
    if (channel->h264Profile != NULL && channel->h264Level != NULL)
    {
        xmlNodePtr profileNode = xmlNewNode(NULL, BAD_CAST STREAM_VIDEO_H264_CODEC_NODE);
        xmlAddChild(codecTypeNode, profileNode);
        xmlNewTextChild(profileNode, NULL, BAD_CAST STREAM_VIDEO_CODEC_PROFILE_NODE, BAD_CAST channel->h264Profile);
        xmlNewTextChild(profileNode, NULL, BAD_CAST STREAM_VIDEO_CODEC_LEVEL_NODE, BAD_CAST channel->h264Level);
    }
    else if (channel->mpeg4Profile != NULL)
    {
        xmlNodePtr profileNode = xmlNewNode(NULL, BAD_CAST STREAM_VIDEO_MPEG4_CODEC_NODE);
        xmlAddChild(codecTypeNode, profileNode);
        xmlNewTextChild(profileNode, NULL, BAD_CAST STREAM_VIDEO_CODEC_PROFILE_NODE, BAD_CAST channel->mpeg4Profile);

    }
    else //if (channel->mjpegProfile != NULL)
    {
        xmlNodePtr profileNode = xmlNewNode(NULL, BAD_CAST STREAM_VIDEO_MJPEG_CODEC_NODE);
        xmlAddChild(codecTypeNode, profileNode);
        if (channel->mjpegProfile != NULL)
        {
            xmlNewTextChild(profileNode, NULL, BAD_CAST STREAM_VIDEO_CODEC_PROFILE_NODE, BAD_CAST channel->mjpegProfile);
        }
        else
        {
            // add empty char to the profile or else we end up with a dead node:
            //   <mjpeg/>
            //
            xmlNodeSetContent(profileNode, BAD_CAST " ");
        }
    }

    // misc video settings
    if (channel->videoScanType != NULL)
    {
        xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_SCAN_TYPE_NODE, BAD_CAST channel->videoScanType);
    }
    sprintf(tmp, "%"PRIu32, channel->videoResolutionWidth);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_RESO_W_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->videoResolutionHeight);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_RESO_H_NODE, BAD_CAST tmp);
    if (channel->videoQualityControlType != NULL)
    {
        xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_CONTROL_NODE, BAD_CAST channel->videoQualityControlType);
    }
    sprintf(tmp, "%"PRIu32, channel->fixedQuality);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_FIXED_QUAL_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->vbrMinRate);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_VBR_MIN_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->vbrMaxRate);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_VBR_MAX_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->constantBitRate);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_CONSTANT_BIT_RATE_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->maxFrameRate);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_MAX_FRAME_RATE_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->keyFrameInterval);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_KEY_FRAME_INTERV_NODE, BAD_CAST tmp);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_MIRROR_ENABLED_NODE, BAD_CAST ((channel->mirrorEnabled) ? "true" : "false"));
    if (channel->snapShotImageType != NULL)
    {
        xmlNewTextChild(node, NULL, BAD_CAST STREAM_VIDEO_SNAPSHOT_TYPE_NODE, BAD_CAST channel->snapShotImageType);
    }
}

/*
 * generate <Audio> XML for a single ohcmStreamChannel object
 */
static void appendOhcmStreamChannelAudioXml(xmlNodePtr rootNode, ohcmStreamChannel *channel)
{
    /*
     * generate XML similar to:
     *  <Audio>
     *      <enabled>true</enabled>
     *      <audioInputChannelID>0</audioInputChannelID>
     *      <audioCompressionType>G.711ulaw</audioCompressionType>
     *  </Audio>
     */

    // first, the top-level wrapper node
    //
    char tmp[256];
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST STREAM_AUDIO_NODE);
    xmlAddChild(rootNode, node);

    // basic values
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_ENABLED_NODE, BAD_CAST ((channel->audioEnabled) ? "true" : "false"));
    if (channel->audioInputChannelID != NULL)
    {
        xmlNewTextChild(node, NULL, BAD_CAST STREAM_AUDIO_IN_CHAN_ID_NODE, BAD_CAST channel->audioInputChannelID);
    }
    if (channel->audioCompressionType != NULL)
    {
        xmlNewTextChild(node, NULL, BAD_CAST STREAM_AUDIO_COMP_TYPE_NODE, BAD_CAST channel->audioCompressionType);
    }
}

/*
 * generate <MediaCapture> XML for a single ohcmStreamChannel object
 */
static void appendOhcmStreamChannelMediaCaptureXml(xmlNodePtr rootNode, ohcmStreamChannel *channel)
{
    // only apply if the pre/post lengths are valid
    //
    if (channel->postCaptureLength == 0 && channel->preCaptureLength == 0)
    {
        return;
    }

    /*
     * generate XML similar to:
     *  <MediaCapture>
     *      <preCaptureLength>5000</preCaptureLength>
     *      <postCaptureLength>10000</postCaptureLength>
     *  </MediaCapture>
     */

    // first, the top-level wrapper node
    //
    char tmp[256];
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST STREAM_MEDIA_CAP_NODE);
    xmlAddChild(rootNode, node);

    // basic values
    sprintf(tmp, "%"PRIu32, channel->preCaptureLength);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_MEDIA_CAP_PRE_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, channel->postCaptureLength);
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_MEDIA_CAP_POST_NODE, BAD_CAST tmp);
}

/*
 * generate <StreamingChannel> XML for a single ohcmStreamChannel object
 */
static void appendOhcmStreamChannelXml(xmlNodePtr rootNode, ohcmStreamChannel *channel)
{
    // needs 5 pieces:
    //  1.  base info (id, name, enabled)
    //  2.  Transport section
    //  3.  Video section
    //  4.  Audio section
    //  5.  MediaCapture section
    //

    // first, the top-level wrapper node
    //
    xmlNodePtr node = xmlNewNode(NULL, BAD_CAST STREAM_TOP_NODE);
    xmlNewProp(node, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(rootNode, node);

    //  1.  base info (id, name, enabled)
    if (channel->id != NULL)
    {
        xmlNewTextChild(node, NULL, BAD_CAST STREAM_ID_NODE, BAD_CAST channel->id);
    }
    if (channel->name != NULL)
    {
        xmlNewTextChild(node, NULL, BAD_CAST STREAM_NAME_NODE, BAD_CAST channel->name);
    }
    xmlNewTextChild(node, NULL, BAD_CAST STREAM_ENABLED_NODE, BAD_CAST ((channel->enabled) ? "true" : "false"));

    //  2.  Transport section
    appendOhcmStreamChannelTransportXml(node, channel);

    //  3.  Video section
    appendOhcmStreamChannelVideoXml(node, channel);

    //  4.  Audio section
    appendOhcmStreamChannelAudioXml(node, channel);

    //  5.  MediaCapture section
    appendOhcmStreamChannelMediaCaptureXml(node, channel);
}

/*
 * generates XML for a set of ohcmStreamChannel objects,
 * adding as a child of 'rootNode'.
 */
void appendOhcmStreamChannelListXml(xmlNodePtr rootNode, icLinkedList *channelList)
{
    // add each ohcmStreamChannel from the list
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(channelList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        // make the node for this ohcmStreamChannel
        //
        ohcmStreamChannel *currChannel = (ohcmStreamChannel *)linkedListIteratorGetNext(loop);
        appendOhcmStreamChannelXml(rootNode, currChannel);
    }
    linkedListIteratorDestroy(loop);
}

/*
 * query the camera for the current 'streaming channel configuration'.
 * if successful, will populate the supplied 'outputList' with ohcmStreamChannel
 * objects (which should be released by caller).
 *
 * @param cam - device to contact
 * @param outputList - linked likst to populate with ohcmStreamChannel objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmStreamingChannels(ohcmCameraInfo *cam, icLinkedList *outputList, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, STREAMING_CHANNELS_URI);
    sprintf(debugUrl,"https://%s%s", cam->cameraIP, STREAMING_CHANNELS_URI);

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

        // success with the 'get', so parse the result (list of channels)
        //
        if (ohcmParseXmlHelper(chunk, parseOhcmStreamChannelListXmlNode, outputList) == false)
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
 * query the camera for a specific 'streaming channel configuration'.
 * if successful, will populate 'target' with details about the channel.
 *
 * @param cam - device to contact
 * @param streamUid - the name of the stream to get config for
 * @param target - ohcmStreamChannel object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmStreamingChannelByID(ohcmCameraInfo *cam, char *streamUid, ohcmStreamChannel *target, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s/%s", cam->userName, cam->password, cam->cameraIP, STREAMING_CHANNELS_URI, streamUid);
    sprintf(debugUrl,"https://%s%s/%s", cam->cameraIP, STREAMING_CHANNELS_URI, streamUid);

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
        if (ohcmParseXmlHelper(chunk, parseStreamChannelXmlNode, target) == false)
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
 * apply new 'streaming channel configuration' to a camera.
 *
 * @param cam - device to contact
 * @param inputList - linked likst of ohcmStreamChannel objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmStreamingChannels(ohcmCameraInfo *cam, icLinkedList *inputList, uint32_t retryCounts)
{
    // build up the URL to hit
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl, "https://%s:%s@%s%s", cam->userName, cam->password, cam->cameraIP, STREAMING_CHANNELS_URI);
    sprintf(debugUrl, "https://%s%s", cam->cameraIP, STREAMING_CHANNELS_URI);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

    // create the payload.  first, build up the XML doc
    //
    icFifoBuff *payload = fifoBuffCreate(4096);
    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST STREAMS_ROOT_NODE);
    xmlNewProp(root, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, root);

    // add each stream from the list
    //
    appendOhcmStreamChannelListXml(root, inputList);

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
            if (rc == CURLE_OK)
            {
                icLogDebug(OHCM_LOG, "setOhcmStreamingChannels was SUCCESSFUL");
            }
            else if (rc == CURLE_LDAP_CANNOT_BIND)
            {
                icLogDebug(OHCM_LOG, "setOhcmStreamingChannels success, responded with 'Needs Reboot'");
            }
            else if (result.statusMessage != NULL)
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
 * helper function to create a blank ohcmVideoInput object
 */
ohcmVideoInput *createOhcmVideoInput()
{
    return (ohcmVideoInput *)calloc(1, sizeof(ohcmVideoInput));
}

/*
 * helper function to destroy the ohcmVideoInput object
 */
void destroyOhcmVideoInput(ohcmVideoInput *obj)
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
 * ohcmVideoInput from a linked list
 */
void destroyOhcmVideoInputFromList(void *item)
{
    ohcmVideoInput *video = (ohcmVideoInput *)item;
    destroyOhcmVideoInput(video);
}

/*
 * parse an XML node for information about a list of "video inputs"
 * assumes 'funcArg' is a linkedList (to hold ohcmVideoInput objects).
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmVideoInputChannelListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    /*
     * parse XML similar to:
     *   <VideoInputChannelList version="1.0">
     *      <VideoInputChannel version="1.0">
     *          ....
     *      </VideoInputChannel>
     *  </VideoInputChannelList>
     */
    icLinkedList *list = (icLinkedList *)funcArg;

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

        if (strcmp((const char *) currNode->name, VIDEO_CHANNEL_NODE) == 0)
        {
            // parse the 'video channel' node then add to our list
            //
            ohcmVideoInput *video = createOhcmVideoInput();
            ohcmParseXmlNodeChildren(currNode, parseOhcmVideoInputChannelXmlNode, video);
            linkedListAppend(list, video);
        }
    }

    return true;
}

/*
 * parse an XML node for information about a "video input"
 * assumes 'funcArg' is a ohcmVideoInput object.
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmVideoInputChannelXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg)
{
    ohcmVideoInput *video = (ohcmVideoInput *)funcArg;

    /*
     * parse XML similar to:
     *   <VideoInputChannel version="1.0">
     *       <id>0</id>
     *       <powerLineFrequencyMode>60hz</powerLineFrequencyMode>
     *       <whiteBalanceMode>auto</whiteBalanceMode>
     *       <brightnessLevel>4</brightnessLevel>
     *       <contrastLevel>4</contrastLevel>
     *       <sharpnessLevel>4</sharpnessLevel>
     *       <saturationLevel>4</saturationLevel>
     *       <DayNightFilter>
     *           <dayNightFilterType>auto</dayNightFilterType>
     *       </DayNightFilter>
     *      <mirrorEnabled>false</mirrorEnabled>
     * </VideoInputChannel>
     */
    if (strcmp((const char *) node->name, STREAM_ID_NODE) == 0)
    {
        video->id = getXmlNodeContentsAsString(node, NULL);
    }
    else if (strcmp((const char *) node->name, VIDEO_POWER_FREQ_NODE) == 0)
    {
        char *tmp = getXmlNodeContentsAsString(node, NULL);
        if (tmp != NULL && strcmp(tmp, "50hz") == 0)
        {
            video->powerLineFrequencyMode = OHCM_POWERLINE_FREQ_50hz;
        }
        else if (tmp != NULL && strcmp(tmp, "60hz") == 0)
        {
            video->powerLineFrequencyMode = OHCM_POWERLINE_FREQ_60hz;
        }
        free(tmp);
    }
    else if (strcmp((const char *) node->name, VIDEO_WHITE_BAL_NODE) == 0)
    {
        char *tmp = getXmlNodeContentsAsString(node, NULL);
        if (tmp != NULL && strcmp(tmp, "auto") == 0)
        {
            video->whiteBalanceMode = OHCM_WHITEBALANCE_AUTO;
        }
        else
        {
            video->whiteBalanceMode = OHCM_WHITEBALANCE_MANUAL;
        }
        free(tmp);
    }
    else if (strcmp((const char *) node->name, VIDEO_BRIGHTNESS_LEVEL_NODE) == 0)
    {
        video->brightnessLevel = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *) node->name, VIDEO_CONTRAST_LEVEL_NODE) == 0)
    {
        video->contrastLevel = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *) node->name, VIDEO_SHARPNESS_LEVEL_NODE) == 0)
    {
        video->sharpnessLevel = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *) node->name, VIDEO_SATURATION_LEVEL_NODE) == 0)
    {
        video->saturationLevel = getXmlNodeContentsAsUnsignedInt(node, 0);
    }
    else if (strcmp((const char *) node->name, VIDEO_DAYNIGHT_FILTER_NODE) == 0)
    {
        if (node->children != NULL && strcmp((const char *) node->children->name, VIDEO_DAYNIGHT_FILTER_TYPE_NODE) == 0)
        {
            // <dayNightFilterType>auto</dayNightFilterType>
            char *tmp = getXmlNodeContentsAsString(node, NULL);
            if (tmp != NULL)
            {
                if (strcmp(tmp, "auto") == 0)
                {
                    video->dayNightFilterType = OHCM_DAYNIGHT_FILTER_AUTO;
                }
                else if (strcmp(tmp, "day") == 0)
                {
                    video->dayNightFilterType = OHCM_DAYNIGHT_FILTER_DAY;
                }
                else if (strcmp(tmp, "night") == 0)
                {
                    video->dayNightFilterType = OHCM_DAYNIGHT_FILTER_NIGHT;
                }
            }
            free(tmp);
        }
    }
    else if (strcmp((const char *) node->name, VIDEO_MIRROR_ENAB_NODE) == 0)
    {
        video->mirrorEnabled = getXmlNodeContentsAsBoolean(node, false);
    }

    return true;
}

/*
 * generates XML for the 'video input', adding as a child of 'rootNode'
 */
static void appendOhcmVideoInputXml(xmlNodePtr rootNode, ohcmVideoInput *video)
{
    /*
     * generate XML similar to:
     *   <VideoInputChannel version="1.0">
     *       <id>0</id>
     *       <powerLineFrequencyMode>60hz</powerLineFrequencyMode>
     *       <whiteBalanceMode>auto</whiteBalanceMode>
     *       <brightnessLevel>4</brightnessLevel>
     *       <contrastLevel>4</contrastLevel>
     *       <sharpnessLevel>4</sharpnessLevel>
     *       <saturationLevel>4</saturationLevel>
     *       <DayNightFilter>
     *           <dayNightFilterType>auto</dayNightFilterType>
     *       </DayNightFilter>
     *      <mirrorEnabled>false</mirrorEnabled>
     * </VideoInputChannel>
     */
    char tmp[128];
    xmlNodePtr channelNode = xmlNewNode(NULL, BAD_CAST VIDEO_CHANNEL_LIST_NODE);
    xmlNewProp(channelNode, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlAddChild(rootNode, channelNode);

    if (video->id != NULL)
    {
        xmlNewTextChild(channelNode, NULL, BAD_CAST STREAM_ID_NODE, BAD_CAST video->id);
    }
    switch (video->powerLineFrequencyMode)
    {
        case OHCM_POWERLINE_FREQ_50hz:
            xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_POWER_FREQ_NODE, BAD_CAST "50hz");
            break;
        case OHCM_POWERLINE_FREQ_60hz:
            xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_POWER_FREQ_NODE, BAD_CAST "60hz");
            break;
    }
    if (video->whiteBalanceMode == OHCM_WHITEBALANCE_AUTO)
    {
        xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_WHITE_BAL_NODE, BAD_CAST "auto");
    }
    else
    {
        xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_WHITE_BAL_NODE, BAD_CAST "manual");
    }

    sprintf(tmp, "%"PRIu32, video->brightnessLevel);
    xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_BRIGHTNESS_LEVEL_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, video->contrastLevel);
    xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_CONTRAST_LEVEL_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, video->sharpnessLevel);
    xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_SHARPNESS_LEVEL_NODE, BAD_CAST tmp);
    sprintf(tmp, "%"PRIu32, video->saturationLevel);
    xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_SATURATION_LEVEL_NODE, BAD_CAST tmp);

    xmlNodePtr filterNode = xmlNewNode(NULL, BAD_CAST VIDEO_DAYNIGHT_FILTER_NODE);
    xmlAddChild(channelNode, filterNode);
    switch (video->dayNightFilterType)
    {
        case OHCM_DAYNIGHT_FILTER_AUTO:
            xmlNewTextChild(filterNode, NULL, BAD_CAST VIDEO_DAYNIGHT_FILTER_TYPE_NODE, BAD_CAST "auto");
            break;
        case OHCM_DAYNIGHT_FILTER_DAY:
            xmlNewTextChild(filterNode, NULL, BAD_CAST VIDEO_DAYNIGHT_FILTER_TYPE_NODE, BAD_CAST "day");
            break;
        case OHCM_DAYNIGHT_FILTER_NIGHT:
            xmlNewTextChild(filterNode, NULL, BAD_CAST VIDEO_DAYNIGHT_FILTER_TYPE_NODE, BAD_CAST "night");
            break;
    }

    xmlNewTextChild(channelNode, NULL, BAD_CAST VIDEO_MIRROR_ENAB_NODE, BAD_CAST ((video->mirrorEnabled) ? "true" : "false"));
}

/*
 * generates XML for a set of ohcmVideoInput objects,
 * adding as a child of 'rootNode'.
 */
void appendOhcmVideoInputChannelListXml(xmlNodePtr rootNode, icLinkedList *channelList)
{
    // iterate through the list
    //
    icLinkedListIterator *loop = linkedListIteratorCreate(channelList);
    while (linkedListIteratorHasNext(loop) == true)
    {
        ohcmVideoInput *video = (ohcmVideoInput *)linkedListIteratorGetNext(loop);

        // make the node for this VideoInputChannelList
        //
        xmlNodePtr node = xmlNewNode(NULL, BAD_CAST VIDEO_CHANNEL_LIST_NODE);
        xmlNewProp(node, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
        xmlAddChild(rootNode, node);
        appendOhcmVideoInputXml(node, video);
    }
    linkedListIteratorDestroy(loop);
}

/*
 * query the camera for the current 'video input configuration'.
 * if successful, will populate the supplied 'outputList' with ohcmVideoInput
 * objects (which should be released by caller).
 *
 * @param cam - device to contact
 * @param outputList - linked likst to populate with ohcmVideoInput objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmVideoInputs(ohcmCameraInfo *cam, icLinkedList *outputList, uint32_t retryCounts)
{
    // TODO: getOhcmVideoInputs

    return OHCM_GENERAL_FAIL;
}

/*
 * query the camera for a specific 'video input configuration'.
 * if successful, will populate 'target' with details about the input.
 *
 * @param cam - device to contact
 * @param videoUid - the name of the video input to get config for
 * @param target - ohcmVideoInput object to populate
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode getOhcmVideoInputByID(ohcmCameraInfo *cam, char *videoUid, ohcmVideoInput *target, uint32_t retryCounts)
{
    // TODO: getOhcmVideoInputByID

    return OHCM_GENERAL_FAIL;
}

/*
 * apply new 'video input configuration' to a camera.
 *
 * @param cam - device to contact
 * @param inputList - linked likst of ohcmVideoInput objects
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode setOhcmVideoInputs(ohcmCameraInfo *cam, icLinkedList *inputList, uint32_t retryCounts)
{
    // TODO: setOhcmVideoInputs

    return OHCM_GENERAL_FAIL;
}

/*
 * helper function to create a blank ohcmUploadVideo object
 */
ohcmUploadVideo *createOhcmUploadVideo()
{
    return (ohcmUploadVideo *)calloc(1, sizeof(ohcmUploadVideo));
}

/*
 * helper function to destroy the ohcmUploadVideo object
 */
void destroyOhcmUploadVideo(ohcmUploadVideo *obj)
{
    if (obj != NULL)
    {
        free(obj->id);
        obj->id = NULL;
        free(obj->eventUrl);
        obj->eventUrl = NULL;
        free(obj->gatewayUrl);
        obj->gatewayUrl = NULL;

        free(obj);
    }
}

/*
 * query the camera to upload a video clip.
 *
 * @param cam - device to contact
 * @param details - hints about the clip to upload (where, format, etc)
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode uploadOhcmVideo(ohcmCameraInfo *cam, ohcmUploadVideo *details, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s/%s/video/upload", cam->userName, cam->password, cam->cameraIP, STREAMING_CHANNELS_URI, details->id);
    sprintf(debugUrl,"https://%s%s/%s/video/upload", cam->cameraIP, STREAMING_CHANNELS_URI, details->id);

    // create the output buffer
    //
    icFifoBuff *chunk = fifoBuffCreate(1024);

    // create the payload
    // first, build up the XML doc
    //
    icFifoBuff *payload = fifoBuffCreate(1024);
    xmlDocPtr doc = xmlNewDoc(BAD_CAST OHCM_XML_VERSION);
    xmlNodePtr root = xmlNewNode(NULL, BAD_CAST VIDEO_UPLOAD_TOP_NODE);
    xmlNewProp(root, BAD_CAST OHCM_XML_VERSION_ATTRIB, BAD_CAST OHCM_XML_VERSION);
    xmlDocSetRootElement(doc, root);
    xmlNewTextChild(root, NULL, BAD_CAST STREAM_ID_NODE, BAD_CAST details->id);
    xmlNewTextChild(root, NULL, BAD_CAST STREAM_VIDEO_SNAPSHOT_TYPE_NODE, BAD_CAST "JPEG"); // only thing supported
    switch (details->videoClipFormatType)
    {
        case OHCM_VIDEO_FORMAT_MP4:
            xmlNewTextChild(root, NULL, BAD_CAST VIDEO_CLIP_FORMAT_TYPE_NODE, BAD_CAST "MP4");
            break;
        case OHCM_VIDEO_FORMAT_FLV:
            xmlNewTextChild(root, NULL, BAD_CAST VIDEO_CLIP_FORMAT_TYPE_NODE, BAD_CAST "FLV");
            break;
    }
    xmlNewTextChild(root, NULL, BAD_CAST VIDEO_UPLOAD_SHOULD_BLOCK_NODE, BAD_CAST ((details->blockUploadComplete) ? "true" : "false"));
    if (details->gatewayUrl != NULL)
    {
        xmlNewTextChild(root, NULL, BAD_CAST VIDEO_UPLOAD_GATEWAY_URL_NODE, BAD_CAST details->gatewayUrl);
    }
    if (details->eventUrl != NULL)
    {
        xmlNewTextChild(root, NULL, BAD_CAST VIDEO_UPLOAD_EVENT_URL_NODE, BAD_CAST details->eventUrl);
    }

    // convert XML to a string, then cleanup
    //
    ohcmExportXmlToBuffer(doc, payload);
    xmlFreeDoc(doc);
    uint32_t payLen = fifoBuffGetPullAvailable(payload);

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
 * query the camera to upload a video clip.
 *
 * @param cam - device to contact
 * @param videoUid - UID of the video channel to use for the picture
 * @param outputFilename - local file to save the picture to
 * @param retryCount - number of retry attempts before giving up
 */
ohcmResultCode downloadOhcmPicture(ohcmCameraInfo *cam, const char *videoUid, const char *outputFilename, uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s/%s/picture", cam->userName, cam->password, cam->cameraIP, STREAMING_CHANNELS_URI, videoUid);
    sprintf(debugUrl,"https://%s%s/%s/picture", cam->cameraIP, STREAMING_CHANNELS_URI, videoUid);

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
        // success with the 'get', so save the results into 'outputFilename'
        //
        FILE *fp = fopen(outputFilename, "wb");
        if (fp != NULL)
        {
            uint32_t len = fifoBuffGetPullAvailable(chunk);
            void *ptr = fifoBuffPullPointer(chunk, len);
            fwrite(ptr, sizeof(char), len, fp);
            fclose(fp);
        }
        else
        {
            icLogWarn(OHCM_LOG, "unable to create output file %s - %s", outputFilename, strerror(errno));
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


static void freeListString(void* item)
{
    free(item);
}

static ohcmVideoStreamCapabilities* createVideoCapabilites()
{
    ohcmVideoStreamCapabilities* caps = (ohcmVideoStreamCapabilities*) calloc(1, sizeof(ohcmVideoStreamCapabilities));

    if (caps) {
        caps->h264Profiles = linkedListCreate();
        caps->h264Levels = linkedListCreate();
        caps->mpeg4Profiles = linkedListCreate();
        caps->scanTypes = linkedListCreate();
        caps->qualityTypes = linkedListCreate();
        caps->snapshotTypes = linkedListCreate();
    }

    return caps;
}

static ohcmAudioStreamCapabilities* createAudioCapabilities()
{
    ohcmAudioStreamCapabilities* caps = (ohcmAudioStreamCapabilities*) calloc(1, sizeof(ohcmAudioStreamCapabilities));

    if (caps) {
        caps->compressionTypes = linkedListCreate();
    }

    return caps;
}

static ohcmMediaStreamCapabilities* createMediaCapabilities()
{
    return (ohcmMediaStreamCapabilities*) calloc(1, sizeof(ohcmMediaStreamCapabilities));
}

static void tokenizeString2List(char* str, icLinkedList* list, const char* delim)
{
    if (str) {
        char *token, *saveptr;

        for (token = strtok_r(str, delim, &saveptr);
             token != NULL;
             token = strtok_r(NULL, delim, &saveptr)) {
            linkedListAppend(list, strdup(token));
        }
    }
}

#define CAPS_TRANSPORT_ELEMENT "streamingTransport"

static void parseStreamCapabilitiesProtocolNode(xmlNodePtr node, ohcmStreamCapabilities* caps)
{
    xmlNodePtr _node;

    for (_node = node->children; _node != NULL; _node = _node->next) {
        if (_node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) _node->name, CAPS_TRANSPORT_ELEMENT) == 0) {
                char* e = getXmlNodeAttributeAsString(_node, "opt", NULL);

                tokenizeString2List(e, caps->streamingTransports, ",");

                if (e) free(e);
            }
        }
    }
}

#define CAPS_VIDEO_CODEC_NODE "videoCodecType"
#define CAPS_VIDEO_H264_NODE "h.264"
#define CAPS_VIDEO_MPEG4_NODE "mpeg4"
#define CAPS_VIDEO_MJPEG_NODE "mjpeg"

#define CAPS_VIDEO_CHANNEL_ELEMENT "videoInputChannelID"
#define CAPS_VIDEO_SCANTYPE_ELEMENT "videoScanType"
#define CAPS_VIDEO_WIDTH_ELEMENT "videoResolutionWidth"
#define CAPS_VIDEO_HEIGHT_ELEMENT "videoResolutionHeight"
#define CAPS_VIDEO_QUALITY_ELEMENT "videoQualityControlType"
#define CAPS_VIDEO_CBR_ELEMENT "constantBitRate"
#define CAPS_VIDEO_FRAMERATE_ELEMENT "maxFrameRate"
#define CAPS_VIDEO_SNAPSHOT_ELEMENT "snapShotImageType"
#define CAPS_VIDEO_PROFILE_ELEMENT "profile"
#define CAPS_VIDEO_LEVEL_ELEMENT "level"

static void parseStreamCapabilitiesCodecNode(xmlNodePtr node, ohcmVideoStreamCapabilities* caps)
{
    xmlNodePtr _node;

    for (_node = node->children; _node != NULL; _node = _node->next) {
        if (_node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) _node->name, CAPS_VIDEO_H264_NODE) == 0) {
                xmlNodePtr __node;

                for (__node = _node->children; __node != NULL; __node = __node->next) {
                    if (__node->type == XML_ELEMENT_NODE) {
                        if (strcmp((const char*) __node->name, CAPS_VIDEO_PROFILE_ELEMENT) == 0) {
                            char* e = getXmlNodeAttributeAsString(__node, "opt", NULL);

                            tokenizeString2List(e, caps->h264Profiles, ",");

                            if (e) free(e);
                        } else if (strcmp((const char*) __node->name, CAPS_VIDEO_LEVEL_ELEMENT) == 0) {
                            char* e = getXmlNodeAttributeAsString(__node, "opt", NULL);

                            tokenizeString2List(e, caps->h264Levels, ",");

                            if (e) free(e);
                        }
                    }
                }
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_MPEG4_NODE) == 0) {
                xmlNodePtr __node;

                for (__node = _node->children; __node != NULL; __node = __node->next) {
                    if (__node->type == XML_ELEMENT_NODE) {
                        if (strcmp((const char*) __node->name, CAPS_VIDEO_PROFILE_ELEMENT) == 0) {
                            char* e = getXmlNodeAttributeAsString(__node, "opt", NULL);

                            tokenizeString2List(e, caps->mpeg4Profiles, ",");

                            if (e) free(e);
                        }
                    }
                }
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_MJPEG_NODE) == 0) {
                caps->supportsMJPEG = true;
            }
        }
    }
}

static void parseStreamCapabilitiesVideoNode(xmlNodePtr node, ohcmVideoStreamCapabilities* caps)
{
    xmlNodePtr _node;

    for (_node = node->children; _node != NULL; _node = _node->next) {
        if (_node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) _node->name, CAPS_VIDEO_CHANNEL_ELEMENT) == 0) {
                caps->inputChannelID = getXmlNodeContentsAsInt(_node, 0);
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_CODEC_NODE) == 0) {
                parseStreamCapabilitiesCodecNode(_node, caps);
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_SCANTYPE_ELEMENT) == 0) {
                char* e = getXmlNodeAttributeAsString(_node, "opt", NULL);

                tokenizeString2List(e, caps->scanTypes, ",");

                if (e) free(e);
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_WIDTH_ELEMENT) == 0) {
                caps->maxWidth = getXmlNodeAttributeAsInt(_node, "max", 0);
                caps->minWidth = getXmlNodeAttributeAsInt(_node, "min", 0);
                caps->widthRange = getXmlNodeAttributeAsString(_node, "range", NULL);
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_HEIGHT_ELEMENT) == 0) {
                caps->maxHeight = getXmlNodeAttributeAsInt(_node, "max", 0);
                caps->minHeight = getXmlNodeAttributeAsInt(_node, "min", 0);
                caps->heightRange = getXmlNodeAttributeAsString(_node, "range", NULL);
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_QUALITY_ELEMENT) == 0) {
                char *e = getXmlNodeAttributeAsString(_node, "opt", NULL);
                if (e != NULL) {
                    // legacy Java code looked for 'all'
                    if (strstr(e, "all") != NULL) {
                        // add both VBR and CBR to the list
                        linkedListAppend(caps->qualityTypes, strdup("CBR"));
                        linkedListAppend(caps->qualityTypes, strdup("VBR"));
                    } else {
                        // get each one listed
                        tokenizeString2List(e, caps->qualityTypes, ",");
                    }
                    free(e);
                }
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_CBR_ELEMENT) == 0) {
                caps->maxCBR = getXmlNodeAttributeAsInt(_node, "max", 0);
                caps->minCBR = getXmlNodeAttributeAsInt(_node, "min", 0);
                caps->cbrRange = getXmlNodeAttributeAsString(_node, "range", NULL);
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_FRAMERATE_ELEMENT) == 0) {
                caps->maxFramerate = getXmlNodeAttributeAsInt(_node, "max", 0);
                caps->minFramerate = getXmlNodeAttributeAsInt(_node, "min", 0);
                caps->framerateRange = getXmlNodeAttributeAsString(_node, "range", NULL);
            } else if (strcmp((const char*) _node->name, CAPS_VIDEO_SNAPSHOT_ELEMENT) == 0) {
                char* e = getXmlNodeAttributeAsString(_node, "opt", NULL);

                tokenizeString2List(e, caps->snapshotTypes, ",");

                if (e) free(e);
            }
        }
    }
}

#define CAPS_AUDIO_CHANNEL_ELEMENT "audioInputChannelID"
#define CAPS_AUDIO_COMPRESSION_ELEMENT "audioCompressionType"
#define CAPS_AUDIO_BITRATE_ELEMENT "audioBitRate"

static void parseStreamCapabilitiesAudioNode(xmlNodePtr node, ohcmAudioStreamCapabilities* caps)
{
    xmlNodePtr _node;

    for (_node = node->children; _node != NULL; _node = _node->next) {
        if (_node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) _node->name, CAPS_AUDIO_CHANNEL_ELEMENT) == 0) {
                caps->inputChannelID = getXmlNodeContentsAsInt(_node, 0);
            } else if (strcmp((const char*) _node->name, CAPS_AUDIO_BITRATE_ELEMENT) == 0) {
                caps->maxBitrate = getXmlNodeAttributeAsInt(_node, "max", 0);
                caps->minBitrate = getXmlNodeAttributeAsInt(_node, "min", 0);
                caps->bitrateRange = getXmlNodeAttributeAsString(_node, "range", NULL);
            } else if (strcmp((const char*) _node->name, CAPS_AUDIO_COMPRESSION_ELEMENT) == 0) {
                char* e = getXmlNodeAttributeAsString(_node, "opt", NULL);

                tokenizeString2List(e, caps->compressionTypes, ",");

                if (e) free(e);
            }
        }
    }
}

#define CAPS_MEDIA_PRE_ELEMENT "preCaptureLength"
#define CAPS_MEDIA_POST_ELEMENT "postCaptureLength"

static void parseStreamCapabilitiesMediaNode(xmlNodePtr node, ohcmMediaStreamCapabilities* caps)
{
    xmlNodePtr _node;

    for (_node = node->children; _node != NULL; _node = _node->next) {
        if (_node->type == XML_ELEMENT_NODE) {
            if (strcmp((const char*) _node->name, CAPS_MEDIA_PRE_ELEMENT) == 0) {
                caps->maxPre = getXmlNodeAttributeAsInt(_node, "max", 0);
                caps->minPre = getXmlNodeAttributeAsInt(_node, "min", 0);
                caps->preRange = getXmlNodeAttributeAsString(_node, "range", NULL);
            } else if (strcmp((const char*) _node->name, CAPS_MEDIA_POST_ELEMENT) == 0) {
                caps->maxPost = getXmlNodeAttributeAsInt(_node, "max", 0);
                caps->minPost = getXmlNodeAttributeAsInt(_node, "min", 0);
                caps->postRange = getXmlNodeAttributeAsString(_node, "range", NULL);
            }
        }
    }
}

#define CAPS_PROTOCOL_LIST_NODE "ControlProtocolList"
#define CAPS_PROTOCOL_NODE "ControlProtocol"
#define CAPS_VIDEO_NODE "Video"
#define CAPS_AUDIO_NODE "Audio"
#define CAPS_MEDIA_NODE "MediaCapture"

#define CAPS_CHANNEL_ID_ELEMENT "id"
#define CAPS_NAME_ELEMENT "name"

static bool parseStreamCapabilitiesNode(const xmlChar *topNodeName, xmlNodePtr node, void *funcArg)
{
    ohcmStreamCapabilities* caps = (ohcmStreamCapabilities*) funcArg;

    if (strcmp((const char*) topNodeName, "StreamingCapabilities") != 0) return false;

    if (strcmp((const char*) node->name, CAPS_CHANNEL_ID_ELEMENT) == 0) {
        caps->id = getXmlNodeContentsAsString(node, NULL);
    } else if (strcmp((const char*) node->name, CAPS_NAME_ELEMENT) == 0) {
        caps->name = getXmlNodeContentsAsString(node, NULL);
    } else if (strcmp((const char*) node->name, CAPS_PROTOCOL_LIST_NODE) == 0) {
        xmlNodePtr _node;

        for (_node = node->children; _node != NULL; _node = _node->next) {
            if (_node->type == XML_ELEMENT_NODE) {
                if (strcmp((const char*) _node->name, CAPS_PROTOCOL_NODE) == 0) {
                    parseStreamCapabilitiesProtocolNode(_node, caps);
                }
            }
        }
    } else if (strcmp((const char*) node->name, CAPS_VIDEO_NODE) == 0) {
        caps->videoCapabilities = createVideoCapabilites();
        parseStreamCapabilitiesVideoNode(node, caps->videoCapabilities);
    } else if (strcmp((const char*) node->name, CAPS_AUDIO_NODE) == 0) {
        caps->audioCapabilities = createAudioCapabilities();
        parseStreamCapabilitiesAudioNode(node, caps->audioCapabilities);
    } else if (strcmp((const char*) node->name, CAPS_MEDIA_NODE) == 0) {
        caps->mediaCapabilities = createMediaCapabilities();
        parseStreamCapabilitiesMediaNode(node, caps->mediaCapabilities);
    }

    return true;
}

ohcmStreamCapabilities* createOhcmStreamCapabilities()
{
    ohcmStreamCapabilities* caps = (ohcmStreamCapabilities*) calloc(1, sizeof(ohcmStreamCapabilities));

    if (caps) {
        caps->streamingTransports = linkedListCreate();
    }

    return caps;
}

static void destroyVideoCapabilites(ohcmVideoStreamCapabilities* caps)
{
    if (caps) {
        if (caps->widthRange) free(caps->widthRange);
        if (caps->heightRange) free(caps->heightRange);

        linkedListDestroy(caps->h264Profiles, freeListString);
        linkedListDestroy(caps->h264Levels, freeListString);
        linkedListDestroy(caps->mpeg4Profiles, freeListString);
        linkedListDestroy(caps->scanTypes, freeListString);
        linkedListDestroy(caps->qualityTypes, freeListString);
        linkedListDestroy(caps->snapshotTypes, freeListString);

        free(caps);
    }
}

static void destroyAudioCapabilites(ohcmAudioStreamCapabilities* caps)
{
    if (caps) {
        if (caps->bitrateRange) free(caps->bitrateRange);

        linkedListDestroy(caps->compressionTypes, freeListString);

        free(caps);
    }
}

static void destroyMediaCapabilites(ohcmMediaStreamCapabilities* caps)
{
    if (caps != NULL) {
        free(caps->preRange);
        free(caps->postRange);
        free(caps);
    }
}

void destroyOhcmStreamCapabilites(ohcmStreamCapabilities* obj)
{
    if (obj) {
        if (obj->id) free(obj->id);
        if (obj->name) free(obj->name);

        linkedListDestroy(obj->streamingTransports, freeListString);

        destroyVideoCapabilites(obj->videoCapabilities);
        destroyAudioCapabilites(obj->audioCapabilities);
        destroyMediaCapabilites(obj->mediaCapabilities);

        free(obj);
    }
}

ohcmResultCode getOhcmStreamCapabilities(ohcmCameraInfo *cam,
                                         const char* id,
                                         ohcmStreamCapabilities* obj,
                                         uint32_t retryCounts)
{
    // build up the URL to hit for this device
    //
    char realUrl[MAX_URL_LENGTH];
    char debugUrl[MAX_URL_LENGTH];
    sprintf(realUrl,"https://%s:%s@%s%s/%s/capabilities", cam->userName, cam->password, cam->cameraIP, STREAMING_CHANNELS_URI, id);
    sprintf(debugUrl,"https://%s%s/%s/capabilities", cam->cameraIP, STREAMING_CHANNELS_URI, id);

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
        if (ohcmParseXmlHelper(chunk, parseStreamCapabilitiesNode, obj) == false)
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

bool isOhcmValueInRange(int minValue, int maxValue, OptRange range, int value)
{
    if (value < minValue) return false;
    if (value > maxValue) return false;

    if (range) {
        OptRange _range = strdup(range);
        char *token, *saveptr;

        for (token = strtok_r(_range, ",", &saveptr);
             token != NULL;
             token = strtok_r(NULL, ",", &saveptr)) {
            char* hyphen = strchr(token, '-');

            if (hyphen) {
                long lower, upper;

                *hyphen = '\0';

                lower = strtol(token, NULL, 10);
                upper = strtol(hyphen + 1, NULL, 10);

                if ((value >= lower) && (value <= upper)) {
                    free(_range);
                    return true;
                }
            } else if (strtol(token, NULL, 10) == value) {
                free(_range);
                return true;
            }
        }

        free(_range);

        return false;
    }

    return true;
}

static bool linkedListComparator(void* lhs, void* rhs)
{
    return (strcmp(lhs, rhs) == 0);
}

bool ohcmContainsCapability(icLinkedList* list, const char* item)
{
    return (linkedListFind(list, (char*) item, linkedListComparator) != NULL);
}

