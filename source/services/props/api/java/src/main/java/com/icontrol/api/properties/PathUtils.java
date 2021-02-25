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

//-----------
//imports
//-----------

import com.icontrol.ipc.impl.IPCException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.concurrent.atomic.AtomicReference;

//-----------------------------------------------------------------------------
// Class definition:    PathUtils
/**
 *  Static helper routines for dealing with filesystem paths
 *  
 *  @author tlea
 **/
//-----------------------------------------------------------------------------
public class PathUtils
{
    private static final Logger logger = LoggerFactory.getLogger(PathUtils.class);

    //Common system property name that can override/set the location for cache files.
    public static final String IC_CACHE_DIR_PROP = "IC_CACHE_DIR";

    //Common system property name that can override/set the location for bin files.
    public static final String IC_BIN_DIR_PROP = "IC_BIN_DIR";

    //Common system property name that can override/set the location for sbin files.
    public static final String IC_SBIN_DIR_PROP = "IC_SBIN_DIR";

    //Common system property name that can override/set the location for storage directory.
    public static final String IC_STORAGE_DIR_PROP = "IC_STORAGE_DIR";
    
    //Common system property name that can override/set the location for tmp directory.
    public static final String IC_TEMP_DIR_PROP = "IC_TEMP_DIR";

    //Common system property name that can override/set the location for storage directory
    public  static final String IC_SYSTEM_BIN_PROP         = "IC_SYSTEM_BIN_DIR";
    public  static final String IC_SYSTEM_ETC_PROP         = "IC_SYSTEM_ETC_DIR";

    private static final String DEFAULT_DYNAMIC_PATH       = "/opt";
    private static final String DEFAULT_STATIC_PATH        = "/vendor";
    private static final String DEFAULT_CACHE_PATH         = "/cache";
    private static final String DEFAULT_BIN_PATH           = "/vendor/bin";
    private static final String DEFAULT_SBIN_PATH          = "/vendor/sbin";
    private static final String DEFAULT_STORAGE_PATH       = "/storage";
    private static final String DEFAULT_STORAGE_PATH_DROID = "/data";
    private static final String DEFAULT_SYSTEM_BIN_PATH    = "/system/bin";
    private static final String DEFAULT_SYSTEM_ETC_PATH    = "/system/etc";
    private static final String DEFAULT_TEMP_PATH          = "/tmp";
    private static final String DEFAULT_FORCED_TROUBLE_PATH = "/data/forcedTrouble";

    private static AtomicReference<String> cachedDynamicConfigPath = new AtomicReference<String>();
    private static AtomicReference<String> cachedStaticConfigPath = new AtomicReference<String>();
    private static AtomicReference<String> cachedDynamicPath = new AtomicReference<String>();
    private static AtomicReference<String> cachedStaticPath = new AtomicReference<String>();

    private static String setPathIfUnset(AtomicReference<String> pathVar, PropPathType pathType)
    {
        if (pathVar.get() == null)
        {
            PropertyPath propPath = new PropertyPath();
            propPath.setPathType(pathType);
            try
            {
                pathVar.set(PropsServiceClient.getPath(propPath));
            }
            catch (IPCException e)
            {
                logger.error("Failed to get path type " + pathType, e);
            }
            catch (Exception e)
            {
                logger.error("Failed to get path type " + pathType, e);
            }
        }
        return pathVar.get();
    }

    //--------------------------------------------
    /**
     * Return the path to where the dynamic config files are stored.
     * This can be overridden by setting a system property.
     * 
     * Typically this returns /opt/etc
     */
    //--------------------------------------------
    public static String getDynamicConfigPath()
    {
        String path = setPathIfUnset(cachedDynamicConfigPath, PropPathType.DYNAMIC_CONFIG_PATH);
        return path == null ? DEFAULT_DYNAMIC_PATH + "/etc" : path;
    }
    
