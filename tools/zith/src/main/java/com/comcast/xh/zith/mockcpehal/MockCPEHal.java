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

package com.comcast.xh.zith.mockcpehal;

import com.icontrol.api.properties.PropSource;
import com.icontrol.api.properties.PropsHelper;

public class MockCPEHal
{
    public static final String TAMPER_PROPERTY_KEY = "devHal.tamper";
    public static final String BAD_BATTERY_PROPERTY_KEY = "devHal.BadBat";
    public static final String LOW_BATTERY_PROPERTY_KEY = "devHal.LowBat";
    public static final String MISSING_BATTERY_PROPERTY_KEY = "devHal.MissingBat";
    public static final String AC_POWER_REMOVED_PROPERTY_KEY = "devHal.AcPowerMissing";

    public static void setTamper(boolean tampered)
    {
        PropsHelper.setCpeProperty(TAMPER_PROPERTY_KEY,tampered?"true":"false", PropSource.PROPERTY_SRC_DEFAULT);
        System.out.println("CPE is now tampered = " + tampered);
    }

    public static void setBadBattery(boolean bad)
    {
        PropsHelper.setCpeProperty(BAD_BATTERY_PROPERTY_KEY,bad?"true":"false", PropSource.PROPERTY_SRC_DEFAULT);
        System.out.println("CPE is now Bad Battery = " + bad);
    }

    public static void setLowBattery(boolean low)
    {
        PropsHelper.setCpeProperty(LOW_BATTERY_PROPERTY_KEY,low?"true":"false", PropSource.PROPERTY_SRC_DEFAULT);
        System.out.println("CPE is now Low Battery = " + low);
    }

    public static void setMissingBattery(boolean missing)
    {
        PropsHelper.setCpeProperty(MISSING_BATTERY_PROPERTY_KEY,missing?"true":"false", PropSource.PROPERTY_SRC_DEFAULT);
        System.out.println("CPE is now missing battery = " + missing);
    }

    public static void setAcPowerRemoved(boolean removed)
    {
        PropsHelper.setCpeProperty(AC_POWER_REMOVED_PROPERTY_KEY, removed?"true":"false", PropSource.PROPERTY_SRC_DEFAULT);
        System.out.println("CPE is now AC power removed = " + removed);
    }

}
