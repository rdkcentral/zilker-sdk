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
 * ohcmPrivate.h
 *
 * declare functions within the ohcm codebase
 *
 * Author: jelderton - 12/3/15
 *-----------------------------------------------*/

#ifndef ZILKER_OHCMPRIVATE_H
#define ZILKER_OHCMPRIVATE_H

/*
 * parse an XML node for device information
 * assumes 'funcArg' is an ohcmDeviceInfo object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmDeviceXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * parse an XML node for information about a list of audio channels.
 * assumes 'funcArg' is a linkedList (to hold ohcmAudioChannel objects).
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmAudioListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * parse an XML node for information about a single audio channel.
 * assumes 'funcArg' is an ohcmAudioChannel object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmAudioXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * generates XML for a set of ohcmAudioChannel objects,
 * adding as a child of 'rootNode'.
 */
void appendOhcmAudioChannelListXml(xmlNodePtr rootNode, icLinkedList *channelList);

/*
 * debug print the object
 */
void printOhcmAudioChannel(ohcmAudioChannel *channel);

/*
 * parse an XML node for information about a list of "video inputs"
 * assumes 'funcArg' is a linkedList (to hold ohcmVideoInput objects).
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmVideoInputChannelListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * parse an XML node for information about a "video input"
 * assumes 'funcArg' is a ohcmVideoInput object.
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmVideoInputChannelXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * generates XML for a set of ohcmVideoInput objects,
 * adding as a child of 'rootNode'.
 */
void appendOhcmVideoInputChannelListXml(xmlNodePtr rootNode, icLinkedList *channelList);

/*
 * parse an XML node for a set of 'stream' objects.
 * assumes 'funcArg' is an icLinkedList object.  adheres to
 * ohcmStreamChannel signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmStreamChannelListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * generates XML for a set of ohcmStreamChannel objects,
 * adding as a child of 'rootNode'.
 */
void appendOhcmStreamChannelListXml(xmlNodePtr rootNode, icLinkedList *channelList);

/*
 * parse an XML node for information about a list of motion detection settings.
 * assumes 'funcArg' is a linkedList (to hold ohcmMotionDetection objects).
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmMotionDetectionListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * parse an XML node for information about the wireless status of a network interface.
 * assumes 'funcArg' is an ohcmWirelessStatus object.  adheres to
 * ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmWirelessStatusXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * parse an XML node for information about a single network interfaces on the camera.
 * assumes 'funcArg' is a ohcmNetworkInterface object to contain the parsed data.
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmNetworkXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * parse an XML node for information about the network interfaces on the camera.
 * assumes 'funcArg' is a linkedList (to hold ohcmNetworkInterface objects).
 * adheres to ohcmParseXmlNodeCallback signature so can be used for ohcmParseXmlHelper()
 */
bool parseOhcmNetworkListXmlNode(const xmlChar *top, xmlNodePtr node, void *funcArg);

/*
 * generates XML for a set of ohcmNetworkInterface objects,
 * adding as a child of 'rootNode'.
 */
void appendOhcmNetworkInterfaceListXml(xmlNodePtr rootNode, icLinkedList *netList);

#endif // ZILKER_OHCMPRIVATE_H
