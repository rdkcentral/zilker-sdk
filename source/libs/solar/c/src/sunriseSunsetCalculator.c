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

// Created by dcalde202 on 3/26/19.
//

#include <icBuildtime.h>
#include <solar/sunriseSunsetCalculator.h>

#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <icLog/logging.h>
#include <icTime/timeUtils.h>

#include <time.h>

#define LOG_TAG "sunriseSunsetCalculator"

#define ZENITH -.83
#define DEGREES_TO_RADIAN (double)M_PI/180
#define RADIAN_TO_DEGREES (double)180/M_PI
#define ONE_DAY_IN_SECS  24 * 60 * 60

#define MIN_LNG -180
#define MAX_LNG 180
#define MIN_LAT -90
#define MAX_LAT 90

/*
 * Caculates the sunrise or the sunset time
 * Use true for sunrise and false for sunset
 * Opted not to break this up into smaller parts as the whole thing will pretty much be unreadable no matter what
 */
static time_t calculateSolarEvent(time_t date, double lat, double lng, bool sunrise)
{
    //1. first get the day of the year
    //
    struct tm currentTime;
    gmtime_r(&date, &currentTime);
    int dayOfYear = currentTime.tm_yday;

    //2. convert the longitude to hour value and calculate an approximate time
    // sun moves 15 degrees per hour, so converting from degrees * 1 hour/ 15 degrees to get the hour
    //
    double lngHour = lng / 15.0;

    // If calculating sunrise, use 6, if sunset use 18
    //
    int approximateEventTime = sunrise ? 6 : 18;

    //Calculate the approximate day/time of sunrise/sunset for the given year
    double approximateSunsetSunrise = (double)dayOfYear + ((approximateEventTime - lngHour) / 24);

    //3. calculate the Sun's mean anomaly
    //  Magic numbers are magic
    //
    double solarMeanAnomaly = (0.9856 * approximateSunsetSunrise) - 3.289;

    //4. calculate the Sun's true longitude
    //  Mod to make sure values are between 0 and 360
    //  Magic numbers are magic
    //
    double solarLongitude = fmod(solarMeanAnomaly + (1.916 * sin((DEGREES_TO_RADIAN) * solarMeanAnomaly)) + (0.02 * sin(2 *(DEGREES_TO_RADIAN) * solarMeanAnomaly)) + 282.634,360.0);

    //5a. calculate the Sun's right ascension
    //  Mod again to make sure the values are between 0 and 360
    //
    double solarRightAscension = fmod(RADIAN_TO_DEGREES * atan(0.91764 * tan((DEGREES_TO_RADIAN) * solarLongitude)), 360.0);

    //5b. right ascension value needs to be in the same quadrant as solarLongitude
    //
    double Lquadrant  = floor(solarLongitude / 90) * 90;
    double RAquadrant = floor(solarRightAscension / 90) * 90;
    solarRightAscension = solarRightAscension + (Lquadrant - RAquadrant);

    //5c. right ascension value needs to be converted into hours
    //  15 degrees per hour
    //
    solarRightAscension = solarRightAscension / 15;

    //6. calculate the Sun's declination
    //  Magic Number again
    //
    double sinDec = 0.39782 * sin((DEGREES_TO_RADIAN) * solarLongitude);
    double cosDec = cos(asin(sinDec));

    //7a. calculate the Sun's local hour angle
    double cosLocalHourAngle = (sin((DEGREES_TO_RADIAN) * ZENITH) - (sinDec * sin((DEGREES_TO_RADIAN) * lat))) / (cosDec * cos((DEGREES_TO_RADIAN) * lat));

    /*
    if (cosLocalHourAngle >  1)
    the sun never rises on this location (on the specified date)
    if (cosLocalHourAngle < -1)
    the sun never sets on this location (on the specified date)
    */
    if(cosLocalHourAngle > 1 || cosLocalHourAngle < -1)
    {
        icLogWarn(LOG_TAG,"%s:  %s event does not occur on the requested date",__FUNCTION__,sunrise ? "Sunrise":"Sunset");
        return 0;
    }

    //7b. finish calculating localHourAngle and convert into hours
    double localHourAngle;
    if(sunrise)
    {
        localHourAngle = 360 - (RADIAN_TO_DEGREES) * acos(cosLocalHourAngle);   //   if if rising time is desired:
    }
    else
    {
        localHourAngle = (RADIAN_TO_DEGREES) * acos(cosLocalHourAngle); //   if setting time is desired:
    }

    // convert into hours  (15 degrees per hour
    //
    localHourAngle = localHourAngle / 15;

    //8. calculate local mean time of rising/setting
    double localMeanTime = localHourAngle + solarRightAscension - (0.06571 * approximateSunsetSunrise) - 6.622;

    //9. adjust back to UTC
    //  Mod to make sure the value is between 0 and 24.
    //  This is the hour of the day that the sunset/sunrise will occur.
    //  Depending on the longitude the sunset time might occur on an earlier hour, but in the next day
    //  The formula doesn't account for that possibility and this must be detected and adjusted manually
    //
    double solarEventHourUTC = fmod(localMeanTime - lngHour, 24.0);

    //  Take the hour and get minutes out of it by using modf to split on the decimal
    //  Convert the decimal value to minutes by multipling by 60 minutes per hour
    //
    double hours;
    double minutes;
    minutes = modf(solarEventHourUTC,&hours) * 60;

    // Add the hours and minutes back into the struct tm to convert into time_t
    // Sometimes given the longitude, the
    //
    currentTime.tm_hour = (int) hours;
    currentTime.tm_min = (int) minutes;
    currentTime.tm_sec = 0;

    time_t retval = timegm(&currentTime);

    return retval;

}


bool calculateSunriseSunset(const time_t date, double lat, double lng, sunriseSunset *output)
{
    bool retVal = false;
    if(output != NULL)
    {
        // Make sure lat/lng are within bounds
        if (lat >= MIN_LAT && lat <= MAX_LAT && lng >= MIN_LNG && lng <= MAX_LNG)
        {
            output->sunriseTime = calculateSolarEvent(date, lat, lng, true);
            output->sunsetTime = calculateSolarEvent(date, lat, lng, false);

            if (output->sunriseTime != 0 || output->sunsetTime != 0)
            {
                retVal = true;
                // this is a work around for the problem that the equation returns just an hour of the day that the sunset/sunrise
                // will occur.  Depending on lat long, the sunset/sunrise for a given day local, might occur on different UTC days
                // Checking to make sure that the time_t describing sunset is greater than time_t describing sunrise is a hack
                // to allow us to know when to manually make this adjustment
                //
                if (output->sunriseTime > output->sunsetTime)
                {
                    output->sunsetTime = output->sunsetTime + ONE_DAY_IN_SECS;
                }
            }
        }
        else
        {
            icLogError(LOG_TAG,"%s:  Failed to calculate sunrise/sunset, invalid arguments",__FUNCTION__);
            output->sunsetTime = 0;
            output->sunriseTime = 0;
        }
    }
    else
    {
        icLogError(LOG_TAG,"%s:  Failed to calculate sunrise/sunset, output is null",__FUNCTION__);
    }
    return retVal;
}

/*
 * Creates memory for sunriseSunset object and initializes the time to 0
 * Free using standard free function
 */
sunriseSunset *createSunriseSunset()
{
    sunriseSunset *retVal = calloc(1, sizeof(sunriseSunset));
    return retVal;
}
