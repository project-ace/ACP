/*
 * procinfo.c
 *
 * - About this tool
 *  Procinfo is a sample tool for referencing static information of each process.
 *  The purpose of this tool is to manage such information in memory-efficient 
 *  way.
 *
 * - Usage
 *  To use these functions, include procinfo.h in the ACP code,
 *  and link the code with this file, procinfo.c.
 * 
 *  In the code, firstly, call the function procinfo_create to construct a handle 
 *  with type procinfo_t that represents a set of information contributed by each
 *  process. Then, information contributed by specific process is referenced by 
 *  the function proc_queryinfo. Another query function, procinfo_querysize, 
 *  queries for the size of the information on each process. The function 
 *  procinfo_free deconstructs the set of information.
 *
 * - Definitions of functions:
 *
 *  -- procinfo_t procinfo_create(void *data, int size)
 *    
 *    Construct a set of information distributed among processes and returns a 
 *    handle of the set with type procinfo_t. data and size are the address and 
 *    the size (byte) of the information that the process contributes to the set. 
 *    The value of size must be the same among all of the processes. This function
 *    is blocking and collective.
 *    
 *  -- int procinfo_queryinfo(procinfo_t pi, int rank, void *data)
 *    
 *    Query for the information on the rank in pi. The result is stored in the 
 *    region specified by data. This function is blocking and asynchronous.
 *    
 *  -- int procinfo_querysize(procinfo_t pi)
 *    
 *    Query for the size of the information on each process in pi. This function
 *    is blocking and asynchronous.
 *    
 *  -- int procinfo_free(procinfo_t pi)
 *    
 *    Free pi. This function is blocking and collective.
 *    
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>
#include "acp.h"
#include "procinfo.h"

#define PI_CACHESZ 2

int clear_startup(int size)
{
    int myid = acp_rank();
    char *addr = (char *)acp_query_address(acp_query_starter_ga(myid));

    memset(addr, 0, size);
}

static inline int pi_setup(procinfo_t *pi, int size)
{
    int sb, nb, np;

    np = acp_procs();

    sb = (int)sqrt(np); 
    if (sb * sb < np) sb++;

    nb = np / sb;
    if (sb * nb < np) nb++;

    pi->sb = sb; 
    pi->nb = nb;
    pi->size = size;
    pi->cachesz = PI_CACHESZ;
    pi->bodyga = ACP_GA_NULL;

}

static inline int pi_blockid(procinfo_t pi, int rank)
{
    return (rank / pi.sb);
}

static inline int pi_bmproc(procinfo_t pi, int blockid)
{
    return (pi.sb * blockid);
}

static inline int pi_blocksz(procinfo_t pi, int blockid)
{
    int np, sz;

    np = acp_procs();

    if (blockid == (pi.nb - 1))
        sz = np - (pi.nb - 1) * pi.sb;
    else 
        sz = pi.sb;

    return sz;
}

static inline int pi_index2rank(procinfo_t pi, int blockid, int index)
{
    return pi_bmproc(pi, blockid) + index;
}

static inline int pi_rank2index (procinfo_t pi, int blockid, int rank)
{
    if (pi_blockid(pi, rank) != blockid)
        return -1;
    else
        return (rank - pi_bmproc(pi, blockid));
}

static inline int pi_searchcache(procinfo_t pi, int blockid)
{
    int idx, i, myid;

    myid = acp_rank();
    idx = -1;

    for (i = 0; i < pi.cachesz; i++)
        if (pi.cacheblockid[i] == blockid) {
#ifdef DEBUG
            fprintf(stderr, "%d: pi_searchcache: found blockid %d at cache %d\n", myid, blockid, i);
#endif
            idx = i;
            break;
        }

    return idx;
}

static inline int pi_copyinfo(procinfo_t pi, void  *data, int blockid, int idx)
{
    int hitidx, hitctr, i, p, s, myid;

    myid = acp_rank();

    hitidx = pi_searchcache(pi, blockid);

    if (hitidx >= 0) { // hit
        /* update status of cache blocks */
        hitctr = pi.cachectr[hitidx];
        for (i = 0; i < pi.cachesz; i++) {
            if ((pi.cachectr[i] >= 0) && (pi.cachectr[i] < hitctr))
                pi.cachectr[i]++;
        }
        pi.cachectr[hitidx] = 0;
    } else { // miss
#ifdef DEBUG
        fprintf(stderr, "%d: pi_copyinfo: cache miss\n", myid);
#endif
        hitidx = -1;
        for (i = 0; i < pi.cachesz; i++){
            if ((pi.cachectr[i] == -1) // empty slot     
                || (pi.cachectr[i] == pi.cachesz-1)){ // Last Recently Used
                hitidx = i;
                break;
            }
        }
        if (hitidx < 0){
            fprintf(stderr, "ERROR: %d: pi_copycache : no cache available\n", myid);
        }

#ifdef DEBUG
        fprintf(stderr, "%d: pi_copyinfo: use cache %d\n", myid, hitidx);
#endif

        for (i = 0; i < pi.cachesz; i++) {
            if (pi.cachectr[i] >= 0) {
                pi.cachectr[i]++;
            }
        }

        pi.cachectr[hitidx] = 0;
        s = pi_blocksz(pi, blockid);
        pi.cacheblockid[hitidx] = blockid;

#ifdef DEBUG
        fprintf(stderr, "%d: pi_copyinfo: cache stat", myid);
        for (i = 0; i < pi.cachesz; i++)
            fprintf(stderr, " (%d, %d)", pi.cachectr[i], pi.cacheblockid[i]);
        fprintf(stderr, "\n");
#endif

        p = pi_bmproc(pi, blockid);
#ifdef DEBUG
        fprintf(stderr, "%d: pi_copyinfo: copy block %d from %d from %p to %p size %d\n", 
                myid, blockid, p, *(pi.body + blockid), pi.cachega + hitidx * pi.sb * pi.size,
            s * pi.size);
#endif
        acp_copy(pi.cachega + hitidx * pi.sb * pi.size, 
                 *(pi.body + blockid), s * pi.size, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
#ifdef DEBUG
        fprintf(stderr, "%d: pi_copyinfo: copy done\n", myid);
#endif
    }

    memcpy(data, pi.cachebody + hitidx * pi.sb * pi.size + idx * pi.size, 
           pi.size);
}

