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

package com.icontrol.generate.service.ipc;

//-----------
//imports
//-----------

import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.generate.service.variable.StringVariable;
import com.icontrol.xmlns.service.ICMessageParm;

//-----------------------------------------------------------------------------
// Class definition:    MessageParam
/**
 *  Input or Output parameter for a Message
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class MessageParam
{
    private BaseVariable   field;      // so we know our 'type'
    private String         name;       // definable
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public MessageParam(ICMessageParm bean)
    {
//        node = bean;

        // look at the bean so we can determine the 'type'
        // that this parameter will be (i.e. String, int, custom, etc)
        //
        if (bean.isSetBool() == true)
        {
            field = Processor.getOrCreateVariable(bean.getBool());
        }
        else if (bean.isSetCustomRef() == true)
        {
            field = Processor.getCustomForName(bean.getCustomRef());
            if (field == null)
            {
                throw new RuntimeException("Invalid messageParm, cannot find Custom node '"+bean.getCustomRef()+"'");
            }
        }
        else if (bean.isSetDate() == true)
        {
            field = Processor.getOrCreateVariable(bean.getDate());
        }
//        else if (bean.isSetEnumTypeName() == true)
//        {
//            field = VariableFactory.getEnumVariableForName(bean.getEnumTypeName());
//        }
//        else if (bean.isSetExistingRef() == true)
//        {
//            field = Processor.getExistingForName(bean.getExistingRef());
//            if (field == null)
//            {
//                throw new RuntimeException("Invalid messageParm, cannot find Existing node '"+bean.getExistingRef()+"'");
//            }
//        }
        else if (bean.isSetNumber() == true)
        {
            field = Processor.getOrCreateVariable(bean.getNumber());
        }
        else if (bean.isSetString() == true)
        {
            field = Processor.getOrCreateVariable(bean.getString());
        }
    }

    //--------------------------------------------
    /**
     * @return Returns the field.
     */
    //--------------------------------------------
    public BaseVariable getField()
    {
        return field;
    }

    //--------------------------------------------
    /**
     * @return Returns the name.
     */
    //--------------------------------------------
    public String getName()
    {
        return name;
    }

    //--------------------------------------------
    /**
     * @param name The name to set.
     */
    //--------------------------------------------
    public void setName(String name)
    {
        this.name = name;
    }

    public String getNativeDeclaration(ContextNative ctx, String varName, boolean forMalloc)
    {
        StringBuffer retVal = new StringBuffer();

        retVal.append(field.getCtype());
        retVal.append(' ');
        if (field.isPtr() == true)
        {
            if (forMalloc == true || field.isString() == true)
            {
                // let something malloc this mem
                //
                retVal.append("*" + varName + " = NULL;");
            }
            else if (field.isCustom() == true)
            {
                // use constructor
                retVal.append("*"+varName+" = "+ctx.getCreateFunctionName()+"();\n");
            }
            else
            {
                // assuming struct
                //
                retVal.append(varName+";");
            }
        }
        else
        {
            retVal.append(varName+" = 0;");
        }

        return retVal.toString();
    }
}

//+++++++++++
//EOF
//+++++++++++