/*
 * ACP Middle Layer: Data Library set
 * 
 * Copyright (c) 2014-2017 FUJITSU LIMITED
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
#include <string.h>
#include <sys/types.h>
#include <acp.h>
#include "acpdl.h"

/** Multiset
 *      [0-] directory of hash table
 ** slot of hash table ([1-3] = list control)
 *      [0]  lock
 *      [1]  ga of head element
 *      [2]  ga of tail element
 *      [3]  number of elements
 ** element of list
 *      [0]  ga of next element
 *      [1]  ga of previos element
 *      [2]  counter
 *      [3]  size of key
 *      [4-] key
 */

static void iacp_clear_list_multiset(volatile uint64_t* list, acp_ga_t buf_elem, volatile uint64_t* elem)
{
    /* slot must be locked */
    /* list must have data */
    
    /*** local buffer variables ***/
    
//  uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    
    /*** clear slot ***/
    
    acp_ga_t ga_next_elem = list[0];
    while (ga_next_elem != ACP_GA_NULL) {
        acp_ga_t ga_elem = ga_next_elem;
        acp_copy(buf_elem, ga_elem, size_elem, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        ga_next_elem = elem[0];
        acp_free(ga_elem);
    }
    
    list[0] = ACP_GA_NULL;
    list[1] = ACP_GA_NULL;
    list[2] = 0;
    return;
}

static int iacp_insert_multiset(acp_multiset_t set, uint64_t count, acp_element_t key, acp_ga_t buf_directory, acp_ga_t buf_lock_var, acp_ga_t buf_list, acp_ga_t buf_elem, acp_ga_t buf_new_key, acp_ga_t buf_elem_key, volatile acp_ga_t* directory, volatile uint64_t* lock_var, volatile uint64_t* list, volatile uint64_t* elem, volatile uint8_t* new_key, volatile uint8_t* elem_key)
{
    /* directory must have data */
    /* elem, new_key and new_value must be continuous */
    
    /*** local buffer variables ***/
    
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = key.size;
    
    /*** read key ***/
    
    if (acp_query_rank(key.ga) != acp_rank()) {
        acp_copy(buf_new_key, key.ga, size_new_key, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        memcpy((void*)new_key, acp_query_address(key.ga), size_new_key);
    }
    
    /*** insert key ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / set.num_slots) % set.num_ranks;
    int slot = crc % set.num_slots;
    
    acp_ga_t ga_lock_var = directory[rank] + slot * 32;
    acp_ga_t ga_list = ga_lock_var + 8;
    do {
        acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
    } while (*lock_var != 0);
    
    acp_ga_t ga_next_elem = list[0];
    while (ga_next_elem != ACP_GA_NULL) {
        acp_ga_t ga_elem = ga_next_elem;
        acp_copy(buf_elem, ga_elem, size_elem, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        ga_next_elem = elem[0];
        if (elem[3] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                elem[2] = elem[2] + count;
                acp_copy(ga_elem + 16, buf_elem + 16, 8, ACP_HANDLE_NULL);
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
                return 1;
            }
        }
    }
    
    acp_ga_t ga_new_elem = acp_malloc(size_elem + size_new_key, acp_query_rank(ga_list));
    if (ga_new_elem == ACP_GA_NULL) {
        acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        return 0;
    }
    
    elem[0] = ACP_GA_NULL;
    elem[2] = count;
    elem[3] = size_new_key;
    if (list[2] == 0) {
        elem[1] = ACP_GA_NULL;
        list[0] = ga_new_elem;
        list[1] = ga_new_elem;
    } else {
        elem[1] = list[1];
        list[1] = ga_new_elem;
        acp_copy(elem[1], buf_list + 8, 8, ACP_HANDLE_NULL);
    }
    list[2]++;
    
    acp_copy(ga_new_elem, buf_elem, size_elem + size_new_key, ACP_HANDLE_NULL);
    acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    
    return 1;
}

static inline acp_multiset_it_t iacp_null_multiset_it(acp_multiset_t set)
{
    acp_multiset_it_t it;
    it.set = set;
    it.rank = set.num_ranks;
    it.slot = 0;
    it.elem = ACP_GA_NULL;
    return it;
}

void acp_assign_local_multiset(acp_multiset_t set1, acp_multiset_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 32;
    uint64_t offset_directory  = 0;
    uint64_t offset_directory2 = offset_directory  + size_directory;
    uint64_t offset_lock_var   = offset_directory2 + size_directory2;
    uint64_t offset_list       = offset_lock_var   + size_lock_var;
    uint64_t offset_elem2      = offset_list       + size_list;
    uint64_t size_buf          = offset_elem2      + size_elem2;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory  = buf + offset_directory;
    acp_ga_t buf_directory2 = buf + offset_directory2;
    acp_ga_t buf_lock_var   = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem2     = buf + offset_elem2;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    /*** clear set1 ***/
    
    acp_copy(buf_directory, set1.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < set1.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set1.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_multiset(list, buf_elem2, elem2);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    /*** local buffer variables ***/
    
    acp_copy(buf_directory2, set2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of set2 ***/
    
    int my_rank = acp_rank();
    
    for (rank = 0; rank < set2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < set2.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            
            acp_ga_t ga_next_elem = list[0];
            while (ga_next_elem != ACP_GA_NULL) {
                acp_ga_t ga_elem = ga_next_elem;
                acp_copy(buf_elem2, ga_elem, size_elem2, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                ga_next_elem = elem2[0];
                uint64_t count = elem2[2];
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_elem_key  = offset_new_key   + size_new_key;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* elem_key;
                
                if (size_buf_elem > max_size_buf_elem) {
                    if (max_size_buf_elem > 0) acp_free(buf_elem);
                    buf_elem = acp_malloc(size_buf_elem, acp_rank());
                    if (buf_elem == ACP_GA_NULL) {
                        acp_free(buf);
                        return;
                    }
                    max_size_buf_elem = size_buf_elem;
                    buf_new_key   = buf_elem + offset_new_key;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = (uintptr_t)acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_multiset(set1, count, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_assign_multiset(acp_multiset_t set1, acp_multiset_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 32;
    uint64_t offset_directory  = 0;
    uint64_t offset_directory2 = offset_directory  + size_directory;
    uint64_t offset_lock_var   = offset_directory2 + size_directory2;
    uint64_t offset_list       = offset_lock_var   + size_lock_var;
    uint64_t offset_elem2      = offset_list       + size_list;
    uint64_t size_buf          = offset_elem2      + size_elem2;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory  = buf + offset_directory;
    acp_ga_t buf_directory2 = buf + offset_directory2;
    acp_ga_t buf_lock_var   = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem2     = buf + offset_elem2;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    /*** clear set1 ***/
    
    acp_copy(buf_directory, set1.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < set1.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set1.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_multiset(list, buf_elem2, elem2);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    /*** local buffer variables ***/
    
    acp_copy(buf_directory2, set2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of set2 ***/
    
    for (rank = 0; rank < set2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        for (slot = 0; slot < set2.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            
            acp_ga_t ga_next_elem = list[0];
            while (ga_next_elem != ACP_GA_NULL) {
                acp_ga_t ga_elem = ga_next_elem;
                acp_copy(buf_elem2, ga_elem, size_elem2, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                ga_next_elem = elem2[0];
                uint64_t count = elem2[2];
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_elem_key  = offset_new_key   + size_new_key;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* elem_key;
                
                if (size_buf_elem > max_size_buf_elem) {
                    if (max_size_buf_elem > 0) acp_free(buf_elem);
                    buf_elem = acp_malloc(size_buf_elem, acp_rank());
                    if (buf_elem == ACP_GA_NULL) {
                        acp_free(buf);
                        return;
                    }
                    max_size_buf_elem = size_buf_elem;
                    buf_new_key   = buf_elem + offset_new_key;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = (uintptr_t)acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_multiset(set1, count, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

acp_multiset_it_t acp_begin_local_multiset(acp_multiset_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t size_buf         = offset_list      + size_list;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_multiset_it(set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** search first element ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            if (list[2] > 0) {
                acp_multiset_it_t it;
                it.set = set;
                it.rank = rank;
                it.slot = slot;
                it.elem = list[0];
                acp_free(buf);
                return it;
            }
        }
    }
    
    acp_free(buf);
    return iacp_null_multiset_it(set);
}

acp_multiset_it_t acp_begin_multiset(acp_multiset_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t size_buf         = offset_list      + size_list;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_multiset_it(set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** search first element ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            if (list[2] > 0) {
                acp_multiset_it_t it;
                it.set = set;
                it.rank = rank;
                it.slot = slot;
                it.elem = list[0];
                acp_free(buf);
                return it;
            }
        }
    }
    
    acp_free(buf);
    return iacp_null_multiset_it(set);
}

void acp_clear_local_multiset(acp_multiset_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** clear multiset ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_multiset(list, buf_elem, elem);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    acp_free(buf);
    return;
}

void acp_clear_multiset(acp_multiset_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** clear multiset ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_multiset(list, buf_elem, elem);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    acp_free(buf);
    return;
}

acp_set_t acp_collapse_multiset(acp_multiset_t set)
{
    acp_set_t ret;
    ret.ga = set.ga;
    ret.num_ranks = set.num_ranks;
    ret.num_slots = set.num_slots;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return ret;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    volatile uint8_t*  elem_byte = (volatile uint8_t* )(ptr + offset_elem);
    
    /*** collapse multiset ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot, i;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            
            acp_ga_t ga_next_elem = list[0];
            while (ga_next_elem != ACP_GA_NULL) {
                acp_ga_t ga_elem = ga_next_elem;
                acp_copy(buf_elem, ga_elem, size_elem, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                /*** transform a counter into a big endian number and concatenate it to the head of the key ***/
                uint64_t counter = elem[2];
                uint64_t size = elem[3];
                elem[2] = size + 8;
                for (i = 0; i < 8; i++) elem_byte[24 + i] = (counter >> ((7 - i) * 8)) & 0xff;
                acp_copy(ga_elem + 16, buf_elem + 16, size_elem - 16, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                ga_next_elem = elem[0];
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    acp_free(buf);
    return ret;
}

acp_multiset_t acp_create_multiset(int num_ranks, const int* ranks, int num_slots, int rank)
{
    acp_multiset_t set;
    set.ga = ACP_GA_NULL;
    set.num_ranks = num_ranks;
    set.num_slots = num_slots;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = num_ranks * 8;
    uint64_t size_table     = num_slots * 32;
    uint64_t offset_directory = 0;
    uint64_t offset_table     = offset_directory + size_directory;
    uint64_t size_buf         = offset_table     + size_table;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return set;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_table     = buf + offset_table;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* table     = (volatile uint64_t*)(ptr + offset_table);
    
    int slot;
    
    for (slot = 0; slot < num_slots; slot++) {
        table[slot * 4 + 0] = 0;
        table[slot * 4 + 1] = ACP_GA_NULL;
        table[slot * 4 + 2] = ACP_GA_NULL;
        table[slot * 4 + 3] = 0;
    }
    
    /*** create set ***/
    
    set.ga = acp_malloc(size_directory, rank);
    if (set.ga == ACP_GA_NULL) {
        acp_free(buf);
        return set;
    }
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = acp_malloc(size_table, ranks[rank]);
        if (ga_table == ACP_GA_NULL) {
            while (rank > 0) acp_free(directory[--rank]);
            acp_free(set.ga);
            acp_free(buf);
            set.ga = ACP_GA_NULL;
            return set;
        }
        acp_copy(ga_table, buf_table, size_table, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        directory[rank] = ga_table;
    }
    acp_copy(set.ga, buf_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return set;
}

void acp_destroy_multiset(acp_multiset_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** destroy set ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_multiset(list, buf_elem, elem);
        }
        acp_free(ga_table);
    }
    
    acp_free(set.ga);
    acp_free(buf);
    return;
}

int acp_empty_local_multiset(acp_multiset_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t size_buf         = offset_list      + size_list;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return 1;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** seak slots ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            if (list[2] > 0) {
                acp_free(buf);
                return 0;
            }
        }
    }
    
    acp_free(buf);
    return 1;
}

int acp_empty_multiset(acp_multiset_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t size_buf         = offset_list      + size_list;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return 1;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** seak slots ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            if (list[2] > 0) {
                acp_free(buf);
                return 0;
            }
        }
    }
    
    acp_free(buf);
    return 1;
}

acp_multiset_it_t acp_end_local_multiset(acp_multiset_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t size_buf         = offset_list      + size_list;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_multiset_it(set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** search first element of next slot ***/
    
    acp_copy(buf_directory, set.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 1; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(directory[rank - 1]) != my_rank) continue;
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            if (list[2] > 0) {
                acp_multiset_it_t it;
                it.set = set;
                it.rank = rank;
                it.slot = slot;
                it.elem = list[0];
                acp_free(buf);
                return it;
            }
        }
    }
    
    acp_free(buf);
    return iacp_null_multiset_it(set);
}

acp_multiset_it_t acp_end_multiset(acp_multiset_t set)
{
    return iacp_null_multiset_it(set);
}

acp_multiset_it_t acp_find_multiset(acp_multiset_t set, acp_element_t key)
{
    if (key.size == 0) return iacp_null_multiset_it(set);
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = key.size;
    uint64_t size_elem_key  = size_new_key;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t offset_new_key   = offset_elem      + size_elem;
    uint64_t offset_elem_key  = offset_new_key   + size_new_key;
    uint64_t size_buf         = offset_elem_key  + size_elem_key;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_multiset_it(set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    acp_ga_t buf_new_key   = buf + offset_new_key;
    acp_ga_t buf_elem_key  = buf + offset_elem_key;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    volatile uint8_t* new_key    = (volatile uint8_t*)(ptr + offset_new_key);
    volatile uint8_t* elem_key   = (volatile uint8_t*)(ptr + offset_elem_key);
    
    /*** read key ***/
    
    if (acp_query_rank(key.ga) != acp_rank()) {
        acp_copy(buf_new_key, key.ga, size_new_key, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        memcpy((void*)new_key, acp_query_address(key.ga), size_new_key);
    }
    
    /*** find key ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / set.num_slots) % set.num_ranks;
    int slot = crc % set.num_slots;
    
    acp_ga_t ga_directory = set.ga + rank * 8;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t ga_lock_var = *directory + slot * 32;
    acp_ga_t ga_list = ga_lock_var + 8;
    do {
        acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
    } while (*lock_var != 0);
    
    acp_ga_t ga_next_elem = list[0];
    while (ga_next_elem != ACP_GA_NULL) {
        acp_ga_t ga_elem = ga_next_elem;
        acp_copy(buf_elem, ga_elem, size_elem, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        ga_next_elem = elem[0];
        if (elem[3] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                acp_multiset_it_t it;
                it.set = set;
                it.rank = rank;
                it.slot = slot;
                it.elem = ga_elem;
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
                acp_free(buf);
                return it;
            }
        }
    }
    
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return iacp_null_multiset_it(set);
}

int acp_insert_multiset(acp_multiset_t set, acp_element_t key)
{
    if (key.size == 0) return 0;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = key.size;
    uint64_t size_elem_key  = size_new_key;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t offset_new_key   = offset_elem      + size_elem;
    uint64_t offset_elem_key  = offset_new_key   + size_new_key;
    uint64_t size_buf         = offset_elem_key  + size_elem_key;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    acp_ga_t buf_new_key   = buf + offset_new_key;
    acp_ga_t buf_elem_key  = buf + offset_elem_key;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    volatile uint8_t* new_key    = (volatile uint8_t*)(ptr + offset_new_key);
    volatile uint8_t* elem_key   = (volatile uint8_t*)(ptr + offset_elem_key);
    
    /*** read key ***/
    
    if (acp_query_rank(key.ga) != acp_rank()) {
        acp_copy(buf_new_key, key.ga, size_new_key, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        memcpy((void*)new_key, acp_query_address(key.ga), size_new_key);
    }
    
    /*** insert key ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / set.num_slots) % set.num_ranks;
    int slot = crc % set.num_slots;
    
    acp_ga_t ga_directory = set.ga + rank * 8;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t ga_lock_var = *directory + slot * 32;
    acp_ga_t ga_list = ga_lock_var + 8;
    do {
        acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
    } while (*lock_var != 0);
    
    acp_ga_t ga_next_elem = list[0];
    while (ga_next_elem != ACP_GA_NULL) {
        acp_ga_t ga_elem = ga_next_elem;
        acp_copy(buf_elem, ga_elem, size_elem, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        ga_next_elem = elem[0];
        if (elem[3] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                elem[2] = elem[2] + 1;
                acp_copy(ga_elem + 16, buf_elem + 16, 8, ACP_HANDLE_NULL);
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
                acp_free(buf);
                return 1;
            }
        }
    }
    
    acp_ga_t ga_new_elem = acp_malloc(size_elem + size_new_key, acp_query_rank(ga_list));
    if (ga_new_elem == ACP_GA_NULL) {
        acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(buf);
        return 0;
    }
    
    elem[0] = ACP_GA_NULL;
    elem[2] = 1;
    elem[3] = size_new_key;
    if (list[2] == 0) {
        elem[1] = ACP_GA_NULL;
        list[0] = ga_new_elem;
        list[1] = ga_new_elem;
    } else {
        elem[1] = list[1];
        list[1] = ga_new_elem;
        acp_copy(elem[1], buf_list + 8, 8, ACP_HANDLE_NULL);
    }
    list[2]++;
    
    acp_copy(ga_new_elem, buf_elem, size_elem + size_new_key, ACP_HANDLE_NULL);
    acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return 1;
}

void acp_merge_local_multiset(acp_multiset_t set1, acp_multiset_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 32;
    uint64_t offset_directory  = 0;
    uint64_t offset_directory2 = offset_directory  + size_directory;
    uint64_t offset_lock_var   = offset_directory2 + size_directory2;
    uint64_t offset_list       = offset_lock_var   + size_lock_var;
    uint64_t offset_elem2      = offset_list       + size_list;
    uint64_t size_buf          = offset_elem2      + size_elem2;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory  = buf + offset_directory;
    acp_ga_t buf_directory2 = buf + offset_directory2;
    acp_ga_t buf_lock_var   = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem2     = buf + offset_elem2;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    acp_copy(buf_directory, set1.ga, size_directory, ACP_HANDLE_NULL);
    acp_copy(buf_directory2, set2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of set2 ***/
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < set2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < set2.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            
            acp_ga_t ga_next_elem = list[0];
            while (ga_next_elem != ga_list && ga_next_elem != ACP_GA_NULL) {
                acp_ga_t ga_elem = ga_next_elem;
                acp_copy(buf_elem2, ga_elem, size_elem2, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                ga_next_elem = elem2[0];
                uint64_t count = elem2[2];
                
                /*** insert key ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_elem_key  = offset_new_key   + size_new_key;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* elem_key;
                
                if (size_buf_elem > max_size_buf_elem) {
                    if (max_size_buf_elem > 0) acp_free(buf_elem);
                    buf_elem = acp_malloc(size_buf_elem, acp_rank());
                    if (buf_elem == ACP_GA_NULL) {
                        acp_free(buf);
                        return;
                    }
                    max_size_buf_elem = size_buf_elem;
                    buf_new_key   = buf_elem + offset_new_key;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = (uintptr_t)acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size  = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_multiset(set1, count, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_merge_multiset(acp_multiset_t set1, acp_multiset_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 32;
    uint64_t offset_directory  = 0;
    uint64_t offset_directory2 = offset_directory  + size_directory;
    uint64_t offset_lock_var   = offset_directory2 + size_directory2;
    uint64_t offset_list       = offset_lock_var   + size_lock_var;
    uint64_t offset_elem2      = offset_list       + size_list;
    uint64_t size_buf          = offset_elem2      + size_elem2;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory  = buf + offset_directory;
    acp_ga_t buf_directory2 = buf + offset_directory2;
    acp_ga_t buf_lock_var   = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem2     = buf + offset_elem2;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    acp_copy(buf_directory, set1.ga, size_directory, ACP_HANDLE_NULL);
    acp_copy(buf_directory2, set2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of set2 ***/
    
    int rank, slot;
    
    for (rank = 0; rank < set2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        for (slot = 0; slot < set2.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            
            acp_ga_t ga_next_elem = list[0];
            while (ga_next_elem != ga_list && ga_next_elem != ACP_GA_NULL) {
                acp_ga_t ga_elem = ga_next_elem;
                acp_copy(buf_elem2, ga_elem, size_elem2, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                ga_next_elem = elem2[0];
                uint64_t count = elem2[2];
                
                /*** insert key ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_elem_key  = offset_new_key   + size_new_key;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* elem_key;
                
                if (size_buf_elem > max_size_buf_elem) {
                    if (max_size_buf_elem > 0) acp_free(buf_elem);
                    buf_elem = acp_malloc(size_buf_elem, acp_rank());
                    if (buf_elem == ACP_GA_NULL) {
                        acp_free(buf);
                        return;
                    }
                    max_size_buf_elem = size_buf_elem;
                    buf_new_key   = buf_elem + offset_new_key;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = (uintptr_t)acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size  = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_multiset(set1, count, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_move_local_multiset(acp_multiset_t set1, acp_multiset_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 32;
    uint64_t offset_directory  = 0;
    uint64_t offset_directory2 = offset_directory  + size_directory;
    uint64_t offset_lock_var   = offset_directory2 + size_directory2;
    uint64_t offset_list       = offset_lock_var   + size_lock_var;
    uint64_t offset_elem2      = offset_list       + size_list;
    uint64_t size_buf          = offset_elem2      + size_elem2;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory  = buf + offset_directory;
    acp_ga_t buf_directory2 = buf + offset_directory2;
    acp_ga_t buf_lock_var   = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem2     = buf + offset_elem2;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    acp_copy(buf_directory, set1.ga, size_directory, ACP_HANDLE_NULL);
    acp_copy(buf_directory2, set2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of set2 ***/
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < set2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < set2.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            
            acp_ga_t ga_next_elem = list[0];
            while (ga_next_elem != ga_list && ga_next_elem != ACP_GA_NULL) {
                acp_ga_t ga_elem = ga_next_elem;
                acp_copy(buf_elem2, ga_elem, size_elem2, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                ga_next_elem = elem2[0];
                uint64_t count = elem2[2];
                
                /*** insert key ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_elem_key  = offset_new_key   + size_new_key;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* elem_key;
                
                if (size_buf_elem > max_size_buf_elem) {
                    if (max_size_buf_elem > 0) acp_free(buf_elem);
                    buf_elem = acp_malloc(size_buf_elem, acp_rank());
                    if (buf_elem == ACP_GA_NULL) {
                        acp_free(buf);
                        return;
                    }
                    max_size_buf_elem = size_buf_elem;
                    buf_new_key   = buf_elem + offset_new_key;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = (uintptr_t)acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size  = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_multiset(set1, count, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
                
                /* remove key */
                
                acp_free(ga_elem);
            }
            
            /* clear slot */
            
            list[0] = ACP_GA_NULL;
            list[1] = ACP_GA_NULL;
            list[2] = 0;
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_move_multiset(acp_multiset_t set1, acp_multiset_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 32;
    uint64_t offset_directory  = 0;
    uint64_t offset_directory2 = offset_directory  + size_directory;
    uint64_t offset_lock_var   = offset_directory2 + size_directory2;
    uint64_t offset_list       = offset_lock_var   + size_lock_var;
    uint64_t offset_elem2      = offset_list       + size_list;
    uint64_t size_buf          = offset_elem2      + size_elem2;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory  = buf + offset_directory;
    acp_ga_t buf_directory2 = buf + offset_directory2;
    acp_ga_t buf_lock_var   = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem2     = buf + offset_elem2;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    acp_copy(buf_directory, set1.ga, size_directory, ACP_HANDLE_NULL);
    acp_copy(buf_directory2, set2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of set2 ***/
    
    int rank, slot;
    
    for (rank = 0; rank < set2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        for (slot = 0; slot < set2.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            
            acp_ga_t ga_next_elem = list[0];
            while (ga_next_elem != ga_list && ga_next_elem != ACP_GA_NULL) {
                acp_ga_t ga_elem = ga_next_elem;
                acp_copy(buf_elem2, ga_elem, size_elem2, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                ga_next_elem = elem2[0];
                uint64_t count = elem2[2];
                
                /*** insert key ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_elem_key  = offset_new_key   + size_new_key;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* elem_key;
                
                if (size_buf_elem > max_size_buf_elem) {
                    if (max_size_buf_elem > 0) acp_free(buf_elem);
                    buf_elem = acp_malloc(size_buf_elem, acp_rank());
                    if (buf_elem == ACP_GA_NULL) {
                        acp_free(buf);
                        return;
                    }
                    max_size_buf_elem = size_buf_elem;
                    buf_new_key   = buf_elem + offset_new_key;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = (uintptr_t)acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size  = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_multiset(set1, count, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
                
                /* remove key */
                
                acp_free(ga_elem);
            }
            
            /* clear slot */
            
            list[0] = ACP_GA_NULL;
            list[1] = ACP_GA_NULL;
            list[2] = 0;
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_remove_multiset(acp_multiset_t set, acp_element_t key)
{
    if (key.size == 0) return;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = key.size;
    uint64_t size_elem_key  = size_new_key;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t offset_new_key   = offset_elem      + size_elem;
    uint64_t offset_elem_key  = offset_new_key   + size_new_key;
    uint64_t size_buf         = offset_elem_key  + size_elem_key;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    acp_ga_t buf_new_key   = buf + offset_new_key;
    acp_ga_t buf_elem_key  = buf + offset_elem_key;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    volatile uint8_t* new_key    = (volatile uint8_t*)(ptr + offset_new_key);
    volatile uint8_t* elem_key   = (volatile uint8_t*)(ptr + offset_elem_key);
    
    /*** read key ***/
    
    if (acp_query_rank(key.ga) != acp_rank()) {
        acp_copy(buf_new_key, key.ga, size_new_key, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        memcpy((void*)new_key, acp_query_address(key.ga), size_new_key);
    }
    
    /*** remove key ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / set.num_slots) % set.num_ranks;
    int slot = crc % set.num_slots;
    
    acp_ga_t ga_directory = set.ga + rank * 8;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t ga_lock_var = *directory + slot * 32;
    acp_ga_t ga_list = ga_lock_var + 8;
    do {
        acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
    } while (*lock_var != 0);
    
    acp_ga_t ga_next_elem = list[0];
    while (ga_next_elem != ACP_GA_NULL) {
        acp_ga_t ga_elem = ga_next_elem;
        acp_copy(buf_elem, ga_elem, size_elem, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        ga_next_elem = elem[0];
        if (elem[3] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                uint64_t count = elem[2];
                if (count > 1) {
                    elem[2] = count - 1;
                    acp_copy(ga_elem + 16, buf_elem + 16, 8, ACP_HANDLE_NULL);
                    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
                    acp_complete(ACP_HANDLE_ALL);
                    acp_free(buf);
                    return;
                }
                
                acp_ga_t tmp_head = list[0];
                acp_ga_t tmp_tail = list[1];
                uint64_t tmp_num  = list[2];
                acp_ga_t tmp_next = elem[0];
                acp_ga_t tmp_prev = elem[1];
                
                if (tmp_num <= 1) {
                    list[0] = ACP_GA_NULL;
                    list[1] = ACP_GA_NULL;
                    list[2] = 0;
                    acp_free(ga_elem);
                } else if (tmp_head == ga_elem) {
                    list[0] = tmp_next;
                    list[2] = tmp_num - 1;
                    acp_copy(tmp_next + 8, ga_elem + 8, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                    acp_free(ga_elem);
                } else if (tmp_tail == ga_elem) {
                    list[1] = tmp_prev;
                    list[2] = tmp_num - 1;
                    acp_copy(tmp_prev, ga_elem, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                    acp_free(ga_elem);
                } else {
                    list[2] = tmp_num - 1;
                    acp_copy(tmp_next + 8, ga_elem + 8, 8, ACP_HANDLE_NULL);
                    acp_copy(tmp_prev, ga_elem, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                    acp_free(ga_elem);
                }
                acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
                acp_free(buf);
                return;
            }
        }
    }
    
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return;
}

void acp_remove_all_multiset(acp_multiset_t set, acp_element_t key)
{
    if (key.size == 0) return;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = key.size;
    uint64_t size_elem_key  = size_new_key;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t offset_new_key   = offset_elem      + size_elem;
    uint64_t offset_elem_key  = offset_new_key   + size_new_key;
    uint64_t size_buf         = offset_elem_key  + size_elem_key;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    acp_ga_t buf_new_key   = buf + offset_new_key;
    acp_ga_t buf_elem_key  = buf + offset_elem_key;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    volatile uint8_t* new_key    = (volatile uint8_t*)(ptr + offset_new_key);
    volatile uint8_t* elem_key   = (volatile uint8_t*)(ptr + offset_elem_key);
    
    /*** read key ***/
    
    if (acp_query_rank(key.ga) != acp_rank()) {
        acp_copy(buf_new_key, key.ga, size_new_key, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        memcpy((void*)new_key, acp_query_address(key.ga), size_new_key);
    }
    
    /*** remove key ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / set.num_slots) % set.num_ranks;
    int slot = crc % set.num_slots;
    
    acp_ga_t ga_directory = set.ga + rank * 8;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t ga_lock_var = *directory + slot * 32;
    acp_ga_t ga_list = ga_lock_var + 8;
    do {
        acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
    } while (*lock_var != 0);
    
    acp_ga_t ga_next_elem = list[0];
    while (ga_next_elem != ACP_GA_NULL) {
        acp_ga_t ga_elem = ga_next_elem;
        acp_copy(buf_elem, ga_elem, size_elem, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        ga_next_elem = elem[0];
        if (elem[3] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                acp_ga_t tmp_head = list[0];
                acp_ga_t tmp_tail = list[1];
                uint64_t tmp_num  = list[2];
                acp_ga_t tmp_next = elem[0];
                acp_ga_t tmp_prev = elem[1];
                
                if (tmp_num <= 1) {
                    list[0] = ACP_GA_NULL;
                    list[1] = ACP_GA_NULL;
                    list[2] = 0;
                    acp_free(ga_elem);
                } else if (tmp_head == ga_elem) {
                    list[0] = tmp_next;
                    list[2] = tmp_num - 1;
                    acp_copy(tmp_next + 8, ga_elem + 8, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                    acp_free(ga_elem);
                } else if (tmp_tail == ga_elem) {
                    list[1] = tmp_prev;
                    list[2] = tmp_num - 1;
                    acp_copy(tmp_prev, ga_elem, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                    acp_free(ga_elem);
                } else {
                    list[2] = tmp_num - 1;
                    acp_copy(tmp_next + 8, ga_elem + 8, 8, ACP_HANDLE_NULL);
                    acp_copy(tmp_prev, ga_elem, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                    acp_free(ga_elem);
                }
                acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
                acp_free(buf);
                return;
            }
        }
    }
    
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return;
}

uint64_t acp_retrieve_multiset(acp_multiset_t set, acp_element_t key)
{
    if (key.size == 0) return 0;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = key.size;
    uint64_t size_elem_key  = size_new_key;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t offset_new_key   = offset_elem      + size_elem;
    uint64_t offset_elem_key  = offset_new_key   + size_new_key;
    uint64_t size_buf         = offset_elem_key  + size_elem_key;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    acp_ga_t buf_new_key   = buf + offset_new_key;
    acp_ga_t buf_elem_key  = buf + offset_elem_key;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    volatile uint8_t* new_key    = (volatile uint8_t*)(ptr + offset_new_key);
    volatile uint8_t* elem_key   = (volatile uint8_t*)(ptr + offset_elem_key);
    
    /*** read key ***/
    
    if (acp_query_rank(key.ga) != acp_rank()) {
        acp_copy(buf_new_key, key.ga, size_new_key, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        memcpy((void*)new_key, acp_query_address(key.ga), size_new_key);
    }
    
    /*** find key ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / set.num_slots) % set.num_ranks;
    int slot = crc % set.num_slots;
    
    acp_ga_t ga_directory = set.ga + rank * 8;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t ga_lock_var = *directory + slot * 32;
    acp_ga_t ga_list = ga_lock_var + 8;
    do {
        acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
    } while (*lock_var != 0);
    
    acp_ga_t ga_next_elem = list[0];
    while (ga_next_elem != ACP_GA_NULL) {
        acp_ga_t ga_elem = ga_next_elem;
        acp_copy(buf_elem, ga_elem, size_elem, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        ga_next_elem = elem[0];
        if (elem[3] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                uint64_t count = elem[2];
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
                acp_free(buf);
                return count;
            }
        }
    }
    
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return 0;
}

size_t acp_size_local_multiset(acp_multiset_t set)
{
    size_t ret = 0;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_table     = set.num_slots * 32;
    uint64_t offset_directory = 0;
    uint64_t offset_table     = offset_directory + size_directory;
    uint64_t size_buf         = offset_table     + size_table;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return ret;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_table     = buf + offset_table;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* table     = (volatile uint64_t*)(ptr + offset_table);
    
    /*** count number of elements in slots ***/
    
    acp_ga_t ga_directory = set.ga;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        acp_copy(buf_table, ga_table, size_table, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        for (slot = 0; slot < set.num_slots; slot++) ret += table[slot * 4 + 3];
    }
    
    acp_free(buf);
    return ret;
}

size_t acp_size_multiset(acp_multiset_t set)
{
    size_t ret = 0;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_table     = set.num_slots * 32;
    uint64_t offset_directory = 0;
    uint64_t offset_table     = offset_directory + size_directory;
    uint64_t size_buf         = offset_table     + size_table;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return ret;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_table     = buf + offset_table;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* table     = (volatile uint64_t*)(ptr + offset_table);
    
    /*** count number of elements in slots ***/
    
    acp_ga_t ga_directory = set.ga;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        acp_copy(buf_table, ga_table, size_table, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        for (slot = 0; slot < set.num_slots; slot++) ret += table[slot * 4 + 3];
    }
    
    acp_free(buf);
    return ret;
}

void acp_swap_multiset(acp_multiset_t set1, acp_multiset_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set2.num_ranks * 8;
    uint64_t size_directory2 = size_directory;
    uint64_t size_table      = set2.num_slots * 32;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem       = 32;
    uint64_t offset_directory  = 0;
    uint64_t offset_directory2 = offset_directory  + size_directory;
    uint64_t offset_table      = offset_directory2 + size_directory2;
    uint64_t offset_lock_var   = offset_table      + size_table;
    uint64_t offset_list       = offset_lock_var   + size_lock_var;
    uint64_t offset_elem       = offset_list       + size_list;
    uint64_t size_buf          = offset_elem       + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return;
    acp_ga_t buf_directory  = buf + offset_directory;
    acp_ga_t buf_directory2 = buf + offset_directory2;
    acp_ga_t buf_table      = buf + offset_table;
    acp_ga_t buf_lock_var   = buf + offset_lock_var;
    acp_ga_t buf_list       = buf + offset_list;
    acp_ga_t buf_elem       = buf + offset_elem;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* table      = (volatile uint64_t*)(ptr + offset_table);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem       = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** move set2 to a new temporal set ***/
    
    acp_copy(buf_directory2, set2.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < set2.num_ranks; rank++) {
        acp_ga_t ga_table = acp_malloc(size_table, acp_query_rank(directory2[rank]));
        if (ga_table == ACP_GA_NULL) {
            while (rank > 0) acp_free(directory[--rank]);
            acp_free(buf);
            return;
        }
        directory[rank] = ga_table;
    }
    
    for (rank = 0; rank < set2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        for (slot = 0; slot < set2.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            table[slot * 4] = 0;
            table[slot * 4 + 1] = list[0];
            table[slot * 4 + 2] = list[1];
            table[slot * 4 + 3] = list[2];
            list[0] = ACP_GA_NULL;
            list[1] = ACP_GA_NULL;
            list[2] = 0;
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
        acp_copy(directory[rank], buf_table, size_table, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_multiset_t set;
    set.ga = buf_directory;
    set.num_ranks = set2.num_ranks;
    set.num_slots = set2.num_slots;
    
    /*** move set1 to set2 ***/
    
    acp_move_multiset(set2, set1);
    
    /*** move temporal set to set1 ***/
    
    acp_move_multiset(set1, set);
    
    /*** destroy temporal set ***/
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            iacp_clear_list_multiset(list, buf_elem, elem);
        }
        acp_free(ga_table);
    }
    
    acp_free(buf);
    
    return;
}

acp_element_t acp_dereference_multiset_it(acp_multiset_it_t it)
{
    acp_element_t key;
    key.ga    = ACP_GA_NULL;
    key.size  = 0;
    if (it.rank >= it.set.num_ranks || it.slot >= it.set.num_slots || it.elem == ACP_GA_NULL) return key;
    
    /*** local buffer variables ***/
    
    uint64_t size_elem      = 32;
    uint64_t offset_elem      = 0;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return key;
    acp_ga_t buf_elem      = buf + offset_elem;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** translate element into pair ***/
    
    acp_copy(buf_elem, it.elem, size_elem, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t key_size = elem[3];
    acp_free(buf);
    
    key.ga    = it.elem + size_elem;
    key.size  = key_size;
    return key;
}

acp_multiset_it_t acp_increment_multiset_it(acp_multiset_it_t it)
{
    if (it.rank >= it.set.num_ranks || it.slot >= it.set.num_slots || it.elem == ACP_GA_NULL) return iacp_null_multiset_it(it.set);
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_multiset_it(it.set);
    acp_ga_t buf_directory = buf + offset_directory;
//  acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
//  volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    int rank = it.rank;
    int slot = it.slot;
    
    /*** search next element ***/
    
    acp_copy(buf_elem, it.elem, size_elem, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (elem[0] != ACP_GA_NULL) {
        it.elem = elem[0];
        acp_free(buf);
        return it;
    }
    
    acp_ga_t ga_directory = it.set.ga + rank * 8;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_ga_t ga_lock_var = *directory + slot * 32;
    acp_ga_t ga_list = ga_lock_var + 8;
    
    while (rank + 1 < it.set.num_ranks) {
        while (slot + 1 < it.set.num_slots) {
            slot++;
            ga_lock_var += 32;
            ga_list += 32;
            acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            
            if (list[2] > 0) {
                it.rank = rank;
                it.slot = slot;
                it.elem = list[0];
                acp_free(buf);
                return it;
            }
        }
        
        rank++;
        slot = 0;
        ga_directory += 8;
        acp_copy(buf_directory, ga_directory, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        ga_lock_var = *directory;
        ga_list = ga_lock_var + 8;
        
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        
        if (list[2] > 0) {
            it.rank = rank;
            it.slot = slot;
            it.elem = list[0];
            acp_free(buf);
            return it;
        }
    }
    
    while (slot < it.set.num_slots - 1) {
        slot++;
        ga_lock_var += 32;
        ga_list += 32;
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        
        if (list[2] > 0) {
            it.rank = rank;
            it.slot = slot;
            it.elem = list[0];
            acp_free(buf);
            return it;
        }
    }
    
    acp_free(buf);
    return iacp_null_multiset_it(it.set);
}

