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

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;

import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.generate.service.io.FileHelper;
import com.icontrol.generate.service.io.MacroSource;
import com.icontrol.generate.service.parse.PojoWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.util.StringUtils;
import com.icontrol.xmlns.service.*;

//-----------------------------------------------------------------------------
// Class definition:    ArrayVariable
/**
 *  Represent a dynamic key/value map of BaseVariable types for Java or C
 *  Assumes the key will be String and the value is something we know how to
 *  encode/decode.
 *
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class MapVariable extends BaseVariable
{
    private static final String JAVA_ENCODE_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/map_encode_java";
    private static final String JAVA_DECODE_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/map_decode_java";
    private static final String NATIVE_ENCODE_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/map_encode_c";
    private static final String NATIVE_DECODE_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/map_decode_c";

    private ArrayList<BaseVariable> possibleTypes;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public MapVariable(ICMapVariable bean)
    {
        // init super with just our name
        //
        super(bean);

        // create container for our value types
        //
        possibleTypes = new ArrayList<BaseVariable>();
    }

    //--------------------------------------------
    /**
     * @return Returns the elementType.
     */
    //--------------------------------------------
    public ArrayList<BaseVariable> getPossibleTypes()
    {
        return possibleTypes;
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
        ICMapVariable bean = (ICMapVariable)node;
        possibleTypes.clear();

        // see which data-types we support.  some are "references" because
        // they need specifics (for ex: Enum & CustomVariables)
        //
        if (bean.getBoolVariableList().size() > 0)
        {
            Iterator<ICBoolVariable> tmp = bean.getBoolVariableList().iterator();
            while (tmp.hasNext() == true)
            {

                BooleanVariable b = new BooleanVariable(tmp.next());
                possibleTypes.add(b);
            }
        }
        if (bean.getStringVariableList().size() > 0)
        {
            Iterator<ICStringVariable> tmp = bean.getStringVariableList().iterator();
            while (tmp.hasNext() == true)
            {
                StringVariable str = new StringVariable(tmp.next());
                possibleTypes.add(str);
            }
        }
        if (bean.getDateVariableList().size() > 0)
        {
            Iterator<ICDateVariable> tmp = bean.getDateVariableList().iterator();
            while (tmp.hasNext() == true)
            {
                DateVariable date = new DateVariable(tmp.next());
                possibleTypes.add(date);
            }
        }
        if (bean.getNumVariableList().size() > 0)
        {
            // add each distinct numeric type
            //
            HashMap<ICNumType.Enum,ICNumVariable> types = new HashMap<ICNumType.Enum,ICNumVariable>();
            Iterator<ICNumVariable> tmp = bean.getNumVariableList().iterator();
            while (tmp.hasNext() == true)
            {
                ICNumVariable next = tmp.next();
                ICNumType.Enum kind = next.getKind();

                // add to temp map if not there
                //
                if (types.containsKey(kind) == false)
                {
                    types.put(kind, next);
                }
            }

            // now iterate the temp hash, adding a number for each
            //
            Iterator<ICNumVariable> loop = types.values().iterator();
            while (loop.hasNext() == true)
            {
                NumberVariable num = new NumberVariable(loop.next());
                possibleTypes.add(num);
            }
        }
        if (bean.getEnumVariableList().size() > 0)
        {
            // resolve each enumeration, and if found add to our set
            //
            Iterator<ICEnumVariable> tmp = bean.getEnumVariableList().iterator();
            while (tmp.hasNext() == true)
            {
                EnumVariable ev = (EnumVariable)Processor.getOrCreateVariable(tmp.next());
                if (ev != null)
                {
                    possibleTypes.add(ev);
                }
            }
        }

        if (bean.getCustomRefList().size() > 0)
        {
            // resolve each custom (for the name), and if found add to our set
            //
            Iterator<String> tmp = bean.getCustomRefList().iterator();
            while (tmp.hasNext() == true)
            {
                CustomVariable cust = (CustomVariable)Processor.getCustomForName(tmp.next());
                if (cust != null)
                {
                    possibleTypes.add(cust);
                }
            }
        }

        // sanity check that we have something defined
        //
        if (possibleTypes.size() == 0)
        {
            throw new RuntimeException("Unable to locate map definitions for "+getName());
        }
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isMap()
     */
    //--------------------------------------------
    @Override
    public boolean isMap()
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
     * @see com.icontrol.generate.service.variable.BaseVariable#getCtype()
     */
    //--------------------------------------------
    @Override
    public String getCtype()
    {
        // native uses the icHashMap
        //
        return "icHashMap *";
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        // make a hash for Java with a key of 'string' and an unknown value type
        //
        return "HashMap<String,Object>";
    }

    //--------------------------------------------
    /**
     * used by CustomVariable
     */
    //--------------------------------------------
    String getJavaTypesType()
    {
        return "HashMap<String,String>";
    }

    //--------------------------------------------
    /**
     * used by CustomVariable
     */
    //--------------------------------------------
    public String getValuesVarName()
    {
        return getName()+"ValuesMap";
    }

    //--------------------------------------------
    /**
     * used by CustomVariable
     */
    //--------------------------------------------
    public String getTypesVarName()
    {
        return getName()+"TypesMap";
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
        retVal.append(getValuesVarName()+" = new "+getJavaType()+"();\n");
        retVal.append(prefix);
        retVal.append(getTypesVarName()+" = new "+getJavaTypesType()+"();\n");

        return retVal.toString();
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
     * Returns the Java getter bean method for this type
     */
    //--------------------------------------------
    public StringBuffer getJavaGetter()
    {
        StringBuffer buff = new StringBuffer();

        // add an iterator for the keys
        //
        buff.append("\t/* helper function to return iterator of hash keys */\n");
        buff.append("\tpublic Iterator<String> get" + StringUtils.camelCase(getName()) + "KeyIterator()\n");
        buff.append("\t{\n");
        buff.append("\t\treturn "+getValuesVarName()+".keySet().iterator();\n");
        buff.append("\t}\n\n");

        // add a "key count"
        //
        buff.append("\t/* helper function to return number of hash items */\n");
        buff.append("\tpublic int get" + StringUtils.camelCase(getName()) + "Count()\n");
        buff.append("\t{\n");
        buff.append("\t\treturn "+getValuesVarName()+".size();\n");
        buff.append("\t}\n\n");

        // add a 'getter' for each possible type allowed in the map
        //
        Iterator<BaseVariable> loop = possibleTypes.iterator();
        while (loop.hasNext() == true)
        {
            // each should have a name that reflects the type,
            // but serves as a safety net
            //
            BaseVariable var = loop.next();
            if (var.getName() == null)
            {
                var.setName(var.getJavaType());
            }

            // use objects not primitives (i.e. boolean --> Boolean)
            //
            String type = var.getJavaType();
            String camelType = StringUtils.camelCase(type);
            if (type.equals("int") == true)
            {
                camelType = "Integer";
            }
            else if (var.isEnum() == true)
            {
                camelType = type;
            }

            buff.append("\tpublic "+camelType);
            buff.append(" get");
            buff.append(StringUtils.camelCase(var.getName()));
            buff.append("(String key)\n");
            buff.append("\t{\n");
            buff.append("\t\treturn (");
            buff.append(camelType+")"+getValuesVarName()+".get(key);\n");
            buff.append("\t}\n\n");
        }

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

        // add a 'setter' that corresponds to the 'getter' for each type
        //
        Iterator<BaseVariable> loop = possibleTypes.iterator();
        while (loop.hasNext() == true)
        {
            BaseVariable var = loop.next();

            // use objects not primitives (i.e. boolean --> Boolean)
            //
            String type = var.getJavaType();
            String camelType = StringUtils.camelCase(type);
            if (type.equals("int") == true)
            {
                camelType = "Integer";
            }
            else if (var.isEnum() == true)
            {
                camelType = type;
            }

            buff.append("\tpublic void set");
            buff.append(StringUtils.camelCase(var.getName()));
            buff.append("(String key, "+camelType+" val)\n");
            buff.append("\t{\n");
            buff.append("\t\t"+getValuesVarName()+".put(key, val);\n");
            buff.append("\t\t"+getTypesVarName()+".put(key, \""+var.getJavaType().toLowerCase()+"\");\n");
            buff.append("\t}\n\n");
        }

        return buff;
    }

    //--------------------------------------------
    /**
     * Returns the Native getter functions for this type
     */
    //--------------------------------------------
    public StringBuffer getNativeGetter(CustomVariable parent, ContextNative ctx, boolean isPrototype)
    {
        StringBuffer buff = new StringBuffer();

        // add a 'get count' function
        //
        buff.append("\n/*\n * return number of items in the hash map '"+getName()+"' contained in '"+parent.getName()+"'\n */\n");
        if (isPrototype == true)
        {
            buff.append("extern ");
        }
        buff.append("uint16_t get_count_of_"+parent.getName()+"_"+getName()+"("+parent.getCtype()+" *pojo)");
        if (isPrototype == true)
        {
            buff.append(";\n");
        }
        else
        {
            // the body
            //
            buff.append("\n{\n");
            buff.append("    return hashMapCount(pojo->"+getValuesVarName()+");\n");
            buff.append("}\n");
        }

        // add a 'getter' for each possible type allowed in the map
        //
        Iterator<BaseVariable> loop = possibleTypes.iterator();
        while (loop.hasNext() == true)
        {
            // each should have a name that reflects the type,
            // but serves as a safety net
            //
            BaseVariable var = loop.next();
            if (var.getName() == null)
            {
                // use java type so it correlates with the Java code generation
                //
                var.setName(var.getJavaType());
            }

            // get the type and the return type (pointer if applicable)
            //
            String type = var.getCtype();
            String returnType = var.getCtype();
            if (var.isPtr() == true)
            {
                returnType += " *";
            }
            else
            {
                returnType += " ";
            }

            // add a comment
            //
            buff.append("\n/*\n * extract value for 'key' of type '"+type+"' from the hash map '"+getName()+"' contained in '"+parent.getName());
            buff.append("'\n */\n");

            if (isPrototype)
            {
                buff.append("extern ");
            }

            // create the get function name, then save in parent.context
            //
            String function = "get_"+var.getName()+"_from_"+parent.getName()+"_"+getName();
            ctx.addGetFunction(getName(), function);
            buff.append(returnType);
            buff.append(function);

            // now the arguments
            //
            buff.append("("+parent.getCtype()+" *pojo, const char *key)");
            if (isPrototype)
            {
                buff.append(";\n");
            }
            else
            {
                // add the function body
                //
                buff.append("\n{\n");
                buff.append("   if (pojo != NULL && key != NULL)\n");
                buff.append("   {\n");
                buff.append("       "+type+" *val = hashMapGet(pojo->"+getValuesVarName()+", (void *)key, (uint16_t)strlen(key));\n");
                buff.append("       if (val != NULL)\n");
                buff.append("       {\n");
                if (var.isPtr() == true)
                {
                    buff.append("           return val;\n");
                }
                else
                {
                    buff.append("           return *val;\n");
                }
                buff.append("       }\n");
                buff.append("   }\n");
                if (var.isPtr() == true)
                {
                    buff.append("   return NULL;\n");
                }
                else
                {
                    buff.append("   return ("+type+")-1;\n");
                }
                buff.append("}\n\n");
            }
        }

        return buff;
    }

    //--------------------------------------------
    /**
     * Returns the Native setter functions for this type
     */
    //--------------------------------------------
    public StringBuffer getNativeSetter(CustomVariable parent, ContextNative ctx, boolean isPrototype)
    {
        StringBuffer buff = new StringBuffer();

        // add a 'setter' that corresponds to the 'getter' for each type
        //
        Iterator<BaseVariable> loop = possibleTypes.iterator();
        while (loop.hasNext() == true)
        {
            // each should have a name that reflects the type,
            // but serves as a safety net
            //
            BaseVariable var = loop.next();
            if (var.getName() == null)
            {
                // use java type so it correlates with the Java code generation
                //
                var.setName(var.getJavaType());
            }

            // get the type and the return type (pointer if applicable)
            //
            String type = var.getCtype();
            String inputType = type;
            String deref = null;
            if (var.isPtr() == true)
            {
                inputType += " *";
                deref = "";
            }
            else
            {
                inputType += " ";
                deref = "&";
            }

            // add a comment
            //
            buff.append("\n/*\n * set/add value for 'key' of type '"+type+"' into the hash map '"+getName()+"' contained in '"+parent.getName());
            buff.append("'\n * NOTE: will clone key & value to keep memory management of internal icHashMap simple");
            buff.append("\n */\n");

            if (isPrototype)
            {
                buff.append("extern ");
            }

            // create the get function name, then save in parent.context
            //
            String function = "put_"+var.getName()+"_in_"+parent.getName()+"_"+getName();
            ctx.addSetFunction(var.getName(), function);
            buff.append("void ");
            buff.append(function);

            // now the arguments
            //
            buff.append("("+parent.getCtype()+" *pojo, const char *key, "+inputType+"value)");
            if (isPrototype)
            {
                buff.append(";\n");
            }
            else
            {
                // add the function body
                //
                buff.append("\n{\n");
                buff.append("   if (pojo != NULL && key != NULL && value != NULL)\n");
                buff.append("   {\n");
                buff.append("       // create copy of 'key' and the supplied 'value' into the map.\n");
                buff.append("       // they will be released when our 'destroy_"+parent.getName()+" is called\n");
                buff.append("       //\n");
                buff.append("       uint16_t keyLen = (uint16_t)strlen(key);\n\n");
                // For strings we want a copy of the value
                if (var.isString())
                {
                    buff.append("       hashMapPut(pojo->"+getValuesVarName()+", (void *)strdup(key), keyLen, strdup(value));\n");
                }
                else
                {
                    buff.append("       hashMapPut(pojo->"+getValuesVarName()+", (void *)strdup(key), keyLen, value);\n");
                }

                // add the 'type', yes it should use the "java" data type so that we keep them in sync
                buff.append("       hashMapPut(pojo->"+getTypesVarName()+", (void *)strdup(key), keyLen, strdup(\""+var.getJavaType().toLowerCase()+"\"));\n");
                buff.append("   }\n");
                buff.append("}\n\n");
            }
        }

        return buff;
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
        String prefix = getIndentBuffer(indent+2);
        String prefix2 = getIndentBuffer(indent+3);
        String prefix3 = getIndentBuffer(indent+4);

        String valueContainer = getName()+"Vals_json";

        // use a template for as much as we can
        //
        MacroSource ms = new MacroSource();
        ms.addMapping("name", getName());
        ms.addMapping("parent_container", parentContainerName);
        ms.addMapping("parent", varName);
        ms.addMapping("pad", getIndentBuffer(indent));

        // iterate through each type to properly encode
        //
        StringBuffer tree = new StringBuffer("\n");
        tree.append(prefix+"// convert for each possible 'type'\n");
        Iterator<BaseVariable> loop = possibleTypes.iterator();
        boolean first = true;
        while (loop.hasNext() == true)
        {
            BaseVariable var = loop.next();

            // use objects not primitives (i.e. boolean --> Boolean)
            //
            String realType = var.getCtype();

            tree.append(prefix);
            if (first == true)
            {
                tree.append("if");
                first = false;
            }
            else
            {
                tree.append("else if");
            }
            tree.append(" (strcmp(kind, \"");
            tree.append(var.getJavaType().toLowerCase());
            tree.append("\") == 0)\n");
            tree.append(prefix+"{\n");

            // can't use the 'getNativeEncode' from 'var' because it wants to quote 'key'
            // ex:   cJSON_AddStringToObject(barValsMap, "mapKey", (char)mapVal);
            //
            if (var.isCustom() == true)
            {
                // call the encode function
                //
                CustomVariable custom = (CustomVariable)var;
                tree.append(prefix2+custom.getCtype()+" *c = ("+custom.getCtype()+" *)mapVal;\n");

                // look for pojo wrapper to see if we can just call this objects 'encode' function
                //
                PojoWrapper wrapper = Processor.getPojoProcessor().getPojoForName(custom.getName());
                if (wrapper != null && StringUtils.isEmpty(wrapper.getContextNative().getEncodeFunctionName()) == false)
                {
                    // perform encode
                    //
                    String function = wrapper.getContextNative().getEncodeFunctionName();
                    tree.append(prefix2+"cJSON *"+custom.getName()+"_json = "+function+"(c);\n");

                    // add to the array
                    //
                    tree.append(prefix2+"cJSON_AddItemToObject("+valueContainer+", mapKey, "+custom.getName()+"_json);\n");
                }
            }
            else if (var.isArray() == true)
            {
                tree.append("// TODO: array\n");

            }
            else if (var.isNumber() == true)
            {
                // use the correct type for the cast, then save in JSON
                String cType = var.getCtype();
                tree.append(prefix2+cType+" *n = ("+cType+" *)mapVal;\n");
                tree.append(prefix2+"cJSON_AddNumberToObject("+valueContainer+", mapKey, *n);\n");
            }
            else if (var.isEnum())
            {
                // use 'int' for the cast, then save in JSON
                tree.append(prefix2+"int *n = (int *)mapVal;\n");
                tree.append(prefix2+"cJSON_AddNumberToObject("+valueContainer+", mapKey, *n);\n");
            }
            else if (var.isDate() == true)
            {
                tree.append(prefix2+"uint64_t *t = (uint64_t *)mapVal;\n");
                tree.append(prefix2+"cJSON_AddNumberToObject("+valueContainer+", mapKey, *t);\n");
            }
            else if (var.isString() == true)
            {
                tree.append(prefix2+"char *c = (char *)mapVal;\n");
                tree.append(prefix2+"cJSON_AddStringToObject("+valueContainer+", mapKey, c);\n");
            }
            else if (var.isBoolean() == true)
            {
                tree.append(prefix2+"bool *flag = (bool *)mapVal;\n");
                tree.append(prefix2+"if (*flag == true)\n");
                tree.append(prefix3+"cJSON_AddTrueToObject("+valueContainer+", mapKey);\n");
                tree.append(prefix2+"else\n");
                tree.append(prefix3+"cJSON_AddFalseToObject("+valueContainer+", mapKey);\n");
            }
            else
            {
                tree.append("// TODO: not yet implemented - "+var.getCtype()+"\n");
//                tree.append(var.getNativeEncodeLines(ctx, indent+3, getName()+"Vals_json", false, "("+realType+")"+deref+"mapVal", "mapKey"));
            }
            tree.append(prefix+"}\n");
        }
        ms.addMapping("check_each_type", tree.toString());

        // translate using the template
        //
        try
        {
            buff.append(FileHelper.translateBufferTemplate(NATIVE_ENCODE_TEMPLATE, ms));
        }
        catch (IOException oops)
        {
            throw new RuntimeException(oops.getMessage(), oops);
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
        String prefix = getIndentBuffer(indent+2);
        String prefix2 = getIndentBuffer(indent+3);
        String prefix3 = getIndentBuffer(indent+4);

        String valueContainer = getName()+"Vals_json";

        // use a template for as much as we can
        //
        MacroSource ms = new MacroSource();
        ms.addMapping("name", getName());
        ms.addMapping("parent_container", parentContainerName);
        ms.addMapping("parent", varName);
        ms.addMapping("pad", getIndentBuffer(indent));

        // iterate through each type to properly encode
        //
        StringBuffer tree = new StringBuffer("\n");
        tree.append(prefix+"// convert for each possible 'type'\n");
        Iterator<BaseVariable> loop = possibleTypes.iterator();
        boolean first = true;
        while (loop.hasNext() == true)
        {
            BaseVariable var = loop.next();

            // use objects not primitives (i.e. boolean --> Boolean)
            //
            String realName = var.getName();
//            if (var.isEnum() == true)
//            {
//                EnumVariable ev = (EnumVariable)var;
//                realName = ev.getVarName();
//            }
            String realType = var.getCtype();

            tree.append(prefix);
            if (first == true)
            {
                tree.append("if");
                first = false;
            }
            else
            {
                tree.append("else if");
            }
            tree.append(" (strcmp(type->valuestring, \"");
            tree.append(var.getJavaType().toLowerCase());
            tree.append("\") == 0)\n");
            tree.append(prefix+"{\n");

            // all of these rely on the 'setter' function for this variable type
            //
            PojoWrapper wrapper = Processor.getPojoProcessor().getPojoForName(realName);

            // get the 'setter' so we know how to store the decoded version of this sub-variable
            //
            String setFunction = ctx.getSetFunctionForVar(realName);
            if (setFunction == null)
            {
                throw new RuntimeException("Unable to find 'set function' for "+realName);
            }

            // can't use the 'getNativeDecode' from 'var' because it wants to quote 'key'
            // ex:   cJSON_AddStringToObject(barValsMap, "mapKey", (char)mapVal);
            //
            if (var.isCustom() == true || var.isArray() == true)
            {
                // create an instance, then call the 'decode' function for it
                //
                tree.append(prefix2+var.getCtype()+" *c = NULL;\n");

                // get for pojo wrapper to extract the create and decode function
                //
                if (wrapper != null)
                {
                    // create then decode
                    //
                    tree.append(prefix2+"// allocate local var\n");
                    if (wrapper.getContextNative().getCreateFunctionName() != null)
                    {
                        tree.append(prefix2+"c = "+wrapper.getContextNative().getCreateFunctionName()+"();\n");
                    }
                    else
                    {
                        // no constructor, malloc
                        tree.append(prefix2+"c = ("+realType+" *)malloc(sizeof("+realType+"));\n");
                        tree.append(prefix2+"memset(c, 0, sizeof("+realType+"));\n");
                    }
                    tree.append(prefix2+"// save type/value in hash\n");
                    String decode = wrapper.getContextNative().getDecodeFunctionName();
                    tree.append(prefix2+decode+"(c, item);\n");
                }

                // now the 'setter'
                //
                tree.append(prefix2+setFunction+"(pojo, key->valuestring, c);\n");
/**
                if (wrapper != null)
                {
                    // free since the 'set' should copy the data
                    //
                    tree.append(prefix2+"// free local var\n");
                    if (wrapper.getContextNative().getDestroyFunctionName() != null)
                    {
                        tree.append(prefix2+wrapper.getContextNative().getDestroyFunctionName()+"(c);\n");
                    }
                    else
                    {
                        tree.append(prefix2+"free(c);\n");
                    }
                }
 **/
            }
            else if (var.isNumber() == true)
            {
                if (var.isShort() || var.isInt())
                {
                    // if short or int, use 'valueint'
                    tree.append(prefix2+setFunction+"(pojo, key->valuestring, ("+realType+")item->valueint);\n");
                }
                else
                {
                    // long, float, double should use 'double' to ensure it has enough space
                    tree.append(prefix2+setFunction+"(pojo, key->valuestring, ("+realType+")item->valuedouble);\n");
                }
            }
            else if (var.isString() == true)
            {
                tree.append(prefix2+setFunction+"(pojo, key->valuestring, ("+realType+" *)item->valuestring);\n");
            }
            else if (var.isBoolean() == true)
            {
                tree.append(prefix2+setFunction+"(pojo, key->valuestring, (item->type == cJSON_True) ? true : false);\n");
            }
            else if (var.isEnum() == true)
            {
                tree.append(prefix2+setFunction+"(pojo, key->valuestring, ("+realType+")item->valueint);\n");
            }

            else if (var.isDate() == true)
            {
                tree.append(prefix2+setFunction+"(pojo, key->valuestring, ("+realType+")item->valuedouble);\n");
            }
            else
            {
                tree.append("// TODO: not yet implemented - "+realType+"\n");
//                tree.append(var.getNativeEncodeLines(ctx, indent+3, getName()+"Vals_json", false, "("+realType+")"+deref+"mapVal", "mapKey"));
            }

            tree.append(prefix+"}\n");
        }
        ms.addMapping("check_each_type", tree.toString());

        // translate using the template
        //
        try
        {
            buff.append(FileHelper.translateBufferTemplate(NATIVE_DECODE_TEMPLATE, ms));
        }
        catch (IOException oops)
        {
            throw new RuntimeException(oops.getMessage(), oops);
        }

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
        String prefix = getIndentBuffer(indent+3);
        String prefix2 = getIndentBuffer(indent+4);

        // use a template for as much as we can
        //
        MacroSource ms = new MacroSource();
        ms.addMapping("name", getName());
        ms.addMapping("parent_container", parentContainerName);
        ms.addMapping("pad", getIndentBuffer(indent));

        // iterate through each type to properly encode
        //
        StringBuffer tree = new StringBuffer("\n");
        tree.append(prefix+"// convert for each possible 'type'\n");
        Iterator<BaseVariable> loop = possibleTypes.iterator();
        boolean first = true;
        while (loop.hasNext() == true)
        {
            BaseVariable var = loop.next();

            // use objects not primitives (i.e. boolean --> Boolean)
            //
            String type = var.getJavaType();
            String camelType = StringUtils.camelCase(type);
            if (type.equals("int") == true)
            {
                camelType = "Integer";
            }
            else if (var.isEnum() == true)
            {
                camelType = type;
            }

            tree.append(prefix);
            if (first == true)
            {
                tree.append("if");
                first = false;
            }
            else
            {
                tree.append("else if");
            }
            tree.append(" (kind.equals(\"");
            tree.append(var.getJavaType().toLowerCase());
            tree.append("\") == true)\n");
            tree.append(prefix+"{\n");

            // can't use the 'getJavaEncode' from 'var' because it wants to quote 'key'
            // ex:   barVals_json.put("key", (long)val);
            //
            if (var.isDate() == true)
            {
                tree.append(prefix2+getName()+"Vals_json.put(key, (("+camelType+")val).getTime());\n");
            }
            else if (var.isCustom() == true)
            {
                tree.append(prefix2+camelType+" "+getName()+"_tmp = ("+camelType+")val;\n");
                tree.append(prefix2+getName()+"Vals_json.put(key, "+getName()+"_tmp.toJSON());\n");

            }
            else if (var.isEnum() == true)
            {
                tree.append(prefix2+getName()+"Vals_json.put(key, (("+var.getJavaType()+")val).getNumValue());\n");
            }
            else
            {
                // just typecast and insert
                //
                tree.append(prefix2+getName()+"Vals_json.put(key, ("+camelType+")val);\n");
            }
            tree.append(prefix+"}\n");
        }
        ms.addMapping("check_each_type", tree.toString());

        // translate using the template
        //
        try
        {
            buff.append(FileHelper.translateBufferTemplate(JAVA_ENCODE_TEMPLATE, ms));
        }
        catch (IOException oops)
        {
            throw new RuntimeException(oops.getMessage(), oops);
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
        String prefix = getIndentBuffer(indent+2);
        String prefix2 = getIndentBuffer(indent+3);
        String prefix3 = getIndentBuffer(indent+4);

        // use a template for as much as we can
        //
        MacroSource ms = new MacroSource();
        ms.addMapping("name", getName());
        ms.addMapping("parent_container", parentContainerName);
        ms.addMapping("pad", getIndentBuffer(indent));

        // iterate through each type to properly encode
        //
        StringBuffer tree = new StringBuffer("\n");
        tree.append(prefix+"// convert for each possible 'type'\n");
        Iterator<BaseVariable> loop = possibleTypes.iterator();
        boolean first = true;
        while (loop.hasNext() == true)
        {
            BaseVariable var = loop.next();

            // use objects not primitives (i.e. boolean --> Boolean)
            //
            String type = var.getJavaType();
            String camelType = StringUtils.camelCase(type);
            if (type.equals("int") == true)
            {
                camelType = "Integer";
            }
            else if (var.isEnum() == true)
            {
                camelType = type;
            }

            tree.append(prefix);
            if (first == true)
            {
                tree.append("if");
                first = false;
            }
            else
            {
                tree.append("else if");
            }
            tree.append(" (type.equals(\"");
            tree.append(var.getJavaType().toLowerCase());
            tree.append("\") == true)\n");
            tree.append(prefix+"{\n");

// don't use, was here as a reference
//            tree.append(var.getJavaDecodeLines(1, getName()+"Vals_json", false, var.getName(), "key"));

            // can't use the 'getJavaDecode' from 'var' because it wants to quote 'key'
            // ex:   barVals_json.optBoolean("key");
            //
            if (var.isCustom() == true)
            {
                tree.append(prefix2+"JSONObject o = "+getName()+"Vals_json.optJSONObject(key);\n");
                tree.append(prefix2+"if (o != null)\n");
                tree.append(prefix2+"{\n");
                tree.append(prefix3+camelType+" "+getName()+"_tmp = new "+camelType+"();\n");
                tree.append(prefix3+getName()+"_tmp.fromJSON(o);\n");
                tree.append(prefix3+"set"+StringUtils.camelCase(var.getName())+"(key, "+getName()+"_tmp);\n");
                tree.append(prefix2+"}\n");
            }
            else if (var.isBoolean() == true)
            {
                tree.append(prefix2+"int b = "+getName()+"Vals_json.optInt(key);\n");
                tree.append(prefix2+"if (b == 1)\n");
                tree.append(prefix3+"set"+StringUtils.camelCase(var.getName())+"(key, true);\n");
                tree.append(prefix2+"else\n");
                tree.append(prefix3+"set"+StringUtils.camelCase(var.getName())+"(key, false);\n");
            }
            else if (var.isEnum() == true)
            {
                tree.append(prefix2+"int d = "+getName()+"Vals_json.optInt(key);\n");
                tree.append(prefix2+"if (d >= 0) set"+StringUtils.camelCase(var.getName())+"(key, "+camelType+".enumForInt(d));\n");
            }
            else if (var.isDate() == true)
            {
                tree.append(prefix2+"long d = "+getName()+"Vals_json.optLong(key);\n");
                tree.append(prefix2+"if (d > 0) set"+StringUtils.camelCase(var.getName())+"(key, new Date(d));\n");
            }
            else if (var.isInt() == true)
            {
                tree.append(prefix2+"int s = "+getName()+"Vals_json.optInt(key);\n");
                tree.append(prefix2+"set"+StringUtils.camelCase(var.getName())+"(key, s);\n");
            }
            else if (var.isNumber() == true)
            {
                tree.append(prefix2+camelType+" s = "+getName()+"Vals_json.opt"+camelType+"(key);\n");
                tree.append(prefix2+"set"+StringUtils.camelCase(var.getName())+"(key, s);\n");
            }
            else
            {
                tree.append(prefix2+camelType+" s = "+getName()+"Vals_json.opt"+camelType+"(key);\n");
                tree.append(prefix2+"if (s != null) set"+StringUtils.camelCase(var.getName())+"(key, s);\n");
            }

            tree.append(prefix+"}\n");
        }
        ms.addMapping("check_each_type", tree.toString());

        // translate using the template
        //
        try
        {
            buff.append(FileHelper.translateBufferTemplate(JAVA_DECODE_TEMPLATE, ms));
        }
        catch (IOException oops)
        {
            throw new RuntimeException(oops.getMessage(), oops);
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
        // only used as input/output parameters in IPC Messages
        // that is NOT SUPPORTED for MapVariable, so return null
        //
        return null;
    }
}

//+++++++++++
//EOF
//+++++++++++
