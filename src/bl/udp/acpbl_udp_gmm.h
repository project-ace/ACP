/*
 * ACP Basic Layer GMM header for UDP
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
#ifndef __ACPBL_UDP_GMM_H__
#define __ACPBL_UDP_GMM_H__

extern int iacpbludp_init_gmm(void);
extern int iacpbludp_finalize_gmm(void);
extern void iacpbludp_abort_gmm(void);

extern int iacpbludp_bit_rank;
extern int iacpbludp_bit_offset;
extern uint64_t iacpbludp_mask_rank;
extern uint64_t iacpbludp_mask_seg;
extern uint64_t iacpbludp_mask_offset;

extern uint64_t iacpbludp_segment[16][2];
extern int iacpbludp_num_segment;
extern int iacpbludp_starter_memory_size;

#define BIT_RANK    iacpbludp_bit_rank
#define BIT_SEG     4
#define BIT_OFFSET  iacpbludp_bit_offset
#define MASK_RANK   iacpbludp_mask_rank
#define MASK_SEG    0x000000000000000fLLU
#define MASK_OFFSET iacpbludp_mask_offset

#define SEGMENT     iacpbludp_segment
#define NUM_SEGMENT iacpbludp_num_segment
#define SMEM_SIZE   iacpbludp_starter_memory_size

#define BIT_GA 64
#define SEGST  15
#define SEGDL  14
#define SEGCL  13
#define SEGMAX 13

static inline int ga2rank(acp_ga_t ga)
{
    return (int)(((ga >> (BIT_SEG + BIT_OFFSET)) - 1) & MASK_RANK);
}

static inline int ga2seg(acp_ga_t ga)
{
    return (int)((ga >> BIT_OFFSET) & MASK_SEG);
}

static inline uint64_t ga2offset(acp_ga_t ga)
{
    return (uint64_t)(ga & MASK_OFFSET);
}

static inline int isgalocal(acp_ga_t ga)
{
    int rank;
    
    rank = ga2rank(ga);
    if (rank == MY_RANK) return 1;
    if (GTWY_TABLE[rank] == MY_GATEWAY && ga2seg(ga) >= SEGMAX) return 1;
    return 0;
}

static inline acp_ga_t address2ga(void* addr, size_t size)
{
    uint64_t start, end, seg, rank, offset, inum;
    
    start = (uintptr_t)addr;
    end = start + size;
    for (seg = 0; seg < NUM_SEGMENT; seg++)
        if (SEGMENT[seg][0] <= start && end <= SEGMENT[seg][1]) {
            rank = MY_RANK + 1;
            offset = start - SEGMENT[seg][0];
            return (acp_ga_t)((rank << (BIT_SEG + BIT_OFFSET)) | (seg << BIT_OFFSET) | offset);
        }
    if (SEGMENT[SEGST][0] <= start && end <= SEGMENT[SEGST][1]) {
        offset = start - SEGMENT[SEGST][0];
        inum = offset / SMEM_SIZE;
        offset -= inum * SMEM_SIZE;
        if (size > SMEM_SIZE - offset) return ACP_GA_NULL;
        rank = LMEM_TABLE[inum] + 1;
        return (acp_ga_t)((rank << (BIT_SEG + BIT_OFFSET)) | (SEGST << BIT_OFFSET) | offset);
    }
    if (SEGMENT[SEGDL][0] <= start && end <= SEGMENT[SEGDL][1]) {
        offset = start - SEGMENT[SEGDL][0];
        inum = offset / iacp_starter_memory_size_dl;
        offset -= inum * iacp_starter_memory_size_dl;
        if (size > iacp_starter_memory_size_dl - offset) return ACP_GA_NULL;
        rank = LMEM_TABLE[inum] + 1;
        return (acp_ga_t)((rank << (BIT_SEG + BIT_OFFSET)) | (SEGDL << BIT_OFFSET) | offset);
    }
    if (SEGMENT[SEGCL][0] <= start && end <= SEGMENT[SEGCL][1]) {
        offset = start - SEGMENT[SEGCL][0];
        inum = offset / iacp_starter_memory_size_cl;
        offset -= inum * iacp_starter_memory_size_cl;
        if (size > iacp_starter_memory_size_cl - offset) return ACP_GA_NULL;
        rank = LMEM_TABLE[inum] + 1;
        return (acp_ga_t)((rank << (BIT_SEG + BIT_OFFSET)) | (SEGCL << BIT_OFFSET) | offset);
    }
    return ACP_GA_NULL;
}

static inline void* ga2address(acp_ga_t ga)
{
    uint64_t rank, seg, offset, ptr;
    
    rank = ((ga >> (BIT_SEG + BIT_OFFSET)) & MASK_RANK) - 1;
    if (GTWY_TABLE[rank] != MY_GATEWAY) return NULL;
    
    seg = (ga >> BIT_OFFSET) & MASK_SEG;
    offset = ga & MASK_OFFSET;
    ptr = SEGMENT[seg][0] + offset;
    if (seg < NUM_SEGMENT) {
        if (rank == MY_RANK && ptr < SEGMENT[seg][1]) return (void*)ptr;
    } else if (seg == SEGST) {
        if (offset < SMEM_SIZE) return (void*)(ptr + INUM_TABLE[rank] * SMEM_SIZE);
    } else if (seg == SEGDL) {
        if (offset < iacp_starter_memory_size_dl) return (void*)(ptr + INUM_TABLE[rank] * iacp_starter_memory_size_dl);
    } else if (seg == SEGCL) {
        if (offset < iacp_starter_memory_size_cl) return (void*)(ptr + INUM_TABLE[rank] * iacp_starter_memory_size_cl);
    }
    return NULL;
}

static inline acp_ga_t atkey2ga(acp_atkey_t atkey, void* addr)
{
    uint64_t start;
    int seg;
    
    start = (uintptr_t)addr;
    if (((atkey >> (BIT_SEG + BIT_OFFSET)) & MASK_RANK) != MY_RANK + 1) return ACP_GA_NULL;
    seg = (atkey >> BIT_OFFSET) & MASK_SEG;
    if (NUM_SEGMENT <= seg && seg < SEGMAX) return ACP_GA_NULL;
    return (acp_ga_t)((atkey & ~MASK_OFFSET) | (start - SEGMENT[seg][0]));
}

#endif /* acpbl_udp_gmm.h */
