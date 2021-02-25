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
// Class definition:    TokenOperator
/**
 *  Specialized Token that represents an operator of 2 Expressions (AND vs OR).
 *  Used as part of Expressions.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
class TokenOperator extends Token
{
	static enum Operator {
		and  ("&&"),
		or   ("||");
		
		private String desc;
		
		//--------------------------------------------
		/**
		 * Constructor for this enum
		 */
		//--------------------------------------------
		Operator(String str)
		{
			desc = str;
		}
		
		//--------------------------------------------
		/**
		 * Returns an enum value for the "string"
		 */
		//--------------------------------------------
		static Operator forDesc(String str)
		{
			if (str == null)
			{
				return null;
			}
			
			// loop through the enum values
			//
			for (Operator x : Operator.values())
			{
				if (str.equals(x.desc) == true)
				{
					return x;
				}
			}
			return null;
		}
	};
	
	private Operator operator;
	
	//--------------------------------------------
	/**
	 * Constructor for this class
	 */
	//--------------------------------------------
	TokenOperator(Token.Type kind, String value)
	{
		super(kind, value);
		
		// create our operator based on the 'value'
		//
		operator = Operator.forDesc(value);
	}

	//--------------------------------------------
	/**
	 * Returns the Operator this token represents
	 */
	//--------------------------------------------
	public Operator getOperator()
	{
		return operator;
	}

	//--------------------------------------------
	/**
	 * @see com.icontrol.substitution.macro.Token#isOperator()
	 */
	//--------------------------------------------
	@Override
	boolean isOperator()
	{
		return true;
	}
}


//+++++++++++
//EOF
//+++++++++++