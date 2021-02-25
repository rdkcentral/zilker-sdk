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

package com.icontrol.api.properties;

import com.icontrol.event.properties.CpePropertyEvent;
import com.icontrol.ipc.impl.IPCException;
import com.icontrol.util.StringUtils;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

//-----------------------------------------------------------------------------
// Class definition:    PropsHelper
/**
 *  Helper class to simplify getting/settings CPE properties,
 *  and potentially converting them from String
 **/
//-----------------------------------------------------------------------------
public class PropsHelper
{
    private static Logger logger = LoggerFactory.getLogger(PropsHelper.class);

    //--------------------------------------------
    /**
     *  gets a CPE property, returns 'defValue' if not defined
     */
    //--------------------------------------------
    public static String getCpeProperty(String key, String defValue)
    {
        String value = defValue;

        try
        {
            Property prop = PropsServiceClient.getCpeProperty(key);
            if (prop != null)
            {
                value = prop.getValue();
            }
        }
        catch (IPCException ex)
        {
            logger.warn("cpeProps: error getting CPE property "+key, ex);
        }

        if (value == null)
        {
            value = defValue;
        }
        return value;
    }

    private static int getInternalInt(String value, int defValue)
    {
        if (StringUtils.isEmpty(value) == false)
        {
            try
            {
                return Integer.parseInt(value);
            }
            catch (NumberFormatException ex)
            {
                logger.warn("cpeProps: "+ex.getMessage(), ex);
            }
        }

        return defValue;
    }

    //--------------------------------------------
    /**
     *  returns defValue if not there or error converting to an int
     */
    //--------------------------------------------
    public static int getCpeIntProperty(String key, int defValue)
    {
        return getInternalInt(getCpeProperty(key, null), defValue);
    }

    public static int getCpeIntProperty(CpePropertyEvent event, int defValue)
    {
        if (event == null)
        {
            return defValue;
        }

        return getInternalInt(event.getPropValue(), defValue);
    }

    private static boolean getInternalBoolean(String value, boolean defValue)
    {
        if (StringUtils.isEmpty(value))
        {
            return defValue;
        }

        return ("true".equalsIgnoreCase(value) ||
                "yes".equalsIgnoreCase(value) ||
                "1".equals(value));
    }

    //--------------------------------------------
    /**
     *  returns defValue if not there or error converting to a boolean
     */
    //--------------------------------------------
    public static boolean getCpeBoolProperty(String key, boolean defValue)
    {
        return getInternalBoolean(getCpeProperty(key, null), defValue);
    }

    public static boolean getCpeBoolProperty(CpePropertyEvent event, boolean defValue)
    {
        if (event == null)
        {
            return defValue;
        }

        return getInternalBoolean(event.getPropValue(), defValue);
    }

    //--------------------------------------------
    /**
     *  sets a CPE property
     */
    //--------------------------------------------
    public static PropertySetResult setCpeProperty(String key, String value, PropSource source)
    {
        Property prop = new Property();
        prop.setKey(key);
        prop.setValue(value);
        prop.setSource(source);

        try
        {
            return PropsServiceClient.setCpeProperty(prop);
        }
        catch (IPCException ex)
        {
            logger.warn("cpeProps: error setting CPE property "+key, ex);
        }
        PropertySetResult retval = new PropertySetResult();
        retval.setResult(PropSetResult.PROPERTY_SET_IPC_ERROR);
        return retval;
    }

    public static PropertySetResult setCpeProperty(String key, boolean value, PropSource source)
    {
        return setCpeProperty(key, Boolean.toString(value), source);
    }

    public static PropertySetResult setCpeProperty(String key, int value, PropSource source)
    {
        return setCpeProperty(key, Integer.toString(value), source);
    }

    public static PropertySetResult setCpeProperty(String key, long value, PropSource source)
    {
        return setCpeProperty(key, Long.toString(value), source);
    }

    public static PropertySetResult setCpeProperty(String key, double value, PropSource source)
    {
        return setCpeProperty(key, Double.toString(value), source);
    }

    public static PropertySetResult setCpeProperty(String key, float value, PropSource source)
    {
        return setCpeProperty(key, Float.toString(value), source);
    }

    //--------------------------------------------
    /**
     *  removes a CPE property
     */
    //--------------------------------------------
    public static boolean delCpeProperty(String key)
    {
        try
        {
            PropsServiceClient.delCpeProperty(key);
            return true;
        }
        catch (IPCException ex)
        {
            logger.warn("cpeProps: error deleting CPE property "+key, ex);
        }

        return false;
    }
}
