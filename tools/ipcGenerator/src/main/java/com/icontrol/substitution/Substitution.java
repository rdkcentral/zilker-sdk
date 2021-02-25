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
// imports
//-----------

import java.util.ArrayList;
import java.util.Calendar;
import java.util.List;
import java.util.Random;
import java.util.StringTokenizer;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.icontrol.substitution.macro.ExpressionFactory;
import com.icontrol.substitution.macro.InvalidMacroException;
import com.icontrol.substitution.macro.Macro;
import com.icontrol.substitution.macro.MacroExpression;

//-----------------------------------------------------------------------------
// Class definition:    Substitution2
/**
 *  Utility to sed-style substitute variables.
 *  Variables are in ${xxx} format.  
 *  <p>
 *  Along with substition, variables can have methods executed
 *  against their values.  These have the format of ${xxx.method}
 *  and are limited in nature.  Generally, these are used for
 *  math, date or string operations.
 */
//-----------------------------------------------------------------------------
public class Substitution extends Object
{
	// pattern to look for that describes ${xxx} formated variables and methods
	//
//	private static final String	SEARCH_PATTERN		= "\\$\\{[\\w\\.\\(,\\)]*\\}";
	//private static final String	SEARCH_PATTERN		= "\\$\\{\\w+(\\}|\\..+\\})";
	
	// internal hard-coded variables
	//
	public static final String TODAY_KEY			= "${TODAY}";		// needs format methods
	public static final String YESTERDAY_KEY		= "${YESTERDAY}";	// needs format methods
	public static final String TOMORROW_KEY			= "${TOMORROW}";	// needs format methods
	public static final String TEMP_DIR_KEY			= "${TEMP_DIR}";	// return java.io.tmpdir value
	public static final String RANDOM_NUM_KEY		= "${RANDOM}";		// return a Random number
	
	// method to take the variable's value
	//
	public static final String VALUE_METHOD			= ".VALUE";			// treats substitution as a key and gets it's value
	public static final String EMPTY_METHOD			= ".EMPTY";			// if var is null, returns "" instead of @var@
	
    private static Logger logger = LoggerFactory.getLogger(Substitution.class);
	private static ArrayList<MethodResolver>	global;
	private static InternalSource				internalSource;
	
	private ArrayList<SubstitutionSource>		sources;
	private ArrayList<MethodResolver>			resolvers;
	
	private boolean    ignoreMacros = false;

	//-------------------------------------------------------------------------
	//-                                                                       -
	//--------------- M E T H O D   I M P L E M E N T A T I O N S -------------
	//-                                                                       -
	//-------------------------------------------------------------------------

	static
	{
		// make global source
		//
		internalSource = new InternalSource();
		
		// add global resolvers
		//
		global = new ArrayList<MethodResolver>();
//		global.add(new StringMethodResolver());
//		global.add(new DateMethodResolver());
//		global.add(new NumberMethodResolver());
//		global.add(new CollectionMethodResolver());
	}
	
	//--------------------------------------------
	/**
	 * Convenience method to copy a substitution source that we want to modify
	 */
	//--------------------------------------------
	public static Substitution copySubstitution(Substitution other)
	{
		Substitution retval = new Substitution();
		
		// copy sources
		//
		List<SubstitutionSource> otherSources = other.getSources();
		for (int cnt = 0; cnt < otherSources.size(); cnt++)
		{
			// skip internal since it was added by 'retval' constructor
			//
			SubstitutionSource src = otherSources.get(cnt);
			if ((src instanceof InternalSource) == false)
			{
				retval.addSubstitutionSource(src);
			}
		}
		
		// copy resolvers
		//
		List<MethodResolver> otherResolvers = other.getResolvers();
		for (int cnt = 0; cnt < otherResolvers.size(); cnt++)
		{
			// skip global since those were added by 'retval' constructor
			//
			MethodResolver res = otherResolvers.get(cnt);
			if (global.contains(res) == false)
			{
				retval.addMethodResolver(res);
			}
		}
		
		return retval;
	}
	
