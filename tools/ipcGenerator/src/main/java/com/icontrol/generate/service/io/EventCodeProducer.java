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
 * Base class for genereating Java & Native Event
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public abstract class EventCodeProducer extends CodeProducer
    implements TemplateMappings
{
    static final String EVENT_SUFFIX = "_event";           // used in filename calculation

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    public EventCodeProducer()
    {
    }

    //--------------------------------------------
    /**
     * Iterate through the all EventWrapper objects
     * to determine which should be generated as some
     * may be part of the POJO section
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
            // see what Enums to use during Event Generation
            //
            EnumDefWrapper wrapper = (EnumDefWrapper)loop.next();
            SectionMask region = wrapper.getSectionMask();
            if (region.inEventSection() == true &&
                wrapper.getContextNative().wasGenerated() == false)
            {
                // need to process this one
                //
                retVal.add(wrapper);
            }
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * Return the POJO wrappers that event generation depends on
     * (i.e. any POJOs referred to within the event definitions)
     */
    //--------------------------------------------
    protected Iterator<Wrapper> getEventDependencies(boolean isNative)
    {
        ArrayList<Wrapper> tmp = new ArrayList<Wrapper>();

        scanWrappersForDependencies(Processor.getEnumProcessor().getWrappers(), tmp, isNative);
        scanWrappersForDependencies(Processor.getPojoProcessor().getWrappers(), tmp, isNative);

        return tmp.iterator();
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void scanWrappersForDependencies(Iterator<Wrapper> list, ArrayList<Wrapper> target, boolean isNative)
    {
        while (list.hasNext() == true)
        {
            // first see if the wrapper has our 'section' and matches our native vs java need
            //
            Wrapper wrapper = list.next();
            if (wrapper.getSectionMask().inEventSection() == false)
            {
                continue;
            }
            if ((isNative == true && wrapper.getLanguageMask().hasNative() == false) ||
                (isNative == false && wrapper.getLanguageMask().hasJava() == false))
            {
                continue;
            }

            // part of the dependency, so add to 'target'
            //
            target.add(wrapper);
        }
    }
}


