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
    if (buf == ACP_GA_NULL) return;
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
    if (buf == ACP_GA_NULL) return;
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
    if (buf == ACP_GA_NULL) return;
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

//extern acp_element_t acp_dereference_list_it(acp_list_it_t it);
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
//extern acp_element_t acp_dereference_set_it(acp_set_it_t it);
//extern acp_set_it_t acp_increment_set_it(acp_set_it_t it);

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
//extern acp_pair_t acp_dereference_map_it(acp_map_it_t it);
//extern acp_map_it_t acp_increment_map_it(acp_map_it_t it);

