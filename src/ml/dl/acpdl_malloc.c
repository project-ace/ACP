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

#if 0
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

#define HEAP  16
#define LLOCK ((HEAP)+32)
#define GLOCK ((HEAP)+40)
#define HEAD  ((HEAP)+48)
#define TOP   ((HEAP)+56)

void iacpdl_init_malloc(void)
{
    acp_ga_t local_heap;
    volatile uint64_t* tmp;
    
    local_heap = iacp_query_starter_ga_dl(acp_rank()) + HEAP;
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
    acp_ga_t var_ga, global_heap, local_heap, head_ga, ptr, prev_ptr;
    acp_atkey_t atkey;
    volatile uint64_t var;
    volatile uint64_t* tmp;
    
    global_heap = iacp_query_starter_ga_dl(rank) + HEAP;
    local_heap = iacp_query_starter_ga_dl(acp_rank()) + HEAP;
    tmp = (volatile uint64_t*)acp_query_address(local_heap);
    
    /* size adjustment to minimum 24B and alignment to 8B boundary */
    size = (size < 24) ? 24 : ((size + 7ULL) & ~7ULL);
    
    /* acquire locks */
    atkey = acp_register_memory(&var, sizeof(var), rank % acp_colors());
    var_ga = acp_query_ga(atkey, &var);
     do {
        acp_cas8(var_ga, local_heap + LLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (var != 0);
    do {
        acp_cas8(var_ga, global_heap + GLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (var != 0);
    acp_unregister_memory(atkey);
    
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
    acp_ga_t var_ga, global_heap, local_heap, head_ga, ptr;
    uint64_t size;
    acp_atkey_t atkey;
    volatile uint64_t var;
    volatile uint64_t* tmp;
    
    if (ga == ACP_GA_NULL || (ga & 7) != 0) return;
    
    global_heap = iacp_query_starter_ga_dl(acp_query_rank(ga)) + HEAP;
    local_heap = iacp_query_starter_ga_dl(acp_rank()) + HEAP;
    tmp = (volatile uint64_t*)acp_query_address(local_heap);
    
    /* acquire locks */
    atkey = acp_register_memory(&var, sizeof(var), acp_query_rank(ga) % acp_colors());
    var_ga = acp_query_ga(atkey, &var);
     do {
        acp_cas8(var_ga, local_heap + LLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (var != 0);
    do {
        acp_cas8(var_ga, global_heap + GLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (var != 0);
    acp_unregister_memory(atkey);
    
    /* acquire size of deallocating block */
    acp_copy(local_heap, ga - 8, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    size = tmp[0];
    
    /* the size must be 24B or larger and aligned to 8B boundary */
    if (size < 24 || (size & 7) != 0) {
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return;
    }
    
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

#else
/*  Map of the DL starter memory:
 *      [0:39] temporal variables
 *      [40:48] local_lock
 *      [48:55] global_lock
 *      [56:63] GA of head entry of free list
 *      [64:71] sentinel (0 + 3)
 *      [72:Size-9] memory blocks
 *          allocated block
 *              [0:7] size + 3
 *              [8:size-9] user memory
 *              [size-8:size-1] size + 3
 *          free block
 *              [0:7] size + 5
 *              [8:size-17] free memory
 *              [size-16:size-9] GA of next entry of free list
 *              [size-8:size-1] size + 5
 *          extended free block
 *              [0:7] size + 6
 *              [8:size-9] free memory
 *              [size-8:size-1] size + 6
 *          (LSB 3bit of size = block type / Hamming distance 2)
 *      [Size-8:Size-1] sentinel (0 + 3)
 */

#define HEAP  16
#define LLOCK ((HEAP)+40)
#define GLOCK ((HEAP)+48)
#define HEAD  ((HEAP)+56)
#define SNTNL ((HEAP)+64)
#define TOP   ((HEAP)+72)

#define BLOCK_ALLOCATED 3
#define BLOCK_FREE      5
#define BLOCK_EXTENDED  6

void iacpdl_init_malloc(void)
{
    acp_ga_t local_heap;
    volatile uint64_t* tmp;
    size_t s;
    
    local_heap = iacp_query_starter_ga_dl(acp_rank()) + HEAP;
    tmp = (volatile uint64_t*)acp_query_address(local_heap);
    s = iacp_starter_memory_size_dl >> 3;
    
    /* initialize locks */
    tmp[LLOCK >> 3] = 0;
    tmp[GLOCK >> 3] = 0;
    
    /* set list head */
    tmp[HEAD >> 3] = local_heap + (s << 3) - 24;
    
    /* set top sentinel */
    tmp[SNTNL >> 3] = 0 + BLOCK_ALLOCATED;
    
    /* set free block */
    tmp[TOP >> 3] = (s << 3) - TOP - 8 + BLOCK_FREE;
    tmp[s - 3] = ACP_GA_NULL;
    tmp[s - 2] = (s << 3) - TOP - 8 + BLOCK_FREE;
    
    /* set bottom sentinel */
    tmp[s - 1] = 0 + BLOCK_ALLOCATED;
    
    return;
}

void iacpdl_finalize_malloc(void)
{
    return;
}

acp_ga_t acp_malloc(size_t size, int rank)
{
    acp_ga_t var_ga, global_heap, local_heap, head_ga, ptr, prev_ptr, next_ptr;
    uint64_t this_size, succ_size;
    acp_atkey_t atkey;
    volatile uint64_t var;
    volatile uint64_t* tmp;
    
    global_heap = iacp_query_starter_ga_dl(rank) + HEAP;
    local_heap = iacp_query_starter_ga_dl(acp_rank()) + HEAP;
    tmp = (volatile uint64_t*)acp_query_address(local_heap);
    
    /* size adjustment to minimum 8B and alignment to 8B boundary */
    size = (size < 8) ? 8 : ((size + 7ULL) & ~7ULL);
    
    /* acquire locks */
    atkey = acp_register_memory((void*)&var, sizeof(var), rank % acp_colors());
    var_ga = acp_query_ga(atkey, (void*)&var);
     do {
        acp_cas8(var_ga, local_heap + LLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (var != 0);
    do {
        acp_cas8(var_ga, global_heap + GLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (var != 0);
    acp_unregister_memory(atkey);
    
    /* acquire list head */
    acp_copy(local_heap, global_heap + HEAD, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    /* search for a sufficient free block */
    head_ga = ptr = tmp[0];
    prev_ptr = global_heap + HEAD;
    
    while (ptr != ACP_GA_NULL) {
        /* read control data */
        acp_copy(local_heap, ptr, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        next_ptr  = tmp[0];
        this_size = tmp[1];
        succ_size  = tmp[2];
        
        /* return if this block is not a free block */
        if ((this_size & 7) != BLOCK_FREE) {
            acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            tmp[LLOCK >> 3] = 0;
            return ACP_GA_NULL;
        }
        
        /* merge the succeding block if it is an extended free block */
        if ((succ_size & 7) == BLOCK_EXTENDED) {
            tmp[1] = this_size + (succ_size - BLOCK_EXTENDED);
            tmp[3] = ptr + (succ_size - BLOCK_EXTENDED);
            acp_copy(local_heap + 16, ptr + (succ_size - BLOCK_EXTENDED) + 16, 8, ACP_HANDLE_NULL);
            acp_copy(ptr - (this_size - BLOCK_FREE) + 16, local_heap + 8, 8, ACP_HANDLE_NULL);
            acp_copy(ptr + (succ_size - BLOCK_EXTENDED), local_heap, 16, ACP_HANDLE_NULL);
            acp_copy(prev_ptr, local_heap + 24, 8, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            
            if (prev_ptr == global_heap + HEAD) {
                head_ga += (succ_size - BLOCK_EXTENDED);
            }
            ptr += (succ_size - BLOCK_EXTENDED);
            this_size += (succ_size - BLOCK_EXTENDED);
            succ_size = tmp[2];
        }
        
        /* merge free blocks and rewind if the succeding block is a free block */
        if ((succ_size & 7) == BLOCK_FREE) {
            tmp[0] = ptr + (succ_size - BLOCK_FREE);
            tmp[1] = this_size + (succ_size - BLOCK_FREE);
            acp_copy(ptr - (this_size - BLOCK_FREE) + 16, local_heap + 8, 8, ACP_HANDLE_NULL);
            acp_copy(ptr + (succ_size - BLOCK_FREE) + 8, local_heap + 8, 8, ACP_HANDLE_NULL);
            acp_copy(prev_ptr, local_heap, 8, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            
            if (ptr == head_ga)
                head_ga = tmp[0];
            ptr = head_ga;
            prev_ptr = global_heap + HEAD;
            continue;
        }
        
        /* allocate the whole block and return */
        if (this_size - BLOCK_FREE >= size + 16 && this_size - BLOCK_FREE <= size + 40) {
            tmp[1] = (this_size - BLOCK_FREE) + BLOCK_ALLOCATED;
            acp_copy(ptr - (this_size - BLOCK_FREE) + 16, local_heap + 8, 8, ACP_HANDLE_NULL);
            acp_copy(ptr + 8, local_heap + 8, 8, ACP_HANDLE_NULL);
            acp_copy(prev_ptr, local_heap, 8, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            
            acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            tmp[LLOCK >> 3] = 0;
            return ptr - (this_size - BLOCK_FREE) + 24;
        }
        
        /* allocate a part of the block and return */
        if (this_size - BLOCK_FREE > size + 40) {
            tmp[1] = size + 16 + BLOCK_ALLOCATED;
            tmp[2] = this_size - size - 16;
            acp_copy(ptr - (this_size - BLOCK_FREE) + 16, local_heap + 8, 8, ACP_HANDLE_NULL);
            acp_copy(ptr - (this_size - BLOCK_FREE) + 16 + 8 + size, local_heap + 8, 16, ACP_HANDLE_NULL);
            acp_copy(ptr + 8, local_heap + 16, 8, ACP_HANDLE_NULL);
            /* move the remain free block to the top of the free list */
            if (prev_ptr != global_heap + HEAD) {
                tmp[3] = head_ga;
                tmp[4] = ptr;
                acp_copy(prev_ptr, local_heap, 8, ACP_HANDLE_NULL);
                acp_copy(ptr, local_heap + 24, 8, ACP_HANDLE_NULL);
                acp_copy(global_heap + HEAD, local_heap + 32, 8, ACP_HANDLE_NULL);
            }
            acp_complete(ACP_HANDLE_ALL);
            
            acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            tmp[LLOCK >> 3] = 0;
            return ptr - (this_size - BLOCK_FREE) + 24;
        }
        
        /* go to the next entry */
        prev_ptr = ptr;
        ptr = tmp[0];
    }
    
    /* there is no sufficient free block */
    acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    tmp[LLOCK >> 3] = 0;
    return ACP_GA_NULL;
}

void acp_free(acp_ga_t ga)
{
    acp_ga_t var_ga, global_heap, local_heap, head_ga, ptr;
    uint64_t prec_size, this_size, succ_size;
    acp_atkey_t atkey;
    volatile uint64_t var;
    volatile uint64_t* tmp;
    
    if (ga == ACP_GA_NULL || (ga & 7) != 0) return;
    
    global_heap = iacp_query_starter_ga_dl(acp_query_rank(ga)) + HEAP;
    local_heap = iacp_query_starter_ga_dl(acp_rank()) + HEAP;
    tmp = (volatile uint64_t*)acp_query_address(local_heap);
    
    /* acquire locks */
    atkey = acp_register_memory((void*)&var, sizeof(var), acp_query_rank(ga) % acp_colors());
    var_ga = acp_query_ga(atkey, (void*)&var);
    do {
        acp_cas8(var_ga, local_heap + LLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (var != 0);
    do {
        acp_cas8(var_ga, global_heap + GLOCK, 0, 1, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } while (var != 0);
    acp_unregister_memory(atkey);
    
    /* acquire preceding, this and succeding block sizes */
    acp_copy(local_heap, ga - 16, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    prec_size = tmp[0];
    this_size = tmp[1];
    if ((this_size & 7) != BLOCK_ALLOCATED) {
        /* broken memory block */
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return;
    }
    acp_copy(local_heap + 16, ga + (this_size - BLOCK_ALLOCATED) - 16, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    if (tmp[2] != this_size) {
        /* broken memory block */
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return;
    }
    succ_size = tmp[3];
    
    /* merge into the succeding block if it is a free block */
    if ((succ_size & 7) == BLOCK_FREE) {
        tmp[2] = succ_size + (this_size - BLOCK_ALLOCATED);
        acp_copy(ga - 8, local_heap + 16, 8, ACP_HANDLE_NULL);
        acp_copy(ga + (this_size - BLOCK_ALLOCATED) + (succ_size - BLOCK_FREE) - 16, local_heap + 16, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return;
    }
    
    /* merge into the preceding block if it is an extended free block */
    if ((prec_size & 7) == BLOCK_EXTENDED) {
        tmp[2] = prec_size + (this_size - BLOCK_ALLOCATED);
        acp_copy(ga - (prec_size - BLOCK_EXTENDED) - 8, local_heap + 16, 8, ACP_HANDLE_NULL);
        acp_copy(ga + (this_size - BLOCK_ALLOCATED) - 16, local_heap + 16, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return;
    }
    
    /* change to an extended free block if the preceding block is a free block */
    if ((prec_size & 7) == BLOCK_FREE) {
        tmp[2] = (this_size - BLOCK_ALLOCATED) + BLOCK_EXTENDED;
        acp_copy(ga - 8, local_heap + 16, 8, ACP_HANDLE_NULL);
        acp_copy(ga + (this_size - BLOCK_ALLOCATED) - 16, local_heap + 16, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        
        acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        tmp[LLOCK >> 3] = 0;
        return;
    }
    
    /* acquire list head */
    acp_copy(local_heap, global_heap + HEAD, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    head_ga = tmp[0];
    
    /* put new free block into the free list */
    tmp[1] = head_ga;
    tmp[2] = (this_size - BLOCK_ALLOCATED) + BLOCK_FREE;
    tmp[4] = ga + (this_size - BLOCK_ALLOCATED) - 24;
    
    acp_copy(ga - 8, local_heap + 16, 8, ACP_HANDLE_NULL);
    acp_copy(ga + (this_size - BLOCK_ALLOCATED) - 24, local_heap + 8, 16, ACP_HANDLE_NULL);
    acp_copy(global_heap + HEAD, local_heap + 32, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_cas8(local_heap, global_heap + GLOCK, 1, 0, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    tmp[LLOCK >> 3] = 0;
    return;
}

#endif
