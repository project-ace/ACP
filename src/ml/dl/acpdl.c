/*
 * ACP Middle Layer: Data Library
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
#include <string.h>
#include <sys/types.h>
#include <acp.h>
#include "acpdl.h"

size_t iacp_starter_memory_size_dl = 64 * 1024 * 1024;

uint64_t crc64_table[256];

int iacp_init_dl(void)
{
    uint64_t c;
    int n, k;
    
    iacpdl_init_malloc();
    
    for (n = 0; n < 256; n++) {
        c = (uint64_t)n;
        for (k = 0; k < 8; k++) c = (c & 1) ? (0xC96C5795D7870F42ULL ^ (c >> 1)) : (c >> 1);
        crc64_table[n] = c;
    }
    
    return 0;
}

int iacp_finalize_dl(void)
{
    iacpdl_finalize_malloc();
    
    return 0;
}

void iacp_abort_dl(void)
{
    return;
}

static uint64_t crc64(const void* ptr, size_t size)
{
    uint64_t c;
    unsigned char* p;
    
    c = 0xFFFFFFFFFFFFFFFFULL;
    p = (unsigned char*)ptr;
    
    while (size--) c = crc64_table[(c ^ *p++) & 0xFF] ^ (c >> 8);
    
    return c ^ 0xFFFFFFFFFFFFFFFFULL;
}

/** Vector
 *      [0]  ga of array body
 *      [1]  elsize
 *      [2]  nelem
 *      [3]  nmax
 */

typedef struct {
    acp_ga_t ga;
    uint64_t elsize;
    uint64_t nelem;
    uint64_t nmax;
} acp_vector_ctrl_t;

