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

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ConnectException;
import java.net.InetAddress;
import java.net.Socket;

import com.icontrol.ipc.impl.ObjectIPC;
import com.icontrol.registry.ServiceHandle;
import com.icontrol.registry.ServiceRegistry;
import com.icontrol.registry.scope.RemoteHostServiceHandle;
import com.icontrol.registry.scope.SameProcessServiceHandle;
import com.icontrol.util.StringUtils;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

//-----------------------------------------------------------------------------
// Class definition:    IPCSender
/**
 *  Helper class that is instantiated on the Client that wishes to send IPC (Inter Process Communication) 
 *  requests to a Server.  This is a Java-to-Java equivalent of the C-to-C devctl mechanism.
 *  <p>
 *  Requests are sent to an instance of IPCReceiver (via a socket) and will wait for a reply.
 *  After decoding the request, the owner of the IPCReceiver would process the request and return
 *  the response code (along with an optional results).
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class IPCSender
{
    public static final int DEFAULT_READ_TIMEOUT_SECS = 10;     // match ipcSender.c
    public static final String  LOCAL_LOOPBACK = "127.0.0.1";
    private static Logger log = LoggerFactory.getLogger("IPCSender");
    
    //--------------------------------------------
    /**
     * Send request to an IPCReceiver in another process.
     * 
     * @param serverPort - The server's port number
     * @param messageCode - Signify the request ID (ex: 2 --> startGPRS)
     * @param inputParm - Parameters required for this particular messageCode.  Can be null.
     * @param outputParm - Object to decode the response message.  Can be null.
     * 
     * @return The IPCMessage object with the request code, response code, and response payload (needs to be decoded)
     */
    //--------------------------------------------
    public static IPCMessage sendRequest(int serverPort, int messageCode, ObjectIPC inputParm, ObjectIPC outputParm)
        throws IOException
    {
        return sendRequest(serverPort, messageCode, inputParm, outputParm, DEFAULT_READ_TIMEOUT_SECS);
    }
    
    //--------------------------------------------
    /**
     * Send request to an IPCReceiver in another process.
     * 
     * @param serverPort - The server's port number
     * @param messageCode - Signify the request ID (ex: 2 --> startGPRS)
     * @param inputParm - Parameters required for this particular messageCode.  Can be null.
     * @param outputParm - Object to decode the response message.  Can be null.
     * @param readTimeoutSecs - amount of timeout to impose on reading the reply.  0 means no timeout
     * 
     * @return The IPCMessage object with the request code, response code, and response payload (needs to be decoded)
     */
    //--------------------------------------------
    public static IPCMessage sendRequest(int serverPort, int messageCode, ObjectIPC inputParm, ObjectIPC outputParm, int readTimeoutSecs)
        throws IOException
    {
        Socket socket = null;
        InputStream input = null;
        OutputStream output = null;
        InetAddress targetHost = null;
        
        // see if the target service is in our VM and can be hit directly
        //
        ServiceHandle service = ServiceRegistry.getServiceForIpcPort(serverPort);
        if (service != null)
        {
            // see if same VM or not
            //
            if (service.isSameProcess() == true)
            {
                // get the IPCReceiverHandler and message it directly
                //
                SameProcessServiceHandle local = (SameProcessServiceHandle)service;
                IPCReceiverHandler direct = local.getIpcHandler();
                if (direct != null)
                {
                    // make the call
                    //
                    log.debug("Sending request directly (no marshal).  port="+serverPort+" msgCode="+messageCode);
                    int rc = direct.handleDirectRequest(messageCode, inputParm, outputParm);
                    
                    // return response in IPCMessage wrapper
                    //
                    IPCMessage retVal = new IPCMessage();
                    retVal.setReturnCode(rc);
                    return retVal;
                }
                else
                {
                    log.warn("Service port="+serverPort+" is in the same VM, but has no direct handle.  Unable to send the requesst");
                    return null;
                }
            }
            else if (service.isRemoteHost() == true)
            {
                // get the remote IP address of the service, and use that instead of LOCAL_LOOPBACK
                //
                RemoteHostServiceHandle remote = (RemoteHostServiceHandle)service;
                try
                {
                    log.debug("Using remote host address "+remote.getRemoteIp());
                    targetHost = InetAddress.getByName(remote.getRemoteIp());
                }
                catch (Exception ex)
                {
                    log.warn("Error getting InetAddress for remote service port="+serverPort+" msgCode="+messageCode+" ip="+remote.getRemoteIp(), ex);
                    return null;
                }
            }

            // not same VM, so fall below and marshal the request and send over TCP to local host
        }
        
        try
        {
            // if we don't know the host, assume our local loopback interface
            //
            if (targetHost == null)
            {
//                log.debug("Using local host address "+LOCAL_LOOPBACK);
                targetHost = InetAddress.getByName(LOCAL_LOOPBACK);
            }
            
            // connect to the service.  we'll try this up to 5 times in
            // case the service isn't quite ready to receive requests
            //
            socket = connect(targetHost, serverPort);
            if (socket == null)
            {
                log.warn("Error sending request.  Service not responding.  port="+serverPort+" msgCode="+messageCode);
                return null;
            }
            
            // build up the request message, then send it
            //
            output = socket.getOutputStream();
            IPCMessage msg = new IPCMessage();
            msg.setMessageCode(messageCode);
            msg.setPayload(encodePayload(inputParm));
            msg.encodeRequest(output);
            output.flush();
            
            // if 'timeout', enforce read timeout at this point so we don't block forever
            //
            if (readTimeoutSecs > 0)
            {
                socket.setSoTimeout(readTimeoutSecs * 1000);
            }
            
            // read back the result and decode it
            //
            input = socket.getInputStream();
            IPCMessage retVal = IPCMessage.decodeReply(input);
            decodePayload(outputParm, retVal.getPayload());
            
            // cleanup
            //
            output.close();
            output = null;
            input.close();
            input = null;
            socket.close();
            socket = null;
            
            return retVal;
        }
        catch (EOFException ex)
        {
            log.warn("Error receiving response with EOF.  It is possible the service exited; port="+serverPort+" msgCode="+messageCode, ex);
        }
        catch (IOException ex)
        {
            log.warn("Error sending request port="+serverPort+" msgCode="+messageCode, ex);
        }
        finally
        {
            if (output != null)
            {
                try
                {
                    output.close();
                }
                catch (IOException whatever) { }
            }
            if (input != null)
            {
                try
                {
                    input.close();
                }
                catch (IOException whatever) { }
            }
            if (socket != null)
            {
                try
                {
                    socket.close();
                }
                catch (IOException whatever) { }
            }
        }
        
        return null;
    }
    
    //--------------------------------------------
    /**
     *  Attempts the open the service port.  Returns true
     *  if the port is responding.
     */
    //--------------------------------------------
    public static boolean isServiceAvailable(int serverPort)
    {
        boolean retVal = false;
        OutputStream output = null;
        InputStream input = null;
        Socket socket = null;
        InetAddress targetHost = null;
        
        // see if the target service is in our VM and can be hit directly
        //
        ServiceHandle service = ServiceRegistry.getServiceForIpcPort(serverPort);
        if (service != null)
        {
            // see if same VM or not
            //
            if (service.isSameProcess() == true)
            {
                // it's registered, so should be available
                //
                log.debug("service port="+serverPort+" is local to our VM, so is available");
                return true;
            }
            else if (service.isRemoteHost() == true)
            {
                // get the remote IP address of the service, and use that instead of LOCAL_LOOPBACK
                //
                RemoteHostServiceHandle remote = (RemoteHostServiceHandle)service;
                try
                {
                    targetHost = InetAddress.getByName(remote.getRemoteIp());
                }
                catch (Exception ex)
                {
                    log.warn("Error getting InetAddress for remote service port="+serverPort+" ip="+remote.getRemoteIp(), ex);
                    return false;
                }
            }

            // not same VM, so fall below and marshal the request and send over TCP to local host
        }
        
        try
        {
            // if we don't know the host, assume our local loopback interface
            //
            if (targetHost == null)
            {
                targetHost = InetAddress.getByName(LOCAL_LOOPBACK);
            }
            log.debug("testing if Service is alive.  port="+serverPort);
            
            // connect to the service.  we'll try this up to 5 times in
            // case the service isn't quite ready to receive requests
            //
            socket = connect(targetHost, serverPort);
            if (socket == null)
            {
                log.warn("Service not responding.  port="+serverPort);
                return false;
            }
            
            // socket there, so send a ping message to be sure
            //
            output = socket.getOutputStream();
            IPCMessage msg = new IPCMessage();
            msg.setMessageCode(IPCReceiver.PING_REQUEST);
            msg.encodeRequest(output);
            output.flush();
            
            // read back the result and decode it
            //
            input = socket.getInputStream();
            IPCMessage response = IPCMessage.decodeReply(input);
            if (response.getReturnCode() == IPCReceiver.PING_RESPONSE)
            {
                log.debug("service responded to PING, returning true");
                retVal = true;
            }
            
            // cleanup
            //
            output.close();
            output = null;
            input.close();
            input = null;
            socket.close();
            socket = null;
        }
        catch (IOException ex)
        {
            log.warn("Error accessing service port="+serverPort, ex);
        }
        finally
        {
            if (socket != null)
            {
                try
                {
                    socket.close();
                }
                catch (IOException whatever) { }
            }
        }
        
        return retVal;
    }
    
    //--------------------------------------------
    /**
     * Wait for the Java Service on the local host to start responding.
     * Same as calling "isServiceAvailable" over and over until success.
     * After 'timeoutSecs' seconds, this will give up.
     */
    //--------------------------------------------
    public static boolean waitForServiceAvailable(int serverPort, int timeoutSecs)
    {
        int count = 0;
        
        while (true)
        {
            // see if responding
            //
            if (isServiceAvailable(serverPort) == true)
            {
                // good to go
                //
                return true;
            }
            
            // not there yet, sleep for a second
            //
            try
            {
                Thread.sleep(1000);
            }
            catch (InterruptedException ex)
            {
                return false;
            }
            
            // see if bail
            //
            if (timeoutSecs > 0)
            {
                count++;
                if (count > timeoutSecs)
                {
                    break;
                }
            }
        }
        
        return false;
    }
 
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    private static Socket connect(InetAddress loop, int serverPort)
        throws IOException
    {
        Socket socket = null;
        
        // connect to the service.  we'll try this up to 5 times in
        // case the service isn't quite ready to receive requests
        //
        for (int i = 0 ; i < 5 ; i++)
        {
            try
            {
                // create the socket
                //
                socket = new Socket(loop, serverPort);
                
                // worked so bail
                //
                break;
            }
            catch (ConnectException notThere)
            {
                log.warn("Error connecting to port "+serverPort+" (attempt "+(i+1)+" of 5)");
                try
                {
                    Thread.sleep(1000);
                }
                catch (InterruptedException whocares) { }
            }
        }
        
        return socket;
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    private static String encodePayload(ObjectIPC parm) throws IOException
    {
        if (parm == null)
        {
            return null;
        }
        
        try
        {
            JSONObject json = parm.toJSON();
            if (json != null)
            {
                return json.toString();
            }
        }
        catch (JSONException ex)
        {
            // error encoding the object
            //
            throw new IOException(ex);
        }

        return null;
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    private static void decodePayload(ObjectIPC parm, String payload) throws IOException
    {
        if (parm == null)
        {
            return;
        }
        
        if (StringUtils.isEmpty(payload) == false)
        {
            try
            {
                JSONObject buffer = (JSONObject) new JSONTokener(payload).nextValue();
                parm.fromJSON(buffer);
            }
            catch (JSONException ex)
            {
                // error decoding the object
            	//
            	throw new IOException(ex);
            }
        }
    }

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    private IPCSender()
    {
    }
}

//+++++++++++
//EOF
//+++++++++++