	//--------------------------------------------
	/**
	 * Adds a MethodResolver to the global static list
	 * so that each instance of Substitution has access
	 * to this resolver
	 */
	//--------------------------------------------
	public static void addGlobalMethodResolver(MethodResolver impl)
	{
		global.add(impl);
	}
	
	//-******************************************-
	//-*
	//-*  Constructors
	//-*
	//-******************************************-

	//--------------------------------------------
	/**
	 * Constructor for this object
	 */
	//--------------------------------------------
	public Substitution()
	{
		sources = new ArrayList<SubstitutionSource>();
		sources.add(internalSource);
		
		// prime our resolvers with the global list
		//
		resolvers = new ArrayList<MethodResolver>();
		resolvers.addAll(global);
	}
	
	//-******************************************-
	//-*
	//-*  Public Methods
	//-*
	//-******************************************-

	//--------------------------------------------
	/**
	 * Adds a SubstitutionSource to the list to 
	 * ask during processing.
	 */
	//--------------------------------------------
	public void addSubstitutionSource(SubstitutionSource src)
	{
		if (src != null && sources.contains(src) == false)
		{
			sources.add(src);
		}
	}

	//--------------------------------------------
	/**
	 * Returns the list of SubstitutionSource known.
	 */
	//--------------------------------------------
	public ArrayList<SubstitutionSource> getSources()
	{
		return sources;
	}
	
	//--------------------------------------------
	/**
	 * Adds a MethodResolver to the list to 
	 * ask during processing.
	 */
	//--------------------------------------------
	public void addMethodResolver(MethodResolver src)
	{
		if (resolvers.contains(src) == false)
		{
			// add to the front so it's used BEFORE the built-in ones
			//
			resolvers.add(0, src);
		}
	}

	//--------------------------------------------
	/**
	 * @return the resolvers
	 */
	//--------------------------------------------
	public ArrayList<MethodResolver> getResolvers()
	{
		return resolvers;
	}
	
	//--------------------------------------------
    /**
     * @return Returns the ignoreMacros.
     */
    //--------------------------------------------
    public boolean isIgnoreMacros()
    {
        return ignoreMacros;
    }

    //--------------------------------------------
    /**
     * @param ignoreMacros The ignoreMacros to set.
     */
    //--------------------------------------------
    public void setIgnoreMacros(boolean flag)
    {
        this.ignoreMacros = flag;
    }

    //--------------------------------------------
	/**
	 *  If the supplied "string" contains any ${xxx}
	 *  style strings, each will be translated against
	 *  known variables and any SubstitutionSources
	 *  that have been defined.
	 *  <p>
	 *  If the "string" contains macros, those will be
	 *  taken into account during the translation.
	 **/
	//--------------------------------------------
	public String substitute(String string)
	{
		if (string == null || string.equalsIgnoreCase("null"))
		{
			return null;
		}
		
		if (ignoreMacros == false)
		{
		    // see if there is an embedded Macro definition (should be a #if in the string)
		    //
		    if (string.indexOf(ExpressionFactory.IF_MARKER) != -1)
		    {
		        try
		        {
		            // parse and execute the macro
		            //
		            Macro macro = ExpressionFactory.parseMacro(string);
		            String replace = macro.evaluateMacro(this);

		            // now swap out the text this macro occupied with the 'replacement'
		            //
		            StringBuffer tmp = new StringBuffer();
		            int start = macro.getSpanStart();
		            if (start > 0)
		            {
		                // add text up-to the macro beginning
		                //
		                tmp.append(string.substring(0, start));
		            }
		            if (replace != null)
		            {
		                // add the replacement
		                //
		                tmp.append(replace);
		            }

		            // now the stuff after the macro
		            //
		            int end = macro.getSpanEnd();
		            if (end < string.length())
		            {
		                tmp.append(string.substring(end));
		            }

		            // run through our 'substitute' again to deal with
		            // sub-macros or additional macros that may exist
		            //
		            return substitute(tmp.toString());
		        }
		        catch (Exception ex)
		        {
		            logger.warn(ex.getMessage(), ex);
		        }
		    }
		}
		
		// see if there is even any variables in the string to process.
		//
		if (string.indexOf("${") == -1)
		{
		    // see if legacy
		    //
		    if (string.indexOf("@") != -1)
		    {
		        return legacySubstitute(string);
		    }
			return string;
		}

		// look for ${....} patterns and treat as Variables
		//
		StringBuffer retVal = new StringBuffer();
		int start = 0;
		int end = string.length();

		while (start < end)
		{
			// look for beginning ${
			//
			int a = string.indexOf("${", start);
			if (a == -1)
			{
				// add what is left
				//
				retVal.append(string.substring(start));
				break;
			}

			// find matching }
			//
			int b = corresponding(string, a+2);
			if (b == -1)
			{
				retVal.append(string.substring(start));
				break;
			}

			// extract our variable then translate it
			//
			String variable = string.substring(a+2, b);
			String value = translate(variable);

			// add "source" up to the variable
			//
			retVal.append(string.substring(start, a));
			if (value != null)
			{
				retVal.append(value);
			}

			start = b+1;
		}
//logger.debug("returning substitutuion\n"+retVal);

		return retVal.toString();
	}
	
