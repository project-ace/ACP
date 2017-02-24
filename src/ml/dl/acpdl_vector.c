/*
 * ACP Middle Layer: Data Library vetor
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

/** Vector
 *      [00:07]  ga of array body
 *      [08:16]  size
 *      [16:23]  max
 */

void acp_assign_vector(acp_vector_t vector1, acp_vector_t vector2)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf1_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf1_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf1_max  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* buf2_ga   = (volatile acp_ga_t*)(ptr + 24);
    volatile uint64_t* buf2_size = (volatile uint64_t*)(ptr + 32);
    volatile uint64_t* buf2_max  = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf,      vector1.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, vector2.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga1   = *buf1_ga;
    uint64_t tmp_size1 = *buf1_size;
    uint64_t tmp_max1  = *buf1_max;
    acp_ga_t tmp_ga2   = *buf2_ga;
    uint64_t tmp_size2 = *buf2_size;
    uint64_t tmp_max2  = *buf2_max;
    
    if (tmp_size2 == 0) {
        if (tmp_size1 > 0) {
            *buf1_size = 0;
            acp_copy(vector1.ga, buf, 24, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
        }
        acp_free(buf);
        return;
    }
    
    if (tmp_size2 == tmp_size1) {
        acp_copy(tmp_ga1, tmp_ga2, tmp_size2, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(buf);
        return;
    }
    
    if (tmp_size2 > tmp_max1) {
        uint64_t new_max1 = tmp_size2 + (((tmp_size2 + 7) ^ 7) & 7);
        acp_ga_t new_ga1  = acp_malloc(new_max1, acp_query_rank(vector1.ga));
        if (new_ga1 == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        if (tmp_ga1 != ACP_GA_NULL) acp_free(tmp_ga1);
        tmp_ga1  = new_ga1;
        tmp_max1 = new_max1;
        *buf1_ga  = new_ga1;
        *buf1_max = new_max1;
    }
    *buf1_size = tmp_size2;
    acp_copy(tmp_ga1, tmp_ga2, tmp_size2, ACP_HANDLE_NULL);
    acp_copy(vector1.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_assign_range_vector(acp_vector_t vector, acp_vector_it_t start, acp_vector_it_t end)
{
    if (start.vector.ga != end.vector.ga) return;
    
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf1_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf1_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf1_max  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* buf2_ga   = (volatile acp_ga_t*)(ptr + 24);
    volatile uint64_t* buf2_size = (volatile uint64_t*)(ptr + 32);
    volatile uint64_t* buf2_max  = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf,      vector.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, start.vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga1   = *buf1_ga;
    uint64_t tmp_size1 = *buf1_size;
    uint64_t tmp_max1  = *buf1_max;
    acp_ga_t tmp_ga2   = *buf2_ga;
    uint64_t tmp_size2 = *buf2_size;
    uint64_t tmp_max2  = *buf2_max;
    
    int size = end.index - start.index;
    
    if (size <= 0) {
        if (tmp_size1 > 0) {
            *buf1_size = 0;
            acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
        }
        acp_free(buf);
        return;
    }
    
    if (size == tmp_size1) {
        acp_copy(tmp_ga1, tmp_ga2 + start.index, size, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(buf);
        return;
    }
    
    if (size > tmp_max1) {
        uint64_t new_max = size + (((size + 7) ^ 7) & 7);
        acp_ga_t new_ga = acp_malloc(new_max, acp_query_rank(vector.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        if (tmp_ga1 != ACP_GA_NULL) acp_free(tmp_ga1);
        tmp_ga1  = new_ga;
        tmp_max1 = new_max;
        *buf1_ga  = new_ga;
        *buf1_max = new_max;
    }
    *buf1_size = size;
    acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
    acp_copy(tmp_ga1, tmp_ga2 + start.index, size, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

acp_ga_t acp_at_vector(acp_vector_t vector, int index)
{
    if (index < 0) return ACP_GA_NULL;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return ACP_GA_NULL;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    acp_free(buf);
    return (index < tmp_size) ? tmp_ga + index : ACP_GA_NULL;
}

acp_vector_it_t acp_begin_vector(acp_vector_t vector)
{
    acp_vector_it_t it;
    
    it.vector.ga = vector.ga;
    it.index = 0;
    
    return it;
}

size_t acp_capacity_vector(acp_vector_t vector)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    acp_free(buf);
    return (size_t)tmp_max;
}

void acp_clear_vector(acp_vector_t vector)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    *buf_size = 0;
    
    acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    vector.ga = acp_malloc(24, rank);
    if (vector.ga == ACP_GA_NULL) {
        acp_free(buf);
        return vector;
    }
    
    acp_ga_t ga = ACP_GA_NULL;
    uint64_t max = size + (((size + 7) ^ 7) & 7);
    
    if (max > 0) {
        ga = acp_malloc(size, rank);
        if (ga == ACP_GA_NULL) {
            acp_free(vector.ga);
            acp_free(buf);
            vector.ga = ACP_GA_NULL;
            return vector;
        }
    }
    
    *buf_ga   = ga;
    *buf_size = size;
    *buf_max  = max;
    
    acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return vector;
}

void acp_destroy_vector(acp_vector_t vector)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    acp_free(tmp_ga);
    acp_free(vector.ga);
    acp_free(buf);
    return;
}

int acp_empty_vector(acp_vector_t vector)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return 1;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    acp_free(buf);
    return (tmp_size == 0) ? 1 : 0;
}

acp_vector_it_t acp_end_vector(acp_vector_t vector)
{
    acp_vector_it_t it;
    
    it.vector.ga = vector.ga;
    it.index = 0;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    it.index = tmp_size;
    
    acp_free(buf);
    return it;
}

acp_vector_it_t acp_erase_vector(acp_vector_it_t it, size_t size)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, it.vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    int index = it.index;
    
    if (index + size >= tmp_size) {
        *buf_size = index;
    } else {
        acp_handle_t handle = ACP_HANDLE_NULL;
        while (index + size < tmp_size) {
            handle = acp_copy(tmp_ga + index, tmp_ga + index + size, size, handle);
            index += size;
        }
        acp_copy(tmp_ga + index, tmp_ga + index + size, tmp_size - index - size, handle);
        
        *buf_size = tmp_size - size;
    }
    acp_copy(it.vector.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return it;
}

acp_vector_it_t acp_erase_range_vector(acp_vector_it_t start, acp_vector_it_t end)
{
    if (start.vector.ga != end.vector.ga || start.index >= end.index) return end;
    return acp_erase_vector(start, end.index - start.index);
}

acp_vector_it_t acp_insert_vector(acp_vector_it_t it, const acp_ga_t ga, size_t size)
{
    int index = it.index;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, it.vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    if (index > tmp_size) index = tmp_size;
    
    if (tmp_max == 0) {
        int max = size + (((size + 7) ^ 7) & 7);
        acp_ga_t new_ga = acp_malloc(max, acp_query_rank(it.vector.ga));
        acp_copy(new_ga, ga, size, ACP_HANDLE_NULL);
        *buf_ga   = new_ga;
        *buf_size = size;
        *buf_max  = max;
        acp_copy(it.vector.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else if (tmp_size + size > tmp_max) {
        int max = tmp_size + size + (((tmp_size + size + 7) ^ 7) & 7);
        acp_ga_t new_ga = acp_malloc(max, acp_query_rank(it.vector.ga));
        acp_copy(new_ga, tmp_ga, index, ACP_HANDLE_NULL);
        acp_copy(new_ga + index, ga, size, ACP_HANDLE_NULL);
        if (index < tmp_size) acp_copy(new_ga + index + size, tmp_ga + index, tmp_size - index, ACP_HANDLE_NULL);
        *buf_ga   = new_ga;
        *buf_size = tmp_size + size;
        *buf_max  = max;
        acp_copy(it.vector.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_ga);
    } else {
        int ptr = tmp_size;
        acp_handle_t handle = ACP_HANDLE_NULL;
        while (ptr > index + size) {
            ptr -= size;
            handle = acp_copy(tmp_ga + ptr + size, tmp_ga + ptr, size, handle);
        }
        if (ptr > index)
            handle = acp_copy(tmp_ga + index + size, tmp_ga + index, ptr - index, handle);
        acp_copy(tmp_ga + index, ga, size, handle);
        
        *buf_size = tmp_size + size;
        acp_copy(it.vector.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    acp_free(buf);
    return it;
}

acp_vector_it_t acp_insert_range_vector(acp_vector_it_t it, acp_vector_it_t start, acp_vector_it_t end)
{
    if (start.vector.ga != end.vector.ga || start.index >= end.index) return it;
    return acp_insert_vector(it, start.vector.ga + start.index, end.index - start.index);
}

void acp_pop_back_vector(acp_vector_t vector, size_t size)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    *buf_size = (tmp_size >= size) ? tmp_size - size : 0;
    
    acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return;
}

void acp_push_back_vector(acp_vector_t vector, const acp_ga_t ga, size_t size)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    if (tmp_max == 0) {
        int max = size + (((size + 7) ^ 7) & 7);
        acp_ga_t new_ga = acp_malloc(max, acp_query_rank(vector.ga));
        acp_copy(new_ga, ga, size, ACP_HANDLE_NULL);
        *buf_ga   = new_ga;
        *buf_size = size;
        *buf_max  = max;
        acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        if (tmp_ga != ACP_GA_NULL) acp_free(tmp_ga);
    } else if (tmp_size + size > tmp_max) {
        int max = tmp_size + size + (((tmp_size + size + 7) ^ 7) & 7);
        acp_ga_t new_ga = acp_malloc(max, acp_query_rank(vector.ga));
        acp_copy(new_ga, tmp_ga, tmp_size, ACP_HANDLE_NULL);
        acp_copy(new_ga + tmp_size, ga, size, ACP_HANDLE_NULL);
        *buf_ga   = new_ga;
        *buf_size = tmp_size + size;
        *buf_max  = max;
        acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_ga);
    } else {
        acp_copy(tmp_ga + tmp_size, ga, size, ACP_HANDLE_NULL);
        
        *buf_size = tmp_size + size;
        acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    acp_free(buf);
    return;
}

void acp_reserve_vector(acp_vector_t vector, size_t size)
{
    size += (((size + 7) & 7) ^ 7);
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    if (size > tmp_max) {
        int new_max = size + (((size + 7) ^ 7) & 7);
        acp_ga_t new_ga = acp_malloc(new_max, acp_query_rank(vector.ga));
        if (tmp_size > 0) acp_copy(new_ga, tmp_ga, tmp_size, ACP_HANDLE_NULL);
        *buf_ga   = new_ga;
        *buf_max  = new_max;
        acp_copy(vector.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        if (tmp_max > 0) acp_free(tmp_ga);
    }
    acp_free(buf);
    return;
}

size_t acp_size_vector(acp_vector_t vector)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    acp_free(buf);
    return (size_t)tmp_size;
}

void acp_swap_vector(acp_vector_t vector1, acp_vector_t vector2)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf1_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf1_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf1_max  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* buf2_ga   = (volatile acp_ga_t*)(ptr + 24);
    volatile uint64_t* buf2_size = (volatile uint64_t*)(ptr + 32);
    volatile uint64_t* buf2_max  = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf,      vector1.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, vector2.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga1   = *buf1_ga;
    uint64_t tmp_size1 = *buf1_size;
    uint64_t tmp_max1  = *buf1_max;
    acp_ga_t tmp_ga2   = *buf2_ga;
    uint64_t tmp_size2 = *buf2_size;
    uint64_t tmp_max2  = *buf2_max;
    
    acp_ga_t new_ga1 = ACP_GA_NULL;
    acp_ga_t new_ga2 = ACP_GA_NULL;
    
    if (tmp_size1 > 0) {
        new_ga2 = acp_malloc(tmp_max1, acp_query_rank(vector2.ga));
        if (new_ga2 == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
    }
    if (tmp_size2 > 0) {
        new_ga1 = acp_malloc(tmp_max2, acp_query_rank(vector1.ga));
        if (new_ga1 == ACP_GA_NULL) {
            if (new_ga2 != ACP_GA_NULL) acp_free(new_ga2);
            acp_free(buf);
            return;
        }
    }
    
    if (tmp_size1 > 0) acp_copy(new_ga2, tmp_ga1, tmp_size1, ACP_HANDLE_NULL);
    if (tmp_size2 > 0) acp_copy(new_ga1, tmp_ga2, tmp_size2, ACP_HANDLE_NULL);
    
    *buf2_ga = new_ga1;
    *buf1_ga = new_ga2;
    
    acp_copy(vector2.ga, buf,      24, ACP_HANDLE_NULL);
    acp_copy(vector1.ga, buf + 24, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (tmp_ga1 != ACP_GA_NULL) acp_free(tmp_ga1);
    if (tmp_ga2 != ACP_GA_NULL) acp_free(tmp_ga2);
    acp_free(buf);
    return;
}

acp_vector_it_t acp_advance_vector_it(acp_vector_it_t it, int n)
{
    it.index += n;
    return it;
}

acp_ga_t acp_dereference_vector_it(acp_vector_it_t it)
{
    if (it.index < 0) return ACP_GA_NULL;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return ACP_GA_NULL;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* buf_ga   = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_size = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_max  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, it.vector.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga   = *buf_ga;
    uint64_t tmp_size = *buf_size;
    uint64_t tmp_max  = *buf_max;
    
    acp_free(buf);
    return tmp_ga + it.index;
}

int acp_distance_vector_it(acp_vector_it_t first, acp_vector_it_t last)
{
    return last.index - first.index;
}

