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
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <sched.h>
#include <acp.h>
#include "acpbl.h"
#include "acpbl_sync.h"
#include "acpbl_udp.h"
#include "acpbl_udp_gmm.h"
#include "acpbl_udp_gma.h"

/*
Xeon 5160     2.933333 GHz -> 15/44 nsec (hana)
Xeon E5520    2.266667 GHz -> 15/34 nsec (rx)
Opteron 8354  2.200000 GHz -> 5/11 nsec (hx)
*/

static inline uint64_t get_nsec(void)
{
#ifdef CLOCK_MONOTONIC_RAW
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
    return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
#else
    return (get_clock() * 15) / 34;
#endif
}

/****************************/
/* Thread control variables */
/****************************/

static pthread_mutex_t mutex_comm_thread_ready;
static pthread_cond_t cond_comm_thread_ready;
static pthread_mutex_t mutex_comm_thread_start;
static pthread_cond_t cond_comm_thread_start;
static pthread_mutex_t mutex_quit_comm_thread;
static int quit_comm_thread;

/*******************/
/* Sequence number */
/*******************/

seq_entry_t* seq_table;

static inline void init_seq(void)
{
    uint32_t rank;
    uint64_t data, crc64;
    int i, j;
    
    seq_table = (seq_entry_t*)malloc(sizeof(seq_entry_t) * NUM_PROCS * NODE_POP);
    
    /* Initialize sequence numbers for all ranks and all VCs */
    for (rank = 0; rank < NUM_PROCS; rank++) {
        data = ((uint64_t)TASKID << 32) | (uint64_t)rank;
        crc64 = 0ULL;
        for (i = 0; i < 64; i++){
            crc64 = (crc64 << 1) ^ ((((crc64 >> 63) ^ data) & 1ULL) ? 0x42f0e1eba9ea3693ULL : 0ULL);
            data >>= 1;
        }
        seq_table[rank].rxseq0 = crc64 & 0xffff;
        seq_table[rank].rxseq1 = (crc64 >> 24) & 0xffff;
        seq_table[rank].rxseq2 = (crc64 >> 48) & 0xffff;
        seq_table[rank].rxseq1fwd = seq_table[rank].rxseq1;
    }
    
    /* Fill the rest of seq_table */
    for (i = 0; i < NODE_POP; i++) {
        int k = i * NUM_PROCS;
        rank = LMEM_TABLE[i];
        for (j = 0; j < NUM_PROCS; j++) {
            seq_table[j + k].txseq0 = seq_table[rank].rxseq0;
            seq_table[j + k].txseq1 = seq_table[rank].rxseq1;
            seq_table[j + k].txseq2 = seq_table[rank].rxseq2;
            seq_table[j + k].full0 = 0;
            seq_table[j + k].full1 = 0;
            seq_table[j + k].full2 = 0;
            if (i == 0) continue;
            seq_table[j + k].rxseq0 = seq_table[j].rxseq0;
            seq_table[j + k].rxseq1 = seq_table[j].rxseq1;
            seq_table[j + k].rxseq2 = seq_table[j].rxseq2;
            seq_table[j + k].rxseq1fwd = seq_table[j].rxseq1;
        }
    }
    
    return;
}

static inline void finalize_seq(void)
{
    if (seq_table != NULL) free(seq_table);
    return;
}

static inline uint16_t inc_seq(uint16_t* ptr)
{
    uint16_t org = *ptr;
    *ptr = (org < 0xffff) ? org + 1 : 0;
    return org;
}

static inline int compare_seq(uint16_t seq1, uint16_t seq2)
{
    if (seq2 > seq1) return (seq2 - seq1 <  0x8000U) ? -1 :  1;
    if (seq1 > seq2) return (seq1 - seq2 <= 0x8000U) ?  1 : -1;
    return 0;
}

/************************/
/* Shared memory buffer */
/************************/

static int shmfd;
static void* shmbuf;
static size_t shmbuf_size;
static doorbell_t *doorbell;
static ibuf_t *ibuf;
static txbuf_t *txbuf;
static rxbuf_t *rxbuf;

static inline int ibuf_pos(int dst_inum, int src_inum)
{
    return NODE_POP * dst_inum + src_inum;
}

static int init_shmbuffer()
{
    char shmfn[256];
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;
    int i, j, p;
    char c;
    
    /* Create shared memory buffer */
    sprintf(shmfn, "%s_task%d_gateway%d", SHMPATH, TASKID, MY_GATEWAY);
    shmfd = open(shmfn, O_CREAT|O_RDWR, 0600);
    if (shmfd == -1) return -1;
    if (NUM_PROCS == NODE_POP)
        shmbuf_size = (sizeof(doorbell_t) + sizeof(ibuf_t) * NODE_POP) * NODE_POP;
    else
        shmbuf_size = (sizeof(doorbell_t) + sizeof(ibuf_t) * NODE_POP + sizeof(txbuf_t) + sizeof(rxbuf_t)) * NODE_POP;
    lseek(shmfd, shmbuf_size, SEEK_SET);
    read(shmfd, &c, sizeof(char));
    write(shmfd, &c, sizeof(char));
    shmbuf = mmap(NULL, shmbuf_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shmbuf == MAP_FAILED) return -1;
    doorbell = (doorbell_t*)shmbuf;
    ibuf = (ibuf_t*)(shmbuf + sizeof(doorbell_t) * NODE_POP);
    if (NUM_PROCS != NODE_POP) {
        txbuf = (txbuf_t*)(shmbuf + (sizeof(doorbell_t) + sizeof(ibuf_t) * NODE_POP) * NODE_POP);
        rxbuf = (rxbuf_t*)(shmbuf + (sizeof(doorbell_t) + sizeof(ibuf_t) * NODE_POP + sizeof(txbuf_t)) * NODE_POP);
    }
    
    /* Prepare attributes */
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
    
    /* Initialize doorbell */
    pthread_mutex_init(&doorbell[MY_INUM].mutex, &mutexattr);
    pthread_cond_init(&doorbell[MY_INUM].cond, &condattr);
    
    /* Initialize ibuf */
    p = NODE_POP * MY_INUM;
    for (i = 0 ; i < NODE_POP; i++) {
        ibuf[p].vc0.dg.head = ibuf[p].vc0.dg.tail = -1;
        for (j = 0; j < IBUF_VC0_SIZE - 1; j++) ibuf[p].vc0.list[j].next = j + 1;
        ibuf[p].vc0.list[j].next = -1;
        ibuf[p].vc0.free.head = 0;
        ibuf[p].vc0.free.tail = j;
        pthread_mutex_init(&ibuf[p].vc0.free.mutex, &mutexattr);
        
        ibuf[p].vc1.dg.head = ibuf[p].vc1.dg.tail = -1;
        ibuf[p].vc1.dg.dg_count = ibuf[p].vc1.free.ack_count = 0;
        for (j = 0; j < IBUF_VC1_SIZE - 1; j++) ibuf[p].vc1.list[j].next = j + 1;
        ibuf[p].vc1.list[j].next = -1;
        ibuf[p].vc1.free.head = 0;
        ibuf[p].vc1.free.tail = j;
        pthread_mutex_init(&ibuf[p].vc1.free.mutex, &mutexattr);
        
        ibuf[p].vc2.dg.head = ibuf[p].vc2.dg.tail = -1;
        for (j = 0; j < IBUF_VC2_SIZE - 1; j++) ibuf[p].vc2.list[j].next = j + 1;
        ibuf[p].vc2.list[j].next = -1;
        ibuf[p].vc2.free.head = 0;
        ibuf[p].vc2.free.tail = j;
        pthread_mutex_init(&ibuf[p].vc2.free.mutex, &mutexattr);
        p++;
    }
    
    if (NUM_PROCS == NODE_POP) return 0;
    
    /* Initialize txbuf */
    txbuf[MY_INUM].vc0.dg.head = txbuf[MY_INUM].vc0.dg.tail = -1;
    for (i = 0; i < TXBUF_VC0_SIZE - 1; i++) txbuf[MY_INUM].vc0.list[i].next = i + 1;
    txbuf[MY_INUM].vc0.list[i].next = -1;
    txbuf[MY_INUM].vc0.free.head = 0;
    txbuf[MY_INUM].vc0.free.tail = i;
    txbuf[MY_INUM].vc0.wait.head = txbuf[MY_INUM].vc0.wait.tail = -1;
    pthread_mutex_init(&txbuf[MY_INUM].vc0.dg.mutex, &mutexattr);
    pthread_mutex_init(&txbuf[MY_INUM].vc0.free.mutex, &mutexattr);
    
    txbuf[MY_INUM].vc1.dg.head = txbuf[MY_INUM].vc1.dg.tail = -1;
    for (i = 0; i < TXBUF_VC1_SIZE - 1; i++) txbuf[MY_INUM].vc1.list[i].next = i + 1;
    txbuf[MY_INUM].vc1.list[i].next = -1;
    txbuf[MY_INUM].vc1.free.head = 0;
    txbuf[MY_INUM].vc1.free.tail = i;
    txbuf[MY_INUM].vc1.ack.head = txbuf[MY_INUM].vc1.ack.tail = -1;
    txbuf[MY_INUM].vc1.wait.head = txbuf[MY_INUM].vc1.wait.tail = -1;
    pthread_mutex_init(&txbuf[MY_INUM].vc1.dg.mutex, &mutexattr);
    pthread_mutex_init(&txbuf[MY_INUM].vc1.ack.mutex, &mutexattr);
    
    txbuf[MY_INUM].vc2.dg.head = txbuf[MY_INUM].vc2.dg.tail = -1;
    for (i = 0; i < TXBUF_VC2_SIZE - 1; i++) txbuf[MY_INUM].vc2.list[i].next = i + 1;
    txbuf[MY_INUM].vc2.list[i].next = -1;
    txbuf[MY_INUM].vc2.free.head = 0;
    txbuf[MY_INUM].vc2.free.tail = i;
    txbuf[MY_INUM].vc2.wait.head = txbuf[MY_INUM].vc2.wait.tail = -1;
    pthread_mutex_init(&txbuf[MY_INUM].vc2.dg.mutex, &mutexattr);
    pthread_mutex_init(&txbuf[MY_INUM].vc2.free.mutex, &mutexattr);
    
    /* Initialize rxbuf */
    rxbuf[MY_INUM].vc0.dg.head = rxbuf[MY_INUM].vc0.dg.tail = -1;
    rxbuf[MY_INUM].vc0.dg.num = 0;
    rxbuf[MY_INUM].vc1.dg.head = rxbuf[MY_INUM].vc1.dg.tail = -1;
    rxbuf[MY_INUM].vc1.dg.num = 0;
    rxbuf[MY_INUM].vc2.dg.head = rxbuf[MY_INUM].vc2.dg.tail = -1;
    rxbuf[MY_INUM].vc2.dg.num = 0;
    for (i = 0; i < RXBUF_SIZE - 1; i++) rxbuf[MY_INUM].list[i].next = i + 1;
    rxbuf[MY_INUM].list[i].next = -1;
    rxbuf[MY_INUM].free.head = 0;
    rxbuf[MY_INUM].free.tail = i;
    rxbuf[MY_INUM].free.vc1ack.head = rxbuf[MY_INUM].free.vc1ack.tail = -1;
    rxbuf[MY_INUM].free.vc1ack.num = 0;
    pthread_mutex_init(&rxbuf[MY_INUM].free.mutex, &mutexattr);
    
    return 0;
}

static void finalize_shmbuffer()
{
    int i, p;
    
    if (NUM_PROCS != NODE_POP) {
        /* Finalize rxbuf */
        pthread_mutex_destroy(&rxbuf[MY_INUM].free.mutex);
        
        /* Finalize txbuf */
        pthread_mutex_destroy(&txbuf[MY_INUM].vc0.dg.mutex);
        pthread_mutex_destroy(&txbuf[MY_INUM].vc0.free.mutex);
        
        pthread_mutex_destroy(&txbuf[MY_INUM].vc1.dg.mutex);
        pthread_mutex_destroy(&txbuf[MY_INUM].vc1.ack.mutex);
        
        pthread_mutex_destroy(&txbuf[MY_INUM].vc2.dg.mutex);
        pthread_mutex_destroy(&txbuf[MY_INUM].vc2.free.mutex);
    }
    
    /* Finalize ibuf */
    p = NODE_POP * MY_INUM;
    for (i = 0 ; i < NODE_POP; i++) {
        pthread_mutex_destroy(&ibuf[p].vc0.free.mutex);
        pthread_mutex_destroy(&ibuf[p].vc1.free.mutex);
        pthread_mutex_destroy(&ibuf[p].vc2.free.mutex);
        p++;
    }
    
    /* Finalize doorbell */
    pthread_cond_destroy(&doorbell[MY_INUM].cond);
    pthread_mutex_destroy(&doorbell[MY_INUM].mutex);
    
    /* Destroy shared memory buffer */
    munmap(shmbuf, shmbuf_size);
    close(shmfd);
    
    return;
}

/* Doorbell functions */

static inline void doorbell_wait(void)
{
    /* Need doorbell.[MY_INUM].mutex locked */
    if (MY_INUM == 0 && NUM_PROCS != NODE_POP) return;
    pthread_cond_wait(&doorbell[MY_INUM].cond, &doorbell[MY_INUM].mutex);
    return;
}

static inline void doorbell_ring(int inum)
{
    /* Need doorbell.[inum].mutex locked */
    if (inum == 0 && NUM_PROCS != NODE_POP) return;
    //pthread_cond_signal(&doorbell[inum].cond);
    pthread_cond_broadcast(&doorbell[inum].cond);
    return;
}

/* Pop free entry from target ibuf */

static inline int ibuf_vc0_pop_free(int inum)
{
    int pos, ret;
    
    pos = ibuf_pos(inum, MY_INUM);
    pthread_mutex_lock(&ibuf[pos].vc0.free.mutex);
    ret = ibuf[pos].vc0.free.head;
    if (ret >= 0)
        if (ibuf[pos].vc0.list[ret].next < 0)
            ibuf[pos].vc0.free.head = ibuf[pos].vc0.free.tail = -1;
        else
            ibuf[pos].vc0.free.head = ibuf[pos].vc0.list[ret].next;
    pthread_mutex_unlock(&ibuf[pos].vc0.free.mutex);
    
    return ret;
}

static inline int ibuf_vc1_pop_free(int inum)
{
    int pos, ret;
    
    pos = ibuf_pos(inum, MY_INUM);
    pthread_mutex_lock(&ibuf[pos].vc1.free.mutex);
    ret = ibuf[pos].vc1.free.head;
    if (ret >= 0)
        if (ibuf[pos].vc1.list[ret].next < 0)
            ibuf[pos].vc1.free.head = ibuf[pos].vc1.free.tail = -1;
        else
            ibuf[pos].vc1.free.head = ibuf[pos].vc1.list[ret].next;
    pthread_mutex_unlock(&ibuf[pos].vc1.free.mutex);
    
    return ret;
}

