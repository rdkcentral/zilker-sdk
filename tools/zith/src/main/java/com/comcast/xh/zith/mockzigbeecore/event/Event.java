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

package com.comcast.xh.zith.mockzigbeecore.event;

//
// Created by tlea on 1/4/19.
//

import com.fasterxml.jackson.databind.ObjectMapper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.util.Date;

public class Event
{
    private static Logger log = LoggerFactory.getLogger(Event.class);

    private static ObjectMapper mapper = new ObjectMapper();

    private String eventType;

    public Event(String eventType)
    {
        this.eventType = eventType;
    }

    public String getEventType()
    {
        return eventType;
    }

    public String getTimestamp()
    {
        Date d = new Date();
        return String.valueOf(d.getTime());
    }

    public void send()
    {
        try
        {
            byte[] responseBytes = mapper.writeValueAsBytes(this);
            DatagramPacket packet = new DatagramPacket(responseBytes, responseBytes.length, InetAddress.getLoopbackAddress(), 8711);

            DatagramSocket socket = new DatagramSocket();
            socket.send(packet);
            socket.close();

            log.debug("Sent event: " + new String(responseBytes));

        } catch (Exception e)
        {
            log.error("Error sending event", e);
        }
    }
}
