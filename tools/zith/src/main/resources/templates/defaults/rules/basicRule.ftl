<#include "ruleHeader.ftl">
<ns2:triggerList<#if negative??> isNegative="true"</#if>>
    ${triggerText}
</ns2:triggerList>
${actionText}
${constraintText}
<ns2:description>Basic Rule</ns2:description>
</ns2:rule>