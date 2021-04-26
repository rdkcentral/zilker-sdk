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

//-----------------------------------------------------------------------------
// Enumeration definition:    ConfigRestoredAction
/**
 *  Enumeration used by the  Service; specifically when sending/getting
 *  IPC messages and Events.  Supports both C and Java clients.
 *  <p>
 *  THIS IS AN AUTO-GENERATED FILE, DO NOT EDIT!!!!!!!!
 **/
//-----------------------------------------------------------------------------
public enum ConfigRestoredAction
{
    CONFIG_RESTORED_CALLBACK (0), 
    CONFIG_RESTORED_COMPLETE (1), 
    CONFIG_RESTORED_RESTART  (2), 
    CONFIG_RESTORED_FAILED   (3); 


    // for ordinal & C support
    private int numValue;

    //--------------------------------------------
    /**
     * Empty Constructor for dynamic allocation
     */
    //--------------------------------------------
    ConfigRestoredAction(int num)
    {
        numValue = num;
    }
    
    //--------------------------------------------
    /**
     * Return numeric value
     */
    //--------------------------------------------
    public int getNumValue()
    {
        return numValue;
    }
    
    //--------------------------------------------
    /**
     * Translate from C int code to the Enumeration
     */
    //--------------------------------------------
    public static ConfigRestoredAction enumForInt(int code)
    {
        for (ConfigRestoredAction possible : ConfigRestoredAction.values())
        {
            if (possible.numValue == code)
            {
                return possible;
            }
        }
        return null;
    }
}

//+++++++++++
//EOF
//+++++++++++
