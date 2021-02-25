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
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;


//-----------------------------------------------------------------------------
// Class definition:    RuntimeStatsPojo
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
public class RuntimeStatsPojo implements ObjectIPC
{
    private String                 serviceName;
    private Date                   collectionTime;                                              
    private HashMap<String,Object> valuesMap;   // two internal variables to represent map
    private HashMap<String,String> typesMap;                                            

    //--------------------------------------------
    /**
     * Empty Constructor for dynamic allocation
     */
    //--------------------------------------------
    public RuntimeStatsPojo()
    {
        valuesMap = new HashMap<String,Object>();
        typesMap = new HashMap<String,String>();
    }

	public String getServiceName()
	{
		return serviceName;
	}

	public void setServiceName(String val)
	{
		serviceName = val;
	}

	public Date getCollectionTime()
	{
		return collectionTime;
	}

	public void setCollectionTime(Date val)
	{
		collectionTime = val;
	}

	/* helper function to return iterator of hash keys */
	public Iterator<String> getKeyIterator()
	{
		return valuesMap.keySet().iterator();
	}

	/* helper function to return number of hash items */
	public int getCount()
	{
		return valuesMap.size();
	}

	public String getString(String key)
	{
		return (String)valuesMap.get(key);
	}

	public Date getDate(String key)
	{
		return (Date)valuesMap.get(key);
	}

	public Integer getInt(String key)
	{
		return (Integer)valuesMap.get(key);
	}

	public Long getLong(String key)
	{
		return (Long)valuesMap.get(key);
	}

	public void setString(String key, String val)
	{
		valuesMap.put(key, val);
		typesMap.put(key, "string");
	}

	public void setDate(String key, Date val)
	{
		valuesMap.put(key, val);
		typesMap.put(key, "date");
	}

	public void setInt(String key, Integer val)
	{
		valuesMap.put(key, val);
		typesMap.put(key, "int");
	}

	public void setLong(String key, Long val)
	{
		valuesMap.put(key, val);
		typesMap.put(key, "long");
	}

    //--------------------------------------------
    /**
     * @see java.lang.Object#toString()
     */
    //--------------------------------------------
    @Override
    public String toString()
    {
        return "[runtimeStatsPojo = "+" serviceName='"+serviceName+"'"+" collectionTime='"+collectionTime+"'";
    }

    //--------------------------------------------
    /**
     * Encode using JSON
     */
    //--------------------------------------------
    public JSONObject toJSON() throws JSONException
    {
        JSONObject retVal = new JSONObject();
        JSONObject runtimeStatsPojo_json = new JSONObject();
        runtimeStatsPojo_json.put("serviceName", serviceName);
        if (collectionTime != null) runtimeStatsPojo_json.put("collectionTime", collectionTime.getTime());
        if (valuesMap != null && typesMap != null)
        {
            // need 3 structures to allow proper decoding later on
            //  1 - values
            //  2 - types
            //  3 - keys
            JSONObject statsMapVals_json = new JSONObject();
            JSONObject statsMapTypes_json = new JSONObject();
            JSONArray statsMapKeys_json = new JSONArray();
            
            // add each value & type
            for (Map.Entry<String,Object> entry : valuesMap.entrySet())
            {
                // extract key, value, and type
                //
                String key = entry.getKey();
                Object val = entry.getValue();
                String kind = typesMap.get(key);
                if (val != null && kind != null)
                {
                    
                    // convert for each possible 'type'
                    if (kind.equals("string") == true)
                    {
                        statsMapVals_json.put(key, (String)val);
                    }
                    else if (kind.equals("date") == true)
                    {
                        statsMapVals_json.put(key, ((Date)val).getTime());
                    }
                    else if (kind.equals("int") == true)
                    {
                        statsMapVals_json.put(key, (Integer)val);
                    }
                    else if (kind.equals("long") == true)
                    {
                        statsMapVals_json.put(key, (Long)val);
                    }

                    // save the key & type for decode
                    //
                    statsMapTypes_json.put(key, kind);
                    statsMapKeys_json.put(key);
                }
            }
            
            // add all 3 containers to the parent
            //
            runtimeStatsPojo_json.put("mapVals", statsMapVals_json);
            runtimeStatsPojo_json.put("mapTypes", statsMapTypes_json);
            runtimeStatsPojo_json.put("mapKeys", statsMapKeys_json);
        }
        retVal.put("runtimeStatsPojo", runtimeStatsPojo_json);
        return retVal;
    }

    //--------------------------------------------
    /**
     * Decode using JSON
     */
    //--------------------------------------------
    public void fromJSON(JSONObject buffer) throws JSONException
    {
        JSONObject runtimeStatsPojo_json = (JSONObject)buffer.opt("runtimeStatsPojo");
        if (runtimeStatsPojo_json != null)
        {
            serviceName = runtimeStatsPojo_json.optString("serviceName");
            long collectionTime_time = runtimeStatsPojo_json.optLong("collectionTime");
            if (collectionTime_time > 0) collectionTime = new Date(collectionTime_time);
            valuesMap.clear();
            typesMap.clear();

            // get the 3 structures added during encoding:
            //  1 - values
            //  2 - types
            //  3 - keys
            JSONObject statsMapVals_json = runtimeStatsPojo_json.optJSONObject("mapVals");
            JSONObject statsMapTypes_json = runtimeStatsPojo_json.optJSONObject("mapTypes");
            JSONArray statsMapKeys_json = runtimeStatsPojo_json.optJSONArray("mapKeys");
            if (statsMapVals_json != null && statsMapTypes_json != null && statsMapKeys_json != null)
            {
                // loop through keys to extract values and types
                //
                int size = statsMapKeys_json.length();
                for (int i = 0 ; i < size ; i++)
                {
                    // get the next key
                    //
                    String key = statsMapKeys_json.optString(i);
                    if (key == null)
                    {
                        continue;
                    }

                    // pull it's type
                    //
                    String type = statsMapTypes_json.optString(key);
                    if (type == null)
                    {
                        continue;
                    }

                    // convert for each possible 'type'
                    if (type.equals("string") == true)
                    {
                        String s = statsMapVals_json.optString(key);
                        if (s != null) setString(key, s);
                    }
                    else if (type.equals("date") == true)
                    {
                        long d = statsMapVals_json.optLong(key);
                        if (d > 0) setDate(key, new Date(d));
                    }
                    else if (type.equals("int") == true)
                    {
                        int s = statsMapVals_json.optInt(key);
                        setInt(key, s);
                    }
                    else if (type.equals("long") == true)
                    {
                        Long s = statsMapVals_json.optLong(key);
                        setLong(key, s);
                    }
                }
            }
        }
    }
}

//+++++++++++
//EOF
//+++++++++++