static inline int ibuf_vc2_pop_free(int inum)
{
    int pos, ret;
    
    pos = ibuf_pos(inum, MY_INUM);
    pthread_mutex_lock(&ibuf[pos].vc2.free.mutex);
    ret = ibuf[pos].vc2.free.head;
    if (ret >= 0)
        if (ibuf[pos].vc2.list[ret].next < 0)
            ibuf[pos].vc2.free.head = ibuf[pos].vc2.free.tail = -1;
        else
            ibuf[pos].vc2.free.head = ibuf[pos].vc2.list[ret].next;
    pthread_mutex_unlock(&ibuf[pos].vc2.free.mutex);
    
    return ret;
}

/* Push datagram entry to target ibuf */

static inline void ibuf_vc0_push_dg(int inum, int elem_id)
{
    int pos;
    
    pos = ibuf_pos(inum, MY_INUM);
    pthread_mutex_lock(&doorbell[inum].mutex);
    ibuf[pos].vc0.list[elem_id].next = -1;
    if (ibuf[pos].vc0.dg.tail < 0)
        ibuf[pos].vc0.dg.head = elem_id;
    else
        ibuf[pos].vc0.list[ibuf[pos].vc0.dg.tail].next = elem_id;
    ibuf[pos].vc0.dg.tail = elem_id;
    doorbell_ring(inum);
    pthread_mutex_unlock(&doorbell[inum].mutex);
    
    return;
}

static inline uint64_t ibuf_vc1_push_dg(int inum, int elem_id)
{
    int pos;
    uint64_t ret;
    
    pos = ibuf_pos(inum, MY_INUM);
    pthread_mutex_lock(&doorbell[inum].mutex);
    ibuf[pos].vc1.list[elem_id].next = -1;
    if (ibuf[pos].vc1.dg.tail < 0)
        ibuf[pos].vc1.dg.head = elem_id;
    else
        ibuf[pos].vc1.list[ibuf[pos].vc1.dg.tail].next = elem_id;
    ibuf[pos].vc1.dg.tail = elem_id;
    ret = ++ibuf[pos].vc1.dg.dg_count;
    doorbell_ring(inum);
    pthread_mutex_unlock(&doorbell[inum].mutex);
    
    return ret;
}

static inline void ibuf_vc2_push_dg(int inum, int elem_id)
{
    int pos;
    
    pos = ibuf_pos(inum, MY_INUM);
    pthread_mutex_lock(&doorbell[inum].mutex);
    ibuf[pos].vc2.list[elem_id].next = -1;
    if (ibuf[pos].vc2.dg.tail < 0)
        ibuf[pos].vc2.dg.head = elem_id;
    else
        ibuf[pos].vc2.list[ibuf[pos].vc2.dg.tail].next = elem_id;
    ibuf[pos].vc2.dg.tail = elem_id;
    doorbell_ring(inum);
    pthread_mutex_unlock(&doorbell[inum].mutex);
    
    return;
}

/* Pop datagram entry from my ibuf */

static inline int ibuf_vc0_pop_dg(int inum)
{
    /* Need doorbell.[MY_INUM].mutex locked if MY_INUM > 0 */
    int pos, ret;
    pos = ibuf_pos(MY_INUM, inum);
    if (MY_INUM == 0) pthread_mutex_lock(&doorbell[MY_INUM].mutex);
    ret = ibuf[pos].vc0.dg.head;
    if (ret >= 0)
        if (ibuf[pos].vc0.list[ret].next < 0)
            ibuf[pos].vc0.dg.head = ibuf[pos].vc0.dg.tail = -1;
        else
            ibuf[pos].vc0.dg.head = ibuf[pos].vc0.list[ret].next;
    if (MY_INUM == 0) pthread_mutex_unlock(&doorbell[MY_INUM].mutex);
    return ret;
}

static inline int ibuf_vc1_pop_dg(int inum)
{
    /* Need doorbell.[MY_INUM].mutex locked if MY_INUM > 0 */
    int pos, ret;
    pos = ibuf_pos(MY_INUM, inum);
    if (MY_INUM == 0) pthread_mutex_lock(&doorbell[MY_INUM].mutex);
    ret = ibuf[pos].vc1.dg.head;
    if (ret >= 0) {
        if (ibuf[pos].vc1.list[ret].next < 0)
            ibuf[pos].vc1.dg.head = ibuf[pos].vc1.dg.tail = -1;
        else
            ibuf[pos].vc1.dg.head = ibuf[pos].vc1.list[ret].next;
    }
    if (MY_INUM == 0) pthread_mutex_unlock(&doorbell[MY_INUM].mutex);
    return ret;
}

static inline int ibuf_vc2_pop_dg(int inum)
{
    /* Need doorbell.[MY_INUM].mutex locked */
    int pos, ret;
    pos = ibuf_pos(MY_INUM, inum);
    ret = ibuf[pos].vc2.dg.head;
    if (ret >= 0)
        if (ibuf[pos].vc2.list[ret].next < 0)
            ibuf[pos].vc2.dg.head = ibuf[pos].vc2.dg.tail = -1;
        else
            ibuf[pos].vc2.dg.head = ibuf[pos].vc2.list[ret].next;
    return ret;
}

/* Push free entry to my ibuf */

static inline void ibuf_vc0_push_free(int inum, int elem_id)
{
    int pos;
    
    pos = ibuf_pos(MY_INUM, inum);
    pthread_mutex_lock(&ibuf[pos].vc0.free.mutex);
    ibuf[pos].vc0.list[elem_id].next = -1;
    if (ibuf[pos].vc0.free.tail < 0)
        ibuf[pos].vc0.free.head = elem_id;
    else
        ibuf[pos].vc0.list[ibuf[pos].vc0.free.tail].next = elem_id;
    ibuf[pos].vc0.free.tail = elem_id;
    pthread_mutex_unlock(&ibuf[pos].vc0.free.mutex);
    
    return;
}

static inline void ibuf_vc1_push_free(int inum, int elem_id)
{
    int pos;
    
    pos = ibuf_pos(MY_INUM, inum);
    pthread_mutex_lock(&ibuf[pos].vc1.free.mutex);
    ibuf[pos].vc1.list[elem_id].next = -1;
    if (ibuf[pos].vc1.free.tail < 0)
        ibuf[pos].vc1.free.head = elem_id;
    else
        ibuf[pos].vc1.list[ibuf[pos].vc1.free.tail].next = elem_id;
    ibuf[pos].vc1.free.tail = elem_id;
    ibuf[pos].vc1.free.ack_count++;
    pthread_mutex_unlock(&ibuf[pos].vc1.free.mutex);
    
    return;
}

static inline uint64_t ibuf_vc1_free_ack_count(int inum)
{
    uint64_t ret;
    int pos;
    
    pos = ibuf_pos(inum, MY_INUM);
    pthread_mutex_lock(&ibuf[pos].vc1.free.mutex);
    ret = ibuf[pos].vc1.free.ack_count;
    pthread_mutex_unlock(&ibuf[pos].vc1.free.mutex);
    
    return ret;
}

static inline void ibuf_vc2_push_free(int inum, int elem_id)
{
    int pos;
    
    pos = ibuf_pos(MY_INUM, inum);
    pthread_mutex_lock(&ibuf[pos].vc2.free.mutex);
    ibuf[pos].vc2.list[elem_id].next = -1;
    if (ibuf[pos].vc2.free.tail < 0)
        ibuf[pos].vc2.free.head = elem_id;
    else
        ibuf[pos].vc2.list[ibuf[pos].vc2.free.tail].next = elem_id;
    ibuf[pos].vc2.free.tail = elem_id;
    pthread_mutex_unlock(&ibuf[pos].vc2.free.mutex);
    
    return;
}

/* Pop free entry from my txbuf */

static inline int txbuf_vc0_pop_free(void)
{
    int ret;
    
    if (MY_INUM > 0) pthread_mutex_lock(&txbuf[MY_INUM].vc0.free.mutex);
    ret = txbuf[MY_INUM].vc0.free.head;
    if (ret >= 0)
        if (txbuf[MY_INUM].vc0.list[ret].next < 0)
            txbuf[MY_INUM].vc0.free.head = txbuf[MY_INUM].vc0.free.tail = -1;
        else
            txbuf[MY_INUM].vc0.free.head = txbuf[MY_INUM].vc0.list[ret].next;
    if (MY_INUM > 0) pthread_mutex_unlock(&txbuf[MY_INUM].vc0.free.mutex);
    
    return ret;
}

static inline int txbuf_vc1_pop_free(void)
{
    int ret;
    
    ret = txbuf[MY_INUM].vc1.free.head;
    if (ret >= 0)
        if (txbuf[MY_INUM].vc1.list[ret].next < 0)
            txbuf[MY_INUM].vc1.free.head = txbuf[MY_INUM].vc1.free.tail = -1;
        else
            txbuf[MY_INUM].vc1.free.head = txbuf[MY_INUM].vc1.list[ret].next;
    
    return ret;
}

static inline int txbuf_vc2_pop_free(void)
{
    int ret;
    
    if (MY_INUM > 0) pthread_mutex_lock(&txbuf[MY_INUM].vc2.free.mutex);
    ret = txbuf[MY_INUM].vc2.free.head;
    if (ret >= 0)
        if (txbuf[MY_INUM].vc2.list[ret].next < 0)
            txbuf[MY_INUM].vc2.free.head = txbuf[MY_INUM].vc2.free.tail = -1;
        else
            txbuf[MY_INUM].vc2.free.head = txbuf[MY_INUM].vc2.list[ret].next;
    if (MY_INUM > 0) pthread_mutex_unlock(&txbuf[MY_INUM].vc2.free.mutex);
    
    return ret;
}

/* Push datagram entry to my txbuf */

static inline void txbuf_vc0_push_dg(int elem_id)
{
    if (MY_INUM > 0) pthread_mutex_lock(&txbuf[MY_INUM].vc0.dg.mutex);
    txbuf[MY_INUM].vc0.list[elem_id].next = -1;
    txbuf[MY_INUM].vc0.list[elem_id].count = 0;
    if (txbuf[MY_INUM].vc0.dg.tail < 0)
        txbuf[MY_INUM].vc0.dg.head = elem_id;
    else
        txbuf[MY_INUM].vc0.list[txbuf[MY_INUM].vc0.dg.tail].next = elem_id;
    txbuf[MY_INUM].vc0.dg.tail = elem_id;
    if (MY_INUM > 0) pthread_mutex_unlock(&txbuf[MY_INUM].vc0.dg.mutex);
    
    return;
}

static inline void txbuf_vc1_push_dg(int elem_id)
{
    if (MY_INUM > 0) pthread_mutex_lock(&txbuf[MY_INUM].vc1.dg.mutex);
    txbuf[MY_INUM].vc1.list[elem_id].next = -1;
    txbuf[MY_INUM].vc1.list[elem_id].count = 0;
    if (txbuf[MY_INUM].vc1.dg.tail < 0)
        txbuf[MY_INUM].vc1.dg.head = elem_id;
    else
        txbuf[MY_INUM].vc1.list[txbuf[MY_INUM].vc1.dg.tail].next = elem_id;
    txbuf[MY_INUM].vc1.dg.tail = elem_id;
    if (MY_INUM > 0) pthread_mutex_unlock(&txbuf[MY_INUM].vc1.dg.mutex);
    
    return;
}

static inline void txbuf_vc2_push_dg(int elem_id)
{
    if (MY_INUM > 0) pthread_mutex_lock(&txbuf[MY_INUM].vc2.dg.mutex);
    txbuf[MY_INUM].vc2.list[elem_id].next = -1;
    txbuf[MY_INUM].vc2.list[elem_id].count = 0;
    if (txbuf[MY_INUM].vc2.dg.tail < 0)
        txbuf[MY_INUM].vc2.dg.head = elem_id;
    else
        txbuf[MY_INUM].vc2.list[txbuf[MY_INUM].vc2.dg.tail].next = elem_id;
    txbuf[MY_INUM].vc2.dg.tail = elem_id;
    if (MY_INUM > 0) pthread_mutex_unlock(&txbuf[MY_INUM].vc2.dg.mutex);
    
    return;
}

/* Pop datagram entry from target txbuf */

static inline int txbuf_vc0_pop_dg(int inum)
{
    dg_union* dgp;
    int ret, full;
    
    if (inum > 0) pthread_mutex_lock(&txbuf[inum].vc0.dg.mutex);
    ret = txbuf[inum].vc0.dg.head;
    if (ret >= 0) {
        dgp = (dg_union*)txbuf[inum].vc0.list[ret].dg;
        if (seq_table[NUM_PROCS * inum + dgp->copy.rank].full0 && txbuf[inum].vc0.list[ret].count < 16) {
            txbuf[inum].vc0.list[ret].count++;
            ret = -1;
        } else if (txbuf[inum].vc0.list[ret].next < 0)
            txbuf[inum].vc0.dg.head = txbuf[inum].vc0.dg.tail = -1;
        else
            txbuf[inum].vc0.dg.head = txbuf[inum].vc0.list[ret].next;
    }
    if (inum > 0) pthread_mutex_unlock(&txbuf[inum].vc0.dg.mutex);
    
    return ret;
}

static inline int txbuf_vc1_pop_dg(int inum)
{
    dg_union* dgp;
    int ret, full;
    
    if (inum > 0) pthread_mutex_lock(&txbuf[inum].vc1.dg.mutex);
    ret = txbuf[inum].vc1.dg.head;
    if (ret >= 0) {
        dgp = (dg_union*)txbuf[inum].vc1.list[ret].dg;
        if (seq_table[NUM_PROCS * inum + dgp->copy.rank].full1 && txbuf[inum].vc1.list[ret].count < 16) {
            txbuf[inum].vc1.list[ret].count++;
            ret = -1;
        } else if (txbuf[inum].vc1.list[ret].next < 0)
            txbuf[inum].vc1.dg.head = txbuf[inum].vc1.dg.tail = -1;
        else
            txbuf[inum].vc1.dg.head = txbuf[inum].vc1.list[ret].next;
    }
    if (inum > 0) pthread_mutex_unlock(&txbuf[inum].vc1.dg.mutex);
    
    return ret;
}

static inline int txbuf_vc2_pop_dg(int inum)
{
    dg_union* dgp;
    int ret, full;
    
    if (inum > 0) pthread_mutex_lock(&txbuf[inum].vc2.dg.mutex);
    ret = txbuf[inum].vc2.dg.head;
    if (ret >= 0) {
        dgp = (dg_union*)txbuf[inum].vc2.list[ret].dg;
        if (seq_table[NUM_PROCS * inum + dgp->copy.rank].full2 && txbuf[inum].vc2.list[ret].count < 16) {
            txbuf[inum].vc2.list[ret].count++;
            ret = -1;
        } else if (txbuf[inum].vc2.list[ret].next < 0)
            txbuf[inum].vc2.dg.head = txbuf[inum].vc2.dg.tail = -1;
        else
            txbuf[inum].vc2.dg.head = txbuf[inum].vc2.list[ret].next;
    }
    if (inum > 0) pthread_mutex_unlock(&txbuf[inum].vc2.dg.mutex);
    
    return ret;
}

