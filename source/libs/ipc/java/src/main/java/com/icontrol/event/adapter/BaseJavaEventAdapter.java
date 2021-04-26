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

package com.icontrol.event.adapter;

//-----------
//imports
//-----------

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.MulticastSocket;
import java.net.NetworkInterface;
import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicBoolean;

import com.icontrol.event.BaseEvent;
import com.icontrol.event.BaseEventListener;
import com.icontrol.event.JavaEvent;
import com.icontrol.event.producer.JavaEventProducer;
import com.icontrol.ipc.IPCSender;
import com.icontrol.registry.ServiceHandle;
import com.icontrol.registry.ServiceRegistry;
import com.icontrol.registry.scope.SameProcessServiceHandle;
import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONTokener;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

//-----------------------------------------------------------------------------
// Class definition:    BaseJavaEventAdapter
/**
 *  Adapter to listen on a predetermined UDP port for events that are broadcasted.
 *  When activity on the port is recognized, the event is un-marshalled and delivered
 *  to Listener objects.  This class can handle Java and Native services that broadcast.
 *  <p>
 *  Follows the convention of:
 *  <ol>
 *    <li> create and register for UDP broadcasts.
 *    <li> as events are received, decode and send it to listeners
 *  </ol>
 *  <p>
 *  <b>NOTE</b>: Listeners that receive events will get them on the background thread <b>NOT the Main/UI thread</b>.
 *  Also note that when the events are received, no additional event processing can occur until
 *  the handleXXX() method returns (i.e. don't spend too much time looking at the events).
 *  <p>
 *  Counterpart to JavaEventProducer, which is used to encode and broadcast the events
 *  @see com.icontrol.event.producer.JavaEventProducer
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public abstract class BaseJavaEventAdapter
{
    /*
     * may be short-sided, but set the MAX 'JSON string' to 64k since native
     * code sends Strings as "short + char *" - meaning maximum of 64k messages
     */
    public static final int MAX_PACKET_SIZE = (64 * 1024);

    // static listener (one per process) and the 'serviceId'
    // to 'adapter' mapping (for forwarding when events arrive)
    //
    private static EventReaderThread readThread = null;
    private static Map<Integer,ServiceAdapterSet> adapterMap = null;

    // list of BaseEventListener objects to notify
    //
    private ArrayList<BaseEventListener> listeners;
    private JavaEventProducer            directProducer;
    private boolean                      registered;
    private int                          serviceId;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public BaseJavaEventAdapter(int serviceIdNum) throws IOException
    {
        // ensure our static environment is setup
        //
        synchronized(BaseJavaEventAdapter.class)
        {
            if (readThread == null)
            {
                readThread = new EventReaderThread();
            }
        }

        // see if the target service is in our VM and can be hit directly
        //
        serviceId = serviceIdNum;
        ServiceHandle service = ServiceRegistry.getServiceForEventPort(serviceIdNum);
        if (service != null)
        {
            // see if same VM or not
            //
            if (service.isSameProcess() == true)
            {
                // get the JavaEventProducer so we can add ourself as a listener directly
                //
                SameProcessServiceHandle local = (SameProcessServiceHandle)service;
                if ((directProducer = local.getEventProducer()) != null)
                {
                    // register to get direct notification of events
                    // (it calls our supportsEvents() and notifyAllListeners() methods)
                    //
                    Logger log = getLog();
                    if (log != null)
                    {
                        log.debug("add ourself directly into event producer notifications");
                    }
                    directProducer.addEventAdapterListener(this);
                }
            }
            // TODO: remote host event registration?
        }
        
        // create our listener list
        //
        listeners = new ArrayList<BaseEventListener>();
        
        // if not directly getting invoked, register with
        // the reader thread for this service id
        //
        if (directProducer == null)
        {
            readThread.addAddapterToMap(this, serviceId);
        }
    }
    
    //--------------------------------------------
    /**
     * Register a listener to the list
     */
    //--------------------------------------------
    protected synchronized void addBaseEventListener(BaseEventListener listener)
    {
        if (listener == null)
        {
            return;
        }

        // add the listener to our notification array
        //
        Logger log = getLog();
        if (log != null)
        {
            log.debug("attempting to add listener " + listener.getClass());
        }
        if (listeners.contains(listener) == false)
        {
            listeners.add(listener);
            if (log != null)
            {
                log.debug("added listener " + listener.getClass());
            }
        }
    }
    
    //--------------------------------------------
    /**
     * Un-Register a listener from the list
     */
    //--------------------------------------------
    public synchronized void removeEventListener(BaseEventListener listener)
    {
        Logger log = getLog();
        if (log != null)
        {
            log.debug("removing listener "+listener.getClass());
        }
        listeners.remove(listener);
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    protected synchronized int listenerCount()
    {
        if (listeners != null)
        {
            return listeners.size();
        }
        return 0;
    }
    
    //--------------------------------------------
    /**
     * Called by EventReaderThread when a message arrives
     * that matches our serviceId.
     */
    //--------------------------------------------
    private void processIncomingEvent(JSONObject payload)
    {
        // extract the minimal amount of information so we can determine
        // what to do with this event (process or pitch).  This also allows
        // the subclass to know how to parse the message
        //
        int eventCode = payload.optInt(BaseEvent.EVENT_CODE_JSON_KEY);
        int eventValue = payload.optInt(BaseEvent.EVENT_VALUE_JSON_KEY);

        // see if subclass wants to parse this (to reduce decoding if uninterested)
        //
        if (supportsEvent(eventCode, eventValue) == true)
        {
            // send to subclass to parse
            //
            JavaEvent event = decodeEvent(eventCode, eventValue, payload);
            if (event != null)
            {
                Logger log = getLog();
                if (log != null)
                {
                    log.debug("decoded event, code="+eventCode+" value="+eventValue+"; sending to listeners");
                }
                notifyAllListeners(event);
            }
        }
    }

//    //--------------------------------------------
//    /**
//     * Shutdown the thread
//     */
//    //--------------------------------------------
//    public synchronized void cancelEvents()
//    {
//        if (job != null)
//        {
//            getLog().debug("un-registering broadcast receiver (shutting down)");
//            registered = false;
//            try
//            {
//                socket.leaveGroup(groupAddress);
//                socket.disconnect();
//                job.cancel();
//            }
//            catch (Exception e)
//            {
//                getLog().warn("unable to unregister broadcast receiver: "+e.getMessage());
//            }
//            job = null;
//        }
//        listeners.clear();
//        if (directProducer != null)
//        {
//            directProducer.removeEvenAdaptertListener(this);
//            directProducer = null;
//        }
//        finishCanceling();
//    }
    
    //--------------------------------------------
    /**
     * Return the Log to use for this adapter
     */
    //--------------------------------------------
    protected abstract Logger getLog();

    //--------------------------------------------
    /**
     * Called by our Thread OR a JavaEventProducer when running in the same process
     * Subclass should return true if it wants to process this event code/value.
     */
    //--------------------------------------------
    public abstract boolean supportsEvent(int eventCode, int eventValue);
    
    //--------------------------------------------
    /**
     * Called by our Thread when an Event was received.
     * It is up to the subclass to parse the JSON message
     * and create a BaseEvent implementation
     */
    //--------------------------------------------
    protected abstract JavaEvent decodeEvent(int eventCode, int eventValue, JSONObject payload);
    
    //--------------------------------------------
    /**
     * Called by our Thread when an event was received and created
     * and needs to be type-casted and sent to the listener
     */
    //--------------------------------------------
    protected abstract void notifyListener(BaseEventListener listener, JavaEvent event);
    
    //--------------------------------------------
    /**
     * Allow subclasses to perform their own cleanup
     */
    //--------------------------------------------
    protected void finishCanceling()
    {
    }
    
    //--------------------------------------------
    /**
     * Called internally by our Thread (or from the JavaEventProducer)
     * to inform all of our listeners of a received event
     */
    //--------------------------------------------
    public synchronized void notifyAllListeners(JavaEvent event)
    {
        if (event == null)
        {
            return;
        }

        // send event to our listeners via the DeliverPool
        // this way we don't waste time waiting on listeners to process
        // the event and allows this thread to get back to monitoring
        // for additional events (reduce lag between them and hopefully
        // not miss any)
        //
        EventDeliveryPool.getSharedInstance().scheduleNotification(this, listeners, event);
    }

    //-----------------------------------------------------------------------------
    // Class definition:    ServiceAdapterSet
    /**
     *  List of BaseJavaEventAdapter instances grouped for a single service.
     *  Necessary to support services that define "multiple adapters" for the
     *  same service identifier (ex: power, battery, tamper)
     **/
    //-----------------------------------------------------------------------------
    private static class ServiceAdapterSet extends CopyOnWriteArrayList<BaseJavaEventAdapter>
    {
        //--------------------------------------------
        /**
         * Called by EventReaderThread when a message arrives
         * that matches our serviceId.
         */
        //--------------------------------------------
        void processIncomingEvent(JSONObject payload)
        {
            // pass along to all adapters we contain
            //
            Iterator<BaseJavaEventAdapter> loop = iterator();
            while (loop.hasNext() == true)
            {
                loop.next().processIncomingEvent(payload);
            }
        }
    }

    //-----------------------------------------------------------------------------
    // Class definition:    EventReaderThread
    /**
     *  Single instance per process that will listen on the public
     *  event port.  When messages arrive, it will convert the bytes
     *  to JSON so it can determine which adapter to forward the message to
     **/
    //-----------------------------------------------------------------------------
    private static class EventReaderThread implements Runnable
    {
        private Logger log = LoggerFactory.getLogger("EventReaderThread");
        private MulticastSocket socket;
        private InetAddress     groupAddress;
        private AtomicBoolean   running;

        //--------------------------------------------
        /**
         *
         */
        //--------------------------------------------
        private EventReaderThread() throws IOException
        {
            // create the static hash to keep the serviceId -> adapter mapping
            //
            adapterMap = new ConcurrentHashMap<Integer,ServiceAdapterSet>();
            running = new AtomicBoolean(true);

            // create our UDP socket to listen on, forcing it to use the local-loopback
            //
            InetAddress loop = InetAddress.getByName(IPCSender.LOCAL_LOOPBACK);
            socket = new MulticastSocket(JavaEventProducer.EVENT_BROADCAST_PORT);
            socket.setInterface(loop);

            // use the same group address that the events should be broadcast on
            // note that when we join, we have to specify both the group address, port, and network interface
            // if we don't use all 3, then we cannot guarantee the events will be received
            //
            groupAddress = InetAddress.getByName(JavaEvent.IC_EVENT_MULTICAST_GROUP);
            NetworkInterface netIf = NetworkInterface.getByInetAddress(loop);
            socket.joinGroup(new InetSocketAddress(groupAddress, JavaEventProducer.EVENT_BROADCAST_PORT), netIf);
            log.debug("created socket to receive external events");

            // start our thread
            //
            Thread t = new Thread(this);
            t.setDaemon(true);
            t.start();
        }

        //--------------------------------------------
        /**
         * Shutdown the reader thread
         */
        //--------------------------------------------
        void cancel()
        {
            running.set(false);
            try
            {
                socket.leaveGroup(groupAddress);
                socket.disconnect();
            }
            catch (IOException ex)
            {
                log.warn("unable to unregister broadcast receiver: "+ex.getMessage(), ex);
            }
        }

        //--------------------------------------------
        /**
         * Adds a BaseJavaEventAdapter to the mapping of serviceId --> adapter
         */
        //--------------------------------------------
        synchronized void addAddapterToMap(BaseJavaEventAdapter adapter, int serviceId)
        {
            // see if we have a ServiceAdapterSet for this service id
            //
            ServiceAdapterSet group = adapterMap.get(serviceId);
            if (group == null)
            {
                group = new ServiceAdapterSet();
                adapterMap.put(serviceId, group);
            }

            // add this adapter to the group
            //
            group.add(adapter);
        }

        //--------------------------------------------
        /**
         * @see java.lang.Runnable#run()
         */
        //--------------------------------------------
        @Override
        public void run()
        {
            // create a buffer that 'should' be large enough
            //
            byte[] buffer = new byte[MAX_PACKET_SIZE];
            DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
            packet.setLength(MAX_PACKET_SIZE);
            
            log.info("receiveThread starting up.");

            // loop as long as we're enabled
            //
            while(running.get() == true)
            {
                try 
                {
                    // wait for an event to arrive
                    //
                    socket.receive(packet);
                    int len = packet.getLength();
                    if (len == 0)
                    {
                        log.warn("got EMPTY event datagram packet, skipping");
                        packet.setLength(MAX_PACKET_SIZE);
                        continue;
                    }

                    // convert from bytes to string
                    //
                    String encodedEvent = new String(packet.getData(), 0, len, "UTF-8");
                    if (encodedEvent != null)
                    {
                        // parse as JSON
                        //
                        JSONObject jsonObject = (JSONObject) new JSONTokener(encodedEvent).nextValue();

                        // extract the service this came from
                        //
                        int serviceId = BaseEvent.extractServiceIdFromRawEvent(jsonObject);

                        // locate the BaseJavaAdapter associated with this serviceId
                        //
                        ServiceAdapterSet adapterSet = adapterMap.get(serviceId);
                        if (adapterSet != null)
                        {
                            // forward on to the adapters for decoding and handling
                            //
                            adapterSet.processIncomingEvent(jsonObject);
                        }
                    }

                    // reset length for next message
                    //
                    packet.setLength(MAX_PACKET_SIZE);
                }
                catch (IOException ex)
                {
                    log.warn("error processing event", ex);
                }
                catch (JSONException ex)
                {
                    log.warn("error processing event", ex);
                }
            }
        }
    }
}

//+++++++++++
//EOF
//+++++++++++