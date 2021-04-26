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
import com.icontrol.generate.service.ipc.Message;
import com.icontrol.generate.service.variable.BaseVariable;

//-----------------------------------------------------------------------------
/**
 * Created by jelderton on 4/17/15.
 */
//-----------------------------------------------------------------------------
public class MessageWrapper extends Wrapper
    implements TemplateMappings
{
    private Message message;

    //--------------------------------------------
    /**
     * Constructor
     */
    //--------------------------------------------
    MessageWrapper(Message msg)
    {
        super();
        sectionMask.addMessageSection();
        message = msg;

        // adjust the scope (Java, C, etc)
        //
        if (msg.supportsC() == true)
        {
            getLanguageMask().addNative();
        }
        if (msg.supportsJava() == true)
        {
            getLanguageMask().addJava();
        }
    }

    //--------------------------------------------
    /**
     * @see Wrapper#getVariable()
     */
    //--------------------------------------------
    public BaseVariable getVariable()
    {
        // not applicable for IPC Messages
        //
        return null;
    }

    //--------------------------------------------

    @Override
    public String toString()
    {
        StringBuffer retVal = new StringBuffer("Message id="+message.getName());
        if (message.getInParm() != null)
        {
            retVal.append(" in="+message.getInParm().getField().getName());
        }
        if (message.getOutParm() != null)
        {
            retVal.append(" out="+message.getOutParm().getField().getName());
        }
        return retVal.toString();
    }

    /**
     * Return the Message object this wrapper represents
     */
    //--------------------------------------------
    public Message getMessage()
    {
        return message;
    }
}
