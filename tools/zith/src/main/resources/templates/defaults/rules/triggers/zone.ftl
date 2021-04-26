<ns2:zoneTrigger>
    <ns2:description>Zone Trigger</ns2:description>
    <ns2:category>sensor</ns2:category>
    <ns2:zoneState>${zoneState}</ns2:zoneState>
    <#if zoneId??>
    <ns2:zoneID>${zoneId}</ns2:zoneID>
    </#if>
    <#if zoneType??>
    <ns2:zoneType>${zoneType}</ns2:zoneType>
    </#if>
</ns2:zoneTrigger>