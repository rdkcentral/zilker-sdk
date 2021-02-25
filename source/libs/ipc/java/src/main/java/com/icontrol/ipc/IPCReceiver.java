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
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.icontrol.util.Task;

//-----------------------------------------------------------------------------
// Class definition:    IPCReceiver
/**
 *  Helper class that is instantiated on the Server that receives IPC (Inter Process Communication) 
 *  requests.  This is a Java-to-Java equivalent of the C-to-C devctl mechanism, however this can
 *  handle requests from both C and Java clients.
 *  <p>
 *  Requests to an instance of IPCReceiver should come from an IPCSender (passing an ObjectIPC).
 *  After decoding the request, the owner of the IPCReceiver would process the request and populate
 *  an IPCMessage with the return information (and possible ObjectIPC with results)
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class IPCReceiver extends Task
{
    private static Logger log = LoggerFactory.getLogger("IPCReceiver");
    
    static final int BACKLOG        = 25;
    static final int PING_REQUEST   = -75;
    static final int PING_RESPONSE  = -74;

    /*
     * stock message codes.  all are negative so we do not
     * conflict with other messages a service may have.
     * NOTE: these must stay in sync with the native code (see ipcStockMessages.h)
     */
//  public static final int STOCK_IPC_SHUTDOWN_SERVICE    = -5;
    public static final int STOCK_IPC_GET_SERVICE_STATUS  = -10;
    public static final int STOCK_IPC_GET_RUNTIME_STATS   = -15;
    public static final int STOCK_IPC_CONFIG_RESTORED     = -20;
    public static final int STOCK_IPC_START_INIT          = -25;

    private int                 portNum;        // the published port number known by the IPCSender
    private ServerSocket        serverSock;
    private IPCReceiverHandler  handler;        // object to send requests off to be handled
    private ExecutorService     pool;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public IPCReceiver(int port)
    {
        // create a thread pool for handling the 
        // requests as the arrive via the socket
        //
        portNum = port;
        pool = Executors.newCachedThreadPool();
        //setAutoRestart(true);
    }

    //--------------------------------------------
    /**
     * @return Returns the port we're listening on.
     */
    //--------------------------------------------
    public int getPortNumber()
    {
        return portNum;
    }

    //--------------------------------------------
    /**
     * @return Returns the asyncHandler.
     */
    //--------------------------------------------
    public IPCReceiverHandler getHandler()
    {
        return handler;
    }

    //--------------------------------------------
    /**
     * Creates a Unix Domain socket using 'name', then starts 
     * a thread to listen on that socket for incoming requests.
     * When a request arrives, it will be forwarded to the
     * the 'handler' so it can be processed.
     */
    //--------------------------------------------
    public void startReceiveThread(IPCReceiverHandler callback)
        throws IOException
    {
        // first see if our thread is already running
        //
        if (isRunning() == true)
        {
            log.debug("Not starting receiver, already have one running");
            return;
        }
        
        // save the callback object
        //
        handler = callback;
        
        // create the listening socket
        //
        InetAddress loop = InetAddress.getByName(IPCSender.LOCAL_LOOPBACK);
        serverSock = new ServerSocket(portNum, BACKLOG, loop);
//        serverSock.setReuseAddress(true);
        
        // start the thread
        //
        super.start();
    }
    
    //--------------------------------------------
    /**
     * Shuts down the socket and thread.  At this point,
     * no more requests can be processed.
     */
    //--------------------------------------------
    public void shutdown()
    {
        // kill the socket
        //
        if (serverSock != null)
        {
            try
            {
                serverSock.close();
            }
            catch (IOException ex) { }
            serverSock = null;
        }
        
        // kill the thread pool
        //
        pool.shutdown();

        // cancel our thread
        //
        super.cancel(true);
    }
    
    //--------------------------------------------
    /**
     * @see com.icontrol.util.Task#start()
     */
    //--------------------------------------------
    @Override
    public synchronized void start()
    {
        // prevent outsiders from running us like a Task
        //
        throw new RuntimeException("don't call this method directly, use the startReceiveThread instead");
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.util.Task#cancel(boolean)
     */
    //--------------------------------------------
    @Override
    public void cancel(boolean waitForStop)
    {
        throw new RuntimeException("don't call this method directly, use the shutdown instead");
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.util.Task#execute()
     */
    //--------------------------------------------
    @Override
    protected void execute()
    {
        try
        {
            // wait for an incoming request to arrive from our server socket
            //
            Socket sock = serverSock.accept();
            if (sock != null)
            {
                // process the request using our thread pool
                //
                pool.execute(new Request(sock));
            }
        }
        catch (IOException ex)
        {
            // if we got an IOException and serverSock is null, we are shutting down
            //
            if(serverSock != null)
            {
                log.warn(portNum+" - error processing request.  bailing.", ex);
                super.cancel(false);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // Class definition:    Request
    /**
     *  Single execution Object to read the incoming request, hand it off to the 
     *  handler, then send back the response.
     **/
    //-----------------------------------------------------------------------------
    private class Request implements Runnable
    {
        private Socket conduit;
        
        //--------------------------------------------
        /**
         * 
         */
        //--------------------------------------------
        Request(Socket client)
        {
            conduit = client;
        }

        //--------------------------------------------
        /**
         * @see java.lang.Runnable#run()
         */
        //--------------------------------------------
        public void run()
        {
            InputStream input = null;
            OutputStream output = null;
            try
            {
                // read the request off of the socket (using an IPCMessage)
                //
                input = conduit.getInputStream();
                IPCMessage msg = IPCMessage.decodeRequest(input);
                if (msg == null)
                {
                    log.warn(portNum+" - nothing read from socket.  Must be a bogus message");
                    return;
                }
                
                // look for a "ping" 
                //
                if (msg.getMessageCode() != PING_REQUEST)
                {
                    // give to the handler
                    //
                    log.debug(portNum+" - decoded request, sending "+msg.getMessageCode()+" to handler");
                    int rc = handler.handleMarshaledRequest(msg);

                    // save the return code.  We're assuming that any additional data that 
                    // needs to be returned will have been set into the IPCMessage container
                    //
                    log.debug(portNum+" - handler complete, returning "+rc);
                    msg.setReturnCode(rc);
                }
                else
                {
                    // just respond with the same code.  The caller is trying to determine if we're alive
                    //
                    log.debug(portNum+" - got PING request, returning PING_RESPONSE");
                    msg.setReturnCode(PING_RESPONSE);
                }
               
                // send back the response
                //
                output = conduit.getOutputStream();
                msg.encodeReply(output);
                output.flush();
                
                // cleanup
                //
                output.close();
                output = null;
                input.close();
                input = null;
                conduit.close();
                conduit = null;
            }
            catch (Exception ex)
            {
                log.warn(portNum+" - error processing request.", ex);
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
                if (conduit != null)
                {
                    try
                    {
                        conduit.close();
                    }
                    catch (IOException whatever) { }
                }
            }
        }
    }
}

//+++++++++++
//EOF
//+++++++++++