	//--------------------------------------------
	/**
	 *  Given a variable (${xxx} or xxx format), will
	 *  iterate through the SubstitutionSource list
	 *  to locate the variable and return it's value.
	 *  <p>
	 *  If the variable has methods attached, each
	 *  will be executed prior to returning the value.
	 *  <p>
	 *  Returns null if the variable is not located
	 */
	//--------------------------------------------
	public String translate(String variable)
	{
		// deal with null input
		//
		if (variable == null || variable.length() == 0)
		{
			return null;
		}
		
		// strip ${  } characters
		//
		if (variable.startsWith("${") && variable.endsWith("}") == true)
		{
			variable = variable.substring(2, variable.length() - 1);
		}
		
		// save off the full variable name for logging or errors
		//
		String fullSrc = "${"+variable+"}";
		
		// see if we have 'methods' in this variable to execute
		//
		ArrayList<String> methods = null;
		int index = variable.indexOf('.');
		if (index != -1)
		{
			// if xxx.zzz, xxx is the src and zzz is the method
			// Get the methods and any arguments they have
			//
			methods = getMethods(variable, index);
			
			// the variable name should be up to the dot
			//
			variable = variable.substring(0, index);
			//logger.debug("extracted variable="+variable+" methods="+methods+" from src="+fullSrc);
			fullSrc = "${"+variable+"}";
		}
		
		// use our SubstitutionSource objects to resolve the variable
		//
		Object retVal = null;
		for (int i = 0, count = sources.size() ; i < count ; i++)
		{
			SubstitutionSource owner = sources.get(i);

			// first look to see if this owner has this as a String
			// then if it has it as an Object
			//
			if (owner.hasStringVariable(variable) == true)
			{
				// get the value
				//
				retVal = owner.getVariable(variable);
			}
			else if (owner.hasObjectVariable(variable) == true)
			{
				retVal = owner.getObject(variable);
			}

			if (retVal != null)
			{
				break;
			}
		}			
		
		// show what the translation was in the log
		//
		//logger.debug(fullSrc+" initially translated to "+retVal);
		
		/*if (retVal == null && methods == null)
		{
			// not found, so return the original
			//
			logger.debug("Unable to find translation for "+fullSrc);
			//retVal = original;
		}*/
		
		if (retVal == null && methods != null)
		{
			// look for the EMPTY method
			//
			if (methods.contains(EMPTY_METHOD) == true)
			{
				//logger.debug("Unable to find translation for "+fullSrc+", however EMPTY was used");
				
				// just return an empty string
				//
				retVal = "";
				
				// remove the EMPTY_METHOD and continue
				//
				methods.remove(EMPTY_METHOD);
				if (methods.size() == 0)
				{
					// nothing more to execute
					//
					return "";
				}
			}
			/*else
			{
				// not found, so log that
				//
				logger.debug("Unable to find translation for "+fullSrc);
			}*/

			// probably running a method against a string
			// 
			retVal = runMethods(variable, methods);
//			if (retVal == null)
//			{
//				retVal = original;
//			}
		}

		else if (retVal != null && methods != null)
		{
			// execute the "methods"
			//
			retVal = runMethods(retVal, methods);
//			if (retVal == null)
//			{
//				retVal = original;
//			}
		}

		// return the string value of whatever we have
		//
		if (retVal != null)
		{
		    return substitute(retVal.toString());
		}

		return null;
	}
	
