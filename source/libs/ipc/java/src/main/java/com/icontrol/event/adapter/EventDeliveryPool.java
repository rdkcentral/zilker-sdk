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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import com.icontrol.event.BaseEventListener;
import com.icontrol.event.JavaEvent;
import org.slf4j.Logger;

//-----------------------------------------------------------------------------
// Class definition:    EventDeliveryPool
/**
 *  Thread pool used to deliver events via one of the base event adapter classes.
 *  The idea is to offload the delivering of the events to the listener so that
 *  the adapter can go back to monitoring for new events.  Hopefully this reduces
 *  the amount of time between events so that none are missed.
 *  <p>
 *  Note that this is a static instance so that multiple adapters within
 *  the same process are sharing the same threads, reducing our thread consumption.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
class EventDeliveryPool
{
//    static final int DEFAULT_MULTI_VML_POOL_SIZE  = 2;
//    static final int DEFAULT_SINGLE_VML_POOL_SIZE = 10;
    
    private static EventDeliveryPool singleton = null;
    private ExecutorService pool;
    
    //--------------------------------------------
    /**
     * Return the shared thread pool used for event delivery
     */
    //--------------------------------------------
    static synchronized EventDeliveryPool getSharedInstance()
    {
        if (singleton == null)
        {
            singleton = new EventDeliveryPool();
        }
        return singleton;
    }
    
    //--------------------------------------------
    /**
     * Clones the Listeners List and schedules them to be 
     * notified in a thread from the pool.
     */
    //--------------------------------------------
//    protected void scheduleNotification(BaseNativeEventAdapter adapter, List<BaseEventListener> listenerList, BaseEvent event)
//    {
//        // add to the pool so it will be executed later
//        //
//        pool.execute(new NativeHandler(adapter, listenerList, event));
//    }

    //--------------------------------------------
    /**
     * Clones the Listeners List and schedules them to be 
     * notified in a thread from the pool.
     */
    //--------------------------------------------
    protected void scheduleNotification(BaseJavaEventAdapter adapter, List<BaseEventListener> listenerList, JavaEvent event)
    {
        // add to the pool so it will be executed later
        //
        pool.execute(new JavaHandler(adapter, listenerList, event));
    }

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    private EventDeliveryPool()
    {
        pool = Executors.newCachedThreadPool();
    }
    
    //-----------------------------------------------------------------------------
    // Class definition:    NativeHandler
    /**
     *  Job to execute in the thread pool for delivering events
     *  via a BaseEventAdapter
     **/
    //-----------------------------------------------------------------------------
//    private static class NativeHandler implements Runnable
//    {
//        private BaseNativeEventAdapter       adapter;
//        private BaseEvent                    event;
//        private ArrayList<BaseEventListener> listeners;
//
//        //--------------------------------------------
//        /**
//         *
//         */
//        //--------------------------------------------
//        NativeHandler(BaseNativeEventAdapter adapter, List<BaseEventListener> listenerList, BaseEvent event)
//        {
//            this.adapter = adapter;
//            this.event = event;
//            this.listeners = new ArrayList<BaseEventListener>(listenerList);
//        }
//
//        //--------------------------------------------
//        /**
//         * @see java.lang.Runnable#run()
//         */
//        //--------------------------------------------
//        @Override
//        public void run()
//        {
//            // send to each listener via the adapter
//            //
//            for (int i = 0, count = listeners.size() ; i < count ; i++)
//            {
//                try
//                {
//                    adapter.notifyListener(listeners.get(i), event);
//                }
//                catch(Exception e)
//                {
//                    Logger log = LoggerFactory.getLogger(NativeHandler.class);
//                    log.error("Uncaught exception during event delivery to " + listeners.get(i), e);
//                }
//            }
//        }
//    }
    
    //-----------------------------------------------------------------------------
    // Class definition:    JavaHandler
    /**
     *  Job to execute in the thread pool for delivering events
     *  via a BaseJavaAdapter (JavaEvent vs BaseEvent)
     **/
    //-----------------------------------------------------------------------------
    private static class JavaHandler implements Runnable
    {
        private BaseJavaEventAdapter         adapter;
        private JavaEvent                    event;
        private ArrayList<BaseEventListener> listeners;
        
        //--------------------------------------------
        /**
         * 
         */
        //--------------------------------------------
        JavaHandler(BaseJavaEventAdapter adapter, List<BaseEventListener> listenerList, JavaEvent event)
        {
            this.adapter = adapter;
            this.event = event;
            this.listeners = new ArrayList<BaseEventListener>(listenerList);
        }
        
        //--------------------------------------------
        /**
         * @see java.lang.Runnable#run()
         */
        //--------------------------------------------
        @Override
        public void run()
        {
            // log how many listeners we should send this to.  Handy when debugging the listeners
            // that take a long time to process the events
            //
            Logger log = adapter.getLog();
            if (log != null && event != null)
            {
                log.debug("notify "+listeners.size()+" listeners of event class "+event.getClass().getCanonicalName());
            }
            
            // send to each listener via the adapter
            //
            for (int i = 0, count = listeners.size() ; i < count ; i++)
            {
                try
                {
                    adapter.notifyListener(listeners.get(i), event);
                }
                catch(Exception e)
                {
                    log.error("Uncaught exception during event processing.", e);
                }
            }
        }
    }
}

//+++++++++++
//EOF
//+++++++++++