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

package com.icontrol.generate.service.variable;

//-----------
//imports
//-----------

import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.util.StringUtils;
import com.icontrol.xmlns.service.ICBaseVariable;

//-----------------------------------------------------------------------------
// Class definition:    BaseVariable
/**
 *  Class description goes here
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public abstract class BaseVariable
{
    protected ICBaseVariable    node;
    protected String            name;
    protected String            description;        // optional
    protected boolean           sensitive;          // optional

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public BaseVariable(ICBaseVariable bean)
    {
        node = bean;
        if (node != null)
        {
            name = node.getVarName();
            description = node.getVarDescription();
            sensitive = bean.getSensitive();
        }
    }

    //--------------------------------------------
    /**
     * Constructor for cloners
     */
    //--------------------------------------------
    protected BaseVariable(String varName)
    {
        name = varName;
    }
    
    //--------------------------------------------
    /**
     * Called after everything has been loaded up so that
     * subclasses can resolve their dependencies and references
     */
    //--------------------------------------------
    public abstract void resolveReferences();
    
    //--------------------------------------------
    /**
     * Return the original, unmodified name of this variable
     */
    //--------------------------------------------
    public String getName()
    {
        return name;
    }

    //--------------------------------------------
    /**
     * Set the original unmodified name of this variable
     */
    //--------------------------------------------
    public void setName(String newName)
    {
        this.name = newName;
    }

    //--------------------------------------------
    /**
     * Returns the optional description of the variable.
     */
    //--------------------------------------------
    public String getDescription()
    {
        return description;
    }

    //--------------------------------------------
    /**
     * Set the description of the variable.
     */
    //--------------------------------------------
    public void setDescription(String description)
    {
        this.description = description;
    }

    //--------------------------------------------
    /**
     * @return Returns the sensitive.
     */
    //--------------------------------------------
    public boolean isSensitive()
    {
        return sensitive;
    }

    //--------------------------------------------
    /**
     * @param flag The sensitive to set.
     */
    //--------------------------------------------
    public void setSensitive(boolean flag)
    {
        this.sensitive = flag;
    }

    //--------------------------------------------
    /**
     * For C generation.  Returns if this variable should be
     * treated as a pointer or not.
     */
    //--------------------------------------------
    public boolean isPtr()
    {
        // let supclass overload
        //
        return false;
    }
    
    //--------------------------------------------
    /**
     * return if this is an ARRAY type
     */
    //--------------------------------------------
    public boolean isArray()
    {
        // let supclass overload
        //
        return false;
    }

    //--------------------------------------------
    /**
     * return if this is an MAP type
     */
    //--------------------------------------------
    public boolean isMap()
    {
        // let supclass overload
        //
        return false;
    }

    //--------------------------------------------
    /**
     * return if this is a CUSTOM type
     */
    //--------------------------------------------
    public boolean isCustom()
    {
        // let supclass overload
        //
        return false;
    }
    
    //--------------------------------------------
    /**
     * return if this is an EVENT definition 
     */
    //--------------------------------------------
    public boolean isEvent()
    {
        // let supclass overload
        //
        return false;
    }
    
    //--------------------------------------------
    /**
     * return if this is a ENUM type
     */
    //--------------------------------------------
    public boolean isEnum()
    {
        // let supclass overload
        //
        return false;
    }
    