/* basic oepration functions */
acp_vector_t acp_create_vector(size_t nelem, size_t size, int rank)
{
    acp_vector_t vector;
    acp_ga_t buf;
    volatile uint64_t* tmp;
    
    vector.ga = ACP_GA_NULL;
    
    if (nelem == 0 || size == 0) return vector;
    
    buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return vector;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    vector.ga = acp_malloc(32, rank);
    if (vector.ga == ACP_GA_NULL) {
        acp_free(buf);
        return vector;
    }
    
    tmp[0] = acp_malloc(size * nelem, rank);
    if (tmp[0] == ACP_GA_NULL) {
        acp_free(vector.ga);
        acp_free(buf);
        vector.ga = ACP_GA_NULL;
        return vector;
    }
    tmp[1] = size;
    tmp[2] = nelem;
    tmp[3] = nelem;
    acp_copy(vector.ga, buf, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return vector;
}

void acp_clear_vector(acp_vector_t vector)
{
    acp_ga_t buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    tmp[0] = 0;
    
    acp_copy(vector.ga + 16, buf, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_destroy_vector(acp_vector_t vector)
{
    acp_ga_t buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, vector.ga, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(tmp[0]);
    acp_free(vector.ga);
    acp_free(buf);
    return;
}

acp_vector_t acp_duplicate_vector(acp_vector_t vector, int rank)
{
    acp_vector_t new_vector;
    size_t size, nelem, nmax;
    acp_ga_t buf, ga;
    volatile uint64_t* tmp;
    
    new_vector.ga = ACP_GA_NULL;
    
    buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return new_vector;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, vector.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    ga = tmp[0];
    size = tmp[1];
    nelem = tmp[2];
    nmax = tmp[3];
    
    new_vector.ga = acp_malloc(32, rank);
    if (new_vector.ga == ACP_GA_NULL) {
        acp_free(buf);
        return new_vector;
    }
    
    tmp[0] = acp_malloc(size * nmax, rank);
    if (tmp[0] == ACP_GA_NULL) {
        acp_free(new_vector.ga);
        acp_free(buf);
        new_vector.ga = ACP_GA_NULL;
        return new_vector;
    }
    
    acp_copy(new_vector.ga, buf, 32, ACP_HANDLE_NULL);
    acp_copy(tmp[0], ga, size * nmax, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return new_vector;
}

void acp_swap_vector(acp_vector_t v1, acp_vector_t v2)
{
    acp_ga_t buf, buf_v1, buf_v2, old1, old2, new1, new2;
    uint64_t size1, size2, num1, num2;
    volatile uint64_t* tmp_v1;
    volatile uint64_t* tmp_v2;
    
    buf = acp_malloc(32 + 32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp_v1 = (volatile uint64_t*)acp_query_address(buf_v1 = buf);
    tmp_v2 = (volatile uint64_t*)acp_query_address(buf_v2 = buf + 32);
    
    acp_copy(buf_v1, v1.ga, 32, ACP_HANDLE_NULL);
    acp_copy(buf_v2, v2.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    old1  = tmp_v1[0];
    size1 = tmp_v1[1];
    num1  = tmp_v1[2];
    old2  = tmp_v2[0];
    size2 = tmp_v2[1];
    num2  = tmp_v2[2];
    
    new1 = acp_malloc(size2 * num2, acp_query_rank(v1.ga));
    if (new1 == ACP_GA_NULL) {
        acp_free(buf);
        return;
    }
    new2 = acp_malloc(size1 * num1, acp_query_rank(v2.ga));
    if (new2 == ACP_GA_NULL) {
        acp_free(new1);
        acp_free(buf);
        return;
    }
    
    tmp_v1[0] = new1;
    tmp_v1[1] = size2;
    tmp_v1[2] = num2;
    tmp_v1[3] = num2;
    tmp_v2[0] = new2;
    tmp_v2[1] = size1;
    tmp_v2[2] = num1;
    tmp_v2[3] = num1;
    acp_copy(v1.ga, buf_v1, 32, ACP_HANDLE_NULL);
    acp_copy(v2.ga, buf_v2, 32, ACP_HANDLE_NULL);
    acp_copy(new1, old2, num2 * size2, ACP_HANDLE_NULL);
    acp_copy(new2, old1, num1 * size1, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(old2);
    acp_free(old1);
    acp_free(buf);
    return;
}

void acp_insert_vector(acp_vector_t vector, acp_vector_it_t it)
{
    return;
}

acp_vector_it_t acp_erase_vector(acp_vector_t vector, acp_vector_it_t it)
{
    return it;
}

void acp_push_back_vector(acp_vector_t vector, void* ptr)
{
    return;
}

void acp_pop_back_vector(acp_vector_t vector)
{
    return;
}

/* element reference functions */
acp_ga_t acp_element_vector(acp_vector_t vector, acp_vector_it_t it)
{
    return vector.ga;
}

acp_ga_t acp_front_vector(acp_vector_t vector)
{
    return vector.ga;
}

acp_ga_t acp_back_vector(acp_vector_t vector)
{
    return vector.ga;
}


/* iterator functions */
acp_vector_it_t acp_begin_vector(acp_vector_t vector)
{
    acp_vector_it_t it;
    
    it.vector = vector;
    it.index = 0;
    return it;
}

acp_vector_it_t acp_end_vector(acp_vector_t vector)
{
    acp_vector_it_t it;
    
    it.vector = vector;
    it.index = 0;
    return it;
}

acp_vector_it_t acp_rbegin_vector(acp_vector_t vector)
{
    acp_vector_it_t it;
    
    it.vector = vector;
    it.index = 0;
    return it;
}

acp_vector_it_t acp_rend_vector(acp_vector_t vector)
{
    acp_vector_it_t it;
    
    it.vector = vector;
    it.index = 0;
    return it;
}

acp_vector_it_t acp_increment_vector(acp_vector_it_t it)
{
    it.index++;
    return it;
}

acp_vector_it_t acp_decrement_vector(acp_vector_it_t it)
{
    it.index--;
    return it;
}


/* misc. functions */
int acp_max_size_vector(acp_vector_t vector)
{
    return 0;
}

int acp_empty_vector(acp_vector_t vector)
{
    return 0;
}

int acp_equal_vector(acp_vector_t v1, acp_vector_t v2)
{
    return 0;
}

int acp_not_equal_vector(acp_vector_t v1, acp_vector_t v2)
{
    return 0;
}

int acp_less_vector(acp_vector_t v1, acp_vector_t v2)
{
    return 0;
}

int acp_greater_vector(acp_vector_t v1, acp_vector_t v2)
{
    return 0;
}

int acp_less_or_equal_vector(acp_vector_t v1, acp_vector_t v2)
{
    return 0;
}

int acp_greater_or_equal_vector(acp_vector_t v1, acp_vector_t v2)
{
    return 0;
}

/** List
 *      [0]  ga of head element
 *      [1]  ga of tail element
 *      [2]  number of elements
 ** List Element
 *      [0]  ga of next element
 *      [1]  ga of previos element
 *      [2]  size of this element
 *      [3-] value
 */

acp_list_t acp_create_list(int rank)
{
    acp_list_t list = { ACP_GA_NULL };
    acp_ga_t buf;
    volatile uint64_t* tmp_list;
    
    buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return list;
    tmp_list = (volatile uint64_t*)acp_query_address(buf);
    
    list.ga = acp_malloc(24, rank);
    if (list.ga == ACP_GA_NULL) {
        acp_free(buf);
        return list;
    }
    
    tmp_list[0] = ACP_GA_NULL;
    tmp_list[1] = ACP_GA_NULL;
    tmp_list[2] = 0;
    acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return list;
}

void acp_clear_list(acp_list_t list)
{
    acp_ga_t buf, ga;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, list.ga, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    ga = *tmp;
    
    while (ga != ACP_GA_NULL) {
        acp_copy(buf, ga, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(ga);
        ga = *tmp;
    }
    
    acp_free(buf);
    return;
}

void acp_destroy_list(acp_list_t list)
{
    acp_clear_list(list);
    acp_free(list.ga);
    return;
}

acp_list_it_t acp_insert_list(acp_list_it_t it, const void* ptr, size_t size, int rank)
{
    acp_ga_t buf, buf_list, buf_next, buf_link, buf_elem, elem;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_next;
    volatile uint64_t* tmp_link;
    volatile uint64_t* tmp_elem;
    
    if (it.elem == it.list.ga) {
        /* insert before end iterator */
        return acp_push_back_list(it.list, ptr, size, rank);
    }
    
    buf = acp_malloc(24 + 24 + 8 + 24 + size, acp_rank());
    if (buf == ACP_GA_NULL) {
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    tmp_list = (volatile uint64_t*)acp_query_address(buf_list = buf);
    tmp_next = (volatile uint64_t*)acp_query_address(buf_next = buf + 24);
    tmp_link = (volatile uint64_t*)acp_query_address(buf_link = buf + 24 + 24);
    tmp_elem = (volatile uint64_t*)acp_query_address(buf_elem = buf + 24 + 24 + 8);
    
    elem = acp_malloc(24 + size, rank);
    if (elem == ACP_GA_NULL) {
        acp_free(buf);
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    acp_copy(buf_list, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf_next, it.elem, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    tmp_elem[0] = it.elem;
    tmp_elem[1] = tmp_next[1];
    tmp_elem[2] = size;
    memcpy((void*)tmp_elem + 24, ptr, size);
    acp_copy(elem, buf_elem, 24 + size, ACP_HANDLE_NULL);
    
    tmp_link[0] = elem;
    if (tmp_next[1] == ACP_GA_NULL) {
        tmp_list[0] = elem;
    } else {
        acp_copy(tmp_next[1], buf_link, 8, ACP_HANDLE_NULL);
    }
    acp_copy(it.elem + 8, buf_link, 8, ACP_HANDLE_NULL);
    
    tmp_list[2]++;
    acp_copy(it.list.ga, buf_list, 24, ACP_HANDLE_NULL);
    
    acp_complete(ACP_HANDLE_ALL);
    
    it.elem = elem;
    
    acp_free(buf);
    return it;
}

acp_list_it_t acp_erase_list(acp_list_it_t it)
{
    acp_ga_t buf, buf_list, buf_elem;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_elem;
    
    /* if the iterator is null or end, it fails */
    if (it.elem == ACP_GA_NULL || it.elem == it.list.ga) {
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    buf = acp_malloc(24 + 24, acp_rank());
    if (buf == ACP_GA_NULL) {
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    tmp_list = (volatile uint64_t*)acp_query_address(buf_list = buf);
    tmp_elem = (volatile uint64_t*)acp_query_address(buf_elem = buf + 24);
    
    acp_copy(buf_list, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf_elem, it.elem, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (tmp_list[2] == 1 && tmp_list[0] == it.elem && tmp_list[1] == it.elem) {
        /* the only element is erased */
        tmp_list[0] = ACP_GA_NULL;
        tmp_list[1] = ACP_GA_NULL;
        tmp_list[2] = 0;
        acp_copy(it.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it.elem);
        it.elem = it.list.ga;
    } else if (tmp_list[2] > 1 && tmp_list[0] == it.elem ) {
        /* the head element is erased */
        tmp_list[0] = tmp_elem[0];
        tmp_list[2]--;
        acp_copy(it.list.ga, buf_list, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_elem[0] + 8, buf_elem + 8, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it.elem);
        it.elem = tmp_elem[0];
    } else if (tmp_list[2] > 1 && tmp_list[1] == it.elem) {
        /* the tail element is erased and it returns an end iterator */
        tmp_list[1] = tmp_elem[1];
        tmp_list[2]--;
        acp_copy(it.list.ga, buf_list, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_elem[1], buf_elem, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it.elem);
        it.elem = it.list.ga;
    } else if (tmp_list[2] > 1 && it.elem != it.list.ga && it.elem != ACP_GA_NULL) {
        /* an intermediate element is erased and it returns an iterator to the next element */
        tmp_list[2]--;
        acp_copy(it.list.ga, buf_list, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_elem[0] + 8, buf_elem + 8, 8, ACP_HANDLE_NULL);
        acp_copy(tmp_elem[1], buf_elem, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it.elem);
        it.elem = tmp_elem[0];
    } else
        it.elem = ACP_GA_NULL;
    
    acp_free(buf);
    return it;
}

acp_list_it_t acp_push_back_list(acp_list_t list, const void* ptr, size_t size, int rank)
{
    acp_list_it_t it;
    acp_ga_t buf, buf_list, buf_elem, prev;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_elem;
    
    it.list = list;
    it.elem = ACP_GA_NULL;
    
    buf = acp_malloc(24 + 24 + size, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    tmp_list = (volatile uint64_t*)acp_query_address(buf_list = buf);
    tmp_elem = (volatile uint64_t*)acp_query_address(buf_elem = buf + 24);
    
    it.elem = acp_malloc(24 + size, rank);
    if (it.elem == ACP_GA_NULL) {
        acp_free(buf);
        return it;
    }
    
    acp_copy(buf_list, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (tmp_list[2] == 0) {
        tmp_elem[0] = ACP_GA_NULL;
        tmp_elem[1] = ACP_GA_NULL;
        tmp_elem[2] = size;
        memcpy((void*)tmp_elem + 24, ptr, size);
        acp_copy(it.elem, buf_elem, 24 + size, ACP_HANDLE_NULL);
        tmp_list[0] = it.elem;
        tmp_list[1] = it.elem;
        tmp_list[2] = 1;
        acp_copy(it.list.ga, buf_list, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        prev = tmp_list[1];
        tmp_elem[0] = ACP_GA_NULL;
        tmp_elem[1] = prev;
        tmp_elem[2] = size;
        memcpy((void*)tmp_elem + 24, ptr, size);
        acp_copy(it.elem, buf_elem, 24 + size, ACP_HANDLE_NULL);
        tmp_list[1] = it.elem;
        tmp_list[2]++;
        acp_copy(prev, buf_list + 8, 8, ACP_HANDLE_NULL);
        acp_copy(it.list.ga + 8, buf_list + 8, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    acp_free(buf);
    return it;
}

/* iterator functions */
acp_list_it_t acp_begin_list(acp_list_t list)
{
    acp_list_it_t it;
    acp_ga_t buf;
    volatile uint64_t* tmp_list;
    
    it.list = list;
    
    buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) {
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    tmp_list = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    it.elem = (tmp_list[2] == 0) ? it.list.ga : tmp_list[0];
    
    acp_free(buf);
    return it;
}

acp_list_it_t acp_end_list(acp_list_t list)
{
    acp_list_it_t it;
    
    it.list = list;
    it.elem = list.ga;
    return it;
}

acp_list_it_t acp_increment_list(acp_list_it_t it)
{
    acp_ga_t buf;
    volatile uint64_t* tmp_elem;
    
    if (it.elem == it.list.ga) {
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) {
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    tmp_elem = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, it.elem, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    it.elem = (tmp_elem[0] == ACP_GA_NULL) ? it.list.ga : tmp_elem[0];
    
    acp_free(buf);
    return it;
}

acp_list_it_t acp_decrement_list(acp_list_it_t it)
{
    acp_ga_t buf;
    volatile uint64_t* tmp_elem;
    
    buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) {
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    tmp_elem = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, it.elem, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    it.elem = tmp_elem[1];
    
    acp_free(buf);
    return it;
}

/** Map
 *      [0-]  table of distributed hash tables
 ** Map Bucket (= List control): element of hash table
 *      [0]  ga of head element
 *      [1]  ga of tail element
 *      [2]  number of elements
 ** Map Pair (= List element)
 *      [0]  ga of next element
 *      [1]  ga of previos element
 *      [2]  size of this element
 *      [3]  size of key
 *      [4-] key + value
 */

acp_map_t acp_create_map(int num_ranks, const int* ranks, int num_slots, int rank)
{
    acp_map_t map;
    uint64_t num, size_map, size_slots;
    acp_ga_t buf, buf_map, buf_slots;
    volatile uint64_t* tmp_map;
    volatile uint64_t* tmp_slots;
    int i, j;
    
    map.ga = ACP_GA_NULL;
    map.num_ranks = num_ranks;
    map.num_slots = num_slots;
    
    size_map = num_ranks * 8;
    size_slots = num_slots * 24;
    
    buf = acp_malloc(size_map + size_slots, acp_rank());
    if (buf == ACP_GA_NULL) return map;
    tmp_map   = (volatile uint64_t*)acp_query_address(buf_map   = buf);
    tmp_slots = (volatile uint64_t*)acp_query_address(buf_slots = buf + size_map);
    
    for (i = 0; i < num_slots; i++) {
        tmp_slots[i * 3    ] = ACP_GA_NULL;
        tmp_slots[i * 3 + 1] = ACP_GA_NULL;
        tmp_slots[i * 3 + 2] = 0;
    }
    
    map.ga = acp_malloc(size_map, rank);
    if (map.ga == ACP_GA_NULL) {
        acp_free(buf);
        return map;
    }
    
    for (i = 0; i < map.num_ranks; i++) {
        tmp_map[i] = acp_malloc(size_slots, ranks[i]);
        if (tmp_map[i] == ACP_GA_NULL) {
            for (j = i - 1; j >= 0; j--) acp_free(tmp_map[j]);
            acp_free(map.ga);
            acp_free(buf);
            map.ga = ACP_GA_NULL;
            return map;
        }
        acp_copy(tmp_map[i], buf_slots, size_slots, ACP_HANDLE_NULL);
    }
    acp_copy(map.ga, buf_map, size_map, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return map;
}

void acp_clear_map(acp_map_t map)
{
    acp_list_t list;
    uint64_t num, size_map;
    acp_ga_t buf;
    volatile uint64_t* tmp_map;
    int i, j;
    
    size_map = map.num_ranks * 8;
    
    buf = acp_malloc(size_map, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp_map = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, map.ga, size_map, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    for (i = 0; i < map.num_ranks; i++)
        for (j = 0; j < map.num_slots; j++) {
            list.ga = tmp_map[i] + j * 24;
            acp_clear_list(list);
        }
    
    acp_free(buf);
    return;
}

void acp_destroy_map(acp_map_t map)
{
    acp_list_t list;
    uint64_t num, size_map;
    acp_ga_t buf;
    volatile uint64_t* tmp_map;
    int i, j;
    
    size_map = map.num_ranks * 8;
    
    buf = acp_malloc(size_map, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp_map = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, map.ga, size_map, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    for (i = 0; i < map.num_ranks; i++) {
        for (j = 0; j < map.num_slots; j++) {
            list.ga = tmp_map[i] + j * 24;
            acp_clear_list(list);
        }
        acp_free(tmp_map[i]);
    }
    
    acp_free(map.ga);
    acp_free(buf);
    return;
}

acp_map_ib_t acp_insert_map(acp_map_t map, const void* key, size_t size_key, const void* value, size_t size_value)
{
    acp_list_it_t it;
    uint64_t c;
    acp_map_ib_t ib;
    acp_ga_t buf, buf_table, buf_list, buf_elem, prev_ga;
    volatile uint64_t* tmp_table;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_elem;
    unsigned char* p1;
    unsigned char* p2;
    int i;
    
    c = crc64(key, size_key) >> 16;
    
    ib.it.map = map;
    ib.it.rank = (c / ib.it.map.num_slots) % ib.it.map.num_ranks;
    ib.it.slot = c % ib.it.map.num_slots;
    ib.it.elem = ACP_GA_NULL;
    ib.success = 0;
    
    buf = acp_malloc(8 + 24 + 32 + size_key + size_value, acp_rank());
    if (buf == ACP_GA_NULL) return ib;
    tmp_table = (volatile uint64_t*)acp_query_address(buf_table = buf);
    tmp_list  = (volatile uint64_t*)acp_query_address(buf_list  = buf + 8);
    tmp_elem  = (volatile uint64_t*)acp_query_address(buf_elem  = buf + 8 + 24);
    
    acp_copy(buf_table, ib.it.map.ga + ib.it.rank * 8, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    it.list.ga = tmp_table[0] + ib.it.slot * 24;
    
    acp_copy(buf_list, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    it.elem = tmp_list[0];
    
    while (it.elem != ACP_GA_NULL) {
        acp_copy(buf_elem, it.elem, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        if (tmp_elem[3] == size_key) {
            acp_copy(buf_elem + 32, it.elem + 32, size_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            p1 = (unsigned char*)(tmp_elem + 4);
            p2 = (unsigned char*)key;
            for (i = 0; i < size_key; i++) if (*p1++ != *p2++) break;
            if (i == size_key) {
                acp_free(buf);
                return ib;
            }
        }
        it.elem = tmp_elem[0];
    }
    
    ib.it.elem = acp_malloc(32 + size_key + size_value, acp_query_rank(it.list.ga));
    if (ib.it.elem == ACP_GA_NULL) {
        acp_free(buf);
        return ib;
    }
    
    if (tmp_list[2] == 0) {
        tmp_elem[0] = ACP_GA_NULL;
        tmp_elem[1] = ACP_GA_NULL;
        tmp_elem[2] = 8 + size_key + size_value;
        tmp_elem[3] = size_key;
        tmp_list[0] = ib.it.elem;
        tmp_list[1] = ib.it.elem;
        tmp_list[2] = 1;
    } else {
        tmp_elem[0] = ACP_GA_NULL;
        tmp_elem[1] = tmp_list[1];
        tmp_elem[2] = 8 + size_key + size_value;
        tmp_elem[3] = size_key;
        
        prev_ga = tmp_list[1];
        
        tmp_list[1] = ib.it.elem;
        tmp_list[2]++;
        
        acp_copy(prev_ga, buf_list + 8, 8, ACP_HANDLE_NULL);
    }
    memcpy((void*)(tmp_elem + 4)           , key  , size_key);
    memcpy((void*)(tmp_elem + 4) + size_key, value, size_value);
    
    acp_copy(ib.it.elem, buf_elem, 32 + size_key + size_value, ACP_HANDLE_NULL);
    acp_copy(it.list.ga, buf_list, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    ib.success = 1;
    return ib;
}

acp_map_it_t acp_find_map(acp_map_t map, const void* key, size_t size_key)
{
    acp_list_it_t it;
    uint64_t c;
    acp_map_it_t map_it;
    acp_ga_t buf, buf_table, buf_list, buf_elem;
    volatile uint64_t* tmp_table;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_elem;
    unsigned char* p1;
    unsigned char* p2;
    int i;
    
    c = crc64(key, size_key) >> 16;
    
    map_it.map = map;
    map_it.rank = (c / map_it.map.num_slots) % map_it.map.num_ranks;
    map_it.slot = c % map_it.map.num_slots;
    map_it.elem = ACP_GA_NULL;
    
    buf = acp_malloc(8 + 24 + 32 + size_key, acp_rank());
    if (buf == ACP_GA_NULL) return map_it;
    tmp_table = (volatile uint64_t*)acp_query_address(buf_table = buf);
    tmp_list  = (volatile uint64_t*)acp_query_address(buf_list  = buf + 8);
    tmp_elem  = (volatile uint64_t*)acp_query_address(buf_elem  = buf + 8 + 24);
    
    acp_copy(buf_table, map_it.map.ga + map_it.rank * 8, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    it.list.ga = tmp_table[0] + map_it.slot * 24;
    
    acp_copy(buf_list, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    it.elem = tmp_list[0];
    
    while (it.elem != ACP_GA_NULL) {
        acp_copy(buf_elem, it.elem, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        if (tmp_elem[3] == size_key) {
            acp_copy(buf_elem + 32, it.elem + 32, size_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            p1 = (unsigned char*)(tmp_elem + 4);
            p2 = (unsigned char*)key;
            for (i = 0; i < size_key; i++) if (*p1++ != *p2++) break;
            if (i == size_key) {
                acp_free(buf);
                return map_it;
            }
        }
        it.elem = tmp_elem[0];
    }
    
    map_it.rank = map_it.map.num_ranks;
    map_it.elem = it.list.ga;
    acp_free(buf);
    return map_it;
}

