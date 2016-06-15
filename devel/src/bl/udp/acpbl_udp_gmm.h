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
#define SEGVD  12
#define SEGMAX 12

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

static inline acp_ga_t address2ga(void* addr, size_t size)
{
    uint64_t rank, start, end, i;
    
    rank = (uint64_t)(MY_RANK + 1) << (BIT_SEG + BIT_OFFSET);
    start = (uintptr_t)addr;
    end = start + size;
    for (i = 0LLU; i < NUM_SEGMENT; i++)
        if (SEGMENT[i][0] <= start && end <= SEGMENT[i][1])
            return (acp_ga_t)(rank | (i << BIT_OFFSET) | (start - SEGMENT[i][0]));
    return ACP_GA_NULL;
}

static inline void* ga2address(acp_ga_t ga)
{
    int seg;
    
    if (((ga >> (BIT_SEG + BIT_OFFSET)) & MASK_RANK) != MY_RANK + 1) return NULL;
    seg = (ga >> BIT_OFFSET) & MASK_SEG;
    if (NUM_SEGMENT <= seg && seg < SEGMAX) return NULL;
    return (void*)(SEGMENT[seg][0] + (ga & MASK_OFFSET));
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
