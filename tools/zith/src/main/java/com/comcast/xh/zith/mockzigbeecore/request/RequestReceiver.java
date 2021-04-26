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

package com.comcast.xh.zith.mockzigbeecore.request;

//
// Created by tlea on 1/4/19.
//

import com.fasterxml.jackson.core.JsonParser;
import com.fasterxml.jackson.core.Version;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.module.SimpleModule;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;

public class RequestReceiver
{
    private static Logger log = LoggerFactory.getLogger(RequestReceiver.class);

    private int portNumber;
    private Thread thread = null;
    private boolean running = false;
    private ServerSocket serverSocket = null;

    public RequestReceiver(int portNumber)
    {
        this.portNumber = portNumber;
    }

    public void start()
    {
        log.debug("start");

        if (thread == null)
        {
            try
            {
                serverSocket = new ServerSocket(portNumber);

                thread = new ReceiverThread();
                running = true;
                thread.start();
            }
            catch(IOException e)
            {
                throw new IllegalStateException("Cannot bind to zhal receiver port, ZigbeeCore is running", e);
            }
        }
    }

    public void stop()
    {
        log.debug("stop");

        if (thread != null)
        {
            try
            {
                running = false;
                serverSocket.close();
                serverSocket = null;
                thread.interrupt();
                thread.join();
            } catch (Exception e)
            {
                log.error("Failed to stop cleanly", e);
            }

            thread = null;
        }
    }

    // Our persistent task that receives requests
    private class ReceiverThread extends Thread
    {
        @Override
        public void run()
        {
            log.debug("ReceiverThread starting");

            try
            {
                ObjectMapper mapper = new ObjectMapper().configure(JsonParser.Feature.AUTO_CLOSE_SOURCE, false);
                SimpleModule module = new SimpleModule("RequestDeserializer", new Version(1, 0, 0, null, null, null));
                module.addDeserializer(Request.class, new RequestDeserializer());
                mapper.registerModule(module);

                String syncResponse = "{\"resultCode\": 0}";
                ByteArrayOutputStream syncResponseBaos = new ByteArrayOutputStream();
                syncResponseBaos.write(0);
                syncResponseBaos.write(syncResponse.length());
                syncResponseBaos.write(syncResponse.getBytes());
                byte[] syncResponseBuf = syncResponseBaos.toByteArray();
                syncResponseBaos.close();

                while (running && !isInterrupted())
                {

                    try (Socket s = serverSocket.accept())
                    {
                        //discard first 2 bytes (length)
                        byte[] length = new byte[2];
                        s.getInputStream().read(length);

                        Request request = mapper.readValue(s.getInputStream(), Request.class);

                        if (request != null)
                        {
                            s.getOutputStream().write(syncResponseBuf);

                            request.execute();
                        }
                        else
                        {
                            log.error("Unsupported request");
                        }
                    } catch (SocketException e)
                    {
                        //this is expected on shutdown
                        log.debug("SocketException, shutting down");
                        break;
                    }
                }

            } catch (IOException e)
            {
                e.printStackTrace();
            }

            log.debug("ReceiverThread exiting");
        }
    }
}
