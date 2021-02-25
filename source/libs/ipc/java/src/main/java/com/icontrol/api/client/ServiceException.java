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

package com.icontrol.api.client;

/**
 * A general base class for any service level exceptions
 */
public class ServiceException extends Exception
{
    private static final long serialVersionUID = 1L;
    
    /**
     * Create a new ServiceException
     */
    public ServiceException()
    {
        super();
    }

    /**
     * Create a new ServiceException with more details and a root cause
     */
    public ServiceException(String detailMessage, Throwable throwable)
    {
        super(detailMessage, throwable);
    }

    /**
     * Create a new ServiceException with more details
     */
    public ServiceException(String detailMessage)
    {
        super(detailMessage);
    }

    /**
     * Create a new ServiceException with a root cause
     */
    public ServiceException(Throwable throwable)
    {
        super(throwable);
    }
}

//+++++++++++
//EOF
//+++++++++++