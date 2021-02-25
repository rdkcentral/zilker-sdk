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

//-----------------------------------------------------------------------------
// Interface definition:    SubstutionSource
/**
 *  Methods for owners of variable/property values to be
 *  located during Substitution.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public interface SubstitutionSource
{
	//--------------------------------------------
	/**
	 *  Return true if this Source contains a variable
	 *  identified by 'key' and is stored as a String
	 *  (meaning getVariable() would return a String).
	 **/
	//--------------------------------------------
	public boolean hasStringVariable(String key);

	//--------------------------------------------
	/**
	 *  Return true if this Source contains a variable
	 *  identified by 'key' and is stored as a non-String
	 *  (meaning getObject() would return something other
	 *  then a String).
	 **/
	//--------------------------------------------
	public boolean hasObjectVariable(String key);

	//--------------------------------------------
	/**
	 *  Return the value for the variable 'key'.
	 *  Only called if the hasStringVariable() returns true.
	 **/
	//--------------------------------------------
	public String getVariable(String key);
	
	//--------------------------------------------
	/**
	 *  Return the object for the variable 'key'.
	 *  Only called if the hasObjectVariable() returns true.
	 **/
	//--------------------------------------------
	public Object getObject(String key);
	
	//--------------------------------------------
	/**
	 * returns all of the keys that this source knows about
	 */
	//--------------------------------------------
	public Object[] getKeys();
}

//+++++++++++
//EOF
//+++++++++++