   //--------------------------------------------
    /**
     *  If the supplied "string" contains any @xxx@
     *  style strings, it will be substituted out
     *  against known variables and any SubstitutionSources
     *  that have been defined.
     **/
    //--------------------------------------------
    public String legacySubstitute(String string)
    {
        if (string == null || string.equalsIgnoreCase("null"))
        {
            return null;
        }
        
        // make sure value has @ characters
        //
        if (string.indexOf("@") == -1)
        {
            return string;
        }

        // look for @....@ pairs and treat as Variables
        //
        StringBuffer retVal = new StringBuffer();
        int start = 0;
        int end = string.length();

        while (start < end)
        {
            // look for beginning @
            //
            int a = string.indexOf('@', start);
            if (a == -1)
            {
                retVal.append(string.substring(start));
                break;
            }

            // find matching @
            //
            int b = string.indexOf('@', a+1);
            if (b == -1)
            {
                retVal.append(string.substring(start));
                break;
            }

            String value = null;
            if (a+1 == b)
            {
                // had "@@", so replace with a single @ character
                //
                value = "@";
            }
            else
            {
                // substitute out the variable between 'a' and 'b'
                //
                String variable = string.substring(a+1, b);
		//logger.debug("   trying to substitute out variable '"+variable+"'");
                value = translate(variable);
            }

            // add "source" up to the variable
            //
            retVal.append(string.substring(start, a));
            if (value != null)
            {
                retVal.append(value);
            }

            start = b+1;
        }
        
	//logger.debug("returning substitutuion\n"+retVal);

        return retVal.toString();
    }


	
	//--------------------------------------------
	/**
	 *  Parses the given macroLine" as a MacroExpression
	 *  and returns if it was successful or not.
	 *  If the macroLine does not start with "#if", then
	 *  it is assumed to be simplistic (ex: ${x} == 'true')
	 */
	//--------------------------------------------
	public boolean evaluate(String macroLine)
		throws InvalidMacroException
	{
		// parse as a MacroExpression then evaluate it
		//
		MacroExpression exp = ExpressionFactory.parseMacroLine(macroLine);
		return exp.evaluate(this);
	}
	
	//-******************************************-
	//-*
	//-*  Private Methods
	//-*
	//-******************************************-
	
	//--------------------------------------------
	/**
	 *  
	 */
	//--------------------------------------------
	private int corresponding(String input, int start)
	{
		int len = input.length();
		int x = 0;
		while (start < len)
		{
			char ch = input.charAt(start);
			if (ch == '{')
			{
				// found { inside the variable (for some reason)
				// increment the marker and continue on
				//
				x++;
			}
			else if (ch == '}')
			{
				// found end marker, see if this is the end of our variable
				// or the end of an additional marker
				//
				x--;
				if (x < 0)
				{
					return start;
				}
			}
			
			start++;
		}
		
		// not found
		// 
		return -1;
	}
	
