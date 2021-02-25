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

package com.icontrol.generate.service.context;

//-----------
//imports
//-----------

import java.io.File;

//-----------------------------------------------------------------------------
// Class definition:    Context
/**
 *  Context of information about the current operation.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class Context
{
    private boolean     debug;
    private int         eventPortNum;
    private int         ipcPortNum;
    private String      serviceName;          // service group name
    private PathValues  paths;
    private File        baseOutputDir;
    private Tracker     genTracker;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public Context()
    {
        genTracker = new Tracker();
    }

    //--------------------------------------------
    /**
     * @return Returns the debug.
     */
    //--------------------------------------------
    public boolean isDebug()
    {
        return debug;
    }

    //--------------------------------------------
    /**
     * @param flag The debug to set.
     */
    //--------------------------------------------
    public void setDebug(boolean flag)
    {
        this.debug = flag;
    }

    //--------------------------------------------
    /**
     * @return Returns the serviceName.
     */
    //--------------------------------------------
    public String getServiceName()
    {
        return serviceName;
    }

    //--------------------------------------------
    /**
     * @param name The serviceName to set.
     */
    //--------------------------------------------
    public void setServiceName(String name)
    {
        this.serviceName = name;
    }

    //--------------------------------------------
    /**
     * @return Returns the eventPortNum.
     */
    //--------------------------------------------
    public int getEventPortNum()
    {
        return eventPortNum;
    }

    //--------------------------------------------
    /**
     * @param port The eventPortNum to set.
     */
    //--------------------------------------------
    public void setEventPortNum(int port)
    {
        this.eventPortNum = port;
    }

    //--------------------------------------------
    /**
     * @return Returns the ipcPortNum.
     */
    //--------------------------------------------
    public int getIpcPortNum()
    {
        return ipcPortNum;
    }

    //--------------------------------------------
    /**
     * @param port The ipcPortNum to set.
     */
    //--------------------------------------------
    public void setIpcPortNum(int port)
    {
        this.ipcPortNum = port;
    }

    //--------------------------------------------
    /**
     * @return Returns the paths.
     */
    //--------------------------------------------
    public PathValues getPaths()
    {
        return paths;
    }

    //--------------------------------------------
    /**
     * @param obj The paths to set.
     */
    //--------------------------------------------
    public void setPaths(PathValues obj)
    {
        this.paths = obj;
    }

    //--------------------------------------------
    /**
     * @return Returns the baseOutputDir.
     */
    //--------------------------------------------
    public File getBaseOutputDir()
    {
        return baseOutputDir;
    }

    //--------------------------------------------
    /**
     * @param dir The baseOutputDir to set.
     */
    //--------------------------------------------
    public void setBaseOutputDir(File dir)
    {
        this.baseOutputDir = dir;
    }

    //--------------------------------------------
    /**
     * @return Returns the genTracker.
     */
    //--------------------------------------------
    public Tracker getGenTracker()
    {
        return genTracker;
    }
}

//+++++++++++
//EOF
//+++++++++++