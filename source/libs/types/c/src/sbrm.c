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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <icLog/logging.h>
#include "icTypes/sbrm.h"

#define LOG_CAT "SBRM"

extern inline void free_generic__auto(void *p);

void fclose__auto(FILE **fp)
{
    if (*fp == NULL)
    {
        return;
    }

    int fd = fileno(*fp);
    if (fd >= 0)
    {
        int flags = fcntl(fd, F_GETFL);
        if (flags != -1)
        {
            if ((flags & O_RDONLY) == 0)
            {
                if (fflush(*fp) != 0)
                {
                    icLogWarn(LOG_CAT, "Failed to flush stream for fd %d: errno %d!", fd, errno);
                }

                if (fsync(fd) != 0)
                {
                    icLogWarn(LOG_CAT, "Failed to sync fd %d: errno %d. Data has been lost!", fd, errno);
                }
            }
        }
        else
        {
            icLogWarn(LOG_CAT, "Failed to get file status flags on fd %d: errno %d", fd, errno);
        }
    }
    else
    {
        icLogWarn(LOG_CAT, "fp is not a valid stream");
    }

    fclose(*fp);
}