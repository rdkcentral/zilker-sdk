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
import java.util.HashSet;
import java.util.List;

import com.icontrol.generate.service.TemplateMappings;
import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.generate.service.io.CodeFormater;
import com.icontrol.generate.service.io.MacroSource;
import com.icontrol.generate.service.parse.EventWrapper;
import com.icontrol.generate.service.parse.PojoWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.Wrapper;
import com.icontrol.util.StringUtils;
import com.icontrol.xmlns.service.ICArrayVariable;
import com.icontrol.xmlns.service.ICBoolVariable;
import com.icontrol.xmlns.service.ICCustomObject;
import com.icontrol.xmlns.service.ICDateVariable;
import com.icontrol.xmlns.service.ICEnumVariable;
import com.icontrol.xmlns.service.ICJsonVariable;
import com.icontrol.xmlns.service.ICMapVariable;
import com.icontrol.xmlns.service.ICNumVariable;
import com.icontrol.xmlns.service.ICStringVariable;

//-----------------------------------------------------------------------------
// Class definition:    CustomVariable
/**
 *  Custom object defined in XML
 *
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class CustomVariable extends BaseVariable
{
    private ArrayList<BaseVariable> fields;
    private String   localName;             // to support a custom within an event or another custom
    private String   parentClassName;		// only applicable for Java generation
    private boolean  parentClassAbstract;	// only applicable for Java generation
    private boolean	 cloneable;
    private String 	 interfaceString;		// only applicable for Java generation

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public CustomVariable(ICCustomObject bean)
    {
        super(bean);
        fields = new ArrayList<BaseVariable>();
        parentClassName = bean.getParentClass();
        parentClassAbstract = bean.getParentAbstract();
        cloneable = bean.getCloneable();

        interfaceString = "";
        for (String additionalInterface : bean.getAdditionalInterfaceList())
        {
//            if (interfaceString.length() > 0)
//            {
//                interfaceString += ", ";
//            }
            interfaceString += ", ";
            interfaceString += additionalInterface;
        }
        // get list of BaseVariables for this custom object
        //
        List<ICStringVariable> stringList = bean.getStringVariableList();
        for (int i = 0 ; i < stringList.size() ; i++)
        {
            BaseVariable impl = Processor.getOrCreateVariable(stringList.get(i));
            if (impl != null)
            {
                fields.add(impl);
            }
        }

        List<ICNumVariable> numList = bean.getNumVariableList();
        for (int i = 0 ; i < numList.size() ; i++)
        {
            BaseVariable impl = Processor.getOrCreateVariable(numList.get(i));
            if (impl != null)
            {
                fields.add(impl);
            }
        }

        List<ICBoolVariable> boolList = bean.getBoolVariableList();
        for (int i = 0 ; i < boolList.size() ; i++)
        {
            BaseVariable impl = Processor.getOrCreateVariable(boolList.get(i));
            if (impl != null)
            {
                fields.add(impl);
            }
        }

        List<ICDateVariable> dateList = bean.getDateVariableList();
        for (int i = 0 ; i < dateList.size() ; i++)
        {
            BaseVariable impl = Processor.getOrCreateVariable(dateList.get(i));
            if (impl != null)
            {
                fields.add(impl);
            }
        }

        List<ICArrayVariable> arrayList = bean.getArrayVariableList();
        for (int i = 0 ; i < arrayList.size() ; i++)
        {
            BaseVariable impl = Processor.getOrCreateVariable(arrayList.get(i));
            if (impl != null)
            {
                fields.add(impl);
            }
        }

        List<ICMapVariable> mapList = bean.getMapVariableList();
        for (int i = 0 ; i < mapList.size() ; i++)
        {
            BaseVariable impl = Processor.getOrCreateVariable(mapList.get(i));
            if (impl != null)
            {
                fields.add(impl);
            }
        }

        List<ICEnumVariable> enumList = bean.getEnumVariableList();
        for (int i = 0 ; i < enumList.size() ; i++)
        {
            BaseVariable impl = Processor.getOrCreateVariable(enumList.get(i));
            if (impl != null)
            {
                fields.add(impl);
            }
        }

        List<ICJsonVariable> jsonList = bean.getJsonVariableList();
        for (int i = 0 ; i < jsonList.size() ; i++)
        {
            BaseVariable impl = Processor.getOrCreateVariable(jsonList.get(i));
            if (impl != null)
            {
                fields.add(impl);
            }
        }

        List<ICCustomObject.CustomRef> subCustList = bean.getCustomRefList();
        for (int i = 0 ; i < subCustList.size() ; i++)
        {
            ICCustomObject.CustomRef ref = subCustList.get(i);
            CustomVariable impl = Processor.getCustomForName(ref.getCustomObj());
            if (impl != null)
            {
                // if this has a local name defined, shallow-clone the original
                // so we can alter the name to use when producing code
                //
                if (StringUtils.isEmpty(ref.getLocalName()) == false)
                {
                    CustomVariable copy = impl.shallowClone();
                    copy.localName = ref.getLocalName();
                    impl = copy;
                }

                fields.add(impl);
            }
        }
    }

    //--------------------------------------------
    /**
     * Constructor when making manually
     */
    //--------------------------------------------
    public CustomVariable(String varName)
    {
        super(varName);
        fields = new ArrayList<BaseVariable>();
    }

    //--------------------------------------------
    /**
     * Used when CustomVariable is owned by another Custom or Event
     */
    //--------------------------------------------
    private CustomVariable shallowClone()
    {
        if (node != null)
        {
            return new CustomVariable((ICCustomObject)node);
        }
        else
        {
            return new CustomVariable(name);
        }
    }

    //--------------------------------------------
    /**
     * @return Returns the customName.
     */
    //--------------------------------------------
    public String getLocalName()
    {
        // if one is set, return that; otherwise use default varName
        //
        if (localName != null)
        {
            return localName;
        }

        return getName();
    }

    //--------------------------------------------
    /**
     * @return Returns the parentClassName.
     */
    //--------------------------------------------
    public String getParentClassName()
    {
        return parentClassName;
    }

    void setParentClassName(String parentCName, boolean isAbstract)
    {
        parentClassName = parentCName;
        parentClassAbstract = isAbstract;
    }

    //--------------------------------------------
    /**
     * @return Returns the parentClassAbstract.
     */
    //--------------------------------------------
    public boolean isParentClassAbstract()
    {
        return parentClassAbstract;
    }

    //--------------------------------------------
    /**
     * @return Returns the 'clonable' flag
     */
    //--------------------------------------------
    public boolean isCloneable()
    {
        return cloneable;
    }

    void setCloneable(boolean flag)
    {
    	cloneable = flag;
    }

    //--------------------------------------------
    /**
     * Adds a BaseVariable to this CustomVariable
     */
    //--------------------------------------------
    public void addVariable(BaseVariable var)
    {
        if (var != null && fields.contains(var) == false)
        {
            fields.add(var);
        }
    }

    //--------------------------------------------
    /**
     * @return Returns the fields.
     */
    //--------------------------------------------
    public ArrayList<BaseVariable> getVariables()
    {
        return fields;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#resolveReferences()
     */
    //--------------------------------------------
    @Override
    public void resolveReferences()
    {
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isCustom()
     */
    //--------------------------------------------
    @Override
    public boolean isCustom()
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
     * return true if one item within this custom is an Array
     */
    //--------------------------------------------
    public boolean doesCustomHaveArray(boolean recursive)
    {
        // loop through our fields to see if any are an array
        //
        for (int i = 0, count = fields.size() ; i < count ; i++)
        {
            BaseVariable curr = fields.get(i);
            if (curr.isArray() == true)
            {
                return true;
            }
            else if (curr.isCustom() == true && recursive == true)
            {
                // custom, so may have a field that's an array
                //
                CustomVariable cust = (CustomVariable)curr;
                if (cust.doesCustomHaveArray(recursive) == true)
                {
                    return true;
                }
            }
        }

        return false;
    }

    //--------------------------------------------
    /**
     * return all 'array' vars within this custom
     */
    //--------------------------------------------
    public ArrayList<ArrayVariable> getArrayVariables(boolean recursive)
    {
        ArrayList<ArrayVariable> retVal = new ArrayList<ArrayVariable>();

        // loop through our fields to see if any are an array
        //
        for (int i = 0, count = fields.size() ; i < count ; i++)
        {
            BaseVariable curr = fields.get(i);
            if (curr.isArray() == true)
            {
                retVal.add((ArrayVariable)curr);
            }
            else if (curr.isCustom() == true && recursive == true)
            {
                // custom, so may have a field that's an array
                //
                CustomVariable cust = (CustomVariable)curr;
                ArrayList<ArrayVariable> tmp = cust.getArrayVariables(recursive);
                retVal.addAll(tmp);
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * return true if one item within this custom is a Map
     */
    //--------------------------------------------
    public boolean doesCustomHaveMap(boolean recursive)
    {
        // loop through our fields to see if any are an array
        //
        for (int i = 0, count = fields.size() ; i < count ; i++)
        {
            BaseVariable curr = fields.get(i);
            if (curr.isMap() == true)
            {
                return true;
            }
            else if (curr.isCustom() == true && recursive == true)
            {
                // custom, so may have a field that's an array
                //
                CustomVariable cust = (CustomVariable)curr;
                if (cust.doesCustomHaveMap(recursive) == true)
                {
                    return true;
                }
            }
        }

        return false;
    }

    //--------------------------------------------
    /**
     * return all 'map' vars within this custom
     */
    //--------------------------------------------
    public ArrayList<MapVariable> getMapVariables(boolean recursive)
    {
        ArrayList<MapVariable> retVal = new ArrayList<MapVariable>();

        // loop through our fields to see if any are an array
        //
        for (int i = 0, count = fields.size() ; i < count ; i++)
        {
            BaseVariable curr = fields.get(i);
            if (curr.isMap() == true)
            {
                retVal.add((MapVariable)curr);
            }
            else if (curr.isCustom() == true && recursive == true)
            {
                // custom, so may have a field that's an array
                //
                CustomVariable cust = (CustomVariable)curr;
                ArrayList<MapVariable> tmp = cust.getMapVariables(recursive);
                retVal.addAll(tmp);
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * return all 'string' vars within this custom
     */
    //--------------------------------------------
    public ArrayList<StringVariable> getStringVariables()
    {
        ArrayList<StringVariable> retVal = new ArrayList<StringVariable>();

        // loop through our fields to see if any are an array
        //
        for (int i = 0, count = fields.size() ; i < count ; i++)
        {
            BaseVariable curr = fields.get(i);
            if (curr.isString() == true)
            {
                retVal.add((StringVariable)curr);
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * return all 'json' vars within this custom
     */
    //--------------------------------------------
    public ArrayList<JsonVariable> getJsonVariables()
    {
        ArrayList<JsonVariable> retVal = new ArrayList<JsonVariable>();

        // loop through our fields to see if any are an array
        //
        for (int i = 0, count = fields.size() ; i < count ; i++)
        {
            BaseVariable curr = fields.get(i);
            if (curr instanceof JsonVariable)
            {
                retVal.add((JsonVariable)curr);
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * return all 'custom' vars within this custom
     */
    //--------------------------------------------
    public ArrayList<CustomVariable> getCustomVariables()
    {
        ArrayList<CustomVariable> retVal = new ArrayList<CustomVariable>();

        // loop through our fields to see if any are an array
        //
        for (int i = 0, count = fields.size() ; i < count ; i++)
        {
            BaseVariable curr = fields.get(i);
            if (curr.isCustom() == true)
            {
                retVal.add((CustomVariable)curr);
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * Return true if any of the elements of this CustomObject
     * are tagged as "event" (meaning should be in the event package)
     */
    //--------------------------------------------
//    public boolean doesCustomHaveEventDependency(Tracker tracker)
//    {
//        boolean includeEvents = false;
//        Iterator<BaseVariable> internals = fields.iterator();
//        while (internals.hasNext() == true)
//        {
//            BaseVariable base = internals.next();
//            if (base.isPossiblyEvent() == true)
//            {
//                if (tracker.isVariableReferencedByEvents(base.getName()) == true)
//                {
//                    // got dependency on event
//                    //
//                    return true;
//                }
//            }
//
//            if (base.isEnum() == true)
//            {
//                EnumVariable enumVar = (EnumVariable)base;
//                if (tracker.isVariableReferencedByEvents(enumVar.getVarName()) == true)
//                {
//                    return true;
//                }
//            }
//            else if (base.isArray() == true)
//            {
//                ArrayVariable arrayVar = (ArrayVariable)base;
//                BaseVariable first = arrayVar.getFirstElement();
//                if (tracker.isVariableReferencedByEvents(first.getName()) == true)
//                {
//                    return true;
//                }
//
//                if (first.isEnum() == true)
//                {
//                    EnumVariable enumVar = (EnumVariable)first;
//                    if (tracker.isVariableReferencedByEvents(enumVar.getVarName()) == true)
//                    {
//                        return true;
//                    }
//                }
//            }
//            else if (base.isCustom() == true)
//            {
//                CustomVariable cust = (CustomVariable)base;
//                if (cust.doesCustomHaveEventDependency(tracker) == true)
//                {
//                    return true;
//                }
//            }
//        }
//
//        return false;
//    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getCtype()
     */
    //--------------------------------------------
    @Override
    public String getCtype()
    {
        return getName();
    }

    public String getInterfaceString()
    {
        return interfaceString;
    }

    //--------------------------------------------
    /**
     * Return the C code that goes into the .h file
     * (the struct definition of this field)
     */
    //--------------------------------------------
    public StringBuffer getNativeStructString()
    {
        StringBuffer buff = new StringBuffer();
        CodeFormater table = new CodeFormater(4);

        String comment = getDescription();
        if (StringUtils.isEmpty(comment) == false)
        {
            // add the description of the enumeration above the 'typedef'
            //
            buff.append("// "+comment);
            buff.append('\n');
        }

        buff.append("typedef struct _");
        buff.append(getName());
        buff.append(" {\n");

        // kinda hacky, but if this is an Event, add the BaseEvent
        // Java already does this via inheritance
        //
        if (isEvent() == true)
        {
            table.nextRow();
            table.setValue("  ", 0);
            table.setValue("BaseEvent", 1);
            table.setValue("baseEvent;", 2);
            table.setValue(" // pojo base, event id, code, val, time", 3);
        }
        else
        {
            table.nextRow();
            table.setValue("  ", 0);
            table.setValue("Pojo", 1);
            table.setValue("base;", 2);
            table.setValue(" // Base object; use pojo.h interface", 3);
        }

        for (int i = 0 ; i < fields.size() ; i++)
        {
            BaseVariable curr = fields.get(i);
            table.nextRow();

            // map needs 2 variables, so take care of that up front
            //
            if (curr.isMap() == true)
            {
                MapVariable map = (MapVariable)curr;

                // first the 'values' variable
                //
                String type = map.getCtype();
                table.setValue("   ", 0);
                table.setValue(type, 1);
                table.setValue(map.getValuesVarName()+";", 2);

                // append comment
                //
                String additive = "two internal variables to represent "+map.getName();
                comment = map.getDescription();
                if (StringUtils.isEmpty(comment) == false)
                {
                    table.setValue(" // "+comment+" ("+additive+")", 3);
                }
                else
                {
                    table.setValue(" // "+additive, 3);
                }

                // next, the 'types' variable
                //
                table.nextRow();
                table.setValue("   ", 0);
                table.setValue(type, 1);
                table.setValue(map.getTypesVarName()+";", 2);

                continue;
            }

            // add type + name
            //
            String val = curr.getCtype();
            table.setValue("   ", 0);
            table.setValue(val, 1);

            String id = curr.getName();
            if (curr.isCustom() == true)
            {
                // custom/event may have local name to differentiate
                // (ex:  foo vs FooObj)
                //
                CustomVariable cust = (CustomVariable)curr;
                id = cust.getLocalName();
            }

            if (curr.isPtr() == true) // && curr.isArray() == false)
            {
                // make pointer
                //
                id = "*"+id;
            }

            if (curr.isEnum() == true)
            {
                EnumVariable ef = (EnumVariable)curr;
                id = ef.getVarName();
            }
            else if (curr.isArray() == true)
            {
                // add comment to the array since it will be implemented via icLinkedList
                // (which doesn't support templates and needs a description of what is inside)
                //
                ArrayVariable array = (ArrayVariable)curr;
                if (StringUtils.isEmpty(curr.getDescription()) == true)
                {
                    curr.setDescription("dynamic list of "+array.getFirstElement().getCtype()+" values (representing "+
                                        array.getFirstElement().getName()+")");
                }
            }
            table.setValue(id+";", 2);

            // lastly, add comment if defined
            //
            comment = curr.getDescription();
            if (StringUtils.isEmpty(comment) == false)
            {
                table.setValue(" // "+comment, 3);
            }
            else
            {
                table.setValue("", 3);
            }
        }

        buff.append(table.toStringBuffer(0));
        buff.append("} ");
        buff.append(getName());
        buff.append(";\n");

        return buff;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaType()
     */
    //--------------------------------------------
    @Override
    public String getJavaType()
    {
        // camel case our name
        //
        return StringUtils.camelCase(getName());
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJsonKey()
     */
    //--------------------------------------------
    @Override
    public String getJsonKey()
    {
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

        buff.append("\tpublic ");
        buff.append(getJavaType());
        buff.append(" get");
        buff.append(StringUtils.camelCase(getName()));
        buff.append("()\n");
        buff.append("\t{\n");
        buff.append("\t\treturn ");
        buff.append(getLocalName());
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
        buff.append(getLocalName());
        buff.append(" = val;\n");
        buff.append("\t}\n\n");

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
        String prefix = getIndentBuffer(indent);

        // should output something like this:
        //  test1
        //	cJSON *test1_json = NULL;
        //	test1_json = cJSON_CreateObject();
        //	cJSON_AddNumberToObject(test1_json, "arg1", test1->arg1);
        //	cJSON_AddNumberToObject(test1_json, "arg2", test1->arg2);
        //	cJSON_AddStringToObject(test1_json, "str", test1->str);
        //	cJSON_AddItemToObject(root, "test1", test1_json);
        //

        // add the json object we'll stuff our elements into
        //
        String container = getName()+"_json";
        buff.append('\n');
        buff.append(prefix+"// create JSON object for "+getName()+"\n");
        buff.append(prefix+"cJSON *"+container+" = NULL;\n");
        buff.append(prefix+container+" = cJSON_CreateObject();\n");

        // now each field within this object
        //
        for (int i = 0 ; i < fields.size() ; i++)
        {
            BaseVariable curr = fields.get(i);
            String currName = curr.getName();
            if (curr.isEnum() == true)
            {
                // enum uses different 'name'
                EnumVariable ef = (EnumVariable)curr;
                currName = ef.getVarName();
            }
            else if (curr.isCustom() == true)
            {
                CustomVariable cust = (CustomVariable)curr;
                currName = cust.getLocalName();
            }

            StringBuffer lines = null;
            if (curr.isDate() == true)
            {
                DateVariable date = (DateVariable)curr;
                String timeEquation;
                // TODO why is this casting to double?
                timeEquation = "(double)" + varName + "->" + currName;
                lines = curr.getNativeEncodeLines(ctx, indent, container, false, timeEquation, currName);
            }
            else if (curr.isMap() == true)
            {
                // pass the container name
                //
                lines = curr.getNativeEncodeLines(ctx, indent, container, false, varName, currName);
            }
            else if (curr.isEvent() == true)
            {
                // see if there is already an 'encode' function for this
                // NOTE: use getName() since that represents the class (vs local name)
                //
                boolean didIt = false;
                EventWrapper ew = Processor.getEventProcessor().getEventForName(curr.getName());
                if (ew != null)
                {
                    String encoder = ew.getContextNative().getEncodeFunctionName();
                    if (encoder != null)
                    {
                        // use the encoder function
                        //
                        lines = createNativeEncodeForWrapper(ew, encoder, varName+"->"+currName, currName, container, indent);
                        didIt = true;
                    }
                }

                if (didIt == false)
                {
                    // replicate what this object should do by itself
                    //
                    lines = curr.getNativeEncodeLines(ctx, indent, container, false, varName + "->" + currName, currName);
                }
            }
            else if (curr.isCustom() == true)
            {
                // see if there is already an 'encode' function for this
                // NOTE: use getName() since that represents the class (vs local name)
                //
                boolean didIt = false;
                PojoWrapper pw = Processor.getPojoProcessor().getPojoForName(curr.getName());
                if (pw != null)
                {
                    String encoder = pw.getContextNative().getEncodeFunctionName();
                    if (encoder != null)
                    {
                        // use the encoder function
                        //
                        lines = createNativeEncodeForWrapper(pw, encoder, varName+"->"+currName, currName, container, indent);
                        didIt = true;
                    }
                }

                if (didIt == false)
                {
                    // replicate what this object should do by itself
                    //
                    lines = curr.getNativeEncodeLines(ctx, indent, container, false, varName + "->" + currName, currName);
                }
            }
            else
            {
                // make the item add to the json object we just created using it's own name as the key
                //
                lines = curr.getNativeEncodeLines(ctx, indent, container, false, varName + "->" + currName, currName);
            }

            if (lines != null)
            {
                buff.append(lines);
            }
        }
        buff.append(prefix+"cJSON_AddItemToObjectCS("+parentContainerName+", \""+getName()+"\", "+container+");\n");

        return buff;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private StringBuffer createNativeEncodeForWrapper(Wrapper wrap, String encodeFunc, String fieldName, String varName, String container, int indent)
    {
        StringBuffer retVal = new StringBuffer();
        String prefix = getIndentBuffer(indent);
        String prefix2 = getIndentBuffer(indent+1);

        // use the encoder function
        //    fooJSON = encode_foo(varName->foobar);
        //
        retVal.append(prefix+"if ("+fieldName+" != NULL)\n");
        retVal.append(prefix+"{\n");
        retVal.append(prefix2+"// create JSON object for "+fieldName+"\n");
        retVal.append(prefix2+"cJSON *"+varName+"_json = "+encodeFunc+"("+fieldName+");\n");
        retVal.append(prefix2+"cJSON_AddItemToObjectCS("+container+", \""+varName+"\", "+varName+"_json);\n");
        retVal.append(prefix+"}\n");

        return retVal;
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
//        String prefix2 = getIndentBuffer(indent+1);

        // extract the json object for our object
        //
        String container = getName()+"_json";
        if (parentIsArray == false)
        {
            buff.append('\n');
            buff.append(prefix+"// extract JSON object for "+getName()+"\n");
            buff.append(prefix+"cJSON *"+container+" = NULL;\n");
            buff.append(prefix+container+" = cJSON_GetObjectItem("+parentContainerName+", (const char *)\""+varStorageKey+"\");\n");
            buff.append(prefix+"if ("+container+" != NULL)\n"+prefix+"{\n");
        }
        else
        {
            // use the name of the supplied container.  We're in an Array and the container has been extracted already
            //
            container = parentContainerName;
        }

        // now each field within this object
        //
        for (int i = 0 ; i < fields.size() ; i++)
        {
            boolean didIt = false;
            BaseVariable curr = fields.get(i);
            String currName = curr.getName();
            if (curr.isEnum() == true)
            {
                EnumVariable ef = (EnumVariable)curr;
                currName = ef.getVarName();
            }
            else if (curr.isCustom())
            {
                CustomVariable cust = (CustomVariable)curr;
                currName = cust.getLocalName();
            }

            String key = varName+"->"+currName;
            StringBuffer lines = null;

            if (curr.isEvent() == true)
            {
                // see if there is already an 'decode' function for this
                // NOTE: use getName() since that represents the class (vs local name)
                //
                EventWrapper ew = Processor.getEventProcessor().getEventForName(curr.getName());
                if (ew != null)
                {
                    String creator = ew.getContextNative().getCreateFunctionName();
                    String decoder = ew.getContextNative().getDecodeFunctionName();
                    if (creator != null && decoder != null)
                    {
                        // use the decoder function
                        //
                        lines = createNativeDecodeForWrapper(ew, varName, currName, container, indent);
                        didIt = true;
                    }
                }
            }
            else if (curr.isCustom() == true)
            {
                // see if there is already an 'decode' function for this
                // NOTE: use getName() since that represents the class (vs local name)
                //
                PojoWrapper pw = Processor.getPojoProcessor().getPojoForName(curr.getName());
                if (pw != null)
                {
                    String creator = pw.getContextNative().getCreateFunctionName();
                    String decoder = pw.getContextNative().getDecodeFunctionName();
                    if (creator != null && decoder != null)
                    {
                        // use the decoder function
                        //
                        lines = createNativeDecodeForWrapper(pw, varName, currName, container, indent);
                        didIt = true;
                    }
                }
            }

            // make the item add to the json object we just created using it's own name as the key
            //
            if (didIt == false)
            {
                lines = curr.getNativeDecodeLines(ctx, indent+1, container, false, key, currName);
            }
            if (lines != null)
            {
                buff.append(lines);
            }
        }

        if (parentIsArray == false)
        {
            // end our if
            buff.append(prefix+"}\n");
        }

        return buff;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private StringBuffer createNativeDecodeForWrapper(Wrapper wrap, String parentName, String fieldName, String container, int indent)
    {
        StringBuffer retVal = new StringBuffer();
        String prefix = getIndentBuffer(indent+1);
        String prefix2 = getIndentBuffer(indent+2);
        String prefix3 = getIndentBuffer(indent+3);

        // use the decoder function
        //    parent->foo = create_foo();
        //    cJSON *foo_json = decode_foo(parent->foo, parent_json);
        //
        String creator = wrap.getContextNative().getCreateFunctionName();
        String destructor = wrap.getContextNative().getDestroyFunctionName();
        String decoder = wrap.getContextNative().getDecodeFunctionName();


        retVal.append(prefix+"cJSON *"+fieldName+"_json = cJSON_GetObjectItem("+container+", (const char *)\""+fieldName+"\");\n");
        retVal.append(prefix+"if ("+fieldName+"_json != NULL)\n");
        retVal.append(prefix+"{\n");
/* caused segfault, even though it doesn't make sense....
        if (destructor != null)
        {
            retVal.append(prefix2+"// reset "+fieldName+" then decode\n");
            retVal.append(prefix2+destructor+"("+parentName+"->"+fieldName+");\n");
        }
        else
        {
            retVal.append(prefix2+"// create "+fieldName+" then decode\n");
        }
        retVal.append(prefix2+parentName+"->"+fieldName+" = "+creator+"();\n");
 */
        // instead of releasing the object, only create if missing
        // yes, this possibly could leak the internals of the object,
        // but better then segfault
        //
        retVal.append(prefix2+"if ("+parentName+"->"+fieldName+" == NULL)\n");
        retVal.append(prefix2+"{\n");
        retVal.append(prefix3+parentName+"->"+fieldName+" = "+creator+"();\n");
        retVal.append(prefix2+"}\n");

        retVal.append(prefix2+decoder+"("+parentName+"->"+fieldName+", "+fieldName+"_json);\n");
        retVal.append(prefix+"}\n");

        return retVal;
    }


    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public MacroSource generateJavaPojo(int indent)
    {
        MacroSource retVal = new MacroSource();
        String prefix = getIndentBuffer(indent);

        // add any freeform text we have
        //
        StringBuffer fform = new StringBuffer();
        if (node != null)
        {
            ICCustomObject bean = (ICCustomObject)node;
            List<String> freeList = bean.getFreeformList();
            for (int i = 0 ; i < freeList.size() ; i++)
            {
                fform.append(freeList.get(i));
                fform.append('\n');
            }
            retVal.addMapping(TemplateMappings.JAVA_FREE_FORM, fform.toString());
        }

        // make the prefix for encode & decode
        //
        StringBuffer encode = new StringBuffer();
        StringBuffer decode = new StringBuffer();

        // if we have a non-abstract parent class, encode it first
        //
        if (isEvent() == true || (StringUtils.isEmpty(parentClassName) == false && parentClassAbstract == false))
        {
            encode.append(prefix+"// encode "+parentClassName+" first\n");
            encode.append(prefix+"JSONObject retVal = super.toJSON();\n\n");
            decode.append(prefix+"// decode "+parentClassName+" first\n");
            decode.append(prefix+"super.fromJSON(buffer);\n\n");
        }
        else
        {
            encode.append(prefix+"JSONObject retVal = new JSONObject();\n");
        }

        // loop through the variables
        //
        CodeFormater vars = new CodeFormater(4);
        StringBuffer initLines = new StringBuffer();
        StringBuffer beanFuncts = new StringBuffer();
        StringBuffer toString = new StringBuffer("\"["+name+" = \"");
        for (int i = 0 ; i < fields.size() ; i++)
        {
            BaseVariable curr = fields.get(i);
            String currName = curr.getName();

            // add type definition
            //
            if (curr.isMap() == true)
            {
                // special as it will have 2 fields
                //
                MapVariable map = (MapVariable)curr;
                vars.nextRow();
                vars.setValue("private", 0);
                vars.setValue(map.getJavaType(), 1);
                vars.setValue(map.getValuesVarName()+";", 2);

                if (StringUtils.isEmpty(map.getDescription()) == false)
                {
                    vars.setValue("  // "+map.getDescription(), 3);
                }

                vars.nextRow();
                vars.setValue("private", 0);
                vars.setValue(map.getJavaTypesType(), 1);
                vars.setValue(map.getTypesVarName()+";", 2);
            }
            else
            {
                // regular field
                //
                vars.nextRow();
                vars.setValue("private", 0);
                vars.setValue(curr.getJavaType(), 1);
                if (curr.isEnum())
                {
                    // enum uses 'var name' for the name
                    //
                    EnumVariable enumF = (EnumVariable)curr;
                    currName = enumF.getVarName();
                }
                else if (curr.isCustom())
                {
                    // use 'localName'
                    //
                    CustomVariable cust = (CustomVariable)curr;
                    currName = cust.getLocalName();
                }
                vars.setValue(currName+";", 2);

                if (StringUtils.isEmpty(curr.getDescription()) == false)
                {
                    vars.setValue("  // "+curr.getDescription(), 3);
                }
            }

            // add getter/setter for this variable
            //
            beanFuncts.append(curr.getJavaGetter());
            beanFuncts.append(curr.getJavaSetter());

            // create toString for this variable
            //
            if (curr.isMap() == false && curr.isSensitive() == false)
            {
                // add this field to the 'toString'
                //
                toString.append("+\" ");
                toString.append(currName);
                toString.append("='\"+");
                toString.append(currName);
                toString.append("+\"'\"");
            }

            // allow for 'constructor' initialization (map only right now)
            //
            if (curr.isArray() == true)
            {
                ArrayVariable array = (ArrayVariable)curr;
                String init = array.getJavaConstructorLines(2);
                if (init != null)
                {
                    initLines.append(init);
                }
            }
            else if (curr.isMap() == true)
            {
                MapVariable map = (MapVariable)curr;
                String init = map.getJavaConstructorLines(2);
                if (init != null)
                {
                    initLines.append(init);
                }
            }
        }

        // finally the encode/decode
        //
        encode.append(getJavaEncodeLines(indent, "retVal", false, getName(), getName()));
        decode.append(getJavaDecodeLines(indent, "buffer", false, getName(), getName()));

        // suffix for encode/decode
        //
        encode.append(prefix+"return retVal;\n");


        retVal.addMapping(TemplateMappings.CONSTRUCTOR_MARKER, initLines.toString());
        retVal.addMapping(TemplateMappings.JAVA_VAR_SECTION, vars.toStringBuffer(4).toString());
        retVal.addMapping(TemplateMappings.JAVA_GET_SET_SECTION, beanFuncts.toString());
        retVal.addMapping(TemplateMappings.POJO_ENCODE_BODY, encode.toString());
        retVal.addMapping(TemplateMappings.POJO_DECODE_BODY, decode.toString());
        retVal.addMapping(TemplateMappings.JAVA_TO_STRING, toString.toString());

        return retVal;
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

        if (parentIsArray == true)
        {
            // use the toJSON method of ourself to encode and append to the array
            //
            buff.append(prefix+"// encode then append to end of the array\n");
            buff.append(prefix+parentContainerName+".put("+varName+".toJSON());\n");
        }
        else
        {
            // make unique container
            //
            String container = getName()+"_json";
            buff.append(prefix+"JSONObject "+container+" = new JSONObject();\n");

            // keep track of the variables we encode/decode to prevent duplication
            //
            HashSet<String> usedVars = new HashSet<String>();
            for (int i = 0 ; i < fields.size() ; i++)
            {
                // shove each field into our custom container object
                //
                BaseVariable curr = fields.get(i);
                String currName = curr.getName();
                if (curr.isEnum() == true)
                {
                    EnumVariable ef = (EnumVariable)curr;
                    currName = ef.getVarName();
                }
                else if (curr.isCustom() == true)
                {
                    CustomVariable cust = (CustomVariable)curr;
                    currName = cust.getLocalName();
                }
                if (usedVars.contains(currName) == true)
                {
                    // already encoded this variable
                    //
                    continue;
                }
                usedVars.add(currName);

                if (curr.isCustom() == false)
                {
                    // add encode lines for this variable type
                    //
                    buff.append(curr.getJavaEncodeLines(indent, container, false, currName, currName));
                }
                else
                {
                    // custom, so just call the custom object 'encode' function
                    //
                    String subCont = currName+"_json";
                    buff.append(prefix+"if ("+currName+" != null)\n");
                    buff.append(prefix+"{\n");

                    buff.append(prefix2+"JSONObject "+subCont+" = "+currName+".toJSON();\n");
                    buff.append(prefix2+container+".put(\""+currName+"\", "+subCont+");\n");
                    buff.append(prefix+"}\n");
                }
            }
            buff.append(prefix+parentContainerName+".put(\""+varStorageKey+"\", "+container+");\n");
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
        String prefix2 = getIndentBuffer(indent+1);
        String prefix3 = getIndentBuffer(indent+2);
//        String prefix4 = getIndentBuffer(indent+3);
//        String prefix5 = getIndentBuffer(indent+4);
//        String prefix6 = getIndentBuffer(indent+5);

        String container = getName()+"_json";
        if (parentIsArray == true)
        {
            // use the toJSON method of ourself to encode and append to the array
            //
            buff.append(prefix+"// decode then append to end of the List\n");
            buff.append(prefix+"JSONObject "+container+" = (JSONObject)"+parentContainerName+".optJSONObject("+varStorageKey+");\n");
            buff.append(prefix+getJavaIpcObjectType()+" next = new "+getJavaIpcObjectType()+"();\n");
            buff.append(prefix+"next.fromJSON("+container+");\n");
        }
        else
        {
            // keep track of the variables we encode/decode to prevent duplication
            //
            HashSet<String> usedVars = new HashSet<String>();

            // make unique container
            //
            buff.append(prefix+"JSONObject "+container+" = (JSONObject)"+parentContainerName+".opt(\""+varStorageKey+"\");\n");
            buff.append(prefix+"if ("+container+" != null)\n");
            buff.append(prefix+"{\n");
            for (int i = 0 ; i < fields.size() ; i++)
            {
                BaseVariable curr = fields.get(i);
                String currName = curr.getName();
                if (curr.isEnum() == true)
                {
                    EnumVariable ef = (EnumVariable)curr;
                    currName = ef.getVarName();
                }
                else if (curr.isCustom() == true)
                {
                    CustomVariable cust = (CustomVariable)curr;
                    currName = cust.getLocalName();
                }
                if (usedVars.contains(currName) == true)
                {
                    // already decoded this variable
                    //
                    continue;
                }
                usedVars.add(currName);

                if (curr.isCustom() == false)
                {
                    // add decode lines for this variable type
                    //
                    buff.append(curr.getJavaDecodeLines(indent + 1, container, false, currName, currName));
                }
                else
                {
                    // custom, so just call the custom object 'decode' function
                    //
                    String subCont = currName+"_json";

                    buff.append(prefix2+"JSONObject "+subCont+" = (JSONObject)"+container+".opt(\""+currName+"\");\n");
                    buff.append(prefix2+"if ("+subCont+" != null)\n");
                    buff.append(prefix2+"{\n");

                    // just allocate & decode
                    //
                    buff.append(prefix3+currName+" = new "+curr.getJavaType()+"();\n");
                    buff.append(prefix3+currName+".fromJSON("+subCont+");\n");
                    buff.append(prefix2+"}\n");
                }
            }
            buff.append(prefix+"}\n");
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
        return StringUtils.camelCase(getName());
    }
}

//+++++++++++
//EOF
//+++++++++++
