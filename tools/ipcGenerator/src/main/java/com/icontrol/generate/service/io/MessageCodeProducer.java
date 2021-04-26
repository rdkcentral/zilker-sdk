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

import com.icontrol.generate.service.TemplateMappings;
import com.icontrol.generate.service.parse.Processor;
import com.icontrol.generate.service.parse.Wrapper;

//-----------------------------------------------------------------------------
/**
 * CodeProducer for Java and Native IPC Message generation
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public abstract class MessageCodeProducer extends CodeProducer
    implements TemplateMappings
{
    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    MessageCodeProducer()
    {
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    protected Iterator<Wrapper> getMessageDependencies(boolean isNative)
    {
        ArrayList<Wrapper> tmp = new ArrayList<Wrapper>();

        scanWrappersForDependencies(Processor.getEnumProcessor().getWrappers(), tmp, isNative);
        scanWrappersForDependencies(Processor.getEventProcessor().getWrappers(), tmp, isNative);
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
            if (wrapper.getSectionMask().inMessageSection() == false)
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
