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
// Class definition:    Generated
/**
 *  Container of information about a generated source
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class Generated
{
    private String  varName;
    private String  className;
    private String  packageName;
    private File    srcFile;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public Generated()
    {
    }

    //--------------------------------------------
    /**
     * @return Returns the varName.
     */
    //--------------------------------------------
    public String getVarName()
    {
        return varName;
    }

    //--------------------------------------------
    /**
     * @param varName The varName to set.
     */
    //--------------------------------------------
    public void setVarName(String varName)
    {
        this.varName = varName;
    }

    //--------------------------------------------
    /**
     * @return Returns the className.
     */
    //--------------------------------------------
    public String getClassName()
    {
        return className;
    }

    //--------------------------------------------
    /**
     * @param className The className to set.
     */
    //--------------------------------------------
    public void setClassName(String className)
    {
        this.className = className;
    }

    //--------------------------------------------
    /**
     * @return Returns the packageName.
     */
    //--------------------------------------------
    public String getPackageName()
    {
        return packageName;
    }

    //--------------------------------------------
    /**
     * @param packageName The packageName to set.
     */
    //--------------------------------------------
    public void setPackageName(String packageName)
    {
        this.packageName = packageName;
    }

    //--------------------------------------------
    /**
     * @return Returns the srcFile.
     */
    //--------------------------------------------
    public File getSrcFile()
    {
        return srcFile;
    }

    //--------------------------------------------
    /**
     * @param srcFile The srcFile to set.
     */
    //--------------------------------------------
    public void setSrcFile(File srcFile)
    {
        this.srcFile = srcFile;
    }

}



//+++++++++++
//EOF
//+++++++++++