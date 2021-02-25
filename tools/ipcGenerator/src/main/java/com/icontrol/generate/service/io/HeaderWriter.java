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

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Calendar;

//-----------------------------------------------------------------------------
// Class definition:    HeaderWriter
/**
 *  Utility class to insert the contents of "header.txt" into 
 *  the supplied output stream.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class HeaderWriter
{
    private final static String   HEADER_FILE = "/com/icontrol/generate/service/io/header.txt";
    private static String   contents = null;
    
    //--------------------------------------------
    /**
     * Insert the contents of the header file into the stream
     */
    //--------------------------------------------
    public static void appendHeader(PrintWriter out) throws IOException
    {
        // first get our header file contents
        //
        if (contents == null)
        {
            loadHeader();
        }
        
        // place header into the output stream
        //
        out.println(contents);
        
        // add a warning about this being an auto-generated file and the Date/Time
        //
//        out.print("/*** THIS IS A GENERATED FILE, DO NOT EDIT!!  Created on ");
//        out.print(new Date());
//        out.println(" ***/");
        out.println("/***  THIS IS A GENERATED FILE, DO NOT EDIT!!  ***/");
    }

    public static String headerToString() throws IOException
    {
        if (contents == null)
        {
            loadHeader();
        }

        return contents;
    }

    private static void loadHeader() throws IOException
    {
        StringBuffer headerContents = FileHelper.loadTemplate(HEADER_FILE);
        MacroSource map = new MacroSource();
        map.addMapping("year", Integer.toString(Calendar.getInstance().get(Calendar.YEAR)));
        contents = FileHelper.translateBuffer(headerContents,map);

    }
    
    //--------------------------------------------
    /**
     * Private Constructor 
     */
    //--------------------------------------------
    private HeaderWriter()
    {
    }
}



//+++++++++++
//EOF
//+++++++++++