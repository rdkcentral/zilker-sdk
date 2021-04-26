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

package com.icontrol.generate.service.io;

import java.io.IOException;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.ipc.Message;
import com.icontrol.generate.service.parse.EnumDefWrapper;
import com.icontrol.generate.service.parse.MessageWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.Wrapper;
import com.icontrol.util.StringUtils;

//-----------------------------------------------------------------------------
/**
 * Created by jelderton on 5/26/15.
 */
//-----------------------------------------------------------------------------
public class MessageJavaCodeProducer extends MessageCodeProducer
{
    private final static String INTERFACE_TEMPLATE = "/com/icontrol/generate/service/templates/ipc/BaseIPCInterface_java";
    private final static String CLIENT_TEMPLATE    = "ipc/BaseIPCClient_java.ftl";
    private final static String HANDLER_TEMPLATE   = "/com/icontrol/generate/service/templates/ipc/BaseIPCHandler_java";
    private final static String IPC_SUFFIX         = "_IPC";
    private final static String CLIENT_SUFFIX      = "Client";
    private final static String HANDLER_SUFFIX     = "Handler";

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public MessageJavaCodeProducer()
    {
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generate(Context, LanguageMask)
     */
    //--------------------------------------------
    @Override
    public boolean generate(Context ctx, LanguageMask lang) throws IOException
    {
        // loop through all enums to see if we need to create ones
        // that are specific to messages
        //
        Iterator<Wrapper> depends = getMessageDependencies(false);
        while (depends.hasNext() == true)
        {
            Wrapper wrap = depends.next();
            if ((wrap instanceof EnumDefWrapper) == true &&
                wrap.getLanguageMask().hasJava() == true &&
                wrap.getSectionMask().inMessageSection() == true &&
                wrap.getContextJava().wasGenerated() == false)
            {
                // Enum that hasn't been generated, but referred to by a Message
                // Luckily the EventJavaCodeProducer knows how to make these
                //
                EventJavaCodeProducer helper = new EventJavaCodeProducer();
                helper.generateWrapperFile(ctx, lang, wrap);
            }
        }

        // generate the IPC Codes Interface
        //
        createIpcCodeInterface(ctx);

        // generate the IPC Client stubs
        //
        createIpcClient(ctx);

        // generate the IPC Handler
        //
        createIpcServiceHandler(ctx);

        return true;
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generateWrapperFile(Context, LanguageMask, Wrapper)
     */
    //--------------------------------------------
    @Override
    public boolean generateWrapperFile(Context ctx, LanguageMask lang, Wrapper wrapper) throws IOException
    {
        // not really applicable since all messages are combined together
        //
        return false;
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generateWrapperBuffer(Context, LanguageMask, Wrapper)
     */
    //--------------------------------------------
    @Override
    public StringBuffer generateWrapperBuffer(Context ctx, LanguageMask lang, Wrapper wrapper) throws IOException
    {
        // not really applicable since all messages are combined together
        //
        return null;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private String getIpcCodesInterfaceName(Context ctx, boolean fullyQualified)
    {
        // get the name of the project + IPC Codes
        //
        String valid = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String projName = StringUtils.camelCase(valid).replace(' ','_') + IPC_SUFFIX;

        if (fullyQualified == true)
        {
            // use package from context + project + EventCodes
            //
            String pkg = ctx.getPaths().getApiPackage();
            return pkg + "." +projName;
        }
        return projName;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private boolean createIpcCodeInterface(Context ctx)
        throws IOException
    {
        // calculate paths & filenames
        //
        String apiDir = ctx.getPaths().getApiDir();
        String pkg = ctx.getPaths().getApiPackage();
        String pkgDir = getDirForPackage(pkg);
        String name = getIpcCodesInterfaceName(ctx, false);

        // create map with basic information
        //
        MacroSource map = new MacroSource();
        map.addMapping(JAVA_CLASS, name);
        map.addMapping(JAVA_PKG, pkg);

        // add server "port" info
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+IPC_SUFFIX;
        map.addMapping(SERVER_PORT_NUM, Integer.toString(ctx.getIpcPortNum()));
        map.addMapping(SERVER_PORT_NAME, headerName.toUpperCase());
        map.addMapping(SERVICE_KEY, ctx.getServiceName());

        // finally, the freeform and IPC codes by looping through all of the defined messages
        //
        StringBuffer freeForm = new StringBuffer();
        CodeFormater table = new CodeFormater(4);
        Iterator<Wrapper> loop = Processor.getMessageProcessor().getWrappers();
        while (loop.hasNext() == true)
        {
            // skip if not java
            //
            MessageWrapper wrapper = (MessageWrapper)loop.next();
            if (wrapper.getLanguageMask().hasJava() == true)
            {
                Message msg = wrapper.getMessage();
                msg.generateJavaInterface(table, freeForm);
            }
        }
        map.addMapping(JAVA_FREE_FORM, freeForm.toString());
        map.addMapping(JAVA_VAR_SECTION, table.toStringBuffer(4).toString());

        // perform substitution against our template to create the file
        //
        createFile(ctx.getBaseOutputDir(), apiDir + "/" + pkgDir, name + ".java", INTERFACE_TEMPLATE, map);

        // no exception, so should be good
        //
        return true;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private String getIpcClientName(Context ctx, boolean fullyQualified)
    {
        // get the name of the project + IPC Codes
        //
        String valid = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String projName = StringUtils.camelCase(valid).replace(' ','_') + CLIENT_SUFFIX;

        if (fullyQualified == true)
        {
            // use package from context + project + EventCodes
            //
            String pkg = ctx.getPaths().getApiPackage();
            return pkg + "." +projName;
        }
        return projName;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private boolean createIpcClient(Context ctx)
        throws IOException
    {
        // calculate paths & filenames
        //
        String apiDir = ctx.getPaths().getApiDir();
        String pkg = ctx.getPaths().getApiPackage();
        String pkgDir = getDirForPackage(pkg);
        String interfaceName = getIpcCodesInterfaceName(ctx, false);
        String name = getIpcClientName(ctx, false);

        // create map with basic information
        //
        Map<String,Object> map = new HashMap<String,Object>();
        map.put(JAVA_CLASS, name);
        map.put(JAVA_PKG, pkg);
        map.put(JAVA_CODES_INTERFACE, interfaceName);

        // add server "port" info
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+IPC_SUFFIX;
        map.put(SERVER_PORT_NUM, Integer.toString(ctx.getIpcPortNum()));
        map.put(SERVER_PORT_NAME, headerName.toUpperCase());
        map.put(SERVICE_KEY, ctx.getServiceName());
        map.put(JAVA_CODES_INTERFACE, interfaceName);
        map.put(SERVER_PORT_NAME, headerName.toUpperCase());
        map.put("wrappers", Processor.getMessageProcessor().getWrappers());

        // add any freeform messages defined in the Native Pragma
        //
        List<String> freeform = ctx.getPaths().getJavaFreeforms();
        StringBuffer freeBuff = new StringBuffer();
        if (freeform != null && freeform.size() > 0)
        {
            Iterator<String> freeLoop = freeform.iterator();
            while (freeLoop.hasNext() == true)
            {
                freeBuff.append(freeLoop.next());
                freeBuff.append('\n');
            }
        }
        map.put(JAVA_FREE_FORM, freeBuff.toString());

        map.put("header", HeaderWriter.headerToString());

        // Write it out
        FreemarkerHelper.processTemplateToFile(ctx.getBaseOutputDir(), apiDir + "/" + pkgDir, name + ".java", CLIENT_TEMPLATE, map);

        // no exception, so should be good
        //
        return true;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private boolean createIpcServiceHandler(Context ctx)
        throws IOException
    {
        // calculate paths & filenames
        //
        String serviceDir = ctx.getPaths().getServiceDir();
        String pkg = ctx.getPaths().getServicePackage();

        // only generate Java Service layer if we have both a
        // "serviceDir" and "package" defined
        //
        if (StringUtils.isEmpty(serviceDir) == true ||
            StringUtils.isEmpty(pkg) == true)
        {
            // don't return failure or it could kill the processing
            //
            return true;
        }

        String pkgDir = getDirForPackage(pkg);
        String valid = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String cName = StringUtils.camelCase(valid).replace(' ','_') + HANDLER_SUFFIX;

        // create map with basic information
        //
        MacroSource map = new MacroSource();
        map.addMapping(JAVA_CLASS, cName);
        map.addMapping(JAVA_PKG, pkg);

        // add server "port" info
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+IPC_SUFFIX;
        map.addMapping(SERVER_PORT_NAME, headerName.toUpperCase());
        map.addMapping(SERVICE_KEY, ctx.getServiceName());
        map.addMapping(JAVA_CODES_INTERFACE, getIpcCodesInterfaceName(ctx, false));
        map.addMapping("java_api_pkg", ctx.getPaths().getApiPackage());

        // finally, the code sniplets by looping through the messages
        //
        StringBuffer caseLines = new StringBuffer();
        StringBuffer directLines = new StringBuffer();
        StringBuffer abstractLines = new StringBuffer();
        Iterator<Wrapper> loop = Processor.getMessageProcessor().getWrappers();
        while (loop.hasNext() == true)
        {
            // skip if not java
            //
            MessageWrapper wrapper = (MessageWrapper)loop.next();
            if (wrapper.getLanguageMask().hasJava() == false)
            {
                continue;
            }

            // get code for IPC 'case'
            //
            Message msg = wrapper.getMessage();
            StringBuffer casePart = msg.generateJavaCaseLines();
            if (casePart != null)
            {
                caseLines.append(casePart);
            }

            // get code for the 'direct case'
            //
            StringBuffer directPart = msg.generateDirectJavaCaseLines();
            if (directPart != null)
            {
                directLines.append(directPart);
            }

            // get code for 'implementation'
            //
            StringBuffer implPart = msg.generateJavaImplementationLines();
            if (implPart != null)
            {
                abstractLines.append(implPart);
            }
        }

        map.addMapping(HAND_MSG_CASE, caseLines.toString());
        map.addMapping(HAND_MSG_DIRECT_CASE, directLines.toString());
        map.addMapping(HAND_ABSTACTS, abstractLines.toString());

        // perform substitution against our template to create the file
        //
        createFile(ctx.getBaseOutputDir(), serviceDir + "/" + pkgDir, cName + ".java", HANDLER_TEMPLATE, map);

        // no exception, so should be good
        //
        return true;
    }
}

