/*
 * ACP Basic Layer GMA for UDP
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
#include <sched.h>
#include <acp.h>
#include "acpbl.h"
#include "acpbl_sync.h"
#include "acpbl_udp.h"
#include "acpbl_udp_gmm.h"
#include "acpbl_udp_gma.h"

/* Datagram format */

#define MAX_DG_SIZE    (ACPBL_UDP_MFS)
#define DG_HEADER_SIZE 32
#define MAX_DATA_SIZE  ((MAX_DG_SIZE)-DG_HEADER_SIZE)

#define TYPE_COPY      0
#define TYPE_PUT       1
#define TYPE_CAS4      2
#define TYPE_CAS8      3
#define TYPE_SWAP4     4
#define TYPE_SWAP8     5
#define TYPE_ADD4      6
#define TYPE_ADD8      7
#define TYPE_XOR4      8
#define TYPE_XOR8      9
#define TYPE_OR4       10
#define TYPE_OR8       11
#define TYPE_AND4      12
#define TYPE_AND8      13
#define TYPE_ACK       14
#define TYPE_NACK      15
#define TYPE_RETRY     16
#define TYPE_READ      17
#define TYPE_READACK   18
#define TYPE_END       19
#define TYPE_ENDACK    20
#define TYPE_SCOPY     21

#define BIT_TYPE       5
#define WIDTH_TYPE     (1 << BIT_TYPE)
#define MASK_TYPE      (WIDTH_TYPE - 1)

#define POS_DSID       (BIT_TYPE)
#define BIT_DSID       (ACPBL_UDP_DS_SIZE)
#define WIDTH_DSID     (1 << BIT_DSID)
#define MASK_DSID      (WIDTH_DSID - 1)

#define POS_SWID       (POS_DSID + BIT_DSID)

int iacpbludp_init_gma(void);
int iacpbludp_finalize_gma(void);
void iacpbludp_abort_gma(void);

static inline uint32_t* dg_task(unsigned char* ptr) { return (uint32_t*)ptr; }
static inline uint32_t* dg_type(unsigned char* ptr) { return (uint32_t*)(ptr + 4); }
static inline uint32_t* dg_rank(unsigned char* ptr) { return (uint32_t*)(ptr + 8); }
static inline uint32_t* dg_seq (unsigned char* ptr) { return (uint32_t*)(ptr + 12); }
static inline uint64_t* dg_ptr (unsigned char* ptr) { return (uint64_t*)(ptr + 16); }
static inline acp_ga_t* dg_dst (unsigned char* ptr) { return (acp_ga_t*)(ptr + 24); }
static inline acp_ga_t* dg_src (unsigned char* ptr) { return (acp_ga_t*)(ptr + DG_HEADER_SIZE); }
static inline uint64_t* dg_size(unsigned char* ptr) { return (uint64_t*)(ptr + DG_HEADER_SIZE + 8); }
static inline uint32_t* dg_old4(unsigned char* ptr) { return (uint32_t*)(ptr + DG_HEADER_SIZE + 8); }
static inline uint64_t* dg_old8(unsigned char* ptr) { return (uint64_t*)(ptr + DG_HEADER_SIZE + 8); }
static inline uint32_t* dg_val4(unsigned char* ptr) { return (uint32_t*)(ptr + DG_HEADER_SIZE + 8); }
static inline uint64_t* dg_val8(unsigned char* ptr) { return (uint64_t*)(ptr + DG_HEADER_SIZE + 8); }
static inline uint32_t* dg_new4(unsigned char* ptr) { return (uint32_t*)(ptr + DG_HEADER_SIZE + 12); }
static inline uint64_t* dg_new8(unsigned char* ptr) { return (uint64_t*)(ptr + DG_HEADER_SIZE + 16); }
static inline uint32_t* dg_len (unsigned char* ptr) { return (uint32_t*)(ptr + 12); }
static inline uint64_t* dg_plen(unsigned char* ptr) { return (uint64_t*)(ptr + 16); }
static inline unsigned char* dg_data(unsigned char* ptr) { return ptr + DG_HEADER_SIZE; }
static inline uint32_t* dg_ret4(unsigned char* ptr) { return (uint32_t*)(ptr + DG_HEADER_SIZE); }
static inline uint64_t* dg_ret8(unsigned char* ptr) { return (uint64_t*)(ptr + DG_HEADER_SIZE); }

static inline int atomic_size(uint32_t type)
{
    if (type == TYPE_CAS4 || type == TYPE_SWAP4 || type == TYPE_ADD4 || type == TYPE_XOR4 || type == TYPE_OR4 || type == TYPE_AND4 ) return 4;
    if (type == TYPE_CAS8 || type == TYPE_SWAP8 || type == TYPE_ADD8 || type == TYPE_XOR8 || type == TYPE_OR8 || type == TYPE_AND8 ) return 8;
    return 0;
}

static inline uint32_t type2dsid(uint32_t type)
{
    return (type >> POS_DSID) & MASK_DSID;
}

static inline uint32_t type2swid(uint32_t type)
{
    return type >> POS_SWID;
}

static inline uint32_t dsid2type(uint32_t type, uint32_t dsid, uint32_t swid)
{
    return type | (dsid << POS_DSID) | (swid << POS_SWID);
}

/* Sequence number table */

static uint32_t* txseq_table;
static uint32_t* rxseq_table;

static void init_sequence(void)
{
    uint32_t rank, crc32c;
    uint64_t data;
    int i;
    
    /* Randomize initial sequence numbers with common seeds */
    for (rank = 0; rank < NUM_PROCS; rank++) {
        data = ((uint64_t)TASKID << 32) | (uint64_t)rank;
        crc32c = 0xffffffffU;
        for (i = 0; i < 64; i++){
            crc32c = (crc32c << 1) ^ ((((crc32c >> 31) ^ data) & 1) ? 0x1edc6f31U : 0U);
            data >>= 1;
        }
        rxseq_table[rank] = crc32c;
    }
    for (i = 0; i < NUM_PROCS; i++)
        txseq_table[i] = rxseq_table[MY_RANK];
#ifdef DEBUG
    printf("rank %d - rx sequendce number table\n", MY_RANK);
    for (i = 0; i < NUM_PROCS; i++)
        printf("rank %d - rxseq_table[%6d] = 0x%08x\n", MY_RANK, i, rxseq_table[i]);
#endif
    
    return;
}

static uint32_t inc_seq(uint32_t seq)
{
    return seq < 0xffffffffU ? seq + 1U : 0U;
}

static uint32_t inc_txseq(uint32_t rank)
{
    uint32_t seq;
    seq = txseq_table[rank];
    txseq_table[rank] = inc_seq(seq);
    return seq;
}

