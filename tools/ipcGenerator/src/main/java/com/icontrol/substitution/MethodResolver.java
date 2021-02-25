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

import java.util.ArrayList;

//-----------------------------------------------------------------------------
// Class definition:    MethodResolver
/**
 *  Interface to take a String (or Object) and run a method
 *  against it for the Substitution engine.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public interface MethodResolver
{
	//--------------------------------------------
	/**
	 *  Return if this resolver knows how to deal 
	 *  with the supplied method for the Class.
	 *  (ex: toUpperCase for String.class)
	 **/
	//--------------------------------------------
	public boolean supportsMethod(String method, Class<?> target);
	
	//--------------------------------------------
	/**
	 *  If supportsMethod() is true, this will be called
	 *  to see if (or how many) arguments are required.
	 *  <p>
	 *  Return of 0 means no arguments are expected or required
	 *  Return of > 0 means that many arguments are expected
	 *  Return of a negative number means that or that -1 is expected
	 *  (ex: -1 means 0 or 1 arg, -2 means 1 or 2 args)
	 **/
	//--------------------------------------------
	public int getArgCount(String method);
	
	//--------------------------------------------
	/**
	 *  Given a String value, invoke the method against
	 *  it (using the optional args) to return a new value.
	 *  (ex: toUpperCase for String.class)
	 *  <p>
	 *  Will only call if the supportsMethod() returned true.
	 *  
	 *  @param target The String to run the 'method' against.
	 *  @param method The method to execute
	 *  @param args Null if no args were provided
	 **/
	//--------------------------------------------
	public String executeMethod(String target, String method, ArrayList<String> args, Substitution src)
		throws IllegalArgumentException;

	//--------------------------------------------
	/**
	 *  Given an Object, invoke the method against
	 *  it (using the optional args) to return a new value.
	 *  (ex: toUpperCase for String.class)
	 *  <p>
	 *  Will only call if the supportsMethod() returned true.
	 *  
	 *  @param target The Object to run the 'method' against.
	 *  @param method The method to execute
	 *  @param args Null if no args were provided
	 **/
	//--------------------------------------------
	public Object executeMethod(Object target, String method, ArrayList<String> args, Substitution src)
		throws IllegalArgumentException;

}


//+++++++++++
//EOF
//+++++++++++