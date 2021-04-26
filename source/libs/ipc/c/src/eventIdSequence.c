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

/*-----------------------------------------------
 * eventIdSequence.h
 *
 * Support for a 'shared memory' segment that is used
 * to serve as a 'unique eventId sequencer'.  Each process
 * that needs an eventId would use this code to:
 *  - read the current eventId value
 *  - increment by 1
 *  - write the new value
 *  - use the new value
 *
 * Created because we need a way to create a unique
 * identifier to prevent duplicate events from the same CPE.
 * Due to the fact our server expects these identifiers
 * to be sequential, we cannot simply grab a random number.
 *
 * The more complicated part is that we have several services
 * running as independent processes, and need to ensure they
 * don't have to synchronize with one-another OR create
 * duplicate identifiers.
 *
 * In the past, we resolved this by adding a kernel-mod,
 * however, that approach is not portable and cannot
 * be used anymore.
 *
 * Because this is uses a 'shared memory segment', it also
 * creates and uses a System V semaphore to deal with the
 * concurrency of multiple processes trying to obtain and
 * update the global eventId
 *
 * In following with System V conventions on semaphors and
 * shared-mem, something needs to create the constructs and
 * eventually delete them.
 *
 * Watchdog's procMgr MUST initialize the semaphore before starting any more processes
 *
 * Author: jelderton - 7/10/15
 *-----------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/stat.h>
#ifndef USE_CLOCK_FOR_EVENT_ID
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/fcntl.h>
#endif

#include <icIpc/eventIdSequence.h>
#include <icLog/logging.h>
#include <icConcurrent/timedWait.h>
#include "ipcCommon.h"

#ifdef USE_CLOCK_FOR_EVENT_ID
#ifdef NO_MONOTONIC
//This is really for Mac OSX (dev machines)
#include <sys/time.h>
#else
#include <time.h>
#endif

uint64_t getNextEventId()
{
    uint64_t now;

#ifdef NO_MONOTONIC
    struct timeval tv;

    gettimeofday(&tv, NULL);
    now = (uint64_t)((tv.tv_sec * 1.0e6) + tv.tv_usec);
#else
  struct timespec tp;

  clock_gettime(CLOCK_MONOTONIC, &tp);
  now = (uint64_t)((tp.tv_sec * 1.0e6) + (tp.tv_nsec / 1000));
#endif

    return now;
}

#else

#define SEMAPHORE_FILE      "/tmp/.eventSem"
#define SEMAPHORE_PERMS     0666
#define SEM_MAX_TRIES       2

#define SHARED_MEM_FILE     "/tmp/.eventId"     // used for the ftok() calls
#define SHARED_MEM_SIZE     sizeof(unsigned long long)
#define SHARED_MEM_PERMS    0660

/*
 * define the semun structure since not all compilers have it defined
 */
#ifdef _SEM_SEMUN_UNDEFINED
union semun {
	int val;                    // value for SETVAL
	struct semid_ds *buf;       // buffer for IPC_STAT & IPC_SET
	ushort *array;              // array for GETALL & SETALL
	struct seminfo *__buf;      // buffer for IPC_INFO
};
#endif

/*
 * private variables
 */
static int semId = -1;          // semaphore id
static key_t semKey;            // semaphore key
static struct sembuf semBuffer; // semaphore

static int memId = -1;          // shared memory id
static key_t memKey;            // shared memory key

static pthread_cond_t LOCAL_COND = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t LOCAL_MTX = PTHREAD_MUTEX_INITIALIZER;     // local mutex for initialization

/*
 * private functions
 */
//static bool createMarker(const char *fileName, mode_t perms);
static bool setupInternals();
static bool lock();
static bool unlock();

/*
 * Return the next eventId available for use.
 * this value is global to the entire CPE and
 * is guaranteed to be unique.  Because this
 * has to work across multiple processes, it
 * can take some time to retrieve
 *
 * Will return 0 if unable to obtain an eventId, and errno will be set appropriately
 */
