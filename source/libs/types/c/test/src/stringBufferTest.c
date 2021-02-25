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
 * stringBufferTest.c
 *
 * Unit test the icStringBuffer object
 *
 * Author: mkoch201 - 5/16/18
 *-----------------------------------------------*/

#include <icLog/logging.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "icTypes/icStringBuffer.h"

#define LOG_CAT     "stringBufferTEST"


static bool canAppendAndGet()
{
    icStringBuffer *buff = stringBufferCreate(1024);

    stringBufferAppend(buff, "ABC");
    stringBufferAppend(buff, "def");
    stringBufferAppend(buff, "123");

    uint32_t len = stringBufferLength(buff);
    if (len != 9)
    {
        // expecting 9, got something else
        icLogError(LOG_CAT, "appendAndGet: expected length of 9, got %"PRIu32, len);
        stringBufferDestroy(buff);
        return false;
    }

    char *out = stringBufferToString(buff);
    if (strcmp(out, "ABCdef123") != 0)
    {
        icLogError(LOG_CAT, "appendAndGet: expected ABCdef123, got %s", out);
        stringBufferDestroy(buff);
        free(out);
        return false;
    }

    stringBufferDestroy(buff);
    free(out);
    return true;
}

static bool canAppendGetAndAppendAgain()
{
    icStringBuffer *buff = stringBufferCreate(1024);

    stringBufferAppend(buff, "ABC");
    char *out1 = stringBufferToString(buff);
    stringBufferAppend(buff, "def");
    char *out2 = stringBufferToString(buff);

    if (strcmp(out1, "ABC") != 0)
    {
        icLogError(LOG_CAT, "canAppendGetAndAppendAgain: expected ABC, got %s", out1);
        stringBufferDestroy(buff);
        free(out1);
        free(out2);
        return false;
    }

    if (strcmp(out2, "ABCdef") != 0)
    {
        icLogError(LOG_CAT, "canAppendGetAndAppendAgain: expected ABCdef, got %s", out1);
        stringBufferDestroy(buff);
        free(out1);
        free(out2);
        return false;
    }

    stringBufferDestroy(buff);
    free(out1);
    free(out2);
    return true;
}


/*
 *
 */
bool runStringBufferTests()
{
    if (canAppendAndGet() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAppendAndGet");
        return false;
    }
    if (canAppendGetAndAppendAgain() != true)
    {
        icLogError(LOG_CAT, "unsuccessful test, canAppendGetAndAppendAgain");
        return false;
    }

    return true;
}


