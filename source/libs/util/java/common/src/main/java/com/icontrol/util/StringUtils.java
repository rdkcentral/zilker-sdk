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
//imports
//-----------

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.Charset;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;

//-----------------------------------------------------------------------------
// Class definition:    StringUtils
/**
 *  Set of String utilities
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class StringUtils
{
    //For Base64 encoding
    private static final String base64code = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            + "abcdefghijklmnopqrstuvwxyz" + "0123456789" + "+/";
    private static final int splitLinesAt = 76;

    // for secure randomization
    private static final String VALID_CHARS  = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    private static final int    VALID_BOUNDS = VALID_CHARS.length();
    private static final int    SAMPLE_SIZE  = 64;

    
    //--------------------------------------------
    /**
     *  Returns a meaningful string from the Exception
     *  If the message is NULL, then the exception class
     *  will be returned.
     **/
	//--------------------------------------------
    public static String getExceptionMessage(Throwable e)
    {
    	String msg = e.getMessage();
    	if (msg == null)
    	{
    		msg = e.getClass().getName();
    	}

    	return msg;
    }

	//--------------------------------------------
    /**
     *  Converts an exception into a String with the
     *  complete Stack dump of the methods called
     *  up-to the catch
     **/
	//--------------------------------------------
    public static String exceptionTrace(Throwable e)
    {
        if(e == null) return "StringUtil.exceptionTrace(null);";

        // get the message of the exception (may be null)
        //
        String eMessage = getExceptionMessage(e);

        // now do the stack dump
        //
        try
        {
            StringWriter sw = new StringWriter();
            PrintWriter pw = new PrintWriter(sw);

            e.printStackTrace(pw);
            pw.close();
            sw.close();
            eMessage += "\n" + sw.toString();
        }
        catch (Exception ex)
        {
            eMessage = e.getMessage();
        }

        return eMessage;
    }

    public static int length(String str)
    {
        return (str == null) ? 0 : str.length();
    }

    //--------------------------------------------
    /**
     * Given a string with underscores (or spaces)
     * it will return a Camel Case version.
     */
    //--------------------------------------------
    public static String camelCase(String str)
    {
        StringBuffer retVal = new StringBuffer();
        
        // replace _ with space
        //
        str = str.replace('_', ' ');
        char[] c = str.toCharArray();
        
        // first char is capitol
        //
        boolean lastWasSpace = true;
        for (int i = 0 ; i < c.length ; i++)
        {
            if (lastWasSpace == true)
            {
                retVal.append(Character.toUpperCase(c[i]));
                lastWasSpace = false;
                continue;
            }
            
            if (c[i] == ' ')
            {
                lastWasSpace = true;
            }
            
            retVal.append(c[i]);
        }

        return retVal.toString();
    }

    public static String javaIdentifier(String str)
    {
        StringBuffer retVal = new StringBuffer();

        // replace _ with space
        //
        str = str.toLowerCase().replace('_', ' ');
        char[] c = str.toCharArray();

        // first char is not capitol
        //
        boolean lastWasSpace = false;
        for (int i = 0 ; i < c.length ; i++)
        {
            if (lastWasSpace == true)
            {
                retVal.append(Character.toUpperCase(c[i]));
                lastWasSpace = false;
                continue;
            }

            if (c[i] == ' ')
            {
                lastWasSpace = true;
            }
            else
            {
                retVal.append(c[i]);
            }
        }

        return retVal.toString();
    }

    //--------------------------------------------
    /**
     *  Takes the provided "buffer" and adds ' ' to the end
     *  until the size of the string matches "totalSize"
     **/
    //--------------------------------------------
    public static void padStringBuffer(StringBuffer buffer, int totalSize, char padChar)
    {
        while (buffer.length() < totalSize)
        {
            buffer.append(padChar);
        }
    }
	
	//--------------------------------------------
    /**
     *  Replaces spaces, commas, and other crap characters
     *  into underscores so the string can be used as a 
     *  valid file name or HTML identifier.
     **/
	//--------------------------------------------
    public static String makeValidIdentifier(String str)
    {
    	if (str == null || str.length() == 0)
    	{
    		return str;
    	}
    	
    	// make all spaces and commas into _
    	//
    	String tmp = str.replace(' ', '_');
    	tmp = tmp.replace('-', '_');
    	tmp = tmp.replace('/', '_');
    	tmp = tmp.replace('\\', '_');
    	return tmp.replace(',', '_');
    }
    
	//--------------------------------------------
    /**
     *  Substitute strings withing a buffer
     **/
    //--------------------------------------------
    public static void substitute(StringBuffer src, String search, String replace)
    {
        if (replace == null || search == null)
        {
        	return;
        }

        String s = src.toString();
        int off = s.indexOf(search);

        while(off >= 0)
        {
            src.replace(off, off+search.length(), replace);
            s = src.toString();
            off = s.indexOf(search);
        }
    }
    
	//--------------------------------------------
    /**
     *  Escape quotation marks (ie " --> \")
     **/
    //--------------------------------------------
    public static String escapeQuotes(String str)
    {
    	int offset = str.indexOf('"');
    	if (offset == -1)
    	{
    		return str;
    	}
    	
    	int start = 0;
    	StringBuffer buff = new StringBuffer();
    	while (offset != -1)
    	{
    		buff.append(str.substring(0, offset));
    		buff.append('\\');
    		buff.append('"');
    		start = offset+1;
    		
    		offset = str.indexOf('"', start);
    	}
    	
    	buff.append(str.substring(start, str.length()));
    	return buff.toString();
    }
    
	//--------------------------------------------
    /**
     *  Escape single-quotes marks (ie ' --> \')
     **/
    //--------------------------------------------
    public static String escapeTicks(String str)
    {
    	int offset = str.indexOf("'");
    	if (offset == -1)
    	{
    		return str;
    	}
    	
    	int start = 0;
    	StringBuffer buff = new StringBuffer();
    	while (offset != -1)
    	{
    		buff.append(str.substring(start, offset));
    		buff.append('\\');
    		buff.append("'");
    		start = offset+1;
    		
    		offset = str.indexOf("'", start);
    	}
    	
    	buff.append(str.substring(start, str.length()));
    	return buff.toString();
    }
    
	//--------------------------------------------
	/**
	 * Removes quotes and ticks at the beginning and end of a string
	 */
	//--------------------------------------------
	public static String trimQuotes(String str)
	{
		if (str == null || str.length() == 0)
		{
			return str;
		}
		
		// handle double quotes first since some macros are in the style
		// of "'xxx'"
		//
		if (str.startsWith("\"") == true && str.endsWith("\"") == true)
		{
			str = str.substring(1, str.length() - 1);
		}
		
		// now look for ticks
		//
		if (str.startsWith("'") == true && str.endsWith("'") == true)
		{
			str = str.substring(1, str.length() - 1);
		}
		return str;
	}

    //--------------------------------------------
    /**
     * Returns if the string is null or empty
     */
    //--------------------------------------------
    public static boolean isEmpty(String str)
    {
        return (str == null || str.trim().length() == 0);
    }
    
    //--------------------------------------------
    /**
     * Returns "" if the string is null, otherwise returns the original str
     */
    //--------------------------------------------
    public static String emptyIfNull(String str)
    {
        if (str == null)
        {
            return "";
        }
        return str;
    }
    
	//--------------------------------------------
	/**
	 * Returns if the strings compare (or are both null)
	 */
	//--------------------------------------------
	public static boolean compare(String a, String b)
	{
		boolean aEmpty = isEmpty(a);
		boolean bEmpty = isEmpty(b);
		
		if (aEmpty == false && bEmpty == false)
		{
			// string equals
			//
			return a.equals(b);
		}
		else if (aEmpty == true && bEmpty == true)
		{
			// both null
			//
			return true;
		}
		else 
		{
			// one null and one is not
			//
			return false;
		}
	}
	
	//--------------------------------------------
	/**
	 * Returns if the strings compare (or are both null)
	 */
	//--------------------------------------------
	public static boolean compare(String a, String b, boolean ignoreCase)
	{
		boolean aEmpty = isEmpty(a);
		boolean bEmpty = isEmpty(b);
		
		if (aEmpty == false && bEmpty == false)
		{
			// string equals
			//
			if (ignoreCase == false)
			{
				return a.equals(b);
			}
			else
			{
				return a.equalsIgnoreCase(b);
			}
		}
		else if (aEmpty == true && bEmpty == true)
		{
			// both null
			//
			return true;
		}
		else 
		{
			// one null and one is not
			//
			return false;
		}
	}
	
    //--------------------------------------------
    /**
     * Returns the compareTo of the strings 
     */
    //--------------------------------------------
    public static int compareTo(String a, String b)
    {
        boolean aEmpty = isEmpty(a);
        boolean bEmpty = isEmpty(b);
        
        if (aEmpty == false && bEmpty == false)
        {
            // string equals
            //
            return a.compareTo(b);
        }
        else if (aEmpty == true && bEmpty == true)
        {
            // both null
            //
            return 0;
        }
        else if (aEmpty == false)
        {
            return -1;
        }
        else
        {
            // b not null
            return 1;
        }
    }
	
	//--------------------------------------------
	/**
	 * Generate a random string of alphanumeric characters.  
	 * Can be used as a username or password.
	 */
	//--------------------------------------------
	public static String generateRandomString(int length)
	{
	    try
        {
            // use the more secure mechanism (if possible)
            //
            return generateSecureRandomAlphaNumString(length);
        }
        catch (NoSuchAlgorithmException oops)
        {
            // ignore the error and fall below
            //
        }

        // unable to get a MessageDigest, so go with the old approach
        //
        SecureRandom rand = new SecureRandom();
	    StringBuffer buff = new StringBuffer();
	    while (buff.length() < length)
	    {
	        // get a random character that is 0-9 or A-Z
	        //
	        int num = rand.nextInt('z' + 1);
	        if (num >= '0' && num <= '9')
	        {
	            buff.append((char)num);
	        }
	        else if (num >= 'A' && num <= 'Z')
	        {
	            buff.append((char)num);
	        }
	        else if (num >= 'a' && num <= 'z')
	        {
	            buff.append((char)num);
	        }
	    }
	    
	    return buff.toString();
	}

	//--------------------------------------------
    /**
     * Generate a random string of numeric characters.  
     * Can be used as a username or password.
     */
    //--------------------------------------------
    public static String generateRandomNumberString(int length)
    {
        SecureRandom rand = new SecureRandom();
        StringBuffer buff = new StringBuffer();
        while (buff.length() < length)
        {
            // get a random character that is 0-9
            //
            int num = rand.nextInt('9');
            if (num >= '0' && num <= '9')
            {
                buff.append((char)num);
            }
        }
        
        return buff.toString();
    }


    //--------------------------------------------
    /**
     * generate a random string of alpha-numeric characters (mixed case)
     * that could be used for usernames, passwords, etc.
     * uses a MessageDigest to further ensure randomness.
     */
    //--------------------------------------------
    public static String generateSecureRandomAlphaNumString(int length)
            throws NoSuchAlgorithmException
    {
        // using the basic algorithm Gregg Weissman provided
        //
        SecureRandom rand = new SecureRandom();
        MessageDigest digester = MessageDigest.getInstance("SHA-256");

        // get a chunk of random bytes
        //
        byte[] sample = new byte[SAMPLE_SIZE];
        rand.nextBytes(sample);

        // create our buffer to append the characters into
        // and loop until it's length matches the requested amount
        //
        StringBuffer retVal = new StringBuffer();
        while (retVal.length() < length)
        {
            // to make this even more obscure, digest the chunk of random bytes.
            // note that this should produce far less bytes then what is in
            // the 'sample' array.
            //
            byte[] hashed = digester.digest(sample);

            // convert hashed bytes to alpha-numeric
            //
            for (int i = 0 ; i < hashed.length ; i++)
            {
                if (retVal.length() < length)
                {
                    // append to the buffer
                    //
                    char ch = convertToAlphaNumeric(hashed[i]);
                    retVal.append(ch);
                }
                else
                {
                    // got enough, so break from the loop
                    //
                    break;
                }
            }

            // see if we need to get another set of random bytes to digest
            //
            if (retVal.length() < length)
            {
                rand.nextBytes(sample);
            }
        }

        return retVal.toString();
    }

    //--------------------------------------------
    /**
     * ensure the byte is within 0-9, A-Z, a-z
     */
    //--------------------------------------------
    private static char convertToAlphaNumeric(byte b)
    {
        // if the byte (as a char) is already in the alpha-numeric range, just use it
        //
        if (Character.isLetterOrDigit(b) == true)
        {
            return (char)b;
        }

        // outside the range, so mod against the length of our VALID_CHARS string,
        // then use as an offset into that array
        //
        int offset = Math.abs(b % VALID_BOUNDS);
        return VALID_CHARS.charAt(offset);
    }

    //--------------------------------------------
    /**
     * Left pad a string to a certain length
     * @param string the string to pad
     * @param length the length to pad the string to
     * @param ch the pad character
     * @return the padded string or if the passed string's length is
     * greater than or equal to the passed length then the original string
     * is returned unchanged.
     * 
     */
    //--------------------------------------------
    public static String lpad(String string, int length, char ch)
    {
        String result = string;
        int diff = length - string.length();
        if (diff > 0)
        {
            StringBuilder sb = new StringBuilder(length);
            for(int i = 0; i < diff; ++i)
            {
                sb.append(ch);
            }
            sb.append(string);
            result = sb.toString();
        }
       
        return result;
    }
    
    /*
     * Helper for base 64 encoding
     */
    private static byte[] zeroPad(int length, byte[] bytes) 
    {
        byte[] padded = new byte[length]; // initialized to zero by JVM
        System.arraycopy(bytes, 0, padded, 0, bytes.length);
        return padded;
    }
 
    /*
     * Helper for base 64 encoding
     */
    private static String splitLines(String string) 
    {
        StringBuffer sb = new StringBuffer();
        for (int i = 0; i < string.length(); i += splitLinesAt)
        {
            sb.append(string.substring(i, Math.min(string.length(), i + splitLinesAt)));
            sb.append("\r\n");
        }
        return sb.toString();
    }

    //--------------------------------------------
	/**
	 * Constructor for this class
	 */
	//--------------------------------------------
	private StringUtils()
	{
	}
}

//+++++++++++
//EOF
//+++++++++++