static uint32_t inc_rxseq(uint32_t rank)
{
    uint32_t seq;
    seq = rxseq_table[rank];
    rxseq_table[rank] = inc_seq(seq);
    return seq;
}

static int compare_seq(uint32_t seq1, uint32_t seq2)
{
    if (seq2 > seq1) return (seq2 - seq1 <  0x80000000U) ? -1 :  1;
    if (seq1 > seq2) return (seq1 - seq2 <= 0x80000000U) ?  1 : -1;
    return 0;
}

/* Command Queue */

#define BIT_CQ    (ACPBL_UDP_CQ_SIZE)
#define WIDTH_CQ  (1LL << BIT_CQ)
#define MASK_CQ   (WIDTH_CQ - 1LL)

#define CQSTAT_COPY3READY    -1
#define CQSTAT_COPY3TRY      -2
#define CQSTAT_COPY3WAIT     -3
#define CQSTAT_COPY2READY    -4
#define CQSTAT_COPY2TRY      -5
#define CQSTAT_COPY2WAIT     -6
#define CQSTAT_COPY1READY    -7
#define CQSTAT_COPY1TRY      -8
#define CQSTAT_COPY0         -9
#define CQSTAT_ATOMIC2READY  -10
#define CQSTAT_ATOMIC2TRY    -11
#define CQSTAT_ATOMIC2WAIT   -12
#define CQSTAT_ATOMIC1READY  -13
#define CQSTAT_ATOMIC1TRY    -14
#define CQSTAT_ATOMIC0       -15
#define CQSTAT_SCOPY3        -16

typedef struct {
    volatile int stat;
    uint32_t type;
    acp_ga_t dst;
    acp_ga_t src;
    size_t size;
    uint32_t old4;
    uint32_t new4;
    uint64_t old8;
    uint64_t new8;
    uint32_t val4;
    uint64_t val8;
    acp_handle_t order;
} cqe_t;

static volatile cqe_t cq[WIDTH_CQ];
static volatile uint64_t cqlk, cqwp, cqfp, cqrp, cqcp;
static uint64_t last_copy_cqp;
static acp_ga_t last_copy_dst, last_copy_src;

static void cq_reset(void)
{
    cqwp = cqfp = cqrp = cqcp = 1;
    sync_synchronize();
    cqlk = 0;
    last_copy_cqp = 0;
    last_copy_dst = last_copy_src = ACP_GA_NULL;
    return;
}

static inline int cq_lock(void)
{
    while (sync_val_compare_and_swap_8(&cqlk, 0, 1)) ;
    uint64_t wp = cqwp;
    while (cqcp + WIDTH_CQ <= wp) ;
    return (int)(wp & MASK_CQ);
}

static inline uint64_t cq_unlock(void)
{
    uint64_t wp = cqwp++;
    sync_synchronize();
    cqlk = 0;
    return wp;
}

static inline int cqstat_copy(acp_ga_t dst, acp_ga_t src, size_t size)
{
    if (ga2rank(dst) == MY_RANK && ga2rank(src) == MY_RANK) return CQSTAT_COPY0;
    if (ga2rank(src) == MY_RANK && size <= MAX_DATA_SIZE) return CQSTAT_COPY1READY;
    if (ga2rank(dst) == MY_RANK) return CQSTAT_COPY2READY;
    return CQSTAT_COPY3READY;
}

static inline int cqstat_atomic(acp_ga_t dst, acp_ga_t src)
{
    if (ga2rank(dst) == MY_RANK && ga2rank(src) == MY_RANK) return CQSTAT_ATOMIC0;
    if (ga2rank(src) == MY_RANK) return CQSTAT_ATOMIC1READY;
    return CQSTAT_ATOMIC2READY;
}

acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, size_t size, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat = cqstat_copy(dst, src, size);
    cq[p].type = TYPE_COPY;
    cq[p].dst  = dst;
    cq[p].src  = src;
    cq[p].size = size;
    if (cqwp == last_copy_cqp + 1 && ga2rank(dst) == ga2rank(last_copy_dst) && ga2rank(src) == ga2rank(last_copy_src)) {
        if (cq[p].stat == CQSTAT_COPY3READY) {
            cq[p].order = ACP_HANDLE_NULL;
            cq[p].stat = CQSTAT_SCOPY3;
            cq[p].type = TYPE_SCOPY;
        }
    }
//    last_copy_cqp = cqwp;
    last_copy_dst = dst;
    last_copy_src = src;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_cas4(acp_ga_t dst, acp_ga_t src, uint32_t oldval, uint32_t newval, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_CAS4;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].old4  = oldval;
    cq[p].new4  = newval;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_cas8(acp_ga_t dst, acp_ga_t src, uint64_t oldval, uint64_t newval, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_CAS8;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].old8  = oldval;
    cq[p].new8  = newval;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_swap4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_SWAP4;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val4  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_swap8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_SWAP8;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val8  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_add4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_ADD4;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val4  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_add8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_ADD8;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val8  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_xor4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_XOR4;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val4  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_xor8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_XOR8;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val8  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_or4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_OR4;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val4  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_or8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_OR8;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val8  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_and4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_AND4;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val4  = value;
    return (acp_handle_t)cq_unlock();
}

acp_handle_t acp_and8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_lock();
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].stat  = cqstat_atomic(dst, src);
    cq[p].type  = TYPE_AND8;
    cq[p].dst   = dst;
    cq[p].src   = src;
    cq[p].val8  = value;
    return (acp_handle_t)cq_unlock();
}

void acp_complete(acp_handle_t handle)
{
    if (handle == ACP_HANDLE_NULL) return;
    if (handle == ACP_HANDLE_ALL || handle == ACP_HANDLE_CONT) handle = cqwp - 1;
    if (handle < cqcp) return;
    if (handle >= cqwp) return;
    if (handle < cqrp) {
        cqcp = handle+1;
      return;
    }
    while (cqrp <= handle) {
        while (cq[cqrp & MASK_CQ].stat < 0) ;
        cqrp++;
    }
    cqcp = cqrp;
    
    return;
}

int acp_inquire(acp_handle_t handle)
{
    if (handle == ACP_HANDLE_NULL) return 0;
    if (handle == ACP_HANDLE_ALL || handle == ACP_HANDLE_CONT) handle = cqwp - 1;
    if (handle < cqrp) return 0;
    if (handle >= cqwp) return 0;
    while (cqrp <= handle) {
      if (cq[cqrp & MASK_CQ].stat < 0) return 1;
      cqrp++;
    }
    
    return 0;
}

/* Command Station */

#define BIT_CS   (ACPBL_UDP_CS_SIZE)
#define WIDTH_CS (1LL << BIT_CS)
#define MASK_CS  (WIDTH_CS - 1LL)

