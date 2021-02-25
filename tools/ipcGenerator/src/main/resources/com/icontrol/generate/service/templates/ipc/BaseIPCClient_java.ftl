${header}

package ${java_pkg};

//-----------
//imports
//-----------
import com.icontrol.ipc.*;
import com.icontrol.ipc.impl.*;

import java.io.IOException;
import java.util.*;

//-----------------------------------------------------------------------------
// Client definition:    ${java_class}
/**
 *  Define client methods to ${service}
 *
 *  THIS IS AN AUTO-GENERATED FILE, DO NOT EDIT!!!!!!!!
 **/
//-----------------------------------------------------------------------------
public class ${java_class}
{
${java_free_form}

<#list wrappers as wrapper>
<#if wrapper.message.supportsJava()>
    <#assign message = wrapper.message>
    <#include "MessageClient_java.ftl">
</#if>
</#list>

}

//+++++++++++
//EOF
//+++++++++++
