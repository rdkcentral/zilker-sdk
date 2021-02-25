<timeTrigger>
    <description>Time Trigger</description>
    <category>time</category>
    <when>
        <exactTime>${startTimeString}</exactTime>
    </when>
    <#if endTimeString??>
    <end>
        <exactTime>${endTimeString}</exactTime>
    </end>
    </#if>
    <#if repeatIntervalMinutes??>
    <repeat>${repeatIntervalMinutes}</repeat>
    </#if>
</timeTrigger>