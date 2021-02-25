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


//-----------------------------------------------------------------------------
// Class definition:    Token
/**
 *  Single entity within a Macro Expression.  This can be the operation
 *  itself (if, elseif), a variable marker, operand (==, >=, &&, etc), or
 *  a static value (such as a number or string).
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
class Token
{
	static enum Type {
		command,
		variable,
		value,
		comparison,
		group_start,
		group_end,
	}
	
	private Type	kind;
	private String	value;

	//--------------------------------------------
	/**
	 * Constructor for this class
	 */
	//--------------------------------------------
	Token(Type kind, String value)
	{
		this.kind = kind;
		this.value = value;
	}
	
	//--------------------------------------------
	/**
	 * @return Returns the type of token this is.
	 */
	//--------------------------------------------
	Type getKind()
	{
		return kind;
	}

	//--------------------------------------------
	/**
	 * @return Returns the value.
	 */
	//--------------------------------------------
	String getValue()
	{
		return value;
	}
	
	//--------------------------------------------
	/**
	 * return true if this is a 'comparison'
	 */
	//--------------------------------------------
	boolean isComparison()
	{
		return false;
	}
	
	//--------------------------------------------
	/**
	 * return true if this is a 'not-comparison'
	 */
	//--------------------------------------------
	boolean isNegateComparison()
	{
		return false;
	}
	
	//--------------------------------------------
	/**
	 * return true if this is a 'operator'
	 */
	//--------------------------------------------
	boolean isOperator()
	{
		return false;
	}
	
	//--------------------------------------------
	/**
	 * Used for debugging
	 * 
	 * @see java.lang.Object#toString()
	 */
	//--------------------------------------------
	@Override
	public String toString()
	{
		return value+"["+kind+"]";
	}
}

//+++++++++++
//EOF
//+++++++++++