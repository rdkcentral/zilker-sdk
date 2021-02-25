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

package com.comcast.xh.zith.mockzigbeecore;

//
// Created by tlea on 1/4/19.
//

import com.comcast.xh.zith.device.EventWaitHelper;
import com.comcast.xh.zith.device.MockZigbeeDevice;
import com.comcast.xh.zith.mockzigbeecore.event.*;
import com.comcast.xh.zith.mockzigbeecore.request.RequestReceiver;
import org.springframework.shell.standard.ShellComponent;
import org.springframework.shell.standard.ShellMethod;
import org.springframework.shell.standard.ShellOption;
import org.springframework.shell.table.*;

import java.util.*;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

@ShellComponent
public class MockZigbeeCore
{
    public static int ZIGBEE_CORE_IPC_PORT = 18443;
    public static int ZIGBEE_CORE_EVENT_PORT = 8711;
    private static final String INVALID_NETWORK_KEY = "00000000000000000000000000000000";
    private static MockZigbeeCore instance = new MockZigbeeCore();

    private RequestReceiver requestReceiver;

    private List<MockZigbeeDevice> unpairedDevices;
    private Map<String, MockZigbeeDevice> pairedDevices;
    private boolean running = false;
    private ZigbeeStatusListener listener;
    private long networkJoinEndTimeNanos = -1;
    private Random random = new Random();
    private String eui64;
    private String originalEui64;
    private byte channel = 25;
    private short panId = 0;
    private String networkKey;
    private EventWaitHelper refreshOtaFilesEventWaitHelper = new EventWaitHelper();
    private AtomicBoolean startupSent = new AtomicBoolean(false);

    private MockZigbeeCore()
    {
        // TODO this should probably be done when networkInit is called.  Just need to take care of any timing issues
        eui64 = String.format("%016x", random.nextLong());
        originalEui64 = String.format("%016x", random.nextLong());
        while(panId == 0)
        {
            panId = (short) random.nextInt();
        }
        byte[] keyBytes = new byte[16];
        random.nextBytes(keyBytes);
        StringBuilder networkKeyBuilder = new StringBuilder();
        for(byte keyByte : keyBytes)
        {
            networkKeyBuilder.append(String.format("%02x", keyByte));
        }
        networkKey = networkKeyBuilder.toString();
    }

    public static MockZigbeeCore getInstance() { return instance; }

    public String getEui64()
    {
        return eui64;
    }

    public String getOriginalEui64()
    {
        return originalEui64;
    }


    @ShellMethod(value="Change the network configuration of Zigbee", key="zigbee-network-change")
    public void networkChange(
            @ShellOption(defaultValue="0") byte channel,
            @ShellOption(defaultValue="0") short panId,
            @ShellOption(defaultValue=INVALID_NETWORK_KEY) String networkKey)
    {
        boolean didChange = false;
        if (channel != 0 && channel != this.channel)
        {
            this.channel = channel;
            didChange = true;
        }

        if (panId != 0 && panId != this.panId)
        {
            this.panId = panId;
            didChange = true;
        }

        if (INVALID_NETWORK_KEY.equals(networkKey) == false && networkKey != null && !networkKey.equals(this.networkKey))
        {
            this.networkKey = networkKey;
            didChange = true;
        }

        if (didChange)
        {
            //Finally, send a network changed event like ZigbeeCore does
            new NetworkConfigChangedEvent().send();

            // TODO should probably have devices "rejoin" to copy real behavior
        }
    }

    public byte getChannel()
    {
        return channel;
    }

    public short getPanId()
    {
        return panId;
    }

    public String getNetworkKey()
    {
        return networkKey;
    }

    @ShellMethod(value="Get Zigbee Network Status", key="zigbee-network-status")
    public Table getNetworkStatus()
    {
        LinkedHashMap<String, Object> headers = new LinkedHashMap<>();
        headers.put("running", "Up");
        headers.put("networkOpenForJoin", "Open For Join");
        headers.put("eui64", "EUI64");
        headers.put("originalEui64", "Original EUI64");
        headers.put("channel", "Channel");
        headers.put("panId", "PAN ID");
        headers.put("networkKey", "Network Key");
        TableModel model = new BeanListTableModel(Collections.singletonList(this), headers);
        TableBuilder tableBuilder = new TableBuilder(model);
        // TODO could format PAN ID as hex tableBuilder.on(column == 4).formatter(...)
        return tableBuilder.addFullBorder(BorderStyle.fancy_light).build();
    }

    public void setStatusListener(ZigbeeStatusListener listener)
    {
        this.listener = listener;
    }

    private void setRunning(boolean running)
    {
        this.running = running;
        if (listener != null)
        {
            listener.statusChanged(running);
        }
    }

    @ShellMethod(value="Start the Mock ZigbeeCore", key="zigbee-start")
    public void start()
    {
        if(running)
        {
            throw new RuntimeException("invalid operation: already running");
        }

        unpairedDevices = new ArrayList<MockZigbeeDevice>();
        pairedDevices = new HashMap<String, MockZigbeeDevice>();

        requestReceiver = new RequestReceiver(ZIGBEE_CORE_IPC_PORT);
        requestReceiver.start();

        //send our standard zhalStartup event
        new ZhalStartupEvent().send();
        setRunning(true);
    }

