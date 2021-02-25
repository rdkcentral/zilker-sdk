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

package com.icontrol.generate.service;

//-----------
//imports
//-----------

import java.io.File;
import java.io.FileFilter;
import java.util.ArrayList;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.context.PathValues;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.util.GetOpt;
import com.icontrol.util.Version;
import com.icontrol.xmlns.service.ICService;
import com.icontrol.xmlns.service.ICServiceDocument;

//-----------------------------------------------------------------------------
// Class definition:    Main
/**
 *  Entry point for the command line utility "ipcGenerator"
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class Main implements FileFilter
{
    private static final String OPTIONS = "f:d:v";
    
    //--------------------------------------------
    /**
     * @param args
     */
    //--------------------------------------------
    public static void main(String[] args)
    {
        String inputFile = null;
        String outputDir = null;
        boolean verbose = false;
        
        // process command line options
        //
        try
        {
            ArrayList<GetOpt> opts = GetOpt.getopts(args, OPTIONS);
            for (int i = 0; i < opts.size(); ++i)
            {
                GetOpt opt = opts.get(i);
                switch (opt.getOption())
                {
                    case 'f':   // input file or directoru
                        inputFile = opt.getArgument();
                        break;
                        
                    case 'd':   // output directoru
                        outputDir = opt.getArgument();
                        break;
                        
                    case 'v':   // verbose mode
                        verbose = true;
                        break;

                    case GetOpt.HELP_OPTION:
                    default:
                        printUsage();
                        System.exit(1);
                }
            }
        }
        catch (IllegalArgumentException ex)
        {
            printUsage();
            System.err.println(ex.getMessage());
            System.exit(1);
        }

        // make sure we have the necessary arguments
        //
        if (inputFile == null)
        {
            printUsage();
            System.err.println("\nNothing to do.  Need -f <file> or <dir> option.\n");
            System.exit(1);
        }
        File out = new File(System.getProperty("user.dir"));
        if (outputDir != null)
        {
            out = new File(outputDir);
        }
        
        if (verbose == true)
        {
            // force logging to STDOUT and turn on debug mode
            //
            System.setProperty("org.slf4j.simpleLogger.logFile", "System.out");
            System.setProperty("org.slf4j.simpleLogger.defaultLogLevel", "debug");
            System.setProperty("org.slf4j.simpleLogger.showThreadName", "false");
            System.setProperty("org.slf4j.simpleLogger.showDateTime", "true");
            System.setProperty("org.slf4j.simplelogger.dateTimeFormat", "yyyy-MM-dd HH:mm:ss:S");
            System.setProperty("org.slf4j.simpleLogger.showShortLogName", "true");
//            Substitution.setDebugMode(true);
        }
        
        try
        {
            // Get the list of files to process
            //
            ArrayList<File> process = getFiles(new File(inputFile));
            for (int i = 0 ; i < process.size() ; i++)
            {
                File inFile = process.get(i);

                // start processing the event input file
                // first, parse the XML into a document
                //
                System.out.println("Processing file " + inFile);

                // parse the XML
                //
                ICServiceDocument doc = ICServiceDocument.Factory.parse(inFile);
                ICService bean = doc.getService();

                // create a Context to keep track of our inputs
                // and generated objects
                //
                Context ctx = new Context();
                ctx.setDebug(verbose);
                ctx.setBaseOutputDir(out);
                ctx.setServiceName(bean.getName());
                ctx.setEventPortNum(bean.getEventPort());
                ctx.setIpcPortNum(bean.getIpcPort());

                // process the paths & package names
                //
                PathValues paths = new PathValues(bean.getPragma());
                ctx.setPaths(paths);

                // analyze the model based on the XML we just parsed
                //
                Processor.buildModel(bean, ctx);

                // produce the code, then clear the cache so we can
                // loop around to process the next XML file (and not
                // have left-over crud from the prev file)
                //
                Processor.generateCode(ctx);
                Processor.clearCache();
            }
        }
        catch (Exception ex)
        {
            ex.printStackTrace();
            System.exit(1);
        }
        
        System.out.println("Complete");
        System.exit(0);
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    private static ArrayList<File> getFiles(File file)
    {
        ArrayList<File> retVal = new ArrayList<File>();
        
        // if argument is a directory, then list it's files
        // but not recursively since we may have 'test' subdirectories
        //
        if (file.isDirectory() == true)
        {
            // get just the xml files
            //
            File[] array = file.listFiles(new Main());
            if (array != null)
            {
                for (int i = 0 ; i < array.length ; i++)
                {
                    retVal.add(array[i]);
                }
            }
        }
        else
        {
            // add just the one file
            //
            retVal.add(file);
        }
        
        return retVal;
    }
    
    //--------------------------------------------
    /**
     * 
     */
    //--------------------------------------------
    private static void printUsage()
    {
        // show the app version and the usage
        //
        Version ver = new Version("/build.version");
        System.err.println("generate "+ver);
        GetOpt.printUsage(System.err, OPTIONS);
        System.err.println("\nFor example: ./build/dist/generate.sh -f definitions/AudioMgr.xml -d /data/dev/android\n");
    }
    

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    private Main()
    {
    }
    
    //--------------------------------------------
    /**
     * @see java.io.FileFilter#accept(java.io.File)
     */
    //--------------------------------------------
    @Override
    public boolean accept(File path)
    {
        // only care about .xml files
        //
        String name = path.getName().toLowerCase();
        if (name.endsWith(".xml") == true)
        {
            return true;
        }

        return false;
    }
}

//+++++++++++
//EOF
//+++++++++++
