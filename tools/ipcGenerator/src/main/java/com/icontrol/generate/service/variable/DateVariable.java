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
import com.icontrol.xmlns.service.ICDateVariable;

//-----------------------------------------------------------------------------
// Class definition:    DateVariable
/**
 *  Represent a Date variable
 *
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class DateVariable extends BaseVariable
{
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public DateVariable(ICDateVariable bean)
    {
        super(bean);
    }

    //--------------------------------------------
    /**
     * Constructor for this class when making one manually
     */
    //--------------------------------------------
    public DateVariable(String varName)
    {
        super(varName);
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
     * @see com.icontrol.generate.service.variable.BaseVariable#isDate()
     */
    //--------------------------------------------
    @Override
    public boolean isDate()
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
        return "uint64_t";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaIpcObjectType()
     */
    //--------------------------------------------
    @Override
    public String getJavaIpcObjectType()
    {
        return "DateIPC";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        return "Date";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJsonKey()
     */
    //--------------------------------------------
    @Override
    public String getJsonKey()
    {
        return TemplateMappings.DATE_JSON_KEY;
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
//        String prefix2 = getIndentBuffer(indent+1);

        // TODO: assuming time_t

        buff.append(prefix);
        if (parentIsArray == true)
        {
            buff.append("cJSON_AddItemToArray("+parentContainerName+", cJSON_CreateNumber(");
            buff.append(varName);
            buff.append("));\n");
        }
        else
        {
            buff.append("cJSON_AddItemToObjectCS("+parentContainerName+", \"");
            buff.append(varStorageKey);
            buff.append("\", cJSON_CreateNumber(");
            if (isPtr() == true)
            {
                buff.append("*");
            }
            buff.append(varName);
            buff.append("));\n");
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

        // assuming milliseconds since epoch as uint64_t

        // now the logic for our data type and dealing with non-null value
        //
        buff.append(prefix+"if (item != NULL)\n"+prefix+"{\n");
        buff.append(prefix2);
        buff.append(varName+" = ("+getCtype()+")item->valuedouble;\n");
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
            buff.append("if ("+varName+" != null) ");
            buff.append(parentContainerName+".put(\""+varStorageKey+"\", "+varName+".getTime());\n");
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
            buff.append(prefix+"long "+varName+"_time = "+parentContainerName+".optLong(\""+varStorageKey+"\");\n");
            buff.append(prefix+"if ("+varName+"_time > 0) "+varName+" = new Date("+varName+"_time);\n");
        }

        return buff;
    }


}

//+++++++++++
//EOF
//+++++++++++
