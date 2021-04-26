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
import com.icontrol.xmlns.service.ICStringVariable;

//-----------------------------------------------------------------------------
// Class definition:    StringField
/**
 *  String implementation of Field
 *
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class StringVariable extends BaseVariable
{
    private boolean doubleDref = false;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public StringVariable(ICStringVariable bean)
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
     * @see com.icontrol.generate.service.variable.BaseVariable#isPtr()
     */
    //--------------------------------------------
    @Override
    public boolean isPtr()
    {
        return true;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isString()
     */
    //--------------------------------------------
    @Override
    public boolean isString()
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
//        return "char["+getLength()+"]";
        return "char";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        return "String";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJsonKey()
     */
    //--------------------------------------------
    @Override
    public String getJsonKey()
    {
        return TemplateMappings.STRING_JSON_KEY;
    }

    public void setDoubleDeref()
    {
        // only applies to "native decoding"
        doubleDref = true;
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

        if (parentIsArray == true)
        {
            buff.append(prefix+"if ("+varName+" != NULL)\n");
            buff.append(prefix+"{\n");
            buff.append(prefix2+"cJSON_AddItemToArray("+parentContainerName+", cJSON_CreateString("+varName+"));\n");
            buff.append(prefix+"}\n");
        }
        else
        {
            buff.append(prefix+"if ("+varName+" != NULL)\n");
            buff.append(prefix+"{\n");
            buff.append(prefix2+"cJSON_AddItemToObjectCS("+parentContainerName+", \""+varStorageKey+"\", cJSON_CreateString("+varName+"));\n");
            buff.append(prefix+"}\n");
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

        if (parentIsArray == true)
        {
            // get item at 'varStorageKey', which we assume is the index into the array
            //
            buff.append(prefix+"item = cJSON_GetArrayItem("+parentContainerName+", ");
            buff.append(varStorageKey);
            buff.append(");\n");
            buff.append(prefix+"if (item != NULL && item->valuestring != NULL)\n"+prefix+"{\n");

            buff.append(prefix2);
            if (doubleDref == true)
            {
                buff.append('*');
            }
            buff.append(varName+" = strdup(item->valuestring);\n");
        }
        else
        {
            buff.append(prefix+"item = cJSON_GetObjectItem("+parentContainerName+", \"");
            buff.append(varStorageKey);
            buff.append("\");\n");
            buff.append(prefix+"if (item != NULL && item->valuestring != NULL)\n"+prefix+"{\n");

            buff.append(prefix2);
            if (doubleDref == true)
            {
                buff.append('*');
            }
            buff.append(varName+" = strdup(item->valuestring);\n");
        }
        buff.append(prefix2+"rc = 0;\n");
        buff.append(prefix+"}\n");

        // force doubleDref flag off to force the next generation
        // to reset if needed
        //
        doubleDref = false;

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

        buff.append(prefix);
        if (parentIsArray == true)
        {
            buff.append(getJavaType()+" "+varName+" = "+parentContainerName+".getString("+varStorageKey+");\n");
        }
        else
        {
            //FIXME: default to null to match native (defaults to empty string)
            buff.append(varName+" = "+parentContainerName+".optString(\""+varStorageKey+"\");\n");
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
        return "StringIPC";
    }
}

//+++++++++++
//EOF
//+++++++++++
