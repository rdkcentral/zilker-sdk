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

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

//-----------------------------------------------------------------------------
// Class definition:    IPCMessage
/**
 *  Container used to encode/decode messages used for a "request" or a "reply" as
 *  part of our IPC protocol.  This supports both Java and C clients, but assumes 
 *  a Java server (obvously since this is written in Java).
 *  <p>
 *  The protocol used is as follows:
 *  <pre>
 *    request:
 *      messageCode    - 1 32-bit integer
 *      messagePayload - 1 32-bit integer, and a series of 1-byte characters (UTF string)
 *      
 *    reply:
 *      returnCode     - 1 32-bit integer
 *      returnPayload  - 1 32-bit integer, and a series of 1-byte characters (UTF string)
 *  </pre>  
 *  
 *  Currently, the 'payload' portions are JSON strings that will need to be decoded.  The
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public class IPCMessage
{
//private static Logger log = LoggerFactory.getLogger("IPCMessage");
    
    private String   payload;
    private int      messageCode;    // the request code 
    private int      returnCode;     // the reply code (generally 0 == success)
    
    //--------------------------------------------
    /**
     * Decodes a request.
     */
    //--------------------------------------------
    static IPCMessage decodeRequest(InputStream stream) throws IOException
    {
        IPCMessage retVal = new IPCMessage();
        
        // use the static helper function
        //
        StringBuffer json = new StringBuffer();
        int code = decode(stream, json);

        // populate the return object
        //
        retVal.setMessageCode(code);
        retVal.setPayload(json.toString());
        
        return retVal;
    }

    //--------------------------------------------
    /**
     * Decodes a reply.
     */
    //--------------------------------------------
    static IPCMessage decodeReply(InputStream stream) throws IOException
    {
        IPCMessage retVal = new IPCMessage();
        
        // use the static helper function
        //
        StringBuffer json = new StringBuffer();
        int code = decode(stream, json);

        // populate the return object
        //
        retVal.setReturnCode(code);
        retVal.setPayload(json.toString());
        
        return retVal;
    }

    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public IPCMessage()
    {
        messageCode = 0;
        returnCode = 0;
        payload = null;
    }

    //--------------------------------------------
    /**
     * Returns the messageCode.  Only valid
     * when this was used as a request.
     */
    //--------------------------------------------
    public int getMessageCode()
    {
        return messageCode;
    }

    //--------------------------------------------
    /**
     * @param code The messageCode to set.
     */
    //--------------------------------------------
    void setMessageCode(int code)
    {
        this.messageCode = code;
    }
    
    //--------------------------------------------
    /**
     * Returns the returnCode.  Only valid when
     * used as a reply.
     */
    //--------------------------------------------
    public int getReturnCode()
    {
        return returnCode;
    }

    //--------------------------------------------
    /**
     * @param returnCode The returnCode to set.
     */
    //--------------------------------------------
    public void setReturnCode(int returnCode)
    {
        this.returnCode = returnCode;
    }

    //--------------------------------------------
    /**
     * @return Returns the payload.
     */
    //--------------------------------------------
    public String getPayload()
    {
        return payload;
    }
    
    //--------------------------------------------
    /**
     * @param payload The payload to set.
     */
    //--------------------------------------------
    public void setPayload(String payload)
    {
        this.payload = payload;
    }

    //--------------------------------------------
    /**
     * Encode the message to be sent as a request
     */
    //--------------------------------------------
    void encodeRequest(OutputStream stream) throws IOException
    {
        // use static function helper
        //
        encode(stream, messageCode, payload);
    }
    
    //--------------------------------------------
    /**
     * Encode the message to be sent as a reply
     */
    //--------------------------------------------
    void encodeReply(OutputStream stream) throws IOException
    {
        // use static function helper
        //
        encode(stream, returnCode, payload);
    }
    
    //--------------------------------------------
    /**
     * Encode a request/reply (they are the same format).
     */
    //--------------------------------------------
    private static void encode(OutputStream stream, int code, String payload) throws IOException
    {
        DataOutputStream helper = null;
        
        // encode the stream using a DataOutputStream 
        //
        helper = new DataOutputStream(stream);

        // removed comment to prevent printing sensitive information to the log such as "user code"
//log.debug("encoding code="+code+" json="+payload);

        // add the code
        //
        helper.writeInt(code);

        // now the payload (even if null)
        //
        if (payload == null)
        {
            // no payload, so send 0 for the length
            //
            helper.writeInt(0);
        }
        else
        {
            // to allow for larger then 64k, send the string length
            // then the bytes of the string
            //
            byte[] payloadBytes = payload.getBytes("UTF-8");
            helper.writeInt(payloadBytes.length);
            helper.write(payloadBytes);
        }
        helper.flush();
    }
    
    //--------------------------------------------
    /**
     * Decodes a request/reply (they are the same format).
     */
    //--------------------------------------------
    private static int decode(InputStream stream, StringBuffer payload) throws IOException
    {
        DataInputStream helper = null;
        int rc = -1;

        // decode using a DataInputStream
        //
        helper = new DataInputStream(stream);

        // same order as encode...
        //
        rc = helper.readInt();
        int payloadLen = helper.readInt();
        byte[] payloadBytes = new byte[payloadLen];
        helper.readFully(payloadBytes);
        String json = new String(payloadBytes, "UTF-8");

        // removed comment to prevent printing sensitive information to the log such as "user code"
//log.debug("decoding code="+rc+" json="+json);

        if (json != null)
        {
            payload.append(json);
        }

        return rc;
    }
}

//+++++++++++
//EOF
//+++++++++++