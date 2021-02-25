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

import com.icontrol.generate.service.TemplateMappings;
import com.icontrol.generate.service.variable.ArrayVariable;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.EnumVariable;
import com.icontrol.generate.service.variable.EventVariable;

//-----------------------------------------------------------------------------
/**
 * Created by jelderton on 4/17/15.
 */
//-----------------------------------------------------------------------------
public class EventWrapper extends Wrapper
    implements TemplateMappings
{
    private EventVariable   variable;
    private List<String>    eventCodes;
    private List<Wrapper>   dependencies;

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    EventWrapper(EventVariable var)
    {
        // save our variable and prime a language mask
        //
        super();
        variable = var;
        dependencies = new ArrayList<Wrapper>();

        // add section to our mask
        //
        sectionMask.addEventSection();
    }

    //--------------------------------------------
    /**
     * @see Wrapper#getVariable()
     */
    //--------------------------------------------
    public BaseVariable getVariable()
    {
        return variable;
    }

    //--------------------------------------------
    /**
     * Return the Wrappers this Event is dependent on
     */
    //--------------------------------------------
    public Iterator<Wrapper> getDependencies()
    {
        return dependencies.iterator();
    }

    //--------------------------------------------
    /**
     * Return a list of the event codes (name) this
     * event is broadcasted for
     */
    //--------------------------------------------
    public List<String> getEventCodes()
    {
        return eventCodes;
    }

    void setEventCodes(List<String> codes)
    {
        eventCodes = codes;
    }

    //--------------------------------------------
    /**
     * Return a the event codes as a comma delimited string
     */
    //--------------------------------------------
    public String getEventCodesString()
    {
        StringBuffer buff = new StringBuffer();
        Iterator<String> loop = eventCodes.iterator();
        while (loop.hasNext() == true)
        {
            buff.append(loop.next());
            if (loop.hasNext() == true)
            {
                buff.append(',');
            }
        }
        return buff.toString();
    }

    //--------------------------------------------
    /**
     * Called by EventProcessor
     */
    //--------------------------------------------
    void buildDependencies()
    {
        // get processor helpers
        //
        PojoProcessor pojoProcessor = Processor.getPojoProcessor();
        EnumProcessor enumProcessor = Processor.getEnumProcessor();
        EventProcessor eventProcessor = Processor.getEventProcessor();

        // look through our custom variable for any
        // dependencies (enum, other customs, etc)
        //
        Iterator<BaseVariable> loop = variable.getVariables().iterator();
        while (loop.hasNext() == true)
        {
            BaseVariable var = loop.next();

            // make sure this variable knows it is referred to
            // by something in the POJO section
            //
            if (var.isEnum() == true)
            {
                EnumVariable enumVar = (EnumVariable)var;
                EnumDefWrapper wrapper = enumProcessor.getEnumForDefinitionName(enumVar.getName());
                if (wrapper != null)
                {
                    addDependency(wrapper);
                }
            }
            else if (var.isEvent() == true)
            {
                EventWrapper wrapper = eventProcessor.getEventForName(var.getName());
                if (wrapper != null)
                {
                    addDependency(wrapper);
                }
            }
            else if (var.isCustom() == true)
            {
                PojoWrapper wrapper = pojoProcessor.getPojoForName(var.getName());
                if (wrapper != null)
                {
                    addDependency(wrapper);
                }
            }
            else if (var.isArray() == true)
            {
                ArrayVariable arrayVar = (ArrayVariable)var;
                BaseVariable element = arrayVar.getFirstElement();
                if (element.isEnum() == true)
                {
                    // have an array of enumerations
                    EnumVariable enumElement = (EnumVariable)element;
                    EnumDefWrapper wrapper = enumProcessor.getEnumForDefinitionName(enumElement.getName());
                    if (wrapper != null)
                    {
                        addDependency(wrapper);
                    }
                }
                else if (element.isCustom() == true)
                {
                    // have an array of custom objects
                    PojoWrapper wrapper = pojoProcessor.getPojoForName(element.getName());
                    if (wrapper != null)
                    {
                        addDependency(wrapper);
                    }
                }
            }
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
        wrapper.getSectionMask().addEventSection();

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


