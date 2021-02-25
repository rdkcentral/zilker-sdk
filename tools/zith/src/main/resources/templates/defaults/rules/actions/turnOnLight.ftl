<ns2:action>
    <ns2:actionID>70</ns2:actionID>
    <ns2:parameter>
        <ns2:key>lightID</ns2:key>
        <ns2:value>${lightId}</ns2:value>
    </ns2:parameter>
    <ns2:parameter>
        <ns2:key>level</ns2:key>
        <ns2:value>${lightLevel!"100"}</ns2:value>
    </ns2:parameter>
    <#if durationSeconds??>
    <ns2:parameter>
        <ns2:key>duration</ns2:key>
        <ns2:value>${durationSeconds}</ns2:value>
    </ns2:parameter>
    </#if>
</ns2:action>