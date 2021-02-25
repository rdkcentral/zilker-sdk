<#include "ruleHeader.ftl">
<ns2:scheduleEntry thermostatID="${thermostatId}">
    <ns2:timeSlot>
        <ns2:exactTime>${leaveTimeString}</ns2:exactTime>
    </ns2:timeSlot>
    <ns2:mode>heat</ns2:mode>
    <ns2:temperature>${leaveHeatTemp?c}</ns2:temperature>
</ns2:scheduleEntry>
<ns2:scheduleEntry thermostatID="${thermostatId}">
    <ns2:timeSlot>
        <ns2:exactTime>${leaveTimeString}</ns2:exactTime>
    </ns2:timeSlot>
    <ns2:mode>cool</ns2:mode>
    <ns2:temperature>${leaveCoolTemp?c}</ns2:temperature>
</ns2:scheduleEntry>
<ns2:scheduleEntry thermostatID="${thermostatId}">
    <ns2:timeSlot>
        <ns2:exactTime>${returnTimeString}</ns2:exactTime>
    </ns2:timeSlot>
    <ns2:mode>heat</ns2:mode>
    <ns2:temperature>${returnHeatTemp?c}</ns2:temperature>
</ns2:scheduleEntry>
<ns2:scheduleEntry thermostatID="${thermostatId}">
    <ns2:timeSlot>
        <ns2:exactTime>${returnTimeString}</ns2:exactTime>
    </ns2:timeSlot>
    <ns2:mode>cool</ns2:mode>
    <ns2:temperature>${returnCoolTemp?c}</ns2:temperature>
</ns2:scheduleEntry>
</ns2:rule>