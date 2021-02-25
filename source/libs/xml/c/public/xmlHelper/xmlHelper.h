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
 * xmlHelper.h
 *
 * Helper routines to improve the interaction with libxml2.
 *
 * Author: gfaulkner, jelderton
 *-----------------------------------------------*/

#ifndef IC_XMLHELPER_H
#define IC_XMLHELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/*-------------------------------*
 *
 *  XML Node Operations
 *
 *-------------------------------*/

/*
 * read the contents of an XML Node, and interpret as an integer
 */
int32_t getXmlNodeContentsAsInt(xmlNodePtr node, int32_t defValue);

/*
 * read the contents of an XML Node, and interpret as an unsigned integer
 */
uint32_t getXmlNodeContentsAsUnsignedInt(xmlNodePtr node, uint32_t defValue);

/*
 * read the contents of an XML Node, and interpret as an unsigned long long
 */
uint64_t getXmlNodeContentsAsUnsignedLongLong(xmlNodePtr node, uint64_t defValue);

/*
 * read the contents of an XML Node, and interpret as a boolean
 * returns 1 for TRUE, 0 for FALSE (or defValue if node is missing/empty)
 */
bool getXmlNodeContentsAsBoolean(xmlNodePtr node, bool defValue);

/*
 * inverse of 'getAsBoolean' since the value is interpreted more then the others
 */
void setXmlNodeContentsAsBoolean(xmlNodePtr node, bool value);

/*
 * read the contents of an XML Node as a string
 * if the return is not NULL, the caller must free() the memory
 */
char *getXmlNodeContentsAsString(xmlNodePtr node, char *defValue);

/*
 * helper function to create a new node, add it
 * to the 'parentNode', and set the 'contents' (if not empty)
 * ex:
 *   <resend>true</resend>
 *
 * returns the newly created node
 */
xmlNodePtr appendNewStringNode(xmlNodePtr parentNode, const char *nodeName, const char *contents);

/*
 * helper function to find a node named 'search' as a child of 'base'
 * return NULL if not able to be located
 */
xmlNodePtr findChildNode(xmlNodePtr base, const char *search, bool recurse);


/*-------------------------------*
 *
 *  XML Attribute Operations
 *
 *-------------------------------*/

/*
 * read the contents of an attribute, and interpret as an integer
 */
int32_t getXmlNodeAttributeAsInt(xmlNodePtr node, const char *attributeName, int32_t defValue);

/*
 * set an integer attribute to the supplied node
 */
void setXmlNodeAttributeAsInt(xmlNodePtr node, const char *attributeName, int32_t value);

/*
 * read the contents of an attribute, and interpret as an unsigned integer
 */
uint32_t getXmlNodeAttributeAsUnsignedInt(xmlNodePtr node, const char *attributeName, uint32_t defValue);

/*
 * read the contents of an attribute, and interpret as an unsigned long long
 */
uint64_t getXmlNodeAttributeAsUnsignedLongLong(xmlNodePtr node, const char *attributeName, uint64_t defValue);

/*
 * look for and read the contents of an attribute from an XML Node as a boolean
 * returns 1 for TRUE, 0 for FALSE (or defValue if node is missing/empty)
 */
bool getXmlNodeAttributeAsBoolean(xmlNodePtr node, const char *attributeName, bool defValue);

/*
 * inverse of 'getAsBoolean' since the value is interpreted more then the others
 */
void setXmlNodeAttributeAsBoolean(xmlNodePtr node, const char *attributeName, bool value);

/*
 * look for and read the contents of an attribute from an XML Node as a string
 * if the return is not NULL, the caller must free() the memory
 */
char *getXmlNodeAttributeAsString(xmlNodePtr node, const char *attributeName, char *defValue);


#endif // IC_XMLHELPER_H

