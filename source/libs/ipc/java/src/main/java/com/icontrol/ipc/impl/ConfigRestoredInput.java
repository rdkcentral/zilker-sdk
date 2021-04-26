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
// Class definition:    ConfigRestoredInput
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
public class ConfigRestoredInput implements ObjectIPC
{
    private String tempRestoreDir;      // temp directory containing unpacked restore information.  service should use this to locate and parse the old files
    private String dynamicConfigPath;   // same as getDynamicConfigPath(), but provided here to remove need for asking for the storage location               

    //--------------------------------------------
    /**
     * Empty Constructor for dynamic allocation
     */
    //--------------------------------------------
    public ConfigRestoredInput()
    {
    }

	public String getTempRestoreDir()
	{
		return tempRestoreDir;
	}

	public void setTempRestoreDir(String val)
	{
		tempRestoreDir = val;
	}

	public String getDynamicConfigPath()
	{
		return dynamicConfigPath;
	}

	public void setDynamicConfigPath(String val)
	{
		dynamicConfigPath = val;
	}

    //--------------------------------------------
    /**
     * @see java.lang.Object#toString()
     */
    //--------------------------------------------
    @Override
    public String toString()
    {
        return "[configRestoredInput = "+" tempRestoreDir='"+tempRestoreDir+"'"+" dynamicConfigPath='"+dynamicConfigPath+"'";
    }

    //--------------------------------------------
    /**
     * Encode using JSON
     */
    //--------------------------------------------
    public JSONObject toJSON() throws JSONException
    {
        JSONObject retVal = new JSONObject();
        JSONObject configRestoredInput_json = new JSONObject();
        configRestoredInput_json.put("tempRestoreDir", tempRestoreDir);
        configRestoredInput_json.put("dynamicConfigPath", dynamicConfigPath);
        retVal.put("configRestoredInput", configRestoredInput_json);
        return retVal;
    }

    //--------------------------------------------
    /**
     * Decode using JSON
     */
    //--------------------------------------------
    public void fromJSON(JSONObject buffer) throws JSONException
    {
        JSONObject configRestoredInput_json = (JSONObject)buffer.opt("configRestoredInput");
        if (configRestoredInput_json != null)
        {
            tempRestoreDir = configRestoredInput_json.optString("tempRestoreDir");
            dynamicConfigPath = configRestoredInput_json.optString("dynamicConfigPath");
        }
    }
}

//+++++++++++
//EOF
//+++++++++++
