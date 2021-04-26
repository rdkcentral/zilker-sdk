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

package com.icontrol.registry.scope;

//-----------
//imports
//-----------

import com.icontrol.registry.ServiceHandle;
import com.icontrol.registry.Visibility;

//-----------------------------------------------------------------------------
// Class definition:    RemoteHostServiceHandle
/**
 *  ServiceHandle implementation to represent a Service that is
 *  running in a different VM and host then the ServiceRegistry, 
 *  and therefore must marshal/unmarshal data for IPC and Events
 *  (presumably through a proxy service).
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class RemoteHostServiceHandle extends ServiceHandle
{
    private String remoteIp;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public RemoteHostServiceHandle(String name, int ipc, int event, String ipAddress)
    {
        super(name, ipc, event);
        remoteIp = ipAddress;
    }

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public RemoteHostServiceHandle(String name, int ipc, int event, Visibility vis, String ipAddress)
    {
        super(name, ipc, event, vis);
        remoteIp = ipAddress;
    }

    //--------------------------------------------
    /**
     * @return Returns the remoteIp.
     */
    //--------------------------------------------
    public String getRemoteIp()
    {
        return remoteIp;
    }

    //--------------------------------------------
    /**
     * @param ipAddr The remoteIp to set.
     */
    //--------------------------------------------
    public void setRemoteIp(String ipAddr)
    {
        this.remoteIp = ipAddr;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.registry.ServiceHandle#isSameProcess()
     */
    //--------------------------------------------
    @Override
    public boolean isSameProcess()
    {
        return false;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.registry.ServiceHandle#isLocalHost()
     */
    //--------------------------------------------
    @Override
    public boolean isLocalHost()
    {
        return false;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.registry.ServiceHandle#isRemoteHost()
     */
    //--------------------------------------------
    @Override
    public boolean isRemoteHost()
    {
        return true;
    }
}

//+++++++++++
//EOF
//+++++++++++