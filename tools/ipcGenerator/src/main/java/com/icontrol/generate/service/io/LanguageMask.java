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

package com.icontrol.generate.service.io;

//-----------------------------------------------------------------------------
/**
 * Simplistic bitmask to define which languages to generate
 * Could be used globally or for a particular item
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class LanguageMask
{
    private static final int JAVA_MASK   = 0x1;
    private static final int NATIVE_MASK = 0x2;

    /* preset mask for NATIVE only */
    public static final LanguageMask NATIVE = new LanguageMask(NATIVE_MASK);

    /* preset mask for JAVA only */
    public static final LanguageMask JAVA = new LanguageMask(JAVA_MASK);

    private int value;

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public LanguageMask()
    {
        value = 0;
    }

    //--------------------------------------------
    /**
     * Private Constructor
     */
    //--------------------------------------------
    private LanguageMask(int val)
    {
        value = val;
    }


    //--------------------------------------------
    /**
     * Adds JAVA to the language mask
     */
    //--------------------------------------------
    public void addJava()
    {
        value |= JAVA_MASK;
    }

    //--------------------------------------------
    /**
     * Return true if JAVA is defined in the mask
     */
    //--------------------------------------------
    public boolean hasJava()
    {
        return ((value & JAVA_MASK) > 0);
    }

    //--------------------------------------------
    /**
     * Adds C to the language mask
     */
    //--------------------------------------------
    public void addNative()
    {
        value |= NATIVE_MASK;
    }

    //--------------------------------------------
    /**
     * Return true if C is defined in the mask
     */
    //--------------------------------------------
    public boolean hasNative()
    {
        return ((value & NATIVE_MASK) > 0);
    }

    //--------------------------------------------
    /**
     * Reset the flags
     */
    //--------------------------------------------
    public void clear()
    {
        value = 0;
    }
}
