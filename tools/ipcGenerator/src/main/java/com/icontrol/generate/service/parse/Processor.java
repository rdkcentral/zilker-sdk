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

package com.icontrol.generate.service.parse;

import java.io.IOException;
import java.util.Map;
import java.util.LinkedHashMap;
import java.util.Iterator;
import java.util.List;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.io.CodeFormater;
import com.icontrol.generate.service.io.EventJavaCodeProducer;
import com.icontrol.generate.service.io.EventNativeCodeProducer;
import com.icontrol.generate.service.io.LanguageMask;
import com.icontrol.generate.service.io.MessageJavaCodeProducer;
import com.icontrol.generate.service.io.MessageNativeCodeProducer;
import com.icontrol.generate.service.io.PojoJavaCodeProducer;
import com.icontrol.generate.service.io.PojoNativeCodeProducer;
import com.icontrol.generate.service.variable.ArrayVariable;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.BooleanVariable;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.generate.service.variable.DateVariable;
import com.icontrol.generate.service.variable.EnumDefinition;
import com.icontrol.generate.service.variable.EnumVariable;
import com.icontrol.generate.service.variable.EventVariable;
import com.icontrol.generate.service.variable.JsonVariable;
import com.icontrol.generate.service.variable.MapVariable;
import com.icontrol.generate.service.variable.NumberVariable;
import com.icontrol.generate.service.variable.StringVariable;
import com.icontrol.xmlns.service.ICArrayVariable;
import com.icontrol.xmlns.service.ICBaseVariable;
import com.icontrol.xmlns.service.ICBoolVariable;
import com.icontrol.xmlns.service.ICCustomObject;
import com.icontrol.xmlns.service.ICDateVariable;
import com.icontrol.xmlns.service.ICEnumDefinition;
import com.icontrol.xmlns.service.ICEnumVariable;
import com.icontrol.xmlns.service.ICEvent;
import com.icontrol.xmlns.service.ICEventList;
import com.icontrol.xmlns.service.ICJsonVariable;
import com.icontrol.xmlns.service.ICMapVariable;
import com.icontrol.xmlns.service.ICNumVariable;
import com.icontrol.xmlns.service.ICService;
import com.icontrol.xmlns.service.ICStringVariable;

