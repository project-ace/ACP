/*
 * ACP Middle Layer: Data Library
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

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <acp.h>
#include <acpbl_input.h>
#include "acpdl.h"
#include "acpdl_malloc.h"

uint64_t crc64_table[256];

int iacp_init_dl(void)
{
    void (*iacpdl_malloc_hook_init)(int, int, int, uint64_t, uint64_t);
    uint64_t c;
    int n, k;
    
    iacpdl_init_malloc();
    
    iacpdl_malloc_hook_init = (void (*)(int, int, int, uint64_t, uint64_t))dlsym(RTLD_DEFAULT, "iacpdl_malloc_hook_init");
    if(iacpdl_malloc_hook_init != NULL) (*iacpdl_malloc_hook_init)(iacpbl_option.mhooksmall.value, iacpbl_option.mhooklarge.value, iacpbl_option.mhookhuge.value, iacpbl_option.mhooklow.value, iacpbl_option.mhookhigh.value);
    
    for (n = 0; n < 256; n++) {
        c = (uint64_t)n;
        for (k = 0; k < 8; k++) c = (c & 1) ? (0xC96C5795D7870F42ULL ^ (c >> 1)) : (c >> 1);
        crc64_table[n] = c;
    }
    
    return 0;
}

int iacp_finalize_dl(void)
{
    void (*iacpdl_malloc_hook_finalize)(void);
    
    iacpdl_malloc_hook_finalize = (void (*)(void))dlsym(RTLD_DEFAULT, "iacpdl_malloc_hook_finalize");
    if (iacpdl_malloc_hook_finalize != NULL) (*iacpdl_malloc_hook_finalize)();
    
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

