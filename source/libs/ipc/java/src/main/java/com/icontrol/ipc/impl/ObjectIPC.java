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
// Class definition:    ObjectIPC
/**
 *  Methods required for an Object to be encoded/decoded for transmission over 
 *  a Socket.  At this time, JSON is the transport syntax utilized.
 *        
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public interface ObjectIPC
{
    //--------------------------------------------
    /**
     * Encode using JSON
     */
    //--------------------------------------------
    public JSONObject toJSON() throws JSONException;

    //--------------------------------------------
    /**
     * Decode using JSON
     */
    //--------------------------------------------
    public void fromJSON(JSONObject buffer) throws JSONException;
}

//+++++++++++
//EOF
//+++++++++++