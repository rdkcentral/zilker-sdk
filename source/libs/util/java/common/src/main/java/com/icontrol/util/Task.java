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

package com.icontrol.util;

//-----------
//imports
//-----------

import java.util.concurrent.atomic.AtomicBoolean;

import org.slf4j.LoggerFactory;

//-----------------------------------------------------------------------------
// Class definition:    Task
/**
 *  Runnable object to execute in a Thread until told to cancel (or it stops on it's own).
 *  This is similar to the FutureTask, but is not really a run 1 time and produce a result.
 *  It's more of a run for a long time to deal with many situations.
 *  <p>
 *  For each execution (run method), the Task will follow these steps:
 *  <pre>
 *    onStart()
 *    while (keepLooping() == true)
 *        execute()
 *    onStop()
 *  </ul>
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public abstract class Task implements Runnable
{
    protected AtomicBoolean   isRunning;
    protected AtomicBoolean   doCancel;
    protected Thread          thread;
    protected boolean         autoRestart;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public Task()
    {
        isRunning = new AtomicBoolean(false);
        doCancel = new AtomicBoolean(false);
    }

    //--------------------------------------------
    /**
     * @return Returns if this Task is currently running.
     */
    //--------------------------------------------
    public boolean isRunning()
    {
        return isRunning.get();
    }

    //--------------------------------------------
    /**
     * @return Returns if the Task has been told to cancel
     */
    //--------------------------------------------
    public boolean isCanceled()
    {
        return doCancel.get();
    }
    
    //--------------------------------------------
    /**
     * Starts the thread
     */
    //--------------------------------------------
    public synchronized void start()
    {
        // make sure we're not already running
        //
        if (isRunning() == true)
        {
            return;
        }
        
        // reset flags and start the thread
        //
        doCancel.set(false);
        isRunning.set(true);
        thread = new Thread(this, this.getClass().getSimpleName());
        thread.start();
    }
    
    //--------------------------------------------
    /**
     * Attempts to cancel the Task by setting the doCancel flag
     * If 'waitForStop' is true, this won't return until the thread
     * has stopped running.
     */
    //--------------------------------------------
    public void cancel(boolean waitForStop)
    {
        // set the atomic boolean
        //
        doCancel.set(true);
        
        // let subclass know we're canceling
        //
        onCancel();
        
        if (waitForStop == true)
        {
            // don't return until the thread stops
            //
            while (isRunning.get() == true)
            {
                synchronized(this)
            	{
                    try
                    {
                        // wait for the notifyAll from run(), but with a relatively short
                        // timeout in case we missed it
                        //
                        this.wait(500);
                    }
                    catch (InterruptedException whatever) { }
                }
            }
        }
    }

    //--------------------------------------------
    /**
     * @return Returns the autoRestart.
     */
    //--------------------------------------------
    public boolean shouldAutoRestart()
    {
        return autoRestart;
    }

    //--------------------------------------------
    /**
     * @param flag The autoRestart to set.
     */
    //--------------------------------------------
    protected void setAutoRestart(boolean flag)
    {
        this.autoRestart = flag;
    }

    //--------------------------------------------
    /**
     * @see java.lang.Runnable#run()
     */
    //--------------------------------------------
    public void run()
    {
        // let subclass perform initalization
        //
        onStart();
        
        // loop until told to cancel
        //
        while (keepLooping() == true)
        {
            try
            {
                execute();
            }
            catch (Exception ex)
            {
                LoggerFactory.getLogger(Task.class).warn(ex.getMessage(), ex);
            }
        }
        
        // out of the loop, so turn off flag
        //
        onStop();

        // notify cancel()  or any other threads that we're done with the task
        //
        synchronized(this)
        {
            isRunning.set(false);
            thread = null;
            this.notifyAll();
        }
        
        // start our thread again if told to auto-restart
        //
        if (autoRestart == true)
        {
            onRestart();
            start();
        }
    }
    
    //--------------------------------------------
    /**
     * Called in run to evaluate if we continue to loop
     */
    //--------------------------------------------
    protected boolean keepLooping()
    {
        return (doCancel.get() == false);
    }
    
    //--------------------------------------------
    /**
     * Optional subclass hook.  Called at the beginning
     * of the thread, before the loop.
     */
    //--------------------------------------------
    protected void onStart()
    {
    }
    
    //--------------------------------------------
    /**
     * Optional subclass hook.  Called just before
     * the thread is restarted.
     */
    //--------------------------------------------
    protected void onRestart()
    {
    }
    
    //--------------------------------------------
    /**
     * Optional subclass hook.  Called at the end
     * of the thread, (after the loop stopped).
     */
    //--------------------------------------------
    protected void onStop()
    {
    }
    
    //--------------------------------------------
    /**
     * Optional subclass hook.  Called at the beginning
     * of "cancel()" (after setting doCancel = true). 
     */
    //--------------------------------------------
    protected void onCancel()
    {
    }

    //--------------------------------------------
    /**
     * Implemented by subclass to perform the execution.
     * Called from the Thread.run if keepLooping == true.
     * This will be called multiple times.
     */
    //--------------------------------------------
    protected abstract void execute();

}

//+++++++++++
//EOF
//+++++++++++