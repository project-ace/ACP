/*
 * ACP Basic Layer GMA header for UDP
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
#ifndef __ACPBL_UDP_GMA_H__
#define __ACPBL_UDP_GMA_H__

/*** Network specification ***/

/* Bandwidth in Mbps */
#ifndef NETWORK_BANDWIDTH
#define NETWORK_BANDWIDTH 1000
#endif

/* Round trip time in usec */
#ifndef NETWORK_RTT
#define NETWORK_RTT 100
#endif

/* Max. predicted round trip time in usec */
#define MAX_NETWORK_RTT 1000000

/*** Datagram specification ***/

#ifndef DATAGRAM_BIAS
#define DATAGRAM_BIAS   66
#endif
#define MAX_DATA_SIZE   1408
#define MAX_DG_SIZE_VC0 56
#define MAX_DG_SIZE_VC1 (24 + MAX_DATA_SIZE)
#define MAX_DG_SIZE_VC2 20
#define MAX_DG_SIZE     MAX_DG_SIZE_VC1

/*** Shared memory buffer ***/

/* shared file path */
#ifndef SHMPATH
#define SHMPATH "/dev/shm/acpbludp"
#endif

/* virtual channel buffer size for intra-node communication */
#define IBUF_VC0_SIZE   8
#define IBUF_VC1_SIZE   8
#define IBUF_VC2_SIZE   8

/* virtual channel buffer size for inter-node transmit */
#define TXBUF_VC0_SIZE  128 // ((NETWORK_BANDWIDTH * NETWORK_RTT) / ((DATAGRAM_BIAS + MAX_DG_SIZE) << 3) + 1)
#define TXBUF_VC1_SIZE  128 // ((NETWORK_BANDWIDTH * NETWORK_RTT) / ((DATAGRAM_BIAS + MAX_DG_SIZE) << 3) + 1)
#define TXBUF_VC2_SIZE  128 // ((NETWORK_BANDWIDTH * NETWORK_RTT) / ((DATAGRAM_BIAS + MAX_DG_SIZE) << 3) + 1)

/* virtual channel buffer size for inter-node receive */
#define RXBUF_VC0_SIZE      256
#define RXBUF_VC1_SIZE      512
#define RXBUF_VC2_SIZE      128
#define RXBUF_VC1ACK_SIZE   127
#define RXBUF_SIZE          (RXBUF_VC0_SIZE + RXBUF_VC1_SIZE + RXBUF_VC2_SIZE + RXBUF_VC1ACK_SIZE + 1)

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} doorbell_t;

typedef struct {
    struct {
        struct {
            int head, tail;
        } dg;
        struct {
            pthread_mutex_t mutex;
            int head, tail;
        } free;
        struct {
            int next;
            uint8_t dg[MAX_DG_SIZE_VC0];
        } list[IBUF_VC0_SIZE];
    } vc0;
    struct {
        struct {
            int head, tail;
            uint64_t dg_count;
        } dg;
        struct {
            pthread_mutex_t mutex;
            int head, tail;
            uint64_t ack_count;
        } free;
        struct {
            int next;
            uint8_t dg[MAX_DG_SIZE_VC1];
        } list[IBUF_VC1_SIZE];
    } vc1;
    struct {
        struct {
            int head, tail;
        } dg;
        struct {
            pthread_mutex_t mutex;
            int head, tail;
        } free;
        struct {
            int next;
            uint8_t dg[MAX_DG_SIZE_VC2];
        } list[IBUF_VC2_SIZE];
    } vc2;
} ibuf_t;

typedef struct {
    struct {
        struct {
            pthread_mutex_t mutex;
            int head, tail;
        } dg, free;
        struct {
            int head, tail;
        } wait;
        struct {
            int next;
            int count;
            uint32_t send_to;
            uint8_t dg[MAX_DG_SIZE_VC0];
        } list[TXBUF_VC0_SIZE];
    } vc0;
    struct {
        struct {
            pthread_mutex_t mutex;
            int head, tail;
        } dg;
        struct {
            int head, tail;
        } free;
        struct {
            pthread_mutex_t mutex;
            int head, tail;
        } ack;
        struct {
            int head, tail;
        } wait;
        struct {
            int next;
            int count;
            int dq_pos;
            uint32_t send_to;
            uint8_t dg[MAX_DG_SIZE_VC1];
        } list[TXBUF_VC1_SIZE];
    } vc1;
    struct {
        struct {
            pthread_mutex_t mutex;
            int head, tail;
        } dg, free;
        struct {
            int head, tail;
        } wait;
        struct {
            int next;
            int count;
            uint32_t send_to;
            uint8_t dg[MAX_DG_SIZE_VC2];
        } list[TXBUF_VC2_SIZE];
    } vc2;
} txbuf_t;

typedef struct {
    struct {
        struct {
            int head, tail, num;
        } dg;
    } vc0, vc1, vc2;
    struct {
        pthread_mutex_t mutex;
        int head, tail;
        struct {
            int head, tail, num;
        } vc1ack;
    } free;
    struct {
        int next;
        uint8_t dg[MAX_DG_SIZE];
    } list[RXBUF_SIZE];
} rxbuf_t;

/*** Datagram format ***/

