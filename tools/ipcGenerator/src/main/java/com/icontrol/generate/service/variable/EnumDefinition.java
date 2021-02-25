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

package com.icontrol.generate.service.variable;

//-----------
//imports
//-----------

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;

import com.icontrol.xmlns.service.ICEnumDefinition;
import com.icontrol.xmlns.service.ICEnumValue;

//-----------------------------------------------------------------------------
// Class definition:    EnumDefinition
/**
 *  Model an enumeration definition (the 'typedef' from the XML file).
 *  Different from an EnumVariable in that this has no instance, it
 *  only describes the enumeration itself.  Once the enum is used 
 *  (part of a CustomVar, Event, or IPC Message), it is then referred
 *  to as an EnumVar so that it has a type and a name
 *  
 *  @see EnumVariable
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class EnumDefinition
{
    private ArrayList<EnumValue> values;
    private String name;
    private String desc;
    private boolean nativeDefineExists;
    private boolean doNativeLabels;
    private boolean doNativeShortNames;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public EnumDefinition(ICEnumDefinition bean)
    {
        // get list of Fields for this custom object
        //
        name = bean.getEnumTypeName();
        desc = bean.getDescription();
        nativeDefineExists = bean.getNativeDefExists();
        doNativeLabels = bean.getCreateNativeLabels();
        doNativeShortNames = bean.getCreateShortLabels();
        values = new ArrayList<EnumValue>();

        List<ICEnumValue> list =  bean.getValueList();
        for (int i = 0 ; i < list.size() ; i++)
        {
            ICEnumValue curr = list.get(i);
            EnumValue save = new EnumValue();
            save.value = curr.getValue();
            save.shortName = curr.getShortName();
            if (curr.isSetNumeric() == true)
            {
                save.numeric = curr.getNumeric();
            }
            else
            {
                // keep them numerically indexed similar C.
                // (i.e. start with 0, then 1, etc)
                //
                save.numeric = i;
            }

            save.desc = curr.getDescription();
            values.add(save);
        }
    }
    
    //--------------------------------------------
    /**
     * @return Returns the name.
     */
    //--------------------------------------------
    public String getName()
    {
        return name;
    }

    //--------------------------------------------
    /**
     * @return Returns the desc.
     */
    //--------------------------------------------
    public String getDescription()
    {
        return desc;
    }

    //--------------------------------------------
    /**
     * If true, that means this enum is defined somewhere else
     * in the native code; and thereofore should not be part of
     * the code generation
     */
    //--------------------------------------------
    public boolean doesNativeDefinitionExist()
    {
        return nativeDefineExists;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public boolean generateNativeLabels()
    {
        return doNativeLabels;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public boolean generateNativeShortNames()
    {
        return doNativeShortNames;
    }

    //--------------------------------------------
    /**
     * Returns an iteration of the enumeration strings
     */
    //--------------------------------------------
    public Iterator<EnumValue> getEnumValues()
    {
        return values.iterator();
    }

    public int getEnumCount()
    {
        return values.size();
    }

    //-----------------------------------------------------------------------------
    // Class definition:    EnumValue
    /**
     *  Container of a single enumeration definition
     **/
    //-----------------------------------------------------------------------------
    static class EnumValue
    {
        String value;   // name of the enumeration value
        int numeric;    // numeric value of the enumeration value
        String desc;    // description (used for comments)
        String shortName; // a short name for the enumeration value
    }
}

//+++++++++++
//EOF
//+++++++++++