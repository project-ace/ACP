/*
 * ACP Middle Layer: Data Library list
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
//  volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
//  volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
//  volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
//  volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    if (tmp_num1 > 0) acp_copy(buf + 48, next1, 24, ACP_HANDLE_NULL);
    if (tmp_num2 > 0) acp_copy(buf + 72, next2, 24, ACP_HANDLE_NULL);
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
            sel = comp(elem1, elem2);
        }
        
        /* link element */
        if (sel <= 0) {
            if (first != ACP_GA_NULL) {
                *prev_next = next1;
                acp_copy(prev, buf + 96, 16, ACP_HANDLE_NULL);
            } else
                first = next1;
            if (tmp_num1 > 1)
                acp_copy(buf + 48, tmp_next1, 24, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            *prev_prev = prev;
            prev = next1;
            next1 = tmp_next1;
            if (tmp_num1 > 1) {
                tmp_next1 = *elem1_next;
                tmp_prev1 = *elem1_prev;
                tmp_size1 = *elem1_size;
            }
            tmp_num1--;
            tmp_num++;
        } else {
            if (first != ACP_GA_NULL) {
                *prev_next = next2;
                acp_copy(prev, buf + 96, 16, ACP_HANDLE_NULL);
            } else
                first = next2;
            if (tmp_num2 > 1)
                acp_copy(buf + 72, tmp_next2, 24, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            *prev_prev = prev;
            prev = next2;
            next2 = tmp_next2;
            if (tmp_num2 > 1) {
                tmp_next2 = *elem2_next;
                tmp_prev2 = *elem2_prev;
                tmp_size2 = *elem2_size;
            }
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
    acp_copy(list2.ga, buf + 24, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    
    acp_free(buf);
    return;
}

void acp_pop_back_list(acp_list_t list)
{
    acp_ga_t buf = acp_malloc(48, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
    volatile acp_ga_t* prev_next = (volatile acp_ga_t*)(ptr + 48);
    volatile acp_ga_t* prev_prev = (volatile acp_ga_t*)(ptr + 56);
    
    uintptr_t p1 = ptr + 64;
    uintptr_t p2 = local ? (uintptr_t)acp_query_address(elem.ga) : ptr + 64 + elem.size;
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    ptr = (uintptr_t)acp_query_address(buf);
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
        next = tmp_next;
    }
    while (list2.ga > buf + 16) {
        list1.ga = list2.ga - 24;
        acp_merge_list(list1, list2, comp);
        list2.ga = list1.ga;
    }
    
    acp_ga_t head = *(volatile acp_ga_t*)(ptr + 16);
    acp_ga_t tail = *(volatile acp_ga_t*)(ptr + 16 + 8);
    *elem_next = list.ga;
    acp_copy(head + 8, buf, 8, ACP_HANDLE_NULL);
    acp_copy(tail    , buf, 8, ACP_HANDLE_NULL);
    acp_copy(list.ga, buf + 16, 24, ACP_HANDLE_NULL);
    acp_complete(ACP_HANDLE_ALL);
    acp_free(buf);
    return;
}

void acp_splice_list(acp_list_it_t it1, acp_list_it_t it2)
{
    if (it2.elem == it2.list.ga) return;
    
    acp_ga_t buf = acp_malloc(88, acp_rank());
    if (buf == ACP_GA_NULL) return;
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
//  acp_ga_t tmp_prev3 = tmp_prev2;
    *elem3_next = tmp_next2;
    *elem3_prev = tmp_prev2;
    acp_ga_t prev = start.elem;
    int n = 1;
    while (tmp_next3 != start.list.ga && tmp_next3 != end.elem) {
        prev = tmp_next3;
        acp_copy(buf + 80, tmp_next3, 16, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        tmp_next3 = *elem2_next;
//      tmp_prev3 = *elem2_prev;
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
    volatile acp_ga_t* list_head = (volatile acp_ga_t*)ptr;
    volatile acp_ga_t* list_tail = (volatile acp_ga_t*)(ptr + 8);
    volatile uint64_t* list_num  = (volatile uint64_t*)(ptr + 16);
    volatile acp_ga_t* elem_next = (volatile acp_ga_t*)(ptr + 24);
    volatile acp_ga_t* elem_prev = (volatile acp_ga_t*)(ptr + 32);
    volatile uint64_t* elem_size = (volatile uint64_t*)(ptr + 40);
//  volatile acp_ga_t* prev_next = (volatile acp_ga_t*)(ptr + 48);
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
                ptr = (uintptr_t)acp_query_address(buf2);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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
    uintptr_t ptr = (uintptr_t)acp_query_address(buf);
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

