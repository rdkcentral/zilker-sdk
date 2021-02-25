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

//-----------
//imports
//-----------

import java.util.HashMap;

import com.icontrol.substitution.SubstitutionSource;

//-----------------------------------------------------------------------------
// Class definition:    MacroSource
/**
 *  Hashtable of String key/values that works as a SubstitutionSource
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class MacroSource implements SubstitutionSource
{
    private HashMap<String,String>  mapping;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public MacroSource()
    {
        mapping = new HashMap<String,String>();
    }
    
    //--------------------------------------------
    /**
     * Adds a new mapping
     */
    //--------------------------------------------
    public void addMapping(String key, String value)
    {
        mapping.put(key, value);
    }

    //--------------------------------------------
    /**
     * Adds a set of mappings
     */
    //--------------------------------------------
    public void addMapping(MacroSource other)
    {
        mapping.putAll(other.mapping);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.substitution.SubstitutionSource#getKeys()
     */
    //--------------------------------------------
    @Override
    public Object[] getKeys()
    {
        return null;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.substitution.SubstitutionSource#getObject(java.lang.String)
     */
    //--------------------------------------------
    @Override
    public Object getObject(String key)
    {
        return null;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.substitution.SubstitutionSource#getVariable(java.lang.String)
     */
    //--------------------------------------------
    @Override
    public String getVariable(String key)
    {
        return mapping.get(key);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.substitution.SubstitutionSource#hasObjectVariable(java.lang.String)
     */
    //--------------------------------------------
    @Override
    public boolean hasObjectVariable(String key)
    {
        return false;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.substitution.SubstitutionSource#hasStringVariable(java.lang.String)
     */
    //--------------------------------------------
    @Override
    public boolean hasStringVariable(String key)
    {
        return mapping.containsKey(key);
    }
}

//+++++++++++
//EOF
//+++++++++++