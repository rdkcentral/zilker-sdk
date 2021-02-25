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

package com.icontrol.util;

//-----------
// imports
//-----------

import java.io.PrintStream;
import java.util.ArrayList;

//-----------------------------------------------------------------------------
// Class definition:    GetOpt
/**
 *  Class to parse the command line arguments.  Works like
 *  standard UNIX getopt where you pass in the args passed
 *  into the program and a string that describes the list
 *  of available options.
 *  <p>
 *  Supports legacy -x notation and the newer --argument format
 **/
//-----------------------------------------------------------------------------
public final class GetOpt extends Object
{
	public static final char	HELP_OPTION				= 'h';
	public static final char	HELP2_OPTION			= '?';
	public static final char	REMAINING_ARGS_OPTION	= '*';
	
	// style for modern implementation
	//
	public static enum Style {
		NO_ARGS_STYLE, ONE_ARG_STYLE,	// identified by ':'
		NONE_OR_ONE_ARG_STYLE,			// identified by '.'
		MANY_ARGS_STYLE					// identified by '::'
	}
	
	// option and type (only 1 boolean flag can be true)
	//
	private char				legacy	= REMAINING_ARGS_OPTION;
	private String				option	= "";
	private String				description;
	private Style				style;
	private boolean				remain;
	private boolean				hidden;		// used by printUsage
	
	// values assigned during parsing of String[]
	//
	private ArrayList<String>	arguments;
	
	//-------------------------------------------------------------------------
	//-                                                                       -
	//------- S T A T I C   M E T H O D   I M P L E M E N T A T I O N S -------
	//-                                                                       -
	//-------------------------------------------------------------------------
	
