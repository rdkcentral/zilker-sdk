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

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Proxy;

/**
 * Base class for ManagerFactories. This provides the basic functionality of
 * exposing a public interface and a private interface.
 * 
 * @author mkoch
 **/
public class ManagerFactoryBase<T>
{
    // TODO could use developer key or something similar to control access
    private static boolean supportsPrivateAPI = true;
    private Class<?> publicInterface = null;
    private Class<?> privateInterface = null;
    private InvocationHandler invocationHandler = null;
    private T instance;

    /**
     * Constructor for this class
     */
    public ManagerFactoryBase(Class<?> publicInterface, Class<?> privateInterface, InvocationHandler invocationHandler)
    {
        this.publicInterface = publicInterface;
        this.privateInterface = privateInterface;
        this.invocationHandler = invocationHandler;
    }

    /**
     * Get a client instance for the API.
     * 
     * @return a client instance, which depending on security may include both
     *         private and public interfaces
     */
    public synchronized T getAPIInstance()
    {
        if (instance == null)
        {
            instance = getProxyInstance(publicInterface, privateInterface, invocationHandler);
        }

        return instance;
    }

    /**
     * Helper method to get a proxy instance that limits access based on
     * security
     * 
     * @param publicInterface
     *            the public interface for the instance
     * @param privateInterface
     *            the private interface for the instance
     * @param invocationHandler
     *            the invocation handler for the instance
     * @return the proxy instance that at a minimum implements the public
     *         interface, but which may also implement the private instance if
     *         security allows
     */
    @SuppressWarnings("unchecked")
    protected static <X> X getProxyInstance(Class<?> publicInterface, Class<?> privateInterface,
            InvocationHandler invocationHandler)
    {
        Class<?>[] interfaces;
        if (privateInterface == null)
        {
            // Only expose the public interface
            interfaces = new Class[] { publicInterface };
        }
        else
        {
            // Expose both interfaces
            interfaces = new Class[] { publicInterface, privateInterface };
        }
        return (X) Proxy.newProxyInstance(ManagerFactoryBase.class.getClassLoader(), interfaces, invocationHandler);
    }
}

// +++++++++++
// EOF
// +++++++++++
