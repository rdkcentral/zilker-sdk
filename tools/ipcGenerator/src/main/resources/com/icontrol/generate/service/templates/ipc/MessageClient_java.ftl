/**
 * ${message.description}
<#if message.inParm??>
 * @param ${message.inParm.field.name} ${(message.inParm.field.description)!""}
</#if>
<#if message.outParm??>
 * @return ${(message.outParm.field.description)!""}
</#if>
 * @throws IPCException if the IPC call fails
 */
public static ${(message.outParm.field.javaType)!"void"} ${message.javaName}(<#if message.inParm??>${message.inParm.field.javaType} ${message.inParm.field.name}</#if>) throws IPCException
{
     <#if message.outParm??>return </#if>${message.javaName}(<#if message.inParm??>${message.inParm.field.name}, </#if>${message.readTimeout});
}

/**
 * ${message.description}
<#if message.inParm??>
 * @param ${message.inParm.field.name} ${(message.inParm.field.description)!""}
</#if>
 * @param timeoutSecs Timeout for IPC response in seconds
<#if message.outParm??>
 * @return ${(message.outParm.field.description)!""}
</#if>
 * @throws IPCException if the IPC call fails
 */
public static ${(message.outParm.field.javaType)!"void"} ${message.javaName}(<#if message.inParm??>${message.inParm.field.javaType} ${message.inParm.field.name}, </#if>int timeoutSecs) throws IPCException
{
    try
    {
        // create our input object
        //
        <#if message.inParm??>
            <#if message.inParm.field.custom>
        ${message.inParm.field.javaIpcObjectType} inArg = ${message.inParm.field.name};
            <#else>
        ${message.inParm.field.javaIpcObjectType} inArg = new ${message.inParm.field.javaIpcObjectType}(${message.inParm.field.name});        
            </#if>
        </#if>

        <#if message.outParm??>
        // create our output object
        //
        ${message.outParm.field.javaIpcObjectType} output = new ${message.outParm.field.javaIpcObjectType}();
        </#if>

        // send the request
        //
        IPCMessage msg = IPCSender.sendRequest(
            ${java_codes_interface}.${server_port_name}_PORT_NUM,
            ${java_codes_interface}.${message.name},
            <#if message.inParm??>inArg<#else>null</#if>,
            <#if message.outParm??>output<#else>null</#if>,
            timeoutSecs
        );
        if (msg != null && msg.getReturnCode() == 0)
        {
            <#if message.outParm??>
                <#if message.outParm.field.custom>
            return output;
                <#else>
            return output.getValue();
                </#if>
            <#else>
            return;
            </#if>
        }
        else
        {
            throw new IPCException("IPC call of ${message.name} returned " + (msg == null ? "null" : msg.getReturnCode()));
        }
    }
    catch (IOException ex)
    {
        throw new IPCException("error running ${message.name}", ex);
    }
}
