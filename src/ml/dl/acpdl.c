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

int iacp_init_dl(void)
{
    iacpdl_init_malloc();
    
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

/** Vector
 *      [0:7]    ga of array body
 *      [8:15]   elsize
 *      [16:23]  nelem
 *      [24:31]  nmax
 */

/* basic oepration functions */
acp_vector_t acp_create_vector(size_t nelem, size_t elsize, int rank)
{
    uint64_t num, size;
    acp_ga_t v, buf;
    volatile uint64_t* tmp;
    
    if (nelem == 0 || elsize == 0) return ACP_GA_NULL;
    num = nelem;
    size = (elsize > 2) ? (elsize + 3ULL) & ~3ULL : elsize;
    
    buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return ACP_GA_NULL;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    v = acp_malloc(32, rank);
    if (v != ACP_GA_NULL) {
        tmp[0] = acp_malloc(size * num, rank);
        if (tmp[0] == ACP_GA_NULL) {
            acp_free(v);
            acp_free(buf);
            return ACP_GA_NULL;
        }
        tmp[1] = size;
        tmp[2] = num;
        tmp[3] = num;
        acp_copy(v, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return v;
}

void acp_destroy_vector(acp_vector_t vector)
{
    acp_ga_t buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, vector, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(tmp[0]);
    acp_free(vector);
    acp_free(buf);
    return;
}

acp_vector_t acp_duplicate_vector(acp_vector_t vector, int rank)
{
    uint64_t num, size;
    acp_ga_t ga, v, buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return ACP_GA_NULL;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, vector, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    ga = tmp[0];
    size = tmp[1];
    num = tmp[2];
    
    v = acp_malloc(32, rank);
    if (v != ACP_GA_NULL) {
        tmp[0] = acp_malloc(size * num, rank);
        if (tmp[0] == ACP_GA_NULL) {
            acp_free(v);
            acp_free(buf);
            return ACP_GA_NULL;
        }
        tmp[3] = num;
        acp_copy(v, buf, 32, ACP_HANDLE_NULL);
        acp_copy(tmp[0], ga, num * size, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return v;
}

void acp_swap_vector(acp_vector_t v1, acp_vector_t v2)
{
    uint64_t num1, size1, num2, size2;
    acp_ga_t old1, old2, new1, new2, buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(64, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, v1, 32, ACP_HANDLE_NULL);
    acp_copy(buf + 32, v2, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    old1 = tmp[0];
    size1 = tmp[1];
    num1 = tmp[2];
    old2 = tmp[4];
    size2 = tmp[5];
    num2 = tmp[6];
    
    new1 = acp_malloc(size2 * num2, acp_query_rank(v1));
    if (new1 == ACP_GA_NULL) {
        acp_free(buf);
        return;
    }
    new2 = acp_malloc(size1 * num1, acp_query_rank(v2));
    if (new2 == ACP_GA_NULL) {
        acp_free(new1);
        acp_free(buf);
        return;
    }
    
    tmp[0] = new1;
    tmp[1] = size2;
    tmp[2] = num2;
    tmp[3] = num2;
    tmp[4] = new2;
    tmp[5] = size1;
    tmp[6] = num1;
    tmp[7] = num1;
    acp_copy(v1, buf, 32, ACP_HANDLE_NULL);
    acp_copy(v2, buf + 32, 32, ACP_HANDLE_NULL);
    acp_copy(new1, old2, num2 * size2, ACP_HANDLE_NULL);
    acp_copy(new2, old1, num1 * size1, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(old2);
    acp_free(old1);
    acp_free(buf);
    return;
}

/** List
 *      [0:7]    ga of begin element
 *      [8:15]   ga of end element
 *      [16:23]  elsize
 *      [24:31]  nelem
 ** List Element
 *      [0:7]    ga of next element
 *      [8:15]   ga of previos element
 *      [16:End] element body
 */

acp_list_t acp_create_list(size_t elsize, int rank)
{
    uint64_t size;
    acp_ga_t l, buf;
    volatile uint64_t* tmp;
    
    if (elsize == 0) return ACP_GA_NULL;
    size = (elsize + 7ULL) & ~7ULL;
    
    buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return ACP_GA_NULL;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    l = acp_malloc(32, rank);
    if (l != ACP_GA_NULL) {
        tmp[0] = ACP_GA_NULL;
        tmp[1] = ACP_GA_NULL;
        tmp[2] = size;
        tmp[3] = 0ULL;
        acp_copy(l, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
    }
    
    acp_free(buf);
    return l;
}

void acp_destroy_list(acp_list_t list)
{
    acp_ga_t ga, buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(32, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, list, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    ga = tmp[0];
    
    while (ga != ACP_GA_NULL) {
        acp_copy(buf, ga, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(ga);
        ga = tmp[0];
    }
    
    acp_free(list);
    acp_free(buf);
    return;
}

acp_list_it_t acp_insert_list(acp_list_t list, acp_list_it_t it, void* ptr, int rank)
{
    acp_ga_t ga, buf1, buf2;
    volatile uint64_t* tmp1;
    volatile uint64_t* tmp2;
    
    /* if the iterator is null or end, it calls push_back */
    if (it == ACP_GA_NULL || list == it) {
        acp_push_back_list(list, ptr, rank);
        return acp_begin_list(list);
    }
    
    buf1 = acp_malloc(32, acp_rank());
    if (buf1 == ACP_GA_NULL) return ACP_GA_NULL;
    tmp1 = (volatile uint64_t*)acp_query_address(buf1);
    
    acp_copy(buf1, list, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    ga = acp_malloc(16 + tmp1[2], rank);
    if (ga == ACP_GA_NULL) {
        acp_free(buf1);
        return ACP_GA_NULL;
    }
    
    buf2 = acp_malloc(24 + tmp1[2], acp_rank());
    if (buf2 == ACP_GA_NULL) {
        acp_free(ga);
        acp_free(buf1);
        return ACP_GA_NULL;
    }
    tmp2 = (volatile uint64_t*)acp_query_address(buf2);
    
    tmp2[0] = ga;
    tmp2[1] = it;
    acp_copy(buf2 + 16, it + 8, 8, ACP_HANDLE_NULL);
    memcpy((void*)tmp2 + 24, ptr, tmp1[2]);
    acp_complete(ACP_HANDLE_ALL);
    
    tmp1[3]++;
    if (tmp2[2] == ACP_GA_NULL) {
        tmp1[0] = ga;
    } else {
        acp_copy(tmp2[2] + 8, buf2, 8, ACP_HANDLE_NULL);
    }
    acp_copy(ga, buf2 + 8, 16 + tmp1[2], ACP_HANDLE_NULL);
    acp_copy(it, buf2, 8, ACP_HANDLE_NULL);
    acp_copy(list, buf1, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf2);
    acp_free(buf1);
    return ga;
}

acp_list_it_t acp_erase_list(acp_list_t list, acp_list_it_t it)
{
    acp_ga_t ga, buf;
    volatile uint64_t* tmp;
    
    /* if the iterator is null or end, it fails */
    if (it == ACP_GA_NULL || list == it) return ACP_GA_NULL;
    
    buf = acp_malloc(32 + 16, acp_rank());
    if (buf == ACP_GA_NULL) return ACP_GA_NULL;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, list, 32, ACP_HANDLE_NULL);
    acp_copy(buf + 32, it, 16, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    if (tmp[3] == 1 && tmp[0] == it && tmp[1] == it) {
        /* the only element is erased */
        tmp[0] = ACP_GA_NULL;
        tmp[1] = ACP_GA_NULL;
        tmp[3] = 0ULL;
        acp_copy(list, buf, 32, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it);
        acp_free(buf);
        return ACP_GA_NULL;
        
    } else if (tmp[3] > 1 && tmp[0] == it ) {
        /* the head element is erased */
        ga = tmp[4];
        tmp[0] = ga;
        tmp[3]--;
        acp_copy(list, buf, 32, ACP_HANDLE_NULL);
        acp_copy(ga + 8, buf + 40, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it);
        acp_free(buf);
        return ga;
        
    } else if (tmp[3] > 1 && tmp[1] == it) {
        /* the tail element is erased and it returns an end iterator */
        ga = tmp[5];
        tmp[1] = ga;
        tmp[3]--;
        acp_copy(list + 8, buf + 8, 24, ACP_HANDLE_NULL);
        acp_copy(ga, buf + 32, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it);
        acp_free(buf);
        return (acp_list_it_t)list;
        
    } else if (tmp[3] > 1) {
        ga = tmp[4];
        tmp[3]--;
        acp_copy(list + 24, buf + 24, 8, ACP_HANDLE_NULL);
        acp_copy(tmp[4] + 8, buf + 40, 8, ACP_HANDLE_NULL);
        acp_copy(tmp[5], buf + 32, 8, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        acp_free(it);
        acp_free(buf);
        return ga;
    }
    
    acp_free(buf);
    return ACP_GA_NULL;
}

void acp_push_back_list(acp_list_t list, void* ptr, int rank)
{
    acp_ga_t prev, ga, buf1, buf2;
    volatile uint64_t* tmp1;
    volatile uint64_t* tmp2;
    
    buf1 = acp_malloc(32, acp_rank());
    if (buf1 == ACP_GA_NULL) return;
    tmp1 = (volatile uint64_t*)acp_query_address(buf1);
    
    acp_copy(buf1, list, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    ga = acp_malloc(16 + tmp1[2], rank);
    if (ga == ACP_GA_NULL) {
        acp_free(buf1);
        return;
    }
    
    buf2 = acp_malloc(16 + tmp1[2], acp_rank());
    if (buf2 == ACP_GA_NULL) {
        acp_free(ga);
        acp_free(buf1);
        return;
    }
    tmp2 = (volatile uint64_t*)acp_query_address(buf2);
    
    memcpy((void*)tmp2 + 16, ptr, tmp1[2]);
    if (tmp1[3] == 0) {
        tmp1[0] = ga;
        tmp1[1] = ga;
        tmp1[3] = 1;
        tmp2[0] = ACP_GA_NULL;
        tmp2[1] = ACP_GA_NULL;
    } else {
        prev = tmp1[1];
        tmp1[1] = ga;
        tmp1[3]++;
        tmp2[0] = ACP_GA_NULL;
        tmp2[1] = prev;
        acp_copy(prev, buf1 + 8, 8, ACP_HANDLE_NULL);
    }
    acp_copy(ga, buf2, 16 + tmp1[2], ACP_HANDLE_NULL);
    acp_copy(list, buf1, 32, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf2);
    acp_free(buf1);
    return;
}

/* iterator functions */
acp_list_it_t acp_begin_list(acp_list_t list)
{
    acp_ga_t ga, buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return ACP_GA_NULL;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, list, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    /* if the list is empty, it returns a null iterator */
    ga = tmp[0];
    
    acp_free(buf);
    return ga;
}

acp_list_it_t acp_end_list(acp_list_t list)
{
    return (acp_list_it_t)list;
}

void acp_increment_list(acp_list_it_t* it)
{
    acp_ga_t buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, *it, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    *it = tmp[0];
    
    acp_free(buf);
    return;
}

void acp_decrement_list(acp_list_it_t* it)
{
    acp_ga_t buf;
    volatile uint64_t* tmp;
    
    buf = acp_malloc(8, acp_rank());
    if (buf == ACP_GA_NULL) return;
    tmp = (volatile uint64_t*)acp_query_address(buf);
    
    acp_copy(buf, *it + 8, 8, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    *it = tmp[0];
    
    acp_free(buf);
    return;
}

/*** Vector (functions currently not implemented) ***/

void acp_clear_vector(acp_vector_t vector)
{
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
    return (acp_ga_t)vector;
}

acp_ga_t acp_front_vector(acp_vector_t vector)
{
    return (acp_ga_t)vector;
}

acp_ga_t acp_back_vector(acp_vector_t vector)
{
    return (acp_ga_t)vector;
}


/* iterator functions */
acp_vector_it_t acp_begin_vector(acp_vector_t vector)
{
    return 0;
}

acp_vector_it_t acp_end_vector(acp_vector_t vector)
{
    return 0;
}

acp_vector_it_t acp_rbegin_vector(acp_vector_t vector)
{
    return 0;
}

acp_vector_it_t acp_rend_vector(acp_vector_t vector)
{
    return 0;
}

acp_vector_it_t acp_increment_vector(acp_vector_it_t* it)
{
    return *it;
}

acp_vector_it_t acp_decrement_vector(acp_vector_it_t* it)
{
    return *it;
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


