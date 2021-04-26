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
import java.util.List;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.parse.EventProcessor;
import com.icontrol.generate.service.parse.EventWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.Wrapper;
import com.icontrol.generate.service.variable.EventVariable;
import com.icontrol.util.StringUtils;

//-----------------------------------------------------------------------------
/**
 * CodeProducer implementation for creating Native Events & EventAdapters
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class EventNativeCodeProducer extends EventCodeProducer
{
    // header templates
    //
    private final static String EVENT_H_TEMPLATE        = "/com/icontrol/generate/service/templates/event/Event_h";
    private final static String DEFINE_CODES_TEMPLATE   = "/com/icontrol/generate/service/templates/event/define_codes_h";
    private final static String MESSAGE_CODES_TEMPLATE  = "/com/icontrol/generate/service/templates/event/message_codes_h";
    private final static String ENUM_DEFS_TEMPLATE      = "/com/icontrol/generate/service/templates/event/enum_defs_h";
    private final static String EVENT_DEFS_TEMPLATE     = "/com/icontrol/generate/service/templates/event/encode_defs_h";
    private final static String EVENT_DECS_TEMPLATE     = "/com/icontrol/generate/service/templates/event/encode_decs_h";

    private final static String ADAPTER_H_TEMPLATE          = "/com/icontrol/generate/service/templates/event/EventAdapter_h";
    private final static String ADAPTER_LISTENER_DEFS       = "/com/icontrol/generate/service/templates/event/adapter_defs_h";
    private final static String ADAPTER_LISTENER_REGISTER   = "/com/icontrol/generate/service/templates/event/adapter_register_h";
    private final static String ADAPTER_LISTENER_UNREGISTER = "/com/icontrol/generate/service/templates/event/adapter_unregister_h";

    // source templates
    //
    private final static String EVENT_C_TEMPLATE        = "/com/icontrol/generate/service/templates/event/Event_c";
    private final static String ADAPTER_C_TEMPLATE      = "/com/icontrol/generate/service/templates/event/EventAdapter_c";
    private final static String ADAPTER_VARIABLE_DEFS   = "/com/icontrol/generate/service/templates/event/adapter_variables_c";
    private final static String ADAPTER_FUNCTION_DECS   = "/com/icontrol/generate/service/templates/event/adapter_functions_dec_c";
    private final static String ADAPTER_FUNCTION_BODY   = "/com/icontrol/generate/service/templates/event/adapter_functions_c";
    private final static String ADAPTER_REGISTER_BODY   = "/com/icontrol/generate/service/templates/event/adapter_register_c";
    private final static String ADAPTER_UNREGISTER_BODY = "/com/icontrol/generate/service/templates/event/adapter_unregister_c";
    private final static String ADAPTER_DECODE_EVENT    = "/com/icontrol/generate/service/templates/event/adapter_decode_c";


    private static final String EVENT_ADAPTER_SUFFIX  = "_eventAdapter";    // used in filename calculation

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public EventNativeCodeProducer()
    {
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generate(Context, LanguageMask)
     */
    //--------------------------------------------
    @Override
    public boolean generate(Context ctx, LanguageMask lang)
        throws IOException
    {
        // There are 3 different steps to generating events:
        //  1.  Event codes & typedefs in client-side header
        //  2.  Event encode/decode functions (similar to POJO)
        //  3.  Event adapter
        //
        String eventHeader = generateNativeEventHeader(ctx);
        generateNativeEventSource(ctx, eventHeader);

        String adapterHeader = generateNativeEventAdapterHeader(ctx, eventHeader);
        generateNativeEventAdapterSource(ctx, adapterHeader);

        // didn't throw IOException, so good to go
        //
        return true;
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generateWrapperFile(Context, LanguageMask, Wrapper)
     */
    //--------------------------------------------
    @Override
    public boolean generateWrapperFile(Context ctx, LanguageMask lang, Wrapper wrapper)
        throws IOException
    {
        // Not applicable for Native Event generation
        //
        return false;
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generateWrapperBuffer(Context, LanguageMask, Wrapper)
     */
    //--------------------------------------------
    @Override
    public StringBuffer generateWrapperBuffer(Context ctx, LanguageMask lang, Wrapper wrapper)
        throws IOException
    {
        // Not applicable for Native Event generation
        //
        return null;
    }

    //--------------------------------------------
    /**
     * Generate the Enum, Event strucs, and encode/decode
     * function prototypes in a single header file
     */
    //--------------------------------------------
    private String generateNativeEventHeader(Context ctx)
        throws IOException
    {
        // determine the filename of the header
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+EVENT_SUFFIX;
        String fileName = headerName+".h";

        // figure out the parent dir of the header (assuming we follow our 'public' convention).
        // ex:  app_mgr_pojo.h --> public/app_mgr/app_mgr_pojo.h
        //
        String dir = ctx.getPaths().getNativeApiHeaderDir();
        String parent = new File(dir).getName();
        String includeName = parent+"/"+fileName;       // app_mgr/app_mgr.h

        // create the MacroSource and start populating it
        //
        StringBuffer definitions = new StringBuffer();      // typedef statements
        StringBuffer prototypes = new StringBuffer();       // function prototypes
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_HEADER_IFDEF, headerName.toUpperCase()+"_H");
        macros.addMapping(SERVER_PORT_NUM, Integer.toString(ctx.getEventPortNum()));
        macros.addMapping(SERVER_PORT_NAME, headerName.toUpperCase());

        // first the event code/value #defines
        //
        EventProcessor eventProcessor = Processor.getEventProcessor();
        StringBuffer buffer = appendCodes(eventProcessor.getDefineCodes());
        if (buffer.length() > 0)
        {
            // load the DEFINE_CODES_TEMPLATE and inject the various definitions
            //
            definitions.append(FileHelper.translateSimpleBuffer(DEFINE_CODES_TEMPLATE, C_TYPE_DECS, buffer.toString()));
        }
        buffer = appendCodes(eventProcessor.getEventCodes());
        if (buffer.length() > 0)
        {
            // load the MESSAGE_CODES_TEMPLATE and inject the message codes
            //
            definitions.append(FileHelper.translateSimpleBuffer(MESSAGE_CODES_TEMPLATE, C_TYPE_DECS, buffer.toString()));
        }

        // now the Enums that that are not covered by POJO generation (i.e ones
        // that are only used by the Event objects)
        //
        EnumNativeCodeProducer enumHelper = new EnumNativeCodeProducer();
        buffer = enumHelper.generateNativeEnums(ctx, getEnumsToGenerate(), includeName);
        if (buffer.length() > 0)
        {
            // load the ENUM_DEFS_TEMPLATE and inject the Enum "typedefs"
            //
            definitions.append(FileHelper.translateSimpleBuffer(ENUM_DEFS_TEMPLATE, C_TYPE_DECS, buffer.toString()));
        }

        // encode/decode function declarations.  Since it's the same as
        // the POJO native code (and EventVariable subclasses CustomVariable),
        // just use a PojoNativeCodeProducer
        //
        PojoNativeCodeProducer pojoHelper = new PojoNativeCodeProducer();
        buffer = pojoHelper.generateNativePojoDefs(eventProcessor.getWrappers(), includeName);
        if (buffer.length() > 0)
        {
            // load the EVENT_DECS_TEMPLATE and inject the Event "typedefs"
            //
            definitions.append(FileHelper.translateSimpleBuffer(EVENT_DEFS_TEMPLATE, C_TYPE_DECS, buffer.toString()));
        }
        buffer = pojoHelper.generateNativePojoFunction(eventProcessor.getWrappers(), true);
        if (buffer.length() > 0)
        {
            // load the EVENT_DECS_TEMPLATE and inject the Event encode/decode function declarations
            //
            prototypes.append(FileHelper.translateSimpleBuffer(EVENT_DECS_TEMPLATE, C_FUNCTION_DECS, buffer.toString()));
        }

        // add includes for pojo references within the event definitions
        //
        HashSet<String> includeNames = new HashSet<String>();
        Iterator<Wrapper> depends = getEventDependencies(true);
        while (depends.hasNext() == true)
        {
            // see if this wrapper generates native
            //
            Wrapper wrap = depends.next();
//System.out.println("JOHAN: --> depends on "+wrap.getVariable().getName());
            if (wrap.getLanguageMask().hasNative() == true &&
                wrap.getContextNative().wasGenerated() == true)
            {
                // generated C code for this dependency, so add to our "include" list
                // (unless of course it refers to the header we're making now)
            	//
                String header = wrap.getContextNative().getHeaderFileName();
                if (header != null && header.equals(includeName) == false)
                {
                    includeNames.add(header);
                }
            }
        }

        // combine all includes into a single buffer so it can be translated
        // as part of the Event_h template
        //
        StringBuffer includes = new StringBuffer();
        ArrayList<String> tmp = new ArrayList<String>(includeNames);
        Collections.sort(tmp);
        for (int i = 0, count = tmp.size() ; i < count ; i++)
        {
            String header = tmp.get(i);
            includes.append("#include \""+header+"\"\n");
        }

        // add buffers to the macro mapping to create the complete file
        // (see the Event_h template)
        //
        macros.addMapping(C_TYPE_DECS, definitions.toString());
        macros.addMapping(C_FUNCTION_DECS, prototypes.toString());
        macros.addMapping(C_INCLUDE_SECTION, includes.toString());

        // finally, create the header file
        //
        File header = createFile(ctx.getBaseOutputDir(), dir, fileName, EVENT_H_TEMPLATE, macros);
        if (header.exists() && header.length() > 0)
        {
            return includeName;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Generate the source file (implementation of the event header)
     */
    //--------------------------------------------
    private boolean generateNativeEventSource(Context ctx, String headerFileName)
        throws IOException
    {
        // create the MacroSource and start populating it
        //
        StringBuffer functions = new StringBuffer();
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_CONFIG_FLAG, ctx.getPaths().getNativeConfigFlag());

        // generate the encode/decode functions.  Since it's the same as the
        // POJO native code (and EventVariable subclasses CustomVariable),
        // just use a PojoNativeCodeProducer
        //
        PojoNativeCodeProducer pojoHelper = new PojoNativeCodeProducer();
        StringBuffer buffer = pojoHelper.generateNativePojoFunction(Processor.getEventProcessor().getWrappers(), false);
        functions.append(buffer);

        // add buffers to the macro mapping (see Event_c template)
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String sourceName = project+EVENT_SUFFIX;
        macros.addMapping(C_FUNCTION_DECS, functions.toString());
        macros.addMapping(C_INCLUDE_SECTION, "#include \""+headerFileName+"\"\n");

        // finally, create the header file
        //
        String dir = ctx.getPaths().getNativeApiSrcDir();
        File source = createFile(ctx.getBaseOutputDir(), dir, sourceName + ".c", EVENT_C_TEMPLATE, macros);
        if (source.exists() && source.length() > 0)
        {
            return true;
        }

        return false;
    }

    //--------------------------------------------
    /**
     * Generate the EventAdapter header that includes
     * the Listener function typedefs and a set of
     * function stubs for adding/removing listeners for
     * the supported Event objects (i.e. register_addAppEvent_listener)
     */
    //--------------------------------------------
    private String generateNativeEventAdapterHeader(Context ctx, String eventHeaderFileName)
        throws IOException
    {
        // determine the filename of the header
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+EVENT_ADAPTER_SUFFIX;
        String fileName = headerName+".h";

        // figure out the parent dir of the header (assuming we follow our 'public' convention).
        // ex:  app_mgr_pojo.h --> public/app_mgr/app_mgr_pojo.h
        //
        String dir = ctx.getPaths().getNativeApiHeaderDir();
        String parent = new File(dir).getName();
        String includeName = parent+"/"+fileName;       // app_mgr/app_mgr.h

        // create the MacroSource and start populating it
        //
        StringBuffer buffer = new StringBuffer();
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_HEADER_IFDEF, headerName.toUpperCase()+"_H");
        macros.addMapping(C_INCLUDE_SECTION, "#include \""+eventHeaderFileName+"\"\n");

        // for each Event that was created in the Event Header, create:
        //  - function typdef for the listener callback
        //  - register function (for event notification)
        //  - unregister function
        //
        EventProcessor eventProcessor = Processor.getEventProcessor();
        Iterator<Wrapper> loop = eventProcessor.getWrappers();
        while (loop.hasNext() == true)
        {
            // should be an EventWrapper.  Skip if not Native
            //
            EventWrapper eventWrapper = (EventWrapper) loop.next();
            if (eventWrapper.getLanguageMask().hasNative() == false)
            {
                continue;
            }

            // all we need is the event 'name' to create each
            // of the portions (one per event)
            //
            String eventName = eventWrapper.getVariable().getName();
            macros.addMapping(EVENT_CLASSNAME, eventName);
            buffer.append(FileHelper.translateBufferTemplate(ADAPTER_LISTENER_DEFS, macros));
            buffer.append(FileHelper.translateBufferTemplate(ADAPTER_LISTENER_REGISTER, macros));
            buffer.append(FileHelper.translateBufferTemplate(ADAPTER_LISTENER_UNREGISTER, macros));
        }

        // add buffers to the macro mapping
        //
        macros.addMapping(C_FUNCTION_DECS, buffer.toString());

        // finally, create the header file
        //
        File header = createFile(ctx.getBaseOutputDir(), dir, fileName, ADAPTER_H_TEMPLATE, macros);
        if (header.exists() && header.length() > 0)
        {
            return includeName;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Generate the code that implements the Event Adapter Header
     * This is quite complex because of how this fits between the
     * foundation and the consumer.
     */
    //--------------------------------------------
    private boolean generateNativeEventAdapterSource(Context ctx,  String adapterHeaderFileName)
        throws IOException
    {
        // for each Event, we need to generate sections of code to:
        //  - create local variables
        //    - linked list of listeners
        //    - mutex for the linked list
        //  - create local functions
        //    - handler to decode from JSON (utilize decode function in Event)
        //    - notify listeners (use the linked list)
        //
        // then make an overall event handler that is registered with the EventConsumer.h (one per service)
        // that will contain a large switch statement to take each event code:
        //  - if listeners for that event, decode and forward to listeners
        //

        // init some buffers to keep sections of generated code so that
        // we can bring it all together at the end
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName())+EVENT_SUFFIX;
        StringBuffer variables = new StringBuffer();
        StringBuffer prototypes = new StringBuffer();
        StringBuffer decoder = new StringBuffer();
        StringBuffer functions = new StringBuffer();

        // create the MacroSource and start populating it
        //
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_CONFIG_FLAG, ctx.getPaths().getNativeConfigFlag());
        macros.addMapping(C_INCLUDE_SECTION, "#include \""+adapterHeaderFileName+"\"\n");
        macros.addMapping(SERVER_PORT_NUM, Integer.toString(ctx.getEventPortNum()));
        macros.addMapping(SERVER_PORT_NAME, project.toUpperCase());

        EventProcessor eventProcessor = Processor.getEventProcessor();
        Iterator<Wrapper> loop = eventProcessor.getWrappers();
        while (loop.hasNext() == true)
        {
            // should be an EventWrapper.  Skip if not Native
            //
            EventWrapper eventWrapper = (EventWrapper) loop.next();
            if (eventWrapper.getLanguageMask().hasNative() == false)
            {
                continue;
            }

            // fill in specifics about this EventWrapper into the macros
            // so we can create all of the sections required for the File
            //
            EventVariable variable = (EventVariable)eventWrapper.getVariable();
            String eventName = variable.getName();
            macros.addMapping(EVENT_CLASSNAME, eventName);
            macros.addMapping("event_innerclass_init", variable.getAdapterNativeInitString("event"));
            macros.addMapping(EVENT_DECODE_FUNCTION, eventWrapper.getContextNative().getDecodeFunctionName());

            // convert event codes into a set of 'case' statements (for the switch)
            //
            String caseStatement = getEventCodeCaseStatement(eventWrapper);
            if (caseStatement != null)
            {
                // only add 'decode' for the switch statement if there are
                // event codes to potentially handle.
                //
                decoder.append(caseStatement);
                decoder.append(FileHelper.translateBufferTemplate(ADAPTER_DECODE_EVENT, macros));
            }

            variables.append(FileHelper.translateBufferTemplate(ADAPTER_VARIABLE_DEFS, macros));
            prototypes.append(FileHelper.translateBufferTemplate(ADAPTER_FUNCTION_DECS, macros));
            functions.append(FileHelper.translateBufferTemplate(ADAPTER_FUNCTION_BODY, macros));
            functions.append(FileHelper.translateBufferTemplate(ADAPTER_REGISTER_BODY, macros));
            functions.append(FileHelper.translateBufferTemplate(ADAPTER_UNREGISTER_BODY, macros));
        }

        macros.addMapping(C_FUNCTION_DECS, prototypes.toString());
        macros.addMapping(C_VARIABLE_DEFS, variables.toString());
        macros.addMapping(EVENT_CODE_HANDLER, decoder.toString());
        macros.addMapping(EVENT_C_FUNCTION_BODY, functions.toString());

        // finally, create the header file
        //
        String dir = ctx.getPaths().getNativeApiSrcDir();
        String sourceName = project+"Adapter";
        File source = createFile(ctx.getBaseOutputDir(), dir, sourceName + ".c", ADAPTER_C_TEMPLATE, macros);
        if (source.exists() && source.length() > 0)
        {
            return true;
        }

        // TODO: save source filename somewhere? maybe tracker?


        return false;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private String getEventCodeCaseStatement(EventWrapper wrapper)
    {
        StringBuffer retVal = new StringBuffer();

        Iterator<String> loop = wrapper.getEventCodes().iterator();
        while (loop.hasNext() == true)
        {
            // make a line that looks like:
            //     case EVENT_CODE:
            //
            retVal.append("        case ");
            retVal.append(loop.next());
            retVal.append(":\n");
        }

        // return null if no codes were set
        //
        if (retVal.length() == 0)
        {
            return null;
        }
        return retVal.toString();
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private StringBuffer appendCodes(List<EventProcessor.CodeDefinition> list)
    {
        // create a CodeFormatter to perform the spacing for us
        //
        CodeFormater format = new CodeFormater(4);

        // now loop through each and to the formatter
        //
        Iterator<EventProcessor.CodeDefinition> loop = list.iterator();
        while (loop.hasNext() == true)
        {
            // make the layout be:
            // #define name value
            //
            EventProcessor.CodeDefinition curr = loop.next();
            format.nextRow();
            format.setValue("#define", 0);
            format.setValue(curr.code, 1);
            format.setValue(Integer.toString(curr.value), 2);

            if (StringUtils.isEmpty(curr.comment) == false)
            {
                format.setValue("     // "+curr.comment, 3);
            }
        }

        // append to the StringBuffer
        //
        return format.toStringBuffer(0);
    }


}

