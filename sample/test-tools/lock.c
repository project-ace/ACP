/*
 * lock.c
 *
 * - About this tool
 *  Lock is a sample tool for lock/unlock among processes.
 *  This tool is quite slow, but memory efficient.
 *
 * - Usage
 *  To use these functions, include lock.h in the ACP code,
 *  and link the code with this file, lock.c, along with procinfo.c.
 * 
 *  In the code, create a lock on each process by calling lock_create. 
 *  The lock on a specific process can be done aquired and relieased
 *  by functions lock_aquire and lock_release. The locks can be freed 
 *  by lock_free.
 *
 * - Definitions of functions:
 *
 *  -- lock_t lock_create()
 *    
 *    Create a set of locks that are located one on each process, and 
 *    returns a handle that represents the set. This is blocking and
 *    collective.
 *    
 *  -- int lock_acquire(lock_t lock, int rank)
 *
 *    Acquires the lock of the rank in the set represented by lock.
 *    This is blocking. So, it waits until the lock is acquired.
 *
 *  -- lock_release(lock_t lock, int rank)
 *
 *    Releases the lock of the rank in the set represented by lock.
 *    This is blocking and local.
 *
 *  -- lock_free(lock_t lock)
 *
 *    Frees the set of locks represented by lock.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"
#include "procinfo.h"
#include "lock.h"

lock_t lock_create()
{
    int myrank, procs;
    lock_t lock;
    int *lockbody;
    procinfo_t lockinfo;
    acp_ga_t ga;

    myrank = acp_rank();
    procs = acp_procs();

    lock.lockbody = (int *)malloc(sizeof(int)*2);
    lock.lockbody[0] = 0;
    lock.lockbody[1] = 0;
    lock.atkey = acp_register_memory(lock.lockbody, sizeof(int)*2, 0);
    ga = acp_query_ga(lock.atkey, lock.lockbody);

    lock.info = procinfo_create(&(ga), sizeof(acp_ga_t));

    return lock;
}

int lock_free(lock_t lock)
{
    procinfo_free(lock.info);
    acp_unregister_memory(lock.atkey);
    free(lock.lockbody);

    return 0;
}

int lock_acquire(lock_t lock, int rank)
{
    acp_ga_t ga, tmpga;
    volatile int *val;

    procinfo_queryinfo(lock.info, rank, &ga);
    tmpga = acp_query_ga(lock.atkey, lock.lockbody) + sizeof(int);
    val = acp_query_address(tmpga);

    do {
        acp_cas4(tmpga, ga, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (*val != 0);

    return 0;
    
}

int lock_release(lock_t lock, int rank)
{
    acp_ga_t ga, tmpga;
    volatile int *val;

    procinfo_queryinfo(lock.info, rank, &ga);
    tmpga = acp_query_ga(lock.atkey, lock.lockbody) + sizeof(int);
    val = acp_query_address(tmpga);

    acp_cas4(tmpga, ga, 1, 0, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);

    return 0;
}

