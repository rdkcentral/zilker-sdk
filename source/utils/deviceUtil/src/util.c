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

/*
 * Created by Thomas Lea on 9/27/19.
 */

#include <deviceService/deviceService_pojo.h>
#include "util.h"
#include <string.h>
#include <deviceService/resourceModes.h>
#include <icUtil/stringUtils.h>

#define SENSITIVE_RESOURCE_VALUE_STRING "(encrypted)"

char *getResourceDump(const DSResource *resource)
{
    AUTO_CLEAN(free_generic__auto) const char *modeStr = stringifyMode(resource->mode);

    return stringBuilder("id=%s, uri=%s, ownerId=%s, ownerClass=%s, value=%s, type=%s, mode=0x%x (%s)",
                         resource->id,
                         resource->uri,
                         resource->ownerId,
                         resource->ownerClass,
                         getResourceValue(resource),
                         resource->type,
                         resource->mode,
                         modeStr);
}

char *getResourceValue(const DSResource *resource)
{
    return (resource->mode & RESOURCE_MODE_SENSITIVE) ? (SENSITIVE_RESOURCE_VALUE_STRING) : (resource->value);
}

char *getInputLine()
{
    char *line = malloc(100), *linep = line;
    size_t lenmax = 100, len = lenmax;
    int c;

    if (line == NULL)
    {
        return NULL;
    }

    for (;;)
    {
        c = fgetc(stdin);
        if (c == EOF)
        {
            free(line);
            return strdup("exit");
        }
        else if (c == '\n')
        {
            break;
        }

        if (--len == 0)
        {
            len = lenmax;
            char *linen = realloc(linep, lenmax *= 2);

            if (linen == NULL)
            {
                free(linep);
                return NULL;
            }
            line = linen + (line - linep);
            linep = linen;
        }

        if ((*line++ = c) == '\n')
        {
            break;
        }
    }
    *line = '\0';
    return linep;
}

static char **saveToken(char **args, int numArgs, const char *start, const char *end)
{
    char **result = realloc(args, numArgs * sizeof(char *));
    size_t tokenLen = end - start + 1;
    result[numArgs - 1] = malloc(tokenLen);
    memcpy(result[numArgs - 1], start, tokenLen - 1);

    //kill any ending qoute
    if(tokenLen >=2 && result[numArgs - 1][tokenLen -2] == '\"')
    {
        result[numArgs - 1][tokenLen - 2] = 0;
    }
    else
    {
        result[numArgs - 1][tokenLen - 1] = 0;
    }

    return result;
}

// the complexity here is handling tokens that are quoted and merging them
char **getTokenizedInput(const char *line, int *numTokens)
{
    char **result = malloc(sizeof(char *));
    *numTokens = 0;

    const char *start = NULL;
    int state = ' ';
    char *s = (char *)line;

    while (*s)
    {
        switch (state)
        {
            case ' ': // Consuming spaces
                if (*s == '\"')
                {
                    start = s+1;
                    state = '\"';  // begin quote
                }
                else if (*s != ' ')
                {
                    start = s;
                    state = 'T';
                }
                break;
            case 'T': // non-quoted text
                if (*s == ' ')
                {
                    ++(*numTokens);
                    result = saveToken(result, *numTokens, start, s);
                    state = ' ';
                }
                else if (*s == '\"')
                {
                    state = '\"'; // begin quote
                }
                break;
            case '\"': // Inside a quote
                if (*s == '\"')
                {
                    state = 'T'; // end quote
                }
                break;

            default:
                break;
        }
        s++;
    }

    if (state != ' ')
    {
        ++(*numTokens);
        result = saveToken(result, *numTokens, start, s);
    }

    return result;
}

const char *stringifyMode(uint8_t mode)
{
    char buf[8];

    buf[0] = (mode & RESOURCE_MODE_READABLE) ? ('r') : ('-');
    buf[1] = (mode & RESOURCE_MODE_WRITEABLE) ? ('w') : ('-');
    buf[2] = (mode & RESOURCE_MODE_EXECUTABLE) ? ('x') : ('-');
    buf[3] = (mode & RESOURCE_MODE_DYNAMIC) || (mode & RESOURCE_MODE_DYNAMIC_CAPABLE) ? ('d') : ('-');
    buf[4] = (mode & RESOURCE_MODE_EMIT_EVENTS) ? ('e') : ('-');
    buf[5] = (mode & RESOURCE_MODE_LAZY_SAVE_NEXT) ? ('l') : ('-');
    buf[6] = (mode & RESOURCE_MODE_SENSITIVE) ? ('s') : ('-');
    buf[7] = '\0';

    return strdup(buf);
}
