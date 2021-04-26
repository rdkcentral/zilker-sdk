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

//-----------------------------------------------------------------------------
/**
 * Set of variables that are common to generating Java files, but
 * specific enough to have direct variables for (instead of tossing into
 * a hash).
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class ContextJava
{
    private String  javaFileName;       // file name of a Java object
    private String  javaClassName;      // class name of a Java object
    private String  javaPackageName;    // class name of a Java object
    // TODO: track interfaces or superclasses?

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public ContextJava()
    {
    }

    public boolean wasGenerated()
    {
        // return true if filname was set, meaining it was generated
        //
        return (javaFileName != null);
    }

    public String getJavaFileName()
    {
        return javaFileName;
    }

    public void setJavaFileName(String fileName)
    {
        this.javaFileName = fileName;
    }

    public String getJavaClassName()
    {
        return javaClassName;
    }

    public void setJavaClassName(String cName)
    {
        this.javaClassName = cName;
    }

    public String getJavaPackageName()
    {
        return javaPackageName;
    }

    public void setJavaPackageName(String pkgName)
    {
        this.javaPackageName = pkgName;
    }
}