	//--------------------------------------------
	/**
	 * Legacy style mechanism to parse the command line arguments with '-x' notation.  
	 * Works like standard UNIX getopt() where you pass in the program args 
	 * and a string that describes the list of available options.
	 * <p>
	 * opstring has the form of "abc:d:" where "a" and "b" are options and
	 * "c" and "d" expect additional arguments.  Two :: tells GetOpt to allow
	 * more than 1 string argument for that option.  For example: "d::" would
	 * alllow "foo -d x y" where "x y" are the values for option 'd'.  A '.'
	 * char after the option char tells GetOpt that there may or may-not be an argument.
	 * <p>
	 * Valid separators are:
	 * <ul>
	 * <li> ':'  - expect 1 argument
	 * <li> '::' - expect 1 or more arguments
	 * <li> '.'  - expect 0 or 1 argument
	 * <p>
	 * Options can be lumped together (ex: ls -al) or provided individually
	 * as long as none of them expect arguments.
	 * Options are can be space delimited or not (ex: -f filename | -ffilename).
	 * <p>
	 * This returns is a List of GetOpt objects.  Each will be able to describe
	 * the option found in the array and show the argument associated with 
	 * the option (if applicable).  
	 * A GetOpt object with an option of '*' will containe any arguments 
	 * listed after the options are complete.  This will be the last element 
	 * of the returned ArrayList and will respond to the 
	 * "getRemainingArguments()" method.
	 * <p>
	 * If an unknown option is encountered, an llegalArgumentException
	 * will be thrown.  The exception will have the unknown option in its
	 * message (exception.getMessage() method).
	 * <p>
	 * For example:
	 * <PRE>
	 	ArrayList opts = GetOpt.getopts(args, "h:s::p.v");
	 	for (int i = 0; i < opts.size(); ++i)
	 	{
	 		GetOpt opt = (GetOpt) opts.get(i);
	 		switch (opt.getOption())
	 		{
	 			case 'v':	// flag
	 			verbose = true;
	 			break;
	 
	 			case 'h':	// one argument
	 			host = opt.getArgument();
	 			break;
	 
	 			case 's':	// more than one argument
	 			list = opt.getArgumentList();
	 			break;
	 
	 			case 'p':	// 0 or 1 argument
	 			maybe = opt.getArgument();
	 			break;
	 
	 			case GetOpt.HELP_OPTION:
	 			default:
	 			printUsage();
	 		}
	 	}
	 * </PRE>
	 **/
	//--------------------------------------------
	public static ArrayList<GetOpt> getopts(String[] argv, String optstring)
		throws IllegalArgumentException
	{
		ArrayList<GetOpt> retVal = new ArrayList<GetOpt>();
		if (argv == null)
		{
			return retVal;
		}
		
		// parse optstring to get the possible options to look for
		//
		ArrayList<GetOpt> possible = parseLegacyOptstring(optstring);
		GetOpt remainArgs = new GetOpt(REMAINING_ARGS_OPTION);
		
		// loop through 'argv'
		//
		GetOpt currOption = null;
		for (int i = 0; i < argv.length; i++)
		{
			String currArg = argv[i];
			if (currArg == null || currArg.length() == 0)
			{
				continue;
			}
			
			// look for '-' options
			//
			if (currArg.startsWith("-"))
			{
				// strip off the '-' and then look at the
				// option(s).  There could be more than one
				// slapped together (ex: ls -al)
				//
				String remain = currArg.substring(1);
				
				// go through the chars in "currChars"
				//
				for (int j = 0, count = remain.length(); j < count; j++)
				{
					// get the character and locate the GetOpt
					//
					char c = remain.charAt(j);
					currOption = findLegacy(c, possible);
					if (currOption == null)
					{
						throw new IllegalArgumentException("'" + c + "' is not a valid option");
					}
					retVal.add(currOption);
					
					// if "currOption" doesn't want arguments, than just
					// add to the return list and keep processing 
					// characters from the "remain" string
					//
					if (currOption.wantsNone() == true)
					{
						currOption = null;
					}
					else if (j + 1 < count)
					{
						// user messed up.  Basically there are more things in this
						// single -xyz but the one we're on wants an argument
						//
						throw new IllegalArgumentException("'Option " + c + "' should not be combined with other options");
					}
				}
			}
			else
			{
				// we have a string that should either be added to 'currOption' or needs
				// to be added as the "remaining args option"
				//
				if (currOption == null)
				{
					// add to remaining args
					//
					remainArgs.arguments.add(currArg);
				}
				else
				{
					currOption.arguments.add(currArg);
					if (currOption.wantsOne() == true)
					{
						currOption = null;
					}
				}
			}
		}
		
		// check everything in retVal to make sure there isn't 
		// anything missing or extra in there
		//
		checkList(retVal);
		
		// append the remainingArgs if it has anything
		//
		if (remainArgs.hasArguments() == true)
		{
			retVal.add(remainArgs);
		}
		
		return retVal;
	}
	
