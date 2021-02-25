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
// Created by Boyd, Weston on 5/15/18.
//

#ifndef ZILKER_ICRULE_TIME_H
#define ZILKER_ICRULE_TIME_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    ICRULE_SUNTIME_SUNRISE = 1,
    ICRULE_SUNTIME_SUNSET,
} icrule_time_sun_t;

#define ICRULE_TIME_INVALID   (0 << 0)
#define ICRULE_TIME_SUNDAY    (1 << 0)
#define ICRULE_TIME_MONDAY    (1 << 1)
#define ICRULE_TIME_TUESDAY   (1 << 2)
#define ICRULE_TIME_WEDNESDAY (1 << 3)
#define ICRULE_TIME_THURSDAY  (1 << 4)
#define ICRULE_TIME_FRIDAY    (1 << 5)
#define ICRULE_TIME_SATURDAY  (1 << 6)

#define ICRULE_TIME_WEEK (0x7F)

typedef struct icrule_time {
    uint8_t day_of_week;
    bool use_exact_time;

    union
    {
        // Seconds since midnight 00:00 -> 23:59
        uint32_t seconds;
        icrule_time_sun_t sun_time;
    } time;
} icrule_time_t;

#endif //ZILKER_ICRULE_TIME_H
