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

import java.util.ArrayList;

import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.generate.service.parse.PojoWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.util.StringUtils;
import com.icontrol.xmlns.service.ICArrayVariable;

//-----------------------------------------------------------------------------
// Class definition:    ArrayVariable
/**
 *  Represent a dynamic list of BaseVariable types for Java or C
 *
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class ArrayVariable extends BaseVariable
{
    private BaseVariable elementType;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public ArrayVariable(ICArrayVariable bean)
    {
        super(bean);
    }

    //--------------------------------------------
    /**
     * @return Returns the elementType.
     */
    //--------------------------------------------
    public BaseVariable getFirstElement()
    {
        return elementType;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#resolveReferences()
     */
    //--------------------------------------------
    @Override
    public void resolveReferences()
    {
        // parse the 'element' so we know what type the array is
        // done here so we can resolve (in the case of Custom)
        //
        ICArrayVariable bean = (ICArrayVariable)node;
        if (bean.isSetBoolVariable() == true)
        {
            elementType = Processor.getOrCreateVariable(bean.getBoolVariable());
        }
        else if (bean.isSetCustomRef() == true)
        {
            elementType = Processor.getCustomForName(bean.getCustomRef());
        }
        else if (bean.isSetDateVariable() == true)
        {
            elementType = Processor.getOrCreateVariable(bean.getDateVariable());
        }
        else if (bean.isSetEnumVariable() == true)
        {
            elementType = Processor.getOrCreateVariable(bean.getEnumVariable());
        }
        else if (bean.isSetNumVariable() == true)
        {
            elementType = Processor.getOrCreateVariable(bean.getNumVariable());
        }
        else if (bean.isSetStringVariable() == true)
        {
            elementType = Processor.getOrCreateVariable(bean.getStringVariable());
        }

//        if (elementType == null)
//        {
//            throw new RuntimeException("Unable to locate array element definition for "+getName());
//        }
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isArray()
     */
    //--------------------------------------------
    @Override
    public boolean isArray()
    {
        return true;
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
     *
     */
    //--------------------------------------------
    public String getJavaBaseType()
    {
        // get the base class of our elements
        //
        String base = elementType.getJavaType();
        if (elementType.isInt() == true || base.equalsIgnoreCase("int") == true)
        {
            base = "Integer";
        }

        return StringUtils.camelCase(base);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        // make an array list for Java
        //
        return "ArrayList<"+getJavaBaseType()+">";
    }

    //--------------------------------------------
    /**
     * used by CustomVariable
     */
    //--------------------------------------------
    protected String getJavaConstructorLines(int indent)
    {
        StringBuffer retVal = new StringBuffer();
        String prefix = getIndentBuffer(indent);

        retVal.append(prefix);
        retVal.append(getName()+" = new "+getJavaType()+"();\n");

        return retVal.toString();
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getCtype()
     */
    //--------------------------------------------
    @Override
    public String getCtype()
    {
        // for C, use the icLinkedList (defined as void *)
        //
        return "icLinkedList";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJsonKey()
     */
    //--------------------------------------------
    @Override
    public String getJsonKey()
    {
        // use the name
        //
        return getName();
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

        // get unique names for a counter and a JSON array container
        // this way if we don't conflict if mixed in with a Custom that has multiple arrays
        //
        String objName = getName();
        String container = getName()+"_jArray";

        // for C, use the linkedListIterator to loop through the contents
        // and then encode each (based on our common type)
        //
        buff.append(prefix+"// create json array to hold elements in "+varName+"\n");
        buff.append(prefix+"cJSON *"+container+" = cJSON_CreateArray();\n");
        buff.append(prefix+"icLinkedListIterator *"+objName+"_loop = linkedListIteratorCreate("+varName+");\n");
        buff.append(prefix+"while (linkedListIteratorHasNext("+objName+"_loop) == true)\n");
        buff.append(prefix+"{\n");
        buff.append(prefix2+elementType.getCtype()+" *c = ("+elementType.getCtype()+" *)linkedListIteratorGetNext("+objName+"_loop);\n");

        if (elementType.isCustom() == true)
        {
            CustomVariable custom = (CustomVariable)elementType;

            // look for pojo wrapper to see if we can just call this objects 'encode' function
            //
            PojoWrapper wrapper = Processor.getPojoProcessor().getPojoForName(custom.getName());
            if (wrapper != null && StringUtils.isEmpty(wrapper.getContextNative().getEncodeFunctionName()) == false)
            {
                // perform encode
                //
                String function = wrapper.getContextNative().getEncodeFunctionName();
                buff.append(prefix2+"cJSON *"+custom.getName()+"_json = "+function+"(c);\n");

                // add to the array
                //
//                buff.append(prefix2+"cJSON_AddItemToObject("+container+", \""+custom.getName()+"\", "+custom.getName()+"_json);\n");
                buff.append(prefix2+"cJSON_AddItemToArray("+container+", "+custom.getName()+"_json);\n");
            }
            else
            {
                // do it the hard way
                //
                ArrayList<BaseVariable> subFields = custom.getVariables();
                for (int x = 0 ; x < subFields.size() ; x++)
                {
                    // encode each field
                    //
                    BaseVariable subField = subFields.get(x);
                    StringBuffer lines = subField.getNativeEncodeLines(ctx, indent+1, container, true, "c", "not_used");
                    buff.append(lines);
                }
            }
        }
        else
        {
            // encode this field, but need to dereference it
            //
            String target = "c";
            if (elementType.isPtr() == false)
            {
                target = "*c";
            }
            StringBuffer lines = elementType.getNativeEncodeLines(ctx, indent+1, container, false, target, "jArray");
            buff.append(lines);
        }
        buff.append(prefix+"}\n");
        buff.append(prefix+"linkedListIteratorDestroy("+objName+"_loop);\n");
        buff.append(prefix+"cJSON_AddItemToObjectCS("+parentContainerName+", \""+varStorageKey+"\", "+container+");\n");

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
        String prefix3 = getIndentBuffer(indent+2);
        String prefix4 = getIndentBuffer(indent+3);

        // get unique names for a counter and a JSON array container
        // this way if we don't conflict if mixed in with a Custom that has multiple arrays
        //
        String counter = getName()+"_cnt";
        String max = getName()+"_max";
        String container = getName()+"_jArray";
        String next = getName()+"_next";

        // get the JSON array, and pull objects until we hit NULL
        //
        buff.append(prefix+"cJSON *"+container+" = cJSON_GetObjectItem("+parentContainerName+", (const char *)\""+varStorageKey+"\");\n");
        buff.append(prefix+"if ("+container+" != NULL)\n"+prefix+"{\n");
        buff.append(prefix2+"int "+counter+" = 0;\n");
        buff.append(prefix2+"int "+max+" = cJSON_GetArraySize("+container+");\n");
        buff.append(prefix2+"for ("+counter+" = 0 ; "+counter+" < "+max+" ; "+counter+"++)\n");
        buff.append(prefix2+"{\n");

        if (elementType.isCustom() == true)
        {
            // add JSON extract this element from the array
            // first, get the element from the array
            //
            buff.append(prefix3+"// extract JSON element from the array\n");
            buff.append(prefix3+"cJSON *"+container+"_element = cJSON_GetArrayItem("+container+", "+counter+");\n");
            buff.append(prefix3+"if ("+container+"_element == NULL)\n");
            buff.append(prefix3+"{\n");
            buff.append(prefix4+"break;\n");
            buff.append(prefix3+"};\n");

//            // now get the container of the custom object
//            //
//            buff.append(prefix3+"// extract Custom JSON object container "+elementType.getName()+" from the array element\n");
//            buff.append(prefix3+"cJSON *"+container+"_item = cJSON_GetObjectItem("+container+"_element, (const char *)\""+ elementType.getName() +"\");\n");
//            buff.append(prefix3+"if ("+container+"_item != NULL)\n");
//            buff.append(prefix3+"{\n");
//
            // decode this field, as a local variable
            //
            buff.append(prefix4);
            buff.append(elementType.getCtype());
            buff.append(" *"+next+" = NULL;\n");
//            String destructor = null;

            // look for pojo wrapper to see if we can just call this objects 'create' or 'encode' function
            //
            PojoWrapper wrapper = Processor.getPojoProcessor().getPojoForName(elementType.getName());
            if (wrapper != null && StringUtils.isEmpty(wrapper.getContextNative().getCreateFunctionName()) == false)
            {
                // add constructor, save destructor
                buff.append(prefix4);
                buff.append(next+" = "+wrapper.getContextNative().getCreateFunctionName()+"();\n");
//                destructor = wrapper.getContextNative().getDestroyFunctionName();
            }
            else
            {
                // malloc & free
                buff.append(prefix4+next+" = ("+elementType.getCtype()+" *)malloc(sizeof("+elementType.getCtype()+"));\n");
                buff.append(prefix4+"memset("+next+", 0, sizeof("+elementType.getCtype()+"));\n");
//                destructor = "free";
            }

            if (wrapper != null && StringUtils.isEmpty(wrapper.getContextNative().getDecodeFunctionName()) == false)
            {
                // perform encode
                //
                String function = wrapper.getContextNative().getDecodeFunctionName();
//                buff.append(prefix4+function+"("+next+","+container+"_item);\n");
                buff.append(prefix4+function+"("+next+","+container+"_element);\n");

                // add to the array
                //
                String setter = ctx.getSetFunctionForVar(getName());
                buff.append(prefix4+setter+"(pojo, "+next+");\n");
//                buff.append(prefix2+"cJSON_AddItemToObject("+container+", \""+custom.getName()+"\", "+custom.getName()+"_json);\n");

//                if (destructor != null)
//                {
//                    buff.append(prefix4+destructor+"("+next+");\n");
//                }
            }
            else
            {
                // since now let the custom field do it's normal extraction, as a local variable
                //
                StringBuffer line = elementType.getNativeDecodeLines(ctx, indent+2, container+"_item", true, next, varStorageKey);
                buff.append(line);

                // now call the 'set' function for this variable
                //
                //buff.append(prefix3+"if (item != NULL)\n");
                //buff.append(prefix3+"{\n");
                buff.append(prefix4+"// found JSON, so should be safe to add\n");
                String setter = ctx.getSetFunctionForVar(getName());
                buff.append(prefix4+setter+"(pojo, "+next+");\n");
            }

            // terminate the "if container != NULL"
            //
//            buff.append(prefix3+"}\n");
        }
        else
        {
            // decode this field, as a local variable
            //
            buff.append(prefix3);
            buff.append(elementType.getCtype());

            if (elementType.isString() == true)
            {
//                StringVariable str = (StringVariable)elementType;
//                buff.append(" "+next+"["+str.getLength()+"];\n");
                buff.append(" *"+next+" = NULL;\n");
            }
            else if (elementType.isPtr() == true)
            {
                buff.append(" *"+next+" = NULL;\n");
            }
            else if (elementType.isNumber() == true)
            {
                buff.append(" "+next+" = 0;\n");
            }
            else if (elementType.isEnum() == true)
            {
                // assign the first value
                //
                buff.append(" "+next+" = ");
                EnumVariable ev = (EnumVariable)elementType;
                buff.append(ev.getValues().get(0)+";\n");
            }
            else
            {
                buff.append(" "+next+";\n");
            }
            StringBuffer lines = elementType.getNativeDecodeLines(ctx, indent+2, container, true, next, counter);
            buff.append(lines);

            if (elementType.isString() == true)
            {
                buff.append(prefix3+"if ("+next+" != NULL)\n");
                buff.append(prefix3+"{\n");
                buff.append(prefix4+"// safe to add\n");
                String setter = ctx.getSetFunctionForVar(getName());
                buff.append(prefix4+setter+"(pojo, "+next+");\n");
                buff.append(prefix3+"}\n");
            }
            else
            {
                // now call the 'set' function for this variable
                //
                buff.append(prefix3+"if (item != NULL)\n");
                buff.append(prefix3+"{\n");
                buff.append(prefix4+"// found JSON, so should be safe to add\n");
                String setter = ctx.getSetFunctionForVar(getName());
                buff.append(prefix4+setter+"(pojo, "+next+");\n");
                buff.append(prefix3+"}\n");
            }
        }

        // end for loop
        buff.append(prefix2+"}\n");

        // end if
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
        String prefix2 = getIndentBuffer(indent+1);
        String prefix3 = getIndentBuffer(indent+2);

        // get unique names for JSON array container
        // this way if we don't conflict if mixed in with a Custom that has multiple arrays
        //
        String container = getName()+"_jArray";
        String loop = getName()+"_loop";
        String base = getJavaBaseType();

        buff.append(prefix+"if ("+varName+" != null)\n");
        buff.append(prefix+"{\n");
        buff.append(prefix2+"JSONArray "+container+" = new JSONArray();\n");
        buff.append(prefix2+"Iterator<"+base+"> "+loop+" = "+varName+".iterator();\n");
        buff.append(prefix2+"while ("+loop+".hasNext() == true)\n");
        buff.append(prefix2+"{\n");
        buff.append(prefix3+base+" curr = "+loop+".next();\n");

        StringBuffer line = elementType.getJavaEncodeLines(indent+2, container, true, "curr", null);
        buff.append(line);
        buff.append(prefix2+"}\n");  // end 'while'

        // now add our array to the parent
        //
        buff.append(prefix2);
        buff.append(parentContainerName+".put(\""+varStorageKey+"\", "+container+");\n");
        buff.append(prefix+"}\n");

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
        String prefix2 = getIndentBuffer(indent+1);
        String prefix3 = getIndentBuffer(indent+2);

        // get unique names for JSON array container
        // this way if we don't conflict if mixed in with a Custom that has multiple arrays
        //
        String container = getName()+"_jArray";
        String loop = getName()+"_loop";
//        String base = getJavaBaseType();

        buff.append(prefix+getName()+" = new ArrayList<"+getJavaBaseType()+">();\n");
        buff.append(prefix+"JSONArray "+container+" = (JSONArray)"+parentContainerName+".opt(\""+varStorageKey+"\");\n");
        buff.append(prefix+"if ("+container+" != null)\n");
        buff.append(prefix+"{\n");
        buff.append(prefix2+"for (int "+loop+" = 0 ; "+loop+" < "+container+".length() ; "+loop+"++)\n");
        buff.append(prefix2+"{\n");

        StringBuffer line = elementType.getJavaDecodeLines(indent+2, container, true, "next", loop);
        buff.append(line);

        buff.append(prefix3+getName()+".add(next);\n");
        buff.append(prefix2+"}\n");
        buff.append(prefix+"}\n");

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
        // only used as input/output parameters in IPC Messages
        // that is NOT SUPPORTED for ArrayVariable, so return null
        //
        return null;
    }

}

//+++++++++++
//EOF
//+++++++++++
