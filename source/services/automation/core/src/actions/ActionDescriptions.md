# Table of Contents

1. [Overview](#overview)
2. [Action Handlers](#action-handlers)
    1. [Device](#device)
    2. [System](#system)
    3. [Notification](#notification)
    4. [Timer](#timer)
    5. [Camera](#camera)
    6. [IPC](#ipc)

# Overview

All action handlers may have different action JSON entries that
they require. This document attempts to describe the action JSON
and the response JSON for each core action handler. If an action
handler is created, and registered, outside of this core directory
then it is up to the implementer to appropriately document said
action handler.

# Action Handlers

## Device

The device action handler communicates directly with `deviceService`
which contains all major devices (i.e. cameras, lights, sensors, etc.)
in the system. A device may be read, written to, or have a part of the
device "executed". Only a single resource may be interacted with
in a single action.

### Action JSON

```json
{
    "to": "device",
    "requestType": "readResource" | "writeResource" | "executeResource",
    "uri": "/<uuid>/ep/*/<resource>",
    "value": "<string>"
}
```

### Response JSON

```json
{
    "success": <bool>,
    "type": "writeResourceResponse" | "executeResourceResponse",
}
```

```json
{
    "success": <bool>,
    "type": "readResourceResponse",
    "value": "<string>"
}
```

## System

The system action handler allows for the core "system" to be
interacted with. This may include changing the system state (ex. alarm
state), or playing audio. Anything that potentially modifies the
overall system itself should belong with the system action handler.

### Action JSON

```json
{
    "to": "system",
    "requestType": "security" | "scene" | "audio",
    "name": "<string>"
}
```

For `audio` entries the name represents the audio clip to be
played.

### Response JSON

```json
{
    "success": <bool>,
    "type": "systemResponse"
}
```

## Notification

The notification action handler allows for SMS and email messages
to be sent to users of the system. A notification may be associated
with another event so that users know why the notification was
created. An attachment of type *either* pictures, *or* video are
allowed.

### Action JSON

```json
{
    "to": "notification",
    "requestType": "sms" | "email",
    "eventId": null | <number>,

    "time": null | <number>,
    "attachment": null | "picture" | "video"
}
```

The `eventId` field allows for a notification to be associated
with a specific event. If no event ID is present then an event
ID of zero will be used, and it will be left to receivers of the
action to determine how to handle the scenario.

The fields `time` and `attachment` are optional. Thus the may
be present, or `null`.

If the `time` field is present, and non-null then it reflects
the time the event that caused the action was created.

If the `attachment` field is present, and non-null, then the
notification will be associated with a specific picture, or
video.

> Note: In the current implementation we *must* wait for the
> picture, or video, to be generated and sent to the server.

### Response JSON

```json
{
    "success": <bool>,
    "type": "notificationResponse"
}
```

## Timer

A timer allows for a new event stimuli to be generated at either after
specific amount time, or on a recurring basis through a cron like
interface.

Timers may be canceled (deleted) any time before they fire. A timer
that is configured for a specific amount of time will be considered
a "one-shot" timer, and will not fire again.

> Note: Repeating specific time timers may be allowed in the future.

### Action JSON

```json
{
    "to": "timer",
    "requestType": "makeTimer",
    "id": "<string>",
    "message": "<string>",

    "in": <number>,
    "cron": "* * * * * *",
}
```

The `id` field will be used as the identifier when a timer fires,
or for identifying which timer to delete.

When a timer fires both the ID and `message` will be sent as a
new stimulus into the automation system.

The `in` and `cron` fields are mutually exclusive.

The `in` field informs the timer to fire after a specific number of
seconds have passed.

The `cron` field provides for repeat-able cron like timers. It is
important to note that the cron line is *not* exactly the same
formatting as cron as a sixth field is provided for *seconds*.

The following is an excerpt from the `crontab` man page:

```
The time and date fields are:
        field          allowed values
        -----          --------------
        minute         0-59
        hour           0-23
        day of month   1-31
        month          1-12 (or names, see below)
        day of week    0-7 (0 or 7 is Sunday, or use names)

 A field may contain an asterisk (*), which always stands for
 "first-last".

 Ranges of numbers are allowed.  Ranges are two numbers separated with
 a hyphen.  The specified range is inclusive.  For example, 8-11 for
 an 'hours' entry specifies execution at hours 8, 9, 10, and 11. The
 first number must be less than or equal to the second one.

 Lists are allowed.  A list is a set of numbers (or ranges) separated
 by commas.  Examples: "1,2,5,9", "0-4,8-12".

 Step values can be used in conjunction with ranges.  Following a
 range with "/<number>" specifies skips of the number's value through
 the range.  For example, "0-23/2" can be used in the 'hours' field to
 specify command execution for every other hour (the alternative in
 the V7 standard is "0,2,4,6,8,10,12,14,16,18,20,22").  Step values
 are also permitted after an asterisk, so if specifying a job to be
 run every two hours, you can use "*/2".

 Names can also be used for the 'month' and 'day of week' fields.  Use
 the first three letters of the particular day or month (case does not
 matter).  Ranges or lists of names are not allowed.

 Note: The day of a command's execution can be specified in the
 following two fields — 'day of month', and 'day of week'.  If both
 fields are restricted (i.e., do not contain the "*" character), the
 command will be run when either field matches the current time.  For
 example,
 "30 4 1,15 * 5" would cause a command to be run at 4:30 am on the 1st
 and 15th of each month, plus every Friday.
```

Action for removing an active timer.

```json
{
    "to": "timer",
    "requestType": "deleteTimer",
    "id": "<string>",
}
```

The `id` *must* identify an existing timer.

### Response JSON

```json
{
    "success": <bool>,
    "type": "timerResponse"
}
```

## Camera

The camera action forces the system to take either a picture (multiple
allowed), or a video clip. An event ID *may* be provided in order
to associate the picture/video with another event.

### Action JSON

The following JSON fields are *required* for both picture and video
actions:

```json
{
    "to": "camera",
    "requestType": "picture",
    "cameraId": "<string>",
    "eventId": null | <number>,
}
```

The `cameraId` field informs the system exactly which camera is to
create pictures, or video.

The `eventId` field allows for a picture/video to be associated
with a specific event. If no event ID is present then an event
ID of zero will be used, and it will be left to receivers of the
action to determine how to handle the scenario.

The following field *must* be provided *only* for pictures.

```json
{
    "count": <number>
}
```

The `count` field informs the camera how many pictures in series
*must* be created.

The following field *must* be provided *only* for video.

```json
{
    "duration": <number>
}
```

The `duration` field informs the camera how long, in total, the
video clip should be. The duration *must* be provided in seconds.

### Response JSON

```json
{
    "success": <bool>,
    "type": "cameraResponse"
}
```

## IPC

> Note: IPC is very specific to the internals of the xFi Home device,
> and thus should be used judiciously.

At times it may be necessary to communicate directly with another
service running in the system as an action rather than explicit
code. The IPC action provides the ability to send requests, with
data, to another service in the system and then to allow the
response to be broadcast back through the automation service.

### Action JSON

```json
{
    "to": "ipc",
    "requestType": "sendIpc",
    "port": <number>,
    "msgCode": <number>,

    "payload": <object>
}
```

The `port` field is used to specify exactly which service is to
be communicated with. All services work off of the the same IP
address, but on served on different ports. The `port` field *must*
be provided.

The `msgCode` field specifies which IPC message is to be sent to
the internal server. The `msgCode` field *must be provided.

The `payload` field *may* be provided. If provided then a JSON
object that is *standard* for the specific service *must* be provided.

### Response JSON

```json
{
    "success": <bool>,
    "type": "sendIpcResponse",

    "payload": <object>
}
```

The `payload` field *may* be provided in the response. If the
`payload` is provided then a JSON object *must* also be provided.
The JSON object will be all data that was outputted by the IPC
message with the service.