	//--------------------------------------------
	/**
	 *  Expects source to be without @ characters
	 */
	//--------------------------------------------
	private String runMethods(Object source, ArrayList<String> methods)
	{
		// loop through each method in the list.  This way we can
		// handle nested executions (ex: foo.trim.toUpper)
		//
		int size = resolvers.size();
		for (int i = 0, count = methods.size() ; i < count ; i++)
		{
			String method = methods.get(i);
			logger.debug("Attempting to resolve and execute method "+method);
		
			if (method.indexOf('(') != -1)
			{
				// remove args from the method so we can
				// correctly identify where it belongs
				//
				method = trimMethod(method);
			}
			
			// special case of .VALUE
			//
			if (method.equalsIgnoreCase(VALUE_METHOD) == true)
			{
				String val = translate(source.toString());
				if (val != null && val.equals("${"+source+"}") == false)
				{
					source = val;
				}
				else
				{
//					source = null;
					return null;
				}
				
				continue;
			}
			else if (method.equalsIgnoreCase(EMPTY_METHOD) == true)
			{
				// only applicable if the value is null
				// it should have been handled above
				//
				if (source == null || source.toString().trim().length() == 0)
//				String val = translate(source.toString());
//				if (val == null)
				{
					return "";
				}
				
				continue;
			}
			else if (source == null)
			{
				return null;
			}
			
			// loop through our MethodResolvers to see which one knows
			// how to deal with this method and value
			//
			boolean resolved = false;
			for (int j = 0 ; j < size ; j++)
			{
				MethodResolver resolver = resolvers.get(j);
				if (resolver != null && resolver.supportsMethod(method, source.getClass()) == true)
				{
					// ask the resolver how many arguments it's expecting
					//
					ArrayList<String> args = null;
					int num = resolver.getArgCount(method);
					if (num != 0)
					{
						// use original method, not the trimmed one
						//
						String orig = methods.get(i);
						args = getMethodArgs(orig, num);
					}
					
					// handle String differently
					//
					if (source instanceof String)
					{
						source = resolver.executeMethod((String)source, method, args, this);
					}
					else
					{
						source = resolver.executeMethod(source, method, args, this);
					}
					
					resolved = true;
					break;
				}
			}
			
			if (resolved == false)
			{
				// we return null here, because if we cannot find the method, then our caller should
				// return the original string.  This insures that things like email addresses aren't
				// mistakenly taken as methods.
				//
				logger.debug("Unknown variable method \""+method+"\"; ignoring method");
				return null;
			}
		}
		
		if (source != null)
		{
			return source.toString();
		}
		return null;
	}
	
	//--------------------------------------------
	/**
	 *  Given a string, returns the methods associated
	 *  (ex: @foo.trim.toUpperCase)
	 */
	//--------------------------------------------
	private ArrayList<String> getMethods(String str, int dot)
	{
		ArrayList<String> retVal = new ArrayList<String>();
		
		while (dot != -1)
		{
			int end = str.indexOf('.', dot+1);
			if (end != -1)
			{
				// has more methods
				//
				String method = str.substring(dot, end);
				retVal.add(method);
			}
			else
			{
				// last method
				//
				String method = str.substring(dot);
				retVal.add(method);
			}
			
			// go to the last one and keep going
			//
			dot = end;
		}
		
		if (retVal.size() == 0)
		{
			return null;
		}
		
		return retVal;
	}
	
	//--------------------------------------------
	/**
	 *  Given a method, removes the args
	 *  (ex: substring(0,1) --> substring)
	 */
	//--------------------------------------------
	private String trimMethod(String method)
	{
		if (method == null)
		{
			return null;
		}
		
		// See if there are parenthesis to the method which
		// indicate arguments (seperated by comma)
		//
		int start = method.indexOf('(');
		
		if (start != -1)
		{
			return method.substring(0, start);
		}
		return method;
	}
	