procinfo_t procinfo_create(void *data, int size)
{
    acp_ga_t *tmp, *body, *stla;
    acp_ga_t tmpga, stga, remotega;
    acp_handle_t h;
    procinfo_t pi;
    int s, myid, myblockid, b, loc, i, rank;
    int *flag;

    myid = acp_rank();

#ifdef DEBUG
    fprintf(stderr, "%d: procinfo_create: enter\n", myid);
#endif
    pi_setup(&pi, size);
#ifdef DEBUG
    fprintf(stderr, "%d: procinfo_create: pi_setup done: sb %d nb %d size %d cachesz %d\n", 
            myid, pi.sb, pi.nb, pi.size, pi.cachesz);
#endif
    myblockid = pi_blockid(pi, myid);
#ifdef DEBUG
    fprintf(stderr, "%d: procinfo_create: myblockid = %d\n", 
            myid, myblockid);
#endif

    /*
     * starter memory:
     *   - myown GA (8)
     *   - tmp  (8)
     *   - flag (4)
     */
    clear_startup(2 * sizeof(acp_ga_t) + sizeof(int));
    stga = acp_query_starter_ga(myid);
    stla = (acp_ga_t *)acp_query_address(stga);
    tmpga = stga + sizeof(acp_ga_t);
    tmp = (acp_ga_t *)((char *)stla + sizeof(acp_ga_t));
    flag = (int *)((char *)stla + 2 * sizeof(acp_ga_t));

    /*
     * procinfo body:
     *    - GAs of block masters (NumBlocks * 8)
     *    - Local Block (BlockSize * size)
     *    - Cache (PI_CACHESZ * BlockSize * size)
     */
    s = pi.nb*sizeof(acp_ga_t) + pi_blocksz(pi, myblockid) * size
        + pi.sb * pi.cachesz * size;
    pi.body = (acp_ga_t *)malloc(s);
    if (pi.body == NULL){
        fprintf(stderr, "%d: procinfo_create: ERROR: malloc failed for pi.body\n", myid);
        pi.bodyga = ACP_GA_NULL;
        return pi;
    }
    pi.bodykey = acp_register_memory(pi.body, s, 0);
    if (pi.bodykey == ACP_ATKEY_NULL){
        fprintf(stderr, "%d: procinfo_create: ERROR: register failed for pi.body\n", myid);
        pi.bodyga = ACP_GA_NULL;
        return pi;
    }
    pi.bodyga = acp_query_ga(pi.bodykey, pi.body);
    *stla = pi.bodyga;

    memcpy((char *)(pi.body) + pi.nb * sizeof(acp_ga_t), data, size);

    pi.cachebody = (void *)pi.body + pi.nb * sizeof(acp_ga_t) 
                    + pi_blocksz(pi, myblockid) * size;
    pi.cachega = pi.bodyga + pi.nb * sizeof(acp_ga_t) 
                    + pi_blocksz(pi, myblockid) * size;
    pi.cacheblockid = (int *)malloc(pi.cachesz * sizeof(int));
    pi.cachectr = (int *)malloc(pi.cachesz * sizeof(int));

#ifdef DEBUG
    fprintf(stderr, "%d: procinfo_create: bodyga = %p , cachega = %p\n", 
            myid, pi.bodyga, pi.cachega);
#endif

    for (i = 0; i < pi.cachesz; i++){
        pi.cachectr[i] = -1;
        pi.cacheblockid[i] = -1;
    }

    acp_sync();

    if (myid ==0) {
        for (b = 0; b < pi.nb; b++){
            if (b != myblockid){
                h = acp_copy(pi.bodyga + b * sizeof(acp_ga_t), 
                         acp_query_starter_ga(pi_bmproc(pi, b)), 
                         sizeof(acp_ga_t), ACP_HANDLE_NULL);
                if (h == ACP_HANDLE_NULL){
                    fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed b = %d\n", myid, b);
                    pi.bodyga = ACP_GA_NULL;
                    return pi;
                }
            } else {
                *(pi.body+b) = pi.bodyga;
            }
        }
        acp_complete(ACP_HANDLE_ALL);

        for (b = 0; b < pi.nb; b++)
            *(pi.body+b) += pi.nb * sizeof(acp_ga_t);

#ifdef DEBUG
        for (b = 0; b < pi.nb; b++){
            fprintf(stderr, "0: BMGA %d: %p\n", b, *(pi.body+b));
        }
#endif

        *flag = 1;
        for (b = 0; b < pi.nb; b++)
            if (b != myblockid)
                acp_copy(acp_query_starter_ga(pi_bmproc(pi, b)) + 2 * sizeof(acp_ga_t), 
                         stga + 2 * sizeof(acp_ga_t), sizeof(int), ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);

        if (myid == pi_bmproc(pi, myblockid)) {
            for (i = 1; i < pi_blocksz(pi, myblockid); i++) {
                rank = pi_index2rank(pi, myblockid, i);
                h = acp_copy(tmpga, acp_query_starter_ga(rank), 
                             sizeof(acp_ga_t), ACP_HANDLE_NULL);
                if (h == ACP_HANDLE_NULL){
                    fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed for tmpga: i = %d\n", myid, i);
                    pi.bodyga = ACP_GA_NULL;
                    return pi;
                }
                acp_complete(h);
                h = acp_copy(pi.bodyga + pi.nb * sizeof(acp_ga_t) + i * size, 
                         *tmp + pi.nb * sizeof(acp_ga_t), size, ACP_HANDLE_NULL);
                if (h == ACP_HANDLE_NULL){
                    fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed for bodyga: i = %d\n", myid, i);
                    pi.bodyga = ACP_GA_NULL;
                    return pi;
                }
            }
            acp_complete(ACP_HANDLE_ALL);
#ifdef DEBUG
        for (b = 0; b < pi.sb; b++){
            fprintf(stderr, "%d: LB %d: %d\n", myid, b, *((int *)((char *)(pi.body)+pi.nb*sizeof(acp_ga_t)+b*pi.size)));
        }
#endif
        }
        acp_sync();
    } else if (myid == pi_bmproc(pi, myblockid)) {
        while (*flag != 1) ;
        h = acp_copy(tmpga, acp_query_starter_ga(0), 
                 sizeof(acp_ga_t), ACP_HANDLE_NULL);
        if (h == ACP_HANDLE_NULL){
            fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed for tmpga\n", myid);
            pi.bodyga = ACP_GA_NULL;
            return pi;
        }
        acp_complete(h);

        h = acp_copy(pi.bodyga, *tmp, pi.nb*sizeof(acp_ga_t), ACP_HANDLE_NULL);
        if (h == ACP_HANDLE_NULL){
            fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed for bodyga\n", myid);
            pi.bodyga = ACP_GA_NULL;
            return pi;
        }

        acp_complete(ACP_HANDLE_ALL);
#ifdef DEBUG
        for (b = 0; b < pi.nb; b++){
            fprintf(stderr, "%d: BMGA %d: %p\n", myid, b, *(pi.body+b));
        }
#endif
        for (i = 1; i < pi_blocksz(pi, myblockid); i++) {
            rank = pi_index2rank(pi, myblockid, i);
            h = acp_copy(tmpga, acp_query_starter_ga(rank), 
                         sizeof(acp_ga_t), ACP_HANDLE_NULL);
            if (h == ACP_HANDLE_NULL){
                fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed for tmpga: i = %d\n", myid, i);
                pi.bodyga = ACP_GA_NULL;
                return pi;
            }
            acp_complete(h);

            h = acp_copy(pi.bodyga + pi.nb * sizeof(acp_ga_t) + i * size, 
                     *tmp + pi.nb * sizeof(acp_ga_t), size, ACP_HANDLE_NULL);
            if (h == ACP_HANDLE_NULL){
                fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed for bodyga: i = %d\n", myid, i);
                pi.bodyga = ACP_GA_NULL;
                return pi;
            }
        }
        acp_complete(ACP_HANDLE_ALL);
#ifdef DEBUG
        for (b = 0; b < pi.sb; b++){
            fprintf(stderr, "%d: LB %d: %d\n", myid, b, *((int *)((char *)(pi.body)+pi.nb*sizeof(acp_ga_t)+b*pi.size)));
        }
#endif
        acp_sync();
    } else {
        acp_sync();
        h = acp_copy(tmpga, acp_query_starter_ga(pi_bmproc(pi, myblockid)), 
                     sizeof(acp_ga_t), ACP_HANDLE_NULL);
        if (h == ACP_HANDLE_NULL){
            fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed for tmpga\n", myid);
            pi.bodyga = ACP_GA_NULL;
            return pi;
        }
        acp_complete(h);
        h = acp_copy(pi.bodyga, *tmp, 
                 pi.nb * sizeof(acp_ga_t) + pi.size * pi_blocksz(pi, myblockid), 
                 ACP_HANDLE_NULL);
        if (h == ACP_HANDLE_NULL){
            fprintf(stderr, "%d: procinfo_create: ERROR: acp_copy failed for bodyga\n", myid);
            pi.bodyga = ACP_GA_NULL;
            return pi;
        }

        acp_complete(ACP_HANDLE_ALL);
#ifdef DEBUG
        for (b = 0; b < pi.nb; b++){
            fprintf(stderr, "%d: BMGA %d: %p\n", myid, b, *(pi.body+b));
        }

        for (b = 0; b < pi.sb; b++){
            fprintf(stderr, "%d: LB %d: %d\n", myid, b, *((int *)((char *)(pi.body)+pi.nb*sizeof(acp_ga_t)+b*pi.size)));
        }
#endif
    }

    acp_sync();

    return pi;
}

