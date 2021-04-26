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

package com.icontrol.substitution;

//-----------
//imports
//-----------

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintStream;
import java.io.StringWriter;
import java.util.ArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.icontrol.util.GetOpt;
import com.icontrol.util.GetOpt.Style;

//-----------------------------------------------------------------------------
// Class definition:    Converter
/**
 *  Utility to convert old @xxx@ and $xxx style variables to the newer
 *  ${xxx} format which is now supported by our Substitution Engine.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class Converter
{
	private static final String	IC_PATTERN       = "@[\\w\\.\\(,\\-\\)]*@";				// '@' followed by 'alpha-numeric' or '.' or '(' or ',' or ')' and ended with '@'
	private static final String	VELOCITY_PATTERN = "\\$[\\w\\.\\(,\\)]*[\\s=!\\)\"]";	// '$' followed by 'alpha-numeric' or '.' or '(' or ',' or ')' and ended with
	
	//--------------------------------------------
	/**
	 * @param args
	 */
	//--------------------------------------------
	public static void main(String[] args)
	{
		// define CLI arguments
		//
		ArrayList<GetOpt> options = new ArrayList<GetOpt>();
		options.add(new GetOpt("in",  Style.ONE_ARG_STYLE,  "Input File to convert"));
		options.add(new GetOpt("out", Style.ONE_ARG_STYLE,  "Output File (or STDOUT if not set)"));
		options.add(new GetOpt("over", Style.NO_ARGS_STYLE, "Replace 'Input File' with results"));
		
		File inFile = null;
		File outFile = null;
		boolean over = false;
		try
		{
			// parse the arguments and get back the list of what was passed
			//
			ArrayList<GetOpt> actual = GetOpt.getopts(args, options);
			for (int i = 0, count = actual.size() ; i < count ; i++)
			{
				GetOpt curr = actual.get(i);
				String option = curr.getOptionString();
				
				if (option.equals("in") == true)
				{
					inFile = new File(curr.getArgument());
				}
				else if (option.equals("out") == true)
				{
					outFile = new File(curr.getArgument());
				}
				else if (option.equals("over") == true)
				{
					over = true;
				}
			}
		}
		catch (IllegalArgumentException ex)
		{
			// print usage and exit
			//
			System.err.println("iControl & Velocity Syntax Converter");
			GetOpt.printUsage(options, System.err);
			System.exit(1);
		}
		
		// make sure we have an input file
		//
		if (inFile == null || inFile.exists() == false || inFile.canRead() == false)
		{
			System.err.println("Unable to read input file '"+inFile+"'.  Use --help for options.");
			System.exit(1);
		}
		
		PrintStream output = System.out;
		boolean doClose = false;
		try
		{
			// perform the conversion
			//
			String replace = convertFile(inFile);
			
			// See if output to file or STDOUT
			//
			if (outFile != null)
			{
				output = new PrintStream(new FileOutputStream(outFile));
				doClose = true;
			}
			else if (over == true)
			{
				output = new PrintStream(new FileOutputStream(inFile));
				doClose = true;
			}
			
			// output the results
			//
			output.print(replace);
			output.flush();
		}
		catch (IOException oops)
		{
			oops.printStackTrace();
			System.exit(1);
		}
		finally
		{
			if (doClose == true)
			{
				output.close();
			}
		}
		System.exit(0);
	}
	
	//--------------------------------------------
	/**
	 * Takes a given 'File' and replaces the old @xx@ or $xx style
	 * variable markers with the new style of ${xx}.  This also
	 * handles methods and arguments.
	 */
	//--------------------------------------------
	public static String convertFile(File input)
		throws IOException
	{
		// get the contents of the file
		//
		String raw = readFile(input);
		return convertString(raw);
	}
	
	//--------------------------------------------
	/**
	 * Takes a given 'string' and replaces the old @xx@ or $xx style
	 * variable markers with the new style of ${xx}.  This also
	 * handles methods and arguments.
	 */
	//--------------------------------------------
	public static String convertString(String input)
	{
		// look for @xxx@ (and potentially @xxx.meth(zzz,zzz)@
		//
		String replace = doReplace(Pattern.compile(IC_PATTERN), input);
		
		// look for $xxx (and potentially $xxx.length() or $xxx.length)
		//
		return doReplace(Pattern.compile(VELOCITY_PATTERN), replace);
	}

	
	//--------------------------------------------
	/**
	 * Extract a File as a String
	 */
	//--------------------------------------------
	private static String readFile(File file)
		throws IOException
	{
		StringWriter out = new StringWriter();
		BufferedReader in = null;
		try
		{
			in = new BufferedReader(new FileReader(file));
			
			char[] buff = new char[16 * 1024];
			int c = 0;
			while ((c = in.read(buff)) > 0)
			{
				out.write(buff, 0, c);
			}
			in.close();
			in = null;
		}
		finally
		{
			if (in != null)
			{
				in.close();
			}
		}
		
		out.flush();
		out.close();
		return out.toString();
	}
	
	//--------------------------------------------
	/**
	 * 
	 */
	//--------------------------------------------
	private static String doReplace(Pattern pattern, String input)
	{
		// get a matcher
		//
		StringBuffer buff = new StringBuffer();
		Matcher match = pattern.matcher(input);
			
		// find/replace the segments
		//
		int start = 0;
		while (match.find(start) == true)
		{
			// get the range of characters that matched
			//
			int begin = match.start();
			int end = match.end();
			
			// add up-to-begin to our output buffer
			//
			if (start != begin)
			{
				buff.append(input.substring(start, begin));
			}
			
			// add replacement, first extract the pattern we found
			// then remove leading char (and possible trailing char)
			// and output in our new ${xxx} format
			//
			String group = match.group();
//System.out.println("  found match - '"+group+"'");
			if (group.startsWith("@") && group.endsWith("@"))
			{
				// replacing old style @xxx@, so just replace @ with new style
				//
				String replace = group.substring(1, group.length() - 1);
				
				// place the replacement in the new format
				//
				buff.append("${");
				buff.append(replace);
				buff.append('}');
			}
			else // (group.startsWith("$") == true)
			{
				// replacing velocity style $xxx.  This should have a trailing space, =, or !
				// Unfortunately, we run into a lot of situations with thes since there isn't
				// a well-defined 'end marker'.  Start at the $ and read characters until we find 
				// the logical end of the variable
				//
				int x = begin + 1;
				int parenCount = 0;
				while (true)
				{
					char ch = input.charAt(x);
					if (ch == '(')
					{
						// hit a parentheses, probably for a function
						//
						parenCount++;
					}
					else if (ch == ')')
					{
						parenCount--;
						if (parenCount <= 0)
						{
							break;
						}
					}
					else if (Character.isWhitespace(ch) == true || ch == '"')
					{
						// got a space or quote.  it may be part of the function parameters
						//
						if (parenCount <= 0)
						{
							break;
						}
					}
					else if (ch == '!' || ch == '=' || ch == '<' || ch == '>')
					{
						// should be at the end of the variable since we're doing a != or == or > or <
						//
						break;
					}
					
					x++;
				}
				
				// grab everything from the $ to the logical end
				//
				String replace = input.substring(begin + 1, x);
//				char extra = group.charAt(group.length() - 1);
				
				// place the replacement in the new format
				//
				buff.append("${");
				buff.append(replace);
				buff.append('}');
				
				// anything from x to 'end' should be inserted
				//
				if (x < end)
				{
					String remain = input.substring(x, end);
					buff.append(remain);
				}
				else
				{
					end = x;
				}
			}
			
			// move 'start' to the end of the matching expression location
			//
			start = end;
				
			// reset the matcher?
			//
//			match.reset();
		}
		
		// now that we're done, grab the rest of the string from 'start'
		// if nothing was found, this should copy the whole string
		//
		buff.append(input.substring(start));
		
		// return replacement
		//
		return buff.toString();
	}
	
	//--------------------------------------------
	/**
	 * Constructor for this class
	 */
	//--------------------------------------------
	private Converter()
	{
	}
}

//+++++++++++
//EOF
//+++++++++++