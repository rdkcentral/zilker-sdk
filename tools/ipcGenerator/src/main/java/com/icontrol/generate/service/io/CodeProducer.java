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

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.parse.Wrapper;

//-----------------------------------------------------------------------------
/**
 * Base structure for generating code for a particular Wrapper and Language
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public abstract class CodeProducer
{
    //--------------------------------------------
    /**
     * Perform code generation for a variety of languages
     *
     * @param ctx - Context of the XML file we're currently processing
     * @param lang - define the languages being generated
     * @return if the generation was successful
     */
    //--------------------------------------------
    public abstract boolean generate(Context ctx, LanguageMask lang)
        throws IOException;

    //--------------------------------------------
    /**
     * Perform code generation for a single Wrapper as a StringBuffer
     * Used when making an entire File.
     *
     * @param ctx - Context of the XML file we're currently processing
     * @param lang - define the languages being generated
     * @param wrapper - the Wrapper to generate for
     * @return if the generation was successful
     */
    //--------------------------------------------
    public abstract boolean generateWrapperFile(Context ctx, LanguageMask lang, Wrapper wrapper)
        throws IOException;

    //--------------------------------------------
    /**
     * Perform code generation for a single Wrapper as a StringBuffer
     * Generally used when piecing code together vs making an entire File
     *
     * @param ctx - Context of the XML file we're currently processing
     * @param lang - define the languages being generated
     * @param wrapper - the Wrapper to generate for
     * @return if the generation was successful
     */
    //--------------------------------------------
    public abstract StringBuffer generateWrapperBuffer(Context ctx, LanguageMask lang, Wrapper wrapper)
        throws IOException;

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    protected static String getDirForPackage(String packageName)
    {
        return packageName.replace('.', '/');
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    protected static File createFile(File baseDir, String outDir, String fileName, String templateResource, MacroSource ms)
        throws IOException
    {
        // substitue against our template
        //
        String body = FileHelper.translateBufferTemplate(templateResource, ms);
        return createFile(baseDir, outDir, fileName, body);
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    protected static File createFile(File baseDir, String outDir, String fileName, String source)
        throws IOException
    {
        // create the file directory
        //
        File targetDir = new File(baseDir, outDir);
        targetDir.mkdirs();

        // create the file (and erase if there already)
        //
        File targetFile = new File(targetDir, fileName);
        if (targetFile.exists() == true)
        {
            targetFile.delete();
        }

        // make the writer
        //
        PrintWriter output = new PrintWriter(new FileWriter(targetFile));

        // show the console the file we just generated
        //
        System.out.print("    ");
        System.out.println(targetFile);

        // append the standard header file
        //
        HeaderWriter.appendHeader(output);

        // write it to the file and cleanup
        //
        output.append(source);
        output.flush();
        output.close();
        output = null;

        return targetFile;
    }
}


