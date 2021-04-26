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
import java.util.Iterator;

import com.icontrol.generate.service.TemplateMappings;
import com.icontrol.generate.service.context.ContextNative;
import com.icontrol.generate.service.io.CodeFormater;
import com.icontrol.generate.service.io.MacroSource;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.variable.EnumDefinition.EnumValue;
import com.icontrol.util.StringUtils;
import com.icontrol.xmlns.service.ICEnumVariable;

//-----------------------------------------------------------------------------
// Class definition:    EnumVariable
/**
 *  Enumeration used as a BaseVariable.  Different from EnumDefinition
 *  in that this is an "instance" of the definition to be used within
 *  another container (Custom, Event, IPC, etc).  This has a 'type'
 *  and a 'variable name'
 *
 *  @see EnumDefinition
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class EnumVariable extends BaseVariable
{
    String          varName;
    String          enumRef;
    EnumDefinition  definition;     // implementation of 'enumRef'
    boolean			isInstance;		// has a 'varname' and can be placed in a container
    boolean         doLabels;
    boolean         doShortNames;

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public EnumVariable(ICEnumVariable bean)
    {
        super(bean);
        if (bean != null)
        {
            // get the name of the EnumDefinition to load
            //
            enumRef = bean.getEnumTypeName();
            name = bean.getEnumTypeName();
            varName = bean.getVarName();
            isInstance = true;
        }
        else
        {
        	// probably just created for the EnumDef and
        	// can't be used in a container yet
        	//
        	isInstance = false;
        }
    }

    public EnumVariable(EnumDefinition def)
    {
    	super(def.getName());
    	isInstance = false;
    	definition = def;
    	varName = null;
    	enumRef = def.getName();
        doLabels = def.generateNativeLabels();
        doShortNames = def.generateNativeShortNames();
    }

    //--------------------------------------------
    /**
     * Returns the varName.  Note these are slightly
     * different then regular BaseVariable due to the
     * reference vs real name
     */
    //--------------------------------------------
    public String getVarName()
    {
        if (varName != null)
        {
            return varName;
        }
        return super.getName();
    }

    //--------------------------------------------
    /**
     * NOTE: should only be used during parsing
     *
     * @param varName The varName to set.
     */
    //--------------------------------------------
    public void setVarName(String v)
    {
    	// set the varName, then clear the isInstance flag
    	//
        this.varName = v;
        isInstance = true;
    }

    public String getEnumRef()
    {
        return enumRef;
    }

    public void setEnumRef(String enumRef)
    {
        this.enumRef = enumRef;
    }

    public void setDefinition(EnumDefinition def)
    {
    	definition = def;
    }

    //--------------------------------------------
    /**
     * Return true if this has a 'varName' set and
     * can be used in a container
     *
     * @param varName The varName to set.
     */
    //--------------------------------------------
    public boolean hasVarName()
    {
    	return isInstance;
    }

    //--------------------------------------------
    /**
     * @return Returns the enumeration values.
     */
    //--------------------------------------------
    public ArrayList<String> getValues()
    {
        ArrayList<String> retVal = new ArrayList<String>();
        if (definition != null)
        {
            Iterator<EnumValue> loop = definition.getEnumValues();
            while (loop.hasNext() == true)
            {
                EnumValue val = loop.next();
                retVal.add(val.value);
            }
        }
        return retVal;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#resolveReferences()
     */
    //--------------------------------------------
    @Override
    public void resolveReferences()
    {
        // ask the Factory for the EnumDefinition
        //
        definition = Processor.getEnumDefinitionForName(enumRef);
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#isEnum()
     */
    //--------------------------------------------
    @Override
    public boolean isEnum()
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
        return getName();
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public EnumDefinition getDefinition()
    {
        return definition;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public boolean generateNativeLabels()
    {
        return doLabels;
    }

    public boolean generateNativeShortNames()
    {
        return doShortNames;
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

        if (definition != null)
        {
            String comment = definition.getDescription();
            if (StringUtils.isEmpty(comment) == false)
            {
                // add the description of the enumeration above the 'typedef'
                //
                buff.append("// "+comment);
                buff.append('\n');
            }
            buff.append("typedef enum {\n");

            // make a table of the elements with their numeric & comments
            //
            CodeFormater table = new CodeFormater(4);

            Iterator<EnumValue> loop = definition.getEnumValues();
            while (loop.hasNext() == true)
            {
                EnumValue val = loop.next();

                // add enum value
                //
                table.nextRow();
                table.setValue("    ", 0);
                table.setValue(val.value, 1);

                // add numeric and the comma (if needed)
                //
                String numeric = " = "+val.numeric;
                if (loop.hasNext() == true)
                {
                    numeric += ",";
                }
                table.setValue(numeric, 2);

                // add description of enum as a comment (if defined)
                //
                if (StringUtils.isEmpty(val.desc) == false)
                {
                    table.setValue(" // "+val.desc, 3);
                }
                else
                {
                    table.setValue("", 3);
                }
            }

            // append to the buffer
            //
            buff.append(table.toStringBuffer(0));
            buff.append("} ");
            buff.append(getName());
            buff.append(";\n");
        }
        else
        {
            buff.append("// ERROR: unable to create Enum "+getName());
        }

        return buff;
    }

    //--------------------------------------------
    /**
     * Return the C code that goes into the .h file
     */
    //--------------------------------------------
    public StringBuffer getNativeLabelsArrayString()
    {
        StringBuffer buff = new StringBuffer();

        if (definition != null && definition.getEnumCount() > 0)
        {
            // add the description of the enumeration above the 'typedef'
            //
            buff.append("// NULL terminated array that should correlate to the "+getName()+" enum\n");
            buff.append("static const char *"+getName()+"Labels[] = {\n");

            // make a table of the elements with their numeric & comments
            //
            CodeFormater table = new CodeFormater(3);

            Iterator<EnumValue> loop = definition.getEnumValues();
            while (loop.hasNext() == true)
            {
                EnumValue val = loop.next();

                // add enum value, surrounded by quotes and ending with a comma
                //
                table.nextRow();;
                table.setValue("    ", 0);
                table.setValue("\""+val.value+"\",", 1);

                // add the value again, but as a comment
                //
                table.setValue("// "+val.value, 2);
            }

            // last row is the "NULL"
            table.nextRow();
            table.setValue("    ", 0);
            table.setValue("NULL", 1);

            // append to the buffer
            //
            buff.append(table.toStringBuffer(0));
            buff.append("};\n");
        }
        else
        {
            buff.append("// ERROR: unable to create labels for Enum "+getName());
        }

        return buff;
    }

    //--------------------------------------------
    /**
     * Return the C code that goes into the .h file
     */
    //--------------------------------------------
    public StringBuffer getNativeShortNameArrayString()
    {
        StringBuffer buff = new StringBuffer();

        if (definition != null && definition.getEnumCount() > 0)
        {
            // add the description of the enumeration above the 'typedef'
            //
            buff.append("// NULL terminated array of short names that should correlate to the "+getName()+" enum\n");
            buff.append("static const char *"+getName()+"ShortNames[] = {\n");

            // make a table of the elements with their numeric & comments
            //
            CodeFormater table = new CodeFormater(3);

            Iterator<EnumValue> loop = definition.getEnumValues();
            while (loop.hasNext() == true)
            {
                EnumValue val = loop.next();

                // add enum shortName, surrounded by quotes and ending with a comma
                // if a shortname isn't provided, use the long name so we don't have a NULL
                //
                table.nextRow();
                if(StringUtils.isEmpty(val.shortName) == false)
                {
                    table.setValue("    ", 0);
                    table.setValue("\"" + val.shortName + "\",", 1);
                }
                else
                {
                    table.setValue("    ",0);
                    table.setValue("\"" + val.value + "\",", 1);
                }

                // add the value, but as a comment for reference
                //
                table.setValue("// "+val.value, 2);
            }

            // last row is the "NULL"
            table.nextRow();
            table.setValue("    ", 0);
            table.setValue("NULL", 1);

            // append to the buffer
            //
            buff.append(table.toStringBuffer(0));
            buff.append("};\n");
        }
        else
        {
            buff.append("// ERROR: unable to create short names for Enum "+getName());
        }

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
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaGetter()
     */
    //--------------------------------------------
    @Override
    public StringBuffer getJavaGetter()
    {
        StringBuffer buff = new StringBuffer();

        buff.append("\tpublic ");
        buff.append(getJavaType());
        buff.append(" get");
        buff.append(StringUtils.camelCase(getVarName()));
        buff.append("()\n");
        buff.append("\t{\n");
        buff.append("\t\treturn ");
        buff.append(getVarName());
        buff.append(";\n\t}\n\n");

        return buff;
    }

    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaSetter()
     */
    //--------------------------------------------
    @Override
    public StringBuffer getJavaSetter()
    {
        StringBuffer buff = new StringBuffer();

        buff.append("\tpublic void set");
        buff.append(StringUtils.camelCase(getVarName()));
        buff.append("("+getJavaType()+" val)\n");
        buff.append("\t{\n\t\t");
        buff.append(getVarName());
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

        // treat as an integer type
        //
        if (parentIsArray == true)
        {
            buff.append(prefix);
            buff.append("cJSON_AddItemToArray("+parentContainerName+", cJSON_CreateNumber(");
            buff.append(varName);
            buff.append("));\n");
        }
        else
        {
            buff.append(prefix);
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
    public StringBuffer getNativeDecodeLines(ContextNative ctx, int indent, String parentContainerName, boolean parentIsArray, String vName, String varStorageKey)
    {
        // treat as an integer type
        //
        StringBuffer buff = new StringBuffer();
        String prefix = getIndentBuffer(indent);
        String prefix2 = getIndentBuffer(indent+1);

        if (parentIsArray == true)
        {
            // get item at 'varStorageKey', which we assume is the index into the array
            //
            buff.append(prefix);
            buff.append("item = cJSON_GetArrayItem("+parentContainerName+", ");
            buff.append(varStorageKey);
            buff.append(");\n");
        }
        else
        {
            // get item with this key
            //
            buff.append(prefix);
            buff.append("item = cJSON_GetObjectItem("+parentContainerName+", \"");
            buff.append(varStorageKey);
            buff.append("\");\n");
        }

        // now the logic for our data type and dealing with non-null value
        //
        buff.append(prefix+"if (item != NULL)\n"+prefix+"{\n");
        buff.append(prefix2+vName+" = ("+getCtype()+")item->valueint;\n");
        buff.append(prefix2+"rc = 0;\n"+prefix+"}\n");

        return buff;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    public MacroSource generateJavaTransport()
    {
        MacroSource retVal = new MacroSource();

        // treat as an integer type
        // first make the values
        //
        if (definition != null)
        {
            CodeFormater vars = new CodeFormater(2);
            Iterator<EnumValue> loop = definition.getEnumValues();
            while (loop.hasNext() == true)
            {
                EnumValue val = loop.next();
                vars.nextRow();
                vars.setValue(val.value, 0);

                String nVal = "("+val.numeric+")";
                if (loop.hasNext() == true)
                {
                    nVal += ",";
                }
                else
                {
                    nVal += ";";
                }
                vars.setValue(nVal, 1);
            }
            retVal.addMapping(TemplateMappings.ENUM_JAVA_VARS, vars.toStringBuffer(4).toString());
        }
        else
        {
            retVal.addMapping(TemplateMappings.ENUM_JAVA_VARS, "    // ERROR: cannot locate EnumDefinition '"+enumRef+"'");
        }

        return retVal;
    }
    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaEncodeLines(int, java.lang.String, boolean, java.lang.String, java.lang.String)
     */
    //--------------------------------------------
    @Override
    public StringBuffer getJavaEncodeLines(int indent, String parentContainerName, boolean parentIsArray, String vName, String varStorageKey)
    {
        StringBuffer buff = new StringBuffer();
        String prefix = getIndentBuffer(indent);
        String prefix2 = getIndentBuffer(indent+1);

        // treat as an integer type
        //
        buff.append(prefix);
        if (parentIsArray == true)
        {
            buff.append("if ("+vName+" != null)\n");
            buff.append(prefix+"{\n");
            buff.append(prefix2+parentContainerName+".put("+vName+".getNumValue());\n");
            buff.append(prefix+"}\n");
        }
        else
        {
            buff.append("if ("+vName+" != null)\n");
            buff.append(prefix+"{\n");
            buff.append(prefix2+parentContainerName+".put(\""+varStorageKey+"\", "+vName+".getNumValue());\n");
            buff.append(prefix+"}\n");
        }

        return buff;
    }
    //--------------------------------------------
    /**
     * @see com.icontrol.generate.service.variable.BaseVariable#getJavaDecodeLines(int, java.lang.String, boolean, java.lang.String, java.lang.String)
     */
    //--------------------------------------------
    @Override
    public StringBuffer getJavaDecodeLines(int indent, String parentContainerName, boolean parentIsArray, String vName, String varStorageKey)
    {
        StringBuffer buff = new StringBuffer();
        String prefix = getIndentBuffer(indent);

        // treat as an integer type
        //
        String cName = StringUtils.camelCase(getName());
        buff.append(prefix);
        if (parentIsArray == true)
        {
            buff.append(getJavaType()+" "+vName+" = "+cName+".enumForInt("+parentContainerName+".getInt("+varStorageKey+"));\n");
        }
        else
        {
            buff.append(vName+" = "+cName+".enumForInt("+parentContainerName+".optInt(\""+varStorageKey+"\"));\n");
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
