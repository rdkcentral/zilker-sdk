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
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.EnumVariable;
import com.icontrol.generate.service.variable.EventVariable;
import com.icontrol.xmlns.service.ICEvent;
import com.icontrol.xmlns.service.ICEventCode;
import com.icontrol.xmlns.service.ICEventList;
import com.icontrol.xmlns.service.ICScope;
import com.icontrol.xmlns.service.ICService;

//-----------------------------------------------------------------------------
/**
 * ProcessorHelper instance to create EventWrapper objects
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class EventProcessor extends ProcessorHelper
{
    private Map<String,EventWrapper>     eventLookup;
    private List<CodeDefinition>         defineCodes;
    private List<CodeDefinition>         eventCodes;

    //--------------------------------------------
    /**
     * Called by Processor during initialization
     */
    //--------------------------------------------
    static EventProcessor buildModel(ICService top)
    {
        EventProcessor retVal = new EventProcessor();

        // check if there's anything to do
        //
        if (top.isSetEventList() == false)
        {
            return retVal;
        }

        // get the event list bean and start out by
        // adjusting the event codes since some have
        // magical values (-1, -99)
        //
        ICEventList eventListBean = top.getEventList();
        retVal.adjustEventCodes(eventListBean);

        // get the language scope for events
        //
        ICScope.Enum scope = eventListBean.getGenerate();

        // loop through the Event XML definitions and create
        // an EventWrapper for each.
        //
        Iterator<ICEvent> eventist = eventListBean.getEventList().iterator();
        while(eventist.hasNext() == true)
        {
            ICEvent bean = eventist.next();
            String varName = bean.getVarName();
            EventVariable var = (EventVariable)Processor.getCustomForName(varName);
            if (var != null)
            {
                // create the EventWrapper
                //
                EventWrapper wrapper = new EventWrapper(var);
                retVal.eventLookup.put(varName, wrapper);

                // apply language scope
                //
                if (scope == ICScope.C || scope == ICScope.BOTH)
                {
                    wrapper.getLanguageMask().addNative();
                }
                if (scope == ICScope.JAVA || scope == ICScope.BOTH)
                {
                    wrapper.getLanguageMask().addJava();
                }

                // get the event codes for this Event
                //
                wrapper.setEventCodes(bean.getEventCodeList());
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * private constructor
     */
    //--------------------------------------------
    private EventProcessor()
    {
        eventLookup = new LinkedHashMap<String,EventWrapper>();
        defineCodes = new ArrayList<CodeDefinition>();
        eventCodes = new ArrayList<CodeDefinition>();
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
        // loop through defined events to see which POJO/Enum items are used by Events
        //
        Iterator<Wrapper> loop = getWrappers();
        while (loop.hasNext() == true)
        {
            // get the POJO (regular or event) and Enum that this
            // particular message utilizes for input or output
            //
            EventWrapper wrapper = (EventWrapper)loop.next();
            EventVariable var = (EventVariable)wrapper.getVariable();
            ArrayList<BaseVariable> items = var.getVariables();
            for (int i = 0, count = items.size() ; i < count ; i++)
            {
                BaseVariable sub = items.get(i);
                analyzeVariable(sub);
            }

            // have the wrapper define it's depenencies
            //'
            wrapper.buildDependencies();
        }
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void analyzeVariable(BaseVariable var)
    {
        if (var == null)
        {
            return;
        }

        if (var.isEvent() == true)
        {
            // find the EventWrapper
            //
            EventWrapper event = Processor.getEventProcessor().getEventForName(var.getName());
            if (event != null)
            {
                event.getSectionMask().addEventSection();
            }

        }
        else if (var.isCustom() == true)
        {
            // find the PojomWrapper
            //
            PojoWrapper pojo = Processor.getPojoProcessor().getPojoForName(var.getName());
            if (pojo != null)
            {
                pojo.getSectionMask().addEventSection();
            }
        }
        else if (var.isEnum() == true)
        {
            EnumVariable enumVar = (EnumVariable)var;
            EnumDefWrapper enumWrapper = Processor.getEnumProcessor().getEnumForDefinitionName(enumVar.getName());
            if (enumWrapper != null)
            {
                enumWrapper.getSectionMask().addEventSection();
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
        Iterator<String> loop = eventLookup.keySet().iterator();
        while (loop.hasNext() == true)
        {
            // fugly, but does the job since we aren't running
            // in a multi-threaded mode and run the risk of NULL
            //
            tmp.add(eventLookup.get(loop.next()));
        }

        return tmp.iterator();
    }

    //--------------------------------------------
    /**
     * Return the EventWrapper with this variable name
     */
    //--------------------------------------------
    public EventWrapper getEventForName(String varName)
    {
        return eventLookup.get(varName);
    }

    //--------------------------------------------
    /**
     * Return a List of the "defines" listed in the XML
     */
    //--------------------------------------------
    public List<CodeDefinition> getDefineCodes()
    {
        return defineCodes;
    }

    //--------------------------------------------
    /**
     * Return a List of the "event codes" listed in the XML
     */
    //--------------------------------------------
    public List<CodeDefinition> getEventCodes()
    {
        return eventCodes;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void adjustEventCodes(ICEventList bean)
    {
        // iterate through the set of event codes and deal with
        // any that are defined with a -1 value, meaning it should
        // take on the previous value + 1
        //
        int lastCode = -1;
        Iterator<ICEventCode> list = bean.getCodeList().iterator();
        while(list.hasNext() == true)
        {
            // see if we need to assign a number to this
            //
            ICEventCode curr = list.next();
            int val = curr.getValue();
            if (val == -1)
            {
                // take last one and add 1 to it
                //
                val = lastCode + 1;
                curr.setValue(val);
            }
            else if (val == -99)
            {
                // take on the value of the previous one
                //
                val = lastCode;
                curr.setValue(val);
            }
            lastCode = val;
        }

        // now that all values are adjusted, create CodeDefinition containers
        // for the defines and events
        //
        list = bean.getDefineList().iterator();
        while(list.hasNext() == true)
        {
            // see if we need to assign a number to this
            //
            ICEventCode curr = list.next();
            defineCodes.add(new CodeDefinition(curr.getName(), curr.getValue(), curr.getDescription()));
        }
        list = bean.getCodeList().iterator();
        while(list.hasNext() == true)
        {
            // see if we need to assign a number to this
            //
            ICEventCode curr = list.next();
            eventCodes.add(new CodeDefinition(curr.getName(), curr.getValue(), curr.getDescription()));
        }
    }

    public static class CodeDefinition
    {
        public String   code;
        public int      value;
        public String   comment;

        private CodeDefinition(String name, int val, String desc)
        {
            code = name;
            value = val;
            comment = desc;
        }
    }
}
