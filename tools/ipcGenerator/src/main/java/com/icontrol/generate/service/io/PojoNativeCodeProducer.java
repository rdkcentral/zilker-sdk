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
import java.util.Iterator;
import java.util.List;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.generate.service.parse.EnumDefWrapper;
import com.icontrol.generate.service.parse.EventWrapper;
import com.icontrol.generate.service.parse.PojoWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.ProcessorHelper;
import com.icontrol.generate.service.parse.Wrapper;
import com.icontrol.generate.service.variable.ArrayVariable;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.generate.service.variable.JsonVariable;
import com.icontrol.generate.service.variable.MapVariable;
import com.icontrol.generate.service.variable.StringVariable;
import com.icontrol.util.StringUtils;

//-----------------------------------------------------------------------------
/**
 * CodeProducer for Native POJOs
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class PojoNativeCodeProducer extends PojoCodeProducer
{
    // header templates
    //
    private final static String POJO_H_TEMPLATE             = "/com/icontrol/generate/service/templates/pojo/Pojo_h";
    private final static String ENUM_DEFS_TEMPLATE          = "/com/icontrol/generate/service/templates/pojo/Pojo_enum_defs_h";
    private final static String POJO_DEFS_TEMPLATE          = "/com/icontrol/generate/service/templates/pojo/Pojo_encode_defs_h";
    private final static String POJO_FUNC_TEMPLATE          = "/com/icontrol/generate/service/templates/pojo/Pojo_encode_decs_h";
    private final static String POJO_ENCODE_PROTO_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/encode_func_proto_h";
    private final static String POJO_ENCODE_BODY_TEMPLATE   = "/com/icontrol/generate/service/templates/pojo/encode_func_c";
    private final static String POJO_ENCODE_EVENT_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/encode_event_func_c";
    private final static String POJO_DECODE_PROTO_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/decode_func_proto_h";
    private final static String POJO_DECODE_BODY_TEMPLATE   = "/com/icontrol/generate/service/templates/pojo/decode_func_c";
    private final static String POJO_DECODE_EVENT_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/decode_event_func_c";

    private final static String POJO_CLONE_PROTO_TEMPLATE   = "/com/icontrol/generate/service/templates/pojo/cloneable_func_h";
    private final static String POJO_CLONE_BODY_TEMPLATE    = "/com/icontrol/generate/service/templates/pojo/cloneable_func_c";
    private final static String POJO_CREATE_PROTO_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/create_func_h";
    private final static String POJO_CREATE_BODY_TEMPLATE   = "/com/icontrol/generate/service/templates/pojo/create_func_c";
    private final static String POJO_DESTROY_PROTO_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/destroy_func_h";
    private final static String POJO_DESTROY_BODY_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/destroy_func_c";
    private final static String POJO_DESTROY_FROM_LIST_PROTO_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/destroy_fromList_func_h";
    private final static String POJO_DESTROY_FROM_LIST_BODY_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/destroy_fromList_func_c";
    private final static String POJO_DESTROY_FROM_MAP_PROTO_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/destroy_fromMap_func_h";
    private final static String POJO_DESTROY_FROM_MAP_BODY_TEMPLATE   = "/com/icontrol/generate/service/templates/pojo/destroy_fromMap_func_c";
    private final static String POJO_SET_LIST_PROTO_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/set_list_func_h";
    private final static String POJO_SET_LIST_BODY_TEMPLATE   = "/com/icontrol/generate/service/templates/pojo/set_list_func_c";
    private final static String POJO_SET_STR_LIST_PROTO_TEMPLATE  = "/com/icontrol/generate/service/templates/pojo/set_listStr_func_h";
    private final static String POJO_SET_STR_LIST_BODY_TEMPLATE   = "/com/icontrol/generate/service/templates/pojo/set_listStr_func_c";
    private final static String POJO_SET_LIST_NONPTR_PROTO_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/set_listNonPtr_func_h";
    private final static String POJO_SET_LIST_NONPTR_BODY_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/set_listNonPtr_func_c";

    // source templates
    //
    private final static String POJO_C_TEMPLATE             = "/com/icontrol/generate/service/templates/pojo/Pojo_c";


    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public PojoNativeCodeProducer()
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
        // generate Native code into 2 files (header & source)
        //
        String headerName = generateNativeHeaderFile(ctx);
        generateNativeSourceFile(ctx, headerName);

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
        // not applicable to native code generation
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
        // not applicable since there are more specific methods
        // available for generating sections of POJO code
        //
        return null;
    }

    //--------------------------------------------
    /**
     * Create the .h file that defines the Enum, POJO structs,
     * and encode/decode function declarations.
     *
     * Returns the "include" filename of the header created
     * so that other code generation knows how to include
     * this header file.
     */
    //--------------------------------------------
    private String generateNativeHeaderFile(Context ctx)
        throws IOException
    {
        // determine the filename of the header
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String headerName = project+"_pojo";
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

        // first the Enums that that are not covered by Event generation
        // each Enum that is NOT used strictly for Events should be added
        // to this buffer.  Since this is for the header, there should
        // be a "typedef enum" for each enumeration.
        //
        EnumNativeCodeProducer enumProducer = new EnumNativeCodeProducer();
        StringBuffer enumBuffer = enumProducer.generateNativeEnums(ctx, getEnumsToGenerate(), includeName);
        if (enumBuffer.length() > 0)
        {
            // load the ENUM_DEFS_TEMPLATE and inject the Enum "typedefs"
            //
            definitions.append(FileHelper.translateSimpleBuffer(ENUM_DEFS_TEMPLATE, C_TYPE_DECS, enumBuffer.toString()));
        }

        // now the typedefs of the POJOs.  This buffer should contain the
        // "typedef struct" for each logical POJO so it can be inserted into the header
        //
        StringBuffer pojoDefBuffer = generateNativePojoDefs(Processor.getPojoProcessor().getWrappers(), includeName);
        if (pojoDefBuffer.length() > 0)
        {
            // load the POJO_DEFS_TEMPLATE and inject the POJO "typedefs"
            //
            definitions.append(FileHelper.translateSimpleBuffer(POJO_DEFS_TEMPLATE, C_TYPE_DECS, pojoDefBuffer.toString()));
        }

        // finally, the create, destroy, encode, and decode function prototypes for each POJO
        //
        StringBuffer pojoProtoBuffer = generateNativePojoFunction(Processor.getPojoProcessor().getWrappers(), true);
        if (pojoProtoBuffer.length() > 0)
        {
            // load the POJO_FUNC_TEMPLATE and inject the POJO function prototypes (encode/decode)
            //
            definitions.append(FileHelper.translateSimpleBuffer(POJO_FUNC_TEMPLATE, C_FUNCTION_DECS, pojoProtoBuffer.toString()));
        }

        // add typedef & function prototypes to the macro mapping so we can
        // create the complete header file (see Pojo_h template)
        //
        macros.addMapping(C_TYPE_DECS, definitions.toString());
        macros.addMapping(C_FUNCTION_DECS, prototypes.toString());

        // add any freeform messages defined in the Native Pragma
        //
        List<String> freeform = ctx.getPaths().getNativeFreeforms();
        if (freeform != null && freeform.size() > 0)
        {
            StringBuffer freeBuff = new StringBuffer();
            Iterator<String> freeLoop = freeform.iterator();
            while (freeLoop.hasNext() == true)
            {
                freeBuff.append(freeLoop.next());
                freeBuff.append('\n');
            }
            macros.addMapping(C_FREE_FORM, freeBuff.toString());
        }

        // finally, create the header file
        //
        File header = createFile(ctx.getBaseOutputDir(), dir, fileName, POJO_H_TEMPLATE, macros);
        if (header.exists() && header.length() > 0)
        {
            return includeName;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * The source file counterpart to the header
     */
    //--------------------------------------------
    private boolean generateNativeSourceFile(Context ctx, String headerFileName)
        throws IOException
    {
        // create the MacroSource and start populating it
        //
        StringBuffer functions = new StringBuffer();
        MacroSource macros = new MacroSource();
        macros.addMapping(SERVICE_KEY, ctx.getServiceName());
        macros.addMapping(C_CONFIG_FLAG, ctx.getPaths().getNativeConfigFlag());

        // generate the function body.  We'll use the same mechanism used
        // to create the function declarations so that the templates match
        //
        StringBuffer buffer = generateNativePojoFunction(Processor.getPojoProcessor().getWrappers(), false);
        functions.append(buffer);

        // add the function bodies to the macro mapping to create
        // the complete source file (see Pojo_c template)
        //
        String project = StringUtils.makeValidIdentifier(ctx.getServiceName());
        String sourceName = project+"_pojo";
        macros.addMapping(C_FUNCTION_DECS, functions.toString());
        macros.addMapping(C_INCLUDE_SECTION, "#include \""+headerFileName+"\"\n");

        // finally, create the header file
        //
        String dir = ctx.getPaths().getNativeApiSrcDir();
        File source = createFile(ctx.getBaseOutputDir(), dir, sourceName + ".c", POJO_C_TEMPLATE, macros);
        if (source.exists() && source.length() > 0)
        {
            return true;
        }

        // TODO: save source filename somewhere? maybe tracker?

        return false;
    }

    //--------------------------------------------
    /**
     * Create typedef for POJOs to be inserted into the Header
     */
    //--------------------------------------------
    StringBuffer generateNativePojoDefs(Iterator<Wrapper> loop, String includeName)
    {
        StringBuffer retVal = new StringBuffer();

        // loop through all pojos
        //
        while (loop.hasNext() == true)
        {
            // skip if Java only
            //
            Wrapper pojo = loop.next();
            if (pojo.getLanguageMask().hasNative() == false)
            {
                // not native
                continue;
            }

            // skip if already produced
            //
            if (pojo.getContextNative().getHeaderFileName() != null)
            {
                // already did this one
                continue;
            }

            // first process anything this pojo depends on, because C
            // is very sensitive to what gets defined first
            //
            if (pojo instanceof PojoWrapper)
            {
                PojoWrapper pw = (PojoWrapper)pojo;
                Iterator<Wrapper> depList = pw.getDependencies();
                StringBuffer sub = generateNativePojoDefs(depList, includeName);
                if (sub != null && sub.length() > 0)
                {
                    retVal.append(sub);
                }
            }
            else if (pojo instanceof EventWrapper)
            {
                EventWrapper ew = (EventWrapper)pojo;
                Iterator<Wrapper> depList = ew.getDependencies();
                StringBuffer sub = generateNativePojoDefs(depList, includeName);
                if (sub != null && sub.length() > 0)
                {
                    retVal.append(sub);
                }
            }
            else if (pojo instanceof EnumDefWrapper)
            {
            	EnumDefWrapper edw = (EnumDefWrapper)pojo;
            	EnumNativeCodeProducer helper = new EnumNativeCodeProducer();
            	StringBuffer sub = new StringBuffer();
            	helper.generateNativeWrapperBuffer(null, edw, sub);
            	if (sub.length() > 0)
            	{
            		retVal.append(sub);
                    pojo.getContextNative().setHeaderFileName(includeName);
            	}
            	continue;
            }

            // let the CustomVariable create the 'typdef'
            //
            CustomVariable variable = (CustomVariable)pojo.getVariable();
            StringBuffer def = variable.getNativeStructString();
            if (def != null)
            {
                retVal.append(def);
                retVal.append('\n');
            }

            // tell the wrapper what header path this was generated
            // in so consumers know how to #include for this enum
            //
            pojo.getContextNative().setHeaderFileName(includeName);
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * Create POJO encode/decode functions for the Header or Source
     */
    //--------------------------------------------
    StringBuffer generateNativePojoFunction(Iterator<Wrapper> loop, boolean isPrototype)
        throws IOException
    {
        StringBuffer retVal = new StringBuffer();

        // loop through all pojos
        //
        while (loop.hasNext() == true)
        {
            // utilize templates for most of the structure
            // and the CustomVariable for the specifics
            //
            Wrapper pojo = loop.next();
            if (pojo.getLanguageMask().hasNative() == false)
            {
                continue;
            }
            CustomVariable variable = (CustomVariable)pojo.getVariable();

            // populate a MacroSource with:
            //    pojo_c_varname
            //    pojo_classname
            //    pojo_encode_body (if not prototype)
            //
            MacroSource ms = new MacroSource();
            ms.addMapping(POJO_C_VARNAME, variable.getLocalName());
            ms.addMapping(POJO_CLASSNAME, variable.getCtype());
            ContextNative ctx = pojo.getContextNative();

            // add functions to create/destroy
            //
            String createFunction = generateNativeCreate(variable, isPrototype, ms, retVal);
            String destroyFunction = generateNativeDestroy(variable, isPrototype, ms, retVal);
            String destroyFromListFunc = null;
            String destroyFromMapFunc = null;

            // if this object is part of a 'list', make a 'linkedListItemFreeFunc' implementation
            // so this can be released when destroying a linked list
            //
            if (pojo.isInList() == true)
            {
            	destroyFromListFunc = generateNativeListDestroy(variable, isPrototype, ms, retVal);
            }

            // if this object is part of a 'map', make a 'hashMapFreeFunc' implementation
            // so this can be released when destroying a map
            //
            if (pojo.isInMap() == true)
            {
            	destroyFromMapFunc = generateNativeMapDestroy(variable, isPrototype, ms, retVal);
            }

            if (variable.isCloneable() == true)
            {
            	// add a 'clone' function
            	//
            	if (isPrototype == false)
            	{
            		// use the body template
            		//
            		retVal.append(FileHelper.translateBufferTemplate(POJO_CLONE_BODY_TEMPLATE, ms));
            	}
            	else
            	{
            		// just the function prototype
            		//
            		retVal.append(FileHelper.translateBufferTemplate(POJO_CLONE_PROTO_TEMPLATE, ms));
            	}
            }

            // save function names that were just created into the ContextNative of the PojoWrapper
            // it will be needed by IPC or Event when encoding/decoding
            //
            ctx.setCreateFunctionName(createFunction);
            ctx.setDestroyFunctionName(destroyFunction);
            ctx.setDestroyFromListFunctionName(destroyFromListFunc);
            ctx.setDestroyFromMapFunctionName(destroyFromMapFunc);

            // create get/set function for each array/map variable
            //
            generateNativeGetters(variable, isPrototype, ms, retVal, pojo.getContextNative());
            generateNativeSetters(variable, isPrototype, ms, retVal, pojo.getContextNative());

            String encodeFunctionName = generateNativeEncode(ctx, variable, isPrototype, ms, retVal);
            String decodeFunctionName = generateNativeDecode(ctx, variable, isPrototype, ms, retVal);

            // save function names that were just created into the ContextNative of the PojoWrapper
            // it will be needed by IPC or Event when encoding/decoding
            //
            ctx.setEncodeFunctionName(encodeFunctionName);
            ctx.setDecodeFunctionName(decodeFunctionName);
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * Create the POJO encode function for Header or Source
     * Returns the function name of the generated code so that
     * we can later utilize this function
     */
    //--------------------------------------------
    private String generateNativeEncode(ContextNative ctx, CustomVariable var, boolean isPrototype, MacroSource ms, StringBuffer buffer)
        throws IOException
    {
        if (isPrototype == false)
        {
            // do the whole function content
            //
            StringBuffer encode = var.getNativeEncodeLines(ctx, 1, "root", false, "pojo", var.getJsonKey());
            if (encode != null)
            {
                // add to macro
                //
                ms.addMapping(POJO_ENCODE_BODY, encode.toString());
            }

            // translate
            //
            if (var.isEvent() == true)
            {
                // event has different way to create the 'root'
                buffer.append(FileHelper.translateBufferTemplate(POJO_ENCODE_EVENT_TEMPLATE, ms));
            }
            else
            {
                // standard way to create the 'root'
                buffer.append(FileHelper.translateBufferTemplate(POJO_ENCODE_BODY_TEMPLATE, ms));
            }
        }
        else
        {
            // just the function prototype
            //
            buffer.append(FileHelper.translateBufferTemplate(POJO_ENCODE_PROTO_TEMPLATE, ms));
        }

        // TODO: make this cleaner, but need to return the function name (not parms)
        //       so that callers know how to invoke this function
        //
        return "encode_"+ms.getVariable(POJO_CLASSNAME)+"_toJSON";
    }

    //--------------------------------------------
    /**
     * Create the POJO decode function for Header or Source
     * Returns the function name of the generated code so that
     * we can later utilize this function
     */
    //--------------------------------------------
    private String generateNativeDecode(ContextNative ctx, CustomVariable var, boolean isPrototype, MacroSource ms, StringBuffer buffer)
        throws IOException
    {
        if (isPrototype == false)
        {
            // do the whole function content
            //
            StringBuffer decode = var.getNativeDecodeLines(ctx, 2, "buffer", false, "pojo", var.getJsonKey());
            if (decode != null)
            {
                // add to macro
                //
                ms.addMapping(POJO_DECODE_BODY, decode.toString());
            }

            // translate
            //
            if (var.isEvent() == true)
            {
                // event has different template to decode BaseEvent first.
                buffer.append(FileHelper.translateBufferTemplate(POJO_DECODE_EVENT_TEMPLATE, ms));
            }
            else
            {
                // standard way to decode the JSON string
                buffer.append(FileHelper.translateBufferTemplate(POJO_DECODE_BODY_TEMPLATE, ms));
            }
        }
        else
        {
            // just the function prototype
            //
            buffer.append(FileHelper.translateBufferTemplate(POJO_DECODE_PROTO_TEMPLATE, ms));
        }

        // TODO: make this cleaner, but need to return the function name (not parms)
        //       so that callers know how to invoke this function
        //
        return "decode_"+ms.getVariable(POJO_CLASSNAME)+"_fromJSON";
    }

    //--------------------------------------------
    /**
     * Create the POJO create function for Header or Source
     * Returns the function name of the generated code so that
     * we can later utilize this function
     */
    //--------------------------------------------
    private String generateNativeCreate(CustomVariable var, boolean isPrototype, MacroSource ms, StringBuffer buffer)
        throws IOException
    {
        if (isPrototype == false)
        {
            // get the list of arrays & maps that this variable has and add the "element = createLinkedList" or
            // "element = createHashMap" for each
            //
            StringBuffer creators = new StringBuffer();
            Iterator<ArrayVariable> arrayVars = var.getArrayVariables(false).iterator();
            while(arrayVars.hasNext() == true)
            {
                ArrayVariable v = arrayVars.next();
                creators.append("    retVal->");
                creators.append(v.getName());
                creators.append(" = linkedListCreate();\n");
            }

            Iterator<MapVariable> mapVars = var.getMapVariables(false).iterator();
            while(mapVars.hasNext() == true)
            {
                MapVariable v = mapVars.next();
                creators.append("    retVal->");
                creators.append(v.getValuesVarName());
                creators.append(" = hashMapCreate();\n");
                creators.append("    retVal->");
                creators.append(v.getTypesVarName());
                creators.append(" = hashMapCreate();\n");
            }

            Iterator<CustomVariable> custVars = var.getCustomVariables().iterator();
            while(custVars.hasNext() == true)
            {
                CustomVariable v = custVars.next();
                creators.append("    retVal->");
                creators.append(v.getLocalName());
                creators.append(" = ");

                // find corresponding constructor from the PojoWrapper
                //
                boolean didIt = false;
                PojoWrapper pw = Processor.getPojoProcessor().getPojoForName(v.getName());
                if (pw != null)
                {
                    String constructor = pw.getContextNative().getCreateFunctionName();
                    if (constructor != null)
                    {
                        creators.append(constructor);
                        creators.append("();\n");
                        didIt = true;
                    }
                }

                if (didIt == false)
                {
                    // need to bail, cannot continue
                    //
                    throw new RuntimeException("Unable to find 'constructor' for "+v.getName());
                }
            }

            StringBuffer cloneWiring = new StringBuffer();
            if (var.isCloneable())
            {
                String clone = "clone_" + ms.getVariable(POJO_CLASSNAME);
                cloneWiring.append(clone);
            }
            else
            {
                cloneWiring.append("NULL");
            }
            ms.addMapping(POJO_C_CLONE_WIRE, cloneWiring.toString());

            // add the creators to the macro source with the matching key 'pojo_dynamic_creations'
            //
            ms.addMapping("pojo_dynamic_creations", creators.toString());

            // use the body template
            //
            buffer.append(FileHelper.translateBufferTemplate(POJO_CREATE_BODY_TEMPLATE, ms));
        }
        else
        {
            // just the function prototype
            //
            buffer.append(FileHelper.translateBufferTemplate(POJO_CREATE_PROTO_TEMPLATE, ms));
        }

        return "create_"+ms.getVariable(POJO_CLASSNAME);
    }

    //--------------------------------------------
    /**
     * Create the POJO destroy function for Header or Source
     * Returns the function name of the generated code so that
     * we can later utilize this function
     */
    //--------------------------------------------
    private String generateNativeDestroy(CustomVariable var, boolean isPrototype, MacroSource ms, StringBuffer buffer)
        throws IOException
    {
        if (isPrototype == false)
        {
            // get the list of arrays & maps that this variable has and add the "destroyLinkedList" or
            // "destroyHashMap" for each
            //
            StringBuffer destroyers = new StringBuffer();
            Iterator<ArrayVariable> arrayVars = var.getArrayVariables(false).iterator();
            while(arrayVars.hasNext() == true)
            {
                destroyers.append("    // destroy linked-list and all elements\n");
                ArrayVariable v = arrayVars.next();
                destroyers.append("    linkedListDestroy(pojo->");
                destroyers.append(v.getName());

                // if the 'elements' of this array are custom, use the destructor
                //
                BaseVariable base = v.getFirstElement();
                if (base.isCustom() == true)
               	{
                	CustomVariable cust = (CustomVariable)base;
                	String destructor = ProcessorHelper.getNativeDestructorForVarFromList(cust);
                	if (destructor != null)
                	{
                		destroyers.append(", "+destructor+");\n");
                	}
                	else
                	{
                		destroyers.append(", NULL);\n");
                	}
               	}
                else
                {
                	// not custom or event
                	//
                	destroyers.append(", NULL);\n");
                }
            }

            Iterator<MapVariable> mapVars = var.getMapVariables(false).iterator();
            while(mapVars.hasNext() == true)
            {
            	// need to support 3 different ways to destroy the map that contains
            	// the 'values':
            	//  1 - map only supports one possible type, and it's custom
            	//  2 - map only supports one possible type, and it's NOT custom
            	//  3 - map supports multiple types
            	//
                MapVariable v = mapVars.next();
                String valuesMap = v.getValuesVarName();
                ArrayList<BaseVariable> possibles = v.getPossibleTypes();
                if (possibles.size() == 1)
                {
                	// scenario #1 or #2
                	//
                	BaseVariable only = possibles.get(0);
                	if (only.isCustom() == true)
                	{
                		// use the generated 'hashMapFreeFunc' for this custom object
                		//
                		String mapFreeFunc = ProcessorHelper.getNativeDestructorForVarFromMap((CustomVariable)only);
                		destroyers.append("    // destroy "+valuesMap+" values (using hashMapFreeFunc)\n");
                		destroyers.append("    hashMapDestroy(pojo->"+valuesMap+", "+mapFreeFunc+");\n");
                	}
                	else
                	{
                		// standard NULL for the free of items within the map
                		//
                		destroyers.append("    // destroy "+valuesMap+" values (using standard free function)\n");
                		destroyers.append("    hashMapDestroy(pojo->"+valuesMap+", NULL);\n");
                	}
                }
                else
                {
                	// scenario #3
                	// need to iterate through the map, deleting each based on it's type
                	//
                	String pad = "    ";
              		destroyers.append(pad+"// destroy "+valuesMap+" values one at a time (due to supporting various data types)\n");
                	destroyers.append(pad+"icHashMapIterator *"+valuesMap+"_loop = hashMapIteratorCreate(pojo->"+valuesMap+");\n");
                	destroyers.append(pad+"while (hashMapIteratorHasNext("+valuesMap+"_loop) == true)\n");
                	destroyers.append(pad+"{\n");
                	destroyers.append(pad+"    void *mapKey;\n");
                	destroyers.append(pad+"    void *mapVal;\n");
                	destroyers.append(pad+"    uint16_t mapKeyLen = 0;\n\n");

                	destroyers.append(pad+"    // get key\n");
                	destroyers.append(pad+"    hashMapIteratorGetNext("+valuesMap+"_loop, &mapKey, &mapKeyLen, &mapVal);\n");
                	destroyers.append(pad+"    if (mapKey == NULL || mapVal == NULL)\n");
                	destroyers.append(pad+"    {\n");
                	destroyers.append(pad+"        continue;\n");
                	destroyers.append(pad+"    }\n\n");

                	destroyers.append(pad+"    // find corresponding 'type'\n");
                	destroyers.append(pad+"    char *kind = (char *)hashMapGet("+v.getTypesVarName()+", mapKey, mapKeyLen);\n");
                	destroyers.append(pad+"    if (kind == NULL)\n");
                	destroyers.append(pad+"    {\n");
                	destroyers.append(pad+"        continue;\n");
                	destroyers.append(pad+"    }\n\n");

                	// depending on the 'type', perform the delete via the iterator
                	//
                    Iterator<BaseVariable> loop = possibles.iterator();
                    boolean first = true;
                    while (loop.hasNext() == true)
                    {
                        BaseVariable inner = loop.next();
                        destroyers.append(pad);
                        if (first == true)
                        {
                            destroyers.append("if");
                            first = false;
                        }
                        else
                        {
                            destroyers.append("else if");
                        }
                        destroyers.append(" (strcmp(kind, \"");
                        destroyers.append(inner.getJavaType().toLowerCase());
                        destroyers.append("\") == 0)\n");
                        destroyers.append(pad+"{\n");
                        destroyers.append(pad+"    hashMapIteratorDeleteCurrent("+valuesMap+"_loop, ");

                        if (inner.isCustom() == true)
                        {
                    		String mapFreeFunc = ProcessorHelper.getNativeDestructorForVarFromMap((CustomVariable)inner);
                        	destroyers.append(mapFreeFunc+");\n");
                        }
                        else
                        {
                        	destroyers.append("NULL);\n");
                        }
                        destroyers.append(pad+"}\n");
                    }

                	destroyers.append(pad+"}\n");
                	destroyers.append(pad+"hashMapIteratorDestroy("+valuesMap+"_loop);\n");
               		destroyers.append(pad+"hashMapDestroy(pojo->"+valuesMap+", NULL);\n");
                }

                // now free the 'types' hash
                //
                destroyers.append("    // destroy "+v.getName()+" 'types' map (key/val should be string/string)\n");
                destroyers.append("    hashMapDestroy(pojo->");
                destroyers.append(v.getTypesVarName());
                destroyers.append(", NULL);\n");
            }

            Iterator<StringVariable> stringVars = var.getStringVariables().iterator();
            while (stringVars.hasNext() == true)
            {
                StringVariable str = stringVars.next();
                destroyers.append("    if (pojo->"+str.getName()+" != NULL)\n");
                destroyers.append("    {\n");
                destroyers.append("        free(pojo->"+str.getName()+");\n");
                destroyers.append("    }\n");
            }

            Iterator<JsonVariable> jsonVars = var.getJsonVariables().iterator();
            while (jsonVars.hasNext() == true)
            {
                JsonVariable str = jsonVars.next();
                destroyers.append("    if (pojo->"+str.getName()+" != NULL)\n");
                destroyers.append("    {\n");
                destroyers.append("        cJSON_Delete(pojo->"+str.getName()+");\n");
                destroyers.append("    }\n");
            }

            Iterator<CustomVariable> custVars = var.getCustomVariables().iterator();
            while (custVars.hasNext() == true)
            {
                CustomVariable cust = custVars.next();
                destroyers.append("    if (pojo->"+cust.getLocalName()+" != NULL)\n");
                destroyers.append("    {\n");

                String destructor = ProcessorHelper.getNativeDestructorForVar(cust);
                if (destructor != null)
                {
                    destroyers.append("        "+destructor+"(pojo->"+cust.getLocalName()+");\n");
                }
                else
                {
                    // just free it
                    //
                    destroyers.append("        free(pojo->"+cust.getLocalName()+");\n");
                }
                destroyers.append("    }\n");
            }

            // add the destroyers to the macro source with the matching key 'pojo_dynamic_creations'
            //
            ms.addMapping("pojo_dynamic_destroys", destroyers.toString());

            // use the body template
            //
            buffer.append(FileHelper.translateBufferTemplate(POJO_DESTROY_BODY_TEMPLATE, ms));
        }
        else
        {
            // just the function prototype
            //
            buffer.append(FileHelper.translateBufferTemplate(POJO_DESTROY_PROTO_TEMPLATE, ms));
        }

        return "destroy_"+ms.getVariable(POJO_CLASSNAME);
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private String generateNativeListDestroy(CustomVariable var, boolean isPrototype, MacroSource ms, StringBuffer buffer)
    	throws IOException
    {
    	if (isPrototype == false)
    	{
    		// use the body template
    		//
    		buffer.append(FileHelper.translateBufferTemplate(POJO_DESTROY_FROM_LIST_BODY_TEMPLATE, ms));
    	}
    	else
    	{
    		// just the function prototype
    		//
    		buffer.append(FileHelper.translateBufferTemplate(POJO_DESTROY_FROM_LIST_PROTO_TEMPLATE, ms));
    	}

    	return "destroy_"+ms.getVariable(POJO_CLASSNAME)+"_fromList";
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private String generateNativeMapDestroy(CustomVariable var, boolean isPrototype, MacroSource ms, StringBuffer buffer)
    	throws IOException
    {
    	if (isPrototype == false)
    	{
    		// use the body template
    		//
    		buffer.append(FileHelper.translateBufferTemplate(POJO_DESTROY_FROM_MAP_BODY_TEMPLATE, ms));
    	}
    	else
    	{
    		// just the function prototype
    		//
    		buffer.append(FileHelper.translateBufferTemplate(POJO_DESTROY_FROM_MAP_PROTO_TEMPLATE, ms));
    	}

    	return "destroy_"+ms.getVariable(POJO_CLASSNAME)+"_fromMap";
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void generateNativeGetters(CustomVariable var, boolean isPrototype, MacroSource ms, StringBuffer retVal, ContextNative ctx)
    {
        // TODO: arrays?

        // now the same for any map variables
        //
        Iterator<MapVariable> mapVars = var.getMapVariables(false).iterator();
        while(mapVars.hasNext() == true)
        {
            MapVariable v = mapVars.next();

            // make similar to Java, in that we create a 'set' for each possible
            // data type so simplify adding into the hash (as well as allow for
            // decoding)
            //
            StringBuffer set = v.getNativeGetter(var, ctx, isPrototype);
            if (set.length() > 0)
            {
                retVal.append(set);
            }
        }
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void generateNativeSetters(CustomVariable var, boolean isPrototype, MacroSource ms, StringBuffer retVal, ContextNative ctx)
        throws IOException
    {

        // copy the macro source so we can fill in additional values without messing up the original
        //
        MacroSource macro = new MacroSource();
        macro.addMapping(ms);

        // get the list of arrays that this variable has and create a 'set' function for each
        //
        Iterator<ArrayVariable> arrayVars = var.getArrayVariables(false).iterator();
        while(arrayVars.hasNext() == true)
        {
            ArrayVariable v = arrayVars.next();
            BaseVariable first = v.getFirstElement();
            String elementType = first.getCtype();

            // populate our macro
            //
            macro.addMapping("list_type", elementType);
            macro.addMapping("list_first", first.getName());
            macro.addMapping("list_var", v.getName());

            if (isPrototype == true)
            {
                if (first.isString() == true)
                {
                    retVal.append(FileHelper.translateBufferTemplate(POJO_SET_STR_LIST_PROTO_TEMPLATE, macro));
                }
                else if (first.isPtr() == true || first.isDate() == true)
                {
                    retVal.append(FileHelper.translateBufferTemplate(POJO_SET_LIST_PROTO_TEMPLATE, macro));
                }
                else
                {
                    retVal.append(FileHelper.translateBufferTemplate(POJO_SET_LIST_NONPTR_PROTO_TEMPLATE, macro));
                }
            }
            else
            {
                if (first.isString() == true)
                {
                    retVal.append(FileHelper.translateBufferTemplate(POJO_SET_STR_LIST_BODY_TEMPLATE, macro));
                }
                else if (first.isPtr() == true || first.isDate() == true)
                {
                    retVal.append(FileHelper.translateBufferTemplate(POJO_SET_LIST_BODY_TEMPLATE, macro));
                }
                else
                {
                    retVal.append(FileHelper.translateBufferTemplate(POJO_SET_LIST_NONPTR_BODY_TEMPLATE, macro));
                }
            }
            ctx.addSetFunction(v.getName(), "put_"+first.getName()+"_in_"+var.getCtype()+"_"+v.getName());
        }

        // now the same for any map variables
        //
        Iterator<MapVariable> mapVars = var.getMapVariables(false).iterator();
        while(mapVars.hasNext() == true)
        {
            MapVariable v = mapVars.next();

            // make similar to Java, in that we create a 'set' for each possible
            // data type so simplify adding into the hash (as well as allow for
            // decoding)
            //
            StringBuffer set = v.getNativeSetter(var, ctx, isPrototype);
            if (set.length() > 0)
            {
                retVal.append(set);
            }
        }
    }
}
