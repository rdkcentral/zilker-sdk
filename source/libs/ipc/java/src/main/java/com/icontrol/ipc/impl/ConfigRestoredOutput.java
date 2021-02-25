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

import java.util.*;
import com.icontrol.ipc.impl.*;
import org.json.*;
import com.icontrol.ipc.impl.ConfigRestoredAction;


//-----------------------------------------------------------------------------
// Class definition:    ConfigRestoredOutput
/**
 *  POJO to contain values for a API or Event object specific to the stockMessages Service.
 *  Implements ObjectIPC so that it can be encoded/decoded and transported
 *  to C and/or Java recipients
 *  <p>
 *  THIS IS AN AUTO-GENERATED FILE, DO NOT EDIT!!!!!!!!
 **/
//-----------------------------------------------------------------------------
public class ConfigRestoredOutput implements ObjectIPC
{


    private ConfigRestoredAction action;  

    
    //--------------------------------------------
    /**
     * Empty Constructor for dynamic allocation
     */
    //--------------------------------------------
    public ConfigRestoredOutput()
    {

    }
    

    //--------------------------------------------
    /**
     * create clone of ConfigRestoredOutput
     */
    //--------------------------------------------
    public ConfigRestoredOutput deepClone()
    {
        try
        {
            JSONObject data = toJSON();
	        ConfigRestoredOutput copy = new ConfigRestoredOutput();
            copy.fromJSON(data);
            return copy;
        }
        catch (JSONException oops)
        {
            throw new RuntimeException("Error cloning ConfigRestoredOutput", oops);
        }
    }

	public ConfigRestoredAction getAction()
	{
		return action;
	}

	public void setAction(ConfigRestoredAction val)
	{
		action = val;
	}



    //--------------------------------------------
    /**
     * @see java.lang.Object#toString()
     */
    //--------------------------------------------
    @Override
    public String toString()
    {
        return "[configRestoredOutput = "+" action='"+action+"'";
    }

    //--------------------------------------------
    /**
     * Encode using JSON
     */
    //--------------------------------------------
    public JSONObject toJSON() throws JSONException
    {
        JSONObject retVal = new JSONObject();
        JSONObject configRestoredOutput_json = new JSONObject();
        if (action != null)
        {
            configRestoredOutput_json.put("action", action.getNumValue());
        }
        retVal.put("configRestoredOutput", configRestoredOutput_json);
        return retVal;

    }

    //--------------------------------------------
    /**
     * Decode using JSON
     */
    //--------------------------------------------
    public void fromJSON(JSONObject buffer) throws JSONException
    {
        JSONObject configRestoredOutput_json = (JSONObject)buffer.opt("configRestoredOutput");
        if (configRestoredOutput_json != null)
        {
            action = ConfigRestoredAction.enumForInt(configRestoredOutput_json.optInt("action"));
        }

    }
}

//+++++++++++
//EOF
//+++++++++++