typedef struct {
    int prev;
    int next;
    uint64_t cqp;
    uint64_t time;
    uint64_t count;
    uint32_t rank;
    unsigned char dg[MAX_DG_SIZE];
    size_t len;
    struct sockaddr_in addr;
} cse_t;

static cse_t cs[WIDTH_CS];
static int csfreelist[WIDTH_CS];
static int cshead, cstail, csflhead, csflnum;

static inline void cs_reset(void)
{
    int i;
    
    for (i = 0; i < WIDTH_CS; i++) {
      *dg_task(cs[i].dg) = TASKID;
      *dg_rank(cs[i].dg) = MY_RANK;
      cs[i].addr.sin_family = AF_INET;
    }
    
    cshead = cstail = -1;
    for (i = 0; i < WIDTH_CS; i++) csfreelist[i] = i;
    csflhead = 0;
    csflnum = WIDTH_CS;
    
    return;
}

static inline int is_cs_full(void)
{
    return (csflnum == 0) ? 1 : 0;
}

static inline int is_cs_not_full(void)
{
    return (csflnum > 0) ? 1 : 0;
}

static inline int is_cs_empty(void)
{
    return (csflnum == WIDTH_CS) ? 1 : 0;
}

static inline int is_cs_not_empty(void)
{
    return (csflnum < WIDTH_CS) ? 1 : 0;
}

static inline int cs_add(uint64_t cqp, uint64_t time)
{
    int pos;
    
    pos = csfreelist[csflhead];
    csflhead = (csflhead + 1) & MASK_CS;
    csflnum--;
    cs[pos].prev = cstail;
    cs[pos].next = -1;
    cs[pos].cqp = cqp;
    cs[pos].time = time;
    cs[pos].count = 0;
    if (cstail >= 0)
      cs[cstail].next = pos;
    else
      cshead = pos;
    cstail = pos;
    debug printf("rank %d - cs_add  cqp %6" PRIu64 " time %" PRIu64 " pos %d\n", MY_RANK, cqp, time, pos);
    
    return pos;
}

static inline void cs_free(int pos, uint64_t time)
{
    debug printf("rank %d - cs_free cqp %6" PRIu64 " time %" PRIu64 " pos %d count %" PRIu64 "\n", MY_RANK, cs[pos].cqp, time, pos, cs[pos].count);
    if (cs[pos].prev >= 0)
      cs[cs[pos].prev].next = cs[pos].next;
    else
      cshead = cs[pos].next;
    if (cs[pos].next >= 0)
      cs[cs[pos].next].prev = cs[pos].prev;
    else
      cstail = cs[pos].prev;
    
    csfreelist[(csflhead + csflnum) & MASK_CS] = pos;
    csflnum++;
    
    return;
}

/* Delegate Station with Streaming Window */

#define BIT_DS   (ACPBL_UDP_DS_SIZE)
#define WIDTH_DS (1LL << BIT_DS)
#define MASK_DS  (WIDTH_DS - 1LL)

#define BIT_SW   (ACPBL_UDP_SW_SIZE)
#define WIDTH_SW (1LL << BIT_SW)
#define MASK_SW  (WIDTH_SW - 1LL)

#define DSSTAT_EMPTY         0
#define DSSTAT_COPY_READ     1
#define DSSTAT_COPY_END      2
#define DSSTAT_COPY2_READ    3
#define DSSTAT_ATOMIC_WRITE  4
#define DSSTAT_ATOMIC_END    5
#define DSSTAT_COPY_FENCE    6

typedef struct {
    int prev;
    int next;
    uint64_t time;
    uint64_t count;
    void* dst;
    acp_ga_t src;
    size_t len;
} swe_t;

typedef struct {
    int prev;
    int next;
    int stat;
    int complete;
    uint64_t time;
    uint64_t count;
    uint32_t rank;
    uint32_t seq;
    acp_ga_t dst;
    acp_ga_t src;
    uint64_t size;
    uint64_t offset;
    size_t len;
    unsigned char dg[DG_HEADER_SIZE + 8];
    struct sockaddr_in addr;
    swe_t sw[WIDTH_SW];
    int swfreelist[WIDTH_SW];
    int swhead, swtail, swflhead, swflnum;
} dse_t;

static dse_t ds[WIDTH_DS];
static int dsfreelist[WIDTH_DS];
static int dshead, dstail, dsflhead, dsflnum;

static inline void ds_reset(void)
{
    int i;
    
    for (i = 0; i < WIDTH_DS; i++) {
      ds[i].stat = DSSTAT_EMPTY;
      *dg_task(ds[i].dg) = TASKID;
      ds[i].addr.sin_family = AF_INET;
    }
    
    dshead = dstail = -1;
    for (i = 0; i < WIDTH_DS; i++) dsfreelist[i] = i;
    dsflhead = 0;
    dsflnum = WIDTH_DS;
    
    return;
}

static inline int is_ds_full(void)
{
    return (dsflnum == 0) ? 1 : 0;
}

static inline int is_ds_not_full(void)
{
    return (dsflnum > 0) ? 1 : 0;
}

static inline int is_ds_empty(void)
{
    return (dsflnum == WIDTH_DS) ? 1 : 0;
}

static inline int is_ds_not_empty(void)
{
    return (dsflnum < WIDTH_DS) ? 1 : 0;
}

static inline int ds_add(int stat, uint64_t time)
{
    int pos, i;
    
    pos = dsfreelist[dsflhead];
    dsflhead = (dsflhead + 1) & MASK_DS;
    dsflnum--;
    ds[pos].prev = dstail;
    ds[pos].next = -1;
    ds[pos].stat = stat;
    ds[pos].complete = 0;
    ds[pos].time = time;
    ds[pos].count = 0;
    if (dstail >= 0)
        ds[dstail].next = pos;
    else
      dshead = pos;
    dstail = pos;
    
    ds[pos].swhead = ds[pos].swtail = -1;
    for (i = 0; i < WIDTH_SW; i++) ds[pos].swfreelist[i] = i;
    ds[pos].swflhead = 0;
    ds[pos].swflnum = WIDTH_SW;
    
    for (i = 0; i < WIDTH_SW; i++) ds[pos].sw[i].src = ACP_GA_NULL;
    
    debug printf("rank %d - ds_add  stat %5d time %" PRIu64 " pos %d\n", MY_RANK, stat, time, pos);
    return pos;
}

static inline void ds_free(int pos, uint64_t time)
{
    debug printf("rank %d - ds_free stat %5d time %" PRIu64 " pos %d count %" PRIu64 "\n", MY_RANK, ds[pos].stat, time, pos, ds[pos].count);
    ds[pos].stat = DSSTAT_EMPTY;
    
    if (ds[pos].prev >= 0)
      ds[ds[pos].prev].next = ds[pos].next;
    else
      dshead = ds[pos].next;
    if (ds[pos].next >= 0)
      ds[ds[pos].next].prev = ds[pos].prev;
    else
      dstail = ds[pos].prev;
    
    dsfreelist[(dsflhead + dsflnum) & MASK_DS] = pos;
    dsflnum++;
    
    return;
}