int procinfo_free(procinfo_t pi)
{
    int myid = acp_rank();
#ifdef DEBUG
    fprintf(stderr, "%d: procinfo_free: enter\n", myid);
#endif
    acp_sync();

    acp_unregister_memory(pi.bodykey);
    free(pi.cacheblockid);
    free(pi.cachectr);
    free(pi.body);
#ifdef DEBUG
    fprintf(stderr, "%d: procinfo_free: finish\n", myid);
#endif

}

int procinfo_queryinfo(procinfo_t pi, int rank, void *data)
{
    int blockid, myid, idx;

    myid = acp_rank();
#ifdef DEBUG
    fprintf(stderr, "%d: procinfo_queryinfo: enter\n", myid);
#endif

    blockid = pi_blockid(pi, rank);
    idx = pi_rank2index(pi, blockid, rank);

    if (blockid == pi_blockid(pi,  myid)) {
        memcpy(data, (char *)(pi.body) + pi.nb * sizeof(acp_ga_t) + idx * pi.size, 
               pi.size);
    } else {
        pi_copyinfo(pi, data, blockid, idx);
    }

#ifdef DEBUG
    fprintf(stderr, "%d: procinfo_queryinfo: finish\n", myid);
#endif

}

int procinfo_querysize(procinfo_t pi)
{
    return pi.size;
}