uint64_t getNextEventId()
{
    // seems odd, but lock the mutex to prevent
    // multiple threads from hitting this at the
    // same time.  the semaphore is a global lock,
    // but still need a local lock
    //
    uint64_t retVal = 0;
    pthread_mutex_lock(&LOCAL_MTX);

    // setup the constructs (if needed)
    if (setupInternals() == false)
    {
        pthread_mutex_unlock(&LOCAL_MTX);
        return retVal;
    }

    // lock semaphore.  this can block for a while
    //
#ifdef DEBUG_IPC_DETAILED
    icLogDebug(API_LOG_CAT, "waiting for semaphore lock...");
#endif
    if (lock() == false)
    {
        pthread_mutex_unlock(&LOCAL_MTX);
        return retVal;
    }
#ifdef DEBUG_IPC_DETAILED
    icLogDebug(API_LOG_CAT, "got semaphore lock");
#endif

    // connect to shared mem, and get a pointer to it
    // note that if it fails we get a -1, which doesn't
    // play well with "unsigned long long", so use temp ptr
    //
    void *data = shmat(memId, (void *)0, 0);
    if ((int *)data != (int *)-1)
    {
        // typecast as uint64_t
        //
        uint64_t *tmp = (uint64_t *)data;
#ifdef DEBUG_IPC_DETAILED
        icLogDebug(API_LOG_CAT, "got shared memory, current eventId = %" PRIu64, *tmp);
#endif

        // read current value and increment by 1
        //
        retVal = *tmp + 1;

        // save new value back into the shared area
        //
        *tmp = retVal;

        // disconnect from shared mem
        //
        if (shmdt(data) == -1)
        {
            icLogWarn(API_LOG_CAT, "unable to disconnect from eventId shared-memory : %d - %s", errno, strerror(errno));
            retVal = 0;
        }
    }
    else
    {
        // unable to attach to the shared mem
        //
        icLogWarn(API_LOG_CAT, "unable to attatch to eventId shared-memory : %d - %s", errno, strerror(errno));
    }

    // unlock semaphore
    //
    unlock();
#ifdef DEBUG_IPC_DETAILED
    icLogDebug(API_LOG_CAT, "released semaphore lock");
#endif

    // release local lock
    //
    pthread_mutex_unlock(&LOCAL_MTX);

    // return the unique eventId that can be used
    //
    return retVal;
}

/*
 * create the tiny marker file to use for our
 * semaphore or shared memory key.  this is
 * what is used by ftok()
 */
