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

uint64_t iacpdl_crc64(const void* ptr, size_t size)
{
    uint64_t c;
    unsigned char* p;
    
    c = 0xFFFFFFFFFFFFFFFFULL;
    p = (unsigned char*)ptr;
    
    while (size--) c = crc64_table[(c ^ *p++) & 0xFF] ^ (c >> 8);
    
    return c ^ 0xFFFFFFFFFFFFFFFFULL;
}

/** Vector
 *      [00:07]  ga of array body
 *      [08:16]  size
 *      [16:23]  max
 */

//extern void acp_assign_vector(acp_vector_t vector1, acp_vector_t vector2);
//extern void acp_assign_range_vector(acp_vector_t vector, acp_vector_it_t start, acp_vector_it_t end);
//extern acp_ga_t acp_at_vector(acp_vector_t vector, int offset);
//extern acp_vector_it_t acp_back_vector(acp_vector_t vector);
//extern acp_vector_it_t acp_begin_vector(acp_vector_t vector);
//extern size_t acp_capacity_vector(acp_vector_t vector);

void acp_clear_vector(acp_vector_t vector)
{
    acp_ga_t buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile uint64_t* vector_size = (volatile uint64_t*)ptr;
    
    *vector_size = 0;
    
    acp_copy(vector.ga + 8, buf, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

acp_vector_t acp_create_vector(size_t size, int rank)
{
    acp_vector_t vector;
    vector.ga = ACP_GA_NULL;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return vector;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* vector_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* vector_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* vector_max  = (volatile uint64_t*)(ptr + 16);
    
    vector.ga = acp_malloc(24, rank);
    if (vector.ga == ACP_GA_NULL) {
        acp_free(buf);
        return vector;
    }
    
    *vector_ga = acp_malloc(size, rank);
    if (*vector_ga == ACP_GA_NULL) {
        acp_free(vector.ga);
        acp_free(buf);
        vector.ga = ACP_GA_NULL;
        return vector;
    }
    *vector_size = size;
    *vector_max  = size;
    
    acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return vector;
}

void acp_destroy_vector(acp_vector_t vector)
{
    acp_ga_t buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* vector_ga = (volatile acp_ga_t*)ptr;
    
    acp_copy(buf, vector.ga, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(*vector_ga);
    acp_free(vector.ga);
    acp_free(buf);
    return;
}

//extern int acp_empty_vector(acp_vector_t vector);
//extern acp_vector_it_t acp_end_vector(acp_vector_t vector);
//extern acp_vector_it_t acp_erase_vector(acp_vector_it_t it, size_t size);
//extern acp_vector_it_t acp_erase_range_vector(acp_vector_it_t start, acp_vector_it_t end);
//extern acp_vector_it_t acp_front_vector(acp_vector_t vector);
//extern acp_vector_it_t acp_insert_vector(acp_vector_it_t it, const acp_ga_t ga, size_t size);
//extern acp_vector_it_t acp_insert_range_vector(acp_vector_it_t it, acp_vector_it_t start, acp_vector_it_t end);
//extern void acp_pop_back_vector(acp_vector_t vector, size_t size);
//extern void acp_push_back_vector(acp_vector_t vector, const acp_ga_t ga, size_t size);
//extern void acp_reserve_vector(acp_vector_t vector, size_t size);
//extern size_t acp_size_vector(acp_vector_t vector);

void acp_swap_vector(acp_vector_t vector1, acp_vector_t vector2)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* vector1_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* vector1_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* vector1_max  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* vector2_ga   = (volatile acp_ga_t*)(ptr + 24);
    volatile uint64_t* vector2_size = (volatile uint64_t*)(ptr + 32);
    volatile uint64_t* vector2_max  = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf,      vector2.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, vector1.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t old_ga1 = *vector2_ga;
    acp_ga_t old_ga2 = *vector1_ga;
    
    *vector1_ga = ACP_GA_NULL;
    *vector2_ga = ACP_GA_NULL;
    *vector1_max = *vector1_size;
    *vector2_max = *vector2_size;
    
    if (*vector1_size > 0) {
        *vector1_ga = acp_malloc(*vector1_size, acp_query_rank(vector1.ga));
        if (*vector1_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
    }
    if (*vector2_size > 0) {
        *vector2_ga = acp_malloc(*vector2_size, acp_query_rank(vector2.ga));
        if (*vector2_ga == ACP_GA_NULL) {
            acp_free(*vector1_ga);
            acp_free(buf);
            return;
        }
    }
    
    if (*vector1_size > 0)
        acp_copy(*vector1_ga, old_ga2, *vector1_size, ACP_HANDLE_NULL);
    if (*vector2_size > 0)
        acp_copy(*vector2_ga, old_ga1, *vector2_size, ACP_HANDLE_NULL);
    
    acp_copy(vector1.ga, buf,      24, ACP_HANDLE_NULL);
    acp_copy(vector2.ga, buf + 24, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (old_ga1 != ACP_GA_NULL) acp_free(old_ga1);
    if (old_ga2 != ACP_GA_NULL) acp_free(old_ga2);
    acp_free(buf);
    return;
}

//extern acp_vector_it_t acp_advance_vector_it(acp_vector_it_t it, int n);
//extern acp_ga_t acp_dereference_vector_it(acp_vector_it_t it);
//extern int acp_distance_vector_it(acp_vector_it_t first, acp_vector_it_t last);

/** Deque
 */

//extern void acp_assign_deque(acp_deque_t deque1, acp_deque_t deque2);
//extern void acp_assign_range_deque(acp_deque_t deque, acp_deque_it_t start, acp_deque_it_t end);
//extern acp_ga_t acp_at_deque(acp_deque_t deque, int offset);
//extern acp_deque_it_t acp_back_deque(acp_deque_t deque);
//extern acp_deque_it_t acp_begin_deque(acp_deque_t deque);
//extern size_t acp_capacity_deque(acp_deque_t deque);
//extern void acp_clear_deque(acp_deque_t deque);
//extern acp_deque_t acp_create_deque(size_t size, int rank);
//extern void acp_destroy_deque(acp_deque_t deque);
//extern int acp_empty_deque(acp_deque_t deque);
//extern acp_deque_it_t acp_end_deque(acp_deque_t deque);
//extern acp_deque_it_t acp_erase_deque(acp_deque_it_t it, size_t size);
//extern acp_deque_it_t acp_erase_range_deque(acp_deque_it_t start, acp_deque_it_t end);
//extern acp_deque_it_t acp_front_deque(acp_deque_t deque);
//extern acp_deque_it_t acp_insert_deque(acp_deque_it_t it, const acp_ga_t ga, size_t size);
//extern acp_deque_it_t acp_insert_range_deque(acp_deque_it_t it, acp_deque_it_t start, acp_deque_it_t end);
//extern void acp_pop_back_deque(acp_deque_t deque, size_t size);
//extern void acp_pop_front_deque(acp_deque_t deque, size_t size);
//extern void acp_push_back_deque(acp_deque_t deque, const acp_ga_t ga, size_t size);
//extern void acp_push_front_deque(acp_deque_t deque, const acp_ga_t ga, size_t size);
//extern void acp_reserve_deque(acp_deque_t deque, size_t size);
//extern size_t acp_size_deque(acp_deque_t deque);
//extern void acp_swap_deque(acp_deque_t deque1, acp_deque_t deque2);
//extern acp_deque_it_t acp_advance_deque_it(acp_deque_it_t it, int n);
//extern acp_ga_t acp_dereference_deque_it(acp_deque_it_t it);
//extern int acp_distance_deque_it(acp_deque_it_t first, acp_deque_it_t last);

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

//extern void acp_assign_list(acp_list_t list1, acp_list_t list2);
//extern void acp_assign_range_list(acp_list_t list, acp_list_it_t start, acp_list_it_t end);
//extern acp_list_it_t acp_back_list(acp_list_t list);

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

void acp_destroy_list(acp_list_t list)
{
    acp_clear_list(list);
    acp_free(list.ga);
    return;
}

//extern int acp_empty_list(acp_list_t list);

acp_list_it_t acp_end_list(acp_list_t list)
{
    acp_list_it_t it;
    
    it.list = list;
    it.elem = list.ga;
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

//extern acp_list_it_t acp_erase_range_list(acp_list_it_t start, acp_list_it_t end);
//extern acp_list_it_t acp_front_list(acp_list_t list);

acp_list_it_t acp_insert_list(acp_list_it_t it, const acp_ga_t ga, size_t size, int rank)
{
    acp_ga_t buf, buf_list, buf_next, buf_link, buf_elem, elem;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_next;
    volatile uint64_t* tmp_link;
    volatile uint64_t* tmp_elem;
    
    if (it.elem == it.list.ga) {
        /* insert before end iterator */
        acp_push_back_list(it.list, ga, size, rank);
        return it;
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
    acp_copy(elem, buf_elem, 24, ACP_HANDLE_NULL);
    acp_copy(elem + 24, ga, size, ACP_HANDLE_NULL);
    
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

//extern acp_list_it_t acp_insert_range_list(acp_list_it_t it, acp_list_it_t start, acp_list_it_t end);
//extern void acp_merge_list(acp_list_t list1, acp_list_t list2, int (*comp)(const acp_list_it_t it1, const acp_list_it_t it2));
//extern void acp_pop_back_list(acp_list_t list);
//extern void acp_pop_front_list(acp_list_t list);

void acp_push_back_list(acp_list_t list, const acp_ga_t ga, size_t size, int rank)
{
    acp_list_it_t it;
    acp_ga_t buf, buf_list, buf_elem, prev;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_elem;
    
    it.list = list;
    it.elem = ACP_GA_NULL;
    
    buf = acp_malloc(24 + 24 + size, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp_list = (volatile uint64_t*)acp_query_address(buf_list = buf);
    tmp_elem = (volatile uint64_t*)acp_query_address(buf_elem = buf + 24);
    
    it.elem = acp_malloc(24 + size, rank);
    if (it.elem == ACP_GA_NULL) {
        acp_free(buf);
        return;
    }
    
    acp_copy(buf_list, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (tmp_list[2] == 0) {
        tmp_elem[0] = ACP_GA_NULL;
        tmp_elem[1] = ACP_GA_NULL;
        tmp_elem[2] = size;
        acp_copy(it.elem, buf_elem, 24, ACP_HANDLE_NULL);
        acp_copy(it.elem + 24, ga, size, ACP_HANDLE_NULL);
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
        acp_copy(it.elem, buf_elem, 24, ACP_HANDLE_NULL);
        acp_copy(it.elem + 24, ga, size, ACP_HANDLE_NULL);
        tmp_list[1] = it.elem;
        tmp_list[2]++;
        acp_copy(prev, buf_list + 8, 8, ACP_HANDLE_NULL);
        acp_copy(it.list.ga + 8, buf_list + 8, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    acp_free(buf);
    return;
}

//extern void acp_push_front_list(acp_list_t list, const acp_ga_t ga, size_t size, int rank);
//extern void acp_remove_list(acp_list_t list, const acp_ga_t ga, size_t size);
//extern void acp_reverse_list(acp_list_t list);
//extern size_t acp_size_list(acp_list_t list);
//extern size_t acp_sort_list(acp_list_t list, int (*comp)(const acp_list_it_t it1, const acp_list_it_t it2));
//extern void acp_splice_list(acp_list_it_t it, acp_list_t list);
//extern void acp_swap_list(acp_list_t list1, acp_list_t list2);
//extern void acp_unique_list(acp_list_t list);
//extern acp_list_it_t acp_advance_list_it(acp_list_it_t it, int n);

acp_list_it_t acp_decrement_list_it(acp_list_it_t it)
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

//extern acp_ga_t acp_dereference_list_it(acp_list_it_t it);
//extern int acp_distance_list_it(acp_list_it_t first, acp_list_it_t last);

acp_list_it_t acp_increment_list_it(acp_list_it_t it)
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

/** Set
 */

//extern void acp_assign_set(acp_set_t set1, acp_set_t set2);
//extern void acp_assign_range_set(acp_set_t set, acp_set_it_t start, acp_set_it_t end);
//extern acp_set_it_t acp_begin_set(acp_set_t set);
//extern int acp_bucket_set(acp_set_t set, const acp_ga_t key, size_t key_size);
//extern int acp_bucket_count_set(acp_set_t set);
//extern int acp_bucket_size_set(acp_set_t set, int index);
//extern void acp_clear_set(acp_set_t set);
//extern acp_set_t acp_create_set(int num_ranks, const int* ranks, int num_slots, int rank);
//extern void acp_destroy_set(acp_set_t set);
//extern int acp_empty_set(acp_set_t set);
//extern acp_set_it_t acp_end_set(acp_set_t set);
//extern acp_set_it_t acp_erase_set(acp_set_it_t it);
//extern acp_set_it_t acp_erase_range_set(acp_set_it_t start, acp_set_it_t end);
//extern acp_set_ib_t acp_find_set(acp_set_t set, const acp_ga_t key, size_t key_size);
//extern acp_set_ib_t acp_insert_set(acp_set_t set, const acp_ga_t key, size_t key_size);
//extern acp_set_ib_t acp_insert_range_set(acp_set_t set, acp_set_it_t start, acp_set_it_t end);
//extern size_t acp_size_set(acp_set_t set);
//extern void acp_swap_set(acp_set_t set1, acp_set_t set2);

//extern acp_set_it_t acp_advance_set_it(acp_set_it_t it, int n);
//extern acp_set_it_t acp_decrement_set_it(acp_set_it_t it);
//extern acp_ga_t acp_dereference_set_it(acp_set_it_t it);
//extern acp_set_it_t acp_increment_set_it(acp_set_it_t it);
//extern size_t acp_size_set_it(acp_set_it_t it);

/** Map
 *      [0]  number of slots
 *      [1-]  table of distributed hash tables
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

//extern void acp_assign_map(acp_map_t map1, acp_map_t map2);
//extern void acp_assign_range_map(acp_map_t map, acp_map_it_t start, acp_map_it_t end);
//extern acp_map_it_t acp_begin_map(acp_map_t map);
//extern int acp_bucket_map(acp_map_t map, const acp_ga_t key, size_t key_size);
//extern int acp_bucket_count_map(acp_map_t map);
//extern int acp_bucket_size_map(acp_map_t map, int index);

void acp_clear_map(acp_map_t map)
{
    acp_list_t list;
    uint64_t num, size_map, num_slots;
    acp_ga_t buf;
    volatile uint64_t* tmp_map;
    int i, j;
    
    size_map = map.num_ranks * 8 + 8;
    
    buf = acp_malloc(size_map, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp_map = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, map.ga, size_map, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    num_slots = tmp_map[0];
    for (i = 1; i <= map.num_ranks; i++)
        for (j = 0; j < num_slots; j++) {
            list.ga = tmp_map[i] + j * 24;
            acp_clear_list(list);
        }
    
    acp_free(buf);
    return;
}

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
    
    size_map = num_ranks * 8 + 8;
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
    
    tmp_map[0] = (uint64_t)num_slots;
    for (i = 0; i < map.num_ranks; i++) {
        tmp_map[i + 1] = acp_malloc(size_slots, ranks[i]);
        if (tmp_map[i + 1] == ACP_GA_NULL) {
            for (j = i; j >= 1; j--) acp_free(tmp_map[j]);
            acp_free(map.ga);
            acp_free(buf);
            map.ga = ACP_GA_NULL;
            return map;
        }
        acp_copy(tmp_map[i + 1], buf_slots, size_slots, ACP_HANDLE_NULL);
    }
    acp_copy(map.ga, buf_map, size_map, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return map;
}

void acp_destroy_map(acp_map_t map)
{
    acp_list_t list;
    uint64_t num, size_map, num_slots;
    acp_ga_t buf;
    volatile uint64_t* tmp_map;
    int i, j;
    
    size_map = map.num_ranks * 8 + 8;
    
    buf = acp_malloc(size_map, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp_map = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, map.ga, size_map, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    num_slots = tmp_map[0];
    for (i = 1; i <= map.num_ranks; i++) {
        for (j = 0; j < num_slots; j++) {
            list.ga = tmp_map[i] + j * 24;
            acp_clear_list(list);
        }
        acp_free(tmp_map[i]);
    }
    
    acp_free(map.ga);
    acp_free(buf);
    return;
}

//extern int acp_empty_map(acp_map_t map);
//extern acp_map_it_t acp_end_map(acp_map_t map);
acp_map_it_t acp_end_map(acp_map_t map)
{
    acp_map_it_t it;
    
    it.map = map;
    it.rank = map.num_ranks;
    it.slot = 0;
    it.elem = ACP_GA_NULL;
    
    return it;
}

//extern acp_map_it_t acp_erase_map(acp_map_it_t it);
//extern acp_map_it_t acp_erase_range_map(acp_map_it_t start, acp_map_it_t end);

acp_map_it_t acp_find_map(acp_map_t map, const acp_ga_t key, size_t key_size)
{
    acp_list_it_t it;
    uint64_t c, num_slots;
    void* ptr;
    acp_map_it_t map_it;
    acp_ga_t buf, buf_nslot, buf_table, buf_list, buf_elem, buf_key;
    volatile uint64_t* tmp_nslot;
    volatile uint64_t* tmp_table;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_elem;
    unsigned char* p1;
    unsigned char* p2;
    int i;
    
    buf = acp_malloc(8 + 8 + 24 + 32 + key_size, acp_rank());
    if (buf == ACP_GA_NULL) return map_it;
    tmp_nslot = (volatile uint64_t*)acp_query_address(buf_nslot = buf);
    tmp_table = (volatile uint64_t*)acp_query_address(buf_table = buf + 8);
    tmp_list  = (volatile uint64_t*)acp_query_address(buf_list  = buf + 8 + 8);
    tmp_elem  = (volatile uint64_t*)acp_query_address(buf_elem  = buf + 8 + 8 + 24);
    ptr       = (void*)             acp_query_address(buf_key   = buf + 8 + 8 + 24 + 32);
    
    acp_copy(buf_nslot, map.ga, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    num_slots = *tmp_nslot;
    
    if (acp_query_rank(key) == acp_rank())
        memcpy(ptr, acp_query_address(key), key_size);
    else {
        acp_copy(buf_key, key, key_size, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    c = iacpdl_crc64(ptr, key_size) >> 16;
    
    map_it.map = map;
    map_it.rank = (c / num_slots) % map.num_ranks;
    map_it.slot = c % num_slots;
    map_it.elem = ACP_GA_NULL;
    
    acp_copy(buf_table, map.ga + 8 + map_it.rank * 8, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    it.list.ga = tmp_table[0] + map_it.slot * 24;
    
    acp_copy(buf_list, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    it.elem = tmp_list[0];
    
    while (it.elem != ACP_GA_NULL) {
        acp_copy(buf_elem, it.elem, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        if (tmp_elem[3] == key_size) {
            acp_copy(buf_elem + 32, it.elem + 32, key_size, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            p1 = (unsigned char*)(tmp_elem + 4);
            p2 = (unsigned char*)key;
            for (i = 0; i < key_size; i++) if (*p1++ != *p2++) break;
            if (i == key_size) {
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

acp_map_ib_t acp_insert_map(acp_map_t map, const acp_ga_t key, size_t key_size, const acp_ga_t value, size_t value_size)
{
    acp_list_it_t it;
    uint64_t c, num_slots;
    void* ptr;
    acp_map_ib_t ib;
    acp_ga_t buf, buf_nslot, buf_table, buf_list, buf_elem, buf_key, buf_value, prev_ga;
    volatile uint64_t* tmp_nslot;
    volatile uint64_t* tmp_table;
    volatile uint64_t* tmp_list;
    volatile uint64_t* tmp_elem;
    unsigned char* p1;
    unsigned char* p2;
    int i;
    
    ib.success = 0;
    
    if (acp_query_rank(value) == acp_rank())
        buf = acp_malloc(8 + 8 + 24 + 32 + key_size + value_size, acp_rank());
    else
        buf = acp_malloc(8 + 8 + 24 + 32 + key_size, acp_rank());
    if (buf == ACP_GA_NULL) return ib;
    tmp_nslot = (volatile uint64_t*)acp_query_address(buf_nslot = buf);
    tmp_table = (volatile uint64_t*)acp_query_address(buf_table = buf + 8);
    tmp_list  = (volatile uint64_t*)acp_query_address(buf_list  = buf + 8 + 8);
    tmp_elem  = (volatile uint64_t*)acp_query_address(buf_elem  = buf + 8 + 8 + 24);
    ptr       = (void*)             acp_query_address(buf_key   = buf + 8 + 8 + 24 + 32);
    buf_value = buf + 8 + 8 + 24 + 32 + key_size;
    
    acp_copy(buf_nslot, map.ga, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    num_slots = *tmp_nslot;
    
    if (acp_query_rank(key) == acp_rank())
        memcpy(ptr, acp_query_address(key), key_size);
    else {
        acp_copy(buf_key, key, key_size, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    if (acp_query_rank(value) == acp_rank())
        memcpy((void*)acp_query_address(buf_value), (void*)acp_query_address(value), value_size);
    
    c = iacpdl_crc64(ptr, key_size) >> 16;
    
    ib.it.map = map;
    ib.it.rank = (c / num_slots) % map.num_ranks;
    ib.it.slot = c % num_slots;
    ib.it.elem = ACP_GA_NULL;
    ib.success = 0;
    
    acp_copy(buf_table, map.ga + 8 + ib.it.rank * 8, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    it.list.ga = *tmp_table + ib.it.slot * 24;
    
    acp_copy(buf_list, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    it.elem = tmp_list[0];
    
    while (it.elem != ACP_GA_NULL) {
        acp_copy(buf_elem, it.elem, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        if (tmp_elem[3] == key_size) {
            acp_copy(buf_elem + 32, it.elem + 32, key_size, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            p1 = (unsigned char*)(tmp_elem + 4);
            p2 = (unsigned char*)key;
            for (i = 0; i < key_size; i++) if (*p1++ != *p2++) break;
            if (i == key_size) {
                acp_free(buf);
                return ib;
            }
        }
        it.elem = tmp_elem[0];
    }
    
    ib.it.elem = acp_malloc(32 + key_size + value_size, acp_query_rank(it.list.ga));
    if (ib.it.elem == ACP_GA_NULL) {
        acp_free(buf);
        return ib;
    }
    
    if (tmp_list[2] == 0) {
        tmp_elem[0] = ACP_GA_NULL;
        tmp_elem[1] = ACP_GA_NULL;
        tmp_elem[2] = 8 + key_size + value_size;
        tmp_elem[3] = key_size;
        tmp_list[0] = ib.it.elem;
        tmp_list[1] = ib.it.elem;
        tmp_list[2] = 1;
    } else {
        tmp_elem[0] = ACP_GA_NULL;
        tmp_elem[1] = tmp_list[1];
        tmp_elem[2] = 8 + key_size + value_size;
        tmp_elem[3] = key_size;
        
        prev_ga = tmp_list[1];
        
        tmp_list[1] = ib.it.elem;
        tmp_list[2]++;
        
        acp_copy(prev_ga, buf_list + 8, 8, ACP_HANDLE_NULL);
    }
    
    if (acp_query_rank(value) == acp_rank())
        acp_copy(ib.it.elem, buf_elem, 32 + key_size + value_size, ACP_HANDLE_NULL);
    else {
        acp_copy(ib.it.elem, buf_elem, 32 + key_size, ACP_HANDLE_NULL);
        acp_copy(ib.it.elem + 32 + key_size, value, value_size, ACP_HANDLE_NULL);
    }
    acp_copy(it.list.ga, buf_list, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    ib.success = 1;
    return ib;
}

//extern acp_map_ib_t acp_insert_range_map(acp_map_t map, acp_map_it_t start, acp_map_it_t end);
//extern size_t acp_size_map(acp_map_t map);
//extern void acp_swap_map(acp_map_t map1, acp_map_t map2);

//extern acp_map_it_t acp_advance_map_it(acp_map_it_t it, int n);
//extern acp_map_it_t acp_decrement_map_it(acp_map_it_t it);
//extern acp_ga_t acp_dereference_map_it(acp_map_it_t it);
//extern acp_ga_t acp_dereference_value_map_it(acp_map_it_t it);
//extern acp_map_it_t acp_increment_map_it(acp_map_it_t it);
//extern size_t acp_size_map_it(acp_map_it_t it);
//extern size_t acp_size_value_map_it(acp_map_it_t it);

