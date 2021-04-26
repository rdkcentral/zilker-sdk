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
import com.icontrol.xmlns.service.ICJsonVariable;
import com.icontrol.xmlns.service.ICStringVariable;

//-----------------------------------------------------------------------------
// Class definition:    JsonVariable
/**
 *  variable that remains as a JSON object.  used in situations
 *  where structured-extraneous data may be present within an
 *  object that should remain intact.  for example, details about
 *  a "trouble event" since we don't have subclassing in C
 *
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class JsonVariable extends BaseVariable
{
    private boolean doubleDref = false;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public JsonVariable(ICJsonVariable bean)
    {
        super(bean);
    }

    //--------------------------------------------
    /**
     * @see BaseVariable#resolveReferences()
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
     * @see BaseVariable#isPtr()
     */
    //--------------------------------------------
    @Override
    public boolean isPtr()
    {
        return true;
    }

    //--------------------------------------------
    /**
     * @see BaseVariable#isString()
     */
    //--------------------------------------------
    @Override
    public boolean isString()
    {
        return false;
    }

    //--------------------------------------------
    /**
     * @see BaseVariable#getCtype()
     */
    //--------------------------------------------
    @Override
    public String getCtype()
    {
//        return "char["+getLength()+"]";
        return "cJSON";
    }

    //--------------------------------------------
    /**
     * @see BaseVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        return "JSONObject";
    }

    //--------------------------------------------
    /**
     * @see BaseVariable#getJsonKey()
     */
    //--------------------------------------------
    @Override
    public String getJsonKey()
    {
        return TemplateMappings.JSON_JSON_KEY;
    }

    public void setDoubleDeref()
    {
        // only applies to "native decoding"
        doubleDref = true;
    }

    //--------------------------------------------
    /**
     * @see BaseVariable#getNativeEncodeLines(int, String, boolean, String, String)
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
            buff.append(prefix2+"cJSON_AddItemReferenceToArray("+parentContainerName+", "+varName+");\n");
            buff.append(prefix+"}\n");
        }
        else
        {
            buff.append(prefix+"if ("+varName+" != NULL)\n");
            buff.append(prefix+"{\n");
            buff.append(prefix2+"cJSON_AddItemToObjectCS("+parentContainerName+", \""+varStorageKey+"\", cJSON_Duplicate("+varName+", true));\n");
            buff.append(prefix+"}\n");
        }

        return buff;
    }
    //--------------------------------------------
    /**
     * @see BaseVariable#getNativeDecodeLines(int, String, boolean, String, String)
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
            buff.append(prefix+"if (item != NULL)\n"+prefix+"{\n");

            buff.append(prefix2);
            if (doubleDref == true)
            {
                buff.append('*');
            }
            buff.append(varName+" = cJSON_Duplicate(item, true);\n");
        }
        else
        {
            buff.append(prefix+"item = cJSON_GetObjectItem("+parentContainerName+", \"");
            buff.append(varStorageKey);
            buff.append("\");\n");
            buff.append(prefix+"if (item != NULL)\n"+prefix+"{\n");

            buff.append(prefix2);
            if (doubleDref == true)
            {
                buff.append('*');
            }
            buff.append(varName+" = cJSON_Duplicate(item, true);\n");
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
     * @see BaseVariable#getJavaEncodeLines(int, String, boolean, String, String)
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
     * @see BaseVariable#getJavaDecodeLines(int, String, boolean, String, String)
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
            buff.append(getJavaType()+" "+varName+" = "+parentContainerName+".getJSONObject("+varStorageKey+");\n");
        }
        else
        {
            buff.append(varName+" = "+parentContainerName+".optJSONObject(\""+varStorageKey+"\");\n");
        }

        return buff;
    }

    //--------------------------------------------
    /**
     * @see BaseVariable#getJavaIpcObjectType()
     */
    //--------------------------------------------
    @Override
    public String getJavaIpcObjectType()
    {
        return "JsonIPC";
    }
}

//+++++++++++
//EOF
//+++++++++++
