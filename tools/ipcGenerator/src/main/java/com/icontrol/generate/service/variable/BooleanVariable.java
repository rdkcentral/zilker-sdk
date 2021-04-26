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

import com.icontrol.generate.service.TemplateMappings;
import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.util.StringUtils;
import com.icontrol.xmlns.service.ICBoolVariable;

//-----------------------------------------------------------------------------
// Class definition:    BooleanVariable
/**
 *  Represent a boolean variable
 *
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class BooleanVariable extends BaseVariable
{
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public BooleanVariable(ICBoolVariable bean)
    {
        super(bean);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#resolveReferences()
     */
    //--------------------------------------------
    @Override
    public void resolveReferences()
    {
        // nothing to resolve
        //
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isBoolean()
     */
    //--------------------------------------------
    @Override
    public boolean isBoolean()
    {
        return true;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getCtype()
     */
    //--------------------------------------------
    @Override
    public String getCtype()
    {
        return "bool";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        return "boolean";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJsonKey()
     */
    //--------------------------------------------
    @Override
    public String getJsonKey()
    {
        return TemplateMappings.BOOL_JSON_KEY;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getNativeEncodeLines(int, java.lang.String, boolean, java.lang.String, java.lang.String)
     */
    //--------------------------------------------
    @Override
    public StringBuffer getNativeEncodeLines(ContextNative ctx, int indent, String parentContainerName, boolean parentIsArray, String varName, String varStorageKey)
    {
        StringBuffer buff = new StringBuffer();
        String prefix = getIndentBuffer(indent);
        String prefix2 = getIndentBuffer(indent+1);

        // boolean, so add code to decide if True or False
        //
        buff.append(prefix);
        buff.append("if (");
        if (isPtr() == true)
        {
            buff.append("*");
        }
        buff.append(varName);
        buff.append(" == true)\n");

        // TRUE path
        buff.append(prefix2);
        if (parentIsArray == true)
        {
            buff.append("cJSON_AddItemToArray("+parentContainerName+", cJSON_CreateTrue());\n");
        }
        else
        {
            buff.append("cJSON_AddItemToObjectCS("+parentContainerName+", \"");
            buff.append(varStorageKey);
            buff.append("\", cJSON_CreateTrue());\n");
        }

        // FALSE path
        if (parentIsArray == true)
        {
            buff.append(prefix2);
            buff.append("cJSON_AddItemToArray("+parentContainerName+", cJSON_CreateFalse());\n");
        }
        else
        {
            buff.append(prefix+"else\n");
            buff.append(prefix2+"cJSON_AddItemToObjectCS("+parentContainerName+", \"");
            buff.append(varStorageKey);
            buff.append("\", cJSON_CreateFalse());\n");
        }

        return buff;
    }
    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getNativeDecodeLines(int, java.lang.String, boolean, java.lang.String, java.lang.String)
     */
    //--------------------------------------------
    @Override
    public StringBuffer getNativeDecodeLines(ContextNative ctx, int indent, String parentContainerName, boolean parentIsArray, String varName, String varStorageKey)
    {
        StringBuffer buff = new StringBuffer();
        String prefix = getIndentBuffer(indent);
        String prefix2 = getIndentBuffer(indent+1);

        buff.append(prefix);
        if (parentIsArray == true)
        {
            // get item at 'varStorageKey', which we assume is the index into the array
            //
            buff.append("item = cJSON_GetArrayItem("+parentContainerName+", ");
            buff.append(varStorageKey);
            buff.append(");\n");
        }
        else
        {
            // get item with this key
            //
            buff.append("item = cJSON_GetObjectItem("+parentContainerName+", \"");
            buff.append(varStorageKey);
            buff.append("\");\n");
        }

        // now the logic for our data type and dealing with non-null value
        //
        buff.append(prefix+"if (item != NULL)\n"+prefix+"{\n");

        buff.append(prefix2);
        buff.append(varName+" = cJSON_IsTrue(item) ? true : false;\n");
        buff.append(prefix2+"rc = 0;\n"+prefix+"}\n");

        return buff;
    }
    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaEncodeLines(int, java.lang.String, boolean, java.lang.String, java.lang.String)
     */
    //--------------------------------------------
    @Override
    public StringBuffer getJavaEncodeLines(int indent, String parentContainerName, boolean parentIsArray, String varName, String varStorageKey)
    {
        StringBuffer buff = new StringBuffer();
        String prefix = getIndentBuffer(indent);

        buff.append(prefix);
        if (parentIsArray == true)
        {
            buff.append(parentContainerName+".put("+varName+");\n");
        }
        else
        {
            buff.append(parentContainerName+".put(\""+varStorageKey+"\", "+varName+");\n");
        }

        return buff;
    }
    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaDecodeLines(int, java.lang.String, boolean, java.lang.String, java.lang.String)
     */
    //--------------------------------------------
    @Override
    public StringBuffer getJavaDecodeLines(int indent, String parentContainerName, boolean parentIsArray, String varName, String varStorageKey)
    {
        StringBuffer buff = new StringBuffer();
        String prefix = getIndentBuffer(indent);

        String base = StringUtils.camelCase(getJavaType());
        if (parentIsArray == true)
        {
            buff.append(prefix);
            buff.append(getJavaType()+" "+varName+" = "+parentContainerName+".get"+base+"("+varStorageKey+");\n");
        }
        else
        {
            buff.append(prefix+varName+" = "+parentContainerName+".opt"+base+"(\""+varStorageKey+"\"");
            buff.append(");\n");
        }

        return buff;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaIpcObjectType()
     */
    //--------------------------------------------
    @Override
    public String getJavaIpcObjectType()
    {
        return "BooleanIPC";
    }
}

//+++++++++++
//EOF
//+++++++++++
