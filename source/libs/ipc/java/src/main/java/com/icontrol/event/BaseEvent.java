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

import java.util.Date;

import org.json.JSONException;
import org.json.JSONObject;

import com.icontrol.ipc.impl.ObjectIPC;

//-----------------------------------------------------------------------------
// Class definition:    BaseEvent
/**
 *  Base class for all of the Event objects (both generated or manual).  
 *  Subclasses can be generated via the 'buildTools' utilities and created 
 *  directly in Java or via API libraries.
 *  <p>
 *  Note that these can be produced by Native or Java producers
 *  
 *  @author jelderton
 **/
//-----------------------------------------------------------------------------
public abstract class BaseEvent
    implements ObjectIPC
{
    /*
     * Set of JSON keys that are commonly used when encoding/decoding
     * the BaseEvent to/from JSON.  These should correlate to BaseEvent.h
     * to ensure transition between Native and Java works the same.
     */
    public static final String EVENT_ID_JSON_KEY       = "_evId";
    public static final String EVENT_CODE_JSON_KEY     = "_evCode";
    public static final String EVENT_VALUE_JSON_KEY    = "_evVal";
    public static final String EVENT_TIME_JSON_KEY     = "_evTime";

    /*
     * id of the service this event came from.  used when filtering out
     * which events the receiver is listening for.
     */
    public static final String SERVICE_ID_JSON_KEY     = "_svcIdNum";

    // information that's in all Events
    //
    protected long    eventId;
    protected int     eventCode;
    protected int     eventValue;
    protected Date    eventTime;
    
    //--------------------------------------------
    /**
     * Constructor for this class
     */
    //--------------------------------------------
    public BaseEvent(long id)
    {
        eventId = id;
    }

    //--------------------------------------------
    /**
     * @return Returns the eventId.
     */
    //--------------------------------------------
    public long getEventId()
    {
        return eventId;
    }

    //--------------------------------------------
    /**
     * @param id The eventId to set.
     */
    //--------------------------------------------
    public void setEventId(long id)
    {
        this.eventId = id;
    }

    //--------------------------------------------
    /**
     * @return Returns the eventCode.
     */
    //--------------------------------------------
    public int getEventCode()
    {
        return eventCode;
    }

    //--------------------------------------------
    /**
     * @param eventCode The eventCode to set.
     */
    //--------------------------------------------
    public void setEventCode(int code)
    {
        this.eventCode = code;
    }

    //--------------------------------------------
    /**
     * @return Returns the eventValue.
     */
    //--------------------------------------------
    public int getEventValue()
    {
        return eventValue;
    }

    //--------------------------------------------
    /**
     * @param eventValue The eventValue to set.
     */
    //--------------------------------------------
    public void setEventValue(int val)
    {
        this.eventValue = val;
    }
    
    //--------------------------------------------
    /**
     * @return Returns the eventTime.
     */
    //--------------------------------------------
    public Date getEventTime()
    {
        return eventTime;
    }

    //--------------------------------------------
    /**
     * @param eventTime The eventTime to set.
     */
    //--------------------------------------------
    public void setEventTime(long millis)
    {
        // coming from C where millis aren't used
        //
        this.eventTime = new Date(millis);
    }
    
    //--------------------------------------------
    /**
     * @param tv_sec the whole seconds part of the eventTime to set
     * @param tv_nsec the nanoseconds part of the eventTime to set
     */
    //--------------------------------------------
    public void setEventTime(long tv_sec, long tv_nsec)
    {
        // coming from C, but allowing for fractional seconds
        //
        long millis = tv_nsec / 1000000;
        millis += (tv_sec * 1000);
        this.eventTime = new Date(millis);
    }

    //--------------------------------------------
    /**
     * @param eventTime The eventTime to set.
     */
    //--------------------------------------------
    public void setEventTime(Date eventTime)
    {
        this.eventTime = eventTime;
    }
    
    //--------------------------------------------
    /**
     *  Used for debugging
     */
    //--------------------------------------------
    public String toString()
    {
    	StringBuilder sb = new StringBuilder();
    	sb.append("[Event: class="); sb.append(this.getClass().getSimpleName());
    	sb.append(", id="); sb.append(eventId);
    	sb.append(", code="); sb.append(eventCode);
    	sb.append(", value="); sb.append(eventValue);
    	sb.append("]");
    	return sb.toString();
    }

    //--------------------------------------------
    /**
     * Encode using JSON
     * @see ObjectIPC#toJSON()
     */
    //--------------------------------------------
    @Override
    public JSONObject toJSON() throws JSONException
    {
        JSONObject buffer = new JSONObject();
        buffer.put(EVENT_ID_JSON_KEY, eventId);
        buffer.put(EVENT_CODE_JSON_KEY, eventCode);
        buffer.put(EVENT_VALUE_JSON_KEY, eventValue);
        if (eventTime != null)
        {
            // store as long (milliseconds)
            //
            buffer.put(EVENT_TIME_JSON_KEY, eventTime.getTime());
        }

        return buffer;
    }

    //--------------------------------------------
    /**
     * Decode using JSON
     * @see ObjectIPC#fromJSON(JSONObject)
     */
    //--------------------------------------------
    @Override
    public void fromJSON(JSONObject buffer) throws JSONException
    {
        eventId = buffer.optLong(EVENT_ID_JSON_KEY);
        eventCode = buffer.optInt(EVENT_CODE_JSON_KEY);
        eventValue = buffer.optInt(EVENT_VALUE_JSON_KEY);
        long eventTime_time = buffer.optLong(EVENT_TIME_JSON_KEY);
        if (eventTime_time > 0)
        {
            // convert form long (milliseconds) to Date
            //
            eventTime = new Date(eventTime_time);
        }
    }

    /*
     * extract the "serviceIdNum" from a raw event (to find where it came from)
     */
    public static int extractServiceIdFromRawEvent(JSONObject buffer)
    {
        return buffer.optInt(SERVICE_ID_JSON_KEY);
    }

    /*
     * embed the "serviceIdNum" into a raw event (to indicate where the event originated from)
     */
    public static void insertServiceIdToRawEvent(JSONObject buffer, int serviceIdNum)
        throws JSONException
    {
        buffer.put(SERVICE_ID_JSON_KEY, serviceIdNum);
    }
}

//+++++++++++
//EOF
//+++++++++++