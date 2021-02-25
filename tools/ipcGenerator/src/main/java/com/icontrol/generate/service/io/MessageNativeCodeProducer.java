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

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.ipc.Message;
import com.icontrol.generate.service.parse.EnumDefWrapper;
import com.icontrol.generate.service.parse.MessageWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.Wrapper;
import com.icontrol.util.StringUtils;

//-----------------------------------------------------------------------------
/**
 * Native code producer for create Message (IPC); both, Client, and Service
 */
//-----------------------------------------------------------------------------
public class MessageNativeCodeProducer extends MessageCodeProducer
{
    // header templates
    //
    private final static String API_H_TEMPLATE        = "/com/icontrol/generate/service/templates/ipc/MessageAPI_h";
    private final static String IPC_CODES_H_TEMPLATE  = "/com/icontrol/generate/service/templates/ipc/MessageCodes_h";
    private final static String ENUM_DEFS_TEMPLATE    = "/com/icontrol/generate/service/templates/pojo/Pojo_enum_defs_h";
    private final static String HANDLER_H_TEMPLATE  = "/com/icontrol/generate/service/templates/ipc/native_handler_h";

    // source templates
    //
    private final static String API_C_TEMPLATE      = "/com/icontrol/generate/service/templates/ipc/MessageAPI_c";
    private final static String HANDLER_C_TEMPLATE  = "/com/icontrol/generate/service/templates/ipc/native_handler_c";

    private static final String IPC_CODES_SUFFIX   = "_ipc_codes";
    private static final String IPC_SUFFIX         = "_ipc";
    private static final String IPC_HANDLER_SUFFIX = "_ipc_handler";

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public MessageNativeCodeProducer()
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
        // When creating the "messages" portion of the code-generation, there
        // are 2 sections to produce:
        //  1.  Client functions to call (that call IPCSender)
        //      - header of message code definitions + enum
        //      - header of function prototypes
        //      - source of function implementations
        //  2.  Service handler
        //      - header of function prototypes that are called
        //      - source of IPCReceiver handler + decode + calls to prototypes
        //
        String codesHeader = generateCodesHeader(ctx);
        String apiHeader = generateClientFunctionsHeader(ctx, codesHeader);
        generateClientFunctionsBody(ctx, codesHeader, apiHeader);

