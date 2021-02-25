# Security Service Architecture

The *security service* is a combination of three legacy "managers"
1.  Zone
2.  Trouble
3.  Alarm

The original design had each as a separate process, but that caused timing issues
as most zone/alarm events need to be tightly coupled together before consumption 
by outside services.  For example, a zone tamper could cause an alarm.  The tamper
event essentially escalates into an alarm.  If the timing of those event broadcasts 
is not correct, outside listeners could incorrectly report the alarm and reason.

## Zone Management

Logically extends Sensor objects with SecurityZone objects, adding attributes that  
are specific to zones:
* zoneNumber
* zoneType 
* zoneFunction
* zoneMute
 
The zone information is stored as "meta-data" within the sensor.  This means that all 
persisted information about a zone is physically stored within the [device](source/services/device/README.md) 
service.

Listens for sensor events from *device service* and will react accordingly from a 
zone perspective; most will create and brodcast a *security zone event*:
* create
* update (label, bypass, type/function)
* delete
* fault/restore

**NOTE:** Prior to broadcasting a *security zone event*, it will be passed to the *alarm state 
machine* for potential action.


## Trouble Management

Similar to zone, this will listen to sensor events and look for "troubles".  It will
also listen for *platform service* events to create system troubles (power, tamper, etc).

When troubles occur or are clear, it will broadcast those *trouble event* to the system.

**NOTE:** Prior to broadcasting a *trouble event*, it will be passed to the *alarm state 
machine* for potential action.


## Alarm State Machine

This is currently a **TODO** stub to allow invention of an *alarm state machine*
