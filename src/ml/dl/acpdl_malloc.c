/*
 * ACP Middle Layer: Data Library malloc
 * 
 * Copyright (c) 2014-2014 FUJITSU LIMITED
 * Copyright (c) 2014      Kyushu University
 * Copyright (c) 2014      Institute of Systems, Information Technologies
 *                         and Nanotechnologies 2014
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <acp.h>
#include "acpbl.h"
#include "acpdl.h"

/*  Algorithm:
 *      K&R malloc with global lock
 *  Map of the DL starter memory:
 *      [0:31] temporal variables
 *      [32:39] local_lock
 *      [40:48] global_lock
 *      [48:55] head_ga
 *      [56:End] memory blocks
 *          free block (cyclic list)
 *              [0:7] size
 *              [8:15] ga of next free block
 *              [16:size-1] free memory
 *              max. allocatable size = size - 24
 *          allocated block (independent)
 *              [0:7] size
 *              [8:size-1] user memory
 *              user memory size = size - 8
 */

#define LLOCK 32
#define GLOCK 40
#define HEAD  48
#define TOP   56

void iacpdl_init_malloc(void)
{
    acp_ga_t local_heap;
    volatile uint64_t* tmp;
    
    local_heap = iacp_query_starter_ga_dl(acp_rank());
    tmp = (volatile uint64_t*)acp_query_address(local_heap);
    
    /* initialize locks */
    tmp[LLOCK >> 3] = 0;
    tmp[GLOCK >> 3] = 0;
    
    /* set list head */
    tmp[HEAD >> 3] = local_heap + TOP;
    
    /* set free block */
    tmp[TOP >> 3] = (iacp_starter_memory_size_dl & ~7ULL) - TOP;
    tmp[(TOP >> 3) + 1] = local_heap + TOP;
    
    return;
}

void iacpdl_finalize_malloc(void)
{
    return;
}

