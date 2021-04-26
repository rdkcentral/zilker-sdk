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

import java.util.Random;
import java.util.concurrent.ConcurrentHashMap;

import com.icontrol.registry.scope.SameProcessServiceHandle;

//-----------------------------------------------------------------------------
// Class definition:    ServiceRegistry
/**
 *  Registry of known services.  Primarily it will contain each running with 
 *  this JRE instance, but is updated (by another means) with the full set of 
 *  services that are on our local host and remote hosts within the system.  
 *  <p>
 *  Used by IPC and Event-listeners to determine the targeted service location 
 *  (same vm, same host, etc) to optimize the calls and skip marshaling if possible.
 *  <p>
 *  Note that there should be 'one of these per JRE', and the synchronization
 *  of each instance will be handled by an outside service (not inside this class).  
 *  <p>
 *  This implementation is part of the Service Bridge Library to help reduce the 
 *  amount of library dependencies and keep calls quick and low-overhead.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class ServiceRegistry
{
    // ideally, we would want to know our physical process id to aid in the determination
    // if a service is within the same process or not.  Unfortunately there is not a 
    // common way within Java to get the pid, so just generate a random number and hope
    // that other VM instances do not generate the exact same random number
    //
    private static long processId = -1;
    private static ConcurrentHashMap<Integer,ServiceHandle> lookup;
    
    //--------------------------------------------
    /**
     * Called by a Service when it completes it's startup initialization
     * and is ready to begin responding to requests and/or delivering events
     * <p>
     * Although the set of services tracked in this object may not all be
     * 'local to our VM', when adding the service this way it must be true.
     */
    //--------------------------------------------
    public static void registerService(SameProcessServiceHandle service)
    {
        // ensure our internal variables are initialized
        //
        init();
        
        // add the service to our hash.  Since the port numbers are
        // unique, go ahead and use both the IPC and Event ports as
        // lookup values for this service instance
        //
        int ipcPort = service.getIpcPort();
        if (ipcPort > 0)
        {
            // this service has an IPC, so add that port to the lookup
            //
            lookup.put(ipcPort, service);
        }
        
        int eventPort = service.getEventPort();
        if (eventPort > 0)
        {
            // this service has an event producer, so add that port to the lookup
            //
            lookup.put(eventPort, service);
        }
        
        // TODO: inform other ServiceRegistry objects (other VMs & other hosts)
    }

    //--------------------------------------------
    /**
     * Return the ServiceHandle for this IPC port number
     */
    //--------------------------------------------
    public static ServiceHandle getServiceForIpcPort(int port)
    {
        // ensure our internal variables are initialized
        //
        init();
        return lookup.get(port);
    }

    //--------------------------------------------
    /**
     * Return the ServiceHandle for this event port number
     */
    //--------------------------------------------
    public static ServiceHandle getServiceForEventPort(int port)
    {
        // ensure our internal variables are initialized
        //
        init();
        return lookup.get(port);
    }

    //--------------------------------------------
    /**
     * Initialize the lookup map and generate our unique identifier.
     * Only needs to be called once per VM.
     */
    //--------------------------------------------
    private static synchronized void init()
    {
        if (processId == -1)
        {
            // as noted above, generate a unique identifier in lieu of  
            // gathering our process id (since we cannot assume Oracle JRE, Android, etc)
            //
            Random helper = new Random();
            while (processId == -1)
            {
                processId = helper.nextLong();
            }

            // init the cache of service handles
            //
            lookup = new ConcurrentHashMap<Integer,ServiceHandle>();
        }
    }

    //--------------------------------------------
    /**
     *  TODO: do we need this???
     */
    //--------------------------------------------
    protected static long getProcessIdentifier()
    {
        init();
        synchronized (ServiceRegistry.class)
        {
            return processId;
        }
    }

    //--------------------------------------------
    /**
     *  
     */
    //--------------------------------------------
    protected void registerService(ServiceHandle service, Visibility vis)
    {
        // TODO: hook for running service to sync.  
        //       add to our hash as a LocalHost or RemoteHost handle
        
    }
    
    //--------------------------------------------
    /**
     * Hidden constructor for this class
     */
    //--------------------------------------------
    private ServiceRegistry()
    {
    }
}

//+++++++++++
//EOF
//+++++++++++