//-----------------------------------------------------------------------------
/**
 * Main driver for the code generation.  Called from "main" to take the top-level
 * XML node and populate/analyze the Enum, Pojo, Event, and Message objects.
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class Processor
{
    // lookup tables
    //
    private static Map<String,EnumDefinition> enumDefLookup = new LinkedHashMap<String,EnumDefinition>();
    private static Map<String,BaseVariable> variableLookup  = new LinkedHashMap<String,BaseVariable>();

    // processor helpers
    //
    private static EnumProcessor    enumProcessor;
    private static EventProcessor   eventProcessor;
    private static PojoProcessor    pojoProcessor;
    private static MessageProcessor messageProcessor;

    //--------------------------------------------
    /**
     * Parse the XML document and create BaseVariable,
     * PojoWrapper, EventWrapper, and MessageWrapper instances.
     */
    //--------------------------------------------
    public static void buildModel(ICService top, Context ctx)
    {
        // preload all variables defined in the XML document
        //
        preparseVariables(top);

        // pre-process by creating Wrapper representations
        //
        enumProcessor = EnumProcessor.buildModel(top);
        pojoProcessor = PojoProcessor.buildModel(top);
        eventProcessor = EventProcessor.buildModel(top);
        messageProcessor = MessageProcessor.buildModel(top);

        // now that all caches are populated, analyze
        // each so they can prepare themselves for
        // code generation
        //
        enumProcessor.analyze(top, ctx);
        pojoProcessor.analyze(top, ctx);
        eventProcessor.analyze(top, ctx);
        messageProcessor.analyze(top, ctx);
        
        // one more pass at enum & pojo to grab 
        // definitions that were not accessed by Event or IPC
        // 
        enumProcessor.analyzeAgain(ctx);
        pojoProcessor.analyzeAgain(ctx);
    }

    //--------------------------------------------
    /**
     * Generate code based on the XML provided to 'buildModel'
     */
    //--------------------------------------------
    public static boolean generateCode(Context ctx)
        throws IOException
    {
        // possibly print the hierarchy of generation (what goes first, next, last, etc)
        //
        if (ctx.isDebug() == true)
        {
            boolean doC = pojoProcessor.shouldGenerateC(ctx);
            boolean doJ = pojoProcessor.shouldGenerateJava(ctx);
            if (doC == true || doJ == true)
            {
                System.out.println("POJO definition order:");
                debugPrintWrappers(pojoProcessor.getWrappers());
            }

            doC = eventProcessor.shouldGenerateC(ctx);
            doJ = eventProcessor.shouldGenerateJava(ctx);
            if (doC == true || doJ == true)
            {
                System.out.println("Event definition order:");
                debugPrintWrappers(eventProcessor.getWrappers());
            }

            doC = messageProcessor.shouldGenerateC(ctx);
            doJ = messageProcessor.shouldGenerateJava(ctx);
            if (doC == true || doJ == true)
            {
                System.out.println("IPC definition order:");
                debugPrintWrappers(messageProcessor.getWrappers());
            }
        }

        // Use a set of CodeProducer objects to generate all of the
        // Java or Native code.  This allows us to not be concerned
        // with file production (since Java produces 1 file per Object
        // and C will combine several into headers).  There are 3
        // phases to produce all of the code for Client and Service:
        // 1. POJO & Enums in the API
        // 2. Events, Enums, and EventAdapters in the API
        // 3. IPC messages (client API & service handling)
        //

        // first, POJOs with any Enums that are part of that Section
        //
        if (pojoProcessor.shouldGenerateC(ctx) == true)
        {
            // native Enum and POJOs
            //
            PojoNativeCodeProducer pojoGenerator = new PojoNativeCodeProducer();
            pojoGenerator.generate(ctx, LanguageMask.NATIVE);
        }
        if (pojoProcessor.shouldGenerateJava(ctx) == true)
        {
            // Java Enum & POJOs
            //
            PojoJavaCodeProducer pojoGenerator = new PojoJavaCodeProducer();
            pojoGenerator.generate(ctx, LanguageMask.JAVA);
        }

        // second, Events & Adapters
        //
        if (eventProcessor.shouldGenerateC(ctx) == true)
        {
            EventNativeCodeProducer eventGenerator = new EventNativeCodeProducer();
            eventGenerator.generate(ctx, LanguageMask.NATIVE);
        }
        if (eventProcessor.shouldGenerateJava(ctx) == true)
        {
            EventJavaCodeProducer eventGenerator = new EventJavaCodeProducer();
            eventGenerator.generate(ctx, LanguageMask.JAVA);
        }

        // now IPC messages
        //
        if (messageProcessor.shouldGenerateC(ctx) == true)
        {
            MessageNativeCodeProducer msgGenerator = new MessageNativeCodeProducer();
            msgGenerator.generate(ctx, LanguageMask.NATIVE);
        }
        if (messageProcessor.shouldGenerateJava(ctx) == true)
        {
            MessageJavaCodeProducer msgGenerator = new MessageJavaCodeProducer();
            msgGenerator.generate(ctx, LanguageMask.JAVA);
        }

        return true;
    }

    //--------------------------------------------
    /**
     * Return the EnumProcessor for ProcessHelpers and Wrappers
     */
    //--------------------------------------------
    public static EnumProcessor getEnumProcessor()
    {
        return enumProcessor;
    }

    //--------------------------------------------
    /**
     * Return the EventProcessor for ProcessHelpers and Wrappers
     */
    //--------------------------------------------
    public static EventProcessor getEventProcessor()
    {
        return eventProcessor;
    }

    //--------------------------------------------
    /**
     * Return the PojoProcessor for ProcessHelpers and Wrappers
     */
    //--------------------------------------------
    public static PojoProcessor getPojoProcessor()
    {
        return pojoProcessor;
    }

    //--------------------------------------------
    /**
     * Return the MessageProcessor for ProcessHelpers and Wrappers
     */
    //--------------------------------------------
    public static MessageProcessor getMessageProcessor()
    {
        return messageProcessor;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private static void preparseVariables(ICService top)
    {
        // create containers
        //
        enumDefLookup = new LinkedHashMap<String, EnumDefinition>();
        variableLookup = new LinkedHashMap<String, BaseVariable>();

        // iterate through each of the major sections defined in the XML file
        // to create all of the BaseVariable instances in the document.  This will
        // pre-populate our cache so that we can make sense of the generation
        // effort (as well as resolve references and dependencies)
        //

        // first, the enumerations as they don't have dependencies.  Note
        // that these MUST be done first so that EnumVariables can be dereferenced
        //
        List<ICEnumDefinition> enumList = top.getTypedefList();
        for (int i = 0 ; i < enumList.size() ; i++)
        {
            // add the variable to our set so we can generate it first
            //
            parseEnumDefinition(enumList.get(i));
        }

        // now "POJO" definitions
        //
        List<ICCustomObject> pojoList = top.getPojoList();
        for (int i = 0 ; i < pojoList.size() ; i++)
        {
            // add to a list so we can generate the supporting Objects
            //
            createVariable(pojoList.get(i));
        }

        // now Event definitions (subclass of CustomVar - i.e. POJO)
        //
        if (top.isSetEventList() == true)
        {
            ICEventList eventList = top.getEventList();
            List<ICEvent> array = eventList.getEventList();
            for (int i = 0, count = array.size(); i < count; i++)
            {
                // parse the bean so we can get the children
                //
                ICEvent ev = array.get(i);
                createVariable(ev);
            }
        }

        // Note that we are not parsing the messages at this point.
        // Each "IPC Message" could refer to an Enum, Event, or Variable that
        // was created above - or it will be some primitive type (i.e. String, int, etc)
        //

        // now that all variables are parsed & cached, have each
        // resolve internal references to other variables, enums, etc
        //
        Iterator<String> loop = variableLookup.keySet().iterator();
        while (loop.hasNext() == true)
        {
            String varName = loop.next();
            BaseVariable var = variableLookup.get(varName);
            if (var != null)
            {
                var.resolveReferences();
            }
        }
    }

    //--------------------------------------------
    /**
     * If already created, returns the BaseVariable that
     * would repesent this bean, otherwise creates a new one
     */
    //--------------------------------------------
    public static BaseVariable getOrCreateVariable(ICBaseVariable bean)
    {
        // see if we have one
        //
        String varName = bean.getVarName();
        BaseVariable var = getVariableForName(varName);
        if (var == null)
        {
        	// enums should always fail the 'getVariableForName' since they
        	// only exist in the "definition" lookup.  Just fall below to
        	// 'create' the enumeration
            //
        }

        // if still null, create it
        //
        if (var == null)
        {
            var = createVariable(bean);
        }

        return var;
    }

    //--------------------------------------------
    /**
     * Create a BaseVariable instance from the XML bean
     */
    //--------------------------------------------
    private static BaseVariable createVariable(ICBaseVariable bean)
    {
        BaseVariable retVal = null;

        if (bean instanceof ICNumVariable)
        {
            retVal = new NumberVariable((ICNumVariable)bean);
        }
        else if (bean instanceof ICDateVariable)
        {
            retVal = new DateVariable((ICDateVariable)bean);
        }
        else if (bean instanceof ICStringVariable)
        {
            retVal = new StringVariable((ICStringVariable)bean);
        }
        else if (bean instanceof ICJsonVariable)
        {
            retVal = new JsonVariable((ICJsonVariable)bean);
        }
        else if (bean instanceof ICBoolVariable)
        {
            retVal = new BooleanVariable((ICBoolVariable)bean);
        }
        else if (bean instanceof ICEnumVariable)
        {
            // hopefully the corresponding EnumDefinition has already
        	// been processed.  Therefore, find it to use as the basis
        	//
        	ICEnumVariable enumBean = (ICEnumVariable)bean;
//        	EnumDefinition def = getEnumDefinitionForName(enumBean.getEnumTypeName());
        	
        	// create the new enumeration, and assign the 'varName' to make
        	// the 'isInstance' be true
        	//
            EnumVariable ev = new EnumVariable(enumBean);
            ev.resolveReferences();
        	
            // at this point, do not add the EnumVar to the 
            // lookup table.  we want a new instance for each
            // container that utilizes it, to allow different
            // uses of the 'varName' 
            // (ie. foo needs enum named 'x', bar needs enum named 'y')
            //
            return ev;
        }
        else if (bean instanceof ICArrayVariable)
        {
            retVal = new ArrayVariable((ICArrayVariable)bean);
        }
        else if (bean instanceof ICMapVariable)
        {
            retVal = new MapVariable((ICMapVariable)bean);
        }
        else if (bean instanceof ICEvent)
        {
            retVal = new EventVariable((ICEvent)bean);
        }
        else if (bean instanceof ICCustomObject)
        {
            // create and save so Objects can be created from them
            //
            retVal = new CustomVariable((ICCustomObject)bean);
        }

        // if we properly created a BaseVariable, add it
        // to our variable cache (but ignore dates, bools, strings, numbers)
        //
        if (retVal != null && 
            retVal.isNumber() == false && retVal.isBoolean() == false &&
            retVal.isString() == false && retVal.isDate() == false)
        {
            variableLookup.put(retVal.getName(), retVal);
            retVal.resolveReferences();
        }
        return retVal;
    }

    //--------------------------------------------
    /**
     * Parse and store the EnumDefinition from the XML bean
     * so that it can be used by EnumVariable objects.
     */
    //--------------------------------------------
    private static EnumDefinition parseEnumDefinition(ICEnumDefinition bean)
    {
        EnumDefinition def = new EnumDefinition(bean);
        enumDefLookup.put(def.getName(), def);

        return def;
    }

    //--------------------------------------------
    /**
     * Return the BaseVariable for a supplied name.
     */
    //--------------------------------------------
    public static BaseVariable getVariableForName(String name)
    {
        return variableLookup.get(name);
    }

    //--------------------------------------------
    /**
     * Return the CustomVariable for a supplied name (if known)
     */
    //--------------------------------------------
    public static CustomVariable getCustomForName(String name)
    {
        // lookup the variable with this name
        //
        BaseVariable possible = variableLookup.get(name);
        if (possible != null && possible instanceof CustomVariable)
        {
            return (CustomVariable)possible;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Return the EnumDefinition for a supplied name (if known)
     * Mainly used by EnumVariable objects to obtain the specifics
     * about a defined enumeration.
     */
    //--------------------------------------------
    public static EnumDefinition getEnumDefinitionForName(String name)
    {
        return enumDefLookup.get(name);
    }

    //--------------------------------------------
    /**
     * Return all of the known EnumDefinition objects
     */
    //--------------------------------------------
    public static Iterator<EnumDefinition> getAllEnumDefinitions()
    {
        return enumDefLookup.values().iterator();
    }

    //--------------------------------------------
    /**
     * Clear cached values.  Generally done before parsing
     * begins on a new input file.
     */
    //--------------------------------------------
    public static void clearCache()
    {
        // clear local cache
        //
        enumDefLookup.clear();
        variableLookup.clear();

        // have each processor do the same
        //
        enumProcessor = null;
        pojoProcessor = null;
        eventProcessor = null;
        messageProcessor = null;
    }
    
    private static void debugPrintWrappers(Iterator<Wrapper> loop)
    {
        // create a table to print 3 colums:
        //   object, do java, do native
        //
        CodeFormater table = new CodeFormater(3);
        table.nextRow();
        table.setValue("OBJECT", 0);
        table.setValue("JAVA", 1);
        table.setValue("NATIVE", 2);
        while (loop.hasNext() == true)
        {
            // add to the table
            //
            table.nextRow();
            Wrapper next = loop.next();
            table.setValue(next.toString(), 0);

            if (next.getLanguageMask().hasJava() == true)
            {
                table.setValue("yes", 1);
            }
            else
            {
                table.setValue("", 1);
            }

            if (next.getLanguageMask().hasNative() == true)
            {
                table.setValue("yes", 2);
            }
            else
            {
                table.setValue("", 2);
            }
        }
        System.out.println(table.toStringBuffer(3));
    }
}
