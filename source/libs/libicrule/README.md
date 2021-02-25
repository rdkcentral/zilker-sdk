# Table of Contents

1. [Overview](#overview)
2. [Rule Legacy Blocks](#rule-legacy-blocks)
    1. [Triggers](#triggers)
    2. [Constraints](#constraints)
    3. [Actions](#actions)
    4. [Schedules](#schedules)
3. [Getting Started](#getting-started)
  
# Overview

This library is designed to parse legacy iControl Rules Engine XML syntax
into a common data driven model that any application may utilize. The library
breaks any Rules XML into four potential blocks: Triggers, Schedules, 
Constraints, and Actions. Schedules do *not* contain actions as any action
is implied by the schedule, however it *may* contain constraints.  

# Rule Legacy Blocks

## Triggers

Triggers describe some event that may drive a rule to a set
of actions. There may be more than one trigger provided in a
trigger list. When more than one trigger is provided any trigger
may cause an action to occur.  

Example 1:  
A door sensor opens, or closes, between 8am-12pm on Mondays then
turn light A on for 5 minutes then turn the light off.

Trigger: Door sensor opens or closes.  
Constraint: between 8am-12pm on Mondays only.  
Action: Turn light A on for 5 minutes then turn the light off.

Example 2:
Thermostat A should set cool point to 70F at 5pm everyday.

Schedule Time: 5pm everyday  
Schedule Mode: Cool  
Schedule Action: Change set point to 70F
    
A trigger list may have two modifiers applied to the entire
list.

* ``delay``
* ``negative``

When a trigger list is marked for delay <comeback here!>

A trigger list marked as "negative" completely changes the
meaning of the triggers. In this situation the *rule* itself is
actually the **constraints**, and then the triggers are verified
after the constraint has been met.

Example:  
A negative rule to fire a trigger when a door opens only
between 8:00am and 12:00pm on Mondays.

Without the negative modifier the normal flow would be 
when a door opens check the constraint of 8-12 on Monday then
fire the action. With the negative modifier the rule changes 
to "if we are between 8-12 on Monday *and* a door opens". Thus
the trigger is now the time period 8-12 on Mondays and the 
constraint is "the door opens".    

## Constraints

A constraint limits a trigger to a specific set of events that
must be valid in order to progress to the action phase.

Available constraints:  
* Time based
* System mode

Constraints may be logically AND-ed and/or OR-ed together.
This allows nesting of constraints.

Time based constraints allow for triggers to be constrained
to specific days of the week, as well as, time of day. The 
time of day may be dynamically tied to sunrise and sunset, or
exact time by specifying the hour and minute.

System mode constraints allow for triggers to be constrained
to the current system state. Thus for *scene* based systems
the system mode will be based on the current scene. (ex. 
away, stay, night, vacation, etc.) For security based systems the
mode will be based on the current armed state. (ex. armed night,
armed stay, disarmed, etc.)

## Actions

Actions describe *what* should be done after a trigger has occurred.

Currently supported *common* actions:  
* Send Email/SMS
  * With, or without, attachments
* Take Pictures
  * Will take a set number of photos (default 5)
* Take Video clip
  * 15s of video (5s pre-event, 10s post event)
* Change light state
  * Turn light on
  * Turn light off
  * Turn light randomly on/off
  * May turn light on/off after 'x' minutes
* Lock/Unlock door
* Change thermostat state
  * Set to Cool mode
  * Set to Heat mode
  * Turn thermostat off

Currently supported *security* actions:  
* Change security state
  * Disarm
  * Arm (away | stay | night)
* Play audio sound

Currently supported *non-security* actions:
* Change scene state
  * Home
  * Away
  * Night
  * Vacation

Currently supported *UI* actions:
* Display widget/application

## Schedules

A schedule allows for direct control of a thermostat depending
on current thermostat mode of operation. A schedule consists of
a day of the week (may be all, or one) the temperature should be
adjusted, and a specific time of day. The time of day may be
explicitly specified via hour and minute, or it may be dynamically
assigned based on sunset, and sunrise, times. Each schedule will
be for an assigned mode of operation: either Cool or Heat. If
the thermostat is actively in the specified mode of operation then
the schedule will take effect. Each thermostat schedule will have
a single temperature set point that the thermostat will be set to.
The changing of the temperature directly correlates to an action.

# Getting Started

The legacy rules system utilizes an external set of XML files 
in order to define supported actions and their parameters. 
Thus in order to get started the ``libicrule`` library
must first be informed where to find these files.

```c
#include <icrule/icrule.h>
    
icrule_set_action_list_dir("/my/xml/dir");
```

If the action list directory is **not** specified then the 
current working directory at execution time will be used.

Once the action list directory has been configured legacy
rules may be parsed from either a file, or directly from
memory.

```c
icrule_t my_rule;

icrule_parse("in-memory XML", &my_rule);
icrule_parse_file("/path/filename.xml", &my_rule);
```

Once the rule has finished being utilized it must
be destroyed. This will release all memory that was
allocated for the rule. Any attempt to use the
rule after being destroyed will be undefined.

```c
icrule_destroy(&my_rule);
```

