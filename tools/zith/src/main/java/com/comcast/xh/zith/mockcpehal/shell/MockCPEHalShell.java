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

package com.comcast.xh.zith.mockcpehal.shell;

import com.comcast.xh.zith.mockcpehal.MockCPEHal;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.shell.standard.ShellCommandGroup;
import org.springframework.shell.standard.ShellComponent;
import org.springframework.shell.standard.ShellMethod;
import org.springframework.shell.standard.ShellOption;

@ShellComponent
@ShellCommandGroup("Mock CPE HAL")
public class MockCPEHalShell
{
    @ShellMethod(value="Tamper the CPE", key="cpe-tamper")
    public void cpeTamper()
    {
        MockCPEHal.setTamper(true);
    }

    @ShellMethod(value="Un-Tamper the CPE",key="cpe-tamper-restore")
    public void cpeTamperRestore()
    {
        MockCPEHal.setTamper(false);
    }

    @ShellMethod(value="Set the battery bad on the CPE", key="cpe-bad-battery")
    public void cpeBatteryBad()
    {
        MockCPEHal.setBadBattery(true);
    }

    @ShellMethod(value="Unset the battery bad on the CPE", key="cpe-bad-battery-restore")
    public void cpeBatteryBadRestore()
    {
        MockCPEHal.setBadBattery(false);
    }

    @ShellMethod(value="Set the battery low on the CPE", key = "cpe-low-battery")
    public void cpeBatteryLow()
    {
        MockCPEHal.setLowBattery(true);
    }

    @ShellMethod(value="Unset the battery low on the CPE", key="cpe-low-battery-restore")
    public void cpeBatteryLowRestore()
    {
        MockCPEHal.setLowBattery(false);
    }

    @ShellMethod(value="Set the AC Power removed on the CPE",key="cpe-ac-power-removed")
    public void cpeAcPowerMissing()
    {
        MockCPEHal.setAcPowerRemoved(true);
    }

    @ShellMethod(value="Unset the Ac Power removed on the CPE", key="cpe-ac-power-restored")
    public void cpeAcPowerRestored()
    {
        MockCPEHal.setAcPowerRemoved(false);
    }
}
