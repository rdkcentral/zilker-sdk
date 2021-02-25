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
 * base64.c
 *
 * Helper functions for performing Base64
 * encode/decode operations.
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <icBuildtime.h>

#ifdef CONFIG_LIB_BEECRYPT
#include <beecrypt/base64.h>
#else
#include <mbedtls/base64.h>
#endif

#include <icUtil/base64.h>

/**
 * Encode a binary array into a Base-64 string.  If the return
 * is not-NULL, then the caller should free the memory.
 *
 * @return encoded string otherwise NULL
 */
char *icEncodeBase64(uint8_t *array, uint16_t arrayLen)
{
    if (array == NULL || arrayLen == 0)
    {
        return NULL;
    }

#ifdef CONFIG_LIB_BEECRYPT
    b64encode_chars_per_line = -1; //this will prevent beecrypt from breaking it up with newlines
    return b64encode(array, arrayLen);
#else
    size_t olen = 0;
    /*
     * Base64 has 4-byte output blocks, so output length must be a multiple of 4. Adding 3 and clearing the last
     * two bits effectively rounds up to the next block size (only a multiple of 4 bit can possibly be set).
     * Add one byte for NUL.
     */
    const size_t dlen = (((arrayLen * 4 / 3) + 3u) & ~3u) + 1;
    uint8_t *dst = calloc(dlen, 1);

    if (mbedtls_base64_encode(dst, dlen, &olen, array, arrayLen))
    {
        free(dst);
        return NULL;
    }
    else
    {
        return dst;
    }
#endif
}

/**
 * Decode the base64 encoded string 'src'.  If successful,
 * this will return true and allocate/populate the 'array' and 'arrayLen'.
 * in that case, the caller must free the array.
 */
bool icDecodeBase64(const char *srcStr, uint8_t **array, uint16_t *arrayLen)
{
    if (srcStr == NULL)
    {
        *arrayLen = 0;
        *array = NULL;
        return false;
    }

#ifdef CONFIG_LIB_BEECRYPT
    size_t len;
    int rc = b64decode(srcStr, (void**)array, &len);
    if(rc == 0)
    {
         *arrayLen = (uint16_t)len;
         return true;
    } 
    else
    {
        return false;
    }
#else
    size_t olen = 0;
    const size_t dlen = strlen(srcStr) * 3 / 4;
    *array = calloc(dlen, 1);
    *arrayLen = 0;

    if (mbedtls_base64_decode(*array, dlen, &olen, srcStr, strlen(srcStr)))
    {
        free(*array);
        *array = NULL;
        return false;
    }

    *arrayLen = (uint16_t) olen;
    return true;
#endif
}


