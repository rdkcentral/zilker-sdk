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

package com.icontrol.registry;

//-----------
//imports
//-----------


//-----------------------------------------------------------------------------
// Class definition:    ServiceHandle
/**
 *  Container of basic information about a particular Service.  This is
 *  used by clients to identify the target of IPC or event-registration; and
 *  used by services to register themselves for remote or direct invocation.
 *  <p>
 *  Tracked within the ServiceRegistry so that clients can quicly determine
 *  if the running service is in the same JRE, same host, or remotely accessible.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public abstract class ServiceHandle
{
    private String      serviceName;
    private int         ipcPort;
    private int         eventPort;
    private Visibility  visibility;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public ServiceHandle(String name, int ipc, int event)
    {
        // assume visibility to the local host in our eco-system
        //
        this(name, ipc, event, Visibility.LOCAL_HOST);
    }
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public ServiceHandle(String name, int ipc, int event, Visibility vis)
    {
        serviceName = name;
        ipcPort = ipc;
        eventPort = event;
        visibility = vis;
    }

    //--------------------------------------------
    /**
     * @return Returns the serviceName.
     */
    //--------------------------------------------
    public String getServiceName()
    {
        return serviceName;
    }

    //--------------------------------------------
    /**
     * @return Returns the ipcPort.
     */
    //--------------------------------------------
    public int getIpcPort()
    {
        return ipcPort;
    }

    //--------------------------------------------
    /**
     * @return Returns the eventPort.
     */
    //--------------------------------------------
    public int getEventPort()
    {
        return eventPort;
    }

    //--------------------------------------------
    /**
     * @return Returns the visibility.
     */
    //--------------------------------------------
    public Visibility getVisibility()
    {
        return visibility;
    }

    //--------------------------------------------
    /**
     * Returns true if this service is executing in
     * the same process as the caller.
     */
    //--------------------------------------------
    public abstract boolean isSameProcess();

    //--------------------------------------------
    /**
     * Returns true if this service is executing on
     * the host as the caller.
     */
    //--------------------------------------------
    public abstract boolean isLocalHost();

    //--------------------------------------------
    /**
     * Return true if this service is executing on
     * a different host then the caller
     */
    //--------------------------------------------
    public abstract boolean isRemoteHost();
}

//+++++++++++
//EOF
//+++++++++++