//    //--------------------------------------------
//    /**
//     * return if this is a CUSTOM type that contains an ARRAY
//     */
//    //--------------------------------------------
//    public boolean doesCustomHaveArray()
//    {
//        // let subclass deal with it
//        //
//        return false;
//    }

    //--------------------------------------------
    /**
     * return if this is a STRING type
     */
    //--------------------------------------------
    public boolean isString()
    {
        // let supclass overload
        //
        return false;
    }

    //--------------------------------------------
    /**
     * returns if this is a Boolean type
     */
    //--------------------------------------------
    public boolean isBoolean()
    {
        // let supclass overload
        //
        return false;
    }

    //--------------------------------------------
    /**
     * returns if this is a Integer type
     */
    //--------------------------------------------
    public boolean isShort()
    {
        // let supclass overload
        //
        return false;
    }
    
    //--------------------------------------------
    /**
     * returns if this is a Integer type
     */
    //--------------------------------------------
    public boolean isInt()
    {
        // let supclass overload
        //
        return false;
    }

    //--------------------------------------------
    /**
     * returns if this is a Long type
     */
    //--------------------------------------------
    public boolean isLong()
    {
        // let supclass overload
        //
        return false;
    }

    //--------------------------------------------
    /**
     * returns if this is a Float type
     */
    //--------------------------------------------
    public boolean isFloat()
    {
        // let supclass overload
        //
        return false;
    }

    //--------------------------------------------
    /**
     * returns if this is a Double type
     */
    //--------------------------------------------
    public boolean isDouble()
    {
        // let supclass overload
        //
        return false;
    }
    
    //--------------------------------------------
    /**
     * returns if this is some form of number
     */
    //--------------------------------------------
    public boolean isNumber()
    {
        return (isLong() || isShort() || isInt() || isFloat() || isDouble());
    }

    //--------------------------------------------
    /**
     * returns if this is a Date type
     */
    //--------------------------------------------
    public boolean isDate()
    {
        // let supclass overload
        //
        return false;
    }

    //--------------------------------------------
    /**
     * Return true if this Variable could possibly be
     * part of an Event
     */
    //--------------------------------------------
    public boolean isPossiblyEvent()
    {
        if (isArray() == true || isEnum() == true || isCustom() == true || isEvent() == true)
        {
            return true;
        }
        return false;
    }

    //--------------------------------------------
    /**
     * Returns the C definition of this type
     */
    //--------------------------------------------
    public abstract String getCtype();

    //--------------------------------------------
    /**
     * Returns the Java definition of this type
     */
    //--------------------------------------------
    public abstract String getJavaType();

    //--------------------------------------------
    /**
     * Returns the JSON key to use
     */
    //--------------------------------------------
    public abstract String getJsonKey();
    
    //--------------------------------------------
    /**
     * Generate C code for encoding this variable
     * 
     * @param parentContainerName - variable name of the parent that the encoding should be inserted to
     * @param varName - variable name to use for accessing this variable
     * @param parentIsArray - true if inserting this value into an Array
     */
    //--------------------------------------------
    public abstract StringBuffer getNativeEncodeLines(ContextNative ctx, int indent, String parentContainerName, boolean parentIsArray, String varName, String varStorageKey);
    
    //--------------------------------------------
    /**
     * Generate C code for decoding this variable
     * 
     * @param parentContainerName - variable name of the parent that the decoding should be extracted from
     * @param varName - variable name to use for accessing this variable
     * @param parentIsArray - true if extracting from an Array
     */
    //--------------------------------------------
    public abstract StringBuffer getNativeDecodeLines(ContextNative ctx, int indent, String parentContainerName, boolean parentIsArray, String varName, String varStorageKey);

    //--------------------------------------------
    /**
     * Returns the Java getter bean method for this type
     */
    //--------------------------------------------
    public StringBuffer getJavaGetter()
    {
        StringBuffer buff = new StringBuffer();
        
        buff.append("\tpublic ");
        buff.append(getJavaType());
        buff.append(" get");
        buff.append(StringUtils.camelCase(getName()));
        buff.append("()\n");
        buff.append("\t{\n");
        buff.append("\t\treturn ");
        buff.append(getName());
        buff.append(";\n\t}\n\n");
        
        return buff;
    }

    //--------------------------------------------
    /**
     * Returns the Java setter bean method for this type
     */
    //--------------------------------------------
    public StringBuffer getJavaSetter()
    {
        StringBuffer buff = new StringBuffer();
        
        buff.append("\tpublic void set");
        buff.append(StringUtils.camelCase(getName()));
        buff.append("("+getJavaType()+" val)\n");
        buff.append("\t{\n\t\t");
        buff.append(getName());
        buff.append(" = val;\n");
        buff.append("\t}\n\n");
        
        return buff;
    }

    //--------------------------------------------
    /**
     * Generate Java code for encoding this variable
     * 
     * @param parentContainerName - variable name of the parent that the encoding should be inserted to
     * @param varName - variable name to use for accessing this variable
     * @param parentIsArray - true if inserting this value into an Array
     */
    //--------------------------------------------
    public abstract StringBuffer getJavaEncodeLines(int indent, String parentContainerName, boolean parentIsArray, String varName, String varStorageKey);
    
    //--------------------------------------------
    /**
     * Generate Java code for decoding this variable
     * 
     * @param parentContainerName - variable name of the parent that the decoding should be extracted from
     * @param varName - variable name to use for accessing this variable
     * @param parentIsArray - true if extracting from an Array
     */
    //--------------------------------------------
    public abstract StringBuffer getJavaDecodeLines(int indent, String parentContainerName, boolean parentIsArray, String varName, String varStorageKey);

    //--------------------------------------------
    /**
     * Returns the Java definition of this type, when utilized
     * directly as an ObjectIPC instance.  This is only used
     * for Message Input/Output objects since they are marshaled
     * and un-marshaled directly.
     */
    //--------------------------------------------
    public abstract String getJavaIpcObjectType();
    
    //--------------------------------------------
    /**
     * convenience routine to add blank spaces out front
     * to make the indentation correct during generation
     */
    //--------------------------------------------
    protected String getIndentBuffer(int indent)
    {
        StringBuffer buff = new StringBuffer();
        for (int i = 0 ; i < indent ; i++)
        {
            buff.append("    ");
        }
        return buff.toString();
    }

	@Override
	public String toString() 
	{
		return "BaseVariable [name=" + name + "]";
	}
}

//+++++++++++
//EOF
//+++++++++++