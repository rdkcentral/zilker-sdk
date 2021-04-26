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

// Created by dcalde202 on 3/28/19.
//

#ifndef ZILKER_SUNSETSUNRISECALCULATOR_H
#define ZILKER_SUNSETSUNRISECALCULATOR_H

#include <time.h>
#include <stdbool.h>

/*
 * Container for holding the values for sunset/sunrise
 * Free using standard free function
 */
typedef struct {
    time_t sunriseTime;
    time_t sunsetTime;
} sunriseSunset;

/*
 * Calculates the sunset/sunrise time for a given date described by the time_t object at the lat/long provided
 * Sets the values to 0 if sunset/sunrise is unavailable or if invalid lat/long provided
 * @date - time_t object describing the desired date's sunset and sunrise.  Should be in local time.
 * @lat - latitude of the location, must be between -90 and 90
 * @lng - longitude of the location, must be between -180 and 180
 * @output - container object to store the values for sunset/sunrise, individual times will be assigned 0
 *              if unable to calculate the sunrise/sunset or sunrise/sunset do not occur
 * returns false if unable to calculate sunset/sunrise
 */
bool calculateSunriseSunset(time_t date, double lat, double lng, sunriseSunset *output);

/*
 * Creates memory for sunriseSunset object and initializes the time to 0
 * Free using standard free function
 */
sunriseSunset *createSunriseSunset();

#endif //ZILKER_SUNSETSUNRISECALCULATOR_H
