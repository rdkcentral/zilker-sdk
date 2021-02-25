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
// imports
//-----------

import java.io.IOException;
import java.io.InputStream;
import java.io.Serializable;
import java.net.URL;
import java.text.DateFormat;
import java.util.Calendar;
import java.util.Date;
import java.util.Properties;
import java.util.StringTokenizer;

import org.slf4j.LoggerFactory;

//-----------------------------------------------------------------------------
// Class definition:    Version
/**
 *  Reads a  .version file that was generated at build time
 *  from within a jar file.  Used to determine compatibility
 *  between builds and other iControl Applications.
 **/
//-----------------------------------------------------------------------------
public final class Version extends Object
	implements Serializable, Comparable<Version>
{
	private static final long serialVersionUID = -3795150016873398642L;
	
	protected int  version;
	protected int  revision;
	protected int  number;
	protected Date date;

	//-------------------------------------------------------------------------
	//-                                                                       -
	//--------------- M E T H O D   I M P L E M E N T A T I O N S -------------
	//-                                                                       -
	//-------------------------------------------------------------------------


	//-******************************************-
	//-*                                        *-
	//-*             Constructors               *-
	//-*                                        *-
	//-******************************************-

	//--------------------------------------------
	/**
	 *  Constructor for this object.  Requires
	 *  the complete path to the file.  It's assumed
	 *  the file will be found in a jar in the Classpath.
	 **/
	//--------------------------------------------
	public Version(String verFileLocation)
	{
	    	InputStream stream = null;
		try
		{
			Properties prop = new Properties();
			URL url = getClass().getResource(verFileLocation);
			if (url != null)
			{
				stream = url.openStream();
				prop.load(stream);
				stream.close();
			}

			// read properties and assign version info
			//
			version = readInt("version", 0, prop);
			revision = readInt("revision", 0, prop);
			number = readInt("number", 0, prop);
			date = readDate("date", prop);
		}
		catch (Exception ex)
		{
	        LoggerFactory.getLogger(Version.class).warn(ex.getMessage(), ex);
			if (stream != null)
			{
			    try
			    {
			        stream.close();
			    }
			    catch (IOException e) { }
			}
		}
	}
	
	/**
	 * Constructor taking the URL of the version file
	 * @param verFileLocation
	 */
	public Version(URL verFileLocation)
	{
	    InputStream stream = null;
	    try
	    {
		Properties prop = new Properties();
		if (verFileLocation != null)
		{
		    stream = verFileLocation.openStream();
		    prop.load(stream);
		    stream.close();
		}
		// read properties and assign version info
		//
		version = readInt("version", 0, prop);
		revision = readInt("revision", 0, prop);
		number = readInt("number", 0, prop);
		date = readDate("date", prop);
	    }
	    catch (Exception ex)
	    {
	        LoggerFactory.getLogger(Version.class).warn(ex.getMessage(), ex);
	        if (stream != null)
	        {
	            try
	            {
	                stream.close();
	            }
	            catch (IOException e) { }
	        }
	    }
	}
	
	//--------------------------------------------
	/**
	 *  Programatic Constructor for this object.
	 **/
	//--------------------------------------------
	public Version(int version, int revision, int number)
	{
		this.version = version;
		this.revision = revision;
		this.number = number;
	}

	//-******************************************-
	//-*                                        *-
	//-*             Public Methods             *-
	//-*                                        *-
	//-******************************************-

	public int getBuildVersion()  { return version; }
	public int getBuildRevision() { return revision; }
	public int getBuildNumber()   { return number; }
	public Date getBuildDate()    { return date; }
	
	//--------------------------------------------
	/**
	 * Ex: 1.3.4
	 **/
	//--------------------------------------------
	public String getFloatString()
	{
		return version+"."+revision+"."+number;
	}

	//--------------------------------------------
	/**
	 *
	 **/
	//--------------------------------------------
	public String getVersionString()
	{
		return version+"."+revision+" (build "+number+")";
	}

	//--------------------------------------------
	/**
	 *
	 **/
	//--------------------------------------------
	public String getBuildDateString()
	{
		if (date == null)
		{
			return "";
		}

		// format as MM/DD/YYY HH:MM
		//
		DateFormat format = DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT);
		return format.format(date);
	}

	//--------------------------------------------
	/**
	 * Standard compare.  Returns positive if 'this' is larger
	 * then 'other', -1 if less, 0 if equal.
	 **/
	//--------------------------------------------
	public int compareTo(Version other)
	{
		// first compare version
		//
		if (version > other.version)
		{
			return 1;
		}
		else if (version < other.version)
		{
			return -1;
		}
		else
		{
			// compare the revision
			//
			if (revision > other.revision)
			{
				return 1;
			}
			else if (revision < other.revision)
			{
				return -1;
			}
			else
			{
				// compare the build number
				//
				if (number > other.number)
				{
					return 1;
				}
				else if (number < other.number)
				{
					return -1;
				}
				else
				{
					/**
                        // compare the build date (if defined)
                        //
                        if (date != null && other.date != null)
                        {
                            return date.compareTo(other.date);
                        }
					 **/

					return 0;
				}
			}
		}
	}

	//--------------------------------------------
	/**
	 *  Shows the version information as a readable string
	 **/
	//--------------------------------------------
	public String toString()
	{
		String retVal = "Version: "+version+"."+revision+" (build "+number;
		if (date != null)
		{
			retVal += " "+getBuildDateString();
		}

		retVal += ")";

		return retVal;
	}

	//-******************************************-
	//-*                                        *-
	//-*            Private Methods             *-
	//-*                                        *-
	//-******************************************-

	//--------------------------------------------
	/**
	 *
	 **/
	//--------------------------------------------
	private int readInt(String key, int defValue, Properties prop)
	{
		String tmp = prop.getProperty(key, Integer.toString(defValue));
		try
		{
			return Integer.parseInt(tmp);
		}
		catch (NumberFormatException e)
		{
			return defValue;
		}
	}

	//--------------------------------------------
	/**
	 *
	 **/
	//--------------------------------------------
	private Date readDate(String key, Properties prop)
	{
		String tmp = prop.getProperty(key, null);
		if (tmp == null)
		{
			return null;
		}

		try
		{
			// should be in the format of 2004/03/08 11:32
			//
			StringTokenizer st = new StringTokenizer(tmp, "/\\ :");
			if (st.countTokens() >= 5)
			{
				String year = st.nextToken();
				String month = st.nextToken();
				String day = st.nextToken();
				String hour = st.nextToken();
				String min = st.nextToken();

				Calendar cal = Calendar.getInstance();
				cal.set(Calendar.YEAR, Integer.parseInt(year));
				cal.set(Calendar.MONTH, Integer.parseInt(month) - 1);
				cal.set(Calendar.DAY_OF_MONTH, Integer.parseInt(day));
				cal.set(Calendar.HOUR_OF_DAY, Integer.parseInt(hour));
				cal.set(Calendar.MINUTE, Integer.parseInt(min));
				cal.set(Calendar.SECOND, 0);
				cal.set(Calendar.MILLISECOND, 0);

				return cal.getTime();
			}
			else
			{
				// try to convert to a Date
				//
				return DateFormat.getDateTimeInstance().parse(tmp);
			}
		}
		catch (Exception e)
		{
			e.printStackTrace();
			return null;
		}
	}
}

//+++++++++++
// EOF
//+++++++++++

