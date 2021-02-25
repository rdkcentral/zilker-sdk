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

import java.util.Arrays;
import java.util.List;

import com.icontrol.util.StringUtils;
import com.icontrol.xmlns.service.ICPragma;
import com.icontrol.xmlns.service.ICPragmaJava;
import com.icontrol.xmlns.service.ICPragmaNative;

//-----------------------------------------------------------------------------
// Class definition:    PathValues
/**
 *  Container of the paths & package names used for a generation session.
 *  This rougly reflects the directories & package names defined in the Pragma
 *  section of the XML file.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class PathValues
{
    private ICPragmaJava   javaBean;
    private ICPragmaNative nativeBean;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public PathValues(ICPragma bean)
    {
        javaBean = bean.getJava();
        nativeBean = bean.getNative();
    }
    
    //--------------------------------------------
    /**
     * Base directory for generated Service code
     * (ex: the IPC handler)
     */
    //--------------------------------------------
    public String getServiceDir()
    {
        return javaBean.getServiceDir();
    }
    
    //--------------------------------------------
    /**
     * Package name for generated Service code
     */
    //--------------------------------------------
    public String getServicePackage()
    {
        return javaBean.getServicePkg();
    }
    
    //--------------------------------------------
    /**
     * Base directory for generated API code
     */
    //--------------------------------------------
    public String getApiDir()
    {
        return javaBean.getApiDir();
    }
    
    //--------------------------------------------
    /**
     * Package name for generated API code
     */
    //--------------------------------------------
    public String getApiPackage()
    {
        return javaBean.getApiPkg();
    }
    
    //--------------------------------------------
    /**
     * Base directory for generated Event code
     */
    //--------------------------------------------
    public String getEventDir()
    {
        return javaBean.getEventDir();
    }
    
    //--------------------------------------------
    /**
     * Package name for generated Event code
     */
    //--------------------------------------------
    public String getEventPackage()
    {
        return javaBean.getEventPkg();
    }
    
    //--------------------------------------------
    /**
     * Returns the directory to use when generating
     * Native POJO, Event, Enum, and IPC header files
     */
    //--------------------------------------------
    public String getNativeApiHeaderDir()
    {
        if (nativeBean != null)
        {
            return nativeBean.getApiHeaderDir();
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Returns the directory to use when generating
     * Native POJO, Event, and Enum source files
     */
    //--------------------------------------------
    public String getNativeApiSrcDir()
    {
        if (nativeBean != null)
        {
            return nativeBean.getApiSrcDir();
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Return true if a native event adapter dir is defined
     */
    //--------------------------------------------
    public boolean isNativeApiDirsDefined()
    {
        return ((nativeBean != null) &&
                (StringUtils.isEmpty(getNativeApiHeaderDir()) == false) ||
                (StringUtils.isEmpty(getNativeApiSrcDir()) == false));
    }

    //--------------------------------------------
    /**
     * Return the directory to use when generating the
     * Native IPC message handler source files
     */
    //--------------------------------------------
    public String getNativeIpcHandlerDir()
    {
        if (nativeBean != null)
        {
            return nativeBean.getIpcHandlerSrcDir();
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Optional list of "includes" to add to the generated native code
     */
    //--------------------------------------------
    public List<String> getNativeIncludes()
    {
        if (nativeBean != null)
        {
            return nativeBean.getIncludeList();
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Optional list of strings to add to the generated native code
     */
    //--------------------------------------------
    public List<String> getNativeFreeforms()
    {
        if (nativeBean != null)
        {
            return nativeBean.getFreeformList();
        }
        return null;
    }

    //--------------------------------------------
    /**
     * Configuration flag to turn on an IPC
     */
    //--------------------------------------------
    public String getNativeConfigFlag()
    {
        if (nativeBean != null)
        {
            return nativeBean.getConfigFlag();
        }
        return null;
    }


    //--------------------------------------------
    /**
     * Optional list of strings to add to the generated native code
     */
    //--------------------------------------------
    public List<String> getJavaFreeforms()
    {
        if (javaBean != null)
        {
            return javaBean.getFreeformList();
        }
        return null;
    }

    @Override
    public String toString()
    {
        StringBuilder sb = new StringBuilder();

        if (javaBean != null)
        {
        }

        if (nativeBean != null)
        {
            sb.append("Native:\n");
            sb.append("  Freeform: [");
            sb.append(nativeBean.getFreeformList().toString());
            sb.append("]\n");

            sb.append("  Config flag: ");
            sb.append(nativeBean.getConfigFlag());
            sb.append("\n");
        }

        return sb.toString();
    }
}

//+++++++++++
//EOF
//+++++++++++
