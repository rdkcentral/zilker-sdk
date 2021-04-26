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
// Class definition:    MacroType
/**
 *  The supported Macro types
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public enum MacroType {
	IF,
	ELSE,
	ELSE_IF,
	END;
	
	//--------------------------------------------
	/**
	 * Return the MacroType for the command string
	 */
	//--------------------------------------------
	public static MacroType getForCommand(String cmd)
	{
		if (cmd == null)
		{
			return null;
		}
		else if (cmd.equalsIgnoreCase("if") == true)
		{
			return IF;
		}
		else if (cmd.equalsIgnoreCase("else") == true)
		{
			return ELSE;
		}
		else if (cmd.equalsIgnoreCase("elseif") == true)
		{
			return ELSE_IF;
		}
		else if (cmd.equalsIgnoreCase("end") == true)
		{
			return END;
		}
		return null;
	}
}


//+++++++++++
//EOF
//+++++++++++