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

package com.icontrol.api;

//-----------
//imports
//-----------

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Provides a point of indirection for calling managers. Its assumed the that
 * the InvocationHandler implements the interfaces on the dynamic proxy, so we
 * just call through to the method on the invocation handler. In the future
 * additional logic could be added for additional security or other checks
 * 
 * @author mkoch
 **/
public class InvocationHandlerBase implements InvocationHandler
{

    @Override
    public Object invoke(Object instance, Method method, Object[] args) throws Throwable
    {
        Object retval = null;

        Method targetMethod = getClass().getMethod(method.getName(), method.getParameterTypes());
        targetMethod.setAccessible(true);
        if (targetMethod != null)
        {
            try
            {
                retval = targetMethod.invoke(this, args);
            }
            catch (InvocationTargetException ex)
            {
                // We just want to throw the real exception back out
                throw ex.getTargetException();
            }
        }

        return retval;
    }

}

// +++++++++++
// EOF
// +++++++++++