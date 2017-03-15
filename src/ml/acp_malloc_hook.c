/*
 * ACP Malloc Hook Library
 * 
 * Copyright (c) 2017 FUJITSU LIMITED
 * Copyright (c) 2017 Kyushu University
 * Copyright (c) 2017 Institute of Systems, Information Technologies
 *                    and Nanotechnologies
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 * This library must be preloaded to hook malloc, free and realloc
 * functions.
 *
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>

typedef uint64_t acp_ga_t;
#define ACP_GA_NULL     0LLU

static int (*acp_rank)(void);
static acp_ga_t (*iacp_query_starter_ga_dl)(int);
static size_t *iacp_starter_memory_size_dl;
static void* (*acp_query_address)(acp_ga_t);
static acp_ga_t (*acp_malloc)(size_t, int);
static void (*acp_free)(acp_ga_t);

static int rank;
static uintptr_t start_ptr;
static uintptr_t end_ptr;
static acp_ga_t start_ga;
static acp_ga_t end_ga;

static int enable = 0;

int iacpdl_malloc_hook_small_enable;
int iacpdl_malloc_hook_large_enable;
int iacpdl_malloc_hook_huge_enable;
uint64_t iacpdl_malloc_hook_threshold_small;
uint64_t iacpdl_malloc_hook_threshold_large;

#define SMALL    iacpdl_malloc_hook_small_enable
#define LARGE    iacpdl_malloc_hook_large_enable
#define HUGE     iacpdl_malloc_hook_huge_enable
#define TH_SMALL iacpdl_malloc_hook_threshold_small
#define TH_LARGE iacpdl_malloc_hook_threshold_large

void iacpdl_malloc_hook_init(int small, int large, int huge, uint64_t threshold1, uint64_t threshold2)
{
    acp_rank                    =             (int (*)(void))dlsym(RTLD_DEFAULT, "acp_rank");
    iacp_query_starter_ga_dl    =         (acp_ga_t (*)(int))dlsym(RTLD_DEFAULT, "iacp_query_starter_ga_dl");
    iacp_starter_memory_size_dl =                  (size_t *)dlsym(RTLD_DEFAULT, "iacp_starter_memory_size_dl");
    acp_query_address           =       (void* (*)(acp_ga_t))dlsym(RTLD_DEFAULT, "acp_query_address");
    acp_malloc                  = (acp_ga_t (*)(size_t, int))dlsym(RTLD_DEFAULT, "acp_malloc");
    acp_free                    =        (void (*)(acp_ga_t))dlsym(RTLD_DEFAULT, "acp_free");
    
    SMALL    = small;
    LARGE    = large;
    HUGE     = huge;
    TH_SMALL = threshold1;
    TH_LARGE = threshold2;
    
    rank = (*acp_rank)();
    start_ga = (*iacp_query_starter_ga_dl)(rank);
    start_ptr = (uintptr_t)(*acp_query_address)(start_ga);
    end_ga = start_ga + *iacp_starter_memory_size_dl;
    end_ptr = start_ptr + *iacp_starter_memory_size_dl;
    
    enable = 1;
    return;
}

void iacpdl_malloc_hook_finalize(void)
{
    enable = 0;
    return;
}

void* malloc(size_t size)
{
    static void* (*mallocptr)(size_t) = NULL;
    
    if (mallocptr == NULL) mallocptr = (void* (*)(size_t))dlsym(RTLD_NEXT, "malloc");
    if (enable && size > 0) {
        if ((SMALL && size < TH_SMALL) || (LARGE && size >= TH_SMALL && size < TH_LARGE) || (HUGE && size >= TH_LARGE)) {
            acp_ga_t ga = (*acp_malloc)(size, rank);
            if (ga != ACP_GA_NULL) {
                if (start_ga <= ga && ga < end_ga)
                    return (void*)(start_ptr + (ga - start_ga));
                else
                    (*acp_free)(ga);
            }
        }
    }
    return (*mallocptr)(size);
}

void free(void* ptr)
{
    static void  (*freeptr)(void*) = NULL;
    
    if(freeptr == NULL) freeptr = (void (*)(void*))dlsym(RTLD_NEXT, "free");
    if (enable) {
        if (start_ptr <= (uintptr_t)ptr && (uintptr_t)ptr < end_ptr) {
            acp_ga_t ga_in = start_ga + ((uintptr_t)ptr - start_ptr);
            (*acp_free)(ga_in);
            return;
        }
    }
    (*freeptr)(ptr);
    return;
}

void* realloc(void* ptr, size_t size)
{
    static void* (*reallocptr)(void*, size_t) = NULL;
    
    if (reallocptr == NULL) reallocptr = (void* (*)(void*, size_t))dlsym(RTLD_NEXT, "realloc");
    if (ptr == NULL) return malloc(size);
    if (enable) {
        if (start_ptr <= (uintptr_t)ptr && (uintptr_t)ptr < end_ptr) {
            acp_ga_t ga_in = start_ga + ((uintptr_t)ptr - start_ptr);
            if (size == 0) {
                (*acp_free)(ga_in);
                return malloc(0);
            } else {
                uint64_t size_old = *((uint64_t*)ptr - 1) & 0xfffffffffffffff8LLU;
                if (size_old >= size) return ptr;
                acp_ga_t ga = (*acp_malloc)(size, rank);
                if (ga != ACP_GA_NULL) {
                    if (start_ga <= ga && ga < end_ga) {
                        void* ptr_out = (void*)(start_ptr + (ga - start_ga));
                        memcpy(ptr_out, ptr, size_old);
                        (*acp_free)(ga_in);
                        return ptr_out;
                    }
                    (*acp_free)(ga);
                }
                void* ret = malloc(size);
                if (ret != NULL) memcpy(ret, ptr, size_old);
                (*acp_free)(ga_in);
                return ret;
            }
        }
    }
    return (*reallocptr)(ptr, size);
}

