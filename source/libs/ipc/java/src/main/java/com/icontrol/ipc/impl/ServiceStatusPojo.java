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

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

//-----------------------------------------------------------------------------
// Class definition:    ServiceStatusPojo
/**
 * Define the standard/stock set of IPC messages that
 * ALL services should handle.  Over time this may grow,
 * but serves the purpose of creating well-known IPC
 * messages without linking in superfluous API libraries
 * or relying on convention.
 *
 * NOTE: originally generated via ipcGenerator (stock_messages.xml)
 *       then hand modified to correlate with the native implemendation
 *       defined in ipcStockMessages.c
 **/
//-----------------------------------------------------------------------------
public class ServiceStatusPojo implements ObjectIPC
{
    private HashMap<String,String> statusMap;   // map of string/string key/value pairs
    
    //--------------------------------------------
    /**
     * Empty Constructor for dynamic allocation
     */
    //--------------------------------------------
    public ServiceStatusPojo()
    {
        statusMap = new HashMap<String,String>();
    }

	/* helper function to return iterator of hash keys */
	public Iterator<String> getKeyIterator()
	{
		return statusMap.keySet().iterator();
	}

	/* helper function to return number of hash items */
	public int getCount()
	{
		return statusMap.size();
	}

	public String getString(String key)
	{
		return statusMap.get(key);
	}

	public void setString(String key, String val)
	{
		statusMap.put(key, val);
	}

	public void setInt(String key, Integer val)
	{
	    if (val != null)
        {
            statusMap.put(key, Integer.toString(val));
        }
	}

    //--------------------------------------------
    /**
     * @see java.lang.Object#toString()
     */
    //--------------------------------------------
    @Override
    public String toString()
    {
        return "[serviceStatusPojo = ";
    }

    //--------------------------------------------
    /**
     * Encode using JSON
     */
    //--------------------------------------------
    public JSONObject toJSON() throws JSONException
    {
        JSONObject retVal = new JSONObject();
        JSONObject serviceStatusPojo_json = new JSONObject();
        if (statusMap != null)
        {
            // need 2 structures to allow proper decoding later on
            //  1 - values
            //  2 - keys
            JSONObject statusMapVals_json = new JSONObject();
            JSONArray statusMapKeys_json = new JSONArray();
            
            // add each value & type
            for (Map.Entry<String,String> entry : statusMap.entrySet())
            {
                // extract key and value, and type
                //
                String key = entry.getKey();
                String val = entry.getValue();
                if (val != null)
                {
                    // save the key & value for decode
                    //
                    statusMapVals_json.put(key, (String)val);
                    statusMapKeys_json.put(key);
                }
            }
            
            // add all 3 containers to the parent
            //
            serviceStatusPojo_json.put("mapVals", statusMapVals_json);
            serviceStatusPojo_json.put("mapKeys", statusMapKeys_json);
        }
        retVal.put("serviceStatusPojo", serviceStatusPojo_json);
        return retVal;

    }

    //--------------------------------------------
    /**
     * Decode using JSON
     */
    //--------------------------------------------
    public void fromJSON(JSONObject buffer) throws JSONException
    {
        JSONObject serviceStatusPojo_json = (JSONObject)buffer.opt("serviceStatusPojo");
        if (serviceStatusPojo_json != null)
        {
            statusMap.clear();
            statusMap.clear();

            // get the 2 structures added during encoding:
            //  1 - values
            //  2 - keys
            JSONObject statusMapVals_json = serviceStatusPojo_json.optJSONObject("mapVals");
            JSONArray statusMapKeys_json = serviceStatusPojo_json.optJSONArray("mapKeys");
            if (statusMapVals_json != null && statusMapKeys_json != null)
            {
                // loop through keys to extract values
                //
                int size = statusMapKeys_json.length();
                for (int i = 0 ; i < size ; i++)
                {
                    // get the next key
                    //
                    String key = statusMapKeys_json.optString(i);
                    if (key == null)
                    {
                        continue;
                    }

                    // add as string
                    String s = statusMapVals_json.optString(key);
                    if (s != null)
                    {
                        setString(key, s);
                    }
                }
            }
        }
    }
}

//+++++++++++
//EOF
//+++++++++++
