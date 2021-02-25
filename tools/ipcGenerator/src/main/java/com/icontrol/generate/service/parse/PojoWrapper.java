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
import java.util.Iterator;
import java.util.List;

import com.icontrol.generate.service.variable.ArrayVariable;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.generate.service.variable.EnumVariable;

//-----------------------------------------------------------------------------
/**
 * Wrapper implementation that represent the POJO variables
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class PojoWrapper extends Wrapper
{
    private CustomVariable  variable;
    private List<Wrapper>   dependencies;

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    PojoWrapper(CustomVariable var)
    {
        // save our variable and prime a language mask
        //
        super();
        variable = var;
        dependencies = new ArrayList<Wrapper>();

        // add POJO to our section
        //
        sectionMask.addPojoSection();
    }

    //--------------------------------------------
    /**
     * Return the varName of his POJO (same as the
     * CustomVariable.varName this POJO represents)
     */
    //--------------------------------------------
    public String getVarName()
    {
        return variable.getName();
    }

    //--------------------------------------------
    /**
     * Return the CustomVariable this POJO represents
     * @see Wrapper#getVariable()
     */
    //--------------------------------------------
    public BaseVariable getVariable()
    {
        return variable;
    }

    //--------------------------------------------
    /**
     * Return the Wrappers this POJO is dependent on
     */
    //--------------------------------------------
    public Iterator<Wrapper> getDependencies()
    {
        return dependencies.iterator();
    }

	//--------------------------------------------
    /**
     * Called by PojoProcessor
     */
    //--------------------------------------------
    void buildDependencies()
    {
        // look through our custom variable for any
        // dependencies (enum, other customs, etc)
        //
        Iterator<BaseVariable> loop = variable.getVariables().iterator();
        while (loop.hasNext() == true)
        {
            BaseVariable var = loop.next();
            checkDependency(var);
        }
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void checkDependency(BaseVariable var)
    {
        // make sure this variable knows it is referred to
        // by something in the POJO section
        //
        if (var.isEnum() == true)
        {
            // look for corresponding EnumDefWrapper
            //
            EnumProcessor enumProcessor = Processor.getEnumProcessor();
            EnumVariable enumVar = (EnumVariable)var;
            EnumDefWrapper wrapper = enumProcessor.getEnumForDefinitionName(enumVar.getName());
            if (wrapper != null)
            {
                addDependency(wrapper);
            }
        }
        else if (var.isEvent() == true)
        {
            // look for corresponding EventWrapper
            //
            EventProcessor eventProcessor = Processor.getEventProcessor();
            EventWrapper wrapper = eventProcessor.getEventForName(var.getName());
            if (wrapper != null)
            {
                addDependency(wrapper);
            }
        }
        else if (var.isCustom() == true)
        {
            // look for corresponding PojoWrapper
            //
            PojoProcessor pojoProcessor = Processor.getPojoProcessor();
            PojoWrapper wrapper = pojoProcessor.getPojoForName(var.getName());
            if (wrapper != null)
            {
                addDependency(wrapper);

                // look at subfields
                //
                CustomVariable custVar = (CustomVariable)var;
                Iterator<BaseVariable> subFields = custVar.getVariables().iterator();
                while (subFields.hasNext() == true)
                {
                    BaseVariable sub = subFields.next();
//System.out.println("checking sub field "+sub.getName());
                    checkDependency(sub);
                }
            }
        }
        else if (var.isArray() == true)
        {
//            addDependency(wrapper);

            // extract the array element type, then recurse
            //
            ArrayVariable arrayVar = (ArrayVariable)var;
            BaseVariable element = arrayVar.getFirstElement();
//System.out.println("checking array field "+element.getName());
            checkDependency(element);
        }
        else if (var.isMap() == true)
        {
            // TODO: map
        }
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void addDependency(Wrapper wrapper)
    {
        // add to dependencies & the POJO section
        //
        dependencies.add(wrapper);
        wrapper.getSectionMask().addPojoSection();
//System.out.println(getVarName()+" depends on "+wrapper.toString());

        // since we depend on this wrapper, ensure it has the
        // same language settings we do
        //
        if (getLanguageMask().hasJava() == true)
        {
            wrapper.getLanguageMask().addJava();
        }
        if (getLanguageMask().hasNative() == true)
        {
            wrapper.getLanguageMask().addNative();
        }
    }

}