static inline int is_sw_full(int pos)
{
    return (ds[pos].swflnum == 0) ? 1 : 0;
}

static inline int is_sw_not_full(int pos)
{
    return (ds[pos].swflnum > 0) ? 1: 0;
}

static inline int is_sw_empty(int pos)
{
    return (ds[pos].swflnum == WIDTH_SW) ? 1 : 0;
}

static inline int is_sw_not_empty(int pos)
{
    return (ds[pos].swflnum < WIDTH_SW) ? 1 : 0;
}

static inline int sw_add(int dsid, uint64_t time)
{
    int pos;
    
    pos = ds[dsid].swfreelist[ds[dsid].swflhead];
    ds[dsid].swflhead = (ds[dsid].swflhead + 1) & MASK_SW;
    ds[dsid].swflnum--;
    ds[dsid].sw[pos].prev = ds[dsid].swtail;
    ds[dsid].sw[pos].next = -1;
    ds[dsid].sw[pos].time = time;
    ds[dsid].sw[pos].count = 0;
    if (ds[dsid].swtail >= 0)
        ds[dsid].sw[ds[dsid].swtail].next = pos;
    else
        ds[dsid].swhead = pos;
    ds[dsid].swtail = pos;
    
    debug printf("rank %d - sw_add  dsid %5d time %" PRIu64 " pos %d\n", MY_RANK, dsid, time, pos);
    return pos;
}

static inline void sw_free(int dsid, int swid, uint64_t time)
{
    debug printf("rank %d - sw_free dsid %5d time %" PRIu64 " pos %d count %" PRIu64 "\n", MY_RANK, dsid, time, swid, ds[dsid].sw[swid].count);
    ds[dsid].sw[swid].src = ACP_GA_NULL;
    
    if (ds[dsid].sw[swid].prev >= 0)
        ds[dsid].sw[ds[dsid].sw[swid].prev].next = ds[dsid].sw[swid].next;
    else
        ds[dsid].swhead = ds[dsid].sw[swid].next;
    if (ds[dsid].sw[swid].next >= 0)
        ds[dsid].sw[ds[dsid].sw[swid].next].prev = ds[dsid].sw[swid].prev;
    else
        ds[dsid].swtail = ds[dsid].sw[swid].prev;
    
    ds[dsid].swfreelist[(ds[dsid].swflhead + ds[dsid].swflnum) & MASK_SW] = swid;
    ds[dsid].swflnum++;
    
    return;
}

/* Communication thread */

volatile static int comm_thread_ready;
volatile static int abort_comm_thread;
static unsigned char dg[MAX_DG_SIZE];

