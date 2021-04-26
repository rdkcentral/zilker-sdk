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

import java.util.Iterator;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.xmlns.service.ICService;

//-----------------------------------------------------------------------------
/**
 * Base set of methods for each of the helper objects used by Processor
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public abstract class ProcessorHelper
{
    //--------------------------------------------
    /**
     * Called by Processor during initialization
     */
    //--------------------------------------------
    abstract void analyze(ICService top, Context ctx);

    //--------------------------------------------
    /**
     * Called by Processor during code generation
     */
    //--------------------------------------------
    abstract Iterator<Wrapper> getWrappers();

    //--------------------------------------------
    /**
     * Returns if any Wrapper will be generated in Java
     */
    //--------------------------------------------
    public boolean shouldGenerateJava(Context ctx)
    {
        // loop through instances until we find one that
        // supports generating Java code
        //
        Iterator<Wrapper> loop = getWrappers();
        if (loop == null)
        {
            return false;
        }
        while (loop.hasNext() == true)
        {
            Wrapper impl = loop.next();
            if (impl != null)
            {
                if (impl.getLanguageMask().hasJava() == true)
                {
                    return true;
                }
            }
        }
        return false;
    }

    //--------------------------------------------
    /**
     * Returns if any Wrapper will be generated in C
     */
    //--------------------------------------------
    public boolean shouldGenerateC(Context ctx)
    {
        // loop through instances until we find one that
        // supports generating C code
        //
        Iterator<Wrapper> loop = getWrappers();
        if (loop == null)
        {
            return false;
        }
        while (loop.hasNext() == true)
        {
            Wrapper impl = loop.next();
            if (impl != null)
            {
                if (impl.getLanguageMask().hasNative() == true)
                {
                    return true;
                }
            }
        }
        return false;
    }
    
    //--------------------------------------------
    /**
     * Simplified way to extract the native 'constructor' function name from a pojo or event
     */
    //--------------------------------------------
    public static String getNativeConstructorForVar(CustomVariable var)
    {
    	Wrapper w = null;
    	String search = var.getName();
    	
    	// try Pojo first
    	//
    	if ((w = Processor.getPojoProcessor().getPojoForName(search)) != null)
    	{
    		return w.getContextNative().getCreateFunctionName();
    	}
    	
    	// see if Event
    	//
    	if ((w = Processor.getEventProcessor().getEventForName(search)) != null)
    	{
    		return w.getContextNative().getCreateFunctionName();
    	}
    	
    	// not defined
    	//
    	return null;
    }
    
    //--------------------------------------------
    /**
     * Simplified way to extract the native 'destructor' function name from a pojo or event
     */
    //--------------------------------------------
    public static String getNativeDestructorForVar(CustomVariable var)
    {
    	Wrapper w = null;
    	String search = var.getName();
    	
    	// try Pojo
    	//
    	if ((w = Processor.getPojoProcessor().getPojoForName(search)) != null)
    	{
    		return w.getContextNative().getDestroyFunctionName();
    	}
    	
    	// try Event
    	//
    	if ((w = Processor.getEventProcessor().getEventForName(search)) != null)
    	{
    		return w.getContextNative().getDestroyFunctionName();
    	}
    	
    	// not defined
    	//
    	return null;
    }
    
    //--------------------------------------------
    /**
     * Simplified way to extract the native 'destructor' function name from a pojo or event
     */
    //--------------------------------------------
    public static String getNativeDestructorForVarFromList(CustomVariable var)
    {
    	Wrapper w = null;
    	String search = var.getName();
    	
    	// try Pojo
    	//
    	if ((w = Processor.getPojoProcessor().getPojoForName(search)) != null)
    	{
    		return w.getContextNative().getDestroyFromListFunctionName();
    	}
    	
    	// try Event
    	//
    	if ((w = Processor.getEventProcessor().getEventForName(search)) != null)
    	{
    		return w.getContextNative().getDestroyFromListFunctionName();
    	}
    	
    	// not defined
    	//
    	return null;
    }

    //--------------------------------------------
    /**
     * Simplified way to extract the native 'destructor' function name from a pojo or event
     */
    //--------------------------------------------
    public static String getNativeDestructorForVarFromMap(CustomVariable var)
    {
    	Wrapper w = null;
    	String search = var.getName();
    	
    	// try Pojo
    	//
    	if ((w = Processor.getPojoProcessor().getPojoForName(search)) != null)
    	{
    		return w.getContextNative().getDestroyFromMapFunctionName();
    	}
    	
    	// try Event
    	//
    	if ((w = Processor.getEventProcessor().getEventForName(search)) != null)
    	{
    		return w.getContextNative().getDestroyFromMapFunctionName();
    	}
    	
    	// not defined
    	//
    	return null;
    }
    
    //--------------------------------------------
    /**
     * Simplified way to extract the native 'encode' function name from a pojo or event
     */
    //--------------------------------------------
    public static String getNativeEncodeForVar(CustomVariable var)
    {
    	Wrapper w = null;
    	String search = var.getName();
    	
    	// Pojo
    	//
    	if ((w = Processor.getPojoProcessor().getPojoForName(search)) != null)
    	{
    		return w.getContextNative().getEncodeFunctionName();
    	}
    	
    	// Event
    	//
    	if ((w = Processor.getEventProcessor().getEventForName(search)) != null)
    	{
    		return w.getContextNative().getEncodeFunctionName();
    	}
    	
    	// not defined
    	//
    	return null;
    }

    //--------------------------------------------
    /**
     * Simplified way to extract the native 'decode' function name from a pojo or event
     */
    //--------------------------------------------
    public static String getNativeDecodeForVar(CustomVariable var)
    {
    	Wrapper w = null;
    	String search = var.getName();
    	
    	// try Pojo first
    	//
    	if ((w = Processor.getPojoProcessor().getPojoForName(search)) != null)
    	{
    		return w.getContextNative().getDecodeFunctionName();
    	}
    	
    	// see if Event
    	//
    	if ((w = Processor.getEventProcessor().getEventForName(search)) != null)
    	{
    		return w.getContextNative().getDecodeFunctionName();
    	}
    	
    	// not defined
    	//
    	return null;
    }
}