static bool createMarker(const char *fileName, mode_t perms)
{
    // see if the file is already there
    //
    struct stat fileInfo;
    if (stat(fileName, &fileInfo) == 0)
    {
        // nothing to do here
        //
        return true;
    }

    // just open & truncate the file
    //
    int tmp = open(fileName, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (tmp < 0)
    {
        icLogError(API_LOG_CAT, "unable to create marker file %s : %d - %s", fileName, errno, strerror(errno));
        return false;
    }

    // write a couple of random numbers
    //
    int i;
    for (i = 0 ; i < 4 ; i++)
    {
        long num = random();
        write(tmp, &num, sizeof(long));
    }

    // set permissions
    //
    fchmod(tmp, perms);

    // close up the file
    //
    close(tmp);
    return true;
}

/*
 * setup the shared-memory key & id
 */
static bool establishSharedMem()
{
#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "initializing eventId shared-memory...");
#endif

    // ensure we have the shared-memory marker file
    //
    createMarker(SHARED_MEM_FILE, SHARED_MEM_PERMS);

    // first create the key
    //
    if ((memKey = ftok(SHARED_MEM_FILE, 'J')) == -1)
    {
        // maybe the file isn't there?
        //
#ifdef CONFIG_DEBUG_IPC
        icLogWarn(API_LOG_CAT, "unable to get key for shared-memory");
#endif
        return false;
    }

    // connect to the segment.  ideally this was created by 'createSharedSegment'
    // so we won't add the IPC_CREAT flag
    //
    if ((memId = shmget(memKey, SHARED_MEM_SIZE, IPC_CREAT | SHARED_MEM_PERMS)) == -1)
    {
#ifdef CONFIG_DEBUG_IPC
        icLogWarn(API_LOG_CAT, "failed to obtain eventId shared-memory id");
#endif
        return false;
    }

#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "done initializing eventId shared-memory");
#endif
    return true;
}

/*
 * create the semaphore for the process.  if the global one
 * is not established, this will create and initialize it.
 * otherwise, it will obtain the key and id so it can be utilized.
 *
 * inspired by W. Richard Stevens' UNIX Network
 * Programming 2nd edition, volume 2, lockvsem.c, page 295.
 */
static bool establishSemaphore()
{
#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "initializing eventId semaphore...");
#endif

    // ensure we have the semaphore marker file
    //
    createMarker(SEMAPHORE_FILE, SEMAPHORE_PERMS);

    // first create the key
    //
    if ((semKey = ftok(SEMAPHORE_FILE, 'k')) == -1)
    {
        // maybe the file isn't there?
        //
#ifdef CONFIG_DEBUG_IPC
        icLogWarn(API_LOG_CAT, "unable to get key for semaphore");
#endif
        return false;
    }

    union semun arg;

    // get the semaphore identifier
    //
    semId = semget(semKey, 1, IPC_CREAT | IPC_EXCL | SEMAPHORE_PERMS);
    if (semId >= 0)
    {
#ifdef CONFIG_DEBUG_IPC
        icLogDebug(API_LOG_CAT, "creating the semaphore for the first time");
#endif

        // first time this is being initialized
        //
        arg.val = 1;

        // Initialize semval and reinitialize semadj for all processes (POSIX.1-2001)
        if (semctl(semId, 0, SETVAL, arg) == -1)
        {
#ifdef CONFIG_DEBUG_IPC
                        icLogWarn(API_LOG_CAT, "error during semaphore initialization: %d", errno);
#endif
            /*
             * SETVAL failed because:
             * EACCESS: The calling process does not have write permission on the semaphore set or does not have
             *          CAP_IPC_OWNER in the IPC namespace's governing user namespace
             * EIDRM: Semaphore set was removed
             * ERANGE: semval > SEMVMX
             * Try to destroy the set anyway; the operation should be tried again
             */
            semctl(semId, 0, IPC_RMID);
            errno = EAGAIN;
            return false;
        }
    }
    else if (errno == EEXIST)
    {
        // another process established already
        //

#ifdef CONFIG_DEBUG_IPC
        icLogDebug(API_LOG_CAT, "semaphore already exists, getting the id to it");
#endif

        // get the id
        //
        if ((semId = semget(semKey, 1, 0)) < 0)
        {
            // check errno for reason
            //
#ifdef CONFIG_DEBUG_IPC
            icLogWarn(API_LOG_CAT, "error getting existing semaphore id");
#endif
            return false;
        }
    }
    else
    {
#ifdef CONFIG_DEBUG_IPC
        icLogWarn(API_LOG_CAT, "failed to obtain eventId semaphore id");
#endif
        return false;
    }

#ifdef CONFIG_DEBUG_IPC
    icLogDebug(API_LOG_CAT, "done initializing eventId semaphore");
#endif
    return true;
}

/*
 * internal function, assumes caller has LOCAL_MTX locked
 */
static bool setupInternals()
{
    // see if we need to setup the semaphore
    //
    if (semId < 0)
    {
        if (establishSemaphore() == false)
        {
            icLogError(API_LOG_CAT, "unable to establish eventId semaphore : %d - %s", errno, strerror(errno));
            return false;
        }
    }

    // setup the shared mem
    //
    if (memId < 0)
    {
        if (establishSharedMem() == false)
        {
            icLogError(API_LOG_CAT, "unable to establish eventId shared-memory : %d - %s", errno, strerror(errno));
            return false;
        }
    }

    return true;
}

/*
 * wait for a lock on the semaphore
 * internal function, assumes caller has LOCAL_MTX locked
 */
static bool lock()
{
    // assign value to our buffer
    //
    semBuffer.sem_num = 0;
    semBuffer.sem_op = -1;  // lock
    semBuffer.sem_flg = SEM_UNDO;

    // ask for the semaphore lock
    //
    if (semop(semId, &semBuffer, 1) == -1)
    {
        icLogError(API_LOG_CAT, "unable to lock eventId semaphore : %d - %s", errno, strerror(errno));
        return false;
    }
    return true;
}

/*
 * release the lock on the semaphore
 * internal function, assumes caller has LOCAL_MTX locked
 */
static bool unlock()
{
    // assign value to our buffer
    //
    semBuffer.sem_num = 0;
    semBuffer.sem_op = 1;  // release
    semBuffer.sem_flg = SEM_UNDO;

    // ask the semaphore to release
    //
    if (semop(semId, &semBuffer, 1) == -1)
    {
        icLogError(API_LOG_CAT, "unable to unlock eventId semaphore : %d - %s", errno, strerror(errno));
        return false;
    }
    return true;
}

#endif


