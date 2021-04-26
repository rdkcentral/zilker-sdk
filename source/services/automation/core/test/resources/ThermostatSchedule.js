{
    "sheensVersion": 1,
    "name": "ThermostatSchedule",
    "nodes": {
    "start": {
        "branching": {
            "type": "message",
                "branches": [
                {
                    "pattern": {
                        "_evCode": 499,
                        "_evId": "??_evId",
                        "_evTime": "?_evTime",
                        "_sunrise": "?_sunrise",
                        "_sunset": "?_sunset",
                        "_systemStatus": "?_systemStatus"
                    },
                    "target": "constraints"
                },
                {
                    "pattern": {
                        "_evCode": 400,
                        "_evId": "??_evId",
                        "_evTime": "?_evTime",
                        "_sunrise": "?_sunrise",
                        "_sunset": "?_sunset",
                        "_systemStatus": "?_systemStatus",
                        "automationEvent": {
                            "ruleId": 1017206
                        }
                    },
                    "target": "constraints"
                },
                {
                    "pattern": {
                        "_evCode": 402,
                        "_evId": "??_evId",
                        "_evTime": "?_evTime",
                        "_sunrise": "?_sunrise",
                        "_sunset": "?_sunset",
                        "_systemStatus": "?_systemStatus",
                        "automationEvent": {
                            "ruleId": 1017206
                        }
                    },
                    "target": "constraints"
                },
                {
                    "pattern": {
                        "_evCode": 303,
                        "_evId": "??_evId",
                        "_evTime": "?_evTime",
                        "_sunrise": "?_sunrise",
                        "_sunset": "?_sunset",
                        "_systemStatus": "?_systemStatus",
                        "DeviceServiceResourceUpdatedEvent": {
                            "resource": {
                                "DSResource": {
                                    "id": "holdOn",
                                    "ownerClass": "thermostat",
                                    "value": "?holdOn"
                                }
                            }
                        }
                    },
                    "target": "constraints"
                }
            ]
        }
    },
    "actions": {
        "action": {
            "interpreter": "ecmascript-5.1",
                "source": "function WeekTime(weekTime) {\n this.daysOfWeek = 0;\n this.seconds = 0;\n if (weekTime.daysOfWeek !== null && weekTime.daysOfWeek !== undefined) {\n this.daysOfWeek = weekTime.daysOfWeek;\n if (weekTime.seconds !== null && weekTime.seconds !== undefined) {\n this.seconds = weekTime.seconds;\n }\n } else if (weekTime.seconds !== null && weekTime.seconds !== undefined) {\n var now = new Date(weekTime.seconds * 1000);\n this.daysOfWeek = 1 << now.getDay();\n this.seconds = (now.getHours() * 60 * 60) + (now.getMinutes() * 60);\n }\n}\nWeekTime.prototype.withDaysUpTo = function(weekTime) {\n var retVal = new WeekTime(this);\n var latestDay = 0;\n var latestDaysMask;\n var tmp = weekTime.daysOfWeek;\n while ((tmp >>= 1) !== 0) {\n latestDay++;\n }\n latestDaysMask = 1 << latestDay;\n latestDaysMask |= latestDaysMask - 1;\n retVal.daysOfWeek &= latestDaysMask;\n return retVal;\n};\nWeekTime.compare = function(t0, t1) {\n var SECS_IN_DAY = 86400;\n var SUNDAY = 0;\n var SATURDAY = 6;\n var diff;\n var latestDayT0 = SUNDAY;\n var latestDayT1 = SUNDAY;\n for (var i = SUNDAY; i <= SATURDAY; i++) {\n if ((t0.daysOfWeek >> i & 1) !== 0) {\n latestDayT0 = i;\n }\n if ((t1.daysOfWeek >> i & 1) !== 0) {\n latestDayT1 = i;\n }\n }\n diff = t0.seconds - t1.seconds;\n diff += (latestDayT0 - latestDayT1) * SECS_IN_DAY;\n return diff;\n};\nfunction ScheduleMode(schedules) {\n this.conditions = schedules;\n}\nfunction processSchedules(mode, now) {\n var which = -1;\n var lastEntryIndex = -1;\n var latestEntry = null;\n var numConditions = mode.conditions.length;\n for (var i = 0; i < numConditions; i++) {\n var entry = new WeekTime(mode.conditions[i]);\n var entryForNow = entry.withDaysUpTo(now);\n if (lastEntryIndex === -1 || WeekTime.compare(entry, mode.conditions[lastEntryIndex]) >= 0) {\n lastEntryIndex = i;\n }\n if (entryForNow.daysOfWeek === 0) {\n continue;\n }\n var diff = WeekTime.compare(now, entryForNow);\n \n if (diff < 0) {\n entryForNow.daysOfWeek >>= 1;\n if (entryForNow.daysOfWeek !== 0) {\n diff = WeekTime.compare(now, entryForNow);\n }\n }\n if (diff >= 0 && (which === -1 || WeekTime.compare(entryForNow, latestEntry) >= 0)) {\n which = i;\n latestEntry = entryForNow;\n }\n }\n \n if (which === -1 && lastEntryIndex !== -1) {\n which = lastEntryIndex\n }\n return {\n actions: (which === -1) ? [] : mode.conditions[which].actions,\n \"which\": which\n };\n}\nvar now = new WeekTime({ seconds: _.bindings['?_evTime'] });\nif (!('persist' in _.bindings)) {\n _.bindings['persist'] = {\n 'cool': new ScheduleMode([{\"daysOfWeek\":65,\"seconds\":18000,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/coolSetpoint\",\"value\":\"2222\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":65,\"seconds\":25200,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/coolSetpoint\",\"value\":\"2333\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":65,\"seconds\":59400,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/coolSetpoint\",\"value\":\"2333\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":127,\"seconds\":75600,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/coolSetpoint\",\"value\":\"2167\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":62,\"seconds\":18000,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/coolSetpoint\",\"value\":\"2056\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":2,\"seconds\":21600,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/coolSetpoint\",\"value\":\"2551\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":62,\"seconds\":59400,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/coolSetpoint\",\"value\":\"2222\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":60,\"seconds\":21600,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/coolSetpoint\",\"value\":\"2555\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]}]),\n 'heat': new ScheduleMode([{\"daysOfWeek\":127,\"seconds\":18000,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/heatSetpoint\",\"value\":\"2111\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":65,\"seconds\":28800,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/heatSetpoint\",\"value\":\"2096\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":127,\"seconds\":59400,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/heatSetpoint\",\"value\":\"2111\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":1,\"seconds\":75600,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/heatSetpoint\",\"value\":\"2113\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":62,\"seconds\":28800,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/heatSetpoint\",\"value\":\"1666\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":62,\"seconds\":75600,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/heatSetpoint\",\"value\":\"1666\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]},{\"daysOfWeek\":64,\"seconds\":75600,\"actions\":[{\"jsonrpc\":\"2.0\",\"method\":\"writeDeviceResource\",\"params\":{\"uri\":\"/002446000006099b/ep/*/r/heatSetpoint\",\"value\":\"2100\",\"actionSuppressResourceURI\":\"/002446000006099b/ep/*/r/holdOn\"}}]}])\n };\n}\nvar persistCool = _.bindings['persist']['cool'];\nvar persistHeat = _.bindings['persist']['heat'];\nvar cool = processSchedules(persistCool, now);\nvar heat = processSchedules(persistHeat, now);\nif (_.bindings['?holdOn'] === \"false\" || persistCool.which !== cool.which || persistHeat.which !== heat.which) {\n _.out(cool.actions.concat(heat.actions));\n persistCool.which = cool.which;\n persistHeat.which = heat.which;\n}\nreturn _.bindings;\n"
        },
        "branching": {
            "branches": [
                {
                    "target": "reset"
                }
            ]
        }
    },
    "constraints": {
        "branching": {
            "branches": [
                {
                    "target": "actions"
                }
            ]
        }
    },
    "reset": {
        "action": {
            "interpreter": "ecmascript-5.1",
                "source": "return ('persist' in _.bindings) ? {'persist': _.bindings['persist']} : {};\n"
        },
        "branching": {
            "branches": [
                {
                    "target": "start"
                }
            ]
        }
    }
}
}