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

/*
 * ECMA-5.1 fragment for scheduler action (injected into a function by sheens)
 * Environment (littlesheens js/sandbox.js):
 *  - _.bindings: Current machine data, which may contain bindings
 *  - _.out({Any} m): function that emits message 'm' that will be executed.
 * This MUST return a 'bindings' object
 *
 * The JavaScript that handles timer, hold, and schedule change events to determine what the current settings are.
 * To change what events are handled by this, update the match patterns in sheens_transcoders.c::transcode_schedules.
 *
 * We first initialize each schedule into its respective cool/heat lists. Then save those into our 'persist' binding so
 * we only have to do that once (everything outside of 'persist' is erased after each machine step).
 *
 * On each matching event, we run through each schedule in the cool/heat lists and determine the most recent transition
 * that occurs before now (i.e., the current schedule entry).  When this transition index is different from the last
 * recorded one, an action is sent. The native automation handler MUST respect the hold setting. Because each step does
 * not necessarily emit an action, a manual adjustment while the schedule is running results in a temporary hold: the
 * manual adjustment is left alone automatically until the next  transition.
 *
 * This file available to native code as 'SCHEDULERACTIONS_JS_BLOB'
 */
// var bs = (function(_) {
/**
 * Class representing a set of weekdays and seconds since midnight on those days.
 * daysOfWeek may have any of bits 0-6 set, where bit 0 is Sunday.
 * @param weekTime
 * @constructor
 */
function WeekTime(weekTime) {
    this.daysOfWeek = 0;
    this.seconds = 0;

    if (weekTime.daysOfWeek !== null && weekTime.daysOfWeek !== undefined) {
        this.daysOfWeek = weekTime.daysOfWeek;

        if (weekTime.seconds !== null && weekTime.seconds !== undefined) {
            this.seconds = weekTime.seconds;
        }
    } else if (weekTime.seconds !== null && weekTime.seconds !== undefined) {
        var now = new Date(weekTime.seconds * 1000);

        this.daysOfWeek = 1 << now.getDay();
        this.seconds = (now.getHours() * 60 * 60) + (now.getMinutes() * 60);
    }
}

/**
 * Get a new WeekTime discarding daysOfWeek after the given weekTime
 * @param weekTime
 * @returns {WeekTime} with any days after weekTime cleared
 * @note daysOfWeek may be 0, indicating no days up to weekTime (inclusive) are set.
 */
WeekTime.prototype.withDaysUpTo = function(weekTime) {
    var retVal = new WeekTime(this);
    var latestDay = 0;
    var latestDaysMask;
    var tmp = weekTime.daysOfWeek;

    while ((tmp >>= 1) !== 0) {
        latestDay++;
    }
    latestDaysMask = 1 << latestDay;
    latestDaysMask |= latestDaysMask - 1;

    retVal.daysOfWeek &= latestDaysMask;

    return retVal;
};

/**
 * Compare two WeekTime objects using the latest day of week for each as the basis
 *
 * @param {WeekTime} t0
 * @param {WeekTime} t1
 * @returns {number} (strcmp-like)
 *   t0 before t1: < 0
 *   t0 after t1: > 0
 *   t0 == t1: 0
 */
WeekTime.compare = function(t0, t1) {
    var SECS_IN_DAY = 86400;
    var SUNDAY = 0;
    var SATURDAY = 6;

    var diff;
    var latestDayT0 = SUNDAY;
    var latestDayT1 = SUNDAY;

    for (var i = SUNDAY; i <= SATURDAY; i++) {
        if ((t0.daysOfWeek >> i & 1) !== 0) {
            latestDayT0 = i;
        }

        if ((t1.daysOfWeek >> i & 1) !== 0) {
            latestDayT1 = i;
        }
    }

    diff = t0.seconds - t1.seconds;
    diff += (latestDayT0 - latestDayT1) * SECS_IN_DAY;

    return diff;
};

/**
 * Container class for an ordered (non-descending by time) set of schedules for a given system mode (heat/cool).
 * @param schedules
 * @constructor
 */
function ScheduleMode(schedules) {
    this.conditions = schedules;
}

/**
 * Find the latest transition point that occurred before now.
 * @param mode
 * @param now
 * @returns {{which: number, actions: *}} The current schedule entry index and its actions
 */
function processSchedules(mode, now) {
    var which = -1;
    var lastEntryIndex = -1;
    var latestEntry = null;
    var numConditions = mode.conditions.length;

    for (var i = 0; i < numConditions; i++) {
        var entry = new WeekTime(mode.conditions[i]);
        var entryForNow = entry.withDaysUpTo(now);

        if (lastEntryIndex === -1 || WeekTime.compare(entry, mode.conditions[lastEntryIndex]) >= 0) {
            lastEntryIndex = i;
        }

        if (entryForNow.daysOfWeek === 0) {
            continue;
        }

        var diff = WeekTime.compare(now, entryForNow);

        /*
         * WeekTimes are compared on the basis of their highest order day bit. When today is included in the schedule,
         * a transition may appear to be in the future: backtrack to the most recent day before now to find the most
         * recent transition.
         */
        if (diff < 0) {
            entryForNow.daysOfWeek >>= 1;
            if (entryForNow.daysOfWeek !== 0) {
                diff = WeekTime.compare(now, entryForNow);
            }
        }

        if (diff >= 0 && (which === -1 || WeekTime.compare(entryForNow, latestEntry) >= 0)) {
            which = i;
            latestEntry = entryForNow;
        }
    }

    /*
     * If there are no transitions before now, backtrack to the last entry on the schedule (i.e., the latest transition
     * for the previous week).
     */
    if (which === -1 && lastEntryIndex !== -1) {
        which = lastEntryIndex
    }

    return {
        actions: (which === -1) ? [] : mode.conditions[which].actions,
        "which": which
    };
}

var now = new WeekTime({ seconds: _.bindings['?_evTime']/1000 });

/* C injects literal JS schedule spec objects here via %s format specifiers */
if (!('persist' in _.bindings)) {
    _.bindings['persist'] = {
        'cool': new ScheduleMode(%s),
        'heat': new ScheduleMode(%s)
    };
}

var persistCool = _.bindings['persist']['cool'];
var persistHeat = _.bindings['persist']['heat'];

var cool = processSchedules(persistCool, now);
var heat = processSchedules(persistHeat, now);

/*
 * Always resume the schedule when turning off hold. When the schedule is edited or created, the machine is recreated
 * and in an initial state, so it will always emit the current scheduled settings after those actions.
 * Otherwise, don't re-set the same schedule until a transition point actually passes. This has the added
 * benefit of working like a temporary hold, allowing manual changes between transitions without having to resort
 * to permanent hold.
 */
if (_.bindings['?holdOn'] === "false" || persistCool.which !== cool.which || persistHeat.which !== heat.which) {
    _.out(cool.actions.concat(heat.actions));
    persistCool.which = cool.which;
    persistHeat.which = heat.which;
}

return _.bindings;
//}({ bindings: bs, out: function(x) {emitting.push(x)} })