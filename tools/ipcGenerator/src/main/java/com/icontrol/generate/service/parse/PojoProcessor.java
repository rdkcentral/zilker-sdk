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
import java.util.Map;
import java.util.LinkedHashMap;
import java.util.Iterator;
import java.util.List;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.io.LanguageMask;
import com.icontrol.generate.service.variable.ArrayVariable;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.generate.service.variable.MapVariable;
import com.icontrol.xmlns.service.ICCustomObject;
import com.icontrol.xmlns.service.ICEvent;
import com.icontrol.xmlns.service.ICEventList;
import com.icontrol.xmlns.service.ICIpc;
import com.icontrol.xmlns.service.ICMessage;
import com.icontrol.xmlns.service.ICMessageParm;
import com.icontrol.xmlns.service.ICScope;
import com.icontrol.xmlns.service.ICService;

//-----------------------------------------------------------------------------
/**
 * ProcessorHelper instance to create PojoWrapper objects
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class PojoProcessor extends ProcessorHelper
{
    private Map<String,PojoWrapper> pojoLookup;

    //--------------------------------------------
    /**
     * Called by Processor during initialization
     */
    //--------------------------------------------
    static PojoProcessor buildModel(ICService top)
    {
        PojoProcessor retVal = new PojoProcessor();

        // loop through the POJO (custom objects) definitions that are listed
        // in the XML document.  Each should already be parsed into a CustomVariable
        // by the Processor.
        //
        List<ArrayVariable> arrayList = new ArrayList<ArrayVariable>();
        List<MapVariable> mapList = new ArrayList<MapVariable>();
        List<ICCustomObject> pojoList = top.getPojoList();
        for (int i = 0; i < pojoList.size(); i++)
        {
            // find the CustomVariable representation
            //
            ICCustomObject bean = pojoList.get(i);
            String varName = bean.getVarName();
            CustomVariable var = Processor.getCustomForName(varName);
            if (var != null)
            {
                // create the PojoWrapper
                //
                retVal.pojoLookup.put(varName, new PojoWrapper(var));
                
                // if this has any ArrayVariables, add each to our 'arrayList'
                //
                if (var.doesCustomHaveArray(true) == true)
                {
                	ArrayList<ArrayVariable> tmp = var.getArrayVariables(true);
                	arrayList.addAll(tmp);
                }
                
                // if this has any MapVariables, add each to our 'arrayList'
                //
                if (var.doesCustomHaveMap(false) == true)
                {
                	ArrayList<MapVariable> tmp = var.getMapVariables(false);
                	mapList.addAll(tmp);
                }
            }
        }
        
        // now that all pojo's have been allocated into Wrapper objects,
        // look through all of the List and Map definitions so we can tag
        // the POJOs that will be included in a List or Map.  mainly needed
        // when generating C code to ensure we create the appropriate 
        // 'destroy' functions
        //
        Iterator<ArrayVariable> arrayLoop = arrayList.iterator();
        while (arrayLoop.hasNext() == true)
        {
        	ArrayVariable next = arrayLoop.next();
        	BaseVariable base = next.getFirstElement();
        	if (base.isCustom() == true)
        	{
        		// have an array with CustomObj as the element type.
        		// find the pojo wrapper for this CustomObj, and set
        		// the 'inList' flag to true
        		//
        		PojoWrapper w = retVal.getPojoForName(base.getName());
        		if (w != null)
        		{
        			w.setInList(true);
        		}
        	}
        }

        Iterator<MapVariable> mapLoop = mapList.iterator();
        while (mapLoop.hasNext() == true)
        {
        	// ask the map for all of the possible data types it can hold
        	//
        	MapVariable next = mapLoop.next();
        	ArrayList<BaseVariable> possibles = next.getPossibleTypes();
        	Iterator<BaseVariable> innerLoop = possibles.iterator();
        	while (innerLoop.hasNext() == true)
        	{
        		// see if this possible map type is custom
        		//
        		BaseVariable innerBase = innerLoop.next();
        		if (innerBase.isCustom() == true)
        		{
        			// have a map, where one of the potential types is a CustomObj.
        			// find the pojo wrapper to set the 'inMap' flag to true
        			//
        			PojoWrapper w = retVal.getPojoForName(innerBase.getName());
        			if (w != null)
        			{
        				w.setInMap(true);
        			}
        		}
        	}
        }

        return retVal;
    }


    //--------------------------------------------
    /**
     * private constructor
     */
    //--------------------------------------------
    private PojoProcessor()
    {
        pojoLookup = new LinkedHashMap<String,PojoWrapper>();
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
        // look at each Event and IPC Message, looking for references
        // to any POJO created during 'preprocess'.  This way we can
        // set the appropriate language flags.
        //
        if (top.isSetEventList() == true)
        {
            // get the list of events from the top bean
            //
            ICEventList eventListBean = top.getEventList();
            ICScope.Enum scope = eventListBean.getGenerate();

            // loop through each Event definition
            //
            List<ICEvent> array = eventListBean.getEventList();
            for (int i = 0, count = array.size() ; i < count ; i++)
            {
                // get the variable name of the Event bean
                //
                ICEvent eventBean = array.get(i);
                CustomVariable impl = Processor.getCustomForName(eventBean.getVarName());
                if (impl != null)
                {
                    // set language for this PojoWrapper
                    //
                    analyzeCustomVar(impl, scope);
                }
            }
        }

        if (top.isSetIpc() == true)
        {
            ICIpc ipcListBean = top.getIpc();
            List<ICMessage> array = ipcListBean.getMessageList();
            for (int i = 0, count = array.size() ; i < count ; i++)
            {
                // examine the IPC message definition
                //
                ICMessage msgBean = array.get(i);
                ICScope.Enum scope = msgBean.getGenerate();

                // look for POJOs used as input/output parameters
                //
                analyzeMessageParm(msgBean.getInput(), scope);
                analyzeMessageParm(msgBean.getOutput(), scope);
            }
        }

        // have each PojoWrapper determine their dependencies
        // and inform those dependencies they are referenced
        // by the POJO section
        //
        Iterator<String> loop = pojoLookup.keySet().iterator();
        while (loop.hasNext() == true)
        {
            String key = loop.next();
            PojoWrapper pojo = pojoLookup.get(key);
            if (pojo != null)
            {
                pojo.buildDependencies();
            }
        }

        // NOTE: anything that depends on this POJO (Event, IPC message parm) will
        // set our language mask during their 'analyze'.  We do NOT set languages
        // on Enum,Custom within these POJOs since generation just implicitly creates them
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
        Iterator<String> loop = pojoLookup.keySet().iterator();
        while (loop.hasNext() == true)
        {
            String key = loop.next();
            PojoWrapper pojo = pojoLookup.get(key);
            if (pojo != null)
            {
                LanguageMask lang = pojo.getLanguageMask();
                if (lang.hasNative() == false && lang.hasJava() == false)
                {
                    System.out.println("WARNING: pojo '"+pojo.getVarName()+"' is not used by Events or IPC and will therefore not get generated!");
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
        Iterator<String> loop = pojoLookup.keySet().iterator();
        while (loop.hasNext() == true)
        {
            // fugly, but does the job since we aren't running
            // in a multi-threaded mode and run the risk of NULL
            //
            tmp.add(pojoLookup.get(loop.next()));
        }

        return tmp.iterator();
    }

    //--------------------------------------------
    /**
     * Returns the PojoWrapper for this variable name
     */
    //--------------------------------------------
    public PojoWrapper getPojoForName(String varName)
    {
        return pojoLookup.get(varName);
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void analyzeCustomVar(CustomVariable var, ICScope.Enum scope)
    {
        // first see if this is a direct correlation to a PojoWrapper
        //
        PojoWrapper pojo = getPojoForName(var.getName());
        if (pojo != null)
        {
            // set the language
            //
            if (scope == ICScope.C || scope == ICScope.BOTH)
            {
                pojo.getLanguageMask().addNative();
            }
            if (scope == ICScope.JAVA || scope == ICScope.BOTH)
            {
                pojo.getLanguageMask().addJava();
            }
        }

        // loop through the children of this variable to see if any
        // are PojoWrappers as well.
        //
        Iterator<BaseVariable> loop = var.getVariables().iterator();
        while (loop.hasNext() == true)
        {
            BaseVariable child = loop.next();
            if (child.isCustom() == true)
            {
                // recurse into this CustomVar in case it has additional POJO references
                //
                analyzeCustomVar((CustomVariable) child, scope);
            }
            else if (child.isArray() == true)
            {
                // look at the element type of the array
                //
                ArrayVariable arrayVar = (ArrayVariable)child;
                BaseVariable element = arrayVar.getFirstElement();
                if (element.isCustom() == true)
                {
                    analyzeCustomVar((CustomVariable) element, scope);
                }
            }
            else if (child.isMap() == true)
            {
                // get possible sub-types this refers to
                //
                MapVariable mapVar = (MapVariable)child;
                Iterator<BaseVariable> possList = mapVar.getPossibleTypes().iterator();
                while (possList.hasNext() == true)
                {
                    BaseVariable element = possList.next();
                    if (element.isCustom() == true)
                    {
                        analyzeCustomVar((CustomVariable) element, scope);
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
    private void analyzeMessageParm(ICMessageParm parm, ICScope.Enum scope)
    {
        if (parm != null && parm.isSetCustomRef() == true)
        {
            // get the name of the CustomVariable used as a message parameter
            //
            String varName = parm.getCustomRef();
            CustomVariable impl = Processor.getCustomForName(varName);
            if (impl != null)
            {
                // set language for this PojoWrapper
                //
                analyzeCustomVar(impl, scope);
            }
        }
    }
}
