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
// Created by mkoch201 on 5/15/18.
//

#include "icTypes/icStringBuffer.h"
#include "icTypes/icFifoBuffer.h"
#include <memory.h>
#include <stdlib.h>

#define COMMA_STRING ","

extern inline void stringBufferDestroy__auto(icStringBuffer **stringBuffer);

/**
 * the string buffer
 */
struct _icStringBuffer
{
    icFifoBuff *fifoBuffer; // Internally this is just a nice API around a fifoBuffer
};

/**
 * create a new StringBuffer.  will need to be released when complete
 * 
 * @param initialSize - number of bytes to pre-allocate for the buffer.  If <= 0, then the default of 1024 will be used
 * @return a new StringBuffer object
 * @see stringBufferDestroy()
 */
icStringBuffer *stringBufferCreate(uint32_t initialSize)
{
    icStringBuffer *stringBuffer = (icStringBuffer *)calloc(1, sizeof(icStringBuffer));
    if (stringBuffer != NULL)
    {
        stringBuffer->fifoBuffer = fifoBuffCreate(initialSize);
    }

    return stringBuffer;
}

/**
 * destroy a StringBuffer and cleanup memory.
 *
 * @param stringBuffer - the StringBuffer to delete
 */
void stringBufferDestroy(icStringBuffer *stringBuffer)
{
    if (stringBuffer != NULL)
    {
        fifoBuffDestroy(stringBuffer->fifoBuffer);
        free(stringBuffer);
    }
}

/**
 * Append a string to the buffer, the contents of the string are copied
 *
 * @param stringBuffer - the StringBuffer to append to
 * @param string - the string to append, its contents will be copied into the buffer
 */
void stringBufferAppend(icStringBuffer *stringBuffer, const char *string)
{
    if (stringBuffer != NULL && string != NULL)
    {
        fifoBuffPush(stringBuffer->fifoBuffer, (void *)string, strlen(string));
    }
}

/**
 * Append a partial string to the buffer, the contents of the string are copied
 *
 * @param stringBuffer - the StringBuffer to append to
 * @param string - the string to append, its contents will be copied into the string
 * @param length - number of characters from 'string' to copy
 */
void stringBufferAppendLength(icStringBuffer *stringBuffer, const char *string, uint16_t length)
{
    if (stringBuffer != NULL && string != NULL && length > 0)
    {
        fifoBuffPush(stringBuffer->fifoBuffer, (void *)string, sizeof(char) * length);
    }
}

/**
 * Append a string to the buffer along with a coma, the contents of the string are copied.
 * Set flag "comaAtBeginning" to add coma to the beginning of the string, false will add
 * coma after the string.
 *
 * @note Will not add comma to the beginning of buffer, if its length is 0.
 *
 * @param stringBuffer - the StringBuffer to append to
 * @param string - the string to append, its contents will be copied into the buffer
 * @param comaAtBeginning - flag for where the coma should go
 */
void stringBufferAppendWithComma(icStringBuffer *stringBuffer, const char *string, bool comaAtBeginning)
{
    if (stringBuffer != NULL && string != NULL)
    {
        if (comaAtBeginning == true)
        {
            if (fifoBuffGetPullAvailable(stringBuffer->fifoBuffer) != 0)
            {
                fifoBuffPush(stringBuffer->fifoBuffer, (void *)COMMA_STRING, strlen(COMMA_STRING));
            }
            fifoBuffPush(stringBuffer->fifoBuffer, (void *)string, strlen(string));
        }
        else
        {
            fifoBuffPush(stringBuffer->fifoBuffer, (void *)string, strlen(string));
            fifoBuffPush(stringBuffer->fifoBuffer, (void *)COMMA_STRING, strlen(COMMA_STRING));
        }
    }
}

/**
 * Get the current length of the conents of the string buffer(does not include null terminator)
 *
 * @param stringBuffer the StringBuffer to get the length of
 * @return the length of the buffer, excluding any null terminator
 */
uint32_t stringBufferLength(icStringBuffer *stringBuffer)
{
    if (stringBuffer != NULL)
    {
        return fifoBuffGetPullAvailable(stringBuffer->fifoBuffer);
    }
    else
    {
        return 0;
    }
}

/**
 * Get the contents of the string buffer, the caller will need to free the results
 *
 * @param stringBuffer - the StringBuffer to get
 *
 * @return the string buffer contents
 */
char *stringBufferToString(icStringBuffer *stringBuffer)
{
    char *retval = NULL;
    if (stringBuffer != NULL)
    {
        uint32_t available = fifoBuffGetPullAvailable(stringBuffer->fifoBuffer);
        // Just a get a pointer so the read position doesn't change, so they can append more after this
        void *ptr = fifoBuffPullPointer(stringBuffer->fifoBuffer, available);
        retval = malloc(available+1);
        memcpy(retval, ptr, available);
        retval[available] = '\0';
    }

    return retval;
}

