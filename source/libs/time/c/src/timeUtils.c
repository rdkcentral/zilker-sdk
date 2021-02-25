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
 * timeUtils.h
 *
 * Provide common mechanism for getting and comparting time
 * Necessary as we have different mechanisms for time based
 * on the target platform.
 *
 * Author: jelderton - 6/23/15
 *-----------------------------------------------*/

#include <icBuildtime.h>
#include <sys/time.h>
#include <icTime/timeUtils.h>
#include <stdint.h>
#include <icLog/logging.h>
#include <icUtil/stringUtils.h>
#include <inttypes.h>

#define LOG_TAG "timeUtils"

#define MILLIS_PER_SEC 1000UL
#define NANOS_PER_MILLI 1000000UL
#define BEGINNING_OF_2019_SECS 1546300800

/*
 * returns if this platform supports the monotonic clock
 * primarily used for getCurrentTime() and incrementalCondTimedWait()
 */
bool supportMonotonic()
{
    // let the compiler tell us if this platform supports monotonic clocks
    // (should be defined in time.h)
    //
#ifdef CLOCK_MONOTONIC
    return true;
#else
    return false;
#endif
}

/*
 * gets the current time (using the platform specific mechanism)
 * and populates 'spec'.  If 'useMonotonic' is false, then the
 * "system real-time" clock will be used.
 */
void getCurrentTime(struct timespec *spec, bool useMonotonic)
{
    // double check that we support monotonic
    //
    if (useMonotonic == true)
    {
        if (supportMonotonic() == false)
        {
            icLogWarn(LOG_TAG, "CLOCK_MONOTONIC requested but not supported, falling back on CLOCK_REALTIME");
            useMonotonic = false;
        }
    }

#ifdef CONFIG_OS_DARWIN
    // do this the Mac way
    //
    struct timeval now;
    gettimeofday(&now, NULL);
    spec->tv_sec  = now.tv_sec;
    spec->tv_nsec = now.tv_usec * 1000;

#else
    // standard linux way
    //
    clockid_t theClock = CLOCK_MONOTONIC;
    if (useMonotonic == false)
    {
        theClock = CLOCK_REALTIME;
    }
    clock_gettime(theClock, spec);
#endif
}

/*
 * Gets the current time and returns it as millis elapsed.  If 'useMonotonic' is false, then the
 * "system real-time" clock will be used.
 */
static uint64_t getCurrentTimeInMillis(bool useMonotonic)
{
    uint64_t result;
    struct timespec now;

    getCurrentTime(&now, useMonotonic);
    result = convertTimespecToUnixTimeMillis(&now);

    return result;
}

/*
 * same as getCurrentTime, but returns as a time_t
 */
time_t getCurrentTime_t(bool useMonotonic)
{
    struct timespec local;

    getCurrentTime(&local, useMonotonic);
    return convertTimespecToTime_t(&local);
}

/*
 * convert from 'struct timespec' to 'time_t'
 */
time_t convertTimespecToTime_t(struct timespec *spec)
{
    return (time_t)spec->tv_sec;
}

/**
 * Convert the timespec to milliseconds since 1970-01-01T00:00:00Z
 */
uint64_t convertTimespecToUnixTimeMillis(const struct timespec *spec)
{
    uint64_t millis = ((uint64_t)spec->tv_sec)*MILLIS_PER_SEC;
    millis += spec->tv_nsec/NANOS_PER_MILLI;
    return millis;
}

/**
 * Convert milliseconds since 1970-01-01T00:00:00Z to timespec
 */
void convertUnixTimeMillisToTimespec(uint64_t millis, struct timespec *spec)
{
    if (spec != NULL)
    {
        spec->tv_sec = millis/MILLIS_PER_SEC;
        spec->tv_nsec = (millis % MILLIS_PER_SEC) * NANOS_PER_MILLI;
    }
}

/**
 * Convert milliseconds since 1970-01-01T00:00:00Z to time_t
 */
time_t convertUnixTimeMillisToTime_t(uint64_t millis)
{
    return millis/MILLIS_PER_SEC;
}

/**
 * Convert seconnds since 1970-01-01T00:00:00Z to milliseconds since 1970-01-01T00:00:00Z
 */
uint64_t convertTime_tToUnixTimeMillis(time_t time)
{
    uint64_t retVal = time;
    return retVal*MILLIS_PER_SEC;
}

/*
 * Compute the end - beginning and store in diff
 */
void timespecDiff(struct timespec *end, struct timespec *beginning, struct timespec *diff)
{
    diff->tv_sec = end->tv_sec - beginning->tv_sec;
    diff->tv_nsec = end->tv_nsec - beginning->tv_nsec;
    if (diff->tv_nsec < 0)
    {
        --(diff->tv_sec);
        diff->tv_nsec += 1000000000UL;
    }
}

/*
 * Computer first + second and store in output
 */
void timespecAdd(struct timespec *first, struct timespec *second, struct timespec *output)
{
    output->tv_sec = first->tv_sec + second->tv_sec;
    output->tv_nsec = first->tv_nsec + second->tv_nsec;
    if (output->tv_nsec > 1000000000UL)
    {
        ++(output->tv_sec);
        output->tv_nsec -= 1000000000UL;
    }
}

uint64_t getCurrentUnixTimeMillis(void)
{
    return getCurrentTimeInMillis(false);
}

/**
 * Get the current monotonic clock in milliseconds elapsed
 * @return the monotonic clock as milliseconds
 */
uint64_t getMonotonicMillis(void)
{
    return getCurrentTimeInMillis(true);
}

/**
 * Convert a unix timestamp in milliseconds since epoch to ISO8601 format
 * @param millis the timestamp in millis
 * @return the string, caller must free
 */
char *unixTimeMillisToISO8601(uint64_t millis)
{
    struct tm timeval;

    // convert 'time_t' to 'tm', then place into the necessary format
    //
    time_t time = millis/MILLIS_PER_SEC;

    gmtime_r(&time, &timeval);
    return stringBuilder("%d-%02d-%02dT%02d:%02d:%02d.%03"PRIu64"Z",
                         timeval.tm_year + 1900,
                         timeval.tm_mon + 1,
                         timeval.tm_mday,
                         timeval.tm_hour,
                         timeval.tm_min,
                         timeval.tm_sec,
                         millis % MILLIS_PER_SEC);
}

/**
 * Check if the current system time looks valid (not close to Unix epoch)
 *
 * @return true if it appears that the current system time is valid
 */
bool isSystemTimeValid()
{
    struct timespec now;
    getCurrentTime(&now, false);
    return now.tv_sec > BEGINNING_OF_2019_SECS;
}

