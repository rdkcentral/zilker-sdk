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

import java.io.IOException;
import java.util.Iterator;
import java.util.List;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.parse.EnumDefWrapper;
import com.icontrol.generate.service.parse.Wrapper;
import com.icontrol.generate.service.variable.EnumVariable;

//-----------------------------------------------------------------------------
/**
 * CodeProducer for Native Enum
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class EnumNativeCodeProducer extends CodeProducer
{
    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public EnumNativeCodeProducer()
    {
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generate(Context, LanguageMask)
     */
    //--------------------------------------------
    @Override
    public boolean generate(Context ctx, LanguageMask lang)
        throws IOException
    {
        // Native Enums aren't created by themselves - always in conjunction
        // with POJO, Event, or Messages
        //
        return false;
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generateWrapperFile(Context, LanguageMask, Wrapper)
     */
    //--------------------------------------------
    @Override
    public boolean generateWrapperFile(Context ctx, LanguageMask lang, Wrapper wrapper)
        throws IOException
    {
        // Native Enums aren't created by themselves - always in conjunction
        // with POJO, Event, or Messages
        //
        return false;
    }

    //--------------------------------------------
    /**
     * @see CodeProducer#generateWrapperBuffer(Context, LanguageMask, Wrapper)
     */
    //--------------------------------------------
    @Override
    public StringBuffer generateWrapperBuffer(Context ctx, LanguageMask lang, Wrapper wrapper)
        throws IOException
    {
        // generate the enum into a buffer
        //
        StringBuffer retVal = new StringBuffer();
        EnumDefWrapper enumWrapper = (EnumDefWrapper)wrapper;

        // assume lang is Native.  generate the typedef for this one Wrapper
        //
        generateNativeWrapperBuffer(ctx, enumWrapper, retVal);

        return retVal;
    }

    //--------------------------------------------
    /**
     * Convenience routine to generate the "typedef" of
     * the supplied EnumWrappers into a single StringBuffer
     */
    //--------------------------------------------
    protected StringBuffer generateNativeEnums(Context ctx, List<EnumDefWrapper> list, String includeName)
    {
        // first the Enums that that are not covered by Event generation
        //
        StringBuffer retVal = new StringBuffer();
        Iterator<EnumDefWrapper> enumLoop = list.iterator();
        while (enumLoop.hasNext() == true)
        {
            // get the struct for this enum and place in our buffer
            //
            EnumDefWrapper curr = enumLoop.next();
            generateNativeWrapperBuffer(ctx, curr, retVal);

            // tell the wrapper what header path this was generated
            // in so consumers know how to #include for this enum
            //
            if (includeName != null)
            {
                curr.getContextNative().setHeaderFileName(includeName);
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    void generateNativeWrapperBuffer(Context ctx, EnumDefWrapper wrapper, StringBuffer buffer)
    {
        // create a new EnumVariable to produce the struct
        //
        EnumVariable var = wrapper.getVariable();
        StringBuffer typedef = var.getNativeStructString();
        if (typedef != null)
        {
            buffer.append(typedef);
            buffer.append('\n');
        }

        // if adding the "labels" do that here
        //
        if (var.generateNativeLabels() == true)
        {
            StringBuffer labels = var.getNativeLabelsArrayString();
            if (labels != null && labels.length() > 0)
            {
                buffer.append(labels);
                buffer.append('\n');
            }
        }

        // if adding the "shortNames" do that here
        //
        if (var.generateNativeShortNames() == true)
        {
            StringBuffer shortNames = var.getNativeShortNameArrayString();
            if (shortNames != null && shortNames.length() > 0)
            {
                buffer.append(shortNames);
                buffer.append('\n');
            }
        }
    }
}


