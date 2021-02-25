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

package com.icontrol.ipc;

//-----------
//imports
//-----------

import java.io.IOException;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.icontrol.ipc.impl.BooleanIPC;
import com.icontrol.ipc.impl.DoubleIPC;
import com.icontrol.ipc.impl.IntegerIPC;
import com.icontrol.ipc.impl.LongIPC;
import com.icontrol.ipc.impl.ObjectIPC;
import com.icontrol.ipc.impl.StringIPC;

//-----------------------------------------------------------------------------
// Class definition:    IPCHelper
/**
 *  This class simplifies some common/basic IPC calls.
 *  
 *  @author tlea
 **/
//-----------------------------------------------------------------------------
public class IPCHelper {

    private static Logger log = LoggerFactory.getLogger("IPCHelper");
    
    //--------------------------------------------
    /**
     * Simple send that takes no input or output
     */
    //--------------------------------------------
    public static boolean simpleSend(int serverPort, int messageCode) 
    {
        return simpleSend(serverPort, messageCode, null);
    }
    
    /**
     * Simple send that has no output
     */
    public static boolean simpleSend(int serverPort, int messageCode, ObjectIPC input)
    {
        try
        {
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, input, null);
            if (msg != null && msg.getReturnCode() == 0)
            {
                return true;
            }
        }
        catch (IOException ex)
        {
            log.warn(ex.getMessage(), ex);
        }
        
        return false;
    }

    //--------------------------------------------
    /**
     * Simple "get" that returns a single Integer or null on failure.  Can
     * have an optional input argument.
     */
    //--------------------------------------------
    public static Integer simpleGetInt(int serverPort, int messageCode, ObjectIPC arg) 
    {
    	Integer result = null;
    	
        try
        {
            // create our integer output object
            //
            IntegerIPC output = new IntegerIPC();
            
            // send the request
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, arg, output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = output.getValue();
            } 
            else
            {
            	log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleGetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }

    //--------------------------------------------
    /**
     * simple "set" that passes a single Integer.  If the response has output and
     * 'output' is not null, it will be populated with the response.
     */
    //--------------------------------------------
    public static boolean simpleSetInt(int serverPort, int messageCode, int value, ObjectIPC output)
    {
        boolean result = false;
        
        try
        {
            // send the request with our input parm and the optional output parm
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, new IntegerIPC(value), output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = true;
            }
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleSetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }

    //--------------------------------------------
    /**
     * Simple "get" that returns a single Long or null on failure.  Can
     * have an optional input argument.
     */
    //--------------------------------------------
    public static Long simpleGetLong(int serverPort, int messageCode, ObjectIPC arg) 
    {
        Long result = null;
        
        try
        {
            // create our long output object
            //
            LongIPC output = new LongIPC();
            
            // send the request
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, arg, output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = output.getValue();
            } 
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleGetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }

    //--------------------------------------------
    /**
     * simple "set" that passes a single Long.  If the response has output and
     * 'output' is not null, it will be populated with the response.
     */
    //--------------------------------------------
    public static boolean simpleSetLong(int serverPort, int messageCode, long value, ObjectIPC output)
    {
        boolean result = false;
        
        try
        {
            // send the request with our input parm and the optional output parm
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, new LongIPC(value), output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = true;
            }
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleSetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }
    
    //--------------------------------------------
    /**
     * Simple "get" that returns a single Double or null on failure.  Can
     * have an optional input argument.
     */
    //--------------------------------------------
    public static Double simpleGetDouble(int serverPort, int messageCode, ObjectIPC arg) 
    {
        Double result = null;
        
        try
        {
            // create our long output object
            //
            DoubleIPC output = new DoubleIPC();
            
            // send the request
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, arg, output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = output.getValue();
            } 
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleGetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }

    //--------------------------------------------
    /**
     * simple "set" that passes a single Double.  If the response has output and
     * 'output' is not null, it will be populated with the response.
     */
    //--------------------------------------------
    public static boolean simpleSetDouble(int serverPort, int messageCode, double value, ObjectIPC output)
    {
        boolean result = false;
        
        try
        {
            // send the request with our input parm and the optional output parm
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, new DoubleIPC(value), output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = true;
            }
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleSetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }
    
    //--------------------------------------------
    /**
     * Simple "get" that returns a single Boolean or null on failure.  Can
     * have an optional input argument.
     */
    //--------------------------------------------
    public static Boolean simpleGetBoolean(int serverPort, int messageCode, ObjectIPC arg) 
    {
        Boolean result = null;
        
        try
        {
            // create our long output object
            //
            BooleanIPC output = new BooleanIPC();
            
            // send the request
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, arg, output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = output.getValue();
            } 
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleGetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }

    //--------------------------------------------
    /**
     * simple "set" that passes a single Boolean.  If the response has output and
     * 'output' is not null, it will be populated with the response.
     */
    //--------------------------------------------
    public static boolean simpleSetBoolean(int serverPort, int messageCode, boolean value, ObjectIPC output)
    {
        boolean result = false;
        
        try
        {
            // send the request with our input parm and the optional output parm
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, new BooleanIPC(value), output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = true;
            }
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleSetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }
    
    //--------------------------------------------
    /**
     * Simple "get" that returns a single String or null on failure.  Can
     * have an optional input argument.
     */
    //--------------------------------------------
    public static String simpleGetString(int serverPort, int messageCode, ObjectIPC arg) 
    {
        String result = null;
        
        try
        {
            // create our string output object
            //
            StringIPC output = new StringIPC();
            
            // send the request
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, arg, output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = output.getValue();
            } 
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleGetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }

    //--------------------------------------------
    /**
     * simple "set" that passes a single String.  If the response has output and
     * 'output' is not null, it will be populated with the response.
     */
    //--------------------------------------------
    public static boolean simpleSetString(int serverPort, int messageCode, String value, ObjectIPC output)
    {
        boolean result = false;
        
        try
        {
            // send the request with our input parm and the optional output parm
            //
            IPCMessage msg = IPCSender.sendRequest(serverPort, messageCode, new StringIPC(value), output);
            if (msg != null && msg.getReturnCode() == 0)
            {
                result = true;
            }
            else
            {
                log.warn("IPC call of " + messageCode + " returned " + (msg == null ? "null" : msg.getReturnCode()));
            }
        }
        catch (IOException ex)
        {
            log.warn(": error running simpleSetInt messageCode="+messageCode, ex);
        }
        
        return result;
    }
    
    /*
	 * Private Constructor since this utility class shouldn't be instantiated 
	 */
	private IPCHelper() {
	}
}

//+++++++++++
//EOF
//+++++++++++