	//--------------------------------------------
	/**
	 * Modern style mechanism to parse the command line arguments with '--string' notation.  
	 * Requires the program args and a List of GetOpt objects.
	 * <p>
	 * Options can NOT be lumped together (ex: ls -al)
	 * <p>
	 * This returns is a List of GetOpt objects.  Each will be able to describe
	 * the option found in the array and show the argument associated with 
	 * the option (if applicable).  
	 * A GetOpt object with an option of '*' will contain any arguments 
	 * listed after the options are complete.  This will be the last element 
	 * of the returned ArrayList and will respond to the 
	 * "getRemainingArguments()" method.
	 * <p>
	 * If an unknown option is encountered, an llegalArgumentException
	 * will be thrown.  The exception will have the unknown option in its
	 * message (exception.getMessage() method).
	 **/
	//--------------------------------------------
	public static ArrayList<GetOpt> getopts(String[] argv, ArrayList<GetOpt> options)
			throws IllegalArgumentException
	{
		ArrayList<GetOpt> retVal = new ArrayList<GetOpt>();
		if (argv == null)
		{
			return retVal;
		}
		
		// Create remainArgs in case it becimes necessary
		//
		GetOpt remainArgs = new GetOpt(REMAINING_ARGS_OPTION);
		
		// loop through 'argv'
		//
		GetOpt currOption = null;
		for (int i = 0; i < argv.length; i++)
		{
			String currArg = argv[i];
			if (currArg == null || currArg.length() == 0)
			{
				continue;
			}
			
			// look for '--' options (unless this has the remain flag)
			//
			if (currArg.startsWith("--") && (currOption == null || currOption.isRemain() == false))
			{
				// strip off the '--' and then find the corresponding GetOpt
				//
				String remain = currArg.substring(2);
				currOption = findModern(remain, options);
				if (currOption == null)
				{
					throw new IllegalArgumentException("'" + remain + "' is not a valid option");
				}
				retVal.add(currOption);
				
				// if "currOption" doesn't want arguments, than just
				// add to the return list and keep processing 
				//
				if (currOption.wantsNone() == true)
				{
					currOption = null;
				}
			}
			else
			{
				// we have a string that should either be added to 'currOption' or needs
				// to be added as the "remaining args option"
				//
				if (currOption == null)
				{
					// add to remaining args
					//
					remainArgs.arguments.add(currArg);
				}
				else
				{
					currOption.arguments.add(currArg);
					if (currOption.wantsOne() == true)
					{
						currOption = null;
					}
				}
			}
		}
		
		// check everything in retVal to make sure there isn't 
		// anything missing or extra in there
		//
		checkList(retVal);
		
		// append the remainingArgs if it has anything
		//
		if (remainArgs.hasArguments() == true)
		{
			retVal.add(remainArgs);
		}
		
		return retVal;
	}
	
	//--------------------------------------------
	/**
	 * Shows a usage of the supplied options.  Works best
	 * on Legacy style options 
	 **/
	//--------------------------------------------
	public static void printUsage(PrintStream out, String optString)
	{
       ArrayList<GetOpt> possible = parseLegacyOptstring(optString);
       printUsage(possible, out);
	}
	
	//--------------------------------------------
	/**
	 * Shows a usage of the supplied options.  Works best
	 * on Modern style options since they have descriptions
	 **/
	//--------------------------------------------
	public static void printUsage(ArrayList<GetOpt> options, PrintStream out)
	{
		out.println("Usage:");
		
		// first find the largest option or option string
		//
		int max = 0;
		int argMax = 0;
		for (int i = 0, count = options.size(); i < count; i++)
		{
			GetOpt curr = options.get(i);
			int size = 1;
			if (curr.isHidden() == true)
			{
				continue;
			}
			
			if (curr.isModern() == true)
			{
				size = curr.getOptionString().length();
			}
			
			if (size > max)
			{
				max = size;
			}
			
			int arg = 0;
			if (curr.wantsOneOrNone() == true || curr.wantsOne() == true)
			{
				// for "<arg>" or "[arg]"
				//
				arg = 5;
			}
			else if (curr.wantsMany() == true)
			{
				// for "<arg>,..."
				//
				arg = 9;
			}
			
			if (arg > argMax)
			{
				argMax = arg;
			}
		}
		
		// add 2 for the '--'
		//
		max += 3;
		
		// now print each option to 'out'
		//
		for (int i = 0, count = options.size(); i < count; i++)
		{
			// get the option string
			//
			GetOpt curr = options.get(i);
			String option = null;
			if (curr.isHidden() == true)
			{
				continue;
			}
			
			if (curr.isModern() == true)
			{
				option = "--" + curr.getOptionString();
			}
			else
			{
				option = '-' + Character.toString(curr.getOption());
			}
			
			// pad to the 'max'
			//
			out.print("   ");
			out.print(pad(option, max));
			
			// show the argument style
			//
			if (curr.wantsOneOrNone() == true)
			{
				out.print(pad("[arg]", argMax));
			}
			else if (curr.wantsOne() == true)
			{
				out.print(pad("<arg>", argMax));
			}
			else if (curr.wantsMany() == true)
			{
				out.print(pad("<arg,...>", argMax));
			}
			else
			{
				out.print(pad("", argMax));
			}
			
			// show the description
			//
			String desc = curr.getDescription();
			if (desc != null)
			{
				out.print(" : ");
				out.print(desc);
			}
			out.println();
		}
	}
	
