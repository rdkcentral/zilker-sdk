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

package com.icontrol.generate.service.context;

//-----------
//imports
//-----------

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;

import com.icontrol.util.StringUtils;

//-----------------------------------------------------------------------------
// Class definition:    Tracker
/**
 *  Keep a set of Generated objects to allow us to track and refer to 
 *  Objects and Code that has been generated.  
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class Tracker
{
    private ArrayList<Generated> set;
    private ArrayList<String>    enumTypes;
    private HashSet<String>      eventReferencedVariables;
    private HashMap<String,TrackedPojo> pojosGenerated;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public Tracker()
    {
        set = new ArrayList<Generated>();
        enumTypes = new ArrayList<String>();
        eventReferencedVariables = new HashSet<String>();
        pojosGenerated = new HashMap<String,TrackedPojo>();
    }

    //--------------------------------------------
    /**
     * Adds a new Generated object to the set
     */
    //--------------------------------------------
    public void addGeneratedObject(Generated obj)
    {
        set.add(obj);
    }

    //--------------------------------------------
    /**
     * Returns the Generated object in this set that
     * matches 'className'
     */
    //--------------------------------------------
    public Generated getGeneratedObjectForClassName(String className)
    {
        for (int i = 0, count = set.size() ; i < count ; i++)
        {
            Generated next = set.get(i);
            if (StringUtils.compare(className, next.getClassName()) == true)
            {
                return next;
            }
        }
        return null;
    }
    
    //--------------------------------------------
    /**
     * Returns the Generated object in this set that
     * matches 'varName'
     */
    //--------------------------------------------
    public Generated getGeneratedObjectForVarName(String varName)
    {
        for (int i = 0, count = set.size() ; i < count ; i++)
        {
            Generated next = set.get(i);
            if (StringUtils.compare(varName, next.getVarName()) == true)
            {
                return next;
            }
        }
        return null;
    }
    
    //--------------------------------------------
    /**
     * Add a generated enum type to the list of tracked objects
     */
    //--------------------------------------------
    public void addGeneratedEnumType(String enumTypeName)
    {
        enumTypes.add(enumTypeName.toLowerCase());
    }
    
    //--------------------------------------------
    /**
     * Return true if this enum type has been generated
     */
    //--------------------------------------------
    public boolean hasGeneratedEnumType(String enumTypeName)
    {
        return enumTypes.contains(enumTypeName.toLowerCase());
    }
    
    //--------------------------------------------
    /**
     * Add a variable as "referenced by events"
     */
    //--------------------------------------------
    public void addEventReferenceVariable(String varName)
    {
        eventReferencedVariables.add(varName);
    }
    
    //--------------------------------------------
    /**
     * Return true if this variable name is "referenced by events"
     */
    //--------------------------------------------
    public boolean isVariableReferencedByEvents(String varName)
    {
        return eventReferencedVariables.contains(varName);
    }

    //--------------------------------------------
    /**
     * return true if a POJO with this name was generated
     */
    //--------------------------------------------
    public boolean hasGeneratedPojo(String pojoVarName)
    {
        return pojosGenerated.containsKey(pojoVarName);
    }

    //--------------------------------------------
    /**
     * adds a POJO header-file to the tracker
     */
    //--------------------------------------------
    public void addPojoHeaderFile(String pojoVarName, String filename)
    {
        TrackedPojo stub = findOrCreate(pojoVarName);
        stub.headerFilename = filename;
    }

    //--------------------------------------------
    /**
     * If generated, returns the POJO header-file
     * for the particular variable
     */
    //--------------------------------------------
    public String getPojoHeaderFile(String pojoVarName)
    {
        TrackedPojo stub = pojosGenerated.get(pojoVarName);
        if (stub != null)
        {
            return stub.headerFilename;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * adds a POJO function to the tracker
     */
    //--------------------------------------------
    public void addPojoEncodeFunction(String pojoVarName, String functionName)
    {
        TrackedPojo stub = findOrCreate(pojoVarName);
        stub.encodeFunctionName = functionName;
    }

    //--------------------------------------------
    /**
     * If generated, returns the POJO function name for
     * the particular variable
     */
    //--------------------------------------------
    public String getPojoEncodeFunction(String pojoVarName)
    {
        TrackedPojo stub = pojosGenerated.get(pojoVarName);
        if (stub != null)
        {
            return stub.encodeFunctionName;
        }
        return null;
    }

    //--------------------------------------------
    /**
     * adds a POJO function to the tracker
     */
    //--------------------------------------------
    public void addPojoDecodeFunction(String pojoVarName, String functionName)
    {
        TrackedPojo stub = findOrCreate(pojoVarName);
        stub.decodeFunctionName = functionName;
    }

    //--------------------------------------------
    /**
     * If generated, returns the POJO function name for
     * the particular variable
     */
    //--------------------------------------------
    public String getPojoDecodeFunction(String pojoVarName)
    {
        TrackedPojo stub = pojosGenerated.get(pojoVarName);
        if (stub != null)
        {
            return stub.decodeFunctionName;
        }
        return null;
    }

    private TrackedPojo findOrCreate(String pojoVarName)
    {
        TrackedPojo retVal = pojosGenerated.get(pojoVarName);
        if (retVal == null)
        {
            retVal = new TrackedPojo();
            pojosGenerated.put(pojoVarName, retVal);
        }

        return retVal;
    }


    private static class TrackedPojo
    {
        String  headerFilename;
        String  encodeFunctionName;
        String  decodeFunctionName;
    }
}

//+++++++++++
//EOF
//+++++++++++