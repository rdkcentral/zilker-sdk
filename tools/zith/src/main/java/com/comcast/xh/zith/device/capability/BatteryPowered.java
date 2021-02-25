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

package com.comcast.xh.zith.device.capability;

public interface BatteryPowered
{
    /**
     * Cause a device to be in low battery state
     * @param isLowBattery
     */
    void setLowBattery(boolean isLowBattery);

    /**
     * Get the current low battery state
     * @return
     */
    boolean isLowBattery();

    /**
     * set the battery voltage
     * @param newVoltage
     */
    void setBatteryVoltage(short newVoltage);

    /**
     * Get the current battery voltage
     * @return
     */
    short getBatteryVoltage();
}
