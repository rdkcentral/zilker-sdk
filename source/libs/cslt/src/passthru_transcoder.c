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
// Created by Boyd, Weston on 4/17/18.
//

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <cslt/cslt.h>

#include "cslt_internal.h"

// stupid Android.  might be a better way to do this
#ifndef SIZE_MAX
#define SIZE_MAX (64*1024)
#endif

int passthru_transcode(const char* src, char** dst, size_t size)
{
    if ((src == NULL) || (strlen(src) == 0)) {
        errno = EINVAL;
        return -1;
    }

    if ((dst == NULL) || (size == 0)) {
        errno = EINVAL;
        return -1;
    }

    if ((*dst == NULL) && (size != SHEEN_MSGSIZE)) {
        errno = EINVAL;
        return -1;
    }

    if (*dst == NULL) {
        *dst = (char*) src;
    } else {
        size_t src_size;

        // Count the null terminator '\0'
        src_size = strlen(src) + 1;

        if (src_size > size) {
            errno = E2BIG;
            return -1;
        }

        memcpy(*dst, src, src_size);
    }

    return 0;
}

