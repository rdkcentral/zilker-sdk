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
 * fileHelper.c
 *
 * file I/O functions to keep touchstone.c clean
 *
 * Author: jelderton - 7/1/16
 *-----------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <libxml/tree.h>

#include <icUtil/stringUtils.h>
#include <xmlHelper/xmlHelper.h>
#include "fileHelper.h"

#define SERVER_HOST_NODE    "hostnameIP"
#define MAX_LINE_LEN        1024

/*
 * parse a communication.conf file.  yes, crappy to duplicate
 * what the service does, but need a way to extract data when
 * the service is down.
 *
 * caller must free the returned string
 */
char *extractHostnameFromCommConf(const char *xmlFile)
{
    xmlDocPtr doc;
    xmlNodePtr topNode, hostNode;

    // open/parse the XML file
    //
    doc = xmlParseFile(xmlFile);
    if (doc == (xmlDocPtr) 0)
    {
        return NULL;
    }

    topNode = xmlDocGetRootElement(doc);
    if (topNode == (xmlNodePtr) 0)
    {
        xmlFreeDoc(doc);
        return NULL;
    }

    // find the <hostnameIP>
    //
    char *retVal = NULL;
    if ((hostNode = findChildNode(topNode, SERVER_HOST_NODE, true)) != NULL)
    {
        // extract contents
        //
        char *tmp = getXmlNodeContentsAsString(hostNode, NULL);
        if (tmp != NULL)
        {
            retVal = strdup(tmp);
        }
        free(tmp);
    }

    // cleanup and return
    //
    xmlFreeDoc(doc);
    return retVal;
}

/*
 * extract the contents of /tmp/server.txt
 */
char *extractHostnameFromMarker(const char *txtFile)
{
    FILE *fp = NULL;

    // open the file
    //
    if ((fp = fopen(txtFile, "r")) == NULL)
    {
        // error opening file
        //
        return NULL;
    }

    // read the contents
    //
    char *retVal = NULL;
    char readBuffer[MAX_LINE_LEN+1];
    if (fgets(readBuffer, MAX_LINE_LEN, fp) != NULL)
    {
        // clone and trim whitespace
        //
        retVal = trimString(readBuffer);
    }

    // cleanup and return
    //
    fclose(fp);
    return retVal;
}

/*
 * create a /tmp/server.txt file, using 'hostname' as the contents
 */
bool saveHostnameToMarker(const char *txtFile, const char *hostname)
{
    FILE *fp = NULL;

    // open the file
    //
    if ((fp = fopen(txtFile, "w")) == NULL)
    {
        // error opening file
        //
        return false;
    }

    // write the contents
    //
    fprintf(fp, "%s\n", hostname);
    fflush(fp);
    fclose(fp);
    return true;
}