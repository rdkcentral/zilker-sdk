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

//-----------
//imports
//-----------

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.net.URL;

import com.icontrol.substitution.Substitution;

//-----------------------------------------------------------------------------
// Class definition:    FileHelper
/**
 *  Utility class to load a resource file into a StringBuffer 
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class FileHelper
{
    //--------------------------------------------
    /**
     * Insert the contents of the header file into the stream
     */
    //--------------------------------------------
    public static StringBuffer loadTemplate(String resourcePath) 
        throws IOException
    {
        // read the template into a StringBuffer
        //
        URL url = FileHelper.class.getResource(resourcePath);
        if (url == null)
        {
            throw new IOException("unable to load template "+resourcePath);
        }
        InputStream in = url.openStream();

        // read line at a time so we can construct a StringBuffer
        // as well as skip over "template comments"
        //
        BufferedReader reader = null;
        StringBuffer retVal = new StringBuffer();
        try
        {
            reader = new BufferedReader(new InputStreamReader(in));
            String line = null;
            while ((line = reader.readLine()) != null)
            {
                // skip template comments (start with ##)
                //
                if (line.startsWith("##") == true)
                {
                    continue;
                }

                retVal.append(line);
                retVal.append('\n');
            }
        }
        finally
        {
            reader.close();
        }
        return retVal;

        /*
        StringWriter out = new StringWriter();
        // copy to the output buffer
        //
        try
        {
            byte[] buf = new byte[1024 * 8];
            int    num = 0;
            while ((num = in.read(buf)) != -1)
            {
                // convert bytes to a String, then append
                //
                String str = new String(buf, 0, num);

                // skip template comments (start with ##)
                //
                if (str.startsWith("##") == true)
                {
                    continue;
                }
                out.write(str);
            }
        }
        finally
        {
            in.close();
            out.close();
        }

        return out.getBuffer();
        */
    }

    //--------------------------------------------
    /**
     * Helper to translate a buffer with the given MacroSource
     */
    //--------------------------------------------
    public static String translateBuffer(StringBuffer input, MacroSource macros)
    {
        // Create a Substitution object and add the various variable mappings
        // so that we can convert our template into a meaningful object
        //
        Substitution sub = new Substitution();
        sub.setIgnoreMacros(true);
        sub.addSubstitutionSource(macros);
        return sub.substitute(input.toString());
    }

    //--------------------------------------------
    /**
     * Helper to load a template and perform a single
     * key/value substitution.  Handy for small buffers
     */
    //--------------------------------------------
    public static String translateSimpleBuffer(String template, String macroKey, String macroValue)
        throws IOException
    {
        // load the supplied template
        //
        StringBuffer temp = loadTemplate(template);

        // create MacroSource that only has this one key/value
        //
        MacroSource ms = new MacroSource();
        ms.addMapping(macroKey, macroValue);

        // translate and return
        //
        return translateBuffer(temp, ms);
    }

    //--------------------------------------------
    /**
     * Helper to load a template and perform translation
     * using the supplied macros
     */
    //--------------------------------------------
    public static String translateBufferTemplate(String template, MacroSource macros)
        throws IOException
    {
        // load the supplied template
        //
        StringBuffer temp = loadTemplate(template);

        // translate and return
        //
        return translateBuffer(temp, macros);
    }

    //--------------------------------------------
    /**
     * Private Constructor 
     */
    //--------------------------------------------
    private FileHelper()
    {
    }
}



//+++++++++++
//EOF
//+++++++++++