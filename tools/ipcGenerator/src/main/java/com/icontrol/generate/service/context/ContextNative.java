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

import java.util.HashMap;

//-----------------------------------------------------------------------------

/**
 * Set of variables that are common to generating Java files, but
 * specific enough to have direct variables for (instead of tossing into
 * a hash).
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class ContextNative
{
    private String  headerFileName;
    private String  sourceFileName;
    private String  encodeFunctionName;     // specific to items that can be converted to JSON
    private String  decodeFunctionName;     // specific to items that can be converted to JSON
    private String  createFunctionName;		// allocate the object
    private String  destroyFunctionName;	// destroy the object (deep)
    private String  destroyFromListFunctionName;	// if the object can be added to a list, the 'linkedListItemFreeFunc' implementation for that
    private String  destroyFromMapFunctionName;		// if the object can be added to a map, the 'hashMapFreeFunc' implementation for that
    private HashMap<String,String> getFunctionMap;
    private HashMap<String,String> setFunctionMap;

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public ContextNative()
    {
        getFunctionMap = new HashMap<String,String>();
        setFunctionMap = new HashMap<String,String>();
    }

    public boolean wasGenerated()
    {
        // return true if header or source has been set, meaning it was generated
        //
        return (headerFileName != null || sourceFileName != null);
    }

    public String getHeaderFileName()
    {
        return headerFileName;
    }

    public void setHeaderFileName(String fileName)
    {
        headerFileName = fileName;
    }

    public String getSourceFileName()
    {
        return sourceFileName;
    }

    public void setSourceFileName(String fileName)
    {
        sourceFileName = fileName;
    }

    public String getEncodeFunctionName()
    {
        return encodeFunctionName;
    }

    public void setEncodeFunctionName(String functionName)
    {
        encodeFunctionName = functionName;
    }

    public String getDecodeFunctionName()
    {
        return decodeFunctionName;
    }

    public void setDecodeFunctionName(String functionName)
    {
        decodeFunctionName = functionName;
    }

    public String getCreateFunctionName()
    {
        return createFunctionName;
    }

    public void setCreateFunctionName(String functionName)
    {
        createFunctionName = functionName;
    }

    public String getDestroyFunctionName()
    {
        return destroyFunctionName;
    }

    public void setDestroyFunctionName(String functionName)
    {
        destroyFunctionName = functionName;
    }
    
    public String getDestroyFromListFunctionName() 
    {
		return destroyFromListFunctionName;
	}

	public void setDestroyFromListFunctionName(String functionName) 
	{
		destroyFromListFunctionName = functionName;
	}

	public String getDestroyFromMapFunctionName() 
	{
		return destroyFromMapFunctionName;
	}

	public void setDestroyFromMapFunctionName(String functionName) 
	{
		destroyFromMapFunctionName = functionName;
	}

	public void addGetFunction(String varName, String functionName)
    {
        getFunctionMap.put(varName, functionName);
    }
    
    //--------------------------------------------
    /**
     * return the 'getter' function name for this variable
     */
    //--------------------------------------------
    public String getGetFunctionForVar(String varName)
    {
        return getFunctionMap.get(varName);
    }
    
    public void addSetFunction(String varName, String functionName)
    {
        setFunctionMap.put(varName, functionName);
    }

    //--------------------------------------------
    /**
     * return the 'setter' function name for this variable
     */
    //--------------------------------------------
    public String getSetFunctionForVar(String varName)
    {
        return setFunctionMap.get(varName);
    }
}
