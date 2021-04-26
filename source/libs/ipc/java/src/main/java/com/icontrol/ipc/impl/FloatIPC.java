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

import org.json.JSONException;
import org.json.JSONObject;

//-----------------------------------------------------------------------------
// Class definition:    FloatIPC
/**
 *  ObjectIPC that can be used when the input/output is simply a Float
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class FloatIPC implements ObjectIPC
{
    public static final String MY_KEY = "f";       // do not change.  generated code depends on this

    private float value;
    
    //--------------------------------------------
    /**
     * Empty constructor for dynamic allocation
     */
    //--------------------------------------------
    public FloatIPC()
    {
    }

    //--------------------------------------------
    /**
     * Basic constructor for setting initial value
     */
    //--------------------------------------------
    public FloatIPC(float val)
    {
        value = val;
    }

    //--------------------------------------------
    /**
     * @return Returns the value.
     */
    //--------------------------------------------
    public float getValue()
    {
        return value;
    }

    //--------------------------------------------
    /**
     * @param amt The value to set.
     */
    //--------------------------------------------
    public void setValue(float amt)
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
        retVal.put(MY_KEY, value);
        return retVal;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.ipc.ObjectIPC#fromJSON(org.json.JSONObject)
     */
    //--------------------------------------------
    public void fromJSON(JSONObject buffer) throws JSONException
    {
        // allow for null values, so ask for optional vs mandatory
        //
        value = (float)buffer.optDouble(MY_KEY, 0.0);
    }
}

//+++++++++++
//EOF
//+++++++++++