	//--------------------------------------------
	/**
	 *  Returns the arguments to a method, or null
	 *  of none were defined.  Args are defined as
	 *  strings seperated by comma and surrounded 
	 *  by parentheses.  For example: xxx(1, 2, 3) 
	 */
	//--------------------------------------------
	private ArrayList<String> getMethodArgs(String method, int expected)
	{
		if (expected == 0)
		{
			return null;
		}
		
		// See if there are parenthesis to the method which
		// indicate arguments (seperated by comma)
		//
		int start = method.indexOf('(');
		int end = method.lastIndexOf(')');
		
		if (start != -1 && end != -1)
		{
			ArrayList<String> retVal = new ArrayList<String>();
			
			// tokenize by comma
			//
			String parms = method.substring(start+1,end);
			if (expected == 1)
			{
				// just take as is, even if it has commas in it
				//
				retVal.add(parms);
			}
			else
			{
				StringTokenizer st = new StringTokenizer(parms, ",");
				while (st.hasMoreTokens() == true)
				{
					String curr = st.nextToken().trim();
					retVal.add(curr);
				}
			}
			
			return retVal;
		}
		
		// no parms
		//
		return null;
	}
	
	//-----------------------------------------------------------------------------
	// Class definition:    InternalSource
	/**
	 *  Internal SubstitutionSource to deal with pre-defined variables known to 
	 *  the Substitution Engine (i.e. ${TODAY}, ${TEMP_DIR}
	 **/
	//-----------------------------------------------------------------------------
	private static class InternalSource implements SubstitutionSource
	{
		private static final String[] myKeys = {
			"TODAY",
			"YESTERDAY",
			"TOMORROW",
			"TEMP_DIR",
			"RANDOM",
		};

		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.SubstitutionSource#getKeys()
		 */
		//--------------------------------------------
		public Object[] getKeys()
		{
			return myKeys;
		}

		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.SubstitutionSource#getObject(java.lang.String)
		 */
		//--------------------------------------------
		public Object getObject(String key)
		{
			// Look for hard-coded known values
			//
			if (key.equalsIgnoreCase("TODAY") == true)
			{
				// return as long to make formating easier
				//
				long l = System.currentTimeMillis();
				return new Long(l);
			}
			else if (key.equalsIgnoreCase("YESTERDAY") == true)
			{
				// return as long to make formating easier
				//
				Calendar cal = Calendar.getInstance();
				cal.add(Calendar.DAY_OF_YEAR, -1);
				long l = cal.getTimeInMillis();
				return new Long(l);
			}
			else if (key.equalsIgnoreCase("TOMORROW") == true)
			{
				// return as long to make formating easier
				//
				Calendar cal = Calendar.getInstance();
				cal.add(Calendar.DAY_OF_YEAR, 1);
				long l = cal.getTimeInMillis();
				return new Long(l);
			}

			return null;
		}

		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.SubstitutionSource#getVariable(java.lang.String)
		 */
		//--------------------------------------------
		public String getVariable(String key)
		{
			if (key.equalsIgnoreCase("TEMP_DIR") == true)
			{
				return System.getProperty("java.io.tmpdir");
			}
			else if (key.equalsIgnoreCase("RANDOM") == true)
			{
				long rand = new Random().nextInt() & 0xffff;
				return Long.toString(rand);
			}
			
			return null;
		}

		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.SubstitutionSource#hasObjectVariable(java.lang.String)
		 */
		//--------------------------------------------
		public boolean hasObjectVariable(String key)
		{
			// Look for hard-coded known values
			//
			if (key.equalsIgnoreCase("TODAY") == true ||
				key.equalsIgnoreCase("YESTERDAY") == true ||
				key.equalsIgnoreCase("TOMORROW") == true)
			{
				return true;
			}

			return false;
		}

		//--------------------------------------------
		/**
		 * @see com.icontrol.substitution.SubstitutionSource#hasStringVariable(java.lang.String)
		 */
		//--------------------------------------------
		public boolean hasStringVariable(String key)
		{
			// Look for hard-coded known values
			//
			if (key.equalsIgnoreCase("TEMP_DIR") == true ||
				key.equalsIgnoreCase("RANDOM") == true)
			{
				return true;
			}

			return false;
		}
	}
}

//+++++++++++
// EOF
//+++++++++++

