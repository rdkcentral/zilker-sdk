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

package com.icontrol.ipc;

//-----------
//imports
//-----------

import com.icontrol.ipc.impl.ObjectIPC;

//-----------------------------------------------------------------------------
// Class definition:    IPCReceiverHandler
/**
 *  Methods to implement for processing an IPC (Inter Process Communication) request.
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public interface IPCReceiverHandler
{
    //--------------------------------------------
    /**
     * Process the request from a remote process or VM.  Within the supplied 'message' is the
     * 'messageCode' and optional input parameters, which is stored in the 'payload' and needs 
     * to be decoded by the implementing object.
     * <p>
     * Unlike devctl requests, input/output parms can be different.
     * Simply clear the palyload (or set it) into the 'message' before returning.
     * <p>
     * Should return a 0 on success.  The returned integer will be
     * returned to the requester within the 'message'.  Any additional data that 
     * needs to be transfered back should be done within the 'message.setPayload()'.
     */
    //--------------------------------------------
    public int handleMarshaledRequest(IPCMessage message);
    
    //--------------------------------------------
    /**
     * Process the request from within the same process (no marshaling required).
     * <p>
     * Should return a 0 on success.  The returned integer will be
     * returned to the requester, along with 'output'.
     */
    //--------------------------------------------
    public int handleDirectRequest(int messageCode, ObjectIPC input, ObjectIPC output);


}

//+++++++++++
//EOF
//+++++++++++