	//--------------------------------------------
	/**
	 *  
	 **/
	//--------------------------------------------
	private static String pad(String input, int num)
	{
		StringBuffer retVal = new StringBuffer(input);
		
		int len = input.length();
		while (len < num)
		{
			retVal.append(' ');
			len++;
		}
		
		return retVal.toString();
	}
	
	//-------------------------------------------------------------------------
	//
	//--------------- M E T H O D   I M P L E M E N T A T I O N S -------------
	//
	//-------------------------------------------------------------------------
	
	//-******************************************-
	//-*
	//-*  Constructors
	//-*
	//-******************************************-
	
	//--------------------------------------------
	/**
	 *  Modern style constructor.
	 **/
	//--------------------------------------------
	public GetOpt(String option, Style style)
	{
		this(option, style, null);
	}
	
	//--------------------------------------------
	/**
	 *  Modern style constructor.
	 **/
	//--------------------------------------------
	public GetOpt(String option, Style style, String desc)
	{
		this.option = option;
		this.arguments = new ArrayList<String>();
		setStyle(style);
		setDescription(desc);
	}
	
	//--------------------------------------------
	/**
	 *  Legacy style constructor 
	 **/
	//--------------------------------------------
	public GetOpt(char opt, Style style, String desc)
	{
		this.legacy = opt;
		this.option = null;
		this.arguments = new ArrayList<String>();
		setStyle(style);
		setDescription(desc);
	}
	
	//--------------------------------------------
	/**
	 *  Legacy style constructor 
	 **/
	//--------------------------------------------
	private GetOpt(char opt)
	{
		this.legacy = opt;
		this.option = null;
		this.arguments = new ArrayList<String>();
		
		setStyle(Style.NO_ARGS_STYLE);
	}
	
	//-******************************************-
	//-*
	//-*  Public Methods
	//-*
	//-******************************************-
	
	//--------------------------------------------
	/**
	 *  Returns the char this option represents.
	 *  Only valid if this is a legacy option
	 **/
	//--------------------------------------------
	public char getOption()
	{
		return legacy;
	}
	
	//--------------------------------------------
	/**
	 *  Returns the string this option represents.
	 *  Only valid if this is a modern option
	 **/
	//--------------------------------------------
	public String getOptionString()
	{
		return option;
	}
	
	//--------------------------------------------
	/**
	 * @return Returns the description.
	 */
	//--------------------------------------------
	public String getDescription()
	{
		return description;
	}
	
	//--------------------------------------------
	/**
	 * Sets a description on the object.  Helpfull if
	 * using the printUsage() method.
	 */
	//--------------------------------------------
	public void setDescription(String val)
	{
		this.description = val;
	}
	
	//--------------------------------------------
	/**
	 * @return Returns the remain.
	 */
	//--------------------------------------------
	public boolean isRemain()
	{
		return remain;
	}

	//--------------------------------------------
	/**
	 * @param remain The remain to set.
	 */
	//--------------------------------------------
	public void setRemain(boolean flag)
	{
		this.remain = flag;
	}

	//--------------------------------------------
	/**
	 *  Returns true if this is a legacy style GetOpt
	 *  meaning it expects -x notation
	 **/
	//--------------------------------------------
	public boolean isLegacy()
	{
		return (option == null);
	}
	
	//--------------------------------------------
	/**
	 *  Returns true if this is a modern style GetOpt
	 *  meaning it expects --string notation
	 **/
	//--------------------------------------------
	public boolean isModern()
	{
		return (option != null);
	}
	
