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

package com.icontrol.generate.service.io;

//-----------------------------------------------------------------------------

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.context.ContextJava;
import com.icontrol.generate.service.parse.EnumDefWrapper;
import com.icontrol.generate.service.parse.PojoWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.Wrapper;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.generate.service.variable.EnumVariable;
import com.icontrol.util.StringUtils;

/**
 * CodeProducer for Java POJOs
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class PojoJavaCodeProducer extends PojoCodeProducer
{
    private static final String POJO_JAVA_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/Pojo_java";
    private static final String ENUM_JAVA_TEMPLATE = "/com/icontrol/generate/service/templates/pojo/Enum_java";
    private static final String POJO_CLONE_JAVA    = "/com/icontrol/generate/service/templates/pojo/cloneable_java";

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public PojoJavaCodeProducer()
    {
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generate(Context, LanguageMask)
     */
    //--------------------------------------------
    @Override
    public boolean generate(Context ctx, LanguageMask lang)
        throws IOException
    {
        // loop through all POJO/Enum twice
        // first - calculate class, package, and filenames (so second iteration can refer to them for 'import')
        // second - generate code
        //
        Iterator<Wrapper> pojos = Processor.getPojoProcessor().getWrappers();
        while (pojos.hasNext() == true)
        {
            Wrapper wrap = pojos.next();
            if (wrap.getLanguageMask().hasJava() == true)
            {
                assignNames(ctx, wrap);
            }
        }
        Iterator<Wrapper> enums = Processor.getEnumProcessor().getWrappers();
        while (enums.hasNext() == true)
        {
            Wrapper wrap = enums.next();
            if (wrap.getLanguageMask().hasJava() == true &&
                wrap.getSectionMask().inPojoSection() == true)
            {
                assignNames(ctx, wrap);
            }
        }
        Iterator<Wrapper> events = Processor.getEventProcessor().getWrappers();
        while (events.hasNext() == true)
        {
            Wrapper wrap = events.next();
            if (wrap.getLanguageMask().hasJava() == true &&
                wrap.getSectionMask().inPojoSection() == true)
            {
                assignNames(ctx, wrap);
            }
        }

        // generate all Enums with a "pojo" section associated with them
        // NOTE: we do enumerations first in case the POJO definitions
        //       have dependencies on them
        //
        enums = Processor.getEnumProcessor().getWrappers();
        while (enums.hasNext() == true)
        {
            EnumDefWrapper wrap = (EnumDefWrapper)enums.next();
            if (wrap.getSectionMask().inPojoSection() == true)
            {
                // generate the Java code for this POJO
                //
                generateWrapperFile(ctx, lang, wrap);
            }
        }

        // generate all POJO objects in Java
        //
        pojos = Processor.getPojoProcessor().getWrappers();
        while (pojos.hasNext() == true)
        {
            PojoWrapper wrap = (PojoWrapper)pojos.next();
            if (wrap.getLanguageMask().hasJava() == true)
            {
                // generate the Java code for this POJO
                //
                generateWrapperFile(ctx, lang, wrap);
            }
        }

       return true;
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generateWrapperFile(Context, LanguageMask, Wrapper)
     */
    //--------------------------------------------
    @Override
    public boolean generateWrapperFile(Context ctx, LanguageMask lang, Wrapper wrapper)
        throws IOException
    {
        // generate the Java class for a single POJO/Enum wrapper
        //
        StringBuffer buffer = generateWrapperBuffer(ctx, lang, wrapper);
        if (buffer == null)
        {
            return false;
        }

        // extract the filename from the wrapper so we can save the file
        //
        File parent = ctx.getBaseOutputDir();
        String apiDir = ctx.getPaths().getApiDir();
        String pkgDir = getDirForPackage(wrapper.getContextJava().getJavaPackageName());

        // save the file
        //
        File target = createFile(parent, apiDir+"/"+pkgDir, wrapper.getContextJava().getJavaFileName(), buffer.toString());
        return (target != null && target.exists());
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generateWrapperBuffer(Context, LanguageMask, Wrapper)
     */
    //--------------------------------------------
    @Override
    public StringBuffer generateWrapperBuffer(Context ctx, LanguageMask lang, Wrapper wrapper)
        throws IOException
    {
        // skip if this wrapper doesn't support Java as an output
        //
        if (wrapper.getLanguageMask().hasJava() == false)
        {
            return null;
        }

        // calculate class, package, and file names
        //
        BaseVariable var = wrapper.getVariable();
        String cName = StringUtils.camelCase(var.getJavaType());
        String packageName = ctx.getPaths().getApiPackage();
        String fileName = cName+".java";

        // generate the Java class for a single POJO/Enum wrapper
        // done in 3 sections:
        //  1. object definition
        //  2. getters/setters
        //  3. encode/decode methods
        //
        String template = null;
        MacroSource map = null;
        if (var.isCustom() == true)
        {
            // get the mappings to generate Java code for this POJO
            //
            CustomVariable custVariable = (CustomVariable)var;
            map = custVariable.generateJavaPojo(2);
            map.addMapping(IMPLEMENTS_MARKER, custVariable.getInterfaceString());
            template = POJO_JAVA_TEMPLATE;

            // get Wrappers this depends on so we can build up the 'imports'
            //
            PojoWrapper pojo = (PojoWrapper)wrapper;
            HashSet<String> importSet = new HashSet<String>();
            Iterator<Wrapper> depends = pojo.getDependencies();
            while (depends.hasNext() == true)
            {
                Wrapper dep = depends.next();
                String depPkg = dep.getContextJava().getJavaPackageName();
                String depClass = dep.getContextJava().getJavaClassName();
                if (depPkg != null && depClass != null)
                {
                    importSet.add("import "+depPkg+"."+depClass+";");
                }
            }
            ArrayList<String> tmp = new ArrayList<String>(importSet);
            Collections.sort(tmp);
            StringBuffer importBuff = new StringBuffer();
            for (int i = 0, count = tmp.size() ; i < count ; i++)
            {
                importBuff.append(tmp.get(i)+"\n");
            }
            map.addMapping(JAVA_IMPORT_SECTION, importBuff.toString());
            
            // add cloneable if applicable
            //
            if (custVariable.isCloneable() == true)
            {
           		// load the template for creating the 'deepClone' method
           		// requires the java classname, so add to the mapping
            	//
            	map.addMapping(JAVA_CLASS, cName);
           		map.addMapping(JAVA_CLONE_METHOD, FileHelper.translateBufferTemplate(POJO_CLONE_JAVA, map));
           	}
        }
        else if (var.isEnum() == true)
        {
            // get the mapping to generate Java code for this Enum
            //
            EnumVariable enumVariable = (EnumVariable)var;
            map = enumVariable.generateJavaTransport();
            template = ENUM_JAVA_TEMPLATE;
        }
        else
        {
            // not able to generate the buffer
            //
            return null;
        }

        // add common items to the mapping
        //
        map.addMapping(JAVA_CLASS, cName);
        map.addMapping(JAVA_PKG, packageName);
        map.addMapping(SERVICE_KEY, ctx.getServiceName());

        // save names to our Context so callers can use this to
        // create the physical file (and know where)
        //
        ContextJava cj = wrapper.getContextJava();
        cj.setJavaClassName(cName);
        cj.setJavaPackageName(packageName);
        cj.setJavaFileName(fileName);

        // finally, translate using the 'template'
        //
        StringBuffer retVal = new StringBuffer();
        retVal.append(FileHelper.translateBufferTemplate(template, map));
        return retVal;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void assignNames(Context ctx, Wrapper wrapper)
    {
        // calculate class, package, and file names
        //
        BaseVariable var = wrapper.getVariable();
        String cName = StringUtils.camelCase(var.getJavaType());
        String packageName = ctx.getPaths().getApiPackage();
        String fileName = cName+".java";

        ContextJava cj = wrapper.getContextJava();
        cj.setJavaClassName(cName);
        cj.setJavaPackageName(packageName);
        cj.setJavaFileName(fileName);
    }

}
