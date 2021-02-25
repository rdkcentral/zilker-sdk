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

package com.icontrol.ipc.impl;

//-----------
//imports
//-----------

import java.util.Date;

import org.json.JSONException;
import org.json.JSONObject;

//-----------------------------------------------------------------------------
// Class definition:    DateIPC
/**
 *  ObjectIPC that can be used when the input/output is a Date object
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class DateIPC implements ObjectIPC
{
    public static final String MY_KEY = "t";       // do not change.  generated code depends on this

    private Date value;
    
    //--------------------------------------------
    /**
     * Empty constructor for dynamic allocation
     */
    //--------------------------------------------
    public DateIPC()
    {
    }

    //--------------------------------------------
    /**
     * Basic constructor for setting initial value
     */
    //--------------------------------------------
    public DateIPC(Date val)
    {
        value = val;
    }

    //--------------------------------------------
    /**
     * @return Returns the value.
     */
    //--------------------------------------------
    public Date getValue()
    {
        return value;
    }

    //--------------------------------------------
    /**
     * @param amt The value to set.
     */
    //--------------------------------------------
    public void setValue(Date amt)
    {
        this.value = amt;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.ipc.ObjectIPC#toJSON()
     */
    //--------------------------------------------
    public JSONObject toJSON() throws JSONException
    {
        JSONObject retVal = new JSONObject();
        if (value != null)
        {
            // output the time in millis
            //
            retVal.put(MY_KEY, value.getTime());
        }
        return retVal;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.ipc.ObjectIPC#fromJSON(org.json.JSONObject)
     */
    //--------------------------------------------
    public void fromJSON(JSONObject buffer) throws JSONException
    {
        // stored as long, so extract as such
        //
        long time = buffer.optLong(MY_KEY);
        if (time != 0)
        {
            value = new Date(time);
        }
    }
}

//+++++++++++
//EOF
//+++++++++++