	//--------------------------------------------
	/**
	 *  Return true if this is HELP or HELP2
	 **/
	//--------------------------------------------
	public boolean isHelpOption()
	{
		if (isLegacy() == true)
		{
			return (legacy == HELP_OPTION || legacy == HELP2_OPTION);
		}
		else
		{
			return (option.equals(Character.toString(HELP_OPTION))
					|| option.equals(Character.toString(HELP2_OPTION)) || option
					.equalsIgnoreCase("help"));
		}
	}
	
	//--------------------------------------------
	/**
	 * @return Returns the hidden.
	 */
	//--------------------------------------------
	public boolean isHidden()
	{
		return hidden;
	}

	//--------------------------------------------
	/**
	 * @param flag The hidden to set.
	 */
	//--------------------------------------------
	public void setHidden(boolean flag)
	{
		this.hidden = flag;
	}

	//--------------------------------------------
	/**
	 *  Returns true if this has an argument to provide
	 **/
	//--------------------------------------------
	public boolean hasArguments()
	{
		return (arguments.size() > 0);
	}
	
	//--------------------------------------------
	/**
	 *  Returns the arguments concatenated together with a space
	 **/
	//--------------------------------------------
	public String getArgument()
	{
		if (hasArguments() == false)
		{
			return null;
		}
		
		StringBuffer buf = new StringBuffer();
		for (int i = 0, count = arguments.size(); i < count; i++)
		{
			buf.append(arguments.get(i));
			if ((i + 1) < count)
			{
				buf.append(' ');
			}
		}
		
		return buf.toString().trim();
	}
	
	//--------------------------------------------
	/**
	 *  Will return a list of Strings that tokenized the argument on " "
	 **/
	//--------------------------------------------
	public ArrayList<String> getArgumentList()
	{
		return this.arguments;
	}
	
	//--------------------------------------------
	/**
	 *  The number if elements in the getArgumentList
	 **/
	//--------------------------------------------
	public int argumentsSize()
	{
		return arguments.size();
	}
	
	//--------------------------------------------
	/**
	 *  The option in the argv[] format
	 **/
	//--------------------------------------------
	public String toString()
	{
		if (option != null)
		{
			return "--" + option;
		}
		else
		{
			return "-" + legacy;
		}
	}
	
	//-******************************************-
	//-*
	//-*  Private Methods
	//-*
	//-******************************************-
	
	//--------------------------------------------
	/**
	 *  
	 **/
	//--------------------------------------------
	private void setStyle(Style style)
	{
		this.style = style;
	}
	
	//--------------------------------------------
	/**
	 *  
	 **/
	//--------------------------------------------
	private boolean wantsNone()
	{
		return (style == Style.NO_ARGS_STYLE);
	}
	
	//--------------------------------------------
	/**
	 *  
	 **/
	//--------------------------------------------
	private boolean wantsOne()
	{
		return (style == Style.ONE_ARG_STYLE);
	}
	
	//--------------------------------------------
	/**
	 *  
	 **/
	//--------------------------------------------
	private boolean wantsOneOrNone()
	{
		return (style == Style.NONE_OR_ONE_ARG_STYLE);
	}
	
	//--------------------------------------------
	/**
	 *  
	 **/
	//--------------------------------------------
	private boolean wantsMany()
	{
		return (style == Style.MANY_ARGS_STYLE);
	}
	
