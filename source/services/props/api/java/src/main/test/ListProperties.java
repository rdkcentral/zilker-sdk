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


//-----------
//imports
//-----------

import java.util.Iterator;

import com.icontrol.api.properties.Property;
import com.icontrol.api.properties.PropertyKeys;
import com.icontrol.api.properties.PropertyValues;
import com.icontrol.api.properties.PropsService_IPC;
import com.icontrol.ipc.IPCMessage;
import com.icontrol.ipc.IPCSender;

//-----------------------------------------------------------------------------
// Class definition:    ListProperties
/**
 *  CLI Test to connect to the native propsService and list out the property keys
 *  Intended to test dynamic arrays from C to Java
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class ListProperties
{
    //--------------------------------------------
    /**
     * @param args
     */
    //--------------------------------------------
    public static void main(String[] args)
    {
        try
        {
            // first get all of the keys
            //
            System.out.println();
            System.out.println("test: retrieving all property keys...");
            if (getKeys() == false)
            {
                System.exit(1);
            }
            
            // now all of the property values
            //
            System.out.println();
            System.out.println("test: retrieving all property values...");
            if (getProperties() == false)
            {
                System.exit(1);
            }
        }
        catch (Exception oops)
        {
            System.err.println("Error communicating with propsService");
            oops.printStackTrace();
            System.exit(1);
        }
        
        System.exit(0);
    }
    
    private static boolean getKeys()
        throws Exception
    {
        PropertyKeys output = new PropertyKeys();
        
        // send the request
        //
        System.out.println("connecting to service port="+PropsService_IPC.PROPSSERVICE_IPC_PORT_NUM+" msg="+PropsService_IPC.GET_ALL_KEYS);
        IPCMessage resp = IPCSender.sendRequest(PropsService_IPC.PROPSSERVICE_IPC_PORT_NUM, PropsService_IPC.GET_ALL_KEYS, null, output);

        // look to see if success
        //
        if (resp != null && resp.getReturnCode() == 0)
        {
            // success return, print results
            //
            System.out.println("got back "+output.getList().size()+" property keys");
            Iterator<String> loop = output.getList().iterator();
            while (loop.hasNext() == true)
            {
                System.out.println(loop.next());
            }
            
            return true;
        }
        else
        {
            System.err.println("Received non-success code from propsService; rc="+resp.getReturnCode());
            return false;
        }
    }
    
    private static boolean getProperties()
        throws Exception
    {
        PropertyValues output = new PropertyValues();
        
        // send the request
        //
        System.out.println("connecting to service port="+PropsService_IPC.PROPSSERVICE_IPC_PORT_NUM+" msg="+PropsService_IPC.GET_ALL_KEY_VALUES);
        IPCMessage resp = IPCSender.sendRequest(PropsService_IPC.PROPSSERVICE_IPC_PORT_NUM, PropsService_IPC.GET_ALL_KEY_VALUES, null, output);

        // look to see if success
        //
        if (resp != null && resp.getReturnCode() == 0)
        {
            // success return, print results
            //
            System.out.println("got back "+output.getCount()+" property values");
            Iterator<String> loop = output.getKeyIterator();
            while (loop.hasNext() == true)
            {
                String key = loop.next();
                System.out.println(key);
                Property p = output.getProperty(key);
                if (p != null)
                {
                    System.out.println("  k="+p.getKey()+" v="+p.getValue());
                }
            }
            
            return true;
        }
        else
        {
            System.err.println("Received non-success code from propsService; rc="+resp.getReturnCode());
            return false;
        }
    }
}

//+++++++++++
//EOF
//+++++++++++