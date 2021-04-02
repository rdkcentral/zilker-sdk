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

/**
 * Convert a point in time (milliseconds) to a weekly occurrence object (WeekTime). WeekTimes specify a
 * bitmask for which days of the week and the number of seconds from 0 (midnight) on those days. The bitmask
 * is a shift of Date's getDay return value. That is, 1 == sunday, 2 == monday, 4 == tuesday, 8 == wednesday,
 * 16 == thursday, 32 == friday, 64 == saturday. All days of the week would have the bitmask 127.
 *
 * @param milliseconds
 * @constructor
 */
function WeekTime(milliseconds) {
  var now = new Date(milliseconds);

  this.daysOfWeek = 1 << now.getDay();
  this.seconds = (now.getHours() * 60 * 60) + (now.getMinutes() * 60);
}

/**
 * Gets the time component from the provided WeekTime t. If the seconds component of t is encoded with a sunrise or
 * sunset key specifier, it will instead convert the provided sunrise/sunset Dates to WeekTimes and return their
 * respective seconds component.
 *
 * @note Macros for sunrise and sunset times 'sheens_sunrise_key' and 'sheens_sunset_key' are defined in sheens_json.h.
 *
 * @param t
 * @param sunrise
 * @param sunset
 * @returns {number|*}
 */
function getTimeSeconds(t, sunrise, sunset) {
  var wt;

  if (t.seconds === '{{sheens_sunrise_key}}') {
      wt = new WeekTime(sunrise);
  } else if (t.seconds === '{{sheens_sunset_key}}') {
      wt = new WeekTime(sunset);
  } else {
      wt = t;
  }

  return wt.seconds;
}

/**
 * Returns true if two WeekTime objects are exact matches. Decodes sunrise/sunset key specifiers.
 *
 * @param t0
 * @param t1
 * @param sunrise
 * @param sunset
 * @returns {boolean}
 */
function isTimeMatch(t0, t1, sunrise, sunset) {
  return (((t1.daysOfWeek & t0.daysOfWeek) != 0) &&
          (getTimeSeconds(t1, sunrise, sunset) == getTimeSeconds(t0, sunrise, sunset)));
}

/**
 * Returns true if a WeekTime is between a start and end WeekTime.
 *
 * @param now
 * @param start
 * @param end
 * @returns {boolean}
 */
function isInInterval(now, start, end) {
  // Handle the trivial case of everything being on the same day
  if ((now.daysOfWeek & start.daysOfWeek & end.daysOfWeek) != 0) {
      if (now.seconds >= start.seconds && now.seconds < end.seconds)
      {
          return true;
      }
  }

  // Backtrack from 'now' to the closest "start" without hitting an "end". If we do that, we
  // know "now" is between "start" and "end"
  for (var i = now.daysOfWeek, looped = false; i != now.daysOfWek && looped; i >>=1) {
      // Wrap around
      if (i == 0) {
          i = (1 << 6)
          looped = true;
      }

      // See if we hit an end before we hit a start
      if ((end.daysOfWeek & i) != 0) {
          // If the end is today, but we have not hit it yet, keep searching for a start
          if ((end.daysOfWeek & now.daysOfWeek) != 0 && now.seconds < end.seconds) {
              continue;
          }
          // Otherwise, we hit an end before a start"
          return false;
      }
      else if ((start.daysOfWeek & i) != 0) {
          // If this start is today, check if now is before it
          if ((start.daysOfWeek & now.daysOfWeek) != 0 && now.seconds < start.seconds) {
              return false;
          }
          // Otherwise, we found a start before now without encountering an end.
          return true;
      }
  }
}

/**
 * Returns the number of seconds to occur between "now" and "end" WeekTimes.
 *
 * @param now
 * @param end
 * @returns {number}
 */
function getEndDate(now, end) {
  if ((end.daysOfWeek & now.daysOfWeek) != 0 && end.seconds > now.seconds) {
      return (end.seconds - now.seconds);
  } else {
      for (var index = 1, i = now.daysOfWeek << 1; i != now.daysOfWeek; i <<= 1, index++) {
          if (i == (1 << 7)) {
              i = 1;
          }

          if ((end.daysOfWeek & i) != 0) {
              return (end.seconds + ((index * 24) * 60 * 60)) - now.seconds;
          }
      }

      return 0;
  }
}