    //--------------------------------------------
    /**
     * Return the path to where the static config files are stored.
     * This can be overridden by setting a system property.
     * 
     * Typically this returns /vendor/etc
     */
    //--------------------------------------------
    public static String getStaticConfigPath()
    {
        String path = setPathIfUnset(cachedStaticConfigPath, PropPathType.STATIC_CONFIG_PATH);
        return path == null ? DEFAULT_STATIC_PATH + "/etc" : path;
    }

    /**
     * Get the path to the brand defaults - the current firmware brand's defaults (e.g., ssh banner, default properties, etc)
     * @return the path to brand defaults
     */
    public static String getBrandDefaultsPath()
    {
        return getStaticConfigPath() + "/defaults";
    }
    
    public static String getSystemBinPath()
    {
        return System.getProperty(IC_SYSTEM_BIN_PROP,DEFAULT_SYSTEM_BIN_PATH);
    }

    public static String getSystemEtcPath()
    {
        return System.getProperty(IC_SYSTEM_ETC_PROP,DEFAULT_SYSTEM_ETC_PATH);
    }

    //--------------------------------------------
    /**
     * Return the path to tmp storage
     *
     * Typically this returns /tmp
     */
    //--------------------------------------------
    public static String getTempPath()
    {
        return System.getProperty(IC_TEMP_DIR_PROP, DEFAULT_TEMP_PATH);
    }

    //--------------------------------------------
    /**
     * Return the path to where the dynamic cache files are stored.
     *
     * Typically this returns /cache
     */
    //--------------------------------------------
    public static String getDynamicCachePath()
    {
        return System.getProperty(IC_CACHE_DIR_PROP, DEFAULT_CACHE_PATH);
    }

    //--------------------------------------------
    /**
     * Return the path to where the bin files are stored.
     *
     * Typically this returns /vendor/bin
     */
    //--------------------------------------------
    public static String getDynamicBinPath()
    {
        return System.getProperty(IC_BIN_DIR_PROP, DEFAULT_BIN_PATH);
    }

    //--------------------------------------------
    /**
     * Return the path to where the sbin files are stored.
     *
     * Typically this returns /vendor/sbin
     */
    //--------------------------------------------
    public static String getDynamicSBinPath()
    {
        return System.getProperty(IC_SBIN_DIR_PROP, DEFAULT_SBIN_PATH);
    }

    //--------------------------------------------
    /**
     * Return the path of storage directory.
     *
     * Typically this returns /storage
     */
    //--------------------------------------------
    public static String getDynamicStoragePath()
    {
        // This is really only Android at this point, so we can skip this check(and dependency)
        //CapabilitiesSettings cap = CapabilitiesFactory.getCapabilitiesInstance();
        //if(cap != null && cap.isAndroid() == true)
        //{
            return System.getProperty(IC_STORAGE_DIR_PROP, DEFAULT_STORAGE_PATH_DROID);
        //}
        //else
        //{
        //    return System.getProperty(IC_STORAGE_DIR_PROP, DEFAULT_STORAGE_PATH);
        //}
    }

    //--------------------------------------------
    /**
     * Return the path to where the dynamic files are stored.
     * This can be overridden by setting a system property.
     * 
     * Typically this returns /opt
     */
    //--------------------------------------------
    public static String getDynamicPath()
    {
        String path = setPathIfUnset(cachedDynamicPath, PropPathType.DYNAMIC_PATH);
        return path == null ? DEFAULT_DYNAMIC_PATH : path;
    }
    
    //--------------------------------------------
    /**
     * Return the path to where the static files are stored.
     * This can be overridden by setting a system property.
     * 
     * Typically this returns /vendor
     */
    //--------------------------------------------
    public static String getStaticPath()
    {
        String path = setPathIfUnset(cachedStaticPath, PropPathType.STATIC_PATH);
        return path == null ? DEFAULT_STATIC_PATH : path;
    }
    
    /**
     * gets the forced trouble directory name. This will only exist on droid, so the caller
     * should deal with that fact appropriately
     * @return the path to the forced trouble directory
     */
    public static String getForcedTroubleDirname()
    {
        return DEFAULT_FORCED_TROUBLE_PATH;
    }

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    private PathUtils()
    {
    }
}

//+++++++++++
//EOF
//+++++++++++
