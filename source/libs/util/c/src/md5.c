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
 * md5.h
 *
 * Helper functions for performing
 * MD5 checksum operations
 *
 * Author: jelderton
 *-----------------------------------------------*/

#include <stdio.h>
#include <string.h>

#include <icBuildtime.h>

#ifdef CONFIG_LIB_BEECRYPT
#include <beecrypt/md5.h>
#else
#include <mbedtls/md5.h>
#endif

#include <icUtil/md5.h>

#define DIGEST_BYTE_LEN     16
#define DIGEST_STR_LEN      (DIGEST_BYTE_LEN * 2)

/*
 *
 */
void copyBinaryToString(char *dest, unsigned char *src)
{
    // convert the binary md5sum into a string
    for (int i = 0; i < DIGEST_BYTE_LEN; i++)
    {
        sprintf(dest + (i * 2), "%02x", src[i]);
    }
    dest[DIGEST_STR_LEN] = '\0';
}

/*
 * perform an MD5 checksum on the provided buffer.
 * if not-NULL, caller is responsible for returning
 * the string
 */
char *icMd5sum(const char *src)
{
    unsigned char checksum[DIGEST_BYTE_LEN];

#ifdef CONFIG_LIB_BEECRYPT
    md5Param param;
    md5Reset(&param);
    md5Update(&param, src, strlen(src));
    md5Digest(&param, checksum);
#else
    mbedtls_md5_ret(src, strlen(src), checksum);
#endif

    // now convert to string
    //
    char result[DIGEST_STR_LEN + 1];
    copyBinaryToString(result, checksum);
    return strdup(result);
}

