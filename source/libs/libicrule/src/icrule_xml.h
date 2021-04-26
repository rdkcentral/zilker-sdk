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
// Created by Boyd, Weston on 4/27/18.
//

#ifndef ZILKER_ICRULE_XML_H
#define ZILKER_ICRULE_XML_H

#include <stdint.h>

#include <libxml2/libxml/tree.h>

/** Helper to acquire an unsigned 64-bit value from XML.
 *
 * @param node The XML node to read the value/prop from.
 * @param name The XML property name to read, or NULL if reading
 * an XML value.
 * @param defvalue The default value to supply if no property/value
 * was read.
 * @return The unsigned 64-bit value.
 */
uint64_t icrule_get_xml_uint64(xmlNodePtr node, const char* name, uint64_t defvalue);

/** Helper to acquire a signed 64-bit value from XML.
 *
 * @param node The XML node to read the value/prop from.
 * @param name The XML property name to read, or NULL if reading
 * an XML value.
 * @param defvalue The default value to supply if no property/value
 * was read.
 * @return The signed 64-bit value.
 */
int64_t icrule_get_xml_int64(xmlNodePtr node, const char* name, int64_t defvalue);

/** Helper to acquire a signed 32-bit value from XML.
 *
 * @param node The XML node to read the value/prop from.
 * @param name The XML property name to read, or NULL if reading
 * an XML value.
 * @param defvalue The default value to supply if no property/value
 * was read.
 * @return The signed 32-bit value.
 */
int icrule_get_xml_int(xmlNodePtr node, const char* name, int defvalue);

/** Helper to acquire a string value from XML.
 *
 * The caller is responsible for freeing memory allocated for
 * the string.
 *
 * @param node The XML node to read the value/prop from.
 * @param name The XML property name to read, or NULL if reading
 * an XML value.
 * @param defvalue The default value to supply if no property/value
 * was read.
 * @return The string value. The caller is responsible for freeing
 * memory allocated for the string.
 */
char* icrule_get_xml_string(xmlNodePtr node, const char* name, const char* defvalue);

/** Helper to acquire a boolean value from XML.
 *
 * @param node The XML node to read the value/prop from.
 * @param name The XML property name to read, or NULL if reading
 * an XML value.
 * @param defvalue The default value to supply if no property/value
 * was read.
 * @return The boolean value.
 */
bool icrule_get_xml_bool(xmlNodePtr node, const char* name, bool defvalue);

/** Helper to acquire a double value from XML.
 *
 * @param node The XML node to read the value/prop from.
 * @param name The XML property name to read, or NULL if reading
 * an XML value.
 * @param defvalue The default value to supply if no property/value
 * was read.
 * @return The double value.
 */
double icrule_get_xml_double(xmlNodePtr node, const char* name, double defvalue);

#endif //ZILKER_ICRULE_XML_H
