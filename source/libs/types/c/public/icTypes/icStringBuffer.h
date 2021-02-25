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

#ifndef ZILKER_ICSTRINGBUFFER_H
#define ZILKER_ICSTRINGBUFFER_H

#include <inttypes.h>
#include <stdbool.h>

/*-------------------------------*
 *
 *  icStringBuffer
 *
 *-------------------------------*/

/**
 * the StringBuffer object representation.
 */
typedef struct _icStringBuffer icStringBuffer;

/**
 * create a new StringBuffer.  will need to be released when complete
 * @param initialSize - number of bytes to pre-allocate for the buffer.  If <= 0, then the default of 1024 will be used
 * @see stringBufferDestroy()
 */
icStringBuffer *stringBufferCreate(uint32_t initialSize);

/**
 * destroy a StringBuffer and cleanup memory.
 *
 * @param stringBuffer - the StringBuffer to delete
 */
void stringBufferDestroy(icStringBuffer *stringBuffer);

/**
 * StringBuffer cleanup helper for AUTO_CLEAN
 * @param stringBuffer
 */
inline void stringBufferDestroy__auto(icStringBuffer **stringBuffer)
{
    stringBufferDestroy(*stringBuffer);
}

/**
 * Append a string to the buffer, the contents of the string are copied
 *
 * @param stringBuffer - the StringBuffer to append to
 * @param string - the string to append, its contents will be copied into the string
 */
void stringBufferAppend(icStringBuffer *stringBuffer, const char *string);

/**
 * Append a partial string to the buffer, the contents of the string are copied
 *
 * @param stringBuffer - the StringBuffer to append to
 * @param string - the string to append, its contents will be copied into the string
 * @param length - number of characters from 'string' to copy
 */
void stringBufferAppendLength(icStringBuffer *stringBuffer, const char *string, uint16_t length);

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
void stringBufferAppendWithComma(icStringBuffer *stringBuffer, const char *string, bool comaAtBeginning);

/**
 * Get the current length of the conents of the string buffer(does not include null terminator)
 *
 * @param stringBuffer the StringBuffer to get the length of
 * @return the length of the buffer, excluding any null terminator
 */
uint32_t stringBufferLength(icStringBuffer *stringBuffer);

/**
 * Get the contents of the string buffer, the caller will need to free the results
 *
 * @param stringBuffer - the StringBuffer to get
 * @return the string buffer contents
 */
char *stringBufferToString(icStringBuffer *stringBuffer);

#endif //ZILKER_ICSTRINGBUFFER_H
