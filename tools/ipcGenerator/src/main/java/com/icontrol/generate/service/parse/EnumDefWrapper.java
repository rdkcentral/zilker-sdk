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

import com.icontrol.generate.service.TemplateMappings;
import com.icontrol.generate.service.variable.EnumDefinition;
import com.icontrol.generate.service.variable.EnumVariable;

//-----------------------------------------------------------------------------
/**
 * Created by jelderton on 4/17/15.
 */
//-----------------------------------------------------------------------------
public class EnumDefWrapper extends Wrapper
    implements TemplateMappings
{
    private EnumDefinition def;

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    EnumDefWrapper(EnumDefinition var)
    {
        // save our variable and prime a language mask
        //
        super();
        def = var;
    }

//    //--------------------------------------------
//    /**
//     * Return the enum variable
//     * @see Wrapper#getVariable()
//     */
//    //--------------------------------------------
//    public EnumVariable getVariable()
//    {
//        return variable;
//    }

    public EnumDefinition getEnumDefinition()
    {
    	return def;
    }
    
    public EnumVariable getVariable()
    {
    	// TODO: cache this?
        return new EnumVariable(def);
    }
    
}