/* Push wait entry to target txbuf */

static inline void txbuf_vc0_push_wait(int inum, int elem_id)
{
    txbuf[inum].vc0.list[elem_id].next = -1;
    if (txbuf[inum].vc0.wait.tail < 0)
        txbuf[inum].vc0.wait.head = elem_id;
    else
        txbuf[inum].vc0.list[txbuf[inum].vc0.wait.tail].next = elem_id;
    txbuf[inum].vc0.wait.tail = elem_id;
    
    return;
}

static inline void txbuf_vc1_push_wait(int inum, int elem_id)
{
    txbuf[inum].vc1.list[elem_id].next = -1;
    if (txbuf[inum].vc1.wait.tail < 0)
        txbuf[inum].vc1.wait.head = elem_id;
    else
        txbuf[inum].vc1.list[txbuf[inum].vc1.wait.tail].next = elem_id;
    txbuf[inum].vc1.wait.tail = elem_id;
    
    return;
}

static inline void txbuf_vc2_push_wait(int inum, int elem_id)
{
    txbuf[inum].vc2.list[elem_id].next = -1;
    if (txbuf[inum].vc2.wait.tail < 0)
        txbuf[inum].vc2.wait.head = elem_id;
    else
        txbuf[inum].vc2.list[txbuf[inum].vc2.wait.tail].next = elem_id;
    txbuf[inum].vc2.wait.tail = elem_id;
    
    return;
}

/* Pop wait entry to target txbuf */

static inline int txbuf_vc0_pop_wait(int inum)
{
    int ret;
    
    ret = txbuf[inum].vc0.wait.head;
    if (ret >= 0)
        if (txbuf[inum].vc0.list[ret].next < 0)
            txbuf[inum].vc0.wait.head = txbuf[inum].vc0.wait.tail = -1;
        else
            txbuf[inum].vc0.wait.head = txbuf[inum].vc0.list[ret].next;
    
    return ret;
}

static inline int txbuf_vc1_pop_wait(int inum)
{
    int ret;
    
    ret = txbuf[inum].vc1.wait.head;
    if (ret >= 0)
        if (txbuf[inum].vc1.list[ret].next < 0)
            txbuf[inum].vc1.wait.head = txbuf[inum].vc1.wait.tail = -1;
        else
            txbuf[inum].vc1.wait.head = txbuf[inum].vc1.list[ret].next;
    
    return ret;
}

static inline int txbuf_vc2_pop_wait(int inum)
{
    int ret;
    
    ret = txbuf[inum].vc2.wait.head;
    if (ret >= 0)
        if (txbuf[inum].vc2.list[ret].next < 0)
            txbuf[inum].vc2.wait.head = txbuf[inum].vc2.wait.tail = -1;
        else
            txbuf[inum].vc2.wait.head = txbuf[inum].vc2.list[ret].next;
    
    return ret;
}

/* Push ack entry to target txbuf vc1 */

static inline void txbuf_vc1_push_ack(int inum, int elem_id)
{
    if (inum > 0) pthread_mutex_lock(&txbuf[inum].vc1.ack.mutex);
    txbuf[inum].vc1.list[elem_id].next = -1;
    if (txbuf[inum].vc1.ack.tail < 0)
        txbuf[inum].vc1.ack.head = elem_id;
    else
        txbuf[inum].vc1.list[txbuf[inum].vc1.ack.tail].next = elem_id;
    txbuf[inum].vc1.ack.tail = elem_id;
    if (inum > 0) pthread_mutex_unlock(&txbuf[inum].vc1.ack.mutex);
    
    return;
}

/* Pop ack entry from my txbuf vc1 */

static inline int txbuf_vc1_pop_ack(void)
{
    int ret;
    
    if (MY_INUM > 0) pthread_mutex_lock(&txbuf[MY_INUM].vc1.ack.mutex);
    ret = txbuf[MY_INUM].vc1.ack.head;
    if (ret >= 0)
        if (txbuf[MY_INUM].vc1.list[ret].next < 0)
            txbuf[MY_INUM].vc1.ack.head = txbuf[MY_INUM].vc1.ack.tail = -1;
        else
            txbuf[MY_INUM].vc1.ack.head = txbuf[MY_INUM].vc1.list[ret].next;
    if (MY_INUM > 0) pthread_mutex_unlock(&txbuf[MY_INUM].vc1.ack.mutex);
    
    return ret;
}

/* Push free entry to target txbuf */

static inline void txbuf_vc0_push_free(int inum, int elem_id)
{
    if (inum > 0) pthread_mutex_lock(&txbuf[inum].vc0.free.mutex);
    txbuf[inum].vc0.list[elem_id].next = -1;
    if (txbuf[inum].vc0.free.tail < 0)
        txbuf[inum].vc0.free.head = elem_id;
    else
        txbuf[inum].vc0.list[txbuf[inum].vc0.free.tail].next = elem_id;
    txbuf[inum].vc0.free.tail = elem_id;
    if (inum > 0) pthread_mutex_unlock(&txbuf[inum].vc0.free.mutex);
    
    return;
}

static inline void txbuf_vc1_push_free(int elem_id)
{
    txbuf[MY_INUM].vc1.list[elem_id].next = -1;
    if (txbuf[MY_INUM].vc1.free.tail < 0)
        txbuf[MY_INUM].vc1.free.head = elem_id;
    else
        txbuf[MY_INUM].vc1.list[txbuf[MY_INUM].vc1.free.tail].next = elem_id;
    txbuf[MY_INUM].vc1.free.tail = elem_id;
    
    return;
}

static inline void txbuf_vc2_push_free(int inum, int elem_id)
{
    if (inum > 0) pthread_mutex_lock(&txbuf[inum].vc2.free.mutex);
    txbuf[inum].vc2.list[elem_id].next = -1;
    if (txbuf[inum].vc2.free.tail < 0)
        txbuf[inum].vc2.free.head = elem_id;
    else
        txbuf[inum].vc2.list[txbuf[inum].vc2.free.tail].next = elem_id;
    txbuf[inum].vc2.free.tail = elem_id;
    if (inum > 0) pthread_mutex_unlock(&txbuf[inum].vc2.free.mutex);
    
    return;
}

/* Pop free entry from target rxbuf */

static inline int rxbuf_pop_free(int inum)
{
    int ret;
    
    if (inum > 0) pthread_mutex_lock(&rxbuf[inum].free.mutex);
    ret = rxbuf[inum].free.head;
    if (ret >= 0)
        if (rxbuf[inum].list[ret].next < 0)
            rxbuf[inum].free.head = rxbuf[inum].free.tail = -1;
        else
            rxbuf[inum].free.head = rxbuf[inum].list[ret].next;
    if (inum > 0) pthread_mutex_unlock(&rxbuf[inum].free.mutex);
    
    return ret;
}

/* Push datagram entry to target rxbuf */

static inline int rxbuf_vc0_push_dg(int inum, int elem_id)
{
    int ret = 0;
    
    if (inum > 0) pthread_mutex_lock(&doorbell[inum].mutex);
    if (rxbuf[inum].vc0.dg.num < RXBUF_VC0_SIZE) {
        rxbuf[inum].list[elem_id].next = -1;
        if (rxbuf[inum].vc0.dg.tail < 0)
            rxbuf[inum].vc0.dg.head = elem_id;
        else
            rxbuf[inum].list[rxbuf[inum].vc0.dg.tail].next = elem_id;
        rxbuf[inum].vc0.dg.tail = elem_id;
        rxbuf[inum].vc0.dg.num += 1;
        doorbell_ring(inum);
    } else
        ret = -1;
    if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
    
    return ret;
}

static inline int rxbuf_vc1_push_dg(int inum, int elem_id)
{
    int ret = 0;
    
    if (inum > 0) pthread_mutex_lock(&doorbell[inum].mutex);
    if (rxbuf[inum].vc1.dg.num < RXBUF_VC1_SIZE) {
        rxbuf[inum].list[elem_id].next = -1;
        if (rxbuf[inum].vc1.dg.tail < 0)
            rxbuf[inum].vc1.dg.head = elem_id;
        else
            rxbuf[inum].list[rxbuf[inum].vc1.dg.tail].next = elem_id;
        rxbuf[inum].vc1.dg.tail = elem_id;
        rxbuf[inum].vc1.dg.num += 1;
        doorbell_ring(inum);
    } else
        ret = -1;
    if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
    
    return ret;
}

static inline int rxbuf_vc2_push_dg(int inum, int elem_id)
{
    int ret = 0;
    
    if (inum > 0) pthread_mutex_lock(&doorbell[inum].mutex);
    if (rxbuf[inum].vc2.dg.num < RXBUF_VC2_SIZE) {
        rxbuf[inum].list[elem_id].next = -1;
        if (rxbuf[inum].vc2.dg.tail < 0)
            rxbuf[inum].vc2.dg.head = elem_id;
        else
            rxbuf[inum].list[rxbuf[inum].vc2.dg.tail].next = elem_id;
        rxbuf[inum].vc2.dg.tail = elem_id;
        rxbuf[inum].vc2.dg.num += 1;
        doorbell_ring(inum);
    } else
        ret = -1;
    if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
    
    return ret;
}

/* Pop datagram entry from my rxbuf */

static inline int rxbuf_vc0_pop_dg(void)
{
    /* Need doorbell.[MY_INUM].mutex locked if MY_INUM > 0 */
    int ret;
    
    ret = rxbuf[MY_INUM].vc0.dg.head;
    if (ret >= 0) {
        if (rxbuf[MY_INUM].list[ret].next < 0)
            rxbuf[MY_INUM].vc0.dg.head = rxbuf[MY_INUM].vc0.dg.tail = -1;
        else
            rxbuf[MY_INUM].vc0.dg.head = rxbuf[MY_INUM].list[ret].next;
        rxbuf[MY_INUM].vc0.dg.num -= 1;
    }
    
    return ret;
}

static inline int rxbuf_vc1_pop_dg(void)
{
    /* Need doorbell.[MY_INUM].mutex locked if MY_INUM > 0 */
    int ret;
    
    ret = rxbuf[MY_INUM].vc1.dg.head;
    if (ret >= 0) {
        if (rxbuf[MY_INUM].list[ret].next < 0)
            rxbuf[MY_INUM].vc1.dg.head = rxbuf[MY_INUM].vc1.dg.tail = -1;
        else
            rxbuf[MY_INUM].vc1.dg.head = rxbuf[MY_INUM].list[ret].next;
        rxbuf[MY_INUM].vc1.dg.num -= 1;
    }
    
    return ret;
}

static inline int rxbuf_vc2_pop_dg(void)
{
    /* Need doorbell.[MY_INUM].mutex locked */
    int ret;
    
    ret = rxbuf[MY_INUM].vc2.dg.head;
    if (ret >= 0) {
        if (rxbuf[MY_INUM].list[ret].next < 0)
            rxbuf[MY_INUM].vc2.dg.head = rxbuf[MY_INUM].vc2.dg.tail = -1;
        else
            rxbuf[MY_INUM].vc2.dg.head = rxbuf[MY_INUM].list[ret].next;
        rxbuf[MY_INUM].vc2.dg.num -= 1;
    }
    
    return ret;
}

/* Push free entry to my rxbuf */

static inline void rxbuf_push_free(int inum, int elem_id)
{
    if (inum > 0) pthread_mutex_lock(&rxbuf[inum].free.mutex);
    rxbuf[inum].list[elem_id].next = -1;
    if (rxbuf[inum].free.tail < 0)
        rxbuf[inum].free.head = elem_id;
    else
        rxbuf[inum].list[rxbuf[inum].free.tail].next = elem_id;
    rxbuf[inum].free.tail = elem_id;
    if (inum > 0) pthread_mutex_unlock(&rxbuf[inum].free.mutex);
    
    return;
}

/* Check full: my rxbuf vc1ack */

static inline int rxbuf_vc1ack_is_not_full(int inum)
{
    int ret;
    
    if (inum > 0) pthread_mutex_lock(&rxbuf[inum].free.mutex);
    ret = (rxbuf[inum].free.vc1ack.num < RXBUF_VC1ACK_SIZE) ? 1 : 0;
    if (inum > 0) pthread_mutex_unlock(&rxbuf[inum].free.mutex);
    
    return ret;
}

/* Push free entry to my rxbuf vc1ack */

static inline void rxbuf_push_free_vc1ack(int inum, int elem_id)
{
    if (inum > 0) pthread_mutex_lock(&rxbuf[inum].free.mutex);
    rxbuf[inum].list[elem_id].next = -1;
    if (rxbuf[inum].free.vc1ack.tail < 0) {
        rxbuf[inum].free.vc1ack.head = elem_id;
        rxbuf[inum].free.vc1ack.num = 1;
    } else {
        rxbuf[inum].list[rxbuf[inum].free.vc1ack.tail].next = elem_id;
        rxbuf[inum].free.vc1ack.num++;
    }
    rxbuf[inum].free.vc1ack.tail = elem_id;
    if (inum > 0) pthread_mutex_unlock(&rxbuf[inum].free.mutex);
    
    return;
}

/* Pop free entry from my rxbuf vc1ack */

static inline int rxbuf_pop_free_vc1ack(int inum)
{
    int ret;
    
    if (inum > 0) pthread_mutex_lock(&rxbuf[inum].free.mutex);
    ret = rxbuf[inum].free.vc1ack.head;
    if (ret >= 0)
        if (rxbuf[inum].list[ret].next < 0) {
            rxbuf[inum].free.vc1ack.head = rxbuf[inum].free.vc1ack.tail = -1;
            rxbuf[inum].free.vc1ack.num = 0;
        } else {
            rxbuf[inum].free.vc1ack.head = rxbuf[inum].list[ret].next;
            rxbuf[inum].free.vc1ack.num--;
        }
    if (inum > 0) pthread_mutex_unlock(&rxbuf[inum].free.mutex);
    
    return ret;
}

/***********************/
/* Interface functions */
/***********************/

static cqe_t cq[WIDTH_CQ];
static uint64_t cqwp, cqxp, cqcp;
static acp_ga_t cq_latest_src_rank, cq_latest_dst_rank;
static pthread_mutex_t mutex_cq;

/*
                .
                .
         (wait) * cqcp
         (done) :
         (wait) *
         (wait) *
        (ready) + cqxp
        (ready) |
        (ready) |
           cqwp .
                .
 [main thread]  .  [protocol thread]
*/

