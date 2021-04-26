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

package com.icontrol.substitution.macro;

//-----------
//imports
//-----------

import java.util.ArrayList;

import com.icontrol.substitution.Substitution;

//-----------------------------------------------------------------------------
// Class definition:    Macro
/**
 *  Container of 1 or more MacroExpression objects and their corresponding
 *  'content' attachments.  Can be used as the single point of 'evaluation'
 *  to determine what (if any) content to utilize.
 *  <p>
 *  Macros have the format of:
 *  <pre>
 *    #if (${x})
 *       ..content of macro if pass...
 *    #elseif (${y.length} > 0)
 *       ..content of macro if pass...
 *    #else
 *       ..content of macro nothing else passed...
 *    #end 
 *  </pre>
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class Macro 
{
	private MacroExpression				main;			// #if
	private ArrayList<MacroExpression>	expressions;	// #elseif containers
	private MacroExpression				catchAll;		// #else
	private int							spanStart;		// where in 'buffer' this macro begins
	private int							spanEnd;		// where in 'buffer' this macro ends

	//--------------------------------------------
	/**
	 * Constructor for this class
	 */
	//--------------------------------------------
	public Macro()
	{
		expressions = new ArrayList<MacroExpression>();
		spanStart = -1;
		spanEnd = -1;
	}
	
	//--------------------------------------------
	/**
	 * Adds the 'expression' to the list.  May throw an
	 * exception if trying to add a second 'primary' command
	 * (such as 2 #else or #if macros)
	 */
	//--------------------------------------------
	public void addExpression(MacroExpression exp) throws InvalidMacroException
	{
		MacroType type = exp.getType();
		if (type == MacroType.IF)
		{
			// check for #if already set
			//
			if (main != null)
			{
				throw new InvalidMacroException(ExpressionFactory.IF_MARKER+" expression already defined.");
			}
			
			main = exp;
		}
		else if (type == MacroType.ELSE)
		{
			// check that #if is set
			//
			if (main == null)
			{
				throw new InvalidMacroException("Missing "+ExpressionFactory.IF_MARKER);
			}
			
			// check for #else already set
			//
			if (catchAll != null)
			{
				throw new InvalidMacroException(ExpressionFactory.ELSE_MARKER+" expression already defined.");
			}
			
			catchAll = exp;
		}
		else if (type == MacroType.ELSE_IF)
		{
			// check that #if is set
			//
			if (main == null)
			{
				throw new InvalidMacroException("Missing "+ExpressionFactory.IF_MARKER);
			}
			
			expressions.add(exp);
		}
	}

	//--------------------------------------------
	/**
	 * Executes each piece of the Macro until success is reached.
	 * Returns the 'attachment' associated with the Macro that passed.
	 * If nothing passes (and no #else defined), will return null.
	 */
	//--------------------------------------------
	public String evaluateMacro(Substitution sub)
	{
		// first check our main IF to see if it works
		//
		if (main.evaluate(sub) == true)
		{
			return main.getAttachment();
		}
		
		// see if we have else-if sections that work
		//
		for (int i = 0, count = expressions.size() ; i < count ; i++)
		{
			MacroExpression curr = expressions.get(i);
			if (curr.evaluate(sub) == true)
			{
				// passed
				//
				return curr.getAttachment();
			}
		}
		
		// nothing so far, so if we have an ELSE, return its contents
		//
		if (catchAll != null)
		{
			return catchAll.getAttachment();
		}
		
		return null;
	}
	
	//--------------------------------------------
	/**
	 * Returns the location from the original 'buffer'
	 * that this macro begins at.
	 */
	//--------------------------------------------
	public int getSpanStart()
	{
		return spanStart;
	}

	//--------------------------------------------
	/**
	 * @param spanStart The spanStart to set.
	 */
	//--------------------------------------------
	void setSpanStart(int loc)
	{
		this.spanStart = loc;
	}

	//--------------------------------------------
	/**
	 * Returns the location from the original 'buffer'
	 * that this macro ends at.
	 */
	//--------------------------------------------
	public int getSpanEnd()
	{
		return spanEnd;
	}

	//--------------------------------------------
	/**
	 * @param loc The spanEnd to set.
	 */
	//--------------------------------------------
	void setSpanEnd(int loc)
	{
		this.spanEnd = loc;
	}

	//--------------------------------------------
	/**
	 * Check that this is a complete entity
	 */
	//--------------------------------------------
	void validateMacro() throws InvalidMacroException
	{
		if (main == null)
		{
			throw new InvalidMacroException("Missing "+ExpressionFactory.IF_MARKER+" section");
		}
		
		if (spanStart < 0 || spanEnd < 0)
		{
			throw new InvalidMacroException("Missing "+ExpressionFactory.END_MARKER+" section");
		}
	}

	//--------------------------------------------
	/**
	 * @see java.lang.Object#toString()
	 */
	//--------------------------------------------
	@Override
	public String toString()
	{
		StringBuffer buff = new StringBuffer();
		
		// get macro for "if"
		//
		buff.append(main.toString());
		buff.append('\n');
		buff.append(main.getAttachment());
		buff.append('\n');
		
		// add each else-if 
		//
		for (int i = 0, count = expressions.size() ; i < count ; i++)
		{
			MacroExpression curr = expressions.get(i);
			buff.append(curr.toString());
			buff.append('\n');
			buff.append(curr.getAttachment());
			buff.append('\n');
		}

		// add else if defined
		//
		if (catchAll != null)
		{
			buff.append(catchAll.toString());
			buff.append('\n');
			buff.append(catchAll.getAttachment());
			buff.append('\n');
		}
		
		// add the #end
		//
		buff.append(ExpressionFactory.END_MARKER);
		buff.append('\n');

		return buff.toString();
	}
}

//+++++++++++
//EOF
//+++++++++++