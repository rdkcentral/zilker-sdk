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

// Created by dcalde202 on 4/1/19.
//

#include <icLog/logging.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <limits.h>
#include "solar/sunriseSunsetCalculator.h"

#define NEW_YORK_LAT 40.72
#define NEW_YORK_LNG -70.02
#define DALLAS_LAT 32.78
#define DALLAS_LNG -96.8
#define SEATTLE_LAT 47.6
#define SEATTLE_LNG -122.32
#define TOKYO_LAT 35.6762
#define TOKYO_LNG 139.6503
#define SYDNEY_LAT -33.8688
#define SYDNEY_LNG 151.2093

/*
 * Pre-computed time_t values for the test date and the associated sunrise/sunset times.
 * Avoids unnecessary problems constructing a struct tm and then converting to time_t.
 */
#define NEW_YORK_TIME 1546300800
#define DALLAS_TIME 1559109600
#define SEATTLE_TIME 1572480000
#define TOKYO_TIME 1561680000
#define SYDNEY_TIME 1555027200

#define NEW_YORK_SUNRISE_MAX 1546344840
#define NEW_YORK_SUNRISE_MIN 1546343640
#define NEW_YORK_SUNSET_MIN 1546377180
#define NEW_YORK_SUNSET_MAX 1546378380

#define DALLAS_SUNRISE_MAX 1559129460
#define DALLAS_SUNRISE_MIN 1559128260
#define DALLAS_SUNSET_MIN 1559179080
#define DALLAS_SUNSET_MAX 1559180280

#define SEATTLE_SUNRISE_MAX 1572534060
#define SEATTLE_SUNRISE_MIN 1572532860
#define SEATTLE_SUNSET_MIN 1572568980
#define SEATTLE_SUNSET_MAX 1572570180

#define TOKYO_SUNRISE_MAX 1561664220
#define TOKYO_SUNRISE_MIN 1561663020
#define TOKYO_SUNSET_MIN 1561715460
#define TOKYO_SUNSET_MAX 1561716660

#define SYDNEY_SUNRISE_MAX 1555014300
#define SYDNEY_SUNRISE_MIN 1555013100
#define SYDNEY_SUNSET_MIN 1555053960
#define SYDNEY_SUNSET_MAX 1555055160

/*
 * Sunset/Sunrise times taken from https://www.timeanddate.com/sun/
 */

void test_calculateSunriseSunset(void **state)
{
    // Jan 1, 2019
    time_t new_york_time = NEW_YORK_TIME;
    sunriseSunset output;
    calculateSunriseSunset(new_york_time,NEW_YORK_LAT,NEW_YORK_LNG,&output);

    // Sunrise:  Jan 1, 2019 12:04 UTC
    // Sunset:  Jan 1, 2019 21:23 UTC
    assert_in_range(output.sunriseTime, NEW_YORK_SUNRISE_MIN, NEW_YORK_SUNRISE_MAX);
    assert_in_range(output.sunsetTime, NEW_YORK_SUNSET_MIN, NEW_YORK_SUNSET_MAX);

    // May 29, 2019
    time_t dallas_time = DALLAS_TIME;

    calculateSunriseSunset(dallas_time,DALLAS_LAT,DALLAS_LNG,&output);

    // Sunrise:  May 29, 2019 11:21 UTC
    // Sunset:  May 30, 2019 01:28 UTC
    assert_in_range(output.sunriseTime, DALLAS_SUNRISE_MIN, DALLAS_SUNRISE_MAX);
    assert_in_range(output.sunsetTime, DALLAS_SUNSET_MIN, DALLAS_SUNSET_MAX);

    // Oct 31. 2019
    time_t seattle_time = SEATTLE_TIME;

    calculateSunriseSunset(seattle_time,SEATTLE_LAT,SEATTLE_LNG,&output);

    // Sunrise:  Oct 31, 2019 14:51 UTC
    // Sunset:  Nov 1, 2019 00:53 UTC
    assert_in_range(output.sunriseTime, SEATTLE_SUNRISE_MIN, SEATTLE_SUNRISE_MAX);
    assert_in_range(output.sunsetTime, SEATTLE_SUNSET_MIN, SEATTLE_SUNSET_MAX);

    // Jun 28, 2019
    time_t tokyo_time = TOKYO_TIME;

    calculateSunriseSunset(tokyo_time,TOKYO_LAT,TOKYO_LNG,&output);

    // Sunrise:  Jun 27, 2019 19:27 UTC
    // Sunset:  Jun 28, 2019 00:53 UTC
    assert_in_range(output.sunriseTime, TOKYO_SUNRISE_MIN, TOKYO_SUNRISE_MAX);
    assert_in_range(output.sunsetTime, TOKYO_SUNSET_MIN, TOKYO_SUNSET_MAX);

    // Apr 12. 2019
    time_t sydney_time = SYDNEY_TIME;

    calculateSunriseSunset(sydney_time,SYDNEY_LAT,SYDNEY_LNG,&output);

    // Sunrise:  Apr 11, 2019 20:15 UTC
    // Sunset:  Apr 12, 2019 07:36 UTC
    assert_in_range(output.sunriseTime, SYDNEY_SUNRISE_MIN, SYDNEY_SUNRISE_MAX);
    assert_in_range(output.sunsetTime, SYDNEY_SUNSET_MIN, SYDNEY_SUNSET_MAX);
    (void )state;
}



int main(int argc, const char **argv)
{
    initIcLogger();

    const struct CMUnitTest tests[] =
            {
                    cmocka_unit_test(test_calculateSunriseSunset),
            };

    int retval = cmocka_run_group_tests(tests, NULL, NULL);

    closeIcLogger();

    return retval;
}