static void init_cq(void)
{
    cqwp = cqxp = cqcp = 1;
    cq_latest_src_rank = cq_latest_dst_rank = -1;
    pthread_mutex_init(&mutex_cq, NULL);
    return;
}

static void finalize_cq(void)
{
    return;
}

static inline int cq_open_entry(acp_ga_t dst, acp_ga_t src, acp_handle_t order)
{
    int src_rank, dst_rank, p;
    
    src_rank = ga2rank(src);
    dst_rank = ga2rank(dst);
    
    while (1) {
        pthread_mutex_lock(&mutex_cq);
        if (cqcp + WIDTH_CQ > cqwp) break;
        pthread_mutex_unlock(&mutex_cq);
        sched_yield();
    }
    p = (int)(cqwp & MASK_CQ);
    
    cq[p].order = (order == ACP_HANDLE_ALL || order == ACP_HANDLE_CONT) ? cqwp - 1 : order;
    cq[p].ptr = cqwp;
    cq[p].rank = MY_RANK;
    cq[p].src = src;
    cq[p].dst = dst;
    
    if (src_rank == MY_RANK && dst_rank == MY_RANK) {
        cq[p].stat = CQSTAT_11;
        cq[p].inum = MY_INUM;
        cq[p].gateway = MY_GATEWAY;
        cq[p].rfence = 0;
    } else {
        if (src_rank == MY_RANK) {
            cq[p].stat = CQSTAT_12;
            cq[p].inum = MY_INUM;
            cq[p].gateway = MY_GATEWAY;
        } else {
            cq[p].stat = CQSTAT_2X;
            cq[p].inum = INUM_TABLE[src_rank];
            cq[p].gateway = GTWY_TABLE[src_rank];
        }
        cq[p].rfence = (cq[p].order == cqwp - 1 && src_rank == cq_latest_src_rank && dst_rank == cq_latest_dst_rank) ? 1 : 0;
    }
    
    cq_latest_src_rank = src_rank;
    cq_latest_dst_rank = dst_rank;
    
    return p;
}

static inline acp_handle_t cq_close_entry(void)
{
    uint64_t wp = cqwp++;
    pthread_mutex_unlock(&mutex_cq);
    if (MY_INUM > 0 || NUM_PROCS == NODE_POP) {
        pthread_mutex_lock(&doorbell[MY_INUM].mutex);
        doorbell_ring(MY_INUM);
        pthread_mutex_unlock(&doorbell[MY_INUM].mutex);
    }
    return (acp_handle_t)wp;
}

static inline acp_handle_t cq_finish_entry(void)
{
    uint64_t wp = cqwp++;
    cqcp = cqxp = cqwp;
    pthread_mutex_unlock(&mutex_cq);
    return (acp_handle_t)wp;
}

void acp_complete(acp_handle_t handle)
{
    if (handle == ACP_HANDLE_NULL) return;
    while (1) {
        pthread_mutex_lock(&mutex_cq);
        if (handle == ACP_HANDLE_ALL || handle == ACP_HANDLE_CONT) handle = cqwp - 1;
        if(cqcp > handle) break;
        pthread_mutex_unlock(&mutex_cq);
        sched_yield();
    }
    pthread_mutex_unlock(&mutex_cq);
    
    return;
}

int acp_inquire(acp_handle_t handle)
{
    int ret = 0;
    
    if (handle == ACP_HANDLE_NULL) return ret;
    pthread_mutex_lock(&mutex_cq);
    if (handle == ACP_HANDLE_ALL || handle == ACP_HANDLE_CONT) handle = cqwp - 1;
    if (cqcp > handle) ret = 1;
    pthread_mutex_unlock(&mutex_cq);
    
    return ret;
}

acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, size_t size, acp_handle_t order)
{
    debug printf("rank %d - main acp_copy(0x%016" PRIx64 ",  0x%016" PRIx64 ", %d, 0x%016" PRIx64 ");\n", MY_RANK, dst, src, size, order);
    int p = cq_open_entry(dst, src, order);
    cq[p].type = COPY;
    cq[p].size = size;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        memcpy(ga2address(cq[p].dst), ga2address(cq[p].src), cq[p].size);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_cas4(acp_ga_t dst, acp_ga_t src, uint32_t oldval, uint32_t newval, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = CAS4;
    cq[p].old4 = oldval;
    cq[p].new4 = newval;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint32_t*)ga2address(cq[p].dst) = sync_val_compare_and_swap_4((uint32_t*)ga2address(cq[p].src), cq[p].old4, cq[p].new4);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_cas8(acp_ga_t dst, acp_ga_t src, uint64_t oldval, uint64_t newval, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = CAS8;
    cq[p].old8 = oldval;
    cq[p].new8 = newval;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint64_t*)ga2address(cq[p].dst) = sync_val_compare_and_swap_8((uint64_t*)ga2address(cq[p].src), cq[p].old8, cq[p].new8);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_swap4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = SWAP4;
    cq[p].val4 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint32_t*)ga2address(cq[p].dst) = sync_swap_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_swap8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = SWAP8;
    cq[p].val8 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint64_t*)ga2address(cq[p].dst) = sync_swap_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_add4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = ADD4;
    cq[p].val4 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_add_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_add8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = ADD8;
    cq[p].val8 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_add_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_xor4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = XOR4;
    cq[p].val4 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_xor_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_xor8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = XOR8;
    cq[p].val8 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_xor_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_or4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = OR4;
    cq[p].val4 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_or_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_or8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = OR8;
    cq[p].val8 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_or_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_and4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = AND4;
    cq[p].val4 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_and_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

