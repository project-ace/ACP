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

extern size_t iacp_starter_memory_size_dl;

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

void acp_assign_vector(acp_vector_t vector1, acp_vector_t vector2)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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
    void* ptr = acp_query_address(buf);
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

/** Deque
 *      [00:07]  ga of queue body
 *      [08:16]  offset
 *      [16:23]  size
 *      [24:31]  max
 *      front = offset, back = offset + size
 */

void acp_assign_deque(acp_deque_t deque1, acp_deque_t deque2)
{
    acp_ga_t buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf1_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf1_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf1_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf1_max    = (volatile uint64_t*)(ptr + 24);
    volatile acp_ga_t* buf2_ga     = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* buf2_offset = (volatile uint64_t*)(ptr + 40);
    volatile uint64_t* buf2_size   = (volatile uint64_t*)(ptr + 48);
    volatile uint64_t* buf2_max    = (volatile uint64_t*)(ptr + 56);
    
    acp_copy(buf,      deque1.ga, 32, ACP_HANDLE_NULL);
    acp_copy(buf + 32, deque2.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga1     = *buf1_ga;
    uint64_t tmp_offset1 = *buf1_offset;
    uint64_t tmp_size1   = *buf1_size;
    uint64_t tmp_max1    = *buf1_max;
    acp_ga_t tmp_ga2     = *buf2_ga;
    uint64_t tmp_offset2 = *buf2_offset;
    uint64_t tmp_size2   = *buf2_size;
    uint64_t tmp_max2    = *buf2_max;
    
    if (tmp_size2 == 0) {
        if (tmp_offset1 > 0 || tmp_size1 > 0) {
            *buf1_offset = 0;
            *buf1_size = 0;
            acp_copy(deque1.ga, buf, 32, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
        }
        acp_free(buf);
        return;
    }
    
    if (tmp_size2 > tmp_max1) {
        acp_ga_t new_ga = acp_malloc(tmp_size2, acp_query_rank(deque1.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        if (tmp_ga1 != ACP_GA_NULL) acp_free(tmp_ga1);
        tmp_ga1   = new_ga;
        tmp_max1  = tmp_size2;
        *buf1_ga  = new_ga;
        *buf1_max = tmp_size2;
    }
    
    if (tmp_offset2 + tmp_size2 > tmp_max2) {
        acp_copy(tmp_ga1, tmp_ga2 + tmp_offset2, tmp_max2 - tmp_offset2, ACP_HANDLE_NULL);
        acp_copy(tmp_ga1 + tmp_max2 - tmp_offset2, tmp_ga2, tmp_offset2 + tmp_size2 - tmp_max2, ACP_HANDLE_NULL);
    } else
        acp_copy(tmp_ga1, tmp_ga2 + tmp_offset2, tmp_size2, ACP_HANDLE_NULL);
    
    *buf1_offset = 0;
    *buf1_size = tmp_size2;
    acp_copy(deque1.ga, buf, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_assign_range_deque(acp_deque_t deque, acp_deque_it_t start, acp_deque_it_t end)
{
    if (start.deque.ga != end.deque.ga) return;
    
    acp_ga_t buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf1_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf1_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf1_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf1_max    = (volatile uint64_t*)(ptr + 24);
    volatile acp_ga_t* buf2_ga     = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* buf2_offset = (volatile uint64_t*)(ptr + 40);
    volatile uint64_t* buf2_size   = (volatile uint64_t*)(ptr + 48);
    volatile uint64_t* buf2_max    = (volatile uint64_t*)(ptr + 56);
    
    acp_copy(buf,      deque.ga,       32, ACP_HANDLE_NULL);
    acp_copy(buf + 32, start.deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga1     = *buf1_ga;
    uint64_t tmp_offset1 = *buf1_offset;
    uint64_t tmp_size1   = *buf1_size;
    uint64_t tmp_max1    = *buf1_max;
    acp_ga_t tmp_ga2     = *buf2_ga;
    uint64_t tmp_offset2 = *buf2_offset;
    uint64_t tmp_size2   = *buf2_size;
    uint64_t tmp_max2    = *buf2_max;
    
    int offset = tmp_offset2 + start.index;
    offset = (offset > tmp_max2) ? offset - tmp_max2 : offset;
    end.index = (end.index > tmp_size2) ? tmp_size2 : end.index;
    int size = end.index - start.index;
    
    if (size <= 0) {
        if (tmp_offset1 > 0 || tmp_size1 > 0) {
            *buf1_offset = 0;
            *buf1_size = 0;
            acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
        }
        acp_free(buf);
        return;
    }
    
    if (size > tmp_max1) {
        acp_ga_t new_ga = acp_malloc(size, acp_query_rank(deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        if (tmp_ga1 != ACP_GA_NULL) acp_free(tmp_ga1);
        *buf1_ga  = new_ga;
        *buf1_max = size;
    }
    
    if (tmp_max2 < offset + size) {
        acp_copy(tmp_ga1, tmp_ga2 + offset, tmp_max2 - offset, ACP_HANDLE_NULL);
        acp_copy(tmp_ga1 + tmp_max2 - offset, tmp_ga2, offset + size - tmp_max2, ACP_HANDLE_NULL);
    } else
        acp_copy(tmp_ga1, tmp_ga2 + offset, size, ACP_HANDLE_NULL);
    
    *buf1_offset = 0;
    *buf1_size = size;
    acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

acp_ga_t acp_at_deque(acp_deque_t deque, int index)
{
    if (index < 0) return ACP_GA_NULL;
    
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return ACP_GA_NULL;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    acp_free(buf);
    
    if (index >= tmp_size) return ACP_GA_NULL;
    if (tmp_offset + index > tmp_max) return tmp_ga +  tmp_offset + index - tmp_max;
    return tmp_ga + tmp_offset + index;
}

acp_deque_it_t acp_begin_deque(acp_deque_t deque)
{
    acp_deque_it_t it;
    
    it.deque.ga = deque.ga;
    it.index = 0;
    
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    it.index = tmp_offset;
    acp_free(buf);
    return it;
}

size_t acp_capacity_deque(acp_deque_t deque)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    acp_free(buf);
    return (size_t)tmp_max;
}

void acp_clear_deque(acp_deque_t deque)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    *buf_offset = 0;
    *buf_size = 0;
    acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

acp_deque_t acp_create_deque(size_t size, int rank)
{
    acp_deque_t deque;
    deque.ga = ACP_GA_NULL;
    
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return deque;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    deque.ga = acp_malloc(32, rank);
    if (deque.ga == ACP_GA_NULL) {
        acp_free(buf);
        return deque;
    }
    
    acp_ga_t ga = ACP_GA_NULL;
    size_t max = size + (((size + 7) ^ 7) & 7);
    
    if (max > 0) {
        ga = acp_malloc(size, rank);
        if (ga == ACP_GA_NULL) {
            acp_free(deque.ga);
            acp_free(buf);
            deque.ga = ACP_GA_NULL;
            return deque;
        }
    }
    
    *buf_ga     = ga;
    *buf_offset = 0;
    *buf_size   = 0;
    *buf_max    = max;
    
    acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return deque;
}

void acp_destroy_deque(acp_deque_t deque)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    if (tmp_ga != ACP_GA_NULL) acp_free(tmp_ga);
    acp_free(deque.ga);
    acp_free(buf);
    return;
}

int acp_empty_deque(acp_deque_t deque)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return 1;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    acp_free(buf);
    return (tmp_size == 0) ? 1 : 0;
}

acp_deque_it_t acp_end_deque(acp_deque_t deque)
{
    acp_deque_it_t it;
    
    it.deque.ga = deque.ga;
    it.index = 0;
    
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    it.index = tmp_size;
    
    acp_free(buf);
    return it;
}

acp_deque_it_t acp_erase_deque(acp_deque_it_t it, size_t size)
{
    int index = it.index;
    
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, it.deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    if (index + size >= tmp_size) {
        *buf_size = index;
    } else {
        acp_handle_t handle = ACP_HANDLE_NULL;
        while (index + size < tmp_size) {
            uint64_t dst = tmp_offset + index;
            uint64_t src = tmp_offset + index + size;
            uint64_t chunk_size = (index + size + size > tmp_size) ? tmp_size - index - size : size;
            if (tmp_offset + index >= tmp_max) {
                dst -= tmp_max;
                src -= tmp_max;
            } else if (tmp_offset + index + chunk_size > tmp_max) {
                uint64_t fragment_size = tmp_offset + index + chunk_size - tmp_max;
                src -= tmp_max;
                handle = acp_copy(tmp_ga + dst, tmp_ga + src, fragment_size, handle);
                index += fragment_size;
                chunk_size -= fragment_size;
                dst = 0;
                src = size;
            } else if (tmp_offset + index + size >= tmp_max) {
                src -= tmp_max;
            } else if (tmp_offset + index + size + chunk_size > tmp_max) {
                uint64_t fragment_size = tmp_offset + index + size + chunk_size - tmp_max;
                handle = acp_copy(tmp_ga + dst, tmp_ga + src, fragment_size, handle);
                index += fragment_size;
                chunk_size -= fragment_size;
                dst += fragment_size;
                src = 0;
            }
            handle = acp_copy(tmp_ga + dst, tmp_ga + src, chunk_size, handle);
            index += chunk_size;
        }
        *buf_size = tmp_size - size;
    }
    acp_copy(it.deque.ga, buf, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return it;
}

acp_deque_it_t acp_erase_range_deque(acp_deque_it_t start, acp_deque_it_t end)
{
    if (start.deque.ga != end.deque.ga || start.index >= end.index) return end;
    return acp_erase_deque(start, end.index - start.index);
}

acp_deque_it_t acp_insert_deque(acp_deque_it_t it, const acp_ga_t ga, size_t size)
{
    int index = it.index;
    
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, it.deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    if (index > tmp_size) index = tmp_size;
    
    if (tmp_max == 0) {
        acp_ga_t new_ga = acp_malloc(size, acp_query_rank(it.deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return it;
        }
        acp_copy(new_ga, ga, size, ACP_HANDLE_NULL);
        *buf_ga     = new_ga;
        *buf_offset = 0;
        *buf_size   = size;
        *buf_max    = size;
        acp_copy(it.deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else if (tmp_size + size > tmp_max) {
        int max = tmp_size + size;
        acp_ga_t new_ga = acp_malloc(max, acp_query_rank(it.deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return it;
        }
        if (index < tmp_size) {
            if (tmp_offset + index > tmp_max) {
                acp_copy(new_ga, tmp_ga + tmp_offset, tmp_max - tmp_offset, ACP_HANDLE_NULL);
                acp_copy(new_ga + tmp_max - tmp_offset, tmp_ga, tmp_offset + index - tmp_max, ACP_HANDLE_NULL);
                acp_copy(new_ga + index + size, tmp_ga + tmp_offset + index - tmp_max, tmp_size - index, ACP_HANDLE_NULL);
            } else if (tmp_offset + index == tmp_max) {
                acp_copy(new_ga, tmp_ga + tmp_offset, index, ACP_HANDLE_NULL);
                acp_copy(new_ga + index + size, tmp_ga, tmp_size - index, ACP_HANDLE_NULL);
            } else if (tmp_offset + tmp_size > tmp_max) {
                acp_copy(new_ga, tmp_ga + tmp_offset, index, ACP_HANDLE_NULL);
                acp_copy(new_ga + index + size, tmp_ga + tmp_offset + index, tmp_max - tmp_offset - index, ACP_HANDLE_NULL);
                acp_copy(new_ga + tmp_max - tmp_offset + size, tmp_ga, tmp_offset + tmp_size - tmp_max, ACP_HANDLE_NULL);
            } else {
                acp_copy(new_ga, tmp_ga + tmp_offset, index, ACP_HANDLE_NULL);
                acp_copy(new_ga + index + size, tmp_ga + tmp_offset + index, tmp_size - index, ACP_HANDLE_NULL);
            }
        } else {
            if (tmp_offset + index > tmp_max) {
                acp_copy(new_ga, tmp_ga + tmp_offset, tmp_max - tmp_offset, ACP_HANDLE_NULL);
                acp_copy(new_ga + tmp_max - tmp_offset, tmp_ga, tmp_offset + index - tmp_max, ACP_HANDLE_NULL);
            } else {
                acp_copy(new_ga, tmp_ga + tmp_offset, index, ACP_HANDLE_NULL);
            }
        }
        acp_copy(new_ga + index, ga, size, ACP_HANDLE_NULL);
        
        *buf_ga     = new_ga;
        *buf_offset = 0;
        *buf_size   = tmp_size + size;
        *buf_max    = tmp_size + size;
        acp_copy(it.deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_ga);
    } else {
        int ptr = tmp_size;
        acp_handle_t handle = ACP_HANDLE_NULL;
        while (ptr > index) {
            uint64_t chunk_size = (ptr - index >= size) ? size : ptr - index;
            uint64_t dst = tmp_offset + ptr + size - chunk_size;
            uint64_t src = tmp_offset + ptr - chunk_size;
            if (tmp_offset + ptr >= tmp_max + chunk_size) {
                dst -= tmp_max;
                src -= tmp_max;
            } else if (tmp_offset + ptr > tmp_max) {
                uint64_t fragment_size = tmp_max + size + chunk_size - tmp_offset - ptr;
                dst -= tmp_max;
                handle = acp_copy(tmp_ga + dst, tmp_ga + src, fragment_size, handle);
                ptr -= fragment_size;
                chunk_size -= fragment_size;
                dst = size;
                src = 0;
            } else if (tmp_offset + ptr + size >= tmp_max + chunk_size) {
                dst -= tmp_max;
            } else if (tmp_offset + ptr + size > tmp_max) {
                uint64_t fragment_size = tmp_max + chunk_size - tmp_offset - ptr;
                handle = acp_copy(tmp_ga + dst, tmp_ga + src, fragment_size, handle);
                ptr -= fragment_size;
                chunk_size -= fragment_size;
                dst = 0;
                src = tmp_max - size;
            }
            handle = acp_copy(tmp_ga + dst, tmp_ga + src, chunk_size, handle);
            ptr -= chunk_size;
        }
        if (tmp_offset + index < tmp_max && tmp_offset + index + size > tmp_max) {
            uint64_t chunk_size = tmp_max - tmp_offset - index;
            acp_copy(tmp_ga + tmp_offset + index, ga, chunk_size, handle);
            acp_copy(tmp_ga, ga + chunk_size, size - chunk_size, handle);
        } else
            acp_copy(tmp_ga + index, ga, size, handle);
        
        *buf_size = tmp_size + size;
        acp_copy(it.deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    acp_free(buf);
    return it;
}

acp_deque_it_t acp_insert_range_deque(acp_deque_it_t it, acp_deque_it_t start, acp_deque_it_t end)
{
    if (start.deque.ga != end.deque.ga || start.index >= end.index) return end;
    acp_pair_t pair = acp_dereference_deque_it(start, end.index - start.index);
    if (pair.second.ga != ACP_GA_NULL) acp_insert_deque(it, pair.second.ga, pair.second.size);
    return acp_insert_deque(it, pair.first.ga, pair.first.size);
}

void acp_pop_back_deque(acp_deque_t deque, size_t size)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    *buf_size = (tmp_size >= size) ? tmp_size - size : 0;
    
    acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return;
}

void acp_pop_front_deque(acp_deque_t deque, size_t size)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    tmp_offset += (tmp_size >= size) ? size : tmp_size;
    tmp_offset -= (tmp_offset >= tmp_max) ? tmp_max : 0;
    tmp_size = (tmp_size >= size) ? tmp_size - size : 0;
    
    *buf_offset = tmp_offset;
    *buf_size   = tmp_size;
    
    acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return;
}

void acp_push_back_deque(acp_deque_t deque, const acp_ga_t ga, size_t size)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    if (tmp_max == 0) {
        acp_ga_t new_ga = acp_malloc(size, acp_query_rank(deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        acp_copy(new_ga, ga, size, ACP_HANDLE_NULL);
        *buf_ga     = new_ga;
        *buf_offset = 0;
        *buf_size   = size;
        *buf_max    = size;
        acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else if (tmp_size + size > tmp_max) {
        int max = tmp_size + size;
        acp_ga_t new_ga = acp_malloc(max, acp_query_rank(deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        if (tmp_offset + tmp_size > tmp_max) {
            acp_copy(new_ga, tmp_ga + tmp_offset, tmp_max - tmp_offset, ACP_HANDLE_NULL);
            acp_copy(new_ga + tmp_max - tmp_offset, tmp_ga, tmp_offset + tmp_size - tmp_max, ACP_HANDLE_NULL);
        } else {
            acp_copy(new_ga, tmp_ga + tmp_offset, tmp_size, ACP_HANDLE_NULL);
        }
        acp_copy(new_ga + tmp_size, ga, size, ACP_HANDLE_NULL);
        
        *buf_ga     = new_ga;
        *buf_offset = 0;
        *buf_size   = tmp_size + size;
        *buf_max    = tmp_size + size;
        acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_ga);
    } else {
        if (tmp_offset + tmp_size >= tmp_max) {
            acp_copy(tmp_ga + tmp_offset + tmp_size - tmp_max, ga, size, ACP_HANDLE_NULL);
        } else if (tmp_offset + tmp_size + size > tmp_max) {
            acp_copy(tmp_ga + tmp_offset + tmp_size, ga, tmp_max - tmp_offset - tmp_size, ACP_HANDLE_NULL);
            acp_copy(tmp_ga, ga + tmp_max - tmp_offset - tmp_size, tmp_offset + tmp_size + size - tmp_max, ACP_HANDLE_NULL);
        } else {
            acp_copy(tmp_ga + tmp_offset + tmp_size, ga, size, ACP_HANDLE_NULL);
        }
        *buf_size = tmp_size + size;
        acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return;
}

void acp_push_front_deque(acp_deque_t deque, const acp_ga_t ga, size_t size)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    if (tmp_max == 0) {
        acp_ga_t new_ga = acp_malloc(size, acp_query_rank(deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        acp_copy(new_ga, ga, size, ACP_HANDLE_NULL);
        *buf_ga     = new_ga;
        *buf_offset = 0;
        *buf_size   = size;
        *buf_max    = size;
        acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else if (tmp_size + size > tmp_max) {
        int max = tmp_size + size;
        acp_ga_t new_ga = acp_malloc(max, acp_query_rank(deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        acp_copy(new_ga, ga, size, ACP_HANDLE_NULL);
        if (tmp_offset + tmp_size > tmp_max) {
            acp_copy(new_ga + size, tmp_ga + tmp_offset, tmp_max - tmp_offset, ACP_HANDLE_NULL);
            acp_copy(new_ga + size + tmp_max - tmp_offset, tmp_ga, tmp_offset + tmp_size - tmp_max, ACP_HANDLE_NULL);
        } else {
            acp_copy(new_ga + size, tmp_ga + tmp_offset, tmp_size, ACP_HANDLE_NULL);
        }
        
        *buf_ga     = new_ga;
        *buf_offset = 0;
        *buf_size   = tmp_size + size;
        *buf_max    = tmp_size + size;
        acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_ga);
    } else {
        if (tmp_offset >= size) {
            acp_copy(tmp_ga + tmp_offset - size, ga, size, ACP_HANDLE_NULL);
            tmp_offset -= size;
        } else {
            acp_copy(tmp_ga + tmp_max + tmp_offset - size, ga, size - tmp_offset, ACP_HANDLE_NULL);
            acp_copy(tmp_ga, ga + size - tmp_offset, tmp_offset, ACP_HANDLE_NULL);
            tmp_offset += tmp_max - size;
        }
        *buf_offset = tmp_offset;
        *buf_size   = tmp_size + size;
        acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return;
}

void acp_reserve_deque(acp_deque_t deque, size_t size)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    if (tmp_max >= size) {
        acp_free(buf);
        return;
    }
    
    if (tmp_max == 0) {
        acp_ga_t new_ga = acp_malloc(size, acp_query_rank(deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        *buf_ga     = new_ga;
        *buf_offset = 0;
        *buf_size   = 0;
        *buf_max    = size;
        acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        acp_ga_t new_ga = acp_malloc(size, acp_query_rank(deque.ga));
        if (new_ga == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
        if (tmp_offset + tmp_size > tmp_max) {
            acp_copy(new_ga, tmp_ga + tmp_offset, tmp_max - tmp_offset, ACP_HANDLE_NULL);
            acp_copy(new_ga + tmp_max - tmp_offset, tmp_ga, tmp_offset + tmp_size - tmp_max, ACP_HANDLE_NULL);
        } else {
            acp_copy(new_ga, tmp_ga + tmp_offset, tmp_size, ACP_HANDLE_NULL);
        }
        
        *buf_ga     = new_ga;
        *buf_offset = 0;
        *buf_size   = tmp_size;
        *buf_max    = size;
        acp_copy(deque.ga, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_ga);
    }
    
    acp_free(buf);
    return;
}

size_t acp_size_deque(acp_deque_t deque)
{
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    acp_free(buf);
    return (size_t)tmp_size;
}

void acp_swap_deque(acp_deque_t deque1, acp_deque_t deque2)
{
    acp_ga_t buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf1_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf1_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf1_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf1_max    = (volatile uint64_t*)(ptr + 24);
    volatile acp_ga_t* buf2_ga     = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* buf2_offset = (volatile uint64_t*)(ptr + 40);
    volatile uint64_t* buf2_size   = (volatile uint64_t*)(ptr + 48);
    volatile uint64_t* buf2_max    = (volatile uint64_t*)(ptr + 56);
    
    acp_copy(buf,      deque1.ga, 32, ACP_HANDLE_NULL);
    acp_copy(buf + 32, deque2.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga1     = *buf1_ga;
    uint64_t tmp_offset1 = *buf1_offset;
    uint64_t tmp_size1   = *buf1_size;
    uint64_t tmp_max1    = *buf1_max;
    acp_ga_t tmp_ga2     = *buf2_ga;
    uint64_t tmp_offset2 = *buf2_offset;
    uint64_t tmp_size2   = *buf2_size;
    uint64_t tmp_max2    = *buf2_max;
    
    acp_ga_t new_ga1   = ACP_GA_NULL;
    uint64_t new_size1 = tmp_size2;
    acp_ga_t new_ga2   = ACP_GA_NULL;
    uint64_t new_size2 = tmp_size1;
    
    if (new_size1 > 0) {
        new_ga1 = acp_malloc(new_size1, acp_query_rank(deque1.ga));
        if (new_ga1 == ACP_GA_NULL) {
            acp_free(buf);
            return;
        }
    }
    if (new_size2 > 0) {
        new_ga2 = acp_malloc(new_size2, acp_query_rank(deque2.ga));
        if (new_ga1 == ACP_GA_NULL) {
            if (new_ga2 != ACP_GA_NULL) acp_free(new_ga2);
            acp_free(buf);
            return;
        }
    }
    
    if (new_size1 > 0) {
        if (tmp_max2 < tmp_offset2 + tmp_size2) {
            acp_copy(new_ga1, tmp_ga2 + tmp_offset2, tmp_max2 - tmp_offset2, ACP_HANDLE_NULL);
            acp_copy(new_ga1 + tmp_max2 - tmp_offset2, tmp_ga2, tmp_offset2 + tmp_size2 - tmp_max2, ACP_HANDLE_NULL);
        } else
            acp_copy(new_ga1, tmp_ga2 + tmp_offset2, tmp_size2, ACP_HANDLE_NULL);
    }
    if (new_size2 > 0) {
        if (tmp_max1 < tmp_offset1 + tmp_size1) {
            acp_copy(new_ga2, tmp_ga1 + tmp_offset1, tmp_max1 - tmp_offset1, ACP_HANDLE_NULL);
            acp_copy(new_ga2 + tmp_max1 - tmp_offset1, tmp_ga1, tmp_offset1 + tmp_size1 - tmp_max1, ACP_HANDLE_NULL);
        } else
            acp_copy(new_ga2, tmp_ga1 + tmp_offset1, tmp_size1, ACP_HANDLE_NULL);
    }
    
    *buf1_ga     = new_ga1;
    *buf1_offset = 0;
    *buf1_size   = new_size1;
    *buf1_max    = new_size1;
    *buf2_ga     = new_ga2;
    *buf2_offset = 0;
    *buf2_size   = new_size2;
    *buf2_max    = new_size2;
    
    acp_copy(deque1.ga, buf,      32, ACP_HANDLE_NULL);
    acp_copy(deque2.ga, buf + 32, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (tmp_ga1 != ACP_GA_NULL) acp_free(tmp_ga1);
    if (tmp_ga2 != ACP_GA_NULL) acp_free(tmp_ga2);
    acp_free(buf);
    return;
}

acp_deque_it_t acp_advance_deque_it(acp_deque_it_t it, int n)
{
    it.index += n;
    return it;
}

acp_pair_t acp_dereference_deque_it(acp_deque_it_t it, size_t size)
{
    acp_pair_t pair;
    
    pair.first.ga = ACP_GA_NULL;
    pair.first.size = 0;
    pair.second.ga = ACP_GA_NULL;
    pair.second.size = 0;
    
    if (it.index < 0 || size < 0) return pair;
    
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return pair;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* buf_ga     = (volatile acp_ga_t*)ptr;
    volatile uint64_t* buf_offset = (volatile uint64_t*)(ptr + 8);
    volatile uint64_t* buf_size   = (volatile uint64_t*)(ptr + 16);
    volatile uint64_t* buf_max    = (volatile uint64_t*)(ptr + 24);
    
    acp_copy(buf, it.deque.ga, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_ga     = *buf_ga;
    uint64_t tmp_offset = *buf_offset;
    uint64_t tmp_size   = *buf_size;
    uint64_t tmp_max    = *buf_max;
    
    acp_free(buf);
    
    if (tmp_size <= it.index) return pair;
    if (size > tmp_size - it.index) size = tmp_size - it.index;
    
    if (tmp_offset + it.index >= tmp_max) {
        pair.first.ga = tmp_ga + tmp_offset + it.index - tmp_max;
        pair.first.size = size;
    } else if (tmp_offset + it.index + size > tmp_max) {
        pair.first.ga = tmp_ga + tmp_offset + it.index;
        pair.first.size = tmp_max - tmp_offset - it.index;
        pair.second.ga = tmp_ga;
        pair.second.size = tmp_offset + it.index + size - tmp_max;
    } else {
        pair.first.ga = tmp_ga + tmp_offset + it.index;
        pair.first.size = size;
    }
    
    return pair;
}

int acp_distance_deque_it(acp_deque_it_t first, acp_deque_it_t last)
{
    return last.index - first.index;
}

/** List
 *      [0]  ga of head element
 *      [1]  ga of tail element
 *      [2]  number of elements
 ** List Element
 *      [0]  ga of next element (or list object at tail)
 *      [1]  ga of previos element (or list object at head)
 *      [2]  size of this element
 *      [3-] value
 */

void acp_assign_list(acp_list_t list1, acp_list_t list2)
{
    acp_ga_t buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* prev_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* prev_prev = (volatile acp_ga_t*)(ptr + 56);
    
    /* clear list1 */
    acp_copy(buf, list1.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    uint64_t tmp_num2;
    
    acp_ga_t next = tmp_head;
    while (tmp_num-- > 0 && next != list1.ga) {
        acp_copy(buf + 24, next, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(next);
        next = *elem_next;
    }
    
    /* duplicate list2 */
    acp_copy(buf, list2.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    tmp_head = *list_head;
    tmp_tail = *list_tail;
    tmp_num  = *list_num;
    
    tmp_num2       = 0;
    next           = tmp_head;
    acp_ga_t prev  = list1.ga;
    acp_ga_t first = list1.ga;
    while (tmp_num-- > 0 && next != list2.ga) {
        acp_copy(buf + 24, next, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        acp_ga_t new_elem = acp_malloc(24 + tmp_size, acp_query_rank(next));
        if (new_elem != ACP_GA_NULL) {
            acp_copy(new_elem + 16, next + 16, tmp_size + 8, ACP_HANDLE_NULL);
            if (first != list1.ga) {
                *prev_next = new_elem;
                acp_copy(prev, buf + 48, 16, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
            } else
                first = new_elem;
            *prev_prev = prev;
            prev = new_elem;
            tmp_num2++;
        }
        next = tmp_next;
    }
    if (first != list1.ga) {
        *prev_next = list1.ga;
        acp_copy(prev, buf + 48, 16, ACP_HANDLE_NULL);
    }
    
    /* set new list1 */
    *list_head = first;
    *list_tail = prev;
    *list_num  = tmp_num2;
    acp_copy(list1.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_assign_range_list(acp_list_t list, acp_list_it_t start, acp_list_it_t end)
{
    if (start.list.ga != end.list.ga) return;
    
    acp_ga_t buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* prev_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* prev_prev = (volatile acp_ga_t*)(ptr + 56);
    
    /* clear list */
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_ga_t next = tmp_head;
    while (tmp_num-- > 0) {
        acp_copy(buf + 24, next, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(next);
        next = *elem_next;
    }
    
    /* duplicate range */
    tmp_num        = 0;
    next           = start.elem;
    acp_ga_t prev  = list.ga;
    acp_ga_t first = list.ga;
    while (next != end.elem) {
        acp_copy(buf + 24, next, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        acp_ga_t new_elem = acp_malloc(24 + tmp_size, acp_query_rank(next));
        if (new_elem != ACP_GA_NULL) {
            acp_copy(new_elem + 16, next + 16, tmp_size + 8, ACP_HANDLE_NULL);
            if (first != list.ga) {
                *prev_next = new_elem;
                acp_copy(prev, buf + 48, 16, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
            } else
                first = new_elem;
            *prev_prev = prev;
            prev = new_elem;
            tmp_num++;
        }
        next = tmp_next;
    }
    if (first != list.ga) {
        *prev_next = list.ga;
        acp_copy(prev, buf + 48, 16, ACP_HANDLE_NULL);
    }
    
    /* set new list */
    *list_head = first;
    *list_tail = prev;
    *list_num  = tmp_num;
    acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

acp_list_it_t acp_begin_list(acp_list_t list)
{
    acp_list_it_t it;
    it.list = list;
    it.elem = list.ga;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    it.elem = tmp_head;
    
    acp_free(buf);
    return it;
}

void acp_clear_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_ga_t ga = tmp_head;
    while (tmp_num-- > 0 && ga != list.ga) {
        acp_copy(buf + 24, ga, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(ga);
        ga = *elem_next;
    }
    
    *list_head = list.ga;
    *list_tail = list.ga;
    *list_num  = 0;
    acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

acp_list_t acp_create_list(int rank)
{
    acp_list_t list;
    list.ga = ACP_GA_NULL;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return list;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    
    list.ga = acp_malloc(24, rank);
    if (list.ga == ACP_GA_NULL) {
        acp_free(buf);
        return list;
    }
    
    *list_head = list.ga;
    *list_tail = list.ga;
    *list_num  = 0;
    acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return list;
}

void acp_destroy_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_ga_t ga = tmp_head;
    while (tmp_num-- > 0 && ga != list.ga) {
        acp_copy(buf + 24, ga, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(ga);
        ga = *elem_next;
    }
    
    acp_free(list.ga);
    acp_free(buf);
    return;
}

int acp_empty_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_free(buf);
    return (tmp_num == 0) ? 1 : 0;
};

acp_list_it_t acp_end_list(acp_list_t list)
{
    acp_list_it_t it;
    
    it.list = list;
    it.elem = list.ga;
    return it;
}

acp_list_it_t acp_erase_list(acp_list_it_t it)
{
    /* if the iterator is null or end, it fails */
    if (it.elem == ACP_GA_NULL || it.elem == it.list.ga) {
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, it.elem, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    acp_ga_t tmp_next = *elem_next;
    acp_ga_t tmp_prev = *elem_prev;
    uint64_t tmp_size = *elem_size;
    
    if (tmp_num == 1 && tmp_head == it.elem && tmp_tail == it.elem) {
        /* the only element is erased */
        *list_head = it.list.ga;
        *list_tail = it.list.ga;
        *list_num = 0;
        acp_copy(it.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it.elem);
        it.elem = it.list.ga;
    } else if (tmp_num > 1 && tmp_head == it.elem ) {
        /* the head element is erased */
        *list_head = tmp_next;
        *list_num  = tmp_num - 1;
        acp_copy(it.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_next + 8, buf + 24 + 8, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it.elem);
        it.elem = tmp_next;
    } else if (tmp_num > 1 && tmp_tail == it.elem) {
        /* the tail element is erased and it returns an end iterator */
        *list_tail = tmp_prev;
        *list_num  = tmp_num - 1;
        acp_copy(it.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_prev, buf + 24, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it.elem);
        it.elem = it.list.ga;
    } else if (tmp_num > 1 && it.elem != it.list.ga && it.elem != ACP_GA_NULL) {
        /* an intermediate element is erased and it returns an iterator to the next element */
        *list_num  = tmp_num - 1;
        acp_copy(it.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_next + 8, buf + 24 + 8, 8, ACP_HANDLE_NULL);
        acp_copy(tmp_prev, buf + 24, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it.elem);
        it.elem = tmp_next;
    } else
        it.elem = ACP_GA_NULL;
    
    acp_free(buf);
    return it;
}

acp_list_it_t acp_erase_range_list(acp_list_it_t start, acp_list_it_t end)
{
    if (start.list.ga != end.list.ga || start.list.ga == ACP_GA_NULL || end.elem == ACP_GA_NULL || start.elem == start.list.ga || start.elem == end.elem) return end;
    
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return start;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    
    acp_ga_t ga = start.elem;
    
    acp_copy(buf, start.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    acp_ga_t tmp_next = *elem_next;
    acp_ga_t tmp_prev = *elem_prev;
    uint64_t tmp_size = *elem_size;
    acp_ga_t prev = tmp_prev;
    
    /* Erase elements */
    while (tmp_next != end.elem && tmp_next != start.list.ga && tmp_num > 1) {
        acp_free(ga);
        ga = tmp_next;
        tmp_num--;
        acp_copy(buf + 24, tmp_next, 24, ACP_HANDLE_NULL);
        tmp_next = *elem_next;
        tmp_prev = *elem_prev;
        tmp_size = *elem_size;
    }
    acp_free(ga);
    tmp_num--;
    
    /* Update list and adjacent emelents */
    if (tmp_num == 0) {
        /* all elements are erased */
        *list_head = start.list.ga;
        *list_tail = start.list.ga;
        *list_num = 0;
        acp_copy(start.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        end.elem = end.list.ga;
    } else if (tmp_num == 1 && tmp_head != start.elem ) {
        /* all elements but the head are erased */
        *list_tail = tmp_head;
        *list_num = 1;
        *elem_next = start.list.ga;
        *elem_prev = start.list.ga;
        acp_copy(start.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_head, buf + 24, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        end.elem = end.list.ga;
    } else if (tmp_num == 1 && tmp_head == start.elem) {
        /* all elements but the tail are erased */
        *list_head = tmp_tail;
        *list_num = 1;
        *elem_next = start.list.ga;
        *elem_prev = start.list.ga;
        acp_copy(start.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_tail, buf + 24, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        end.elem = tmp_tail;
    } else if (tmp_head == start.elem) {
        /* the head element is changed */
        *list_head = tmp_next;
        *list_num = tmp_num;
        *elem_prev = start.list.ga;
        acp_copy(start.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_next + 8, buf + 24 + 8, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        end.elem = tmp_tail;
    } else if (tmp_next == start.list.ga) {
        /* the tail emelent is changed */
        *list_tail = tmp_next;
        *list_num = tmp_num;
        *elem_next = start.list.ga;
        acp_copy(start.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_next + 8, buf + 24 + 8, 8, ACP_HANDLE_NULL);
        acp_copy(prev, buf + 24, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        end.elem = tmp_next;
    } else {
        /* middle emelents are erased */
        *list_num = tmp_num;
        *elem_next = tmp_next;
        *elem_prev = prev;
        acp_copy(start.list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_next + 8, buf + 24 + 8, 8, ACP_HANDLE_NULL);
        acp_copy(prev, buf + 24, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        end.elem = tmp_next;
    }
    
    acp_free(buf);
    return end;
}

acp_list_it_t acp_insert_list(acp_list_it_t it, const acp_element_t elem, int rank)
{
    acp_ga_t buf = acp_malloc(56, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* elem_ga   = (volatile acp_ga_t*)(ptr + 48);
    
    acp_ga_t new_elem = acp_malloc(24 + elem.size, rank);
    if (new_elem == ACP_GA_NULL) {
        acp_free(buf);
        it.elem = ACP_GA_NULL;
        return it;
    }
    
    acp_copy(buf, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24 + 8, it.elem + 8, 8, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    acp_ga_t tmp_next = it.elem;
    acp_ga_t tmp_prev = *elem_prev;
    uint64_t tmp_size = elem.size;
    
    *list_num  = tmp_num + 1;
    *elem_next = tmp_next;
    *elem_size = tmp_size;
    *elem_ga   = new_elem;
    
    acp_copy(new_elem, buf + 24, 24, ACP_HANDLE_NULL);
    acp_copy(new_elem + 24, elem.ga, elem.size, ACP_HANDLE_NULL);
    acp_copy(tmp_prev, buf + 24 + 24, 8, ACP_HANDLE_NULL);
    acp_copy(tmp_next + 8, buf + 24 + 24, 8, ACP_HANDLE_NULL);
    acp_copy(it.list.ga + 16, buf + 16, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    it.elem = new_elem;
    
    acp_free(buf);
    return it;
}

acp_list_it_t acp_insert_range_list(acp_list_it_t it, acp_list_it_t start, acp_list_it_t end)
{
    acp_ga_t buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* prev_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* prev_prev = (volatile acp_ga_t*)(ptr + 56);
    
    acp_copy(buf, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24 + 8, it.elem + 8, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    acp_ga_t next     = start.elem;
    acp_ga_t prev     = *elem_prev;
    acp_ga_t pred     = prev;
    acp_ga_t first    = ACP_GA_NULL;
    while (next != end.elem) {
        acp_copy(buf + 24, next, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        acp_ga_t new_elem = acp_malloc(24 + tmp_size, acp_query_rank(next));
        if (new_elem != ACP_GA_NULL) {
            acp_copy(new_elem + 16, next + 16, tmp_size + 8, ACP_HANDLE_NULL);
            if (first != ACP_GA_NULL) {
                *prev_next = new_elem;
                acp_copy(prev, buf + 48, 16, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
            } else
                first = new_elem;
            *prev_prev = prev;
            prev = new_elem;
            tmp_num++;
        }
        next = tmp_next;
    }
    if (first != ACP_GA_NULL) {
        *prev_next = it.elem;
        acp_copy(prev, buf + 48, 16, ACP_HANDLE_NULL);
    }
    
    *list_head = first;
    *list_tail = prev;
    *list_num = tmp_num;
    acp_copy(pred, buf, 8, ACP_HANDLE_NULL);
    acp_copy(it.elem + 8, buf + 8, 8, ACP_HANDLE_NULL);
    acp_copy(it.list.ga + 16, buf + 16, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    it.elem = first;
    
    acp_free(buf);
    return it;
}

void acp_merge_list(acp_list_t list1, acp_list_t list2, int (*comp)(const acp_element_t elem1, const acp_element_t elem2))
{
    acp_ga_t buf = acp_malloc(112, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list1_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list1_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list1_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* list2_head = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* list2_tail = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* list2_num  = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* elem1_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* elem1_prev = (volatile acp_ga_t*)(ptr + 56);
    volatile uint64_t* elem1_size = (volatile uint64_t*)(ptr + 64);
    volatile acp_ga_t* elem2_next = (volatile acp_ga_t*)(ptr + 72);
    volatile acp_ga_t* elem2_prev = (volatile acp_ga_t*)(ptr + 80);
    volatile uint64_t* elem2_size = (volatile uint64_t*)(ptr + 88);
    volatile acp_ga_t* prev_next  = (volatile acp_ga_t*)(ptr + 96);
    volatile acp_ga_t* prev_prev  = (volatile acp_ga_t*)(ptr + 104);
    
    acp_copy(buf, list1.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, list2.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head1 = *list1_head;
    acp_ga_t tmp_tail1 = *list1_tail;
    uint64_t tmp_num1  = *list1_num;
    acp_ga_t tmp_head2 = *list2_head;
    acp_ga_t tmp_tail2 = *list2_tail;
    uint64_t tmp_num2  = *list2_num;
    
    uint64_t tmp_num   = 0;
    acp_ga_t next1     = tmp_head1;
    acp_ga_t next2     = tmp_head2;
    
    /* prepare head elements */
    if (tmp_num1 > 0) {
        acp_copy(buf + 48, next1, 24, ACP_HANDLE_NULL);
        tmp_num1--;
    }
    if (tmp_num2 > 0) {
        acp_copy(buf + 72, next2, 24, ACP_HANDLE_NULL);
        tmp_num2--;
    }
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_next1 = *elem1_next;
    acp_ga_t tmp_prev1 = *elem1_prev;
    uint64_t tmp_size1 = *elem1_size;
    acp_ga_t tmp_next2 = *elem2_next;
    acp_ga_t tmp_prev2 = *elem2_prev;
    uint64_t tmp_size2 = *elem2_size;
    
    /* merge elements into a new list */
    acp_ga_t prev      = list1.ga;
    acp_ga_t first     = ACP_GA_NULL;
    while (tmp_num1 > 0 || tmp_num2 > 0) {
        acp_element_t elem1, elem2;
        int sel;
        
        /* select element */
        if (tmp_num1 == 0) {
            sel = 1;
        } else if (tmp_num2 == 0) {
            sel = -1;
        } else {
            elem1.ga   = next1 + 24;
            elem1.size = tmp_size1;
            elem2.ga   = next2 + 24;
            elem2.size = tmp_size2;
            sel = comp(elem1, elem2);;
        }
        
        /* link element */
        if (sel <= 0) {
            if (first != ACP_GA_NULL) {
                *prev_next = next1;
                acp_copy(prev, buf + 96, 16, ACP_HANDLE_NULL);
            } else
                first = next1;
            acp_copy(buf + 48, tmp_next1, 24, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            tmp_next1 = *elem1_next;
            tmp_prev1 = *elem1_prev;
            tmp_size1 = *elem1_size;
            *prev_prev = prev;
            prev = next1;
            next1 = tmp_next1;
            tmp_num1--;
            tmp_num++;
        } else {
            if (first != ACP_GA_NULL) {
                *prev_next = next2;
                acp_copy(prev, buf + 96, 16, ACP_HANDLE_NULL);
            } else
                first = next2;
            acp_copy(buf + 72, tmp_next2, 24, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            tmp_next2 = *elem2_next;
            tmp_prev2 = *elem2_prev;
            tmp_size2 = *elem2_size;
            *prev_prev = prev;
            prev = next2;
            next2 = tmp_next2;
            tmp_num2--;
            tmp_num++;
        }
    }
    if (first != ACP_GA_NULL) {
        *prev_next = list1.ga;
        acp_copy(prev, buf + 96, 16, ACP_HANDLE_NULL);
    }
    
    /* update list1 and list2 */
    *list1_head = first;
    *list1_tail = prev;
    *list1_num  = tmp_num;
    *list2_head = list2.ga;
    *list2_tail = list2.ga;
    *list2_num  = 0;
    acp_copy(list1.ga, buf, 24, ACP_HANDLE_NULL);
    acp_copy(list2.ga + 24, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_pop_back_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    if (tmp_num == 1) {
        *list_head = list.ga;
        *list_tail = list.ga;
        *list_num  = 0;
        
        acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_tail);
    } else if (tmp_num > 1) {
        acp_copy(buf + 24, tmp_tail, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        
        *list_tail = tmp_prev;
        *list_num  = tmp_num - 1;
        *elem_next = list.ga;
        
        acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_prev, buf + 24, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_tail);
    }
    
    acp_free(buf);
    return;
}

void acp_pop_front_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    if (tmp_num == 1) {
        *list_head = list.ga;
        *list_tail = list.ga;
        *list_num  = 0;
        
        acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(tmp_tail);
    } else if (tmp_num > 1) {
        acp_copy(buf + 24, tmp_head, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        
        *list_head = tmp_next;
        *list_num  = tmp_num - 1;
        *elem_prev = list.ga;
        
        acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_next + 8, buf + 32, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return;
}

void acp_push_back_list(acp_list_t list, const acp_element_t elem, int rank)
{
    acp_ga_t buf = acp_malloc(56, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* elem_ga   = (volatile acp_ga_t*)(ptr + 48);
    
    acp_ga_t new_elem = acp_malloc(24 + elem.size, rank);
    if (new_elem == ACP_GA_NULL) {
        acp_free(buf);
        return;
    }
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    if (tmp_num == 0) {
        *list_head = new_elem;
        *list_tail = new_elem;
        *list_num  = 1;
        *elem_next = list.ga;
        *elem_prev = list.ga;
        *elem_size = elem.size;
        acp_copy(new_elem, buf + 24, 24, ACP_HANDLE_NULL);
        acp_copy(new_elem + 24, elem.ga, elem.size, ACP_HANDLE_NULL);
        acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        *list_tail = new_elem;
        *list_num  = tmp_num + 1;
        *elem_next = list.ga;
        *elem_prev = tmp_tail;
        *elem_size = elem.size;
        *elem_ga   = new_elem;
        acp_copy(new_elem, buf + 24, 24, ACP_HANDLE_NULL);
        acp_copy(new_elem + 24, elem.ga, elem.size, ACP_HANDLE_NULL);
        acp_copy(tmp_tail, buf + 48, 8, ACP_HANDLE_NULL);
        acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return;
}

void acp_push_front_list(acp_list_t list, const acp_element_t elem, int rank)
{
    acp_ga_t buf = acp_malloc(56, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* elem_ga   = (volatile acp_ga_t*)(ptr + 48);
    
    acp_ga_t new_elem = acp_malloc(24 + elem.size, rank);
    if (new_elem == ACP_GA_NULL) {
        acp_free(buf);
        return;
    }
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    if (tmp_num == 0) {
        *list_head = new_elem;
        *list_tail = new_elem;
        *list_num  = 1;
        *elem_next = list.ga;
        *elem_prev = list.ga;
        *elem_size = elem.size;
        acp_copy(new_elem, buf + 24, 24, ACP_HANDLE_NULL);
        acp_copy(new_elem + 24, elem.ga, elem.size, ACP_HANDLE_NULL);
        acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        *list_head = new_elem;
        *list_num  = tmp_num + 1;
        *elem_next = tmp_head;
        *elem_prev = list.ga;
        *elem_size = elem.size;
        *elem_ga   = new_elem;
        acp_copy(new_elem, buf + 24, 24, ACP_HANDLE_NULL);
        acp_copy(new_elem + 24, elem.ga, elem.size, ACP_HANDLE_NULL);
        acp_copy(tmp_head + 8, buf + 48, 8, ACP_HANDLE_NULL);
        acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return;
}

void acp_remove_list(acp_list_t list, const acp_element_t elem)
{
    int local = (acp_query_rank(elem.ga) == acp_rank()) ? 1 : 0;
    int size = local ? elem.size : elem.size + elem.size;
    
    acp_ga_t buf = acp_malloc(64 + size, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* prev_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* prev_prev = (volatile acp_ga_t*)(ptr + 56);
    
    void* p1 = ptr + 64;
    void* p2 = local ? acp_query_address(elem.ga) : ptr + 64 + elem.size;
    if (!local) acp_copy(buf + 64 + elem.size, elem.ga, elem.size, ACP_HANDLE_NULL);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_ga_t next  = tmp_head;
    acp_ga_t prev  = list.ga;
    acp_ga_t first = list.ga;
    while (next != list.ga) {
        int flag, i;
        acp_copy(buf + 24, next, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        flag = 0;
        if (tmp_size == elem.size) {
            acp_copy(buf + 64, next + 24, tmp_size, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            for (i = 0; i < elem.size; i++)
                if (*(unsigned char*)(p1 + i) != *(unsigned char*)(p2 + i)) break;
            if (i == elem.size) {
                acp_free(next);
                tmp_num--;
                flag = 1;
            }
        }
        if (!flag) {
            if (first != list.ga) first = next;
            if (prev != tmp_prev) {
                *prev_next = next;
                *prev_prev = prev;
                acp_copy(prev, buf + 48, 8, ACP_HANDLE_NULL);
                acp_copy(next + 8, buf + 56, 8, ACP_HANDLE_NULL);
            }
            prev = next;
        }
        next = tmp_next;
    }
    
    /* set new list */
    *list_head = first;
    *list_tail = prev;
    *list_num = tmp_num;
    acp_copy(list.ga, buf, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_reverse_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(56, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile acp_ga_t* prev_next = (volatile acp_ga_t*)(ptr + 40);
    volatile acp_ga_t* prev_prev = (volatile acp_ga_t*)(ptr + 48);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_ga_t next  = tmp_head;
    while (next != list.ga) {
        acp_copy(buf + 24, next, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        *prev_next = tmp_prev;
        *prev_prev = tmp_next;
        acp_copy(buf + 40, next, 16, ACP_HANDLE_NULL);
        next = tmp_next;
    }
    
    /* set new list */
    *list_head = tmp_tail;
    *list_tail = tmp_head;
    acp_copy(list.ga, buf, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

size_t acp_size_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_free(buf);
    return (size_t)tmp_num;
}

static inline uint64_t fill_lower_bit(uint64_t x)
{
    x |= x >>  1;
    x |= x >>  2;
    x |= x >>  4;
    x |= x >>  8;
    x |= x >> 16;
    x |= x >> 32;
    return x;
}

static inline int pop_count(uint64_t x)
{
    x = (x & 0x5555555555555555ULL) + ((x >>  1) & 0x5555555555555555ULL);
    x = (x & 0x3333333333333333ULL) + ((x >>  2) & 0x3333333333333333ULL);
    x = (x & 0x0707070707070707ULL) + ((x >>  4) & 0x0707070707070707ULL);
    x = (x & 0x000f000f000f000fULL) + ((x >>  8) & 0x000f000f000f000fULL);
    x = (x & 0x0000001f0000001fULL) + ((x >> 16) & 0x0000001f0000001fULL);
    x = (x & 0x000000000000003fULL) + ((x >> 32) & 0x000000000000003fULL);
    return (int)x;
}

static inline int pop_lower_count(uint64_t x)
{
    return pop_count(x ^ (x + 1)) - 1;
}

void acp_sort_list(acp_list_t list, int (*comp)(const acp_element_t elem1, const acp_element_t elem2))
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_free(buf);
    
    if (tmp_num <= 1) return;
    
    /* merge sort */
    int log2_num = pop_count(fill_lower_bit(tmp_num - 1));
    buf = acp_malloc(16 + 24 * (log2_num + 1), acp_rank());
    if (buf == ACP_GA_NULL) return;
    ptr = acp_query_address(buf);
    volatile acp_ga_t* elem_next = (volatile uint64_t*)ptr;
    volatile acp_ga_t* elem_prev = (volatile uint64_t*)(ptr + 8);
    
    acp_ga_t next = tmp_head;
    uint64_t i;
    acp_list_t list1, list2;
    int p, q;
    list2.ga = buf + 16;
    for (i = 0; i < tmp_num; i++) {
        acp_copy(buf, next, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_ga_t tmp_next = *elem_next;
        p = pop_count(i);
        *(volatile acp_ga_t*)(ptr + 16 + p * 24)      = next;
        *(volatile acp_ga_t*)(ptr + 16 + p * 24 + 8)  = next;
        *(volatile uint64_t*)(ptr + 16 + p * 24 + 16) = 1;
        *elem_next = buf + 16 + p * 24;
        *elem_prev = buf + 16 + p * 24;
        acp_copy(next, buf, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        q = pop_lower_count(i);
        list2.ga = buf + 16 + p * 24;
        while (q > 0) {
            list1.ga = list2.ga - 24;
            acp_merge_list(list1, list2, comp);
            list2.ga = list1.ga;
            q--;
        }
    }
    while (list2.ga > buf + 16) {
        list1.ga = list2.ga - 24;
        acp_merge_list(list1, list2, comp);
        list2.ga = list1.ga;
    }
    
    acp_copy(list.ga, buf + 16, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    return;
}

void acp_splice_list(acp_list_it_t it1, acp_list_it_t it2)
{
    if (it2.elem == it2.list.ga) return;
    
    acp_ga_t buf = acp_malloc(88, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list1_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list1_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list1_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* list2_head = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* list2_tail = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* list2_num  = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* elem1_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* elem1_prev = (volatile acp_ga_t*)(ptr + 56);
    volatile acp_ga_t* elem2_next = (volatile acp_ga_t*)(ptr + 64);
    volatile acp_ga_t* elem2_prev = (volatile acp_ga_t*)(ptr + 72);
    volatile acp_ga_t* elem2_ga   = (volatile acp_ga_t*)(ptr + 80);
    
    acp_copy(buf, it1.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, it2.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 48, it1.elem, 16, ACP_HANDLE_NULL);
    acp_copy(buf + 64, it2.elem, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head1 = *list1_head;
    acp_ga_t tmp_tail1 = *list1_tail;
    uint64_t tmp_num1  = *list1_num;
    acp_ga_t tmp_head2 = *list2_head;
    acp_ga_t tmp_tail2 = *list2_tail;
    uint64_t tmp_num2  = *list2_num;
    acp_ga_t tmp_next1 = *elem1_next;
    acp_ga_t tmp_prev1 = *elem1_prev;
    acp_ga_t tmp_next2 = *elem2_next;
    acp_ga_t tmp_prev2 = *elem2_prev;
    
    /* remove an element at it2 */
    acp_copy(tmp_prev2, buf + 64, 8, ACP_HANDLE_NULL);
    acp_copy(tmp_next2 + 8, buf + 72, 8, ACP_HANDLE_NULL);
    
    /* insert the element before it1 */
    *elem1_next = it1.elem;
    *elem1_prev = tmp_prev1;
    acp_copy(it2.elem, buf + 48, 16, ACP_HANDLE_NULL);
    
    *elem2_ga = it2.elem;
    acp_copy(tmp_prev1, buf + 80, 8, ACP_HANDLE_NULL);
    acp_copy(it1.elem + 8, buf + 80, 8, ACP_HANDLE_NULL);
    
    /* modify the numbers of elements */
    *list1_num  = tmp_num1 + 1;
    *list2_num  = tmp_num2 - 1;
    acp_copy(it1.list.ga + 16, buf + 16, 8, ACP_HANDLE_NULL);
    acp_copy(it2.list.ga + 16, buf + 40, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_splice_range_list(acp_list_it_t it, acp_list_it_t start, acp_list_it_t end)
{
    if (start.list.ga != end.list.ga || end.elem == end.list.ga ) return;
    
    acp_ga_t buf = acp_malloc(112, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list1_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list1_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list1_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* list2_head = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* list2_tail = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* list2_num  = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* elem1_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* elem1_prev = (volatile acp_ga_t*)(ptr + 56);
    volatile acp_ga_t* elem2_next = (volatile acp_ga_t*)(ptr + 64);
    volatile acp_ga_t* elem2_prev = (volatile acp_ga_t*)(ptr + 72);
    volatile acp_ga_t* elem3_next = (volatile acp_ga_t*)(ptr + 80);
    volatile acp_ga_t* elem3_prev = (volatile acp_ga_t*)(ptr + 88);
    volatile acp_ga_t* elem2_ga   = (volatile acp_ga_t*)(ptr + 96);
    volatile acp_ga_t* elem3_ga   = (volatile acp_ga_t*)(ptr + 104);
    
    acp_copy(buf, it.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, start.list.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 48, it.elem, 16, ACP_HANDLE_NULL);
    acp_copy(buf + 64, start.elem, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head1 = *list1_head;
    acp_ga_t tmp_tail1 = *list1_tail;
    uint64_t tmp_num1  = *list1_num;
    acp_ga_t tmp_head2 = *list2_head;
    acp_ga_t tmp_tail2 = *list2_tail;
    uint64_t tmp_num2  = *list2_num;
    acp_ga_t tmp_next1 = *elem1_next;
    acp_ga_t tmp_prev1 = *elem1_prev;
    acp_ga_t tmp_next2 = *elem2_next;
    acp_ga_t tmp_prev2 = *elem2_prev;
    
    acp_ga_t tmp_next3 = tmp_next2;
    acp_ga_t tmp_prev3 = tmp_prev2;
    *elem3_next = tmp_next2;
    *elem3_prev = tmp_prev2;
    acp_ga_t prev = start.elem;
    int n = 1;
    while (tmp_next3 != start.list.ga && tmp_next3 != end.elem) {
        prev = tmp_next3;
        acp_copy(buf + 80, tmp_next3, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        tmp_next3 = *elem2_next;
        tmp_prev3 = *elem2_prev;
        n++;
    }
    
    /* remove elements */
    acp_copy(tmp_prev2, buf + 80, 8, ACP_HANDLE_NULL);
    acp_copy(tmp_next3 + 8, buf + 72, 8, ACP_HANDLE_NULL);
    
    /* insert the element before it */
    *elem1_next = tmp_next3;
    *elem1_prev = tmp_prev1;
    acp_copy(prev, buf + 48, 16, ACP_HANDLE_NULL);
    acp_copy(start.elem + 8, buf + 56, 16, ACP_HANDLE_NULL);
    
    *elem2_ga = start.elem;
    *elem3_ga = prev;
    acp_copy(tmp_prev1, buf + 96, 8, ACP_HANDLE_NULL);
    acp_copy(tmp_next3 + 8, buf + 104, 8, ACP_HANDLE_NULL);
    
    /* modify the numbers of elements */
    *list1_num  = tmp_num1 + n;
    *list2_num  = tmp_num2 - n;
    acp_copy(it.list.ga + 16, buf + 16, 8, ACP_HANDLE_NULL);
    acp_copy(start.list.ga + 16, buf + 40, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_swap_list(acp_list_t list1, acp_list_t list2)
{
    acp_ga_t buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list1_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list1_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list1_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* list2_head = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* list2_tail = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* list2_num  = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* list1_ga   = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* list2_ga   = (volatile acp_ga_t*)(ptr + 56);
    
    acp_copy(buf, list1.ga, 24, ACP_HANDLE_NULL);
    acp_copy(buf + 24, list2.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head1 = *list1_head;
    acp_ga_t tmp_tail1 = *list1_tail;
    uint64_t tmp_num1  = *list1_num;
    acp_ga_t tmp_head2 = *list2_head;
    acp_ga_t tmp_tail2 = *list2_tail;
    uint64_t tmp_num2  = *list2_num;
    *list1_ga = list1.ga;
    *list2_ga = list2.ga;
    
    if (tmp_num1 > 0 && tmp_num2 > 0) {
        acp_copy(list2.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(list1.ga, buf + 24, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_head2 + 8, buf + 48, 8, ACP_HANDLE_NULL);
        acp_copy(tmp_tail2, buf + 48, 8, ACP_HANDLE_NULL);
        acp_copy(tmp_head1 + 8, buf + 56, 8, ACP_HANDLE_NULL);
        acp_copy(tmp_tail1, buf + 56, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else if (tmp_num1 > 0) {
        *list2_head = list1.ga;
        *list2_tail = list1.ga;
        acp_copy(list2.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(list1.ga, buf + 24, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_head1 + 8, buf + 56, 8, ACP_HANDLE_NULL);
        acp_copy(tmp_tail1, buf + 56, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else if (tmp_num2 > 0) {
        *list1_head = list2.ga;
        *list1_tail = list2.ga;
        acp_copy(list2.ga, buf, 24, ACP_HANDLE_NULL);
        acp_copy(list1.ga, buf + 24, 24, ACP_HANDLE_NULL);
        acp_copy(tmp_head2 + 8, buf + 48, 8, ACP_HANDLE_NULL);
        acp_copy(tmp_tail2, buf + 48, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return;
}

void acp_unique_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* prev_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* prev_prev = (volatile acp_ga_t*)(ptr + 56);
    
    acp_copy(buf, list.ga, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_head = *list_head;
    acp_ga_t tmp_tail = *list_tail;
    uint64_t tmp_num  = *list_num;
    
    acp_ga_t next     = tmp_head;
    acp_ga_t prev     = list.ga;
    uint64_t size     = 0;
    acp_ga_t buf2     = ACP_GA_NULL;
    acp_ga_t cache    = ACP_GA_NULL;
    uint64_t capacity = 0;
    while (next != list.ga) {
        acp_copy(buf + 24, next, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        if (prev != tmp_prev) {
            *prev_prev = prev;
            acp_copy(next + 8, buf + 56, 8, ACP_HANDLE_NULL);
        }
        if (prev != list.ga && tmp_size == size) {
            if (capacity < size) {
                if (capacity > 0) acp_free(buf2);
                buf2 = acp_malloc(capacity + capacity, acp_rank());
                ptr = acp_query_address(buf2);
                capacity = size;
            }
            if (buf2 != ACP_GA_NULL) {
                if (cache != prev) {
                    acp_copy(buf2, prev + 24, size, ACP_HANDLE_NULL);
                    cache = prev;
                }
                acp_copy(buf2 + size, next + 24, size, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                int i;
                for (i = 0; i < size; i++)
                    if (*(unsigned char*)(ptr + i) != *(unsigned char*)(ptr + size + i)) break;
                if (i == size) {
                    acp_free(next);
                    acp_copy(prev, buf + 24, 8, ACP_HANDLE_NULL);
                    acp_copy(tmp_next + 8, buf + 32, 8, ACP_HANDLE_NULL);
                    acp_complete(ACP_HANDLE_ALL);
                    next = prev;
                    tmp_num--;
                } else {
                    for (i = 0; i < size; i++)
                        *(unsigned char*)(ptr + i) = *(unsigned char*)(ptr + size + i);
                    cache = next;
                }
            }
        }
        prev = next;
        next = tmp_next;
        size = tmp_size;
    }
    *list_tail = prev;
    acp_copy(list.ga + 8, buf + 8, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (buf2 != ACP_GA_NULL) acp_free(buf2);
    acp_free(buf);
    return;
}

acp_list_it_t acp_advance_list_it(acp_list_it_t it, int n)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 16);
    
    acp_ga_t next = it.elem;
    
    while (next != it.list.ga && n-- > 0) {
        acp_copy(buf, next, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        
        next = tmp_next;
    }
    
    acp_free(buf);
    
    it.elem = next;
    return it;
}

acp_list_it_t acp_decrement_list_it(acp_list_it_t it)
{
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, it.elem, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_next = *elem_next;
    acp_ga_t tmp_prev = *elem_prev;
    uint64_t tmp_size = *elem_size;
    
    acp_free(buf);
    
    if (tmp_prev != it.list.ga) it.elem = tmp_prev;
    return it;
}

acp_element_t acp_dereference_list_it(acp_list_it_t it)
{
    acp_element_t elem;
    elem.ga = ACP_GA_NULL;
    elem.size = 0;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return elem;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, it.elem, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_next = *elem_next;
    acp_ga_t tmp_prev = *elem_prev;
    uint64_t tmp_size = *elem_size;
    
    acp_free(buf);
    
    elem.ga = it.elem + 24;
    elem.size = tmp_size;
    return elem;
}

int acp_distance_list_it(acp_list_it_t first, acp_list_it_t last)
{
    if (first.list.ga != last.list.ga) return 0;
    
    acp_ga_t buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 16);
    
    int n = 0;
    acp_ga_t next = first.elem;
    while (next != first.list.ga && next != last.elem) {
        acp_copy(buf, next, 24, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        
        acp_ga_t tmp_next = *elem_next;
        acp_ga_t tmp_prev = *elem_prev;
        uint64_t tmp_size = *elem_size;
        
        n++;
        next = tmp_next;
    }
    
    acp_free(buf);
    
    return n;
}

acp_list_it_t acp_increment_list_it(acp_list_it_t it)
{
    acp_ga_t buf;
    if (it.elem == it.list.ga)
        return it;
    buf = acp_malloc(24, acp_rank());
    if (buf == ACP_GA_NULL) return it;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 16);
    
    acp_copy(buf, it.elem, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_ga_t tmp_next = *elem_next;
    acp_ga_t tmp_prev = *elem_prev;
    uint64_t tmp_size = *elem_size;
    
    acp_free(buf);
    
    it.elem = tmp_next;
    return it;
}

/** Set
 *      [0-] directory of hash table
 ** slot of hash table ([1-3] = list control)
 *      [0]  lock
 *      [1]  ga of head element
 *      [2]  ga of tail element
 *      [3]  number of elements
 ** element of list
 *      [0]  ga of next element
 *      [1]  ga of previos element
 *      [2]  size of key
 *      [3-] key
 */

static void iacp_clear_list_set(volatile uint64_t* list, acp_ga_t buf_elem, volatile uint64_t* elem)
{
    /* slot must be locked */
    /* list must have data */
    
    /*** local buffer variables ***/
    
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
    
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

static int iacp_insert_set(acp_set_t set, acp_element_t key, acp_ga_t buf_directory, acp_ga_t buf_lock_var, acp_ga_t buf_list, acp_ga_t buf_elem, acp_ga_t buf_new_key, acp_ga_t buf_elem_key, volatile acp_ga_t* directory, volatile uint64_t* lock_var, volatile uint64_t* list, volatile uint64_t* elem, volatile uint8_t* new_key, volatile uint8_t* elem_key)
{
    /* directory must have data */
    /* elem, new_key and new_value must be continuous */
    
    /*** local buffer variables ***/
    
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
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
        if (elem[2] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                return 0;
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
    elem[2] = size_new_key;
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

static inline acp_set_it_t iacp_null_set_it(acp_set_t set)
{
    acp_set_it_t it;
    it.set = set;
    it.rank = set.num_ranks;
    it.slot = 0;
    it.elem = ACP_GA_NULL;
    return it;
}

void acp_assign_local_set(acp_set_t set1, acp_set_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 24;
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
    void* ptr = acp_query_address(buf);
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
            iacp_clear_list_set(list, buf_elem2, elem2);
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
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 24;
                uint64_t size_new_key   = elem2[2];
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
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_set(set1, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_assign_set(acp_set_t set1, acp_set_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 24;
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
    void* ptr = acp_query_address(buf);
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
            iacp_clear_list_set(list, buf_elem2, elem2);
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
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 24;
                uint64_t size_new_key   = elem2[2];
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
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_set(set1, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

acp_set_it_t acp_begin_local_set(acp_set_t set)
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
    if (buf == ACP_GA_NULL) return iacp_null_set_it(set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    void* ptr = acp_query_address(buf);
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
                acp_set_it_t it;
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
    return iacp_null_set_it(set);
}

acp_set_it_t acp_begin_set(acp_set_t set)
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
    if (buf == ACP_GA_NULL) return iacp_null_set_it(set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    void* ptr = acp_query_address(buf);
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
                acp_set_it_t it;
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
    return iacp_null_set_it(set);
}

void acp_clear_local_set(acp_set_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** clear set ***/
    
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
            iacp_clear_list_set(list, buf_elem, elem);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    acp_free(buf);
    return;
}

void acp_clear_set(acp_set_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** clear set ***/
    
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
            iacp_clear_list_set(list, buf_elem, elem);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    acp_free(buf);
    return;
}

acp_set_t acp_create_set(int num_ranks, const int* ranks, int num_slots, int rank)
{
    acp_set_t set;
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
    void* ptr = acp_query_address(buf);
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

void acp_destroy_set(acp_set_t set)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = set.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
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
    void* ptr = acp_query_address(buf);
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
            iacp_clear_list_set(list, buf_elem, elem);
        }
        acp_free(ga_table);
    }
    
    acp_free(set.ga);
    acp_free(buf);
    return;
}

int acp_empty_local_set(acp_set_t set)
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
    void* ptr = acp_query_address(buf);
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

int acp_empty_set(acp_set_t set)
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
    void* ptr = acp_query_address(buf);
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

acp_set_it_t acp_end_local_set(acp_set_t set)
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
    if (buf == ACP_GA_NULL) return iacp_null_set_it(set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    void* ptr = acp_query_address(buf);
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
                acp_set_it_t it;
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
    return iacp_null_set_it(set);
}

acp_set_it_t acp_end_set(acp_set_t set)
{
    return iacp_null_set_it(set);
}

acp_set_it_t acp_find_set(acp_set_t set, acp_element_t key)
{
    if (key.size == 0) return iacp_null_set_it(set);
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
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
    if (buf == ACP_GA_NULL) return iacp_null_set_it(set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    acp_ga_t buf_new_key   = buf + offset_new_key;
    acp_ga_t buf_elem_key  = buf + offset_elem_key;
    void* ptr = acp_query_address(buf);
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
        if (elem[2] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                acp_set_it_t it;
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
    return iacp_null_set_it(set);
}

int acp_insert_set(acp_set_t set, acp_element_t key)
{
    if (key.size == 0) return 0;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
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
    void* ptr = acp_query_address(buf);
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
        if (elem[2] == size_new_key) {
            acp_copy(buf_elem_key, ga_elem + size_elem, size_new_key, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            uint8_t* p1 = (uint8_t*)elem_key;
            uint8_t* p2 = (uint8_t*)new_key;
            int i;
            for (i = 0; i < size_new_key; i++) if (*p1++ != *p2++) break;
            if (i == size_new_key) {
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                acp_free(buf);
                return 0;
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
    elem[2] = size_new_key;
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

void acp_merge_local_set(acp_set_t set1, acp_set_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 24;
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
    void* ptr = acp_query_address(buf);
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
                
                /*** insert key ***/
                
                uint64_t size_elem      = 24;
                uint64_t size_new_key   = elem2[2];
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
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size  = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_set(set1, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_merge_set(acp_set_t set1, acp_set_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 24;
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
    void* ptr = acp_query_address(buf);
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
                
                /*** insert key ***/
                
                uint64_t size_elem      = 24;
                uint64_t size_new_key   = elem2[2];
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
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size  = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_set(set1, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_move_local_set(acp_set_t set1, acp_set_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 24;
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
    void* ptr = acp_query_address(buf);
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
                
                /*** insert key ***/
                
                uint64_t size_elem      = 24;
                uint64_t size_new_key   = elem2[2];
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
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size  = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_set(set1, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
                
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

void acp_move_set(acp_set_t set1, acp_set_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set1.num_ranks * 8;
    uint64_t size_directory2 = set2.num_ranks * 8;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem2      = 24;
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
    void* ptr = acp_query_address(buf);
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
                
                /*** insert key ***/
                
                uint64_t size_elem      = 24;
                uint64_t size_new_key   = elem2[2];
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
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_element_t key;
                key.size  = size_new_key;
                key.ga  = ga_elem + size_elem;
                
                iacp_insert_set(set1, key, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_elem_key, directory, lock_var, list, elem, new_key, elem_key);
                
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

void acp_remove_set(acp_set_t set, acp_element_t key)
{
    if (key.size == 0) return;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
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
    void* ptr = acp_query_address(buf);
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
        if (elem[2] == size_new_key) {
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

size_t acp_size_local_set(acp_set_t set)
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
    void* ptr = acp_query_address(buf);
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
        for (slot = 0; slot < set.num_slots; slot++) ret += table[slot * 32 + 2];
    }
    
    acp_free(buf);
    return ret;
}

size_t acp_size_set(acp_set_t set)
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
    void* ptr = acp_query_address(buf);
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
        for (slot = 0; slot < set.num_slots; slot++) ret += table[slot * 32 + 2];
    }
    
    acp_free(buf);
    return ret;
}

void acp_swap_set(acp_set_t set1, acp_set_t set2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = set2.num_ranks * 8;
    uint64_t size_directory2 = size_directory;
    uint64_t size_table      = set2.num_slots * 32;
    uint64_t size_lock_var   = 8;
    uint64_t size_list       = 24;
    uint64_t size_elem       = 24;
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
    void* ptr = acp_query_address(buf);
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
    
    acp_set_t set;
    set.ga = buf_directory;
    set.num_ranks = set2.num_ranks;
    set.num_slots = set2.num_slots;
    
    /*** move set1 to set2 ***/
    
    acp_move_set(set2, set1);
    
    /*** move temporal set to set1 ***/
    
    acp_move_set(set1, set);
    
    /*** destroy temporal set ***/
    
    for (rank = 0; rank < set.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < set.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            iacp_clear_list_set(list, buf_elem, elem);
        }
        acp_free(ga_table);
    }
    
    acp_free(buf);
    
    return;
}

acp_element_t acp_dereference_set_it(acp_set_it_t it)
{
    acp_element_t key;
    key.ga    = ACP_GA_NULL;
    key.size  = 0;
    if (it.rank >= it.set.num_ranks || it.slot >= it.set.num_slots || it.elem == ACP_GA_NULL) return key;
    
    /*** local buffer variables ***/
    
    uint64_t size_elem      = 24;
    uint64_t offset_elem      = 0;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return key;
    acp_ga_t buf_elem      = buf + offset_elem;
    void* ptr = acp_query_address(buf);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** translate element into pair ***/
    
    acp_copy(buf_elem, it.elem, size_elem, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t key_size = elem[2];
    
    key.ga    = it.elem + size_elem;
    key.size  = key_size;
    return key;
}

acp_set_it_t acp_increment_set_it(acp_set_it_t it)
{
    if (it.rank >= it.set.num_ranks || it.slot >= it.set.num_slots || it.elem == ACP_GA_NULL) return iacp_null_set_it(it.set);
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_set_it(it.set);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    int rank = it.rank;
    int slot = it.slot;
    
    /*** search next element ***/
    
    acp_copy(buf_elem, it.elem, size_elem, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (elem[0] != ACP_GA_NULL) {
        it.elem = elem[0];
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
            return it;
        }
    }
    
    return iacp_null_set_it(it.set);
}

/** Map
 *      [0-] directory of hash table
 ** slot of hash table ([1-3] = list control)
 *      [0]  lock
 *      [1]  ga of head element
 *      [2]  ga of tail element
 *      [3]  number of elements
 ** element of list
 *      [0]  ga of next element (or ACP_GA_NULL at tail)
 *      [1]  ga of previos element (or ACP_GA_NULL at head)
 *      [2]  size of this element
 *      [3]  size of key
 *      [4-] key + value
 */

static void iacp_clear_list_map(volatile uint64_t* list, acp_ga_t buf_elem, volatile uint64_t* elem)
{
    /* slot must be locked */
    /* list must have data */
    
    /*** local buffer variables ***/
    
    uint64_t size_list      = 24;
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

static int iacp_insert_map(acp_map_t map, acp_pair_t pair, acp_ga_t buf_directory, acp_ga_t buf_lock_var, acp_ga_t buf_list, acp_ga_t buf_elem, acp_ga_t buf_new_key, acp_ga_t buf_new_value, acp_ga_t buf_elem_key, volatile acp_ga_t* directory, volatile uint64_t* lock_var, volatile uint64_t* list, volatile uint64_t* elem, volatile uint8_t* new_key, volatile uint8_t* new_value, volatile uint8_t* elem_key)
{
    /* directory must have data */
    /* elem, new_key and new_value must be continuous */
    
    /*** local buffer variables ***/
    
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = pair.first.size;
    uint64_t size_new_value = pair.second.size;
    
    /*** read key-value pair ***/
    
    int pattern = (acp_query_rank(pair.first.ga) == acp_rank() ? 2 : 0) + (acp_query_rank(pair.second.ga) == acp_rank() ? 1 : 0);
    if (pattern == 0) {
        if (pair.second.ga == pair.first.ga + size_new_key) {
            acp_copy(buf_new_key, pair.first.ga, size_new_key + size_new_value, ACP_HANDLE_NULL);
        } else {
            acp_copy(buf_new_key, pair.first.ga, size_new_key, ACP_HANDLE_NULL);
            acp_copy(buf_new_value, pair.second.ga, size_new_value, ACP_HANDLE_NULL);
        }
        acp_complete(ACP_HANDLE_ALL);
    } else if (pattern == 1) {
        acp_copy(buf_new_key, pair.first.ga, size_new_key, ACP_HANDLE_NULL);
        memcpy((void*)new_value, acp_query_address(pair.second.ga), size_new_value);
        acp_complete(ACP_HANDLE_ALL);
    } else if (pattern == 2) {
        acp_copy(buf_new_value, pair.second.ga, size_new_value, ACP_HANDLE_NULL);
        memcpy((void*)new_key, acp_query_address(pair.first.ga), size_new_key);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        if (pair.second.ga == pair.first.ga + size_new_key) {
            memcpy((void*)new_key, acp_query_address(pair.first.ga), size_new_key + size_new_value);
        } else {
            memcpy((void*)new_key, acp_query_address(pair.first.ga), size_new_key);
            memcpy((void*)new_value, acp_query_address(pair.second.ga), size_new_value);
        }
    }
    
    /*** insert key-value pair ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / map.num_slots) % map.num_ranks;
    int slot = crc % map.num_slots;
    
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
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                return 0;
            }
        }
    }
    
    acp_ga_t ga_new_elem = acp_malloc(size_elem + size_new_key + size_new_value, acp_query_rank(ga_list));
    if (ga_new_elem == ACP_GA_NULL) {
        acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        return 0;
    }
    
    elem[0] = ACP_GA_NULL;
    elem[2] = 8 + size_new_key + size_new_value;
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
    
    acp_copy(ga_new_elem, buf_elem, size_elem + size_new_key + size_new_value, ACP_HANDLE_NULL);
    acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    
    return 1;
}

static inline acp_map_it_t iacp_null_map_it(acp_map_t map)
{
    acp_map_it_t it;
    it.map = map;
    it.rank = map.num_ranks;
    it.slot = 0;
    it.elem = ACP_GA_NULL;
    return it;
}

void acp_assign_local_map(acp_map_t map1, acp_map_t map2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = map1.num_ranks * 8;
    uint64_t size_directory2 = map2.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    /*** clear map1 ***/
    
    acp_copy(buf_directory, map1.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < map1.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < map1.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_map(list, buf_elem2, elem2);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    /*** local buffer variables ***/
    
    acp_copy(buf_directory2, map2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of map2 ***/
    
    int my_rank = acp_rank();
    
    for (rank = 0; rank < map2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < map2.num_slots; slot++) {
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
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_new_value = elem2[2] - 8 - size_new_key;
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_new_value = offset_new_key   + size_new_key;
                uint64_t offset_elem_key  = offset_new_value + size_new_value;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_new_value;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* new_value;
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
                    buf_new_value = buf_elem + offset_new_value;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    new_value = (volatile uint8_t*)(ptr + offset_new_value);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_pair_t pair;
                pair.first.size  = size_new_key;
                pair.second.size = size_new_value;
                pair.first.ga  = ga_elem + size_elem;
                pair.second.ga = ga_elem + size_elem + size_new_key;
                
                iacp_insert_map(map1, pair, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_new_value, buf_elem_key, directory, lock_var, list, elem, new_key, new_value, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_assign_map(acp_map_t map1, acp_map_t map2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = map1.num_ranks * 8;
    uint64_t size_directory2 = map2.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    /*** clear map1 ***/
    
    acp_copy(buf_directory, map1.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < map1.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < map1.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_map(list, buf_elem2, elem2);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    /*** local buffer variables ***/
    
    acp_copy(buf_directory2, map2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of map2 ***/
    
    for (rank = 0; rank < map2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        for (slot = 0; slot < map2.num_slots; slot++) {
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
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_new_value = elem2[2] - 8 - size_new_key;
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_new_value = offset_new_key   + size_new_key;
                uint64_t offset_elem_key  = offset_new_value + size_new_value;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_new_value;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* new_value;
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
                    buf_new_value = buf_elem + offset_new_value;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    new_value = (volatile uint8_t*)(ptr + offset_new_value);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_pair_t pair;
                pair.first.size  = size_new_key;
                pair.second.size = size_new_value;
                pair.first.ga  = ga_elem + size_elem;
                pair.second.ga = ga_elem + size_elem + size_new_key;
                
                iacp_insert_map(map1, pair, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_new_value, buf_elem_key, directory, lock_var, list, elem, new_key, new_value, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

acp_map_it_t acp_begin_local_map(acp_map_t map)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t size_buf         = offset_list      + size_list;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_map_it(map);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** search first element ***/
    
    acp_copy(buf_directory, map.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < map.num_slots; slot++) {
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
                acp_map_it_t it;
                it.map = map;
                it.rank = rank;
                it.slot = slot;
                it.elem = list[0];
                acp_free(buf);
                return it;
            }
        }
    }
    
    acp_free(buf);
    return iacp_null_map_it(map);
}

acp_map_it_t acp_begin_map(acp_map_t map)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t size_buf         = offset_list      + size_list;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_map_it(map);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** search first element ***/
    
    acp_copy(buf_directory, map.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < map.num_slots; slot++) {
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
                acp_map_it_t it;
                it.map = map;
                it.rank = rank;
                it.slot = slot;
                it.elem = list[0];
                acp_free(buf);
                return it;
            }
        }
    }
    
    acp_free(buf);
    return iacp_null_map_it(map);
}

void acp_clear_local_map(acp_map_t map)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** clear map ***/
    
    acp_copy(buf_directory, map.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < map.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_map(list, buf_elem, elem);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    acp_free(buf);
    return;
}

void acp_clear_map(acp_map_t map)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** clear map ***/
    
    acp_copy(buf_directory, map.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < map.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_map(list, buf_elem, elem);
            acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    acp_free(buf);
    return;
}

acp_map_t acp_create_map(int num_ranks, const int* ranks, int num_slots, int rank)
{
    acp_map_t map;
    map.ga = ACP_GA_NULL;
    map.num_ranks = num_ranks;
    map.num_slots = num_slots;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = num_ranks * 8;
    uint64_t size_table     = num_slots * 32;
    uint64_t offset_directory = 0;
    uint64_t offset_table     = offset_directory + size_directory;
    uint64_t size_buf         = offset_table     + size_table;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return map;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_table     = buf + offset_table;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* table     = (volatile uint64_t*)(ptr + offset_table);
    
    int slot;
    
    for (slot = 0; slot < num_slots; slot++) {
        table[slot * 4 + 0] = 0;
        table[slot * 4 + 1] = ACP_GA_NULL;
        table[slot * 4 + 2] = ACP_GA_NULL;
        table[slot * 4 + 3] = 0;
    }
    
    /*** create map ***/
    
    map.ga = acp_malloc(size_directory, rank);
    if (map.ga == ACP_GA_NULL) {
        acp_free(buf);
        return map;
    }
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = acp_malloc(size_table, ranks[rank]);
        if (ga_table == ACP_GA_NULL) {
            while (rank > 0) acp_free(directory[--rank]);
            acp_free(map.ga);
            acp_free(buf);
            map.ga = ACP_GA_NULL;
            return map;
        }
        acp_copy(ga_table, buf_table, size_table, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        directory[rank] = ga_table;
    }
    acp_copy(map.ga, buf_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return map;
}

void acp_destroy_map(acp_map_t map)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** destroy map ***/
    
    acp_copy(buf_directory, map.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < map.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            do {
                acp_cas8(buf_lock_var, ga_lock_var, 0, 1, ACP_HANDLE_NULL);
                acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
            } while (*lock_var != 0);
            iacp_clear_list_map(list, buf_elem, elem);
        }
        acp_free(ga_table);
    }
    
    acp_free(map.ga);
    acp_free(buf);
    return;
}

int acp_empty_local_map(acp_map_t map)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** seak slots ***/
    
    acp_copy(buf_directory, map.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < map.num_slots; slot++) {
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

int acp_empty_map(acp_map_t map)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** seak slots ***/
    
    acp_copy(buf_directory, map.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < map.num_slots; slot++) {
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

acp_map_it_t acp_end_local_map(acp_map_t map)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t size_buf         = offset_list      + size_list;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return iacp_null_map_it(map);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    
    /*** search first element of next slot ***/
    
    acp_copy(buf_directory, map.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 1; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(directory[rank - 1]) != my_rank) continue;
        for (slot = 0; slot < map.num_slots; slot++) {
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
                acp_map_it_t it;
                it.map = map;
                it.rank = rank;
                it.slot = slot;
                it.elem = list[0];
                acp_free(buf);
                return it;
            }
        }
    }
    
    acp_free(buf);
    return iacp_null_map_it(map);
}

acp_map_it_t acp_end_map(acp_map_t map)
{
    return iacp_null_map_it(map);
}

acp_map_it_t acp_find_map(acp_map_t map, acp_element_t key)
{
    if (key.size == 0) return iacp_null_map_it(map);
    
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
    if (buf == ACP_GA_NULL) return iacp_null_map_it(map);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    acp_ga_t buf_new_key   = buf + offset_new_key;
    acp_ga_t buf_elem_key  = buf + offset_elem_key;
    void* ptr = acp_query_address(buf);
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
    int rank = (crc / map.num_slots) % map.num_ranks;
    int slot = crc % map.num_slots;
    
    acp_ga_t ga_directory = map.ga + rank * 8;
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
                acp_map_it_t it;
                it.map = map;
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
    return iacp_null_map_it(map);
}

int acp_insert_map(acp_map_t map, acp_pair_t pair)
{
    if (pair.first.size == 0) return 0;
    if (pair.second.size == 0) {
        acp_remove_map(map, pair.first);
        return 0;
    }
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = pair.first.size;
    uint64_t size_new_value = pair.second.size;
    uint64_t size_elem_key  = size_new_key;
    uint64_t offset_directory = 0;
    uint64_t offset_lock_var  = offset_directory + size_directory;
    uint64_t offset_list      = offset_lock_var  + size_lock_var;
    uint64_t offset_elem      = offset_list      + size_list;
    uint64_t offset_new_key   = offset_elem      + size_elem;
    uint64_t offset_new_value = offset_new_key   + size_new_key;
    uint64_t offset_elem_key  = offset_new_value + size_new_value;
    uint64_t size_buf         = offset_elem_key  + size_elem_key;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return 0;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    acp_ga_t buf_new_key   = buf + offset_new_key;
    acp_ga_t buf_new_value = buf + offset_new_value;
    acp_ga_t buf_elem_key  = buf + offset_elem_key;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    volatile uint8_t* new_key    = (volatile uint8_t*)(ptr + offset_new_key);
    volatile uint8_t* new_value  = (volatile uint8_t*)(ptr + offset_new_value);
    volatile uint8_t* elem_key   = (volatile uint8_t*)(ptr + offset_elem_key);
    
    /*** read key-value pair ***/
    
    int pattern = (acp_query_rank(pair.first.ga) == acp_rank() ? 2 : 0) + (acp_query_rank(pair.second.ga) == acp_rank() ? 1 : 0);
    if (pattern == 0) {
        if (pair.second.ga == pair.first.ga + size_new_key) {
            acp_copy(buf_new_key, pair.first.ga, size_new_key + size_new_value, ACP_HANDLE_NULL);
        } else {
            acp_copy(buf_new_key, pair.first.ga, size_new_key, ACP_HANDLE_NULL);
            acp_copy(buf_new_value, pair.second.ga, size_new_value, ACP_HANDLE_NULL);
        }
        acp_complete(ACP_HANDLE_ALL);
    } else if (pattern == 1) {
        acp_copy(buf_new_key, pair.first.ga, size_new_key, ACP_HANDLE_NULL);
        memcpy((void*)new_value, acp_query_address(pair.second.ga), size_new_value);
        acp_complete(ACP_HANDLE_ALL);
    } else if (pattern == 2) {
        acp_copy(buf_new_value, pair.second.ga, size_new_value, ACP_HANDLE_NULL);
        memcpy((void*)new_key, acp_query_address(pair.first.ga), size_new_key);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        if (pair.second.ga == pair.first.ga + size_new_key) {
            memcpy((void*)new_key, acp_query_address(pair.first.ga), size_new_key + size_new_value);
        } else {
            memcpy((void*)new_key, acp_query_address(pair.first.ga), size_new_key);
            memcpy((void*)new_value, acp_query_address(pair.second.ga), size_new_value);
        }
    }
    
    /*** insert key-value pair ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / map.num_slots) % map.num_ranks;
    int slot = crc % map.num_slots;
    
    acp_ga_t ga_directory = map.ga + rank * 8;
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
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_NULL);
                acp_complete(ACP_HANDLE_ALL);
                acp_free(buf);
                return 0;
            }
        }
    }
    
    acp_ga_t ga_new_elem = acp_malloc(size_elem + size_new_key + size_new_value, acp_query_rank(ga_list));
    if (ga_new_elem == ACP_GA_NULL) {
        acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(buf);
        return 0;
    }
    
    elem[0] = ACP_GA_NULL;
    elem[2] = 8 + size_new_key + size_new_value;
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
    
    acp_copy(ga_new_elem, buf_elem, size_elem + size_new_key + size_new_value, ACP_HANDLE_NULL);
    acp_copy(ga_list, buf_list, size_list, ACP_HANDLE_NULL);
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return 1;
}

void acp_merge_local_map(acp_map_t map1, acp_map_t map2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = map1.num_ranks * 8;
    uint64_t size_directory2 = map2.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    acp_copy(buf_directory, map1.ga, size_directory, ACP_HANDLE_NULL);
    acp_copy(buf_directory2, map2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of map2 ***/
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < map2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < map2.num_slots; slot++) {
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
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_new_value = elem2[2] - 8 - size_new_key;
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_new_value = offset_new_key   + size_new_key;
                uint64_t offset_elem_key  = offset_new_value + size_new_value;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_new_value;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* new_value;
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
                    buf_new_value = buf_elem + offset_new_value;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    new_value = (volatile uint8_t*)(ptr + offset_new_value);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_pair_t pair;
                pair.first.size  = size_new_key;
                pair.second.size = size_new_value;
                pair.first.ga  = ga_elem + size_elem;
                pair.second.ga = ga_elem + size_elem + size_new_key;
                
                iacp_insert_map(map1, pair, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_new_value, buf_elem_key, directory, lock_var, list, elem, new_key, new_value, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_merge_map(acp_map_t map1, acp_map_t map2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = map1.num_ranks * 8;
    uint64_t size_directory2 = map2.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    acp_copy(buf_directory, map1.ga, size_directory, ACP_HANDLE_NULL);
    acp_copy(buf_directory2, map2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of map2 ***/
    
    int rank, slot;
    
    for (rank = 0; rank < map2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        for (slot = 0; slot < map2.num_slots; slot++) {
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
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_new_value = elem2[2] - 8 - size_new_key;
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_new_value = offset_new_key   + size_new_key;
                uint64_t offset_elem_key  = offset_new_value + size_new_value;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_new_value;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* new_value;
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
                    buf_new_value = buf_elem + offset_new_value;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    new_value = (volatile uint8_t*)(ptr + offset_new_value);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_pair_t pair;
                pair.first.size  = size_new_key;
                pair.second.size = size_new_value;
                pair.first.ga  = ga_elem + size_elem;
                pair.second.ga = ga_elem + size_elem + size_new_key;
                
                iacp_insert_map(map1, pair, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_new_value, buf_elem_key, directory, lock_var, list, elem, new_key, new_value, elem_key);
            }
            
            acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
        }
    }
    
    if (buf_elem != ACP_GA_NULL) acp_free(buf_elem);
    acp_free(buf);
    return;
}

void acp_move_local_map(acp_map_t map1, acp_map_t map2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = map1.num_ranks * 8;
    uint64_t size_directory2 = map2.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    acp_copy(buf_directory, map1.ga, size_directory, ACP_HANDLE_NULL);
    acp_copy(buf_directory2, map2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of map2 ***/
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < map2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        for (slot = 0; slot < map2.num_slots; slot++) {
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
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_new_value = elem2[2] - 8 - size_new_key;
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_new_value = offset_new_key   + size_new_key;
                uint64_t offset_elem_key  = offset_new_value + size_new_value;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_new_value;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* new_value;
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
                    buf_new_value = buf_elem + offset_new_value;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    new_value = (volatile uint8_t*)(ptr + offset_new_value);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_pair_t pair;
                pair.first.size  = size_new_key;
                pair.second.size = size_new_value;
                pair.first.ga  = ga_elem + size_elem;
                pair.second.ga = ga_elem + size_elem + size_new_key;
                
                iacp_insert_map(map1, pair, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_new_value, buf_elem_key, directory, lock_var, list, elem, new_key, new_value, elem_key);
                
                /* remove key-value pair */
                
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

void acp_move_map(acp_map_t map1, acp_map_t map2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = map1.num_ranks * 8;
    uint64_t size_directory2 = map2.num_ranks * 8;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem2      = (volatile uint64_t*)(ptr + offset_elem2);
    
    acp_copy(buf_directory, map1.ga, size_directory, ACP_HANDLE_NULL);
    acp_copy(buf_directory2, map2.ga, size_directory2, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t max_size_buf_elem = 0;
    acp_ga_t buf_elem = ACP_GA_NULL;
    
    /*** for all elements of map2 ***/
    
    int rank, slot;
    
    for (rank = 0; rank < map2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        for (slot = 0; slot < map2.num_slots; slot++) {
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
                
                /*** insert key-value pair ***/
                
                uint64_t size_elem      = 32;
                uint64_t size_new_key   = elem2[3];
                uint64_t size_new_value = elem2[2] - 8 - size_new_key;
                uint64_t size_elem_key  = size_new_key;
                uint64_t offset_elem      = 0;
                uint64_t offset_new_key   = offset_elem      + size_elem;
                uint64_t offset_new_value = offset_new_key   + size_new_key;
                uint64_t offset_elem_key  = offset_new_value + size_new_value;
                uint64_t size_buf_elem    = offset_elem_key  + size_elem_key;
                acp_ga_t buf_new_key;
                acp_ga_t buf_new_value;
                acp_ga_t buf_elem_key;
                volatile uint64_t* elem;
                volatile uint8_t* new_key;
                volatile uint8_t* new_value;
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
                    buf_new_value = buf_elem + offset_new_value;
                    buf_elem_key  = buf_elem + offset_elem_key;
                    ptr = acp_query_address(buf_elem);
                    elem      = (volatile uint64_t*)(ptr + offset_elem);
                    new_key   = (volatile uint8_t*)(ptr + offset_new_key);
                    new_value = (volatile uint8_t*)(ptr + offset_new_value);
                    elem_key  = (volatile uint8_t*)(ptr + offset_elem_key);
                }
                
                acp_pair_t pair;
                pair.first.size  = size_new_key;
                pair.second.size = size_new_value;
                pair.first.ga  = ga_elem + size_elem;
                pair.second.ga = ga_elem + size_elem + size_new_key;
                
                iacp_insert_map(map1, pair, buf_directory, buf_lock_var, buf_list, buf_elem, buf_new_key, buf_new_value, buf_elem_key, directory, lock_var, list, elem, new_key, new_value, elem_key);
                
                /* remove key-value pair */
                
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

void acp_remove_map(acp_map_t map, acp_element_t key)
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
    void* ptr = acp_query_address(buf);
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
    
    /*** remove key-value pair ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / map.num_slots) % map.num_ranks;
    int slot = crc % map.num_slots;
    
    acp_ga_t ga_directory = map.ga + rank * 8;
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

size_t acp_retrieve_map(acp_map_t map, acp_pair_t pair)
{
    if (pair.first.size == 0) return 0;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = 8;
    uint64_t size_lock_var  = 8;
    uint64_t size_list      = 24;
    uint64_t size_elem      = 32;
    uint64_t size_new_key   = pair.first.size;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    volatile uint8_t* new_key    = (volatile uint8_t*)(ptr + offset_new_key);
    volatile uint8_t* elem_key   = (volatile uint8_t*)(ptr + offset_elem_key);
    
    /*** read key ***/
    
    if (acp_query_rank(pair.first.ga) != acp_rank()) {
        acp_copy(buf_new_key, pair.first.ga, size_new_key, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    } else {
        memcpy((void*)new_key, acp_query_address(pair.first.ga), size_new_key);
    }
    
    /*** find key ***/
    
    uint64_t crc = iacpdl_crc64((void*)new_key, size_new_key) >> 16;
    int rank = (crc / map.num_slots) % map.num_ranks;
    int slot = crc % map.num_slots;
    
    acp_ga_t ga_directory = map.ga + rank * 8;
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
                uint64_t size_new_value = elem[2] - 8 - size_new_key;
                uint64_t size_actual_value = (size_new_value > pair.second.size) ? pair.second.size : size_new_value;
                if (size_actual_value > 0) acp_copy(pair.second.ga, ga_elem + 32 + size_new_key, size_actual_value, ACP_HANDLE_NULL);
                acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
                acp_complete(ACP_HANDLE_ALL);
                acp_free(buf);
                return size_new_value;
            }
        }
    }
    
    acp_copy(ga_lock_var, buf_lock_var, size_lock_var, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return 0;
}

size_t acp_size_local_map(acp_map_t map)
{
    size_t ret = 0;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
    uint64_t size_table     = map.num_slots * 32;
    uint64_t offset_directory = 0;
    uint64_t offset_table     = offset_directory + size_directory;
    uint64_t size_buf         = offset_table     + size_table;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return ret;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_table     = buf + offset_table;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* table     = (volatile uint64_t*)(ptr + offset_table);
    
    /*** count number of elements in slots ***/
    
    acp_ga_t ga_directory = map.ga;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int my_rank = acp_rank();
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        if (acp_query_rank(ga_table) != my_rank) continue;
        acp_copy(buf_table, ga_table, size_table, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        for (slot = 0; slot < map.num_slots; slot++) ret += table[slot * 32 + 2];
    }
    
    acp_free(buf);
    return ret;
}

size_t acp_size_map(acp_map_t map)
{
    size_t ret = 0;
    
    /*** local buffer variables ***/
    
    uint64_t size_directory = map.num_ranks * 8;
    uint64_t size_table     = map.num_slots * 32;
    uint64_t offset_directory = 0;
    uint64_t offset_table     = offset_directory + size_directory;
    uint64_t size_buf         = offset_table     + size_table;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return ret;
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_table     = buf + offset_table;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* table     = (volatile uint64_t*)(ptr + offset_table);
    
    /*** count number of elements in slots ***/
    
    acp_ga_t ga_directory = map.ga;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        acp_copy(buf_table, ga_table, size_table, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        for (slot = 0; slot < map.num_slots; slot++) ret += table[slot * 32 + 2];
    }
    
    acp_free(buf);
    return ret;
}

void acp_swap_map(acp_map_t map1, acp_map_t map2)
{
    /*** local buffer variables ***/
    
    uint64_t size_directory  = map2.num_ranks * 8;
    uint64_t size_directory2 = size_directory;
    uint64_t size_table      = map2.num_slots * 32;
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
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory  = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile acp_ga_t* directory2 = (volatile acp_ga_t*)(ptr + offset_directory2);
    volatile uint64_t* table      = (volatile uint64_t*)(ptr + offset_table);
    volatile uint64_t* lock_var   = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list       = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem       = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** move map2 to a new temporal map ***/
    
    acp_copy(buf_directory2, map2.ga, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    int rank, slot;
    
    for (rank = 0; rank < map2.num_ranks; rank++) {
        acp_ga_t ga_table = acp_malloc(size_table, acp_query_rank(directory2[rank]));
        if (ga_table == ACP_GA_NULL) {
            while (rank > 0) acp_free(directory[--rank]);
            acp_free(buf);
            return;
        }
        directory[rank] = ga_table;
    }
    
    for (rank = 0; rank < map2.num_ranks; rank++) {
        acp_ga_t ga_table = directory2[rank];
        for (slot = 0; slot < map2.num_slots; slot++) {
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
    
    acp_map_t map;
    map.ga = buf_directory;
    map.num_ranks = map2.num_ranks;
    map.num_slots = map2.num_slots;
    
    /*** move map1 to map2 ***/
    
    acp_move_map(map2, map1);
    
    /*** move temporal map to map1 ***/
    
    acp_move_map(map1, map);
    
    /*** destroy temporal map ***/
    
    for (rank = 0; rank < map.num_ranks; rank++) {
        acp_ga_t ga_table = directory[rank];
        for (slot = 0; slot < map.num_slots; slot++) {
            acp_ga_t ga_lock_var = ga_table + slot * 32;
            acp_ga_t ga_list = ga_lock_var + 8;
            acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            iacp_clear_list_map(list, buf_elem, elem);
        }
        acp_free(ga_table);
    }
    
    acp_free(buf);
    
    return;
}

acp_pair_t acp_dereference_map_it(acp_map_it_t it)
{
    acp_pair_t pair;
    pair.first.ga    = ACP_GA_NULL;
    pair.first.size  = 0;
    pair.second.ga   = ACP_GA_NULL;
    pair.second.size = 0;
    if (it.rank >= it.map.num_ranks || it.slot >= it.map.num_slots || it.elem == ACP_GA_NULL) return pair;
    
    /*** local buffer variables ***/
    
    uint64_t size_elem      = 32;
    uint64_t offset_elem      = 0;
    uint64_t size_buf         = offset_elem      + size_elem;
    
    acp_ga_t buf = acp_malloc(size_buf, acp_rank());
    if (buf == ACP_GA_NULL) return pair;
    acp_ga_t buf_elem      = buf + offset_elem;
    void* ptr = acp_query_address(buf);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    /*** translate element into pair ***/
    
    acp_copy(buf_elem, it.elem, size_elem, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    uint64_t element_size = elem[2];
    uint64_t key_size = elem[3];
    
    pair.first.ga    = it.elem + size_elem;
    pair.first.size  = key_size;
    pair.second.ga   = it.elem + size_elem + key_size;
    pair.second.size = element_size - 8 - key_size;
    return pair;
}

acp_map_it_t acp_increment_map_it(acp_map_it_t it)
{
    if (it.rank >= it.map.num_ranks || it.slot >= it.map.num_slots || it.elem == ACP_GA_NULL) return iacp_null_map_it(it.map);
    
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
    if (buf == ACP_GA_NULL) return iacp_null_map_it(it.map);
    acp_ga_t buf_directory = buf + offset_directory;
    acp_ga_t buf_lock_var  = buf + offset_lock_var;
    acp_ga_t buf_list      = buf + offset_list;
    acp_ga_t buf_elem      = buf + offset_elem;
    void* ptr = acp_query_address(buf);
    volatile acp_ga_t* directory = (volatile acp_ga_t*)(ptr + offset_directory);
    volatile uint64_t* lock_var  = (volatile uint64_t*)(ptr + offset_lock_var);
    volatile uint64_t* list      = (volatile uint64_t*)(ptr + offset_list);
    volatile uint64_t* elem      = (volatile uint64_t*)(ptr + offset_elem);
    
    int rank = it.rank;
    int slot = it.slot;
    
    /*** search next element ***/
    
    acp_copy(buf_elem, it.elem, size_elem, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (elem[0] != ACP_GA_NULL) {
        it.elem = elem[0];
        return it;
    }
    
    acp_ga_t ga_directory = it.map.ga + rank * 8;
    acp_copy(buf_directory, ga_directory, size_directory, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_ga_t ga_lock_var = *directory + slot * 32;
    acp_ga_t ga_list = ga_lock_var + 8;
    
    while (rank + 1 < it.map.num_ranks) {
        while (slot + 1 < it.map.num_slots) {
            slot++;
            ga_lock_var += 32;
            ga_list += 32;
            acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
            acp_complete(ACP_HANDLE_ALL);
            
            if (list[2] > 0) {
                it.rank = rank;
                it.slot = slot;
                it.elem = list[0];
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
            return it;
        }
    }
    
    while (slot < it.map.num_slots - 1) {
        slot++;
        ga_lock_var += 32;
        ga_list += 32;
        acp_copy(buf_list, ga_list, size_list, ACP_HANDLE_ALL);
        acp_complete(ACP_HANDLE_ALL);
        
        if (list[2] > 0) {
            it.rank = rank;
            it.slot = slot;
            it.elem = list[0];
            return it;
        }
    }
    
    return iacp_null_map_it(it.map);
}

