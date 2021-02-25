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

#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>

#include "icrule_xml.h"

uint64_t icrule_get_xml_uint64(xmlNodePtr node, const char* name, uint64_t defvalue)
{
    uint64_t value = defvalue;
    xmlChar* value_xml;

    if (name) {
        value_xml = xmlGetProp(node, (const xmlChar*) name);
    } else {
        value_xml = xmlNodeGetContent(node);
    }

    if (value_xml) {
        errno = 0;
        value = strtoull((const char*) value_xml, NULL, 10);

        if (errno == ERANGE) value = defvalue;

        xmlFree(value_xml);
    }

    return value;
}

int64_t icrule_get_xml_int64(xmlNodePtr node, const char* name, int64_t defvalue)
{
    int64_t value = defvalue;
    xmlChar* value_xml;

    if (name) {
        value_xml = xmlGetProp(node, (const xmlChar*) name);
    } else {
        value_xml = xmlNodeGetContent(node);
    }

    if (value_xml) {
        errno = 0;
        value = strtoll((const char*) value_xml, NULL, 10);

        if (errno == ERANGE) value = defvalue;

        xmlFree(value_xml);
    }

    return value;
}

int icrule_get_xml_int(xmlNodePtr node, const char* name, int defvalue)
{
    long value = defvalue;
    xmlChar* value_xml;

    if (name) {
        value_xml = xmlGetProp(node, (const xmlChar*) name);
    } else {
        value_xml = xmlNodeGetContent(node);
    }

    if (value_xml) {
        errno = 0;
        value = strtol((const char*) value_xml, NULL, 10);

        if (errno == ERANGE) value = defvalue;

        xmlFree(value_xml);
    }

    return (int) value;
}

char* icrule_get_xml_string(xmlNodePtr node, const char* name, const char* defvalue)
{
    char* value;
    xmlChar* value_xml;

    if (name) {
        value_xml = xmlGetProp(node, (const xmlChar*) name);
    } else {
        value_xml = xmlNodeGetContent(node);
    }

    if (value_xml) {
        value = strdup((const char*) value_xml);
        xmlFree(value_xml);
    } else {
        value = (defvalue == NULL) ? NULL : strdup(defvalue);
    }

    return value;
}

bool icrule_get_xml_bool(xmlNodePtr node, const char* name, bool defvalue)
{
    bool value = defvalue;
    xmlChar* value_xml;

    if (name) {
        value_xml = xmlGetProp(node, (const xmlChar*) name);
    } else {
        value_xml = xmlNodeGetContent(node);
    }

    if (value_xml) {
        char* c;
        for (c = (char*) value_xml; *c; ++c) *c = (char) tolower(*c);

        value = (strcmp((const char*) value_xml, "true") == 0);
        xmlFree(value_xml);
    }

    return value;
}

double icrule_get_xml_double(xmlNodePtr node, const char* name, double defvalue)
{
    double value = defvalue;
    xmlChar* value_xml;

    if (name) {
        value_xml = xmlGetProp(node, (const xmlChar*) name);
    } else {
        value_xml = xmlNodeGetContent(node);
    }

    if (value_xml) {
        errno = 0;
        value = strtod((const char*) value_xml, NULL);

        if (errno == ERANGE) value = defvalue;

        xmlFree(value_xml);
    }

    return value;

}
