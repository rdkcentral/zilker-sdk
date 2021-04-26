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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Map;
import java.util.LinkedHashMap;
import java.util.Iterator;
import java.util.List;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.io.LanguageMask;
import com.icontrol.generate.service.variable.EnumDefinition;
import com.icontrol.generate.service.variable.EnumVariable;
import com.icontrol.xmlns.service.ICEnumDefinition;
import com.icontrol.xmlns.service.ICService;

//-----------------------------------------------------------------------------
/**
 * ProcessorHelper instance to create EnumWrapper objects
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class EnumProcessor extends ProcessorHelper
{
    private Map<String,EnumDefWrapper> enumLookup;

    //--------------------------------------------
    /**
     * Called by Processor during initialization
     */
    //--------------------------------------------
    static EnumProcessor buildModel(ICService top)
    {
        EnumProcessor retVal = new EnumProcessor();

        // make EnumWrapper for each enumeration defined in the XML doc
        //
        List<ICEnumDefinition> enumList = top.getTypedefList();
        for (int i = 0 ; i < enumList.size() ; i++)
        {
            // add the variable to our set so we can generate it first
            //
            String name = enumList.get(i).getEnumTypeName();
            EnumDefinition var = Processor.getEnumDefinitionForName(name);
            if (var != null)
            {
                retVal.enumLookup.put(name, new EnumDefWrapper(var));
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * private constructor
     */
    //--------------------------------------------
    private EnumProcessor()
    {
        enumLookup = new LinkedHashMap<String,EnumDefWrapper>();
    }

    //--------------------------------------------
    /**
     * Called by Processor during initialization
     * @see ProcessorHelper#analyze(ICService, Context)
     */
    //--------------------------------------------
    @Override
    public void analyze(ICService top, Context ctx)
    {
        // nothing to analyze really since POJO, Message,
        // and Event analyze set the scope and section
        // of the Enums for us
        //
    }

    //--------------------------------------------
    /**
     * Called by Processor during initialization
     */
    //--------------------------------------------
    public void analyzeAgain(Context ctx)
    {
        // examine each PojoWrapper and print to the screen any
        // that were not referenced by Event or IPC.  This way 
        // we can cut out cruft and not generate code that never
        // gets used.
        //
        Iterator<String> loop = enumLookup.keySet().iterator();
        while (loop.hasNext() == true)
        {
            String key = loop.next();
            Wrapper wrap = enumLookup.get(key);
            if (wrap != null)
            {
                LanguageMask lang = wrap.getLanguageMask();
                if (lang.hasNative() == false && lang.hasJava() == false)
                {
                    EnumVariable var = (EnumVariable)wrap.getVariable();
                    System.out.println("WARNING: enum '"+var.getVarName()+"' is not used by Events or IPC and will therefore not get generated!");
                }
            }
        } 
    }

    //--------------------------------------------
    /**
     * Called by Processor during code generation
     * @see ProcessorHelper#getWrappers()
     */
    //--------------------------------------------
    @Override
    public Iterator<Wrapper> getWrappers()
    {
        // place each into a List for the Iterator
        //
        ArrayList<Wrapper> tmp = new ArrayList<Wrapper>();
        Iterator<String> loop = enumLookup.keySet().iterator();
        while (loop.hasNext() == true)
        {
            // fugly, but does the job since we aren't running
            // in a multi-threaded mode and run the risk of NULL
            //
            tmp.add(enumLookup.get(loop.next()));
        }

        return tmp.iterator();
    }

    //--------------------------------------------
    /**
     * Return the EnumDefWrapper with this enum definition name
     * (not to be confused with varName)
     */
    //--------------------------------------------
    public EnumDefWrapper getEnumForDefinitionName(String defName)
    {
        return enumLookup.get(defName);
    }

}
