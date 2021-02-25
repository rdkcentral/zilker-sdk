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

/**
 * Interface that describes a tamperable cluster (e.g., device case opened)
 */
public interface Tamperable
{
    /**
     * Query tamper switch state
     * @return true when the device tamper switch is in the 'unsafe' position
     */
    public boolean isTampered();

    /**
     * Set tamper switch state
     * @param isTampered
     */
    public void setTampered(boolean isTampered);
}
