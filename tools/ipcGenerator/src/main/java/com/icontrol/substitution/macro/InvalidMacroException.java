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
// Class definition:    InvalidMacroException
/**
 *  Exception thrown with parsing an invalid Macro
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class InvalidMacroException extends Exception
{
	private static final long serialVersionUID = 1L;
	private String macroLine;

	//--------------------------------------------
	/**
	 * Constructor for this Exception
	 */
	//--------------------------------------------
	public InvalidMacroException()
	{
	}

	//--------------------------------------------
	/**
	 * Constructor for this Exception
	 */
	//--------------------------------------------
	public InvalidMacroException(String message)
	{
		super(message);
	}

	//--------------------------------------------
	/**
	 * Constructor for this Exception
	 */
	//--------------------------------------------
	public InvalidMacroException(String message, Throwable cause)
	{
		super(message, cause);
	}

	//--------------------------------------------
	/**
	 * @see java.lang.Throwable#getMessage()
	 */
	//--------------------------------------------
	@Override
	public String getMessage()
	{
		// get the exception message
		//
		String msg = super.getMessage();
		
		// if we have the macro that errored, append that to the message
		//
		if (macroLine != null)
		{
			return msg + " macro='"+macroLine+"'";
		}
		return msg;
	}

	//--------------------------------------------
	/**
	 * @see java.lang.Throwable#getLocalizedMessage()
	 */
	//--------------------------------------------
	@Override
	public String getLocalizedMessage()
	{
		// get the exception message
		//
		String msg = super.getLocalizedMessage();
		
		// if we have the macro that errored, append that to the message
		//
		if (macroLine != null)
		{
			return msg + " macro='"+macroLine+"'";
		}
		return msg;
	}

	//--------------------------------------------
	/**
	 * @return Returns the macroLine.
	 */
	//--------------------------------------------
	public String getMacroLine()
	{
		return macroLine;
	}

	//--------------------------------------------
	/**
	 * @param macro The macroLine to set.
	 */
	//--------------------------------------------
	public void setMacroLine(String macro)
	{
		this.macroLine = macro;
	}
}


//+++++++++++
//EOF
//+++++++++++