/*
 * ACP Basic Layer GMM for UDP
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <acp.h>
#include "acpbl.h"
#include "acpbl_sync.h"
#include "acpbl_udp.h"
#include "acpbl_udp_gmm.h"
#include "acpbl_udp_gma.h"

size_t iacp_starter_memory_size_dl = 64 * 1024 * 1024;
size_t iacp_starter_memory_size_cl = 1024;

int iacpbludp_init_gmm(void);
int iacpbludp_finalize_gmm(void);
void iacpbludp_abort_gmm(void);

int iacpbludp_bit_rank;
int iacpbludp_bit_offset;
uint64_t iacpbludp_mask_rank;
uint64_t iacpbludp_mask_seg;
uint64_t iacpbludp_mask_offset;

uint64_t iacpbludp_segment[16][2];
volatile uint64_t* iacpbludp_shared_segment;
int iacpbludp_starter_memory_size;

int iacpbludp_init_gmm(void)
{
    FILE *fp;
    char s[4096];
    uint64_t start_addr, end_addr;
    uint64_t seg_top, seg_bottom;
    uint64_t seg_half, seg_full, bottom;
    int i;
    
    BIT_RANK = 1;
    while ((1LLU << BIT_RANK) <= NUM_PROCS) BIT_RANK++;
    BIT_OFFSET = BIT_GA - BIT_SEG - BIT_RANK;
    MASK_RANK     = (1LLU << BIT_RANK) - 1;
    MASK_OFFSET = (1LLU << BIT_OFFSET) - 1;
    
    debug printf("rank %d - global address {rank[63:%d], seg[%d:%d], addr[%d:0]}\n", MY_RANK, BIT_SEG + BIT_OFFSET, BIT_SEG + BIT_OFFSET - 1, BIT_OFFSET, BIT_OFFSET - 1);
    
    sprintf(s, "/proc/%d/maps", getpid());
    if((fp = fopen(s, "r")) == NULL){
        printf("Cannot open file %s.", s);
        return 1;
    }
    seg_top = seg_bottom = 0LLU;
    seg_full = (1LLU << BIT_OFFSET);
    seg_half = (seg_full >> 1);
    bottom = 0LLU - seg_full;
    for (i = 0; i < SEGMAX && fgets(s, 4096, fp) != NULL; ){
        sscanf(s, "%" PRIx64 "-%" PRIx64, &start_addr, &end_addr);
        if(seg_top == 0 && seg_bottom == 0){
            seg_top = start_addr;
            seg_bottom = end_addr;
        } else if (end_addr < seg_top + seg_full){
            seg_bottom = end_addr;
        } else {
            SEGMENT[i][0] = (seg_bottom < seg_half) ? 0 : (seg_top > bottom) ? bottom : (seg_bottom >= seg_top + seg_half) ? seg_top : seg_bottom - seg_half;
            SEGMENT[i][1] = SEGMENT[i][0] + seg_full - 1;
            i++;
            seg_top = start_addr;
            seg_bottom = end_addr;
        }
    }
    if (seg_top != 0){
        SEGMENT[i][0] = (seg_bottom < seg_half) ? 0 : (seg_top > bottom) ? bottom : seg_bottom - seg_half;
        SEGMENT[i][1] = SEGMENT[i][0] + seg_full - 1;
        i++;
    }
    fclose(fp);
    for (; i < SEGMAX; i++)
        SEGMENT[i][1] = SEGMENT[i][0] = 0;
#ifdef DEBUG
    for (i = 0; i < SEGMAX; i++)
        printf("rank %d - segment[%2d] 0x%016llx - 0x%016llx\n", MY_RANK, i, SEGMENT[i][0], SEGMENT[i][1]);
#endif
    
    return 0;
}

int iacpbludp_finalize_gmm(void)
{
    return 0;
}

void iacpbludp_abort_gmm(void)
{
    return;
}

acp_ga_t acp_query_starter_ga(int rank)
{
    return (acp_ga_t)(((uint64_t)(rank + 1) << (BIT_SEG + BIT_OFFSET)) | ((uint64_t)SEGST << BIT_OFFSET));
}

acp_ga_t iacp_query_starter_ga_dl(int rank)
{
    return (acp_ga_t)(((uint64_t)(rank + 1) << (BIT_SEG + BIT_OFFSET)) | ((uint64_t)SEGDL << BIT_OFFSET));
}

acp_ga_t iacp_query_starter_ga_cl(int rank)
{
    return (acp_ga_t)(((uint64_t)(rank + 1) << (BIT_SEG + BIT_OFFSET)) | ((uint64_t)SEGCL << BIT_OFFSET));
}

acp_atkey_t acp_register_memory(void* addr, size_t size, int color)
{
    uint64_t start, end, seg;
    uint64_t seg_half, seg_full, seg_qrtr, bottom, ptr;
    
    start = (uintptr_t)addr;
    end = start + size - 1;
    for (seg = 0; seg < SEGMAX; seg++) {
        if (SEGMENT[seg][0] <= start && end <= SEGMENT[seg][1]) return seg + 1;
        if (SEGMENT[seg][1] == 0) {
            seg_full = (1LLU << BIT_OFFSET);
            seg_half = (seg_full >> 1);
            seg_qrtr = (seg_half >> 1);
            bottom = 0LLU - seg_full;
            if (size > seg_full) return 0;
            if (size > seg_half) {
                ptr = start;
            } else {
                ptr = start & ~(seg_qrtr - 1);
                ptr = (end >= ptr + seg_full) ? start : (end < ptr + seg_half && ptr >= seg_qrtr) ? ptr - seg_qrtr : ptr;
            }
            SEGMENT[seg][0] = (ptr > bottom) ? bottom : ptr;
            SEGMENT[seg][1] = SEGMENT[seg][0] + seg_full - 1;
            SHARESEG(MY_INUM, seg, 0) = SEGMENT[seg][0];
            SHARESEG(MY_INUM, seg, 1) = SEGMENT[seg][1];
            return seg + 1;
        }
    }
    return 0;
}

int acp_unregister_memory(acp_atkey_t atkey)
{
    return 0;
}

acp_ga_t acp_query_ga(acp_atkey_t atkey, void* addr)
{
    return atkey2ga(atkey, addr);
}

void* acp_query_address(acp_ga_t ga)
{
    return ga2address(ga);
}

int acp_query_rank(acp_ga_t ga)
{
    return ga2rank(ga);
}

int acp_query_color(acp_ga_t ga)
{
    return 0;
}
