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

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.LinkedHashMap;

import com.icontrol.generate.service.context.Context;
import com.icontrol.generate.service.ipc.Message;
import com.icontrol.generate.service.ipc.MessageParam;
import com.icontrol.generate.service.variable.ArrayVariable;
import com.icontrol.generate.service.variable.BaseVariable;
import com.icontrol.generate.service.variable.CustomVariable;
import com.icontrol.generate.service.variable.EnumVariable;
import com.icontrol.xmlns.service.ICIpc;
import com.icontrol.xmlns.service.ICMessage;
import com.icontrol.xmlns.service.ICService;

//-----------------------------------------------------------------------------
/**
 * ProcessorHelper instance to create MessageWrapper objects
 *
 * @author jelderton
 */
//-----------------------------------------------------------------------------
public class MessageProcessor extends ProcessorHelper
{
    private Map<Integer,MessageWrapper> messageLookup;

    //--------------------------------------------
    /**
     * Called by Processor during initialization
     */
    //--------------------------------------------
    static MessageProcessor buildModel(ICService top)
    {
        MessageProcessor retVal = new MessageProcessor();

        // see if there are messages defined in the XML
        //
        if (top.isSetIpc() == false)
        {
            return retVal;
        }

        // create a Message object for each bean
        // then shove that into a MessageWrapper
        //
        ICIpc bean = top.getIpc();
        List<ICMessage> msgList = bean.getMessageList();
        for (int i = 0 ; i < msgList.size() ; i++)
        {
            // create the Message & the MessageWrapper
            // Note that the constructor sets the scope (Java, C, etc)
            //
            Message msg = new Message(msgList.get(i));
            MessageWrapper wrapper = new MessageWrapper(msg);
            retVal.messageLookup.put(msg.getCode(), wrapper);
        }

        return retVal;
    }

    //--------------------------------------------
    /**
     * private constructor
     */
    //--------------------------------------------
    private MessageProcessor()
    {
        messageLookup = new LinkedHashMap<Integer, MessageWrapper>();
    }

    //--------------------------------------------
    /**
     * Called by Processor during initialization
     * @see ProcessorHelper#analyze(ICService, Context)
     */
    //--------------------------------------------
    @Override
    public void analyze(ICService top, Context ctx)
    {
        // loop through defined events to see which POJO/Enum items are used by messages
        //
        Iterator<Wrapper> loop = getWrappers();
        while (loop.hasNext() == true)
        {
            // get the POJO (regular or event) and Enum that this
            // particular message utilizes for input or output
            //
            MessageWrapper wrapper = (MessageWrapper)loop.next();
            Message msg = wrapper.getMessage();
            analyzeMessageParam(msg.getInParm());
            analyzeMessageParam(msg.getOutParm());
        }
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void analyzeMessageParam(MessageParam param)
    {
        if (param == null)
        {
            return;
        }
        BaseVariable var = param.getField();
        if (var == null)
        {
            return;
        }

        analyzeBase(var);
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void analyzeBase(BaseVariable var)
    {
        if (var.isEvent() == true)
        {
            // find the EventWrapper
            //
            EventWrapper event = Processor.getEventProcessor().getEventForName(var.getName());
            if (event != null)
            {
                // loop through the items within this EventVar
                //
                event.getSectionMask().addMessageSection();
                analyzeCustom((CustomVariable)var);
            }
        }
        else if (var.isCustom() == true)
        {
            // find the PojomWrapper
            //
            PojoWrapper pojo = Processor.getPojoProcessor().getPojoForName(var.getName());
            if (pojo != null)
            {
                // loop through the items within this CustomVar
                //
                pojo.getSectionMask().addMessageSection();
                analyzeCustom((CustomVariable)var);
            }
        }
        else if (var.isEnum() == true)
        {
            EnumVariable enumVar = (EnumVariable)var;
            EnumDefWrapper enumWrapper = Processor.getEnumProcessor().getEnumForDefinitionName(enumVar.getName());
            if (enumWrapper != null)
            {
                enumWrapper.getSectionMask().addMessageSection();
            }
        }
        else if (var.isArray() == true)
        {
            // look at elements of this array
            //
            ArrayVariable arrayVar = (ArrayVariable)var;
            BaseVariable element = arrayVar.getFirstElement();
            analyzeBase(element);
            PojoWrapper pojo = Processor.getPojoProcessor().getPojoForName(var.getName());
            if (pojo != null)
            {
                pojo.getSectionMask().addMessageSection();
            }
        }
    }

    //--------------------------------------------
    /**
     *
     */
    //--------------------------------------------
    private void analyzeCustom(CustomVariable var)
    {
        ArrayList<BaseVariable> list = var.getVariables();
        for (int i = 0 ; i < list.size() ; i++)
        {
            BaseVariable next = list.get(i);
            analyzeBase(next);
        }
    }

    //--------------------------------------------
    /**
     * Called by Processor during code generation
     * @see ProcessorHelper#getWrappers()
     */
    //--------------------------------------------
    @Override
    public Iterator<Wrapper> getWrappers()
    {
        // place each into a List for the Iterator
        //
        ArrayList<Wrapper> tmp = new ArrayList<Wrapper>();
        Iterator<Integer> loop = messageLookup.keySet().iterator();
        while (loop.hasNext() == true)
        {
            // fugly, but does the job since we aren't running
            // in a multi-threaded mode and run the risk of NULL
            //
            tmp.add(messageLookup.get(loop.next()));
        }

        return tmp.iterator();
    }
}