	//--------------------------------------------
	/**
	 *  
	 **/
	//--------------------------------------------
	private static ArrayList<GetOpt> parseLegacyOptstring(String optstring)
	{
		ArrayList<GetOpt> retVal = new ArrayList<GetOpt>();
		if (optstring == null || optstring.length() == 0)
		{
			return retVal;
		}
		
		// loop through the chars if the optstring
		//
		char curr;
		char next;
		for (int i = 0, count = optstring.length(); i < count; i++)
		{
			// get the current one and peek at the next one
			// to easily check for '.' or ':'
			//
			curr = optstring.charAt(i);
			if (i + 1 < count)
			{
				next = optstring.charAt(i + 1);
			}
			else
			{
				next = ' ';
			}
			
			// go to next char if '.' or ':'
			//
			if (curr == '.' || curr == ':')
			{
				continue;
			}
			
			// make the GetOpt object 
			//
			GetOpt opt = new GetOpt(curr);
			if (next == '.')
			{
				opt.setStyle(Style.NONE_OR_ONE_ARG_STYLE);
			}
			else if (next == ':')
			{
				opt.setStyle(Style.ONE_ARG_STYLE);
				
				// see if there are 2 ':'
				//
				if (i + 2 < count && optstring.charAt(i + 2) == ':')
				{
					opt.setStyle(Style.MANY_ARGS_STYLE);
				}
			}
			retVal.add(opt);
		}
		
		return retVal;
	}
	
	//--------------------------------------------
	/**
	 *
	 **/
	//--------------------------------------------
	private static GetOpt findLegacy(char search, ArrayList<GetOpt> list)
	{
		for (int i = 0, count = list.size(); i < count; i++)
		{
			GetOpt curr = list.get(i);
			if (curr.legacy == search)
			{
				return curr;
			}
		}
		
		return null;
	}
	
	//--------------------------------------------
	/**
	 *
	 **/
	//--------------------------------------------
	private static GetOpt findModern(String search, ArrayList<GetOpt> list)
	{
		for (int i = 0, count = list.size(); i < count; i++)
		{
			GetOpt curr = list.get(i);
			if (search.equals(curr.option) == true)
			{
//				return curr;
				
				// make a copy
				//
				GetOpt retVal = new GetOpt(curr.getOptionString(), curr.style, curr.description);
				retVal.setRemain(curr.isRemain());
				return retVal;
			}
		}
		
		return null;
	}
	
	//--------------------------------------------
	/**
	 *
	 **/
	//--------------------------------------------
	private static void checkList(ArrayList<GetOpt> list)
	{
		// check everything in retVal to make sure there isn't 
		// anything missing or extra in there
		//
		for (int i = 0, count = list.size(); i < count; i++)
		{
			GetOpt curr = list.get(i);
			if (curr.wantsNone() == true && curr.hasArguments() == true)
			{
				throw new IllegalArgumentException("'Option " + curr
						+ "' was supplied argument, but did not expect any");
			}
			else if (curr.wantsOne() == true && curr.argumentsSize() != 1)
			{
				throw new IllegalArgumentException("'Option " + curr
						+ "' expects 1 argument, but has "
						+ curr.arguments.size());
			}
			else if (curr.wantsOneOrNone() == true && curr.argumentsSize() > 1)
			{
				throw new IllegalArgumentException("'Option " + curr
						+ "' expects 0 or 1 argument, but has "
						+ curr.arguments.size());
			}
			else if (curr.wantsMany() == true && curr.hasArguments() == false)
			{
				throw new IllegalArgumentException("'Option " + curr
						+ "' expects 1 or more argument, but has none");
			}
		}
	}

/**
	public static void main(String[] args)
	{
		if (args.length == 0)
		{
			System.exit(1);
		}
		
		// test modern
		//
		ArrayList list = new ArrayList();
		list.add(new GetOpt("none", NO_ARGS_STYLE));
		list.add(new GetOpt("one", ONE_ARG_STYLE));
		list.add(new GetOpt("many", MANY_ARGS_STYLE));
		list.add(new GetOpt("yes", NONE_OR_ONE_ARG_STYLE));
		ArrayList tmp = getopts(args, list);
		for (int i = 0, count = tmp.size() ; i < count ; i++)
		{
			GetOpt curr = (GetOpt)tmp.get(i);
			System.out.println("option "+curr+" has arguments "+curr.getArgument());
		}
		System.exit(0);
	}
 **/
}

//+++++++++++
// EOF
//+++++++++++