acp_handle_t acp_and8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{
    int p = cq_open_entry(dst, src, order);
    cq[p].type = AND8;
    cq[p].val8 = value;
    if (cq[p].stat == CQSTAT_11 && cq[p].order < cqcp) {
        debug printf("rank %d - main Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
        *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_and_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
        cq[p].stat = CQSTAT_DONE;
        if (cqxp == cqwp && cqcp == cqwp) return cq_finish_entry();
    }
    return cq_close_entry();
}

/************************/
/* Communication thread */
/************************/

/* Delegate queue */

static cqe_t dq[WIDTH_DQ];
static int dqnext[WIDTH_DQ];
static int dqprev[WIDTH_DQ];
static int dqhead, dqexec, dqtail;
static uint64_t dqoffset;
static uint64_t dqwait[WIDTH_DQ];
static int dqfreelist[WIDTH_DQ];
static int dqflhead, dqflnum;

/*
  (wait/notify) * dqhead
  (wait/notify) *
                :
  (wait/notify) *
                :
  (wait/notify) *
       (active) + dqexec
       (active) |
       (active) | dqtail
*/

static inline void init_dq(void)
{
    int i;
    
    debug printf("rank %d - dq reset\n", MY_RANK);
    dqhead = dqexec = dqtail = -1;
    dqoffset = 0;
    for (i = 0; i < WIDTH_DQ; i++) dqfreelist[i] = i;
    dqflhead = 0;
    dqflnum = WIDTH_DQ;
    
    return;
}

static inline int is_dq_full(void)
{
    return (dqflnum == 0) ? 1 : 0;
}

static inline int is_dq_not_full(void)
{
    return (dqflnum > 0) ? 1 : 0;
}

static inline int is_dq_empty(void)
{
    return (dqflnum == WIDTH_DQ) ? 1 : 0;
}

static inline int is_dq_not_empty(void)
{
    return (dqflnum < WIDTH_DQ) ? 1 : 0;
}

static inline int dq_push(uint64_t ptr, uint32_t rank, uint32_t rfence, uint16_t type, acp_ga_t dst, acp_ga_t src)
{
    int dst_rank, pos;
    
    pos = dqfreelist[dqflhead];
    dqflhead = (dqflhead + 1) & MASK_DQ;
    dqflnum--;
    dqnext[pos] = -1;
    dqprev[pos] = dqtail;
    dq[pos].stat = rfence ? DQSTAT_FENCE : DQSTAT_ACTIVE;
    if (dqtail < 0) {
        dqhead = dqexec = dqtail = pos;
        dqoffset = 0;
    } else {
        dqnext[dqtail] = dqtail = pos;
        if (dqexec < 0) {
            dqexec = pos;
            dqoffset = 0;
        }
    }
    
    dst_rank = ga2rank(dst);
    dq[pos].inum = INUM_TABLE[dst_rank];
    dq[pos].gateway = GTWY_TABLE[dst_rank];
    dq[pos].ptr = ptr;
    dq[pos].rank = rank;
    dq[pos].rfence = rfence;
    dq[pos].type = type;
    dq[pos].dst = dst;
    dq[pos].src = src;
    
    return pos;
}

static inline void dq_free(int pos)
{
    if (pos < 0) return;
    if (dqexec == pos) {
        dqexec = dqnext[pos];
        dqoffset = 0;
    }
    if (dqhead == pos && dqtail == pos) {
        dqhead = dqexec = dqtail = -1;
        dqoffset = 0;
    }else if (dqhead == pos) {
        dqhead = dqnext[pos];
        dqprev[dqhead] = -1;
    } else if (dqtail == pos) {
        dqtail = dqprev[pos];
        dqnext[dqtail] = -1;
    } else {
        dqnext[dqprev[pos]] = dqnext[pos];
        dqprev[dqnext[pos]] = dqprev[pos];
    }
    dqfreelist[(dqflhead + dqflnum) & MASK_DQ] = pos;
    dqflnum++;
    
    return;
}

/* Datagram size utilities */

static inline int dg_size_vc0(uint32_t type)
{
    if (type == CAS4 || type == SWAP4 || type == ADD4 || type == XOR4 || type == OR4 || type == AND4 ) return 44;
    if (type == CAS8 ) return 56;
    return 48;
}

static inline int dg_biased_size(int len)
{
    return ((len < 18) ? 18 : len) + DATAGRAM_BIAS;
}

/* Serial number */

ser_entry_t* ser_table;

static inline void init_ser(void)
{
    int i;
    
    ser_table = (ser_entry_t*)malloc(sizeof(ser_entry_t) * 3 * NODE_POP);
    
    for (i = 0; i < 3 * NODE_POP; i++) {
        ser_table[i].last = 0;
        ser_table[i].next = 0;
    }
    
    return;
}

static inline void finalize_ser(void)
{
    if (ser_table != NULL) free(ser_table);
    return;
}

static inline uint16_t inc_ser(int inum, int vc)
{
    uint16_t next;
    int pos;
    
    pos = inum * 3 + vc;
    next = ser_table[pos].next;
    ser_table[pos].next = (next < 0xffff) ? next + 1 : 0;
    if (ser_table[pos].last == next) ser_table[pos].last = (next < 0xffff) ? next + 1 : 0;
    
    return next;
}

static inline int update_ser(int inum, int vc, uint16_t ser)
{
    uint16_t last, next;
    int pos, ret;
    
    pos = inum * 3 + vc;
    next = ser_table[pos].next;
    last = ser_table[pos].last;
    ret = 0;
    if (last != next + 1 && (last != 0 || next != 0xffff)) {
        if (last <= next && (last <= ser && ser < next)) ret = 1;
        if (next < last && (ser < next || last <= ser)) ret = 1;
    }
    ser_table[pos].last = (ser < 0xffff) ? ser + 1 : 0;
    
    return ret;
}

/* Transmit time table */

static uint64_t* txtime_table;

static inline void init_txtime(uint64_t time)
{
    int i;
    
    txtime_table = (uint64_t*)malloc(sizeof(uint64_t) * 0x10000 * 3 * NODE_POP);
    for (i = 0; i < 0x10000 * 3 * NODE_POP; i++) txtime_table[i] = time;
    
    return;
}

static inline void finalize_txtime(void)
{
    if (txtime_table != NULL) free(txtime_table);
    return;
}

static inline uint64_t txtime(int inum, int vc, int ser)
{
    return txtime_table[((inum * 3 + vc) << 16) | ser];
}

static inline void set_txtime(int inum, int vc, int ser, uint64_t time)
{
    txtime_table[((inum * 3 + vc) << 16) | ser] = time;
    return;
}

/* Round trip time prediction */

static rtt_pred_entry_t* rtt_pred_table;

static inline void init_rtt_pred(void)
{
    int i;
    
    rtt_pred_table = (rtt_pred_entry_t*)malloc(sizeof(rtt_pred_entry_t) * 3 * NUM_PROCS);
    
    for (i = 0; i < 3 * NUM_PROCS; i++) {
        rtt_pred_table[i].sa = NETWORK_RTT * 1000ULL;
        rtt_pred_table[i].sv = NETWORK_RTT * 1000ULL;
    }
    return;
}

static inline void finalize_rtt_pred(void)
{
    if (rtt_pred_table != NULL) free(rtt_pred_table);
    return;
}

static inline uint64_t rtt_pred(int rank, int vc)
{
    int64_t sa, sv;
    uint64_t max_rtt, rtt;
    int pos;
    
    pos = rank * 3 + vc;
    sa = rtt_pred_table[pos].sa;
    sv = rtt_pred_table[pos].sv;
    
    max_rtt = MAX_NETWORK_RTT * 1000ULL;
    rtt = sa + (sv << 2);
    if (rtt > max_rtt) return max_rtt;
    
    return rtt;
}

static inline void rtt_update(int rank, int vc, int64_t rtt)
{
    int64_t delta, sa, sv;
    int pos;
    
    pos = rank * 3 + vc;
    sa = rtt_pred_table[pos].sa;
    sv = rtt_pred_table[pos].sv;
    
    delta = (rtt > sa) ? rtt - sa : sa - rtt;
    
    sa = ((sa + rtt >> 1) + sa >> 1) + sa >> 1;
    sv = (sv + delta >> 1) + sv >> 1;
    
    rtt_pred_table[pos].sa = sa;
    rtt_pred_table[pos].sv = sv;
    
    return;
}

/* Retransmit list */

static retx_list_entry_t* retx_list;
static int retx_head, retx_tail;

static inline void init_retx_list(void)
{
    int num, i;
    
    num = (TXBUF_VC0_SIZE + TXBUF_VC1_SIZE + TXBUF_VC2_SIZE) * NODE_POP;
    retx_list = (retx_list_entry_t*)malloc(sizeof(retx_list_entry_t) * num);
    for (i = 0; i < num; i++) retx_list[i].time = 0;
    retx_head = retx_tail = -1;
    return;
}

static inline void finalize_retx_list(void)
{
    if (retx_list != NULL) free(retx_list);
    return;
}

static inline int retx_list_pos(int inum, int vc, int elem_id)
{
    if (vc == 0) return (TXBUF_VC0_SIZE + TXBUF_VC1_SIZE + TXBUF_VC2_SIZE) * inum + elem_id;
    if (vc == 1) return (TXBUF_VC0_SIZE + TXBUF_VC1_SIZE + TXBUF_VC2_SIZE) * inum + TXBUF_VC0_SIZE + elem_id;
    return (TXBUF_VC0_SIZE + TXBUF_VC1_SIZE + TXBUF_VC2_SIZE) * inum + TXBUF_VC0_SIZE + TXBUF_VC1_SIZE + elem_id;
}

static inline int retx_list_pos_inum(int pos)
{
    return pos / (TXBUF_VC0_SIZE + TXBUF_VC1_SIZE + TXBUF_VC2_SIZE);
}

static inline int retx_list_pos_vc(int pos)
{
    int i = pos % (TXBUF_VC0_SIZE + TXBUF_VC1_SIZE + TXBUF_VC2_SIZE);
    if (i < TXBUF_VC0_SIZE) return 0;
    if (i < TXBUF_VC0_SIZE + TXBUF_VC1_SIZE) return 1;
    return 2;
}

static inline int retx_list_pos_elem_id(int pos)
{
    int i = pos % (TXBUF_VC0_SIZE + TXBUF_VC1_SIZE + TXBUF_VC2_SIZE);
    if (i < TXBUF_VC0_SIZE) return i;
    if (i < TXBUF_VC0_SIZE + TXBUF_VC1_SIZE) return i - TXBUF_VC0_SIZE;
    return i - TXBUF_VC0_SIZE - TXBUF_VC1_SIZE;
}

static inline void delete_retx_entry(int pos)
{
    int next, prev;
    
    if (retx_list[pos].time == 0) return;
    
    next = retx_list[pos].next;
    prev = retx_list[pos].prev;
    
    if (prev == -1)
        retx_head = next;
    else
        retx_list[prev].next = next;
    if (next == -1)
        retx_tail = prev;
    else
        retx_list[next].prev = prev;
    
    retx_list[pos].time = 0;
    
    return;
}

static inline void insert_retx_time(int pos, uint64_t time)
{
    int ptr = retx_tail;
    int next = -1;
    
    if (retx_list[pos].time) delete_retx_entry(pos);
    retx_list[pos].time = time ? time : 1;
    
    while (ptr >= 0) {
        if (retx_list[ptr].time <= time) break;
        next = ptr;
        ptr = retx_list[ptr].prev;
    }
    
    retx_list[pos].next = next;
    retx_list[pos].prev = ptr;
    if (ptr >= 0)
        retx_list[ptr].next = pos;
    else
        retx_head = pos;
    if (next >= 0)
        retx_list[next].prev = pos;
    else
        retx_tail = pos;
    return;
}

/* Communication thread function */

static void* comm_thread_func(void *param)
{
    struct pollfd* pfds;
    struct sockaddr_in addr, from;
    socklen_t addr_len;
    ssize_t recv_len;
    dg_union* dgp;
    dg_control_t dgc;
    uint32_t send_to;
    uint64_t estimated_nsec = 0, current_nsec, tmp_nsec, count, size;
    int i, j, check, check_clear, check_cont, check_not_full, check_quit, check_wait;
    int elem_id, inum, len, next, p, pos, prev, ptr, sock, tx_bytes, type, vc;
    int tx_vc0_next_inum = 0, tx_vc1_next_inum = 0, tx_vc2_next_inum = 0, rx_vc0_next_inum = 0, rx_vc1_next_inum = 0;
    
    /******** Initinalization for the transport processing ********/
    
    if (MY_INUM == 0 && NUM_PROCS != NODE_POP) {
        debug printf("rank %d - initinalization for the transport processing\n", MY_RANK);
        init_ser();
        init_seq();
        init_txtime(get_nsec());
        init_rtt_pred();
        init_retx_list();
        
        /*** Setup pollfds for UDP communication ***/
        
        pfds = (struct pollfd*)malloc(sizeof(struct pollfd) * NODE_POP);
        if (pfds == NULL) {
            pthread_mutex_lock(&mutex_comm_thread_ready);
            pthread_cond_signal(&cond_comm_thread_ready);
            pthread_mutex_unlock(&mutex_comm_thread_ready);
            finalize_retx_list();
            finalize_rtt_pred();
            finalize_txtime();
            finalize_seq();
            finalize_ser();
            return NULL;
        }
        
        addr_len = sizeof(struct sockaddr_in);
        for (i = 0; i < NODE_POP; i++) {
            /* bind socket */
            pfds[i].fd = socket(AF_INET, SOCK_DGRAM, 0);
            addr.sin_family = AF_INET;
            addr.sin_port = PORT_TABLE[LMEM_TABLE[i]];
            addr.sin_addr.s_addr = INADDR_ANY;
            if (bind(pfds[i].fd, (struct sockaddr *)&addr, addr_len)) {
                for (j = i - 1; j >= 0; j--) close(pfds[j].fd);
                pthread_mutex_lock(&mutex_comm_thread_ready);
                pthread_cond_signal(&cond_comm_thread_ready);
                pthread_mutex_unlock(&mutex_comm_thread_ready);
                finalize_retx_list();
                finalize_rtt_pred();
                finalize_txtime();
                finalize_seq();
                finalize_ser();
                return NULL;
            }
            /* set events */
            pfds[i].events = POLLIN;
            pfds[i].revents = 0;
        }
    }
    
    /*** Main loop ***/
    pthread_mutex_lock(&mutex_comm_thread_ready);
    pthread_cond_signal(&cond_comm_thread_ready);
    pthread_mutex_unlock(&mutex_comm_thread_ready);
    debug printf("rank %d - communication thread ready\n", MY_RANK);
    
    pthread_mutex_lock(&mutex_comm_thread_start);
    pthread_cond_wait(&cond_comm_thread_start, &mutex_comm_thread_start);
    pthread_mutex_unlock(&mutex_comm_thread_start);
    debug printf("rank %d - communication thread start\n", MY_RANK);
    
    while (1) {
        /*** Quit check ***/
        
        pthread_mutex_lock(&mutex_quit_comm_thread);
        check_quit = quit_comm_thread;
        pthread_mutex_unlock(&mutex_quit_comm_thread);
        if (check_quit) break;
        
        /******** Transport processing ********/
        
        if (MY_INUM == 0 && NUM_PROCS != NODE_POP) {
            
            /*** Sweep rxbuf vc1 ack ***/
            current_nsec = get_nsec();
            tx_bytes = 0;
            
            for (inum = 0; inum < NODE_POP; inum++) {
                while((elem_id = rxbuf_pop_free_vc1ack(inum)) >= 0) {
                    dgp = (dg_union*)rxbuf[inum].list[elem_id].dg;
                    send_to = dgp->put.rank;
                    pos = inum * NUM_PROCS + send_to;
                    sock = pfds[inum].fd;
                    dgc.task = TASKID;
                    dgc.c = ACK;
                    dgc.vc = 1;
                    dgc.rank = LMEM_TABLE[inum];
                    dgc.ser = dgp->put.ser;
                    inc_seq(&seq_table[pos].rxseq1);
                    dgc.seq = seq_table[pos].rxseq0;
                    dgc.seq1 = seq_table[pos].rxseq1;
                    dgc.seq2 = seq_table[pos].rxseq2;
                    len = 16;
                    addr.sin_family = AF_INET;
                    addr.sin_port = PORT_TABLE[send_to];
                    addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                    tx_bytes += dg_biased_size(len);
                    rxbuf_push_free(inum, elem_id);
                    sendto(sock, (void*)&dgc, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                    debug printf("rank %d - transport Transmit control %d to = %d, vc = %d, ser = 0x%04x, seq0 = 0x%04x, seq1 = 0x%04x, seq2 = 0x%04x\n", MY_RANK, dgc.c, send_to, dgc.vc, dgc.ser, dgc.seq, dgc.seq1, dgc.seq2);
                }
            }
            
            /*** Recieve datagram ***/
            
            poll(pfds, NODE_POP, 0);
            for (inum = 0; inum < NODE_POP; inum++) {
                if ((pfds[inum].revents & POLLIN) == 0) continue;
                elem_id = rxbuf_pop_free(inum);
                dgp = (dg_union*)rxbuf[inum].list[elem_id].dg;
                recv_len = recvfrom(pfds[inum].fd, (void*)dgp, MAX_DG_SIZE, 0, (struct sockaddr*)&from, &addr_len);
                if (dgp->ack.task != TASKID) {
                    rxbuf_push_free(inum, elem_id);
                    continue;
                    
                } else if (dgp->ack.c != NORMAL) {
                    debug printf("rank %d - transport Receive control %d from = %d, vc = %d, ser = 0x%04x, seq0 = 0x%04x, seq1 = 0x%04x, seq2 = 0x%04x\n", MY_RANK, dgp->ack.c, dgp->ack.rank, dgp->ack.vc, dgp->ack.ser, dgp->ack.seq, dgp->ack.seq1, dgp->ack.seq2);
                    /* Check txbuf vc2 wait list */
                    prev = -1;
                    ptr = txbuf[inum].vc2.wait.head;
                    while (ptr >= 0) {
                        next = txbuf[inum].vc2.list[ptr].next;
                        if (txbuf[inum].vc2.list[ptr].send_to == dgp->ack.rank && compare_seq(((dg_control_t*)&txbuf[inum].vc2.list[ptr].dg)->seq, dgp->ack.seq2) < 0) {
                            if (prev == -1) {
                                txbuf[inum].vc2.wait.head = next;
                                if (next == -1) txbuf[inum].vc2.wait.tail = -1;
                            } else {
                                txbuf[inum].vc2.list[prev].next = next;
                                if (next == -1) txbuf[inum].vc2.wait.tail = prev;
                            }
                            delete_retx_entry(retx_list_pos(inum, 2, ptr));
                            txbuf_vc2_push_free(inum, ptr);
                        } else
                            prev = ptr;
                        ptr = next;
                    }
                    
                    /* Check txbuf vc1 wait list */
                    prev = -1;
                    ptr = txbuf[inum].vc1.wait.head;
                    while (ptr >= 0) {
                        next = txbuf[inum].vc1.list[ptr].next;
                        if (txbuf[inum].vc1.list[ptr].send_to == dgp->ack.rank && compare_seq(((dg_control_t*)&txbuf[inum].vc1.list[ptr].dg)->seq, dgp->ack.seq1) < 0) {
                            if (prev == -1) {
                                txbuf[inum].vc1.wait.head = next;
                                if (next == -1) txbuf[inum].vc1.wait.tail = -1;
                            } else {
                                txbuf[inum].vc1.list[prev].next = next;
                                if (next == -1) txbuf[inum].vc1.wait.tail = prev;
                            }
                            delete_retx_entry(retx_list_pos(inum, 1, ptr));
                            txbuf_vc1_push_ack(inum, ptr);
                        } else
                            prev = ptr;
                        ptr = next;
                    }
                    
                    /* Check txbuf vc0 wait list */
                    prev = -1;
                    ptr = txbuf[inum].vc0.wait.head;
                    while (ptr >= 0) {
                        next = txbuf[inum].vc0.list[ptr].next;
                        if (txbuf[inum].vc0.list[ptr].send_to == dgp->ack.rank && compare_seq(((dg_control_t*)&txbuf[inum].vc0.list[ptr].dg)->seq, dgp->ack.seq) < 0) {
                            if (prev == -1) {
                                txbuf[inum].vc0.wait.head = next;
                                if (next == -1) txbuf[inum].vc0.wait.tail = -1;
                            } else {
                                txbuf[inum].vc0.list[prev].next = next;
                                if (next == -1) txbuf[inum].vc0.wait.tail = prev;
                            }
                            delete_retx_entry(retx_list_pos(inum, 0, ptr));
                            txbuf_vc0_push_free(inum, ptr);
                        } else
                            prev = ptr;
                        ptr = next;
                    }
                    
                    /* Update full flag */
                    if (dgp->ack.c == ACK) {
                        if (dgp->ack.vc == 0) seq_table[NUM_PROCS * inum + dgp->ack.rank].full0 = 0;
                        if (dgp->ack.vc == 1) seq_table[NUM_PROCS * inum + dgp->ack.rank].full1 = 0;
                        if (dgp->ack.vc == 2) seq_table[NUM_PROCS * inum + dgp->ack.rank].full2 = 0;
                    } else if (dgp->full.c == FULL) {
                        if (dgp->full.vc == 0) seq_table[NUM_PROCS * inum + dgp->full.rank].full0 = 1;
                        if (dgp->full.vc == 1) seq_table[NUM_PROCS * inum + dgp->full.rank].full1 = 1;
                        if (dgp->full.vc == 2) seq_table[NUM_PROCS * inum + dgp->full.rank].full2 = 1;
                    }
                    
                    /* Update round trip time */
                    if (update_ser(inum, dgp->ack.vc, dgp->ack.ser)) rtt_update(dgp->ack.rank, dgp->ack.vc, get_nsec() - txtime(inum, dgp->ack.vc, dgp->ack.ser));
                    rxbuf_push_free(inum, elem_id);
                    continue;
                    
                } else if (dgp->end.vc == 2) {
                    debug printf("rank %d - transport Receive END from = %d, ser = 0x%04x, seq = 0x%04x, cqp = 0x%016" PRIx64 "\n", MY_RANK, dgp->end.rank, dgp->end.ser, dgp->end.seq, dgp->end.ptr);
                    send_to = dgp->end.rank;
                    pos = inum * NUM_PROCS + send_to;
                    sock = pfds[inum].fd;
                    dgc.task = TASKID;
                    dgc.c = ACK;
                    dgc.vc = 2;
                    dgc.rank = LMEM_TABLE[inum];
                    dgc.ser = dgp->end.ser;
                    dgc.seq = seq_table[pos].rxseq0;
                    dgc.seq1 = seq_table[pos].rxseq1;
                    dgc.seq2 = seq_table[pos].rxseq2;
                    len = 16;
                    addr.sin_family = AF_INET;
                    addr.sin_port = PORT_TABLE[send_to];
                    addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                    tx_bytes += dg_biased_size(len);
                    if (dgp->end.seq == dgc.seq2) {
                        if (inum > 0) pthread_mutex_lock(&doorbell[inum].mutex);
                        if (rxbuf[inum].vc2.dg.num > (RXBUF_VC2_SIZE >> 1)) dgc.c = FULL;
                        if (rxbuf[inum].vc2.dg.num < RXBUF_VC2_SIZE) {
                            rxbuf[inum].list[elem_id].next = -1;
                            if (rxbuf[inum].vc2.dg.tail < 0)
                                rxbuf[inum].vc2.dg.head = elem_id;
                            else
                                rxbuf[inum].list[rxbuf[inum].vc2.dg.tail].next = elem_id;
                            rxbuf[inum].vc2.dg.tail = elem_id;
                            rxbuf[inum].vc2.dg.num += 1;
                            doorbell_ring(inum);
                            if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
                            inc_seq(&seq_table[pos].rxseq2);
                            dgc.seq2 = seq_table[pos].rxseq2;
                            sendto(sock, (void*)&dgc, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                            debug printf("rank %d - transport Transmit control %d to = %d, vc = %d, ser = 0x%04x, seq0 = 0x%04x, seq1 = 0x%04x, seq2 = 0x%04x\n", MY_RANK, dgc.c, send_to, dgc.vc, dgc.ser, dgc.seq, dgc.seq1, dgc.seq2);
                            continue;
                        }
                        if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
                    }
                    dgc.c = NACK;
                    rxbuf_push_free(inum, elem_id);
                    sendto(sock, (void*)&dgc, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                    debug printf("rank %d - transport Transmit control %d to = %d, vc = %d, ser = 0x%04x, seq0 = 0x%04x, seq1 = 0x%04x, seq2 = 0x%04x\n", MY_RANK, dgc.c, send_to, dgc.vc, dgc.ser, dgc.seq, dgc.seq1, dgc.seq2);
                    continue;
                    
                } else if (dgp->put.vc == 1) {
                    debug printf("rank %d - transport Receive PUT from = %d, ser = 0x%04x, seq = 0x%04x, dst = 0x%016" PRIx64 ", len = %d\n", MY_RANK, dgp->put.rank, dgp->put.ser, dgp->put.seq, dgp->put.dst, dgp->put.len);
                    send_to = dgp->put.rank;
                    pos = inum * NUM_PROCS + send_to;
                    sock = pfds[inum].fd;
                    dgc.task = TASKID;
                    dgc.c = ACK;
                    dgc.vc = 1;
                    dgc.rank = LMEM_TABLE[inum];
                    dgc.ser = dgp->put.ser;
                    dgc.seq = seq_table[pos].rxseq0;
                    dgc.seq1 = seq_table[pos].rxseq1;
                    dgc.seq2 = seq_table[pos].rxseq2;
                    len = 16;
                    addr.sin_family = AF_INET;
                    addr.sin_port = PORT_TABLE[send_to];
                    addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                    tx_bytes += dg_biased_size(len);
                    if (dgp->put.seq == seq_table[pos].rxseq1fwd) {
                        if (inum > 0) pthread_mutex_lock(&doorbell[inum].mutex);
                        if (rxbuf[inum].vc1.dg.num > (RXBUF_VC1_SIZE >> 1)) dgc.c = FULL;
                        if (rxbuf[inum].vc1.dg.num < RXBUF_VC1_SIZE) {
                            rxbuf[inum].list[elem_id].next = -1;
                            if (rxbuf[inum].vc1.dg.tail < 0)
                                rxbuf[inum].vc1.dg.head = elem_id;
                            else
                                rxbuf[inum].list[rxbuf[inum].vc1.dg.tail].next = elem_id;
                            rxbuf[inum].vc1.dg.tail = elem_id;
                            rxbuf[inum].vc1.dg.num += 1;
                            doorbell_ring(inum);
                            if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
                            inc_seq(&seq_table[pos].rxseq1fwd);
                            debug printf("rank %d - transport Transmit control %d to = %d, vc = %d, ser = 0x%04x, seq0 = 0x%04x, seq1 = 0x%04x, seq2 = 0x%04x\n", MY_RANK, dgc.c, send_to, dgc.vc, dgc.ser, dgc.seq, dgc.seq1, dgc.seq2);
                            continue;
                        }
                        if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
                    }
                    dgc.c = NACK;
                    rxbuf_push_free(inum, elem_id);
                    sendto(sock, (void*)&dgc, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                    debug printf("rank %d - transport Transmit control %d to = %d, vc = %d, ser = 0x%04x, seq0 = 0x%04x, seq1 = 0x%04x, seq2 = 0x%04x\n", MY_RANK, dgc.c, send_to, dgc.vc, dgc.ser, dgc.seq, dgc.seq1, dgc.seq2);
                    continue;
                    
                } else if (dgp->copy.vc == 0) {
                    debug printf("rank %d - transport Receive COMMAND %d from = %d, ser = 0x%04x, seq = 0x%04x, ptr = 0x%016" PRIx64 ", s = %d, dst = 0x%016" PRIx64 ", src = 0x%016" PRIx64 "\n", MY_RANK, dgp->copy.type, dgp->copy.rank, dgp->copy.ser, dgp->copy.seq, dgp->copy.ptr, dgp->copy.s, dgp->copy.dst, dgp->copy.src);
                    send_to = dgp->copy.rank;
                    pos = inum * NUM_PROCS + send_to;
                    sock = pfds[inum].fd;
                    dgc.task = TASKID;
                    dgc.c = ACK;
                    dgc.vc = 0;
                    dgc.rank = LMEM_TABLE[inum];
                    dgc.ser = dgp->copy.ser;
                    dgc.seq = seq_table[pos].rxseq0;
                    dgc.seq1 = seq_table[pos].rxseq1;
                    dgc.seq2 = seq_table[pos].rxseq2;
                    len = 16;
                    addr.sin_family = AF_INET;
                    addr.sin_port = PORT_TABLE[send_to];
                    addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                    tx_bytes += dg_biased_size(len);
                    if (dgp->copy.seq == dgc.seq) {
                        if (inum > 0) pthread_mutex_lock(&doorbell[inum].mutex);
                        if (rxbuf[inum].vc0.dg.num > (RXBUF_VC0_SIZE >> 1)) dgc.c = FULL;
                        if (rxbuf[inum].vc0.dg.num < RXBUF_VC0_SIZE) {
                            rxbuf[inum].list[elem_id].next = -1;
                            if (rxbuf[inum].vc0.dg.tail < 0)
                                rxbuf[inum].vc0.dg.head = elem_id;
                            else
                                rxbuf[inum].list[rxbuf[inum].vc0.dg.tail].next = elem_id;
                            rxbuf[inum].vc0.dg.tail = elem_id;
                            rxbuf[inum].vc0.dg.num += 1;
                            doorbell_ring(inum);
                            if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
                            inc_seq(&seq_table[pos].rxseq0);
                            dgc.seq = seq_table[pos].rxseq0;
                            sendto(sock, (void*)&dgc, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                            debug printf("rank %d - transport Transmit control %d to = %d, vc = %d, ser = 0x%04x, seq0 = 0x%04x, seq1 = 0x%04x, seq2 = 0x%04x\n", MY_RANK, dgc.c, send_to, dgc.vc, dgc.ser, dgc.seq, dgc.seq1, dgc.seq2);
                            continue;
                        }
                        if (inum > 0) pthread_mutex_unlock(&doorbell[inum].mutex);
                    }
                    dgc.c = NACK;
                    rxbuf_push_free(inum, elem_id);
                    sendto(sock, (void*)&dgc, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                    debug printf("rank %d - transport Transmit control %d to = %d, vc = %d, ser = 0x%04x, seq0 = 0x%04x, seq1 = 0x%04x, seq2 = 0x%04x\n", MY_RANK, dgc.c, send_to, dgc.vc, dgc.ser, dgc.seq, dgc.seq1, dgc.seq2);
                    continue;
                }
            }
            
            /*** Check injection rate ***/
            if (estimated_nsec > current_nsec) {
                estimated_nsec += tx_bytes * 8000ULL / NETWORK_BANDWIDTH;
                continue;
            }
            
            /*** Retransmit datagram ***/
            
            pos = retx_head;
            check = 0;
            while (0) {//while (pos >= 0) {
                if (retx_list[pos].time > current_nsec) break;
                next = retx_list[pos].next;
                inum = retx_list_pos_inum(pos);
                vc = retx_list_pos_inum(pos);
                elem_id = retx_list_pos_elem_id(pos);
                if (vc == 2) {
                    if ((check & 4) == 0) {
                        sock = pfds[inum].fd;
                        dgp = (dg_union*)txbuf[inum].vc2.list[elem_id].dg;
                        len = 20;
                        send_to = txbuf[inum].vc2.list[elem_id].send_to;
                        dgp->end.ser = inc_ser(inum, 2);
                        struct sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_port = PORT_TABLE[send_to];
                        addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                        sendto(sock, (void*)dgp, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                        tx_bytes += dg_biased_size(len);
                        tmp_nsec = get_nsec();
                        delete_retx_entry(pos);
                        insert_retx_time(pos, tmp_nsec + rtt_pred(send_to, 2));
                        set_txtime(inum, 2, dgp->end.ser, tmp_nsec);
                        check |= 4;
                    }
                } else if (vc == 1) {
                    if ((check & 2) == 0) {
                        sock = pfds[inum].fd;
                        dgp = (dg_union*)txbuf[inum].vc1.list[elem_id].dg;
                        len = 24 + dgp->put.len;
                        send_to = txbuf[inum].vc1.list[elem_id].send_to;
                        dgp->put.ser = inc_ser(inum, 1);
                        addr.sin_family = AF_INET;
                        addr.sin_port = PORT_TABLE[send_to];
                        addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                        sendto(sock, (void*)dgp, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                        tx_bytes += dg_biased_size(len);
                        tmp_nsec = get_nsec();
                        delete_retx_entry(pos);
                        insert_retx_time(pos, tmp_nsec + rtt_pred(send_to, 1));
                        set_txtime(inum, 1, dgp->put.ser, tmp_nsec);
                        check |= 2;
                    }
                } else { /* vc == 0 */
                    if ((check & 1) == 0) {
                        sock = pfds[inum].fd;
                        dgp = (dg_union*)txbuf[inum].vc0.list[elem_id].dg;
                        len = dg_size_vc0(dgp->copy.type);
                        send_to = txbuf[inum].vc0.list[elem_id].send_to;
                        dgp->copy.ser = inc_ser(inum, 0);
                        addr.sin_family = AF_INET;
                        addr.sin_port = PORT_TABLE[send_to];
                        addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                        sendto(sock, (void*)dgp, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                        tx_bytes += dg_biased_size(len);
                        tmp_nsec = get_nsec();
                        delete_retx_entry(pos);
                        insert_retx_time(pos, tmp_nsec + rtt_pred(send_to, 0));
                        set_txtime(inum, 0, dgp->copy.ser, tmp_nsec);
                        check |= 1;
                    }
                }
                if (check == 7) break;
                pos = next;
            }
            
            /*** Check injection rate ***/
            if (estimated_nsec > current_nsec) {
                estimated_nsec += tx_bytes * 8000ULL / NETWORK_BANDWIDTH;
                continue;
            }
            
            /*** Transmit datagram ***/
            
            /* VC2 */
            for (i = 0; i < NODE_POP; i++) {
                inum = tx_vc2_next_inum;
                tx_vc2_next_inum = (tx_vc2_next_inum < NODE_POP - 1) ? tx_vc2_next_inum + 1 : 0;
                elem_id = txbuf_vc2_pop_dg(inum);
                if (elem_id >= 0) {
                    sock = pfds[inum].fd;
                    dgp = (dg_union*)txbuf[inum].vc2.list[elem_id].dg;
                    len = 20;
                    send_to = txbuf[inum].vc2.list[elem_id].send_to;
                    dgp->end.ser = inc_ser(inum, 2);
                    dgp->end.seq = inc_seq(&seq_table[inum * NUM_PROCS + send_to].txseq2);
                    addr.sin_family = AF_INET;
                    addr.sin_port = PORT_TABLE[send_to];
                    addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                    debug printf("rank %d - transport Transmit END from = %d, to = %d, ser = 0x%04x, seq = 0x%04x, cqp = 0x%016" PRIx64 "\n", MY_RANK, dgp->end.rank, send_to, dgp->end.ser, dgp->end.seq, dgp->end.ptr);
                    sendto(sock, (void*)dgp, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                    tx_bytes += dg_biased_size(len);
                    txbuf_vc2_push_wait(inum, elem_id);
                    tmp_nsec = get_nsec();
                    set_txtime(inum, 2, dgp->end.ser, tmp_nsec);
                    insert_retx_time(retx_list_pos(inum, 2, elem_id), tmp_nsec + rtt_pred(send_to, 2));
                    break;
                }
            }
            
            /* VC1 */
            for (i = 0; i < NODE_POP; i++) {
                inum = tx_vc1_next_inum;
                tx_vc1_next_inum = (tx_vc1_next_inum < NODE_POP - 1) ? tx_vc1_next_inum + 1 : 0;
                elem_id = txbuf_vc1_pop_dg(inum);
                if (elem_id >= 0) {
                    sock = pfds[inum].fd;
                    dgp = (dg_union*)txbuf[inum].vc1.list[elem_id].dg;
                    len = 24 + dgp->put.len;
                    send_to = txbuf[inum].vc1.list[elem_id].send_to;
                    dgp->put.ser = inc_ser(inum, 1);
                    dgp->put.seq = inc_seq(&seq_table[inum * NUM_PROCS + send_to].txseq1);
                    addr.sin_family = AF_INET;
                    addr.sin_port = PORT_TABLE[send_to];
                    addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                    debug printf("rank %d - transport Transmit PUT from = %d, to = %d, ser = 0x%04x, seq = 0x%04x, dst = 0x%016" PRIx64 ", len = %d\n", MY_RANK, dgp->put.rank, send_to, dgp->put.ser, dgp->put.seq, dgp->put.dst, dgp->put.len);
                    sendto(sock, (void*)dgp, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                    tx_bytes += dg_biased_size(len);
                    txbuf_vc1_push_wait(inum, elem_id);
                    tmp_nsec = get_nsec();
                    set_txtime(inum, 1, dgp->put.ser, tmp_nsec);
                    insert_retx_time(retx_list_pos(inum, 1, elem_id), tmp_nsec + rtt_pred(send_to, 1));
                    break;
                }
            }
            
            /* VC0 */
            for (i = 0; i < NODE_POP; i++) {
                inum = tx_vc0_next_inum;
                tx_vc0_next_inum = (tx_vc0_next_inum < NODE_POP - 1) ? tx_vc0_next_inum + 1 : 0;
                elem_id = txbuf_vc0_pop_dg(inum);
                if (elem_id >= 0) {
                    sock = pfds[inum].fd;
                    dgp = (dg_union*)txbuf[inum].vc0.list[elem_id].dg;
                    len = dg_size_vc0(dgp->copy.type);
                    send_to = txbuf[inum].vc0.list[elem_id].send_to;
                    dgp->copy.ser = inc_ser(inum, 0);
                    dgp->copy.seq = inc_seq(&seq_table[inum * NUM_PROCS + send_to].txseq0);
                    addr.sin_family = AF_INET;
                    addr.sin_port = PORT_TABLE[send_to];
                    addr.sin_addr.s_addr = ADDR_TABLE[send_to];
                    debug printf("rank %d - transport Transmit COMMAND %d from = %d, to = %d, ser = 0x%04x, seq = 0x%04x, ptr = 0x%016" PRIx64 ", s = %d, dst = 0x%016" PRIx64 ", dst = 0x%016" PRIx64 "\n", MY_RANK, dgp->copy.type, dgp->copy.rank, send_to, dgp->copy.ser, dgp->copy.seq, dgp->copy.ptr, dgp->copy.s, dgp->copy.dst, dgp->copy.src);
                    sendto(sock, (void*)dgp, (ssize_t)len, 0, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
                    tx_bytes += dg_biased_size(len);
                    txbuf_vc0_push_wait(inum, elem_id);
                    tmp_nsec = get_nsec();
                    set_txtime(inum, 0, dgp->copy.ser, tmp_nsec);
                    insert_retx_time(retx_list_pos(inum, 0, elem_id), tmp_nsec + rtt_pred(send_to, 0));
                    break;
                }
            }
            
            /* Update estimated time */
            estimated_nsec = current_nsec + tx_bytes * 8000ULL / NETWORK_BANDWIDTH;
        }
        
        /******** Protocol processing ********/
        
        /*** Recieve VC2 and Complete commands ***/
        
        pthread_mutex_lock(&doorbell[MY_INUM].mutex);
        
        /* Receive END: ibuf and rxbuf vc2 */
        for (inum = 0; inum < NODE_POP; inum++) {
            while ((elem_id = ibuf_vc2_pop_dg(inum)) >= 0) {
                dgp = (dg_union*)ibuf[ibuf_pos(MY_INUM, inum)].vc2.list[elem_id].dg;
                pos = dgp->end.ptr & MASK_CQ;
                /* if (cq[pos].stat != CQSTAT_WAIT) exception; */
                cq[pos].stat = CQSTAT_DONE;
                ibuf_vc2_push_free(inum, elem_id);
            }
        }
        if (NUM_PROCS != NODE_POP) {
            while ((elem_id = rxbuf_vc2_pop_dg()) >= 0) {
                dgp = (dg_union*)rxbuf[MY_INUM].list[elem_id].dg;
                pos = dgp->end.ptr & MASK_CQ;
                /* if (cq[pos].stat != CQSTAT_WAIT) exception; */
                cq[pos].stat = CQSTAT_DONE;
                rxbuf_push_free(MY_INUM, elem_id);
            }
        }
        
        /* Advance completion pointer */
        pthread_mutex_lock(&mutex_cq);
        while(cqcp < cqxp) {
            p = cqcp & MASK_CQ;
            if (cq[p].stat != CQSTAT_DONE) break;
            cqcp++;
            debug printf("rank %d - protocol cqcp advance to 0x%016" PRIx64 " (cqwp 0x%016" PRIx64 ")\n", MY_RANK, cqcp, cqwp);
        }
        pthread_mutex_unlock(&mutex_cq);
        
        /* Check empty */
        if (MY_INUM > 0 && (check_clear = is_dq_empty())) {
            /* Check rxbuf */
            if (NUM_PROCS != NODE_POP) {
                if (rxbuf[MY_INUM].vc0.dg.head >= 0) check_clear = 0;
                if (rxbuf[MY_INUM].vc1.dg.head >= 0) check_clear = 0;
            }
            
            /* Check ibuf */
            pos = NODE_POP * MY_INUM;
            for (i = 0 ; i < NODE_POP; i++) {
                if (ibuf[pos].vc0.dg.head >= 0) check_clear = 0;
                if (ibuf[pos].vc1.dg.head >= 0) check_clear = 0;
                pos++;
            }
            
            /* Check command queue */
            pthread_mutex_lock(&mutex_cq);
            if (cqcp < cqwp) check_clear = 0;
            pthread_mutex_unlock(&mutex_cq);
            
            /* Wait and redo if it is clear */
            if (check_clear) {
                doorbell_wait();
                pthread_mutex_unlock(&doorbell[MY_INUM].mutex);
                continue;
            }
        }
        pthread_mutex_unlock(&doorbell[MY_INUM].mutex);
        
        /*** Receive VC1 and Execute PUT ***/
        
        /* Receive a datagram: ibuf and rxbuf vc1 */
        if (NUM_PROCS != NODE_POP) check_not_full = rxbuf_vc1ack_is_not_full(MY_INUM);
        if (MY_INUM > 0) pthread_mutex_lock(&doorbell[MY_INUM].mutex);
        for (i = 0; i < NODE_POP + 1; i++) {
            if (rx_vc1_next_inum == NODE_POP) {
                if (NUM_PROCS == NODE_POP)
                    elem_id = -1;
                else if (check_not_full)
                    elem_id = rxbuf_vc1_pop_dg();
                else
                    elem_id = -1;
            } else
                elem_id = ibuf_vc1_pop_dg(rx_vc1_next_inum);
            if (elem_id >= 0) break;
            rx_vc1_next_inum = (rx_vc1_next_inum < NODE_POP) ? rx_vc1_next_inum + 1 : 0;
        }
        if (MY_INUM > 0) pthread_mutex_unlock(&doorbell[MY_INUM].mutex);
        
        if (elem_id >= 0) {
            if (rx_vc1_next_inum == NODE_POP) {
                dgp = (dg_union*)rxbuf[MY_INUM].list[elem_id].dg;
                memcpy(ga2address(dgp->put.dst), (void*)dgp->put.data, dgp->put.len);
                rxbuf_push_free_vc1ack(MY_INUM, elem_id);
            } else {
                dgp = (dg_union*)ibuf[ibuf_pos(MY_INUM, rx_vc1_next_inum)].vc1.list[elem_id].dg;
                memcpy(ga2address(dgp->put.dst), (void*)dgp->put.data, dgp->put.len);
                ibuf_vc1_push_free(rx_vc1_next_inum, elem_id);
            }
            rx_vc1_next_inum = (rx_vc1_next_inum + 1 < NODE_POP) ? rx_vc1_next_inum + 1 : 0;
        }
        
        /*** Delegate Queue ***/
        
        /* Sweep txbuf vc1 ack */
        if (NUM_PROCS != NODE_POP) {
            while ((elem_id = txbuf_vc1_pop_ack()) >= 0) {
                if ((pos = txbuf[MY_INUM].vc1.list[elem_id].dq_pos) >= 0) {
                    /* if (dq[pos].stat != DQSTAT_WAIT) exception */
                    if (dq[pos].rank != MY_RANK) {
                        dq[pos].stat = DQSTAT_NOTIFY;
                        dq[pos].inum = INUM_TABLE[dq[pos].rank];
                        dq[pos].gateway = GTWY_TABLE[dq[pos].rank];
                    } else { /* dq[pos].rank == MY_RANK */
                        /* Notify completion directly to the corresponding CQ entry */
                        p = dq[pos].ptr & MASK_CQ;
                        cq[p].stat = CQSTAT_DONE;
                        dq_free(pos);
                    }
                }
                txbuf_vc1_push_free(elem_id);
                debug printf("rank %d - protocol dq[%d] got ack from txbuf vc1 ack\n", MY_RANK, pos);
            }
        }
        
        /* Check waiting entries */
        check_wait = 0;
        if (is_dq_not_empty()) {
            pos = dqhead;
            while (pos != dqexec) {
                next = dqnext[pos];
                if (dq[pos].stat == DQSTAT_WAIT) {
                    check_wait |= 2;
                    if (dq[pos].gateway == MY_GATEWAY) {
                        if (ibuf_vc1_free_ack_count(dq[pos].inum) >= dqwait[pos]) {
                            debug printf("rank %d - protocol dq[%d] got ack from ibuf inum =%d vc1 ack_count = %d\n", MY_RANK, pos, dq[pos].inum, dqwait[pos]);
                            if (dq[pos].rank != MY_RANK) {
                                dq[pos].stat = DQSTAT_NOTIFY;
                                dq[pos].inum = INUM_TABLE[dq[pos].rank];
                                dq[pos].gateway = GTWY_TABLE[dq[pos].rank];
                            } else { /* dq[pos].rank == MY_RANK */
                                /* Notify completion directly to the corresponding CQ entry */
                                p = dq[pos].ptr & MASK_CQ;
                                cq[p].stat = CQSTAT_DONE;
                                dq_free(pos);
                            }
                            check_wait--;
                        }
                    }
                    check_wait >>= 1;
                }
                if (dq[pos].stat == DQSTAT_NOTIFY) {
                    if (dq[pos].gateway == MY_GATEWAY) {
                        elem_id = ibuf_vc2_pop_free(dq[pos].inum);
                        if (elem_id >= 0) {
                            dgp = (dg_union*)ibuf[ibuf_pos(dq[pos].inum, MY_INUM)].vc2.list[elem_id].dg;
                            dgp->end.task = TASKID;
                            dgp->end.c    = NORMAL;
                            dgp->end.vc   = 2;
                            dgp->end.rank = MY_RANK;
                            dgp->end.ptr  = dq[pos].ptr;
                            ibuf_vc2_push_dg(dq[pos].inum, elem_id);
                            dq_free(pos);
                       }
                    } else { /* dq[pos].gateway != MY_GATEWAY */
                        elem_id = txbuf_vc2_pop_free();
                        if (elem_id >= 0) {
                            txbuf[MY_INUM].vc2.list[elem_id].send_to = dq[pos].rank;
                            dgp = (dg_union*)txbuf[MY_INUM].vc2.list[elem_id].dg;
                            dgp->end.task = TASKID;
                            dgp->end.c    = NORMAL;
                            dgp->end.vc   = 2;
                            dgp->end.rank = MY_RANK;
                            dgp->end.ptr  = dq[pos].ptr;
                            txbuf_vc2_push_dg(elem_id);
                            dq_free(pos);
                        }
                    }
                }
                pos = next;
            }
        }
        
        if (is_dq_not_empty() && dqexec >= 0) {
            /* Process a command */
            pos = dqexec;
            if (dq[pos].stat == DQSTAT_FENCE && check_wait == 0) dq[pos].stat = DQSTAT_ACTIVE;
            if (dq[pos].stat == DQSTAT_ACTIVE) {
                if (dq[pos].inum == MY_INUM && dq[pos].gateway == MY_GATEWAY) {
                    /* Execute a command directly at remote */
                    type = dq[pos].type;
                    if (type == COPY) {
                        memcpy(ga2address(dq[pos].dst), ga2address(dq[pos].src), dq[pos].size);
                    } else if (type == CAS4) {
                        *(uint32_t*)ga2address(dq[pos].dst) = sync_val_compare_and_swap_4((uint32_t*)ga2address(dq[pos].src), dq[pos].old4, dq[pos].new4);
                    } else if (type == CAS8) {
                        dgp->put.len = 8;
                        *(uint64_t*)ga2address(dq[pos].dst) = sync_val_compare_and_swap_8((uint64_t*)ga2address(dq[pos].src), dq[pos].old8, dq[pos].new8);
                    } else if (type == SWAP4) {
                        dgp->put.len = 4;
                        *(uint32_t*)ga2address(dq[pos].dst) = sync_swap_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                    } else if (type == SWAP8) {
                        dgp->put.len = 8;
                        *(uint64_t*)ga2address(dq[pos].dst) = sync_swap_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                    } else if (type == ADD4) {
                        dgp->put.len = 4;
                        *(uint32_t*)ga2address(dq[pos].dst) = sync_fetch_and_add_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                    } else if (type == ADD8) {
                        dgp->put.len = 8;
                        *(uint64_t*)ga2address(dq[pos].dst) = sync_fetch_and_add_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                    } else if (type == XOR4) {
                        dgp->put.len = 4;
                        *(uint32_t*)ga2address(dq[pos].dst) = sync_fetch_and_xor_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                    } else if (type == XOR8) {
                        dgp->put.len = 8;
                        *(uint64_t*)ga2address(dq[pos].dst) = sync_fetch_and_xor_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                    } else if (type == OR4) {
                        dgp->put.len = 4;
                        *(uint32_t*)ga2address(dq[pos].dst) = sync_fetch_and_or_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                    } else if (type == OR8) {
                        dgp->put.len = 8;
                        *(uint64_t*)ga2address(dq[pos].dst) = sync_fetch_and_or_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                    } else if (type == AND4) {
                        dgp->put.len = 4;
                        *(uint32_t*)ga2address(dq[pos].dst) = sync_fetch_and_and_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                    } else { /* type == AND8 */
                        dgp->put.len = 8;
                        *(uint64_t*)ga2address(dq[pos].dst) = sync_fetch_and_and_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                    }
                    debug printf("rank %d - protocol Dq %d local execution dqhead = %d, dqexec = %d, dqtail =%d, dqflnum = %d, ds[%d] stat = %d, rank = %d, ptr = 0x%016" PRIx64 ", type = %d\n", MY_RANK, pos, dqhead, dqexec, dqtail, dqflnum, pos, dq[pos].stat, dq[pos].rank, dq[pos].ptr, dq[pos].type);
                    dq[pos].stat = DQSTAT_NOTIFY;
                    dq[pos].inum = INUM_TABLE[dq[pos].rank];
                    dq[pos].gateway = GTWY_TABLE[dq[pos].rank];
                    dqexec = dqnext[pos];
                    dqoffset = 0;
                } else {
                    if (dq[pos].gateway == MY_GATEWAY) {
                        elem_id = ibuf_vc1_pop_free(dq[pos].inum);
                        if (elem_id >= 0) dgp = (dg_union*)ibuf[ibuf_pos(dq[pos].inum, MY_INUM)].vc1.list[elem_id].dg;
                        debug printf("rank %d - protocol Dq %d to ibuf[%d][%d] vc1 elem_id = %d\n", MY_RANK, pos, dq[pos].inum, MY_INUM, elem_id);
                    } else {
                        elem_id = txbuf_vc1_pop_free();
                        if (elem_id >= 0) {
                            txbuf[MY_INUM].vc1.list[elem_id].send_to = ga2rank(dq[pos].dst);
                            dgp = (dg_union*)txbuf[MY_INUM].vc1.list[elem_id].dg;
                        }
                    }
                    if (elem_id >= 0) {
                        dgp->put.task = TASKID;
                        dgp->put.c    = NORMAL;
                        dgp->put.vc   = 1;
                        dgp->put.rank = MY_RANK;
                        dgp->put.dst  = dq[pos].dst;
                        check_cont = 0;
                        type = dq[pos].type;
                        if (type == COPY) {
                            size = dq[pos].size - dqoffset;
                            size = (size < MAX_DATA_SIZE) ? size : MAX_DATA_SIZE;
                            dgp->put.dst += dqoffset;
                            dgp->put.len = size;
                            memcpy(dgp->put.data, ga2address(dq[pos].src) + dqoffset, size);
                            dqoffset += size;
                            if (dq[pos].size > dqoffset) check_cont = 1;
                        } else if (type == CAS4) {
                            dgp->put.len = 4;
                            *(uint32_t*)dgp->put.data = sync_val_compare_and_swap_4((uint32_t*)ga2address(dq[pos].src), dq[pos].old4, dq[pos].new4);
                        } else if (type == CAS8) {
                            dgp->put.len = 8;
                            *(uint64_t*)dgp->put.data = sync_val_compare_and_swap_8((uint64_t*)ga2address(dq[pos].src), dq[pos].old8, dq[pos].new8);
                        } else if (type == SWAP4) {
                            dgp->put.len = 4;
                            *(uint32_t*)dgp->put.data = sync_swap_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                        } else if (type == SWAP8) {
                            dgp->put.len = 8;
                            *(uint64_t*)dgp->put.data = sync_swap_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                        } else if (type == ADD4) {
                            dgp->put.len = 4;
                            *(uint32_t*)dgp->put.data = sync_fetch_and_add_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                        } else if (type == ADD8) {
                            dgp->put.len = 8;
                            *(uint64_t*)dgp->put.data = sync_fetch_and_add_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                        } else if (type == XOR4) {
                            dgp->put.len = 4;
                            *(uint32_t*)dgp->put.data = sync_fetch_and_xor_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                        } else if (type == XOR8) {
                            dgp->put.len = 8;
                            *(uint64_t*)dgp->put.data = sync_fetch_and_xor_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                        } else if (type == OR4) {
                            dgp->put.len = 4;
                            *(uint32_t*)dgp->put.data = sync_fetch_and_or_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                        } else if (type == OR8) {
                            dgp->put.len = 8;
                            *(uint64_t*)dgp->put.data = sync_fetch_and_or_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                        } else if (type == AND4) {
                            dgp->put.len = 4;
                            *(uint32_t*)dgp->put.data = sync_fetch_and_and_4((uint32_t*)ga2address(dq[pos].src), dq[pos].val4);
                        } else { /* type == AND8 */
                            dgp->put.len = 8;
                            *(uint64_t*)dgp->put.data = sync_fetch_and_and_8((uint64_t*)ga2address(dq[pos].src), dq[pos].val8);
                        }
                        if (check_cont) {
                            if (dq[pos].gateway == MY_GATEWAY) {
                                ibuf_vc1_push_dg(dq[pos].inum, elem_id);
                            } else {
                                txbuf[MY_INUM].vc1.list[elem_id].dq_pos = -1;
                                txbuf_vc1_push_dg(elem_id);
                            }
                        } else { /* check_cont == 0 */
                            if (dq[pos].gateway == MY_GATEWAY) {
                                dqwait[pos] = ibuf_vc1_push_dg(dq[pos].inum, elem_id);
                                debug printf("rank %d - protocol Dq %d wait for ibuf vc1 ack_count %d\n", MY_RANK, pos, dqwait[pos]);
                            } else {
                                txbuf[MY_INUM].vc1.list[elem_id].dq_pos = pos;
                                txbuf_vc1_push_dg(elem_id);
                                debug printf("rank %d - protocol Dq %d wait for txbuf vc1 ack\n", MY_RANK, pos);
                            }
                            dq[pos].stat = DQSTAT_WAIT;
                            dqexec = dqnext[pos];
                            dqoffset = 0;
                        }
                    }
                }
            }
        }
        
        /* Receive VC0 and Enqueue a new command */
        if (is_dq_not_full()) {
            if (MY_INUM > 0) pthread_mutex_lock(&doorbell[MY_INUM].mutex);
            for (i = 0; i < NODE_POP + 1; i++) {
                if (rx_vc0_next_inum == NODE_POP)
                    if (NUM_PROCS != NODE_POP)
                        elem_id = rxbuf_vc0_pop_dg();
                    else
                        elem_id = -1;
                else
                    elem_id = ibuf_vc0_pop_dg(rx_vc0_next_inum);
                if (elem_id >= 0) break;
                rx_vc0_next_inum = (rx_vc0_next_inum < NODE_POP) ? rx_vc0_next_inum + 1 : 0;
            }
            if (MY_INUM > 0) pthread_mutex_unlock(&doorbell[MY_INUM].mutex);
            
            if (elem_id >= 0) {
                if (rx_vc0_next_inum == NODE_POP)
                    dgp = (dg_union*)rxbuf[MY_INUM].list[elem_id].dg;
                else
                    dgp = (dg_union*)ibuf[ibuf_pos(MY_INUM, rx_vc0_next_inum)].vc0.list[elem_id].dg;
                pos = dq_push(dgp->copy.ptr, dgp->copy.rank, dgp->copy.s, dgp->copy.type, dgp->copy.dst, dgp->copy.src);
                type = dq[pos].type;
                debug printf("rank %d - protocol Exec dq[%d] dqhead = %d, dqexec = %d, dqtail =%d, dqflnum = %d, from = %d, cqp = 0x%016" PRIx64 " type = %d remote to X\n", MY_RANK, pos, dqhead, dqexec, dqtail, dqflnum, dgp->copy.rank, dgp->copy.ptr, type);
                if (type == COPY) {
                    dq[pos].size = dgp->copy.size;
                } else if (type == CAS4) {
                    dq[pos].old4 = dgp->cas4.oldval;
                    dq[pos].new4 = dgp->cas4.newval;
                } else if (type == CAS8) {
                    dq[pos].old8 = dgp->cas8.oldval;
                    dq[pos].new8 = dgp->cas8.newval;
                } else if (type == SWAP4) {
                    dq[pos].val4 = dgp->swap4.val;
                } else if (type == SWAP8) {
                    dq[pos].val8 = dgp->swap8.val;
                } else if (type == ADD4) {
                    dq[pos].val4 = dgp->add4.val;
                } else if (type == ADD8) {
                    dq[pos].val8 = dgp->add8.val;
                } else if (type == XOR4) {
                    dq[pos].val4 = dgp->xor4.val;
                } else if (type == XOR8) {
                    dq[pos].val8 = dgp->xor8.val;
                } else if (type == OR4) {
                    dq[pos].val4 = dgp->or4.val;
                } else if (type == OR8) {
                    dq[pos].val8 = dgp->or8.val;
                } else if (type == AND4) {
                    dq[pos].val4 = dgp->and4.val;
                } else { /* type == AND8 */
                    dq[pos].val8 = dgp->and8.val;
                }
                if (rx_vc0_next_inum == NODE_POP)
                    rxbuf_push_free(MY_INUM, elem_id);
                else
                    ibuf_vc0_push_free(rx_vc0_next_inum, elem_id);
                
                rx_vc0_next_inum = (rx_vc0_next_inum < NODE_POP) ? rx_vc0_next_inum + 1 : 0;
            }
        }
        
        /*** Command Queue ***/
        
        /* Send a command */
        pthread_mutex_lock(&mutex_cq);
        p = cqxp & MASK_CQ;
        if (cqxp < cqwp && (cq[p].order < cqcp || cq[p].rfence == 1)) {
            if (cq[p].stat == CQSTAT_11) {
                /* Execute a command directly at local */
                debug printf("rank %d - protocol Exec cq 0x%016" PRIx64 " type = %d order = 0x%016" PRIx64 " local to local\n", MY_RANK, cqxp, cq[p].type, cq[p].order);
                type = cq[p].type;
                if (type == COPY)
                    memcpy(ga2address(cq[p].dst), ga2address(cq[p].src), cq[p].size);
                else if (type == CAS4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_val_compare_and_swap_4((uint32_t*)ga2address(cq[p].src), cq[p].old4, cq[p].new4);
                else if (type == CAS8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_val_compare_and_swap_8((uint64_t*)ga2address(cq[p].src), cq[p].old8, cq[p].new8);
                else if (type == SWAP4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_swap_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
                else if (type == SWAP8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_swap_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
                else if (type == ADD4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_add_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
                else if (type == ADD8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_add_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
                else if (type == XOR4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_xor_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
                else if (type == XOR8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_xor_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
                else if (type == OR4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_or_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
                else if (type == OR8)
                    *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_or_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
                else if (type == AND4)
                    *(uint32_t*)ga2address(cq[p].dst) = sync_fetch_and_and_4((uint32_t*)ga2address(cq[p].src), cq[p].val4);
                else /* type == AND8 */
                    *(uint64_t*)ga2address(cq[p].dst) = sync_fetch_and_and_8((uint64_t*)ga2address(cq[p].src), cq[p].val8);
                cq[p].stat = CQSTAT_DONE;
                cqxp++;
                pthread_mutex_unlock(&mutex_cq);
            } else if (cq[p].stat == CQSTAT_12) {
                /* Enqueue a command directly to the local delegate queue */
                if (is_dq_not_full()) {
                    type = cq[p].type;
                    pos = dq_push(cqxp, MY_RANK, cq[p].rfence, type, cq[p].dst, cq[p].src);
                    debug printf("rank %d - protocol Exec cq 0x%016" PRIx64 " into dq[%d] type = %d local to remote\n", MY_RANK, cqxp, pos, cq[p].type);
                    if (type == COPY) {
                        dq[pos].size = cq[p].size;
                    } else if (type == CAS4) {
                        dq[pos].old4 = cq[p].old4;
                        dq[pos].new4 = cq[p].new4;
                    } else if (type == CAS8) {
                        dq[pos].old8 = cq[p].old8;
                        dq[pos].new8 = cq[p].new8;
                    } else if (type == SWAP4 || type == ADD4 || type == XOR4 || type == OR4 || type == AND4) {
                        dq[pos].val4 = cq[p].val4;
                    } else { /* type == SWAP8 || type == ADD8 || type == XOR8 || type == OR8 || type == AND8 */
                        dq[pos].val8 = cq[p].val8;
                    }
                    cq[p].stat = CQSTAT_WAIT;
                    cqxp++;
                }
                pthread_mutex_unlock(&mutex_cq);
            } else if (cq[p].stat == CQSTAT_2X) {
                /* Transmit a command datagram: ibuf and txbuf vc0 */
                if (cq[p].gateway == MY_GATEWAY) {
                    elem_id = ibuf_vc0_pop_free(cq[p].inum);
                    if (elem_id >= 0) dgp = (dg_union*)ibuf[ibuf_pos(cq[p].inum, MY_INUM)].vc0.list[elem_id].dg;
                } else {
                    elem_id = txbuf_vc0_pop_free();
                    if (elem_id >= 0) {
                        dgp = (dg_union*)txbuf[MY_INUM].vc0.list[elem_id].dg;
                        txbuf[MY_INUM].vc0.list[elem_id].send_to = ga2rank(cq[p].src);
                    }
                }
                if (elem_id >= 0) {
                    debug printf("rank %d - protocol Exec cq 0x%016" PRIx64 " type = %d remote to X\n", MY_RANK, cqxp, cq[p].type);
                    type = cq[p].type;
                    dgp->copy.task = TASKID;
                    dgp->copy.c    = NORMAL;
                    dgp->copy.vc   = 0;
                    dgp->copy.rank = MY_RANK;
                    dgp->copy.ptr  = cqxp;
                    dgp->copy.s    = cq[p].rfence;
                    dgp->copy.type = type;
                    dgp->copy.dst  = cq[p].dst;
                    dgp->copy.src  = cq[p].src;
                    if (type == COPY) {
                        dgp->copy.size = cq[p].size;
                    } else if (type == CAS4) {
                        dgp->cas4.oldval = cq[p].old4;
                        dgp->cas4.newval = cq[p].new4;
                    } else if (type == CAS8) {
                        dgp->cas8.oldval = cq[p].old8;
                        dgp->cas8.newval = cq[p].new8;
                    } else if (type == SWAP4 || type == ADD4 || type == XOR4 || type == OR4 || type == AND4) {
                        dgp->swap4.val = cq[p].val4;
                    } else { /* type == SWAP8 || type == ADD8 || type == XOR8 || type == OR8 || type == AND8 */
                        dgp->swap8.val = cq[p].val8;
                    }
                    cq[p].stat = CQSTAT_WAIT;
                    cqxp++;
                    pthread_mutex_unlock(&mutex_cq);
                    if (cq[p].gateway == MY_GATEWAY)
                        ibuf_vc0_push_dg(cq[p].inum, elem_id);
                    else
                        txbuf_vc0_push_dg(elem_id);
                } else
                    pthread_mutex_unlock(&mutex_cq);
            } else {
                if (cq[p].stat == CQSTAT_DONE) cqxp++;
                pthread_mutex_unlock(&mutex_cq);
            }
        } else {
            pthread_mutex_unlock(&mutex_cq);
        }
    }
    
    if (MY_INUM == 0 && NUM_PROCS != NODE_POP) {
        for (i = NODE_POP - 1; i >= 0; i--) close(pfds[i].fd);
        finalize_retx_list();
        finalize_rtt_pred();
        finalize_txtime();
        finalize_seq();
        finalize_ser();
    }
    debug printf("rank %d - communication thread end\n", MY_RANK);
    return NULL;
}

/****************************/
/* Infrastructure functions */
/****************************/

static pthread_t comm_thread_id;

int iacpbludp_init_gma(void)
{
    int r;
    
    r = init_shmbuffer();
    if (r) return r;
    init_cq();
    init_dq();
    
    pthread_mutex_init(&mutex_comm_thread_ready, NULL);
    pthread_cond_init(&cond_comm_thread_ready, NULL);
    pthread_mutex_init(&mutex_comm_thread_start, NULL);
    pthread_cond_init(&cond_comm_thread_start, NULL);
    quit_comm_thread = 0;
    pthread_mutex_init(&mutex_quit_comm_thread, NULL);
    
    pthread_create(&comm_thread_id, NULL, comm_thread_func, NULL);
    
    pthread_mutex_lock(&mutex_comm_thread_ready);
    pthread_cond_wait(&cond_comm_thread_ready, &mutex_comm_thread_ready);
    pthread_mutex_unlock(&mutex_comm_thread_ready);
    acp_sync();
    pthread_mutex_lock(&mutex_comm_thread_start);
    pthread_cond_signal(&cond_comm_thread_start);
    pthread_mutex_unlock(&mutex_comm_thread_start);
    
    return 0;
}

int iacpbludp_finalize_gma(void)
{
    pthread_mutex_lock(&mutex_quit_comm_thread);
    quit_comm_thread = 1;
    pthread_mutex_unlock(&mutex_quit_comm_thread);
    
    pthread_mutex_lock(&doorbell[MY_INUM].mutex);
    doorbell_ring(MY_INUM);
    pthread_mutex_unlock(&doorbell[MY_INUM].mutex);
    
    pthread_join(comm_thread_id, NULL);
    
    pthread_mutex_destroy(&mutex_quit_comm_thread);
    
    pthread_cond_destroy(&cond_comm_thread_ready);
    pthread_mutex_destroy(&mutex_comm_thread_ready);
    
    finalize_cq();
    finalize_shmbuffer();
    
    return 0;
}

void iacpbludp_abort_gma(void)
{
    quit_comm_thread = 1;
    pthread_cancel(comm_thread_id);
    pthread_mutex_destroy(&mutex_quit_comm_thread);
    
    pthread_cond_destroy(&cond_comm_thread_ready);
    pthread_mutex_destroy(&mutex_comm_thread_ready);
    
    finalize_cq();
    finalize_shmbuffer();
    
    return;
}

