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

//
// Created by wboyd747 on 6/11/18.
//

#ifndef ZILKER_AUTOMATIONCREW_H
#define ZILKER_AUTOMATIONCREW_H

#include <stdbool.h>
#include <stdint.h>
#include <cjson/cJSON.h>

typedef struct _automationEngineOps {
    const char* name;

    void (*destroy)(void);

    bool (*enable)(const char* id, const char* specification);
    void (*disable)(const char* id);

    cJSON* (*get_state)(const char* id);

    cJSON* (*process)(const cJSON* message, cJSON* stats);
} automationEngineOps;

/**
 * Initialize the Engine factory and any internally known
 * engines.
 */
void automationEngineInit(void);

/**
 * Destroy the Engine factory and all registered engines.
 */
void automationEngineDestroy(void);

/**
 * Register a new automation engine with the factory.
 *
 * Once registered the engine will have new specifications
 * and messaged passed to it. All engines *should* be registered
 * before starting the message handling sub-system.
 *
 * @param ops The new engine abstracted operations.
 */
void automationEngineRegister(automationEngineOps* ops);

/**
 * Start the Engine factory message handling sub-system.
 */
void automationEngineStart(void);

/**
 * Stop the Engine factory message handling sub-system.
 */
void automationEngineStop(void);

/**
 * Enable, or update, a specification with an Engine.
 *
 * Once enabled the specification will be processed whenever
 * a new message is posted to the factory.
 *
 * @param specId The ID of the specification.
 * @return True if the specification was successfully enabled/updated.
 */
bool automationEngineEnable(const char* specId, const char* specification);

/**
 * Disable a specification owned by an Engine.
 *
 * @param specId The ID of the specification.
 */
void automationEngineDisable(const char* specId);

/**
 * Get the current state of a machine within the crew.
 *
 * @param id The ID of the machine.
 * @return A JSON object representing the current machine state.
 */
cJSON* automationEngineGetState(const char* specId);

/**
 * Post a new message to the Engine factory.
 *
 * @param message The message to pass to the factory.
 * @return True if the message was successfully posted into an
 * engine.
 */
bool automationEnginePost(const cJSON* message);

#endif //ZILKER_AUTOMATIONCREW_H