acp_ga_t acp_malloc(size_t size, int rank)
{
    acp_ga_t global_heap, local_heap, head_ga, ptr, prev_ptr;
    volatile uint64_t* tmp;
    
    global_heap = iacp_query_starter_ga_dl(rank);
    local_heap = iacp_query_starter_ga_dl(acp_rank());
    tmp = (volatile uint64_t*)acp_query_address(local_heap);
    
    /* size ajustment to minimum 24B and alignment to 8B boundary */
    size = (size < 24) ? 24 : ((size + 7ULL) & ~7ULL);
    
    /* acquire locks */
    do {
        acp_cas8(local_heap, local_heap + LLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (tmp[0] != 0);
    
    do {
        acp_cas8(local_heap, global_heap + GLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (tmp[0] != 0);
    
    /* acquire list head */
    acp_copy(local_heap, global_heap + HEAD, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    /* return if there is no free block */
    if (tmp[0] == ACP_GA_NULL) {
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return ACP_GA_NULL;
    }
    
    /* search for a sufficient free block */
    head_ga = ptr = tmp[0];
    prev_ptr = global_heap + HEAD - 8;
    acp_copy(local_heap, ptr, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    while (tmp[0] < size + 8 && tmp[1] != head_ga) {
        prev_ptr = ptr;
        ptr = tmp[1];
        acp_copy(local_heap, ptr, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    /* return if there is no sufficient free block */
    if (tmp[0] < size + 8) {
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return ACP_GA_NULL;
    }
    
    /* allocate a memory block */
    if (tmp[0] > size + 24) {
        /* the free block is going to be shrinked */
        if (ptr == head_ga && tmp[1] == head_ga) {
            /* the free block is the only free block in the list */
            tmp[0] -= 8 + size;
            tmp[1] = ptr + 8 + size;
            tmp[2] = 8 + size;
            tmp[3] = ptr + 8 + size;
            acp_copy(ptr + 8 + size, local_heap, 16, ACP_HANDLE_NULL);
            acp_copy(ptr, local_heap + 16, 8, ACP_HANDLE_NULL);
            acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
        } else {
            acp_copy(local_heap + 16, tmp[1] + 8, 8, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            if (ptr == tmp[2]) {
                /* there are two free blocks is in the list */
                tmp[0] -= 8 + size;
                tmp[2] = 8 + size;
                tmp[3] = ptr + 8 + size;
                acp_copy(ptr + 8 + size, local_heap, 16, ACP_HANDLE_NULL);
                acp_copy(ptr, local_heap + 16, 8, ACP_HANDLE_NULL);
                acp_copy(tmp[1] + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
                acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
            } else if (ptr == head_ga) {
                /* the free block is on the top of the list */
                tmp[0] -= 8 + size;
                tmp[2] = 8 + size;
                tmp[3] = ptr + 8 + size;
                acp_copy(ptr + 8 + size, local_heap, 16, ACP_HANDLE_NULL);
                acp_copy(ptr, local_heap + 16, 8, ACP_HANDLE_NULL);
                acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                do {
                    tmp[0] = tmp[1];
                    acp_copy(local_heap + 8, tmp[1] + 8, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                } while (tmp[1] != ptr);
                acp_copy(tmp[0] + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
            } else {
                tmp[0] -= 8 + size;
                tmp[2] = 8 + size;
                tmp[3] = ptr + 8 + size;
                acp_copy(ptr + 8 + size, local_heap, 16, ACP_HANDLE_NULL);
                acp_copy(ptr, local_heap + 16, 8, ACP_HANDLE_NULL);
                acp_copy(prev_ptr + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
                acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
            }
        }
    } else {
        /* the free block is going to be fully consumed */
        if (ptr == head_ga && tmp[1] == head_ga) {
            /* the free block is the only free block in the list */
            tmp[0] = ACP_GA_NULL;
            acp_copy(global_heap + HEAD, local_heap, 8, ACP_HANDLE_NULL);
        } else {
            acp_copy(local_heap + 16, tmp[1] + 8, 8, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            if (ptr == tmp[2]) {
                /* there are two free blocks is in the list */
                acp_copy(tmp[1] + 8, local_heap + 8, 8, ACP_HANDLE_NULL);
                acp_copy(global_heap + HEAD, local_heap + 8, 8, ACP_HANDLE_NULL);
            } else if (ptr == head_ga) {
                /* the free block is on the top of the list */
                do {
                    tmp[3] = tmp[2];
                    acp_copy(local_heap + 16, tmp[2] + 8, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                } while (tmp[2] != head_ga);
                acp_copy(tmp[3] + 8, local_heap + 8, 8, ACP_HANDLE_NULL);
                acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
            } else {
                tmp[3] = prev_ptr;
                acp_copy(prev_ptr + 8, local_heap + 8, 8, ACP_HANDLE_NULL);
                acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
            }
        }
    }
    
    acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    tmp[LLOCK >> 3] = 0;
    return ptr + 8;
}

void acp_free(acp_ga_t ga)
{
    acp_ga_t global_heap, local_heap, head_ga, ptr;
    volatile uint64_t* tmp;
    uint64_t size;
    
    if (ga == ACP_GA_NULL || (ga & 7) != 0) return;
    
    global_heap = iacp_query_starter_ga_dl(acp_query_rank(ga));
    local_heap = iacp_query_starter_ga_dl(acp_rank());
    tmp = (volatile uint64_t*)acp_query_address(local_heap);
    
    /* acquire size of deallocating block */
    acp_copy(local_heap, ga - 8, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    size = tmp[0];
    
    /* the size must be 24B or larger and aligned to 8B boundary */
    if (size < 24 || (size & 7) != 0) return;
    
    /* acquire locks */
    do {
        acp_cas8(local_heap, local_heap + LLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (tmp[0] != 0);
    
    do {
        acp_cas8(local_heap, global_heap + GLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (tmp[0] != 0);
    
    /* acquire the list head */
    acp_copy(local_heap, global_heap + HEAD, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    head_ga = ptr = tmp[0];
    
    /* seach the list for finding place to insert a block */
    if (head_ga == ACP_GA_NULL) {
        /* the list is empty */
        tmp[0] = ga - 8;
        acp_copy(global_heap + HEAD, local_heap, 8, ACP_HANDLE_NULL);
        acp_copy(ga, local_heap, 8, ACP_HANDLE_NULL);
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return;
    }
    acp_copy(local_heap, ptr, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    if (tmp[1] == head_ga) {
        /* the free block is the only free block */
        if (ptr + tmp[0] == ga - 8) {
            /* append; the total number of free blocks is not changed */
            tmp[0] += size;
            tmp[3] = ptr;
            acp_copy(ptr, local_heap, 8, ACP_HANDLE_NULL);
        } else if (ga - 8 + size == tmp[1]) {
            /* merge forward; the total number of free blocks is not changed */
            tmp[0] += size;
            tmp[1] = ga - 8;
            tmp[3] = ga - 8;
            acp_copy(ga - 8, local_heap, 16, ACP_HANDLE_NULL);
        } else {
            /* link; the total number of free blocks inceases */
            tmp[3] = ga - 8;
            acp_copy(ga, local_heap + 8, 8, ACP_HANDLE_NULL);
            acp_copy(ptr + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
        }
        acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
    } else {
        do {
            if ( ptr < tmp[1] ) {
                if (ptr < ga && ga < tmp[1]) {
                    if (ptr + tmp[0] == ga - 8 && ga - 8 + size == tmp[1]) {
                        /* connect; the total number of free blocks decreases */
                        acp_copy(local_heap + 16, ga - 8 + size, 16, ACP_HANDLE_NULL);
                        acp_complete(ACP_HANDLE_ALL);
                        tmp[0] += size + tmp[2];
                        tmp[1] = tmp [3];
                        tmp[3] = ptr;
                        acp_copy(ptr, local_heap, 16, ACP_HANDLE_NULL);
                    } else if (ptr + tmp[0] == ga - 8) {
                        /* append; the total number of free blocks is not changed */
                        tmp[0] += size;
                        tmp[3] = ptr;
                        acp_copy(ptr, local_heap, 8, ACP_HANDLE_NULL);
                    } else if (ga - 8 + size == tmp[1]) {
                        /* merge forward; the total number of free blocks is not changed */
                        acp_copy(local_heap, ga - 8 + size, 16, ACP_HANDLE_NULL);
                        acp_complete(ACP_HANDLE_ALL);
                        tmp[0] += size ;
                        tmp[3] = ga - 8;
                        acp_copy(ga - 8, local_heap, 16, ACP_HANDLE_NULL);
                        acp_copy(ptr + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
                    } else {
                        /* link; the total number of free blocks inceases */
                        tmp[3] = ga - 8;
                        acp_copy(ga, local_heap + 8, 8, ACP_HANDLE_NULL);
                        acp_copy(ptr + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
                    }
                    acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
                    acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
                    acp_complete(ACP_HANDLE_ALL);
                    tmp[LLOCK >> 3] = 0;
                    return;
                }
            } else {
                /* wrap around */
                if (ptr < ga) {
                    if (ptr + tmp[0] == ga - 8) {
                        /* append; the total number of free blocks is not changed */
                        tmp[0] += size;
                        tmp[3] = ptr;
                        acp_copy(ptr, local_heap, 8, ACP_HANDLE_NULL);
                    } else {
                        /* link; the total number of free blocks inceases */
                        tmp[3] = ga - 8;
                        acp_copy(ga, local_heap + 8, 8, ACP_HANDLE_NULL);
                        acp_copy(ptr + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
                    }
                    acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
                    acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
                    acp_complete(ACP_HANDLE_ALL);
                    tmp[LLOCK >> 3] = 0;
                    return;
                } else if (ga < tmp[1]) {
                    if (ga - 8 + size == tmp[1]) {
                        /* merge forward; the total number of free blocks is not changed */
                        acp_copy(local_heap, ga - 8 + size, 16, ACP_HANDLE_NULL);
                        acp_complete(ACP_HANDLE_ALL);
                        tmp[0] += size ;
                        tmp[3] = ga - 8;
                        acp_copy(ga - 8, local_heap, 16, ACP_HANDLE_NULL);
                        acp_copy(ptr + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
                    } else {
                        /* link; the total number of free blocks inceases */
                        tmp[3] = ga - 8;
                        acp_copy(ga, local_heap + 8, 8, ACP_HANDLE_NULL);
                        acp_copy(ptr + 8, local_heap + 24, 8, ACP_HANDLE_NULL);
                    }
                    acp_copy(global_heap + HEAD, local_heap + 24, 8, ACP_HANDLE_NULL);
                    acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
                    acp_complete(ACP_HANDLE_ALL);
                    tmp[LLOCK >> 3] = 0;
                    return;
                }
            }
            ptr = tmp[1];
            acp_copy(local_heap, ptr, 16, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
        } while (ptr != head_ga);
        /* if it is here, it is a fatal case */
    }
    
    acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    tmp[LLOCK >> 3] = 0;
    return;
}

