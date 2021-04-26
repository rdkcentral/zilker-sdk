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

#include <icBuildtime.h>
#include <icLog/logging.h>
#include <signal.h>

static void resetSignals(void)
{
#ifndef CONFIG_DEBUG_SINGLE_PROCESS
    /*
     * Set up signals for our services.
     * All our services (including watchdog) should:
     *
     * - Block nothing. All signals should be ignored or delivered.
     *
     * Ignore:
     *  - PIPE (don't kill the process just because of a socket error)
     *
     * Default:
     *  - QUIT (bash will mask or ignore it and it is used for tripping a core dump)
     *  - TERM (just die)
     *  - INT (just die)
     *
     * Note that STOP and KILL can NOT be trapped on POSIX systems.
     */

    struct sigaction sa;
    sa.sa_flags = 0;

    /* Don't block anything in signal handlers */
    sigemptyset(&sa.sa_mask);

    /*
     * Unblock (prevent queueing) all signals: invoking process may have
     * blocked signals we want such as SIGQUIT.
     */
    sigset_t sigmask;
    sigemptyset(&sigmask);
    pthread_sigmask(SIG_SETMASK, &sigmask, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    /* Make sure this process can make cores with SIGQUIT and dies sanely */
    sa.sa_handler = SIG_DFL;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);
#endif /* CONFIG_DEBUG_SINGLE_PROCESS */
}

/**
 * This is called just before main()
 */
__attribute__ ((constructor)) static void mainInit(void)
{
    resetSignals();
    //TODO: Make icLogger self initializing
    initIcLogger();
    atexit(closeIcLogger);
    icLogDebug("main", "mainInit");
}

/**
 * This is called just before the program ends (after main() or at exit())
 */
__attribute__ ((destructor)) void mainCleanup(void)
{
    /* Nothing to do */
}
