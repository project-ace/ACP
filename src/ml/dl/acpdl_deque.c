/*
 * ACP Middle Layer: Data Library deque
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    
    if (it.index < 0) return pair;
    
    acp_ga_t buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return pair;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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

