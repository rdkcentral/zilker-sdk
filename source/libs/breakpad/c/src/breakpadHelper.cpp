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
// Created by mkoch201 on 6/11/18.
//

#include <icBuildtime.h>
#ifdef CONFIG_DEBUG_BREAKPAD_WRAPPER
#include "breakpad_wrapper.h"
#else
#include <breakpad/client/linux/handler/exception_handler.h>
#include <signal.h>
#include <atomic>
static google_breakpad::ExceptionHandler* excHandler = NULL;
static std::atomic<int> refCount(0);

static void sigHandler(int signum, siginfo_t* sigInfo, void* context)
{
    if (excHandler != NULL)
    {
        excHandler->WriteMinidump();
    }
}

static void registerSignalHandler(int signum)
{
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_sigaction = &sigHandler;
    act.sa_flags = SA_SIGINFO;
    if (sigaction(signum, &act, NULL) < 0)
    {
        printf("Failed to register signal handler for %d\n", signum);
    }
}

static bool breakpadDumpCallback(const google_breakpad::MinidumpDescriptor& descriptor,
                                 void* context,
                                 bool succeeded)
{
    (void)context; // Unused
    /* Do the desired exit process here*/
    printf("Minidump success: %s, path: %s\n", succeeded ? "true" : "false", descriptor.path());
    return succeeded;
}
#endif

extern "C" void breakpadHelperSetup()
{
#ifdef CONFIG_DEBUG_BREAKPAD_WRAPPER
    breakpad_ExceptionHandler();
#else
    // Use refcount so we can support multi service in a single process is necessary
    if (refCount++ == 0)
    {
        excHandler = new google_breakpad::ExceptionHandler(google_breakpad::MinidumpDescriptor(CONFIG_DEBUG_BREAKPAD_DUMP_PATH), NULL, breakpadDumpCallback, NULL, true, -1);
        // Install signal handler so we can trigger on demand
        // This is kind of useless as it doesn't seem to be able to get a full backtrace of the stack(at least on Droid)
        registerSignalHandler(SIGUSR2);
    }
#endif

}

extern "C" void breakpadHelperCleanup()
{
    // breakpad wrapper doesn't have any cleanup
#ifndef CONFIG_DEBUG_BREAKPAD_WRAPPER
    if (--refCount == 0)
    {
        google_breakpad::ExceptionHandler* tmp = excHandler;
        excHandler = NULL;
        delete tmp;
    }
#endif
}

