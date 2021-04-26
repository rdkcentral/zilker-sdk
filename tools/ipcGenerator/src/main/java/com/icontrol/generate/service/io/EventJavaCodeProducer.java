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

//-----------------------------------------------------------------------------

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.context.ContextJava;
import com.icontrol.generate.service.parse.EnumDefWrapper;
import com.icontrol.generate.service.parse.EventProcessor;
import com.icontrol.generate.service.parse.EventWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.Wrapper;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.EnumVariable;
import com.icontrol.generate.service.variable.EventVariable;
import com.icontrol.util.StringUtils;

/**
 * CodeProducer implementation for creating Native Events & EventAdapters
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class EventJavaCodeProducer extends EventCodeProducer
{
    private static final String EVENT_TEMPLATE        = "/com/icontrol/generate/service/templates/event/Event_java";
    private static final String EVENT_CODES_TEMPLATE  = "/com/icontrol/generate/service/templates/event/EventCodes_java";
    private static final String LISTENER_TEMPLATE     = "/com/icontrol/generate/service/templates/event/Listener_java";
    private final static String ADAPTER_TEMPLATE      = "/com/icontrol/generate/service/templates/event/EventAdapter_java";
    private static final String ENUM_TEMPLATE         = "/com/icontrol/generate/service/templates/pojo/Enum_java";

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public EventJavaCodeProducer()
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
        // generate the event codes interface
        //
        createEventCodesFile(ctx);

        // loop through all Event/Enum objects twice
        // first - calculate class, package, and filenames (so second iteration can refer to them for 'import')
        // second - generate code
        //
        Iterator<Wrapper> events = Processor.getEventProcessor().getWrappers();
        while (events.hasNext() == true)
        {
            Wrapper wrap = events.next();
            if (wrap.getLanguageMask().hasJava() == true)
            {
                assignNames(ctx, wrap);
            }
        }
        Iterator<Wrapper> enums = Processor.getEnumProcessor().getWrappers();
        while (enums.hasNext() == true)
        {
            Wrapper wrap = enums.next();
            if (wrap.getLanguageMask().hasJava() == true &&
                wrap.getSectionMask().inEventSection() == true)
            {
                assignNames(ctx, wrap);
            }
        }
        Iterator<Wrapper> pojos = Processor.getPojoProcessor().getWrappers();
        while (pojos.hasNext() == true)
        {
            Wrapper wrap = pojos.next();
            if (wrap.getLanguageMask().hasJava() == true &&
                wrap.getSectionMask().inEventSection() == true)
            {
                assignNames(ctx, wrap);
            }
        }

        // generate all Enums with an "event" section associated with them
        // NOTE: we do enumerations first in case the Event definitions
        //       have dependencies on them
        //
        enums = Processor.getEnumProcessor().getWrappers();
        while (enums.hasNext() == true)
        {
            EnumDefWrapper wrap = (EnumDefWrapper)enums.next();
            // Only generate under event if its exclusive to event
            if (wrap.getSectionMask().inEventSection() == true &&
                wrap.getSectionMask().inMessageSection() == false)
            {
                // generate the Java code for this Enum
                //
                generateWrapperFile(ctx, lang, wrap);
            }
        }

        // generate all Event objects in Java
        //
        events = Processor.getEventProcessor().getWrappers();
        while (events.hasNext() == true)
        {
            EventWrapper wrap = (EventWrapper)events.next();
            if (wrap.getLanguageMask().hasJava() == true)
            {
                // generate the Java code for this POJO
                //
                generateWrapperFile(ctx, lang, wrap);
            }
        }

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
        // skip if this wrapper doesn't support Java as an output
        //
        if (wrapper.getLanguageMask().hasJava() == false)
        {
            return false;
        }

        // generate the Java class for a single Event/Enum wrapper
        //
        StringBuffer buffer = generateWrapperBuffer(ctx, lang, wrapper);
        if (buffer == null)
        {
            return false;
        }

        // extract the filename from the wrapper so we can save the file
        //
        File parent = ctx.getBaseOutputDir();
        String eventDir = ctx.getPaths().getEventDir();
        String pkgDir = getDirForPackage(wrapper.getContextJava().getJavaPackageName());

        // save the event/enum file
        //
        createFile(parent, eventDir + "/" + pkgDir, wrapper.getContextJava().getJavaFileName(), buffer.toString());

        if (wrapper instanceof EventWrapper)
        {
            // generate event adapter & listener
            //
            createEventListenerFile(ctx, (EventWrapper)wrapper);
            createEventAdapterFile(ctx, (EventWrapper)wrapper);
        }

        return true;
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
        // skip if this wrapper doesn't support Java as an output
        //
        if (wrapper.getLanguageMask().hasJava() == false)
        {
            return null;
        }

        // calculate class, package, and file names
        //
        BaseVariable var = wrapper.getVariable();
        String cName = StringUtils.camelCase(var.getJavaType());
        String packageName = ctx.getPaths().getEventPackage();
        String fileName = cName+".java";

        // generate the Java class for a single Event/Enum wrapper
        // done in 3 sections:
        //  1. object definition
        //  2. getters/setters
        //  3. encode/decode methods
        //
        String template = null;
        MacroSource map = null;
        if (var.isEvent() == true)
        {
            // get the mappings to generate Java code for this POJO
            //
            EventVariable eventVar = (EventVariable)var;
            map = eventVar.generateJavaPojo(2);
            template = EVENT_TEMPLATE;

            // get Wrappers this depends on so we can build up the 'imports'
            //
            EventWrapper event = (EventWrapper)wrapper;
            HashSet<String> importSet = new HashSet<String>();
            Iterator<Wrapper> depends = event.getDependencies();
            while (depends.hasNext() == true)
            {
                Wrapper dep = depends.next();
                String depPkg = dep.getContextJava().getJavaPackageName();
                String depClass = dep.getContextJava().getJavaClassName();
                if (depPkg != null && depClass != null)
                {
                    importSet.add("import "+depPkg+"."+depClass+";");
                }
            }
            ArrayList<String> tmp = new ArrayList<String>(importSet);
            Collections.sort(tmp);
            StringBuffer importBuff = new StringBuffer();
            for (int i = 0, count = tmp.size() ; i < count ; i++)
            {
                importBuff.append(tmp.get(i)+"\n");
            }
            map.addMapping(JAVA_IMPORT_SECTION, importBuff.toString());

            // pre-allocate any arrays this event has (inside the constructor)
            //
            StringBuffer construct = new StringBuffer();
            List<BaseVariable> subvars = eventVar.getVariables();
            for (int i = 0 ; i < subvars.size() ; i++)
            {
                BaseVariable sub = subvars.get(i);
                if (sub.isArray() == true)
                {
                    construct.append("        ");
                    construct.append(sub.getName()+" = new "+sub.getJavaType()+"();\n");
                }
            }
            map.addMapping(CONSTRUCTOR_MARKER, construct.toString());

            // add the event codes this event is created for
            //
            map.addMapping(APPLY_EVENT_CODE, event.getEventCodesString());

            // define the superclass, and default to BaseEvent
            //
            String parent = eventVar.getParentClassName();
            if (StringUtils.isEmpty(parent) == true)
            {
                parent = "BaseEvent";
            }
            map.addMapping(EVENT_SUPER_CLASS, parent);

            // add the interfaces this event implements
            // start out with what we have via the EventVariable
            // then append the eventCodes & JavaEvent
            //
            String interf = eventVar.getInterfaceString();
            String codes = getEventCodesInterfaceName(ctx, event, false);
            if (codes != null)
            {
//                if (interf.length() > 0)
                {
                    interf += ", ";
                }
                interf += codes;
            }
            if (event.getEventCodes().size() > 0)
            {
//                if (interf.length() > 0)
                {
                    interf += ", ";
                }
                interf += "JavaEvent";
            }
//            if (interf.length() > 0)
//            {
//                interf = "implements "+interf;
//            }
            map.addMapping(IMPLEMENTS_MARKER, interf);
        }
        else if (var.isEnum() == true)
        {
            // get the mapping to generate Java code for this Enum
            //
            EnumVariable enumVariable = (EnumVariable)var;
            map = enumVariable.generateJavaTransport();
            template = ENUM_TEMPLATE;
        }
        else
        {
            // not able to generate the buffer
            //
            return null;
        }

        // add common items to the mapping
        //
        map.addMapping(JAVA_CLASS, cName);
        map.addMapping(JAVA_PKG, packageName);
        map.addMapping(SERVICE_KEY, ctx.getServiceName());

        // save names to our Context so callers can use this to
        // create the physical file (and know where)
        //
        ContextJava cj = wrapper.getContextJava();
        cj.setJavaClassName(cName);
        cj.setJavaPackageName(packageName);
        cj.setJavaFileName(fileName);

        // finally, translate using the 'template'
        //
        StringBuffer retVal = new StringBuffer();
        retVal.append(FileHelper.translateBufferTemplate(template, map));
        return retVal;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private String getEventCodesInterfaceName(Context ctx, EventWrapper wrapper, boolean fullyQualified)
    {
        // very inefficient, but recalculate each time asked since
        // the 'wrapper' isn't always the same
        //
        if (wrapper != null && wrapper.getEventCodes().size() == 0)
        {
            return null;
        }

        // get the name of the project + EventCodes
        //
        String valid = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String projName = StringUtils.camelCase(valid).replace(' ','_') + "_EventCodes";

        if (fullyQualified == true)
        {
            // use package from context + project + EventCodes
            //
            String pkg = ctx.getPaths().getEventPackage();
            return pkg + "." +projName;
        }
        return projName;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void createEventCodesFile(Context ctx)
        throws IOException
    {
        StringBuffer buff = new StringBuffer();

        // ask the EventProcessor for the list of defines & event codes
        //
        EventProcessor proc = Processor.getEventProcessor();
        List<EventProcessor.CodeDefinition> defines = proc.getDefineCodes();
        if (defines.size() > 0)
        {
            // add a nice comment above these (if any to add)
            //
            buff.append("    // various constant values\n    //\n");
            CodeFormater format = formatEventCodes(defines);
            buff.append(format.toStringBuffer(4));
        }

        List<EventProcessor.CodeDefinition> codes = proc.getEventCodes();
        if (codes.size() > 0)
        {
            // add a nice comment above these
            //
            buff.append("    // available event codes\n    //\n");
            CodeFormater format = formatEventCodes(codes);
            buff.append(format.toStringBuffer(4));
        }

        // bail if nothing to produce
        //
        if (buff.length() == 0)
        {
            return;
        }

        // translate into our event codes file
        //
        String eventDir = ctx.getPaths().getEventDir();
        String pkg = ctx.getPaths().getEventPackage();
        String pkgDir = getDirForPackage(pkg);
        String name = getEventCodesInterfaceName(ctx, null, false);

        // create map with basic information
        //
        MacroSource map = new MacroSource();
        map.addMapping(JAVA_CLASS, name);
        map.addMapping(JAVA_PKG, pkg);
        map.addMapping(JAVA_VAR_SECTION, buff.toString());

        // add server "event port"
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+EVENT_SUFFIX;
        map.addMapping(SERVER_PORT_NUM, Integer.toString(ctx.getEventPortNum()));
        map.addMapping(SERVER_PORT_NAME, headerName.toUpperCase());

        // perform substitution against our template to create the file
        //
        createFile(ctx.getBaseOutputDir(), eventDir + "/" + pkgDir, name + ".java", EVENT_CODES_TEMPLATE, map);
    }

    //--------------------------------------------
    /**
     * Uses a template to generate the EventListener .java file
     */
    //--------------------------------------------
    private void createEventListenerFile(Context ctx, EventWrapper wrapper)
        throws IOException
    {
        // calculate class, package, and file names
        //
        BaseVariable var = wrapper.getVariable();
        String cName = StringUtils.camelCase(var.getJavaType());
        String packageName = ctx.getPaths().getEventPackage();
        String fileName = cName+"Listener.java";
        String eventDir = ctx.getPaths().getEventDir();
        String pkg = ctx.getPaths().getEventPackage();
        String pkgDir = getDirForPackage(pkg);

        // create a macro map
        //
        MacroSource map = new MacroSource();
        map.addMapping(JAVA_CLASS, cName);
        map.addMapping(JAVA_PKG, packageName);
        map.addMapping(SERVICE_KEY, ctx.getServiceName());

        // perform substitution against our template to create the file
        //
        createFile(ctx.getBaseOutputDir(), eventDir + "/" + pkgDir, fileName, LISTENER_TEMPLATE, map);
    }

    //--------------------------------------------
    /**
     * Uses a template to generate the EventListener .java file
     */
    //--------------------------------------------
    private void createEventAdapterFile(Context ctx, EventWrapper wrapper)
        throws IOException
    {
        // calculate class, package, and file names
        //
        BaseVariable var = wrapper.getVariable();
        String cName = StringUtils.camelCase(var.getJavaType());
        String packageName = ctx.getPaths().getEventPackage();
        String fileName = cName+"Adapter.java";
        String eventDir = ctx.getPaths().getEventDir();
        String pkg = ctx.getPaths().getEventPackage();
        String pkgDir = getDirForPackage(pkg);

        String eventCodesInterface = getEventCodesInterfaceName(ctx, wrapper, false);
        String serviceUpper = StringUtils.makeValidIdentifier(ctx.getServiceName()).toUpperCase();

        // create a macro map
        //
        MacroSource map = new MacroSource();
        map.addMapping(JAVA_CLASS, cName);
        map.addMapping(JAVA_PKG, packageName);
        map.addMapping(SERVICE_KEY, ctx.getServiceName());
        map.addMapping("event_codes_interface", eventCodesInterface);
        map.addMapping("service_upper", serviceUpper);

        // logic to determine if an incoming event should be processed
        //
        StringBuffer supportEvent = new StringBuffer();
        StringBuffer decodeEvent = new StringBuffer();
        List<String> eventCodeNames = wrapper.getEventCodes();
        for (int i = 0, count = eventCodeNames.size() ; i < count ; i++)
        {
            // add something similar to:
            //
            //  if (eventCode.equals(CommTrouble_codes.COMM_CAPABILITIES_CHANGED_EVENT) == true)
            //  {
            //      event = new CapabilitiesChangedEvent(0);
            //  }
            //
            decodeEvent.append("            ");
            if (count > 1)
            {
                if (i == 0)
                {
                    decodeEvent.append("if (");
                }
                else
                {
                    decodeEvent.append("    ");
                }
            }
            else
            {
                decodeEvent.append("if ");
            }
            decodeEvent.append("(eventCode == "+eventCodesInterface+".");
            decodeEvent.append(eventCodeNames.get(i));
            decodeEvent.append(" == true)");
            if (count > 1)
            {
                if (i+1 >= count)
                {
                    decodeEvent.append(")\n");
                }
                else
                {
                    decodeEvent.append(" || \n");
                }
            }
            else
            {
                decodeEvent.append('\n');
            }
        }

        // for 'supports event' it's the same as the decode, except for what we do inside the 'if' block
        //
        supportEvent.append(decodeEvent);
        supportEvent.append("            {\n");
        supportEvent.append("                rc = true;\n");
        supportEvent.append("            }\n");
        decodeEvent.append("            {\n");
        decodeEvent.append("                event = new ${"+JAVA_CLASS+"}(0);\n");
        decodeEvent.append("            }\n");

        map.addMapping(DECODE_JAVA_EVENT, decodeEvent.toString());
        map.addMapping(SUPPORT_JAVA_EVENT, supportEvent.toString());

        // perform substitution against our template to create the file
        //
        createFile(ctx.getBaseOutputDir(), eventDir + "/" + pkgDir, fileName, ADAPTER_TEMPLATE, map);
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private CodeFormater formatEventCodes(List<EventProcessor.CodeDefinition> list)
    {
        // add each code to the interface, but use a
        // CodeFormatter to perform the spacing for us
        //
        CodeFormater format = new CodeFormater(5);
        for (int i = 0, count = list.size() ; i < count ; i++)
        {
            EventProcessor.CodeDefinition curr = list.get(i);

            format.nextRow();
            format.setValue("public static final int", 0);
            format.setValue(curr.code, 1);
            format.setValue(" = ", 2);
            format.setValue(curr.value + ";", 3);

            if (StringUtils.isEmpty(curr.comment) == false)
            {
                format.setValue("     // "+curr.comment, 4);
            }
        }

        return format;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void assignNames(Context ctx, Wrapper wrapper)
    {
        // calculate class, package, and file names
        //
        BaseVariable var = wrapper.getVariable();
        String cName = StringUtils.camelCase(var.getJavaType());
        String packageName = ctx.getPaths().getEventPackage();
        String fileName = cName+".java";

        ContextJava cj = wrapper.getContextJava();

        // skip if this wrapper has been generated
        //
        if (cj.wasGenerated() == false)
        {
            cj.setJavaClassName(cName);
            cj.setJavaPackageName(packageName);
            cj.setJavaFileName(fileName);
        }
    }
}
