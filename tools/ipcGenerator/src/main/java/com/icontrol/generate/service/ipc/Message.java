/*
 * Copyright 2021 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.icontrol.generate.service.ipc;

//-----------
//imports
//-----------

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

import com.icontrol.generate.service.TemplateMappings;
import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.generate.service.io.CodeFormater;
import com.icontrol.generate.service.io.FileHelper;
import com.icontrol.generate.service.io.MacroSource;
import com.icontrol.generate.service.parse.EventWrapper;
import com.icontrol.generate.service.parse.PojoWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.generate.service.variable.StringVariable;
import com.icontrol.util.StringUtils;
import com.icontrol.xmlns.service.ICMessage;
import com.icontrol.xmlns.service.ICMessageParm;
import com.icontrol.xmlns.service.ICScope;

//-----------------------------------------------------------------------------
// Class definition:    Message
/**
 *  Model a request that can be sent to the server.  Generate C and/or Java code
 *  for encoding the parameters, sending the request, receiving the reply, and decoding the results
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class Message implements Comparable<Message>
{
    private final static String NATIVE_BODY_TEMPLATE   = "/com/icontrol/generate/service/templates/ipc/native_message.c";
    private final static String NATIVE_HANDLE_TEMPLATE = "/com/icontrol/generate/service/templates/ipc/native_handle_msg.c";

    private ICMessage       node;
    private MessageParam    inParm;
    private MessageParam    outParm;

    // C functions to generate
    //
    private CFunction sendRequestFunction;      // client-side
    private CFunction sendRequestTimeoutFunction;
    private CFunction handleRequestFunction;    // service-side
    private boolean   needsPojoHeader;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public Message(ICMessage bean)
    {
        node = bean;
        
        // see if we have input parameters
        //
        ICMessageParm input = node.getInput();
        if (input != null)
        {
            // give it the name of 'input'
            //
            inParm = new MessageParam(input);
            inParm.setName("input");
        }
        
        // see if we have output parameters
        //
        ICMessageParm output = node.getOutput();
        if (output != null)
        {
            // give it the name of 'output'
            //
            outParm = new MessageParam(output);
            outParm.setName("output");
        }
    }

    //--------------------------------------------
    /**
     * Used to sort by "code"
     */
    //--------------------------------------------
    @Override
    public int compareTo(Message message)
    {
        if (message != null)
        {
            if (getCode() < message.getCode())
            {
                return -1;
            }
            else if (getCode() > message.getCode())
            {
                return 1;
            }
        }
        return 0;
    }

    //--------------------------------------------
    /**
     * get the name of the message that can be sent to the server
     */
    //--------------------------------------------
    public String getName()
    {
        return node.getName();
    }
    
    //--------------------------------------------
    /**
     * the numeric code that represents this message
     */
    //--------------------------------------------
    public int getCode()
    {
        return node.getId();
    }
    
    //--------------------------------------------
    /**
     * comments about the message
     */
    //--------------------------------------------
    public String getDescription()
    {
        if (node.isSetDescription() == true)
        {
            return node.getDescription().trim();
        }
        return "";
    }
    
    //--------------------------------------------
    /**
     * @return Returns the inParm.
     */
    //--------------------------------------------
    public MessageParam getInParm()
    {
        return inParm;
    }

    //--------------------------------------------
    /**
     * @return Returns the outParm.
     */
    //--------------------------------------------
    public MessageParam getOutParm()
    {
        return outParm;
    }

    //--------------------------------------------
    /**
     * @return Returns the read timeout
     */
    //--------------------------------------------
    public int getReadTimeout()
    {
        return node.getReadTimeout();
    }

    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public boolean supportsC()
    {
        // see if JAVA or BOTH
        //
        ICScope.Enum gen = node.getGenerate();
        if (gen == ICScope.BOTH || gen == ICScope.C)
        {
            return true;
        }
        return false;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public boolean supportsJava()
    {
        // see if JAVA or BOTH
        //
        ICScope.Enum gen = node.getGenerate();
        if (gen == ICScope.BOTH || gen == ICScope.JAVA)
        {
            return true;
        }
        return false;
    }
    
    private StringBuffer createNativeDeclaration(boolean includeTimeoutParm)
    {
        // make the Function declaration using the  input and/or output parms
        //
        StringBuffer dec = new StringBuffer();
        
        // first, the comment describing the function
        //
        dec.append("/**\n");
        if (node.isSetDescription() == true)
        {
            dec.append(" * ");
            dec.append(node.getDescription().trim());
            dec.append('\n');
        }
        if (inParm != null)
        {
            dec.append(" *   "+inParm.getName());
            BaseVariable inField = inParm.getField();
            if (StringUtils.isEmpty(inField.getDescription()) == false)
            {
                dec.append(" - "+inField.getDescription());
            }
            else
            {
                dec.append(" - value to send to the service");
            }
            if (inField.isBoolean() == true)
            {
                dec.append(" (boolean)");
            }
            dec.append('\n');
        }
        if (outParm != null)
        {
            dec.append(" *   "+outParm.getName());
            BaseVariable outField = outParm.getField();
            if (StringUtils.isEmpty(outField.getDescription()) == false)
            {
                dec.append(" - "+outField.getDescription());
            }
            else
            {
                dec.append(" - response from service");
            }
            
            if (outField.isBoolean() == true)
            {
                dec.append(" (boolean)");
            }
            else if (outField.isString() == true)
            {
                // add more to the comment
                //
                dec.append(" (implementation must allocate, and caller must free the string)");
            }
            dec.append('\n');
        }
        if (includeTimeoutParm == true)
        {
            dec.append(" *   readTimeoutSecs - number of seconds to wait for a reply before timing out.\n");
            dec.append(" *                     if 0, there is no timeout on the read\n");
        }
        dec.append(" **/\n");
        
        // now the function declaration
        //
        dec.append("IPCCode ${FUNC_NAME}(");
        if (inParm != null)
        {
            // add input parm
            dec.append(inParm.getField().getCtype()+" ");
            if (inParm.getField().isPtr() == true)
            {
                dec.append('*');
            }
            dec.append(inParm.getName());
            
            if (outParm != null || includeTimeoutParm == true)
            {
                dec.append(", ");
            }
        }
        if (outParm != null)
        {
            // add input parm
            dec.append(outParm.getField().getCtype()+" ");
            if (outParm.getField().isString() == true)
            {
                // add extra pointer so we use:  char **output
                dec.append('*');
            }
            dec.append('*');
            dec.append(outParm.getName());

            if (includeTimeoutParm == true)
            {
                dec.append(", ");
            }
        }
        
        if (includeTimeoutParm == true)
        {
            dec.append("time_t readTimeoutSecs");
        }
        dec.append(')');
        
        return dec;
    }

    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public void calculateNativeFunctions(String family, String ipcCodePrefix, String configFlag)
        throws IOException
    {
        // first the IPC API function...

        // make the global function name (one declared in .h)
        // and the handle function (local to the service).  they
        // are virtually identical (except for the prefix)
        //
        String apiName = family+"_request_"+getName();
        sendRequestFunction = new CFunction(apiName);
        sendRequestTimeoutFunction = new CFunction(apiName+"_timeout");

        String handleName = "handle_"+getName()+"_request";
        handleRequestFunction = new CFunction(handleName);

        StringBuffer dec = createNativeDeclaration(false);
        StringBuffer timeoutDec = createNativeDeclaration(true);

        // sub out $FUNC_NAME and save
        sendRequestFunction.declaration = translateDeclaration(dec, apiName);
        sendRequestTimeoutFunction.declaration = translateDeclaration(timeoutDec, apiName+"_timeout");
        handleRequestFunction.declaration = translateDeclaration(dec, handleName);

        // populate the IPCMessage used for 'input' (even if no input parameters)
        //
        StringBuffer inputbody = createNativeParamEncoding(1, true, false, inParm, "ipcInParm", "inRoot");

        // see if we have output parameters
        //
        StringBuffer outputBody = new StringBuffer();
        if (outParm != null)
        {
            // make the decoder function
            //
            if (outParm.getField().isString() == true)
            {
                // hack for output strings
                //
                StringVariable str = (StringVariable)outParm.getField();
                str.setDoubleDeref();
            }
            createNativeParamDecoding(1, false, false, outParm, outputBody, "ipcOutParm", "outRoot");
        }

        // combine encode input + decode output, saving as the 'timeout' version
        //
        MacroSource macros = new MacroSource();
        macros.addMapping(TemplateMappings.SERVER_PORT_NAME, family.toUpperCase());
        macros.addMapping(TemplateMappings.IPC_CODE_PREFIX, ipcCodePrefix);
        macros.addMapping("service", family);
        macros.addMapping("encode_input", inputbody.toString());
        macros.addMapping("decode_output", outputBody.toString());
        macros.addMapping("message_code", Integer.toString(getCode()));
        macros.addMapping("isTimeout", "Timeout");
        macros.addMapping("readReqTimeoutSecs", ", readTimeoutSecs");
        macros.addMapping(TemplateMappings.C_CONFIG_FLAG, configFlag);

        // now the body of the API function (building from the declaration)
        //
        StringBuffer body = new StringBuffer(sendRequestTimeoutFunction.declaration);
        body.append('\n');
        body.append(FileHelper.translateBufferTemplate(NATIVE_BODY_TEMPLATE, macros));
        sendRequestTimeoutFunction.body = body.toString();

        // have the body of the non-timeout function call the timeout variation,
        // using the default timeout for this message.
        //
        body = new StringBuffer(sendRequestFunction.declaration);
        body.append("\n{\n");
        body.append("    // call 'timeout' variation using the default read timeout of "+node.getReadTimeout()+" seconds\n");
        body.append("    return "+apiName+"_timeout(");
        if (inParm != null)
        {
            body.append("input");
        }
        if (outParm != null)
        {
            if (inParm != null)
            {
                body.append(',');
            }
            body.append("output");
        }
        if (inParm != null || outParm != null)
        {
            body.append(',');
        }
        body.append(node.getReadTimeout()+");\n}\n\n");
        sendRequestFunction.body = body.toString();

        // now for the IPC handler...

        // create local variables for our block
        //
        boolean derefInput = false;     // if input needs to be dereferenced
        boolean derefOutput = false;
        String inputDestroy = null;
        String outputDestroy = null;
        StringBuffer vars = new StringBuffer();
        String pad = getIndentBuffer(3);
        if (inParm != null)
        {
            // get the NativeContext for 'input'
            //
            ContextNative ctx = null;
            PojoWrapper wrapper = Processor.getPojoProcessor().getPojoForName(inParm.getField().getName());
            if (wrapper != null)
            {
                ctx = wrapper.getContextNative();
            }
            vars.append(pad+inParm.getNativeDeclaration(ctx, "input", false));
            vars.append('\n');

            if (inParm.getField().isString() == true)
            {
                // nothing to add, just preventing it from being treated like other object types
                //
            }
            else if (inParm.getField().isPtr() == true)
            {
                // look for custom that contains array/map
                //
                if (inParm.getField().isCustom() == true)
                {
                    inputDestroy = ctx.getDestroyFunctionName();
                }
                else
                {
                    // not dynamic, so clear the memory of it
                    //
                    vars.append(pad + "memset(&input, 0, sizeof(" + inParm.getField().getCtype() + "));\n");
                    derefInput = true;
                }
            }
        }
        if (outParm != null)
        {
            // get the NativeContext for 'input'
            //
            ContextNative ctx = null;
            PojoWrapper wrapper = Processor.getPojoProcessor().getPojoForName(outParm.getField().getName());
            if (wrapper != null)
            {
                ctx = wrapper.getContextNative();
            }
            vars.append(pad+outParm.getNativeDeclaration(ctx, "output", false));
            vars.append('\n');

            if (outParm.getField().isString() == true)
            {
                // deref so we send the address of the "char *"
                //
                derefOutput = true;
            }
            else if (outParm.getField().isPtr() == true)
            {
                // look for custom that contains array/map
                //
                if (outParm.getField().isCustom() == true)
                {
                    outputDestroy = ctx.getDestroyFunctionName();
                }
                else
                {
                    vars.append(pad + "memset(&output, 0, sizeof(" + outParm.getField().getCtype() + "));\n");
                    derefOutput = true;
                }
            }
            else
            {
                // probably a number
                derefOutput = true;
            }
        }

        // make the body of the IPC handler
        //
        StringBuffer handleInput = new StringBuffer();
        StringBuffer handleReply = new StringBuffer();
        if (inParm != null)
        {
            // decode input
            //
            createNativeParamDecoding(3, derefInput, true, inParm, handleInput, "request", "inRoot");
            String indent = getIndentBuffer(1);
            String base = getIndentBuffer(3);
            handleInput.append("\n");
            handleInput.append(base + "if (rc != IPC_SUCCESS)\n");
            handleInput.append(base + "{\n");
            if (inputDestroy != null)
            {
                handleInput.append(indent + base + inputDestroy+"(input);\n");
            }
            else if (inParm != null && inParm.getField().isString() == true)
            {
                handleInput.append(indent + base + "free(input);\n");
            }
            if (outputDestroy != null)
            {
                handleInput.append(indent + base + outputDestroy+"(output);\n");
            }
            else if (outParm != null && outParm.getField().isString() == true)
            {
                handleInput.append(indent + base + "free(output);\n");
            }
            handleInput.append(indent + base + "return rc;\n");
            handleInput.append(base + "}\n");
        }
        if (outParm != null)
        {
            // encode output.  only pass 'true' for the 'dereference' if the flag
            // is set AND this is a pointer field.  otherwise we put * in front of numbers
            // when we shouldn't
            //
            handleReply = createNativeParamEncoding(3, false, derefOutput && outParm.getField().isPtr(), outParm, "response", "outRoot");
        }
        
        // perform "create" cleanup at the bottom
        //
        if (inputDestroy != null)
        {
            handleReply.append("            "+inputDestroy+"(input);\n");
        }
        else if (inParm != null && inParm.getField().isString() == true)
        {
            handleReply.append("            free(input);\n");
        }
        if (outputDestroy != null)
        {
            handleReply.append("            "+outputDestroy+"(output);\n");
        }
        else if (outParm != null && outParm.getField().isString() == true)
        {
            handleReply.append("            free(output);\n");
        }

        // call the 'stub' function
        //
        StringBuffer call = new StringBuffer(getIndentBuffer(3));
        call.append("rc = ");
        call.append(handleRequestFunction.functName);
        call.append('(');
        if (inParm != null)
        {
            // add input parm
            if (derefInput == true)
            {
                // local var, so use address
                call.append('&');
            }
            call.append(inParm.getName());

            if (outParm != null)
            {
                call.append(", ");
            }
        }
        if (outParm != null)
        {
            // need output to come back from the 'handle' function,
            // so always pass the local variable's address
            //
            if (derefOutput == true)
            {
                // local var, so use address
                call.append('&');
            }
            call.append(outParm.getName());
        }
        call.append(");");

        body = new StringBuffer();
        macros = new MacroSource();
        macros.addMapping(TemplateMappings.SERVER_PORT_NAME, family.toUpperCase());
        macros.addMapping(TemplateMappings.IPC_CODE_PREFIX, ipcCodePrefix);
        macros.addMapping("service", family);
        macros.addMapping("handle_vars", vars.toString());
        macros.addMapping("decode_input", handleInput.toString());
        macros.addMapping("encode_output", handleReply.toString());
        macros.addMapping("message_code", getName());
        macros.addMapping("call_api_function", call.toString());
        body.append(FileHelper.translateBufferTemplate(NATIVE_HANDLE_TEMPLATE, macros));
        handleRequestFunction.body = body.toString();
    }

    private String translateDeclaration(StringBuffer dec, String functionName)
    {
        MacroSource src = new MacroSource();
        src.addMapping("FUNC_NAME", functionName);

        return FileHelper.translateBuffer(dec, src);
    }
    
    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private StringBuffer createNativeParamEncoding(int indent, boolean addMsgCode, boolean dereference, MessageParam parm, String parmName, String jsonName)
    {
        StringBuffer encodeBuffer = new StringBuffer();
        String pad = getIndentBuffer(indent);

        if (addMsgCode == true)
        {
            // first, add the message code to the IPCMessage (see native_message.c template)
            //
            encodeBuffer.append(pad + "// set message code for request " + getName() + "\n");
            encodeBuffer.append(pad + parmName + "->msgCode = ${"+TemplateMappings.IPC_CODE_PREFIX +"}");
            encodeBuffer.append(getName());
            encodeBuffer.append(";\n\n");
        }

        if (parm != null)
        {
            // take the variable from the 'parm' and see if this is a POJO
            // ultimately we need to populate the "input IPC message" with the
            // encoded JSON string
            //
            BaseVariable field = parm.getField();
            String fieldName = field.getName();
            String encodeFunction = null;

            ContextNative ctx = null;
            if (field.isEvent() == true)
            {
                // get the wrapper for this event
                //
                EventWrapper wrapper = Processor.getEventProcessor().getEventForName(fieldName);
                if (wrapper != null)
                {
                    // use the encode function already created
                    //
                    encodeFunction = wrapper.getContextNative().getEncodeFunctionName();
                    ctx = wrapper.getContextNative();
                }
            }
            else if (field.isCustom() == true)
            {
                // get the wrapper for this custom
                //
                PojoWrapper wrapper = Processor.getPojoProcessor().getPojoForName(fieldName);
                if (wrapper != null)
                {
                    // use the encode function already created
                    //
                    encodeFunction = wrapper.getContextNative().getEncodeFunctionName();
                    ctx = wrapper.getContextNative();
                }
            }

            // if we have an encode funciton, use it
            //
            StringBuffer body = new StringBuffer();
            if (encodeFunction != null)
            {
                body.append(pad+"// encode "+parm.getName()+" parameter\n");
                body.append(pad+"cJSON *"+jsonName+" = "+encodeFunction+"(");

                if (dereference == true && field.isPtr() == true)
                {
                    // need to deref the variable address
                    body.append('&');
                }
                body.append(parm.getName()+");\n");
            }
            else
            {
                // let the variable do the encode.  Ideally we'd use a StringIPC or something similar
                // but for now this works as those helper objects are really just internal
                //
                body.append(pad+"// encode "+parm.getName()+" parameter\n");
                body.append(pad+"cJSON *"+jsonName+" = cJSON_CreateObject();\n");
                                                                                // TODO: should be parmName?
                body.append(field.getNativeEncodeLines(ctx, indent, jsonName, false, parm.getName(), field.getJsonKey()));
            }

            // now stuff the cJSON into our IPCMessage
            //
            body.append("#ifdef CONFIG_DEBUG_IPC\n");
            body.append(pad+"char *encodeStr = cJSON_Print("+jsonName+");\n");
            body.append("#else\n");
            body.append(pad+"char *encodeStr = cJSON_PrintUnformatted("+jsonName+");\n");
            body.append("#endif\n");
            body.append(pad + "populateIPCMessageWithJSON(" + parmName + ", encodeStr);\n\n");
            body.append(pad+"// mem cleanup\n");
            body.append(pad+"free(encodeStr);\n");
            body.append(pad+"cJSON_Delete("+jsonName+");\n");

            encodeBuffer.append(body);
        }

        return encodeBuffer;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void createNativeParamDecoding(int indent, boolean dereference, boolean isLocalVar, MessageParam parm, StringBuffer buffer, String parmName, String jsonName)
    {
        String pad = getIndentBuffer(indent);
        String pad2 = getIndentBuffer(indent+1);
        String pad3 = getIndentBuffer(indent+2);

        // add variables at the top
        //
        buffer.append(pad+"rc = IPC_GENERAL_ERROR;\n");
        buffer.append(pad+"// parse "+parm.getName()+"\n");
        buffer.append(pad+"//\n");
        buffer.append(pad+"if ("+parmName+"->payloadLen > 0)\n");
        buffer.append(pad+"{\n");
        buffer.append(pad2+"cJSON *item = NULL;\n");
        buffer.append(pad2+"cJSON *"+jsonName+" = cJSON_Parse((const char *)"+parmName+"->payload);\n");
        buffer.append(pad2+"if ("+jsonName+" != NULL)\n");
        buffer.append(pad2+"{\n");

        // now the decode.  if the variable is a Wapper that is setup, use it's decode function
        //
        ContextNative ctx = null;
        String decodeFunction = null;
        BaseVariable field = parm.getField();
        if (field != null)
        {
            String fieldName = field.getName();
            if (field.isEvent() == true)
            {
                // get the wrapper for this event
                //
                EventWrapper wrapper = Processor.getEventProcessor().getEventForName(fieldName);
                if (wrapper != null)
                {
                    // use the encode function already created
                    //
                    decodeFunction = wrapper.getContextNative().getDecodeFunctionName();
                    ctx = wrapper.getContextNative();
                }
            }
            else if (field.isCustom() == true)
            {
                // get the wrapper for this custom
                //
                PojoWrapper wrapper = Processor.getPojoProcessor().getPojoForName(fieldName);
                if (wrapper != null)
                {
                    // use the encode function already created
                    //
                    decodeFunction = wrapper.getContextNative().getDecodeFunctionName();
                    ctx = wrapper.getContextNative();
                }
            }
        }

        if (decodeFunction != null)
        {
            // call the decode function to populate 'output' using cJSON we just created
            //
            buffer.append(pad3);
            buffer.append("rc = " + decodeFunction+"(");
            if (dereference == true && field.isPtr() == true)
            {
                // need to deref the variable address
                buffer.append('&');
            }
            buffer.append(parm.getName()+", "+jsonName+") == 0 ? IPC_SUCCESS : IPC_GENERAL_ERROR;\n");
        }
        else
        {
            // let variable inject the decode logic inline
            //
            //buffer.append(field.getNativeDecodeLines(indent+2, jsonName, false, "*"+parm.getName(), field.getJsonKey()));
            String n = parm.getName();
            if (dereference == false && isLocalVar == false && field.isPtr() == false)
            {
                // need to dereference since input is not a pointer nor a local variable
                //
                n = "*"+n;
            }
            else if (dereference == true && field.isPtr() == false)
            {
                // need to deref the variable address
                //
                n = "&"+n;
            }
            buffer.append(field.getNativeDecodeLines(ctx, indent+2, jsonName, false, n, field.getJsonKey()));
        }

        buffer.append(pad3+"cJSON_Delete("+jsonName+");\n");
        buffer.append(pad2+"}\n");
        buffer.append(pad+"}\n");
    }

    //--------------------------------------------
    /**
     * convenience routine to add blank spaces out front
     * to make the indentation correct during generation
     */
    //--------------------------------------------
    private String getIndentBuffer(int indent)
    {
        StringBuffer buff = new StringBuffer();
        for (int i = 0 ; i < indent ; i++)
        {
            buff.append("    ");
        }
        return buff.toString();
    }

    //--------------------------------------------
    /**
     *  adds the #define columns to a row in the supplied table
     */
    //--------------------------------------------
    public void addNativeCodeDefines(CodeFormater table, String upperFamilyName)
    {
        table.setValue("#define", 0);
        table.setValue(upperFamilyName+"_"+getName().toUpperCase()+"  ", 1);
        table.setValue(Integer.toString(getCode()), 2);
    }

    //--------------------------------------------
    /**
     * return true if the Native generation requires
     * the POJO header in the #include section
     */
    //--------------------------------------------
    public boolean needsPojoHeader()
    {
        return needsPojoHeader;
    }

    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public StringBuffer getNativeApiFunctionDeclarations()
    {
        // only add our global one
        //
        StringBuffer retVal = new StringBuffer();
        if (sendRequestFunction != null)
        {
            retVal.append("\n");
            retVal.append(sendRequestFunction.declaration);
            retVal.append(";\n");
        }
        if (sendRequestTimeoutFunction != null)
        {
            retVal.append("\n");
            retVal.append(sendRequestTimeoutFunction.declaration);
            retVal.append(";\n");
        }
        
        return retVal;
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public StringBuffer getNativeApiFunctionImplementations()
    {
        StringBuffer retVal = new StringBuffer();
        if (sendRequestFunction != null)
        {
            retVal.append(sendRequestFunction.body);
            retVal.append("\n");
        }
        if (sendRequestTimeoutFunction != null)
        {
            retVal.append(sendRequestTimeoutFunction.body);
            retVal.append("\n");
        }
        
        return retVal;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public StringBuffer getNativeHandlerFunctionDeclarations()
    {
        // only add our global one
        //
        StringBuffer retVal = new StringBuffer();
        if (handleRequestFunction != null)
        {
            retVal.append("\n");
            retVal.append(handleRequestFunction.declaration);
            retVal.append(";\n");
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public StringBuffer getNativeHandlerFunctionImplementation()
    {
        // only add our global one
        //
        StringBuffer retVal = new StringBuffer();
        if (handleRequestFunction != null)
        {
            retVal.append("\n");
            retVal.append(handleRequestFunction.body);
            retVal.append("\n");
        }

        return retVal;
    }

    //-----------------------------------------------------------------------------
    // Class definition:    CFunction
    /**
     *  Model a Function (for native code generation)
     **/
    //-----------------------------------------------------------------------------
    private static class CFunction
    {
        String  functName;
        String  declaration;
        String  body;

        CFunction(String fname)
        {
            functName = fname;
        }
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public void generateJavaInterface(CodeFormater table, StringBuffer freeForm)
    {
        // first add a filler row with the description of this message
        //
        StringBuffer comment = new StringBuffer("\n    /* ");
        String desc = getDescription();
        if (StringUtils.isEmpty(desc) == true)
        {
            desc = getName();
        }
        comment.append(desc);
        comment.append(" - ");
        
        if (inParm != null)
        {
            BaseVariable field = inParm.getField();
            if (field == null)
            {
                throw new RuntimeException("Message '"+desc+"' is missing input field");
            }
            comment.append(" input: ");
            comment.append(field.getJavaType());
        }
        if (outParm != null)
        {
            BaseVariable field = outParm.getField();
            if (field == null)
            {
                throw new RuntimeException("Message '"+desc+"' is missing output field");
            }
            comment.append(" output: ");
            comment.append(field.getJavaType());
        }
        comment.append(" */");
        table.appendFillerRow(comment.toString());
        
        // now add the tabular row
        //
        table.nextRow();
        table.setValue("public static final int", 0);
        table.setValue(getName().toUpperCase(), 1);
        table.setValue("=", 2);
        table.setValue(getCode()+";", 3);
        
        // see if we have free form lines of text to add
        //
        List<String> fform = node.getFreeformList();
        for (int i = 0 ; i < fform.size() ; i++)
        {
            freeForm.append(fform.get(i));
            freeForm.append('\n');
        }
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public String getJavaName()
    {
        String valid = StringUtils.makeValidIdentifier(getName());
        return StringUtils.javaIdentifier(valid);
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public StringBuffer generateJavaCaseLines()
    {
        StringBuffer buff = new StringBuffer();
        
        buff.append("\t\t\tcase ");
        buff.append(getName());
        buff.append(":\n\t\t\t{\n");
        buff.append("\t\t\t\tlog.debug(\"handler: responding to IPC request "+getName()+"\");\n");
        
        // look at input/output parms for this message.  
        //
        String abs = "handle_"+getName();
        String args = "";
//        String signature = "";
        if (inParm != null)
        {
            // need to decode supplied arguments (from json)
            //
            BaseVariable field = inParm.getField();
            String ipcType = field.getJavaIpcObjectType();
            buff.append("\t\t\t\t"+ipcType+" input = null;\n");
            buff.append("\t\t\t\tString json = message.getPayload();\n");
            buff.append("\t\t\t\tif (StringUtils.isEmpty(json) == false)\n");
            buff.append("\t\t\t\t{\n");
            buff.append("\t\t\t\t\ttry\n\t\t\t\t\t{\n");
            buff.append("\t\t\t\t\t\t// decode input\n");
            buff.append("\t\t\t\t\t\tJSONObject decoded = (JSONObject) new JSONTokener(json).nextValue();\n");
            
            // deal with input being an event, so needs a numerical value in the constructor
            //
            buff.append("\t\t\t\t\t\tinput = new "+ipcType);
            if (field.isEvent() == true)
            {
                buff.append("(0);\n");
            }
            else
            {
                buff.append("();\n");
            }
            buff.append("\t\t\t\t\t\tinput.fromJSON(decoded);\n");

            
            buff.append("\t\t\t\t\t}\n");
            buff.append("\t\t\t\t\tcatch (JSONException jsEx) { log.warn(jsEx.getMessage(), jsEx); }\n");
            
            buff.append("\t\t\t\t}\n");
            
            args += "input";
//            signature += ipcType+" input";
        }
        if (outParm != null)
        {
            // deal with output being an event, so needs a numerical value in the constructor
            //
            BaseVariable field = outParm.getField();
            String ipcType = field.getJavaIpcObjectType();
            buff.append("\t\t\t\t"+ipcType+" output = new "+ipcType);
            if (field.isEvent() == true)
            {
                buff.append("(0);\n");
            }
            else
            {
                buff.append("();\n");
            }

            if (args.length() > 0)
            {
                args += ", ";
//                signature += ", ";
            }
            args += "output";
//            signature += ipcType+" output";
        }
        
        // our 'process' abstract function
        buff.append("\t\t\t\t// handle request\n");
        buff.append("\t\t\t\tboolean worked = "+abs+"(");
        buff.append(args);
        buff.append(");\n");
        buff.append("\t\t\t\tif (worked == true)\n\t\t\t\t{\n");
        
        if (outParm != null)
        {
            buff.append("\t\t\t\t\ttry\n\t\t\t\t\t{\n");
            
            buff.append("\t\t\t\t\t\t// encode output\n");
            buff.append("\t\t\t\t\t\tJSONObject jsonOut = output.toJSON();\n");
            buff.append("\t\t\t\t\t\tmessage.setPayload(jsonOut.toString());\n");
            buff.append("\t\t\t\t\t\trc = 0;\n");
            
            buff.append("\t\t\t\t\t}\n");
            buff.append("\t\t\t\t\tcatch (JSONException jsEx) { log.warn(jsEx.getMessage(), jsEx); }\n");
        }
        else
        {
            buff.append("\t\t\t\t\trc = 0;\n");
        }
        buff.append("\t\t\t\t}\n");
        
        
        buff.append("\t\t\t\tbreak;\n");
        buff.append("\t\t\t}\n");
        return buff;
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public StringBuffer generateDirectJavaCaseLines()
    {
        StringBuffer buff = new StringBuffer();
        
        buff.append("\t\t\tcase ");
        buff.append(getName());
        buff.append(":\n\t\t\t{\n");
        buff.append("\t\t\t\tlog.debug(\"handler: responding to direct request "+getName()+"\");\n");
        
        // look at input/output parms for this message.  
        //
        String abs = "handle_"+getName();
        String args = "";
        if (inParm != null)
        {
            // nothing to decode, simply add 'input' as part of the arguments
            // but typecast to the proper Class first
            //
            args += "("+inParm.getField().getJavaIpcObjectType()+")input";
        }
        if (outParm != null)
        {
            // nothing to decode, simply add 'output' as part of the arguments
            //
            if (args.length() > 0)
            {
                args += ", ";
            }
            args += "("+outParm.getField().getJavaIpcObjectType()+")output";
        }
        
        // our 'process' abstract function
        buff.append("\t\t\t\t// handle request\n");
        buff.append("\t\t\t\tboolean worked = "+abs+"(");
        buff.append(args);
        buff.append(");\n");
        buff.append("\t\t\t\tif (worked == true)\n\t\t\t\t{\n");
        
        if (outParm != null)
        {
            buff.append("\t\t\t\t\trc = 0;\n");
        }
        else
        {
            buff.append("\t\t\t\t\trc = 0;\n");
        }
        buff.append("\t\t\t\t}\n");
        
        
        buff.append("\t\t\t\tbreak;\n");
        buff.append("\t\t\t}\n");
        return buff;
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public StringBuffer generateJavaImplementationLines()
    {
        StringBuffer buff = new StringBuffer();
        buff.append("\n\t/* process request for "+getName()+" request */\n");
        buff.append("\tprotected abstract boolean handle_");
        buff.append(getName());
        buff.append("(");
        
        if (inParm != null)
        {
            buff.append(inParm.getField().getJavaIpcObjectType());
            buff.append(" input");
            if (outParm != null)
            {
                buff.append(", ");
            }
        }
        if (outParm != null)
        {
            buff.append(outParm.getField().getJavaIpcObjectType());
            buff.append(" output");
        }
        
        buff.append(");\n");
        
        return buff;
    }
}

//+++++++++++
//EOF
//+++++++++++