static void* comm_thread_func(void *param)
{
    uint64_t free_count;
    uint64_t last_cqwp;
    /* UDP communication variables */
    struct sockaddr_in addr;
    int sock;
    struct pollfd pfds[1];
    socklen_t sock_addr_in_size = sizeof(struct sockaddr_in);
    int bind_retval;
    struct sockaddr_in from;
    socklen_t addr_len;
    ssize_t recv_len;
    
#ifdef DEBUG
    printf("rank %d comm_thread - begin\n", MY_RANK);
    printf("rank %d comm_thread - maximum datagram size%5dB (data%5dB)\n", MY_RANK, MAX_DG_SIZE, MAX_DATA_SIZE);
    printf("rank %d comm_thread - command queue    %5d bytes %5d entries\n", MY_RANK, sizeof(cqe_t), WIDTH_CQ);
    printf("rank %d comm_thread - command station  %5d bytes %5d entries\n", MY_RANK, sizeof(cse_t), WIDTH_CS);
    printf("rank %d comm_thread - delegate station %5d bytes %5d entries\n", MY_RANK, sizeof(dse_t), WIDTH_DS);
    printf("rank %d comm_thread - streaming window %5d bytes %5d entries\n", MY_RANK, sizeof(swe_t), WIDTH_SW);
    
#endif
    /* bind socket for UDP */
    addr.sin_family = AF_INET;
    addr.sin_port = PORT_TABLE[MY_RANK];
    addr.sin_addr.s_addr = INADDR_ANY;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    bind_retval = bind(sock, (struct sockaddr *)&addr, sock_addr_in_size);
    
    /* set pollfd for UDP */
    pfds[0].fd = sock;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;
    
    cq_reset();
    cs_reset();
    ds_reset();
    
    sync_synchronize();
    comm_thread_ready = 1;
    free_count = 0;
    last_cqwp = cqwp;
    
    while (!abort_comm_thread) {
        uint64_t time, cqp, p, cqnum, len;
        uint32_t type, pos, rank, seq;
        uint32_t dsid, swid, drank;
        uint32_t reverse_seq;
        int i, j, stat;
        
        time = get_clock();
        
        /*** Command Queue ****/
        
        if (free_count++ > 10 && last_cqwp == cqwp)
            sched_yield();
        else
            last_cqwp = cqwp;
        
        while (cqrp < cqcp && cq[cqrp & MASK_CQ].stat >= 0) cqrp++;
        cqnum = (cqwp - cqrp < 64) ? cqwp - cqrp : 64;
        if (cqfp < cqrp) cqfp = cqrp;
        for (i = 0; i < cqnum; i++) {
            if (cqwp <= cqfp) cqfp = cqrp;
            cqp = cqfp++;
            p = cqp & MASK_CQ;
            if (cq[p].order >= cqrp) continue;
            if (cq[p].stat >= 0) continue;
            
            /* Delegating Copy to destination */
            if (cq[p].stat == CQSTAT_COPY3READY && is_cs_not_full()) {
                pos = cs_add(cqp, time);
                rank = cs[pos].rank = ga2rank(cq[p].dst);
                *dg_type(cs[pos].dg) = TYPE_COPY;
                *dg_seq (cs[pos].dg) = inc_txseq(rank);
                *dg_ptr (cs[pos].dg) = cqp;
                *dg_dst (cs[pos].dg) = cq[p].dst;
                *dg_src (cs[pos].dg) = cq[p].src;
                *dg_size(cs[pos].dg) = cq[p].size;
                cs[pos].len = DG_HEADER_SIZE + 16;
                cs[pos].addr.sin_port = PORT_TABLE[rank];
                cs[pos].addr.sin_addr.s_addr = ADDR_TABLE[rank];
                sync_synchronize();
                cq[p].stat = CQSTAT_COPY3TRY;
            }
            
            /* Delegating Serialized Copy to destination */
            if (cq[p].stat == CQSTAT_SCOPY3 && is_cs_not_full()) {
                stat = 0;
                if (cqp - 1 > cqrp) stat = cq[(cqp - 1) & MASK_CQ].stat;
                if (stat == CQSTAT_COPY3TRY || stat == CQSTAT_COPY3WAIT || stat == 0) {
                    pos = cs_add(cqp, time);
                    rank = cs[pos].rank = ga2rank(cq[p].dst);
                    *dg_type(cs[pos].dg) = TYPE_SCOPY;
                    *dg_seq (cs[pos].dg) = inc_txseq(rank);
                    *dg_ptr (cs[pos].dg) = cqp;
                    *dg_dst (cs[pos].dg) = cq[p].dst;
                    *dg_src (cs[pos].dg) = cq[p].src;
                    *dg_size(cs[pos].dg) = cq[p].size;
                    cs[pos].len = DG_HEADER_SIZE + 16;
                    cs[pos].addr.sin_port = PORT_TABLE[rank];
                    cs[pos].addr.sin_addr.s_addr = ADDR_TABLE[rank];
                    sync_synchronize();
                    cq[p].stat = CQSTAT_COPY3TRY;
                }
            }
            
            /* Delegating Copy locally */
            if (cq[p].stat == CQSTAT_COPY2READY && is_ds_not_full()){
                pos = ds_add(DSSTAT_COPY2_READ, time);
                rank = ds[pos].rank = ga2rank(cq[p].dst);
                ds[pos].dst  = cq[p].dst;
                ds[pos].src  = cq[p].src;
                ds[pos].size = cq[p].size;
                ds[pos].offset = 0;
                *dg_rank(ds[pos].dg) = rank;
                *dg_ptr (ds[pos].dg) = cqp;
                drank = ga2rank(cq[p].src);
                ds[pos].len = DG_HEADER_SIZE;
                ds[pos].addr.sin_port = PORT_TABLE[drank];
                ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                sync_synchronize();
                cq[p].stat = CQSTAT_COPY2WAIT;
            }
            
            /* One-way Copy */
            if (cq[p].stat == CQSTAT_COPY1READY && is_cs_not_full()) {
                pos = cs_add(cqp, time);
                rank = cs[pos].rank = ga2rank(cq[p].dst);
                *dg_type(cs[pos].dg) = TYPE_PUT;
                *dg_seq (cs[pos].dg) = inc_txseq(rank);
                *dg_plen(cs[pos].dg) = cq[p].size;
                *dg_dst (cs[pos].dg) = cq[p].dst;
                memcpy(dg_data(cs[pos].dg), ga2address(cq[p].src), cq[p].size);
                cs[pos].len = DG_HEADER_SIZE + *dg_plen(cs[pos].dg);
                cs[pos].addr.sin_port = PORT_TABLE[rank];
                cs[pos].addr.sin_addr.s_addr = ADDR_TABLE[rank];
                sync_synchronize();
                cq[p].stat = CQSTAT_COPY1TRY;
            }
            
            /* Local Copy */
            if (cq[p].stat == CQSTAT_COPY0) {
                debug printf("rank %d - memcpy  cqp %6" PRIu64 " time %" PRIu64 " dst 0x%016" PRIx64 " src 0x%016" PRIx64 " size %" PRIu64 "\n", MY_RANK, cqp, time, (uint64_t)ga2address(cq[p].dst), (uint64_t)ga2address(cq[p].src), cq[p].size);
                memcpy(ga2address(cq[p].dst), ga2address(cq[p].src), cq[p].size);
                sync_synchronize();
                cq[p].stat = 0;
            }
            
            /* Delegating Atomic to source */
            if (cq[p].stat == CQSTAT_ATOMIC2READY && is_cs_not_full()) {
                type = cq[p].type;
                pos = cs_add(cqp, time);
                rank = cs[pos].rank = ga2rank(cq[p].src);
                *dg_type(cs[pos].dg) = type;
                *dg_seq (cs[pos].dg) = inc_txseq(rank);
                *dg_ptr (cs[pos].dg) = cqp;
                *dg_dst (cs[pos].dg) = cq[p].dst;
                *dg_src (cs[pos].dg) = cq[p].src;
                if        (type == TYPE_CAS4) {
                    *dg_old4(cs[pos].dg) = cq[p].old4;
                    *dg_new4(cs[pos].dg) = cq[p].new4;
                    cs[pos].len = DG_HEADER_SIZE + 16;
                } else if (type == TYPE_CAS8) {
                    *dg_old8(cs[pos].dg) = cq[p].old8;
                    *dg_new8(cs[pos].dg) = cq[p].new8;
                    cs[pos].len = DG_HEADER_SIZE + 24;
                } else if (type == TYPE_SWAP4 || type == TYPE_ADD4 || type == TYPE_XOR4 || type == TYPE_OR4 || type == TYPE_AND4){
                    *dg_val4(cs[pos].dg) = cq[p].val4;
                    cs[pos].len = DG_HEADER_SIZE + 12;
                } else {
                    *dg_val8(cs[pos].dg) = cq[p].val8;
                    cs[pos].len = DG_HEADER_SIZE + 16;
                }
                cs[pos].addr.sin_port = PORT_TABLE[rank];
                cs[pos].addr.sin_addr.s_addr = ADDR_TABLE[rank];
                sync_synchronize();
                cq[p].stat = CQSTAT_ATOMIC2TRY;
            }
            
            /* One-way Atomic */
            if (cq[p].stat == CQSTAT_ATOMIC1READY && is_cs_not_full()) {
                type = cq[p].type;
                pos = cs_add(cqp, time);
                rank = cs[pos].rank = ga2rank(cq[p].dst);
                *dg_type(cs[pos].dg) = TYPE_PUT;
                *dg_seq (cs[pos].dg) = inc_txseq(rank);
                *dg_plen(cs[pos].dg) = atomic_size(type);
                *dg_dst (cs[pos].dg) = cq[p].dst;
                if      (type == TYPE_CAS4)
                    *dg_ret4(cs[pos].dg) = sync_val_compare_and_swap_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].old4, cq[p].new4);
                else if (type == TYPE_CAS8)
                    *dg_ret8(cs[pos].dg) = sync_val_compare_and_swap_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].old8, cq[p].new8);
                else if (type == TYPE_SWAP4)
                    *dg_ret4(cs[pos].dg) = sync_swap_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else if (type == TYPE_SWAP8)
                    *dg_ret8(cs[pos].dg) = sync_swap_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                else if (type == TYPE_ADD4)
                    *dg_ret4(cs[pos].dg) = sync_fetch_and_add_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else if (type == TYPE_ADD8)
                    *dg_ret8(cs[pos].dg) = sync_fetch_and_add_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                else if (type == TYPE_XOR4)
                    *dg_ret4(cs[pos].dg) = sync_fetch_and_xor_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else if (type == TYPE_XOR8)
                    *dg_ret8(cs[pos].dg) = sync_fetch_and_xor_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                else if (type == TYPE_OR4)
                    *dg_ret4(cs[pos].dg) = sync_fetch_and_or_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else if (type == TYPE_OR8)
                    *dg_ret8(cs[pos].dg) = sync_fetch_and_or_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                else if (type == TYPE_AND4)
                    *dg_ret4(cs[pos].dg) = sync_fetch_and_and_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else
                    *dg_ret8(cs[pos].dg) = sync_fetch_and_and_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                cs[pos].len = DG_HEADER_SIZE + *dg_plen(cs[pos].dg);
                cs[pos].addr.sin_port = PORT_TABLE[rank];
                cs[pos].addr.sin_addr.s_addr = ADDR_TABLE[rank];
                sync_synchronize();
                cq[p].stat = CQSTAT_ATOMIC1TRY;
            }
            
            /* Local Atomic */
            if (cq[p].stat == CQSTAT_ATOMIC0) {
                type = cq[p].type;
                if      (type == TYPE_CAS4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_val_compare_and_swap_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].old4, cq[p].new4);
                else if (type == TYPE_CAS8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_val_compare_and_swap_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].old8, cq[p].new8);
                else if (type == TYPE_SWAP4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_swap_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else if (type == TYPE_SWAP8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_swap_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                else if (type == TYPE_ADD4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_add_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else if (type == TYPE_ADD8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_add_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                else if (type == TYPE_XOR4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_xor_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else if (type == TYPE_XOR8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_xor_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                else if (type == TYPE_OR4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_or_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else if (type == TYPE_OR8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_or_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                else if (type == TYPE_AND4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_and_4((uint32_t* volatile)ga2address(cq[p].src), cq[p].val4);
                else
                    *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_and_8((uint64_t* volatile)ga2address(cq[p].src), cq[p].val8);
                debug printf("rank %d - local atomic cqp %6" PRIu64 " time %" PRIu64 " dst 0x%016" PRIx64 " src 0x%016" PRIx64 " type %u result %10u new_value %10u\n", MY_RANK, cqp, time, (uint64_t)ga2address(cq[p].dst), (uint64_t)ga2address(cq[p].src), cq[p].type, *(uint32_t*)ga2address(cq[p].dst), *(uint32_t*)ga2address(cq[p].src));
                sync_synchronize();
                cq[p].stat = 0;
            }
        }
        
        /*** Command Station ***/
        
        for (i = cshead; i >= 0; i = cs[i].next) {
            if (cs[i].count < 2) free_count = 0;
            if (time >= cs[i].time) {
                sendto(sock, cs[i].dg, cs[i].len, 0, (struct sockaddr *)&cs[i].addr, sock_addr_in_size);
                /* Set retransmit timer from 200K to 200M clock (around 100 usec to 100 msec) */
                reverse_seq = *dg_seq(cs[i].dg);
                reverse_seq = (reverse_seq & 0x249) << 2 | (reverse_seq & 0x492) | (reverse_seq & 0x924) >> 2;
                reverse_seq = (reverse_seq & 0x1c7) << 3 | (reverse_seq & 0xe38) >> 3;
                reverse_seq = (reverse_seq & 0x03f) << 6 | (reverse_seq & 0xfc0) >> 6;
                cs[i].time = time + ((uint64_t)(0x4000 + reverse_seq) << ((cs[i].count < 10) ? cs[i].count + 4 : 10 + 4));
                cs[i].count++;
            }
        }
        
        /*** Recieve Datagram ***/
        
        poll(pfds, 1, 0);
        if (pfds[0].revents & POLLIN) {
            free_count = 0;
            recv_len = recvfrom(sock, dg, MAX_DG_SIZE, 0, (struct sockaddr*)&from, &addr_len);
            if (*dg_task(dg) == TASKID && recv_len >= 16) {
                type = *dg_type(dg) & MASK_TYPE;
                rank = *dg_rank(dg);
                seq  = *dg_seq (dg);
                
                /* Command datagrams */
                if (type == TYPE_COPY ) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 16) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_COPY_READ, time);
                        ds[pos].dst  = *dg_dst(dg);
                        ds[pos].src  = *dg_src(dg);
                        ds[pos].size = *dg_size(dg);
                        ds[pos].offset = 0;
                        *dg_rank(ds[pos].dg) = rank;
                        *dg_ptr (ds[pos].dg) = *dg_ptr(dg);
                        drank = ga2rank(*dg_src(dg));
                        ds[pos].len = DG_HEADER_SIZE;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_SCOPY) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 16) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_COPY_FENCE, time);
                        ds[pos].dst  = *dg_dst(dg);
                        ds[pos].src  = *dg_src(dg);
                        ds[pos].size = *dg_size(dg);
                        ds[pos].offset = 0;
                        *dg_rank(ds[pos].dg) = rank;
                        *dg_ptr (ds[pos].dg) = *dg_ptr(dg);
                        drank = ga2rank(*dg_src(dg));
                        ds[pos].len = DG_HEADER_SIZE;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_PUT) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (recv_len < DG_HEADER_SIZE) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        memcpy(ga2address(*dg_dst(dg)), dg_data(dg), *dg_plen(dg));
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_CAS4) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 16) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 4;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret4(ds[pos].dg) = sync_val_compare_and_swap_4((uint32_t* volatile)ga2address(*dg_src(dg)), *dg_old4(dg), *dg_new4(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 4;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_CAS8) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 24) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 8;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret8(ds[pos].dg) = sync_val_compare_and_swap_8((uint64_t* volatile)ga2address(*dg_src(dg)), *dg_old8(dg), *dg_new8(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 8;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_SWAP4) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 12) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 4;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret4(ds[pos].dg) = sync_swap_4((volatile uint32_t*)ga2address(*dg_src(dg)), *dg_val4(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 4;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_SWAP8) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 16) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 8;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret8(ds[pos].dg) = sync_swap_8((volatile uint64_t*)ga2address(*dg_src(dg)), *dg_val8(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 8;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_ADD4) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 12) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 4;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret4(ds[pos].dg) = sync_fetch_and_add_4((volatile uint32_t*)ga2address(*dg_src(dg)), *dg_val4(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 4;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_ADD8) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 16) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 8;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret8(ds[pos].dg) = sync_fetch_and_add_8((volatile uint64_t*)ga2address(*dg_src(dg)), *dg_val8(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 8;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_XOR4) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 12) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 4;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret4(ds[pos].dg) = sync_fetch_and_xor_4((volatile uint32_t*)ga2address(*dg_src(dg)), *dg_val4(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 4;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_XOR8) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 16) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 8;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret8(ds[pos].dg) = sync_fetch_and_xor_8((volatile uint64_t*)ga2address(*dg_src(dg)), *dg_val8(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 8;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_OR4) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 12) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 4;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret4(ds[pos].dg) = sync_fetch_and_or_4((volatile uint32_t*)ga2address(*dg_src(dg)), *dg_val4(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 4;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_OR8) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 16) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 8;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret8(ds[pos].dg) = sync_fetch_and_or_8((volatile uint64_t*)ga2address(*dg_src(dg)), *dg_val8(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 8;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_AND4) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 12) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 4;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret4(ds[pos].dg) = sync_fetch_and_and_4((volatile uint32_t*)ga2address(*dg_src(dg)), *dg_val4(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 4;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_AND8) {
                    *dg_type(dg) = compare_seq(rxseq_table[rank], seq) <= 0 ? TYPE_ACK : TYPE_NACK;
                    if (is_ds_full() || recv_len < DG_HEADER_SIZE + 16) {
                        *dg_type(dg) = TYPE_RETRY;
                    } else if (rxseq_table[rank] == seq) {
                        pos = ds_add(DSSTAT_ATOMIC_WRITE, time);
                        drank = ga2rank(*dg_dst(dg));
                        *dg_type(ds[pos].dg) = TYPE_PUT;
                        *dg_rank(ds[pos].dg) = MY_RANK;
                        *dg_seq (ds[pos].dg) = inc_txseq(drank);
                        *dg_plen(ds[pos].dg) = 8;
                        *dg_dst (ds[pos].dg) = *dg_dst(dg);
                        *dg_ret8(ds[pos].dg) = sync_fetch_and_and_8((volatile uint64_t*)ga2address(*dg_src(dg)), *dg_val8(dg));
                        ds[pos].rank = rank;
                        ds[pos].offset = *dg_ptr(dg);
                        ds[pos].len = DG_HEADER_SIZE + 8;
                        ds[pos].addr.sin_port = PORT_TABLE[drank];
                        ds[pos].addr.sin_addr.s_addr = ADDR_TABLE[drank];
                        inc_rxseq(rank);
                    }
                    *dg_rank(dg) = MY_RANK;
                    *dg_seq (dg) = rxseq_table[rank];
                    sendto(sock, dg, 16, 0, (struct sockaddr *)&from, addr_len);
                    
                /* Control datagrams */
                } else if ((type == TYPE_ACK || type == TYPE_NACK || type == TYPE_RETRY) && recv_len >= 16) {
                    /* check command station */
                    for (i = cshead; i >= 0; i = cs[i].next)
                        if (cs[i].rank == rank) {
                            if (compare_seq(*dg_seq(cs[i].dg), seq) < 0) {
                                if      (cq[cs[i].cqp & MASK_CQ].stat == CQSTAT_COPY3TRY  ) cq[cs[i].cqp & MASK_CQ].stat = CQSTAT_COPY3WAIT;
                                else if (cq[cs[i].cqp & MASK_CQ].stat == CQSTAT_COPY1TRY  ) cq[cs[i].cqp & MASK_CQ].stat = 0;
                                else if (cq[cs[i].cqp & MASK_CQ].stat == CQSTAT_ATOMIC2TRY) cq[cs[i].cqp & MASK_CQ].stat = CQSTAT_ATOMIC2WAIT;
                                else if (cq[cs[i].cqp & MASK_CQ].stat == CQSTAT_ATOMIC1TRY) cq[cs[i].cqp & MASK_CQ].stat = 0;
                                cs_free(i, time);
                            } else if (type == TYPE_NACK && *dg_seq(cs[i].dg) == seq)
                                cs[i].time = time;
                            else if (type == TYPE_RETRY && *dg_seq(cs[i].dg) == seq)
                                cs[i].count = 0;
                        }
                    
                    /* check delegate station */
                    for (i = dshead; i >= 0; i = ds[i].next)
                        if (ds[i].stat == DSSTAT_ATOMIC_WRITE)
                            if (ga2rank(*dg_dst(ds[i].dg)) == rank)
                                if (compare_seq(*dg_seq(ds[i].dg), seq) < 0) {
                                    ds[i].stat = DSSTAT_ATOMIC_END;
                                    ds[i].time = time;
                                    ds[i].count = 0;
                                    *dg_type(ds[i].dg) = dsid2type(TYPE_END, i, 0);
                                    *dg_ptr(ds[i].dg) = ds[i].offset;
                                    ds[i].len = 24;
                                    ds[i].addr.sin_port = PORT_TABLE[ds[i].rank];
                                    ds[i].addr.sin_addr.s_addr = ADDR_TABLE[ds[i].rank];
                                }
                    
                /* Delegate datagrams */
                } else if (type == TYPE_READ && recv_len >= DG_HEADER_SIZE) {
                    *dg_type(dg) = dsid2type(TYPE_READACK, type2dsid(*dg_type(dg)), type2swid(*dg_type(dg)));
                    memcpy(dg_data(dg), ga2address(*dg_dst(dg)), *dg_len(dg));
                    sendto(sock, dg, DG_HEADER_SIZE + *dg_len(dg), 0, (struct sockaddr *)&from, addr_len);
                    
                } else if (type == TYPE_READACK && recv_len >= DG_HEADER_SIZE) {
                    dsid = type2dsid(*dg_type(dg));
                    swid = type2swid(*dg_type(dg));
                    if (ds[dsid].stat == DSSTAT_COPY_READ || ds[dsid].stat == DSSTAT_COPY2_READ)
                        if (*dg_rank(ds[dsid].dg) == *dg_rank(dg) && *dg_ptr(ds[dsid].dg) == *dg_ptr(dg) && ds[dsid].sw[swid].src == *dg_dst(dg)) {
                            memcpy(ds[dsid].sw[swid].dst, dg_data(dg), ds[dsid].sw[swid].len);
                            sw_free(dsid, swid, time);
                            if (ds[dsid].size == ds[dsid].offset && is_sw_empty(dsid)){
                                if (ds[dsid].stat == DSSTAT_COPY_READ) {
                                    ds[dsid].stat = DSSTAT_COPY_END;
                                    ds[dsid].time = time;
                                    ds[dsid].count = 0;
                                    *dg_type(ds[dsid].dg) = dsid2type(TYPE_END, dsid, 0);
                                    ds[dsid].len = 24;
                                    ds[dsid].addr.sin_port = PORT_TABLE[*dg_rank(ds[dsid].dg)];
                                    ds[dsid].addr.sin_addr.s_addr = ADDR_TABLE[*dg_rank(ds[dsid].dg)];
                                } else if (ds[dsid].stat == DSSTAT_COPY2_READ) {
                                    cqp = *dg_ptr(ds[dsid].dg);
                                    ds_free(dsid, time);
                                    if (cq[cqp & MASK_CQ].stat == CQSTAT_COPY2WAIT) {
                                        sync_synchronize();
                                        cq[cqp & MASK_CQ].stat = 0;
                                    }
                                }
                            }
                        }
                    
                } else if (type == TYPE_END && recv_len >= 24) {
                    *dg_type(dg) = dsid2type(TYPE_ENDACK, type2dsid(*dg_type(dg)), 0);
                    cqp = *dg_ptr(dg);
                    if (cqp < cqwp) {
                        if (cqp >= cqcp) {
                            if (cq[cqp & MASK_CQ].stat == CQSTAT_COPY3WAIT || cq[cqp & MASK_CQ].stat == CQSTAT_ATOMIC2WAIT) {
                                sync_synchronize();
                                cq[cqp & MASK_CQ].stat = 0;
                                sendto(sock, dg, 24, 0, (struct sockaddr *)&from, addr_len);
                            } else if (cq[cqp & MASK_CQ].stat >= 0)
                                sendto(sock, dg, 24, 0, (struct sockaddr *)&from, addr_len);
                        } else
                            sendto(sock, dg, 24, 0, (struct sockaddr *)&from, addr_len);
                    }
                    
                } else if (type == TYPE_ENDACK && recv_len >= 24) {
                    dsid = type2dsid(*dg_type(dg));
                    if (ds[dsid].stat == DSSTAT_COPY_END || ds[dsid].stat == DSSTAT_ATOMIC_END)
                        if (*dg_rank(ds[dsid].dg) == *dg_rank(dg) && *dg_ptr(ds[dsid].dg) == *dg_ptr(dg))
                            ds_free(dsid, time);
                }
            }
        }
        
        /*** Delegate Station ***/
        
        for (i = dshead; i >= 0; i = ds[i].next) {
            if (time >= ds[i].time) {
                if (ds[i].stat == DSSTAT_COPY_FENCE) {
                    rank = *dg_rank(ds[i].dg);
                    cqp = *dg_ptr(ds[i].dg);
                    for (j = dshead; j >= 0; j = ds[j].next)
                        if (j != i && *dg_rank(ds[j].dg) == rank && *dg_ptr(ds[j].dg) < cqp && (ds[j].stat == DSSTAT_COPY_FENCE || ds[j].stat == DSSTAT_COPY_READ)) break;
                    if (j < 0) ds[i].stat = DSSTAT_COPY_READ;
                }
                if (ds[i].stat == DSSTAT_COPY_READ && ga2rank(ds[i].dst) == ga2rank(ds[i].src)) {
                    /* Remote local copy */
                    memcpy(ga2address(ds[i].dst), ga2address(ds[i].src), ds[i].size);
                    ds[i].stat = DSSTAT_COPY_END;
                    ds[i].time = time;
                    ds[i].count = 0;
                    *dg_type(ds[i].dg) = dsid2type(TYPE_END, dsid, 0);
                    ds[i].len = 24;
                    ds[i].addr.sin_port = PORT_TABLE[*dg_rank(ds[i].dg)];
                    ds[i].addr.sin_addr.s_addr = ADDR_TABLE[*dg_rank(ds[i].dg)];
                }
                if (ds[i].stat == DSSTAT_COPY_READ || ds[i].stat == DSSTAT_COPY2_READ) {
                    /* Fill streaming window */
                    while (!ds[i].complete && is_sw_not_full(i)) {
                        j = sw_add(i, time);
                        len = ds[i].size - ds[i].offset;
                        if (len > MAX_DATA_SIZE) len = MAX_DATA_SIZE;
                        ds[i].sw[j].count = 0;
                        ds[i].sw[j].dst = ga2address(ds[i].dst + ds[i].offset);
                        ds[i].sw[j].src = ds[i].src + ds[i].offset;
                        ds[i].sw[j].len = len;
                        ds[i].offset += len;
                        if(ds[i].size <= ds[i].offset){
                            ds[i].complete = 1;
                        }
                    }
                    
                } else if (ds[i].stat == DSSTAT_ATOMIC_WRITE || ds[i].stat == DSSTAT_COPY_END || ds[i].stat == DSSTAT_ATOMIC_END) {
                    sendto(sock, ds[i].dg, ds[i].len, 0, (struct sockaddr *)&ds[i].addr, sock_addr_in_size);
                    /* Set retransmit timer from 200K to 20M clock (around 100 usec to 10 msec) */
                    ds[i].time = time + ((uint64_t)(0x4000 + ((i & 0xff) << 6)) << ((ds[i].count < 7) ? ds[i].count + 4 : 7 + 4));
                    ds[i].count++;
                }
            }
        }
        
        /*** Streaming Window ***/
        
        for (i = dshead; i >= 0; i = ds[i].next) {
            for (j = ds[i].swhead; j >= 0; j = ds[i].sw[j].next) {
                if (ds[i].sw[j].count < 2) free_count = 0;
                if (time >= ds[i].sw[j].time) {
                    *dg_type(ds[i].dg) = dsid2type(TYPE_READ, i, j);
                    *dg_len(ds[i].dg) = ds[i].sw[j].len;
                    *dg_dst(ds[i].dg) = ds[i].sw[j].src;
                    sendto(sock, ds[i].dg, DG_HEADER_SIZE, 0, (struct sockaddr *)&ds[i].addr, sock_addr_in_size);
                    /* Set retransmit timer from 200K to 200M clock (around 100 usec to 100 msec) */
                    ds[i].sw[j].time = time + ((uint64_t)(0x4000 + ((i & 0xff) << 6)) << ((ds[i].sw[j].count < 10) ? ds[i].sw[j].count + 4 : 10 + 4));
                    ds[i].sw[j].count++;
                }
            }
        }
    }
    
    close(sock);
    return NULL;
}

static pthread_t comm_thread_id;

int iacpbludp_init_gma(void)
{
    txseq_table = (uint32_t*)malloc(NUM_PROCS * sizeof(uint32_t));
    rxseq_table = (uint32_t*)malloc(NUM_PROCS * sizeof(uint32_t));
    init_sequence();
    
    comm_thread_ready = 0;
    abort_comm_thread = 0;
    sync_synchronize();
    pthread_create(&comm_thread_id, NULL, comm_thread_func, NULL);
    while (comm_thread_ready == 0) ;
    
    return 0;
}

int iacpbludp_finalize_gma(void)
{
    abort_comm_thread = 1;
    sync_synchronize();
    pthread_join(comm_thread_id, NULL);
    
    free(txseq_table);
    free(rxseq_table);
    txseq_table = NULL;
    rxseq_table = NULL;
    
    return 0;
}

void iacpbludp_abort_gma(void)
{
    abort_comm_thread = 1;
    sync_synchronize();
    pthread_join(comm_thread_id, NULL);
    
    free(txseq_table);
    free(rxseq_table);
    txseq_table = NULL;
    rxseq_table = NULL;
    
    return;
}

