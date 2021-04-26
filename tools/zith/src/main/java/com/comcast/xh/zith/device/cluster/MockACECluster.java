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

package com.comcast.xh.zith.device.cluster;

import com.comcast.xh.zith.mockzigbeecore.event.ACE.client.ArmCommandEvent;
import com.comcast.xh.zith.mockzigbeecore.event.ClusterCommandEvent;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class MockACECluster extends MockCluster
{
    private static Logger log = LoggerFactory.getLogger(MockACECluster.class);

    public static final int CLUSTER_ID  = 0x501;
    public static final byte ZONE_ID    = 0x02;

    private ArmMode armMode;
    private byte panelStatus;
    private byte alarmStatus;

    /* Zigbee enumerates mute (0), default(1), and 0x80-0xff as mfr specific */
    private boolean audible;

    public MockACECluster(String eui64, int endpointId)
    {
        super("ace", eui64, endpointId, CLUSTER_ID, false);

        // No Attributes
    }

    /**
     * Simulate button pushes on the ACE device to request an arm/disarm
     * @param accessCode
     */
    public void requestPanelStateChange(String accessCode, ArmMode armMode)
    {
        new ArmCommandEvent(getEui64(),
                            getEndpointId(),
                            armMode,
                            accessCode,
                            ZONE_ID).send();
    }

    public void requestPanelStatus()
    {
        new ClusterCommandEvent(getEui64(), getEndpointId(), CLUSTER_ID, false, ClientCommand.GET_PANEL_STATUS).send();
    }

    private void parsePanelStatus(byte[] payload, String whatCommand)
    {
        if (payload.length == 4)
        {
            panelStatus = payload[0];
            audible = payload[2] != 0;
            alarmStatus = payload[3];

            if(panelStatus < PanelStatus.DISARMED || panelStatus > PanelStatus.ARMING_AWAY)
            {
                log.error("Panel status {} not between {} and {}", panelStatus, PanelStatus.DISARMED, PanelStatus.ARMING_AWAY);
                panelStatus = PanelStatus.DISARMED;
            }

            if(alarmStatus < AlarmStatus.NO_ALARM || alarmStatus > AlarmStatus.EMERGENCY_PANIC)
            {
                log.error("Alarm status {} not between {} and {}", alarmStatus, AlarmStatus.NO_ALARM, AlarmStatus.EMERGENCY_PANIC);
                alarmStatus = AlarmStatus.NO_ALARM;
            }

            log.info(whatCommand + ": {}, seconds left: {}, audible: {}, alarm: {}", panelStatus, payload[1], payload[2], alarmStatus);
        }
        else
        {
            log.error("Invalid Panel Status payload");
        }
    }

    @Override
    public void handleCommand(int commandId, byte[] payload)
    {
        switch(commandId)
        {
            case ServerCommand.ARM_RESP:
                byte armMode = payload[0];
                if (armMode >= ArmMode.DISARM.toByte() && armMode <= ArmMode.ARM_ALL.toByte())
                {
                    this.armMode = ArmMode.valueOf(armMode);
                }
                else
                {
                    log.error("Arm mode {} is not between {} and {}", armMode, ArmMode.DISARM, ArmMode.ARM_ALL);
                }
                break;
            case ServerCommand.GET_PANEL_STATUS_RESP:
                parsePanelStatus(payload, "Get Panel Status Response");
                break;
            case ServerCommand.PANEL_STATUS_CHANGED:
                parsePanelStatus(payload, "Panel Status Changed");
                break;
            default:
                super.handleCommand(commandId, payload);
        }
    }

    public ArmMode getArmMode()
    {
        return armMode;
    }

    public byte getPanelStatus()
    {
        return panelStatus;
    }

    public byte getAlarmStatus()
    {
        return alarmStatus;
    }

    public boolean isAudible()
    {
        return audible;
    }

    public enum ArmMode
    {
        DISARM      (0x00),
        ARM_DAY     (0x01),
        ARM_NIGHT   (0x02),
        ARM_ALL     (0x03);

        private byte mode;
        ArmMode(int mode)
        {
            this.mode = (byte) mode;
        }

        public static ArmMode valueOf(Byte value)
        {
            ArmMode armMode = null;
            for (ArmMode mode : ArmMode.values())
            {
                if (mode.toByte() == value)
                {
                    armMode = mode;
                    break;
                }
            }
            return armMode;
        }

        public byte toByte()
        {
            return this.mode;
        }
    }

    public static final class AlarmStatus
    {
        public static final byte NO_ALARM       = 0x00;
        public static final byte BURGLAR        = 0x01;
        public static final byte FIRE           = 0x02;
        public static final byte EMERGENCY      = 0x03;
        public static final byte POLICE_PANIC   = 0x04;
        public static final byte FIRE_PANIC     = 0x05;
        public static final byte EMERGENCY_PANIC= 0x06;
    }

    public static final class PanelStatus
    {
        public static final byte DISARMED       = 0x00;
        public static final byte ARMED_STAY     = 0x01;
        public static final byte ARMED_NIGHT    = 0x02;
        public static final byte ARMED_AWAY     = 0x03;
        public static final byte EXIT_DELAY     = 0x04;
        public static final byte ENTRY_DELAY    = 0x05;
        public static final byte NOT_READY      = 0x06;
        public static final byte IN_ALARM       = 0x07;
        public static final byte ARMING_STAY    = 0x08;
        public static final byte ARMING_NIGHT   = 0x09;
        public static final byte ARMING_AWAY    = 0x0a;
    }

    public static final class ArmNotification
    {
        static final byte ALL_DISARMED      = 0x00;
        static final byte DAY_ARMED         = 0x01;
        static final byte NIGHT_ARMED       = 0x02;
        static final byte ALL_ARMED         = 0x03;
        static final byte INVLAID_CODE      = 0x04;
        static final byte NOT_READY         = 0x05;
        static final byte ALREADY_DISARMED  = 0x06;
    }

    private static final class ClientCommand
    {
        static final byte ARM              = 0x00;
        static final byte BYPASS           = 0x01;
        static final byte EMERGENCY        = 0x02;
        static final byte FIRE             = 0x03;
        static final byte PANIC            = 0x04;
        static final byte GET_ZONE_ID_MAP  = 0x05;
        static final byte GET_ZONE_INFO    = 0x06;
        static final byte GET_PANEL_STATUS = 0x07;
        static final byte GET_BYPASS_LIST  = 0x08;
        static final byte GET_ZONE_STATUS  = 0x09;
    }

    private static final class ServerCommand
    {
        static final byte ARM_RESP              = 0x00;
        static final byte GET_ZONE_ID_MAP_RESP  = 0x01;
        static final byte GET_ZONE_INFO_RESP    = 0x02;
        static final byte PANEL_STATUS_CHANGED  = 0x04;
        static final byte GET_PANEL_STATUS_RESP = 0x05;
        static final byte SET_BYPASS_LIST       = 0x06;
        static final byte BYPASS_RESP           = 0x07;
        static final byte GET_ZONE_STATUS_RESP  = 0x08;
    }
}
