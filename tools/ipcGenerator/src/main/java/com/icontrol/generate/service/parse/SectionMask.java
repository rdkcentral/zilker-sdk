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

package com.icontrol.generate.service.parse;

//-----------------------------------------------------------------------------
/**
 * Define the set of sections a Wrapper may belong to
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class SectionMask
{
    private static final int    EVENT_MASK   = 0x1;     // event section
    private static final int    POJO_MASK    = 0x2;     // POJO section
    private static final int    MESSAGE_MASK = 0x4;     // IPC messages section

    private int value;

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public SectionMask()
    {
        value = 0;
    }

    //--------------------------------------------
    /**
     * Adds EVENT to the section mask
     */
    //--------------------------------------------
    public void addEventSection()
    {
        value |= EVENT_MASK;
    }

    //--------------------------------------------
    /**
     * Return true if EVENT is defined in the mask
     */
    //--------------------------------------------
    public boolean inEventSection()
    {
        return ((value & EVENT_MASK) > 0);
    }

    //--------------------------------------------
    /**
     * Adds POJO to the section mask
     */
    //--------------------------------------------
    public void addPojoSection()
    {
        value |= POJO_MASK;
    }

    //--------------------------------------------
    /**
     * Return true if POJO is defined in the mask
     */
    //--------------------------------------------
    public boolean inPojoSection()
    {
        return ((value & POJO_MASK) > 0);
    }

    //--------------------------------------------
    /**
     * Adds MESSAGE to the section mask
     */
    //--------------------------------------------
    public void addMessageSection()
    {
        value |= MESSAGE_MASK;
    }

    //--------------------------------------------
    /**
     * Return true if MESSAGE is defined in the mask
     */
    //--------------------------------------------
    public boolean inMessageSection()
    {
        return ((value & MESSAGE_MASK) > 0);
    }
}


