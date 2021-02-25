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

import com.icontrol.ipc.impl.ObjectIPC;

//-----------------------------------------------------------------------------
// Class definition:    JavaEvent
/**
 *  Interface to signify that the BaseEvent is produced by a Java-based Service.
 *  Not overly interesting to know except for the Producer/Consumer code.
 *  <p>
 *  To keep the platforms as agnostic as we can, the implementations of this 
 *  will marshal/unmarshal the content using the ObjectIPC interface.
 *  <p>
 *  When sent/received, it will be done using the define IC_EVENT_MULTICAAST_GROUP
 *  so that sending and receiving can be multicast and not serial.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public interface JavaEvent extends ObjectIPC
{
    // Define the multicast group for BaseEvents to be sent over.
    // This is the same multicast group as our Native code
    //
    public static final String IC_EVENT_MULTICAST_GROUP = "225.0.0.50";
//    public static final String IC_EVENT_MULTICAST_GROUP = "229.9.9.9"; -- zigbee group

}

//+++++++++++
//EOF
//+++++++++++