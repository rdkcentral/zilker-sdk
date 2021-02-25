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

import java.util.Iterator;

//-----------
//imports
//-----------

import com.icontrol.xmlns.service.ICEvent;

//-----------------------------------------------------------------------------
// Class definition:    EventVariable
/**
 *  Represent a defined Event from the XML
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class EventVariable extends CustomVariable
{

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public EventVariable(ICEvent bean)
    {
        super(bean);

        // by default, set BaseEvent as our parent
        // to make JSON generation work the way we want
        // (i.e call super.toJSON first)
        //
        setParentClassName("BaseEvent", false);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isEvent()
     */
    //--------------------------------------------
    @Override
    public boolean isEvent()
    {
        return true;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.CustomVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        // adjust our class-name to end in Event (if necessary)
        //
        String retVal = super.getJavaType();
        if (retVal.endsWith("Event") == false)
        {
            retVal += "Event";
        }
        
        return retVal;
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    public String getAdapterNativeInitString(String parentName)
    {
        StringBuffer buff = new StringBuffer();
        String pad = getIndentBuffer(3);
        
        // look at the variables within our CustomVar.  If any are
        // other CustomVariable objects, then need an init line to memset & store it
        //
        Iterator<BaseVariable> list = getVariables().iterator();
        while (list.hasNext() == true)
        {
            BaseVariable next = list.next();
            if (next.isEvent() == true)
            {
                // recurse (in a sense)
                //
                EventVariable sub = (EventVariable)next;
                buff.append(sub.getAdapterNativeInitString(getName()));
            }
            else if (next.isCustom() == true)
            {
                // add 3 lines that look something like:
                //   deviceInfo d;
                //   memset(&d, 0, sizeof(deviceInfo));
                //   event.d = &d;
                //
                CustomVariable cv = (CustomVariable)next;
                buff.append(pad+next.getCtype()+" "+cv.getLocalName()+";\n");
                buff.append(pad+"memset(&"+cv.getLocalName()+", 0, sizeof("+cv.getCtype()+"));\n");
                buff.append(pad+parentName+"."+cv.getLocalName()+" = &"+cv.getLocalName()+";\n\n");
            }
        }

        return buff.toString();
    }
}

//+++++++++++
//EOF
//+++++++++++