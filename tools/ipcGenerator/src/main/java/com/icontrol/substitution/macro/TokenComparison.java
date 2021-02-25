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
// Class definition:    TokenComparison
/**
 *  Specialized Token that represents a comparison of 2 Tokens.
 *  Used as part of Expressions.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
class TokenComparison extends Token
{
	static enum Comparison {
		equals         ("=="),
		not_equals     ("!="),
		not            ("!"),
		greater        (">"),
		greater_equals (">="),
		less           ("<"),
		less_equals    ("<=");
		
		private String desc;
		
		//--------------------------------------------
		/**
		 * Constructor for this enum
		 */
		//--------------------------------------------
		Comparison(String str)
		{
			desc = str;
		}
		
		//--------------------------------------------
		/**
		 * Returns an enum value for the "string"
		 */
		//--------------------------------------------
		static Comparison forDesc(String str)
		{
			if (str == null)
			{
				return null;
			}
			
			// loop through the enum values
			//
			for (Comparison x : Comparison.values())
			{
				if (str.equals(x.desc) == true)
				{
					return x;
				}
			}
			return null;
		}
	};
	
	private Comparison	compare;
	
	//--------------------------------------------
	/**
	 * Constructor for this class
	 */
	//--------------------------------------------
	TokenComparison(Token.Type kind, String value)
	{
		super(kind, value);
		
		// create our comparison based on the 'value'
		//
		compare = Comparison.forDesc(value);
	}

	//--------------------------------------------
	/**
	 * Returns the comparison this token represents
	 */
	//--------------------------------------------
	public Comparison getComparison()
	{
		return compare;
	}

	//--------------------------------------------
	/**
	 * @see com.icontrol.substitution.macro.Token#isComparison()
	 */
	//--------------------------------------------
	@Override
	boolean isComparison()
	{
		return true;
	}

	//--------------------------------------------
	/**
	 * @see com.icontrol.substitution.macro.Token#isNegateComparison()
	 */
	//--------------------------------------------
	@Override
	boolean isNegateComparison()
	{
		return (compare == Comparison.not);
	}
}

//+++++++++++
//EOF
//+++++++++++