enum { COPY, CAS4, CAS8, SWAP4, SWAP8, ADD4, ADD8, XOR4, XOR8, OR4, OR8, AND4, AND8 };
enum { NORMAL, ACK, NACK, FULL};

#pragma pack(push, 4)

typedef struct {
    uint32_t task;
    uint32_t c:2, vc:2, rank:28;
    uint32_t ser:16, seq:16;
    uint64_t ptr;
    uint32_t s:1, type:4, :27;
    uint64_t dst, src, size;
} dg_copy_t;

typedef struct {
    uint32_t task;
    uint32_t c:2, vc:2, rank:28;
    uint32_t ser:16, seq:16;
    uint64_t ptr;
    uint32_t s:1, type:4, :27;
    uint64_t dst, src;
    uint32_t oldval, newval;
} dg_cas4_t;

typedef struct {
    uint32_t task;
    uint32_t c:2, vc:2, rank:28;
    uint32_t ser:16, seq:16;
    uint64_t ptr;
    uint32_t s:1, type:4, :27;
    uint64_t dst, src, oldval, newval;
} dg_cas8_t;

typedef struct {
    uint32_t task;
    uint32_t c:2, vc:2, rank:28;
    uint32_t ser:16, seq:16;
    uint64_t ptr;
    uint32_t s:1, type:4, :27;
    uint64_t dst, src;
    uint32_t val;
} dg_atomic4_t;

typedef struct {
    uint32_t task;
    uint32_t c:2, vc:2, rank:28;
    uint32_t ser:16, seq:16;
    uint64_t ptr;
    uint32_t s:1, type:4, :27;
    uint64_t dst, src, val;
} dg_atomic8_t;

typedef struct {
    uint32_t task;
    uint32_t c:2, vc:2, rank:28;
    uint32_t ser:16, seq:16;
    uint64_t dst;
    uint32_t len;
    uint8_t data[MAX_DATA_SIZE];
} dg_put_t;

typedef struct {
    uint32_t task;
    uint32_t c:2, vc:2, rank:28;
    uint32_t ser:16, seq:16;
    uint64_t ptr;
} dg_end_t;

typedef struct {
    uint32_t task;
    uint32_t c:2, vc:2, rank:28;
    uint32_t ser:16, seq:16, seq1:16, seq2:16;
} dg_control_t;

typedef union {
    dg_copy_t       copy;
    dg_cas4_t       cas4;
    dg_cas8_t       cas8;
    dg_atomic4_t    swap4;
    dg_atomic8_t    swap8;
    dg_atomic4_t    add4;
    dg_atomic8_t    add8;
    dg_atomic4_t    xor4;
    dg_atomic8_t    xor8;
    dg_atomic4_t    or4;
    dg_atomic8_t    or8;
    dg_atomic4_t    and4;
    dg_atomic8_t    and8;
    dg_put_t        put;
    dg_end_t        end;
    dg_control_t    ack;
    dg_control_t    nack;
    dg_control_t    full;
} dg_union;

#pragma pack(pop)

/*** Command queue and Delegate queue ***/

typedef struct {
    uint32_t stat;
    int inum;
    uint32_t gateway;
    acp_handle_t order;
    uint64_t ptr;
    uint32_t rank;
    uint32_t rfence;
    uint32_t type;
    acp_ga_t dst;
    acp_ga_t src;
    uint64_t size;
    uint32_t old4;
    uint32_t new4;
    uint64_t old8;
    uint64_t new8;
    uint32_t val4;
    uint64_t val8;
} cqe_t;

#ifndef ACPBL_UDP_CQ_SIZE
/* size in binary exponent */
#define ACPBL_UDP_CQ_SIZE 8
#endif

#define BIT_CQ    (ACPBL_UDP_CQ_SIZE)
#define WIDTH_CQ  (1LL << BIT_CQ)
#define MASK_CQ   (WIDTH_CQ - 1LL)

enum { CQSTAT_DONE, CQSTAT_WAIT, CQSTAT_11, CQSTAT_12, CQSTAT_2X };

#ifndef ACPBL_UDP_DQ_SIZE
/* size in binary exponent */
#define ACPBL_UDP_DQ_SIZE 10
#endif

#define BIT_DQ   (ACPBL_UDP_DQ_SIZE)
#define WIDTH_DQ (1LL << BIT_DQ)
#define MASK_DQ  (WIDTH_DQ - 1LL)

enum { DQSTAT_NOTIFY, DQSTAT_WAIT, DQSTAT_ACTIVE, DQSTAT_FENCE };

#pragma pack(push, 2)

/*** Serial number table ***/
typedef struct {
    uint16_t last, next;
} ser_entry_t;

/*** Sequence number table ***/

typedef struct {
    uint16_t txseq0, txseq1, txseq2, rxseq0, rxseq1, rxseq1fwd, rxseq2, full0:1, full1:1, full2:1;
} seq_entry_t;

#pragma pack(pop)

/*** Round trip time prediction table ***/

typedef struct {
    int64_t sa, sv;
} rtt_pred_entry_t;

/*** Retransmit list ***/

typedef struct {
    int next, prev;
    uint64_t time;
} retx_list_entry_t;

/*** Infrastructure functions ***/

extern int iacpbludp_init_gma(void);
extern int iacpbludp_finalize_gma(void);
extern void iacpbludp_abort_gma(void);

#endif /* acpbl_udp_gma.h */
