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

import com.icontrol.generate.service.context.ContextJava;
import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.generate.service.io.LanguageMask;
import com.icontrol.generate.service.variable.BaseVariable;

//-----------------------------------------------------------------------------
/**
 * Used when generating code via the LanguageFormat class
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public abstract class Wrapper
{
    SectionMask     sectionMask;    // sections this is referenced by
    LanguageMask    langMask;       // languages to generate
    ContextJava     ctxJava;
    ContextNative   ctxNative;
    boolean			inList;			// true of part of another ArrayVariable definition
    boolean			inMap;			// true of part of another MapVariable definition

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    Wrapper()
    {
        langMask = new LanguageMask();
        sectionMask = new SectionMask();
        ctxJava = new ContextJava();
        ctxNative = new ContextNative();
    }

    //--------------------------------------------
    /**
     * Return the BaseVariable this Wrapper represents
     */
    //--------------------------------------------
    public abstract BaseVariable getVariable();

    //--------------------------------------------
    /**
     * Used during the analyze phase where sections are determined
     */
    //--------------------------------------------
    public SectionMask getSectionMask()
    {
        return sectionMask;
    }

    //--------------------------------------------
    /**
     * Used during the analyze phase where languages are determined
     */
    //--------------------------------------------
    public LanguageMask getLanguageMask()
    {
        return langMask;
    }

    //--------------------------------------------
    /**
     * Used when preparing for Java generation
     */
    //--------------------------------------------
    public ContextJava getContextJava()
    {
        return ctxJava;
    }

    //--------------------------------------------
    /**
     * Used when preparing for C generation
     */
    //--------------------------------------------
    public ContextNative getContextNative()
    {
        return ctxNative;
    }
    
    public boolean isInList() 
    {
		return inList;
	}

	void setInList(boolean flag) 
	{
		inList = flag;
	}

	public boolean isInMap() 
	{
		return inMap;
	}

	void setInMap(boolean flag) 
	{
		inMap = flag;
	}
	
    //--------------------------------------------
    /**
     * Debugging
     */
    //--------------------------------------------
    public String toString()
    {
        BaseVariable var = getVariable();
        if (var != null)
        {
            return var.getName();
        }
        return super.toString();
    }

}
