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

package com.icontrol.event;

//-----------
//imports
//-----------


//-----------------------------------------------------------------------------
// Class definition:    RequestEvent
/**
 *  Optional interface that various BaseEvent implementations could support.
 *  Meant for events that stem from a server request and contain the requestId.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public interface RequestEvent
{
    //--------------------------------------------
    /**
     * Return the requestId that caused an operation and
     * ultimately generated this event
     */
    //--------------------------------------------
    public long getRequestId();
}

//+++++++++++
//EOF
//+++++++++++