        // now the service
        //
        String serviceHeader = generateServiceFunctionsHeader(ctx, codesHeader);
        if (serviceHeader != null)
        {
            generateServiceFunctionsBody(ctx, codesHeader, serviceHeader);
        }

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
        // not applicable to native messages
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
        // not applicable to native messages
        //
        return null;
    }

    //--------------------------------------------
    /**
     * Create the .h file with the IPC codes
     * Used by Client & Service
     */
    //--------------------------------------------
    private String generateCodesHeader(Context ctx)
        throws IOException
    {
        // determine the filename of the header
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+IPC_CODES_SUFFIX;
        String headerUpper = headerName.toUpperCase();
        String fileName = headerName+".h";

        // figure out the parent dir of the header (assuming we follow our 'public' convention).
        // ex:  app_mgr_pojo.h --> public/app_mgr/app_mgr_pojo.h
        //
        String dir = ctx.getPaths().getNativeApiHeaderDir();
        String parent = new File(dir).getName();
        String includeName = parent+"/"+fileName;       // app_mgr/app_mgr.h
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_HEADER_IFDEF, headerUpper+"_H");
        macros.addMapping(IPC_CODE_PREFIX, headerUpper+"_");
        macros.addMapping(SERVER_PORT_NAME, project.toUpperCase());
        macros.addMapping(SERVER_PORT_NUM, Integer.toString(ctx.getIpcPortNum()));

        // loop through all Native messages
        //
        CodeFormater msgCodesTable = new CodeFormater(3);
        Iterator<Wrapper> loop = Processor.getMessageProcessor().getWrappers();
        while (loop.hasNext() == true)
        {
            // get the wrapper and skip if not Native
            //
            MessageWrapper messageWrapper = (MessageWrapper)loop.next();
            if (messageWrapper.getLanguageMask().hasNative() == false)
            {
                continue;
            }

            // add the message codes for this one
            //
            Message msg = messageWrapper.getMessage();
            msgCodesTable.nextRow();
            msg.addNativeCodeDefines(msgCodesTable, headerUpper);
        }
        macros.addMapping("api_message_codes", msgCodesTable.toStringBuffer(0).toString());

        // finally, create the header file
        //
        File header = createFile(ctx.getBaseOutputDir(), dir, fileName, IPC_CODES_H_TEMPLATE, macros);
        if (header.exists() && header.length() > 0)
        {
            return includeName;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Create the .h for the IPC calls made by clients
     */
    //--------------------------------------------
    private String generateClientFunctionsHeader(Context ctx, String codesHeader)
        throws IOException
    {
        // determine the filename of the header
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+IPC_SUFFIX;
        String headerUpper = headerName.toUpperCase();
        String fileName = headerName+".h";

        // figure out the parent dir of the header (assuming we follow our 'public' convention).
        // ex:  app_mgr_pojo.h --> public/app_mgr/app_mgr_pojo.h
        //
        String dir = ctx.getPaths().getNativeApiHeaderDir();
        String parent = new File(dir).getName();
        String includeName = parent+"/"+fileName;       // app_mgr/app_mgr.h
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_HEADER_IFDEF, headerUpper+"_H");

        String ipc_code_prefix = project+IPC_CODES_SUFFIX;
        macros.addMapping(IPC_CODE_PREFIX, ipc_code_prefix.toUpperCase()+"_");

        // save list of items we need to include headers for (POJO, Enum, etc)
        //
        HashSet<String> includeHeaders = new HashSet<String>();
        includeHeaders.add(codesHeader);
        StringBuffer definitions = new StringBuffer();
        StringBuffer declarations = new StringBuffer();

        // get all enums and pojos that are native and have the "message section"
        // set, so we can add those typedefs or #include for their headers
        //
        Iterator<Wrapper> depends = getMessageDependencies(true);
        while (depends.hasNext() == true)
        {
            // examine the wrapper to see if Enum, Event, or POJO
            // if code is already generated for this, add the header file
            // to the hash so we can utilize it and not re-generate
            // the code for it
            //
            Wrapper wrap = depends.next();
            if (wrap.getContextNative().wasGenerated() == true)
            {
                // add header to the hash
                //
                includeHeaders.add(wrap.getContextNative().getHeaderFileName());
            }
            else if (wrap instanceof EnumDefWrapper)
            {
                // special casae of Enum that hasn't been generated yet
                //
                EnumNativeCodeProducer enumHelper = new EnumNativeCodeProducer();
                StringBuffer enumStub = enumHelper.generateWrapperBuffer(ctx, LanguageMask.NATIVE, wrap);
                if (enumStub.length() > 0)
                {
                    definitions.append(enumStub);
                }
            }
        }

        // loop through all Native messages
        //
        Iterator<Wrapper> loop = Processor.getMessageProcessor().getWrappers();
        while (loop.hasNext() == true)
        {
            // get the wrapper and skip if not Native
            //
            MessageWrapper messageWrapper = (MessageWrapper)loop.next();
            if (messageWrapper.getLanguageMask().hasNative() == false)
            {
                continue;
            }

            // add the message codes for this one
            //
            Message msg = messageWrapper.getMessage();
            msg.calculateNativeFunctions(project, ipc_code_prefix.toUpperCase()+"_", ctx.getPaths().getNativeConfigFlag());

            // get the message's function declaration for sending the request
            //
            StringBuffer dec = msg.getNativeApiFunctionDeclarations();
            if (dec != null && dec.length() > 0)
            {
                declarations.append(dec);
            }
        }

        // populate the MacroSource with any enums
        //
        StringBuffer topDefs = new StringBuffer();
        if (definitions.length() > 0)
        {
            // load the ENUM_DEFS_TEMPLATE and inject the Enum "typedefs"
            //
            topDefs.append(FileHelper.translateSimpleBuffer(ENUM_DEFS_TEMPLATE, C_TYPE_DECS, definitions.toString()));
        }
        macros.addMapping(C_TYPE_DECS, topDefs.toString());

        // sort the unique includes and add to the macros
        //
        macros.addMapping(C_INCLUDE_SECTION, combineIncludes(includeHeaders));
        macros.addMapping(C_FUNCTION_DECS, declarations.toString());

        // finally, create the header file
        //
        File header = createFile(ctx.getBaseOutputDir(), dir, fileName, API_H_TEMPLATE, macros);
        if (header.exists() && header.length() > 0)
        {
            return includeName;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Create the .c for the IPC calls made by clients
     */
    //--------------------------------------------
    private boolean generateClientFunctionsBody(Context ctx, String codesHeader, String funcHeader)
        throws IOException
    {
        // create the MacroSource and start populating it
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+IPC_SUFFIX;
        String headerUpper = headerName.toUpperCase();
        StringBuffer functions = new StringBuffer();
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_CONFIG_FLAG, ctx.getPaths().getNativeConfigFlag());

        // generate the function body.  We'll use the same mechanism used
        // to create the function declarations so that the templates match
        //
        Iterator<Wrapper> loop = Processor.getMessageProcessor().getWrappers();
        while (loop.hasNext() == true)
        {
            // get the wrapper and skip if not Native
            //
            MessageWrapper messageWrapper = (MessageWrapper)loop.next();
            if (messageWrapper.getLanguageMask().hasNative() == false)
            {
                continue;
            }

            // add the function impls for the "request api"
            //
            Message msg = messageWrapper.getMessage();
            StringBuffer impl = msg.getNativeApiFunctionImplementations();
            if (impl != null && impl.length() > 0)
            {
                functions.append(impl);
            }
        }

        // add the function bodies to the macro mapping to create
        // the complete source file (see Pojo_c template)
        //
        String sourceName = project+IPC_SUFFIX;
        macros.addMapping(C_FUNCTION_DECS, functions.toString());
        macros.addMapping(C_INCLUDE_SECTION, "#include \""+codesHeader+"\"\n#include \""+funcHeader+"\"\n");

        String ipc_code_prefix = project+IPC_CODES_SUFFIX;
        macros.addMapping(IPC_CODE_PREFIX, ipc_code_prefix.toUpperCase()+"_");

        // finally, create the header file
        //
        String dir = ctx.getPaths().getNativeApiSrcDir();
        File source = createFile(ctx.getBaseOutputDir(), dir, sourceName + ".c", API_C_TEMPLATE, macros);
        if (source.exists() && source.length() > 0)
        {
            return true;
        }

        return false;
    }

    //--------------------------------------------
    /**
     * Create the .h of handler functions used by the Service
     */
    //--------------------------------------------
    private String generateServiceFunctionsHeader(Context ctx, String codesHeader)
        throws IOException
    {
        String dir = ctx.getPaths().getNativeIpcHandlerDir();
        if (dir == null)
        {
            // no C code to generate
            //
            return null;
        }

        // determine the filename of the header
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+IPC_HANDLER_SUFFIX;
        String headerUpper = headerName.toUpperCase();
        String fileName = headerName+".h";

        // figure out the parent dir of the header (assuming we follow our 'public' convention).
        // ex:  app_mgr_pojo.h --> public/app_mgr/app_mgr_pojo.h
        //
        String parent = new File(dir).getName();
        String includeName = parent+"/"+fileName;       // app_mgr/app_mgr.h
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_HEADER_IFDEF, headerUpper+"_H");
        macros.addMapping(SERVER_PORT_NAME, project.toUpperCase());
        macros.addMapping(SERVER_PORT_NUM, Integer.toString(ctx.getIpcPortNum()));

        String ipc_code_prefix = project+IPC_CODES_SUFFIX;
        macros.addMapping(IPC_CODE_PREFIX, ipc_code_prefix.toUpperCase()+"_");

        // save list of items we need to include headers for (POJO, Enum, etc)
        //
        HashSet<String> includeHeaders = new HashSet<String>();
        includeHeaders.add(codesHeader);

        // get all enums and pojos that are native and have the "message section"
        // set, so we can add those typedefs or #include for their headers
        //
        Iterator<Wrapper> depends = getMessageDependencies(true);
        while (depends.hasNext() == true)
        {
            // examine the wrapper to see if Enum, Event, or POJO
            // if code is already generated for this, add the header file
            // to the hash so we can utilize it and not re-generate
            // the code for it
            //
            Wrapper wrap = depends.next();
            if (wrap.getContextNative().wasGenerated() == true)
            {
                // add header to the hash
                //
                includeHeaders.add(wrap.getContextNative().getHeaderFileName());
            }
        }

        // create "handler" function for each message, with decoded input/output parameters
        // should be very similar to the API side of things created above
        //
        StringBuffer functions = new StringBuffer();
        Iterator<Wrapper> loop = Processor.getMessageProcessor().getWrappers();
        while (loop.hasNext() == true)
        {
            // get the wrapper and skip if not Native
            //
            MessageWrapper messageWrapper = (MessageWrapper)loop.next();
            if (messageWrapper.getLanguageMask().hasNative() == false)
            {
                continue;
            }

            // add the function declarations for handling the "request"
            //
            Message msg = messageWrapper.getMessage();
            StringBuffer impl = msg.getNativeHandlerFunctionDeclarations();
            if (impl != null && impl.length() > 0)
            {
                functions.append(impl);
            }
        }

        // sort the unique includes and add to the macros
        //
        macros.addMapping(C_INCLUDE_SECTION, combineIncludes(includeHeaders));
        macros.addMapping(C_FUNCTION_DECS, functions.toString());

        // finally, create the header file
        //
        File header = createFile(ctx.getBaseOutputDir(), dir, fileName, HANDLER_H_TEMPLATE, macros);
        if (header.exists() && header.length() > 0)
        {
            return fileName;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Create the .c of handler functions used by the Service
     */
    //--------------------------------------------
    private boolean generateServiceFunctionsBody(Context ctx, String codesHeader, String funcHeader)
        throws IOException
    {
        // create the MacroSource and start populating it
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+IPC_HANDLER_SUFFIX;
        String headerUpper = headerName.toUpperCase();
        StringBuffer functions = new StringBuffer();
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_CONFIG_FLAG, ctx.getPaths().getNativeConfigFlag());
        macros.addMapping(SERVER_PORT_NAME, project.toUpperCase());

        // generate the function body.  We'll use the same mechanism used
        // to create the function declarations so that the templates match
        //
        Iterator<Wrapper> loop = Processor.getMessageProcessor().getWrappers();
        while (loop.hasNext() == true)
        {
            // get the wrapper and skip if not Native
            //
            MessageWrapper messageWrapper = (MessageWrapper)loop.next();
            if (messageWrapper.getLanguageMask().hasNative() == false)
            {
                continue;
            }

            // for each native message, create the "case" statement
            // using the native_handler.c template
            //

            Message msg = messageWrapper.getMessage();
            StringBuffer impl = msg.getNativeHandlerFunctionImplementation();
            if (impl != null && impl.length() > 0)
            {
                functions.append(impl);
            }
        }

        // add the function bodies to the macro mapping to create
        // the complete source file (see Pojo_c template)
        //
        String sourceName = project+IPC_HANDLER_SUFFIX;
        macros.addMapping("message_handle", functions.toString());
//        macros.addMapping(C_FUNCTION_DECS, functions.toString());
        macros.addMapping(C_INCLUDE_SECTION, "#include \""+codesHeader+"\"\n#include \""+funcHeader+"\"\n");

        String ipc_code_prefix = project+IPC_CODES_SUFFIX;
        macros.addMapping(IPC_CODE_PREFIX, ipc_code_prefix.toUpperCase()+"_");

        // finally, create the header file
        //
        String dir = ctx.getPaths().getNativeIpcHandlerDir();
        File source = createFile(ctx.getBaseOutputDir(), dir, sourceName + ".c", HANDLER_C_TEMPLATE, macros);
        if (source.exists() && source.length() > 0)
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
    private String combineIncludes(HashSet<String> includeHeaders)
    {
        ArrayList<String> includes = new ArrayList<String>(includeHeaders);
        Collections.sort(includes);
        StringBuffer incBuff = new StringBuffer();
        for (int i = 0 ; i < includes.size() ; i++)
        {
            incBuff.append("#include \"");
            incBuff.append(includes.get(i));
            incBuff.append("\"\n");
        }

        return incBuff.toString();
    }



}