    @ShellMethod(value="List devices",key={"list-devices","list"})
    public Table listDevices()
    {
        List<MockZigbeeDevice> devices = MockZigbeeCore.getInstance().getAllDevices();
        // TODO filter by deviceType, should be enum?
        LinkedHashMap<String, Object> headers = new LinkedHashMap<>();
        headers.put("name", "Name");
        headers.put("class.simpleName", "Type");
        headers.put("eui64", "EUI64");
        headers.put("discovered", "Discovered");
        TableModel model = new BeanListTableModel(devices, headers);
        TableBuilder tableBuilder = new TableBuilder(model);
        Table table = tableBuilder.addFullBorder(BorderStyle.fancy_light).build();
        return table;
    }

    public boolean isRunning()
    {
        return running;
    }

    public void stop()
    {
        if(!running)
        {
            throw new RuntimeException("invalid operation: not running");
        }

        setRunning(false);
        requestReceiver.stop();
        requestReceiver = null;
        unpairedDevices = null;
        pairedDevices = null;
        networkJoinEndTimeNanos = -1;
        startupSent.set(false);
    }

    public void sendStartupIfNecessary()
    {
        if (startupSent.compareAndSet(false, true))
        {
            new ZhalStartupEvent().send();
        }
    }

    public void sendNetworkHealthProblem()
    {
        new NetworkHealthProblemEvent().send();
    }

    public void sendPanIdAttack()
    {
        new ZigbeePanIdAttack().send();
    }

    public void sendNetworkHealthProblemRestored()
    {
        new NetworkHealthProblemRestoredEvent().send();
    }

    public <T> List<T> getDevicesByType(Class<T> deviceType)
    {
        List<T> devices = new ArrayList<>();
        for(MockZigbeeDevice d : getAllDevices())
        {
            if (deviceType.isInstance(d))
            {
                devices.add((T)d);
            }
        }

        return devices;
    }

    public <T> T getDeviceByNameAndType(String name, Class<T> deviceType)
    {
        T device = null;
        for(MockZigbeeDevice md : getAllDevices())
        {
            if (md.getName().equals(name) && deviceType.isInstance(md))
            {
                device = (T)md;
                break;
            }
        }

        return device;
    }

    public void addDevice(MockZigbeeDevice device)
    {
        if(!running)
        {
            throw new RuntimeException("invalid operation: not running");
        }

        if (networkJoinEndTimeNanos > 0)
        {
            pairDevice(device);
        }
        else
        {
            unpairedDevices.add(device);
        }

        System.out.println(device.getName() + " added");
    }

    public void addPairedDevice(MockZigbeeDevice device)
    {
        if(!running)
        {
            throw new RuntimeException("invalid operation: not running");
        }
        pairedDevices.put(device.getEui64(), device);
        device.setDiscovered(true);

        device.setDefaulted(false);

        device.joinComplete();
    }

    public void removeDevice(MockZigbeeDevice device)
    {
        if (pairedDevices.containsKey(device.getEui64()))
        {
            pairedDevices.remove(device.getEui64());
            unpairedDevices.add(device);
        }
    }

    private void pairDevice(MockZigbeeDevice device)
    {
        pairedDevices.put(device.getEui64(), device);
        device.setDiscovered(true);

        new DeviceJoinedEvent(device.getEui64()).send();
        new DeviceCommunicationSucceededEvent(device.getEui64()).send();
        new DeviceAnnouncedEvent(device.getEui64(), device.isRouter(), device.isMainsPowered()).send();

        device.setDefaulted(false);

        device.joinComplete();
    }

    public boolean isPaired(MockZigbeeDevice device)
    {
        return pairedDevices.containsKey(device.getEui64());
    }

    public void enableJoin(int durationSeconds)
    {
        if(!running)
        {
            throw new RuntimeException("invalid operation: not running");
        }

        if(durationSeconds > 0)
        {
            networkJoinEndTimeNanos = System.nanoTime() + TimeUnit.NANOSECONDS.convert(durationSeconds, TimeUnit.SECONDS);
            //starting discovery
            for(MockZigbeeDevice device : unpairedDevices)
            {
                pairDevice(device);
            }

            unpairedDevices.clear();
        }
        else
        {
            //stopping discovery
            networkJoinEndTimeNanos = -1;
        }
    }

    public boolean isNetworkOpenForJoin()
    {
        return networkJoinEndTimeNanos > 0 && networkJoinEndTimeNanos > System.nanoTime();
    }

    public MockZigbeeDevice getDevice(String eui64)
    {
        if(!running)
        {
            throw new RuntimeException("invalid operation: not running");
        }

        return pairedDevices.get(eui64);
    }

    public List<MockZigbeeDevice> getAllDevices()
    {
        if(!running)
        {
            throw new RuntimeException("invalid operation: not running");
        }

        List<MockZigbeeDevice> allDevices = new ArrayList<>(unpairedDevices);
        allDevices.addAll(pairedDevices.values());

        return allDevices;
    }

    public List<MockZigbeeDevice> getPairedDevices()
    {
        if(!running)
        {
            throw new RuntimeException("invalid operation: not running");
        }

        return new ArrayList<>(pairedDevices.values());
    }

    public List<MockZigbeeDevice> getUnpairedDevices()
    {
        if(!running)
        {
            throw new RuntimeException("invalid operation: not running");
        }

        return new ArrayList<>(unpairedDevices);
    }

    public EventWaitHelper getRefreshOtaFilesEventWaitHelper()
    {
        return refreshOtaFilesEventWaitHelper;
    }

    public void refreshOtaFiles()
    {
        refreshOtaFilesEventWaitHelper.notifyWaiters();
    }

    public void networkHealthCheckConfigure(int intervalMillis,
                                            int ccaThreshold,
                                            int ccaFailureThreshold,
                                            int restoreThreshold,
                                            int delayBetweenThresholdRetriesMillis)
    {
    }
}
