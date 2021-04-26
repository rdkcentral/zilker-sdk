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
import com.icontrol.xmlns.service.ICNumType;
import com.icontrol.xmlns.service.ICNumVariable;

//-----------------------------------------------------------------------------
// Class definition:    NumberVariable
/**
 *  Variable for primitive types (int, double, etc)
 *
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class NumberVariable extends BaseVariable
{
    private ICNumType.Enum type;
    private boolean nativeSigned = true;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public NumberVariable(ICNumVariable bean)
    {
        super(bean);
        if (bean != null)
        {
            type = bean.getKind();
            if (bean.isSetNativeSigned() == true)
            {
                nativeSigned = bean.getNativeSigned();
            }
        }
    }

    //--------------------------------------------
    /**
     * Constructor when all we have is a 'type'.  Cannot be
     * used for native implementations
     */
    //--------------------------------------------
    public NumberVariable(String varName, ICNumType.Enum kind)
    {
        super(varName);
        type = kind;
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
     * @see com.icontrol.generate.service.variable.BaseVariable#isInt()
     */
    //--------------------------------------------
    @Override
    public boolean isShort()
    {
        return (type == ICNumType.SHORT);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isInt()
     */
    //--------------------------------------------
    @Override
    public boolean isInt()
    {
        return (type == ICNumType.INT);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isLong()
     */
    //--------------------------------------------
    @Override
    public boolean isLong()
    {
//        return (type == ICNumType.LONG || type == ICNumType.LONG_LONG);
        return (type == ICNumType.LONG);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isFloat()
     */
    //--------------------------------------------
    @Override
    public boolean isFloat()
    {
        return (type == ICNumType.FLOAT);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isDouble()
     */
    //--------------------------------------------
    @Override
    public boolean isDouble()
    {
        return (type == ICNumType.DOUBLE);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getCtype()
     */
    //--------------------------------------------
    @Override
    public String getCtype()
    {
        if (type == ICNumType.DOUBLE)
        {
            return "double";
        }
        else if (type == ICNumType.FLOAT)
        {
            return "float";
        }
        else if (type == ICNumType.INT)
        {
            if (nativeSigned == false)
            {
                return "uint32_t";
            }
            return "int32_t";
        }
        else if (type == ICNumType.SHORT)
        {
            if (nativeSigned == false)
            {
                return "uint16_t";
            }
            return "int16_t";
        }
        else if (type == ICNumType.LONG)
        {
            if (nativeSigned == false)
            {
                return "uint64_t";
            }
            return "int64_t";
        }

        return null;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        if (type == ICNumType.DOUBLE)
        {
            return "double";
        }
        else if (type == ICNumType.FLOAT)
        {
            return "float";
        }
        else if (type == ICNumType.INT || type == ICNumType.SHORT)
        {
            return "int";
        }
        else if (type == ICNumType.LONG) // || type == ICNumType.LONG_LONG)
        {
            return "long";
        }

        return null;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJsonKey()
     */
    //--------------------------------------------
    @Override
    public String getJsonKey()
    {
        if (type == ICNumType.DOUBLE)
        {
            return TemplateMappings.DOUBLE_JSON_KEY;
        }
        else if (type == ICNumType.FLOAT)
        {
            return TemplateMappings.FLOAT_JSON_KEY;
        }
        else if (type == ICNumType.INT || type == ICNumType.SHORT)
        {
            return TemplateMappings.INT_JSON_KEY;
        }
        else if (type == ICNumType.LONG) // || type == ICNumType.LONG_LONG)
        {
            return TemplateMappings.LONG_JSON_KEY;
        }

        return null;
    }

    public ICNumType.Enum getXmlDefinedType()
    {
        return type;
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

        // now the logic for our data type and dealing with non-null value
        //
        buff.append(prefix+"if (item != NULL)\n"+prefix+"{\n");

        buff.append(prefix2);
        if (isInt() == true || isShort() == true)
        {
            // boolean & small numbers
            buff.append(varName+" = ("+getCtype()+")item->valueint;\n");
        }
        else
        {
            // larger numbers
            buff.append(varName+" = ("+getCtype()+")item->valuedouble;\n");
        }
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
            if (isDouble() == true || isFloat() == true)
            {
                // add extra "0.0" as the fallback
                //
                buff.append(prefix+varName+" = ");
                if (isFloat() == true)
                {
                    // need to typecast the result
                    buff.append("(float)");
                }
                buff.append(parentContainerName+".optDouble(\""+varStorageKey+"\", 0.0");
            }
            else
            {
                buff.append(prefix+varName+" = "+parentContainerName+".opt"+base+"(\""+varStorageKey+"\"");
            }
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
        if (type == ICNumType.DOUBLE)
        {
            return "DoubleIPC";
        }
        else if (type == ICNumType.FLOAT)
        {
            return "FloatIPC";
        }
        else if (type == ICNumType.INT || type == ICNumType.SHORT)
        {
            return "IntegerIPC";
        }
        else if (type == ICNumType.LONG) // || type == ICNumType.LONG_LONG)
        {
            return "LongIPC";
        }

        return null;
    }
}

//+++++++++++
//EOF
//+++++++++++
