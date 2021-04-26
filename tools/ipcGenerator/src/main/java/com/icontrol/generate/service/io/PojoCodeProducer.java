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

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import com.icontrol.generate.service.TemplateMappings;
import com.icontrol.generate.service.parse.EnumDefWrapper;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.SectionMask;
import com.icontrol.generate.service.parse.Wrapper;

//-----------------------------------------------------------------------------
/**
 * CodeProducer for Java and Native POJO generation
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public abstract class PojoCodeProducer extends CodeProducer
    implements TemplateMappings
{
    //--------------------------------------------

    /**
     * Constructor
     */
    //--------------------------------------------
    public PojoCodeProducer()
    {
    }

    //--------------------------------------------

    /**
     * Iterate through the all PojoWrapper objects
     * to determine which have dependencies on Enum.
     * Build up a list of unique EnumWrappers that
     * are not designated for Events
     */
    //--------------------------------------------
    protected List<EnumDefWrapper> getEnumsToGenerate()
    {
        ArrayList<EnumDefWrapper> retVal = new ArrayList<EnumDefWrapper>();

        // get all enums
        //
        Iterator<Wrapper> loop = Processor.getEnumProcessor().getWrappers();
        while (loop.hasNext() == true)
        {
            // see what enums this pojo depends on and skip if
            // not for POJO or has already been generated
            //
            EnumDefWrapper wrapper = (EnumDefWrapper) loop.next();
            SectionMask region = wrapper.getSectionMask();
            if (region.inPojoSection() == true &&
                region.inEventSection() == false &&
                wrapper.getContextNative().wasGenerated() == false)
            {
                // need to process this one
                //
                retVal.add(wrapper);
            }
        }

        return retVal;
    }
}



