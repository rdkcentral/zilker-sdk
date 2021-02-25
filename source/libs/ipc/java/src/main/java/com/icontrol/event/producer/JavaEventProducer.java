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

package com.icontrol.event.producer;

//-----------
//imports
//-----------

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.MulticastSocket;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import com.icontrol.event.BaseEvent;
import com.icontrol.event.JavaEvent;
import com.icontrol.event.adapter.BaseJavaEventAdapter;
import com.icontrol.ipc.IPCSender;
import org.json.JSONException;
import org.json.JSONObject;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

//-----------------------------------------------------------------------------
// Class definition:    JavaEventProducer
/**
 *  Broadcast a JavaEvent object to any listeners (via BaseJavaEventAdapter).  
 *  Uses the ObjectIPC interface of the JavaEvent to marshal the object into JSON, 
 *  then will multicast those bits to any Listeners.
 *  <p>
 *  Counterpart to BaseJavaEventAdapter, which is used to read and decode the events
 *  @see com.icontrol.adapter.BaseJavaEventAdpter
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class JavaEventProducer
{
    // UDP port used when broadcasting/receiving events over the local loopback
    // interface.  this matches the one used by the native implementation to allow
    // Java and C clients/services to send/receive events
    //
    public static final int EVENT_BROADCAST_PORT = 12575;

    private Logger log = LoggerFactory.getLogger("JavaEventProducer");
    
    private MulticastSocket socket;
    private InetAddress     groupAddress;
    private int             serviceId;
    private List<BaseJavaEventAdapter>  innerProcessListeners;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public JavaEventProducer(int serviceIdNum) throws IOException
    {
        InetAddress loop = InetAddress.getByName(IPCSender.LOCAL_LOOPBACK);

        // create the UDP socket to broadcast with, forcing it to use the local-loopback network interface
        //
        serviceId = serviceIdNum;
        socket = new MulticastSocket();
        socket.setInterface(loop);
        socket.setLoopbackMode(true);
        socket.setBroadcast(true);
        innerProcessListeners = new ArrayList<BaseJavaEventAdapter>();
        
        // get the "group" using the name defined in JavaEvent.  This way the
        // receivers can filter out what they see and restrict their view to our events
        //
        groupAddress = InetAddress.getByName(JavaEvent.IC_EVENT_MULTICAST_GROUP);
    }
    
    //--------------------------------------------
    /**
     * Closes the ports/sockets
     */
    //--------------------------------------------
    public void cancel()
    {
        socket.close();
        socket = null;
        groupAddress = null;
    }

    //--------------------------------------------
    /**
     * Register a inner-process event adapter to be notified
     * before we broadcast the event to other processes.
     * Used when the event listener and producer are in the same process
     * to short-circuit the need for marshal/unmarshaling
     */
    //--------------------------------------------
    public synchronized void addEventAdapterListener(BaseJavaEventAdapter listener)
    {
        // add the listener to our notification array
        //
        if (innerProcessListeners.contains(listener) == false)
        {
            innerProcessListeners.add(listener);
        }
    }
    
    //--------------------------------------------
    /**
     * Un-Register a inner-process event adapter from the notification list
     */
    //--------------------------------------------
    public synchronized void removeEvenAdaptertListener(BaseJavaEventAdapter listener)
    {
        innerProcessListeners.remove(listener);
    }

    //--------------------------------------------
    /**
     * Similar to the native implementation, will broadcast the supplied
     * event to any multicast listeners. Uses the event code & value to identify
     * the event contents so that listeners know how to parse the stream.
     *  @param eventCode - the 'classification' of this JavaEvent falls under (ex: network, camera)
     * @param eventValue - the key to stuff the event under in the payload (i.e. event_extra)
     * @param event - the JavaEvent (a BaseEvent implementing Parceable)
     */
    //--------------------------------------------
    public void broadcastEvent(int eventCode, int eventValue, JavaEvent event) throws IOException
    {
        // send to internal listeners first
        //
        synchronized (this)
        {
            Iterator<BaseJavaEventAdapter> loop = innerProcessListeners.iterator();
            while (loop.hasNext() == true)
            {
                // see if this adapter wants to receive this event
                //
                BaseJavaEventAdapter next = loop.next();
                if (next.supportsEvent(eventCode, eventValue) == true)
                {
                    next.notifyAllListeners(event);
                }
            }
        }

        // marshall event + codes into a byte array
        //
        byte[] data = marshalEvent(event);
        
        // add to a Datagram packet and deliver to listeners.  note that this
        // uses the shared UDP port (just like the native implementation)
        //
        log.debug("broadcasting event code="+eventCode+" value="+eventValue+" size="+data.length);
        DatagramPacket packet = new DatagramPacket(data, data.length, groupAddress, EVENT_BROADCAST_PORT);
        socket.send(packet);
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    private byte[] marshalEvent(JavaEvent event)
    {
        String encodedEvent = "";
        try
        {
            // marshal the event (JSON)
            //
            JSONObject json = event.toJSON();
            if (json != null)
            {
                // inject this 'serviceId' into that JSON structure so
                // that the receivers can determine where this came from.
                // this matches the behavior of our native codebase
                //
                BaseEvent.insertServiceIdToRawEvent(json, serviceId);

                // get string representation of the event
                //
                encodedEvent = json.toString();
            }
        }
        catch (JSONException ex)
        {
            log.warn("error encoding "+event+" to JSON - "+ex.getMessage(), ex);
        }

        // convert from string to bytes, using the default charset
        //
        return encodedEvent.getBytes(Charset.defaultCharset());
    }
}

//+++++++++++
//EOF
//+++++++++++