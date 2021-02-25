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

import com.icontrol.event.producer.JavaEventProducer;
import com.icontrol.ipc.IPCReceiverHandler;
import com.icontrol.registry.ServiceHandle;
import com.icontrol.registry.Visibility;

//-----------------------------------------------------------------------------
// Class definition:    SameProcessServiceHandle
/**
 *  ServiceHandle implementation to represent a Service that is
 *  running in the same VM as the ServiceRegistry, and does not need
 *  to have IPC/Events marshaled.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class SameProcessServiceHandle extends ServiceHandle
{
    private JavaEventProducer   eventProducer;
    private IPCReceiverHandler  ipcHandler;
    

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public SameProcessServiceHandle(String name, int ipc, int event)
    {
        super(name, ipc, event);
    }

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public SameProcessServiceHandle(String name, int ipc, int event, Visibility vis)
    {
        super(name, ipc, event, vis);
    }

    //--------------------------------------------
    /**
     * @return Returns the ipcHandler.
     */
    //--------------------------------------------
    public IPCReceiverHandler getIpcHandler()
    {
        return ipcHandler;
    }

    //--------------------------------------------
    /**
     * @param handler The ipcHandler to set.
     */
    //--------------------------------------------
    public void setIpcHandler(IPCReceiverHandler handler)
    {
        this.ipcHandler = handler;
    }

    //--------------------------------------------
    /**
     * @return Returns the eventProducer.
     */
    //--------------------------------------------
    public JavaEventProducer getEventProducer()
    {
        return eventProducer;
    }

    //--------------------------------------------
    /**
     * @param producer The eventProducer to set.
     */
    //--------------------------------------------
    public void setEventProducer(JavaEventProducer producer)
    {
        this.eventProducer = producer;
    }


    //--------------------------------------------
    /**
     * @see com.icontrol.registry.ServiceHandle#isSameProcess()
     */
    //--------------------------------------------
    @Override
    public boolean isSameProcess()
    {
        return true;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.registry.ServiceHandle#isLocalHost()
     */
    //--------------------------------------------
    @Override
    public boolean isLocalHost()
    {
        // shouldn't be called since we're in the same process
        // but technically correct that we are running in the
        // same process as our Registry instance
        //
        return true;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.registry.ServiceHandle#isRemoteHost()
     */
    //--------------------------------------------
    @Override
    public boolean isRemoteHost()
    {
        return false;
    }
}

//+++++++++++
//EOF
//+++++++++++