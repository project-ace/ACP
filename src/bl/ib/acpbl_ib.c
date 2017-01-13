/*
 * ACP Basic Layer for InfiniBand
 * 
 * Copyright (c) 2014-2014 Kyushu University
 * Copyright (c) 2014      Institute of Systems, Information Technologies
 *                         and Nanotechnologies 2014
 * Copyright (c) 2014      FUJITSU LIMITED
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h> /* write() */
#include <string.h> /* memset() */
#include <stdlib.h> /* malloc() */
#include <pthread.h>
#include <errno.h>
#include <infiniband/verbs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h> /* for sched_yield and setaffnity */
#include <arpa/inet.h>
#include <acp.h>
#include "acpbl.h"
#include "acpbl_sync.h"
#include "acpbl_input.h"
/* H.Honda Jan.12 2016 begin */
#include <netdb.h>
/* H.Honda Jan.12 2016 end   */
/* H.Honda Dec.31 2015 begin */
#ifdef MPIACP
#include <netdb.h>
#include <poll.h>
#include "mpi.h"
#endif /* MPIACP */
/* H.Honda Dec.31 2015 end   */

size_t iacp_starter_memory_size_dl = 64 * 1024 * 1024;
size_t iacp_starter_memory_size_cl = 1024;

#define alm8_add_func(alm_add) if (alm8_add != 0) {alm8_add = 8 - alm8_add;}

/* define sizes */
#define MAX_RM_SIZE      255U
#define MAX_CQ_SIZE     2048U
#define MAX_WR_SIZE        1U
#define MAX_CMDQ_ENTRY  1024U
#define MAX_RCMDB_SIZE  2048U
#define MAX_ACK_COUNT   0x3fffffffffffffffLLU
#define MAX_RKEY_CASH_SIZE 1024

/* bits range on GA format */
#define RANK_BITS    21U
#define COLOR_BITS    1U
#define GMTAG_BITS    8U
#define OFFSET_BITS  34U

/* MASKs */
#define RANK_MASK    0x00000000001fffffLLU
#define COLOR_MASK   0x0000000000000001LLU
#define GMTAG_MASK   0x00000000000000ffLLU
#define OFFSET_MASK  0x00000003ffffffffLLU

#define TAG_SM  0xff

#define MASK_WRID_RCMDB     0x8000000000000000LLU
#define MASK_WRID_ACK       0xc000000000000000LLU
#define MASK_ATOMIC   128U
#define MASK_ATOMIC8  192U

/* define command types */
#define NOCMD     0U
#define COPY      1U
#define CAS4    128U
#define SWAP4   129U
#define ADD4    130U
#define XOR4    131U
#define AND4    132U
#define OR4     133U
#define CAS8    192U
#define SWAP8   193U
#define ADD8    194U
#define XOR8    195U
#define OR8     196U
#define AND8    197U
#define FIN     255U 

/* define STATUSs */
#define COMPLETED                             0U
#define UNISSUED                              1U
#define ISSUED                                2U
#define FIRST_ISSUED                          3U
#define PRE_PUTRRMFLAG                        4U
#define FINISHED                              5U
#define WAIT_RRM                              6U
#define GETED_RRM                             7U
#define WAIT_TAIL                             8U
#define WAIT_HEAD                             9U
#define WAIT_PUT_CMD                         10U
#define WAIT_PUT_DST                         11U
#define PRE_GET_HEAD                         12U
#define PRE_PUT_CMD                          13U
#define CMD_UNISSUED                         14U
#define CMD_ISSUED                           15U
#define CMD_FIRST_ISSUED                     16U
#define CMD_WAIT_RRM                         17U
#define CMD_WAIT_PUT_DST                     18U
#define CMD_WAIT_FIRST_PUT_DST               19U
#define CMD_GETED_RRM                        20U
#define CMD_WRITEBACK_FIN                    21U
#define CMD_PRE_WRITEBACK_FIN                22U
#define CMD_PRE_PUTRRMGETEDFLAG_ISSUED       23U
#define CMD_PRE_PUTRRMGETEDFLAG_PUT_DST      24U

/* comm. thread releases the own cpu time.  */
#define ECOUNT 100

/* acp_sync() */
#define ST_SYNC_INIT 0
#define ST_SYNC_IDLE 1
#define ST_SYNC_BUSY 2

/* resouce info for starter memory */
typedef struct resource_info{
    int myrank; /* rank ID */
    struct ibv_port_attr port_attr; /* IB port attributes */
    struct ibv_context *ib_ctx; /* device handle */
    struct ibv_pd *pd; /* PD handle */
    struct ibv_mr *mr; /* MR handle for starter memory */
} RES;

typedef struct reg_mem{
    uint32_t rank;
    char *addr;  /* the front of memory region */
    uint32_t rkey;  /* memory registration key in IB */
    size_t size;  /* the size of memory region */
    uint64_t lock; /* flag utilization of RM */
    uint64_t valid; /* valid of tag of memory regeion  */
} RM;

typedef struct connect_ib_info{
    uint64_t addr;/* address of starter memory */
    uint32_t rkey;  /* rkey of starter memory */
    uint16_t lid;  /* local ID of Local IB */
    int rank;  /* local rank */
} CII;

typedef struct starter_memroy_info{
    uint64_t addr; /* starter memory address */
    uint32_t rkey; /* the rkey of starter memory */
} SMI;

typedef struct copy_command_format{
    size_t size; /* copy size */
} CCMD;

typedef struct cas4_command_format{
    uint32_t data1; /* data1(4) */
    uint32_t data2; /* data2(4) */
} CAS4CMD;

typedef struct cas8_command_format{
    uint64_t data1; /* data1(8) */
    uint64_t data2; /* data2(8) */
} CAS8CMD;

typedef struct atomic4_command_format{
    uint32_t data; /* data(4) */
} ATOMIC4CMD;

typedef struct atomic8_command_format{
    uint64_t data; /* data(8) */
} ATOMIC8CMD;

typedef union command_format_exstra{
    CCMD copy_cmd; /* copy command format */
    CAS4CMD cas4_cmd; /* cas4 command format */
    CAS8CMD cas8_cmd; /* cas8 command format */
    ATOMIC4CMD atomic4_cmd; /* atomic4 command format */
    ATOMIC8CMD atomic8_cmd; /* atomic4 command format */
} CMDE;

typedef struct command_format{
    uint64_t valid_head; /* validation of CMD for RCMDBUF */
    int rank; /* issued rank */
    uint32_t type; /* command type */
    acp_handle_t ishdl; /* issued handle */
    acp_handle_t ohdl; /* order handle */
    acp_handle_t wr_id; /* IB work request ID */
    uint64_t stat; /* command status */
    acp_ga_t gadst; /* destination of ga */
    acp_ga_t gasrc; /* srouce of ga */
    uint64_t tail_buf; /* tail buffer */
    uint64_t head_buf; /* head_buffer */
    uint64_t writebackstat;/* write back status buffer */
    uint64_t replydata; /* reply data for atomic buffer */
    CMDE cmde; /* Command format exstra for GMA */
    uint64_t valid_tail; /* validation of CMD for RCMDBUF */
} CMD;

/* acp_sync() */
enum sync_role {PARENT, NORMAL, REMAIN};

/* acp_sync() */
volatile static uint64_t *sync_bodyla;
static acp_atkey_t sync_bodykey;
static acp_ga_t sync_bodyga;
static acp_ga_t *sync_stepgatbl;
volatile static int sync_nsteps, sync_peer;
volatile static enum sync_role sync_myrole;
volatile static int sync_status = ST_SYNC_INIT;

/* socket file descripter */
static int sock_accept;
static int sock_connect;

static RES res; /* resource of IB */
static struct ibv_qp **qp; /* QP handle */
static struct ibv_cq *cq; /* CQ handle */
static int num_devices; /* # of devices */

static int acp_myrank = -1; /* my rank on acp lib*/
static int acp_numprocs = -1; /* # of process on acp lib */
static uint32_t  acp_taskid = 0 ; /* # of process on acp lib */
static size_t acp_smsize = -1; /* the size of starter memory on acp lib */
static size_t acp_smsize_adj; /* adjust the size of starter memory to 8 byte alignment */
static size_t acp_smdlsize_adj; /* adjust the size of dl starter memory to 8 byte alignment */
static size_t acp_smclsize_adj; /* adjust the size of cl starter memory to 8 byte alignment */
static size_t acp_smvdsize_adj; /* adjust the size of vd starter memory to 8 byte alignment */
static size_t ncharflagtb_adj; /* adjust the size of flag table of char to 8 byte alignment */
static uint16_t my_port; /* my port */
static uint16_t dst_port; /* destination port */
static uint32_t dst_addr; /* destination ip address */

static SMI *smi_tb; /* starter memory info table */
volatile static acp_handle_t head; /* the head of command queue */
volatile static acp_handle_t tail; /* the tail of command queue */
static RM **rrmtb; /* Remote addr/rkey info table */
struct ibv_mr *libvmrtb[MAX_RM_SIZE]; /* local ibv_mr table */ 
static uint32_t rkey_cache_size; /* max # of rrm table */

static uint64_t ack_id; /* wr_id for ack */
static uint64_t ack_comp_count;/* # of success ack */

/* system memory is able to rdma access */
static char *sysmem; /* starter memory address*/
static CMD *cmdq; /* comand queue */

static CMD *rcmdbuf; /* recieve buffer for command */
static CMD *putcmdbuf; /* local put cmd buffer */
static uint64_t *rcmdbuf_head; /* head of rcmdbuf */
static uint64_t *rcmdbuf_tail; /* tail of rcmdbuf */
static uint64_t *finished_stat_buf; /* finished status buffer */

static RM *lrmtb; /* Local addr/rkey info table */
static RM *recv_lrmtb; /* recv buffer for local addr/rkey table */

volatile static char *rrm_get_flag_tb; /* rrm get flag tb */
volatile static char *rrm_reset_flag_tb; /* rrm reset flag tb */
volatile static char *rrm_ack_flag_tb; /* rrm ack flag tb */
static char *rrm_hav_flag_tb; /* having rrm flag tb */
static char *true_flag_buf; /* get rrm flag buf */

static char *acp_buf_dl; /* acp starter memory for dl */
static char *acp_buf_cl; /* acp starter memory for cl */

/* offset of each pointers from sysmem top */
static uint64_t offset_cmdq; /* offset of cmdq from sysmem top */

static uint64_t offset_rcmdbuf; /* offset of recieve buffer for command from sysmem top */
static uint64_t offset_rcmdbuf_head; /* offset of head of rcmdbuf from sysmem top */
static uint64_t offset_rcmdbuf_tail; /* offset of tail of rcmdbuf from sysmem top */
static uint64_t offset_finished_stat_buf; /* offset of finished status buf from sysmem top */
static uint64_t offset_putcmdbuf; /* offset of writeback buffer from sysmem top */

static uint64_t offset_lrmtb; /* offset of Local addr/rkey info table from sysmem top */
static uint64_t offset_recv_lrmtb; /* offset of recv buffer for local addr/rkey table form sysmem top*/

static uint64_t offset_rrm_get_flag_tb; /* offset of rrm get flag tb from sysmem top */
static uint64_t offset_rrm_reset_flag_tb; /* offset of rrm reset flag tb sysmem top */
static uint64_t offset_rrm_ack_flag_tb; /* offset of rrm ack flag tb sysmem top */
static uint64_t offset_rrm_hav_flag_tb; /* offset of rrm hav flag tb sysmem top */
static uint64_t offset_true_flag_buf; /* offset of get rrm flag buf sysmem top */

static uint64_t offset_acp_buf_dl; /* offset of acp starter memory for dl from sysmem top */
static uint64_t offset_acp_buf_cl; /* offset of acp starter memory for cl from sysmem top */
static uint64_t offset_acp_buf_vd; /* offset of acp starter memory for vd from sysmem top */

static uint64_t offset_stat; /* offest of writestat from cmd top */

/* flags */
static int recv_rrm_flag = true; /* get enable recv rrm falg */
static int executed_acp_init = false; /* check if acp_init executed or not. */ 

static pthread_t comm_thread_id; /* communcation thread ID */

void acp_abort(const char *str){
  
    int i; /* general index */
    
    /* release socket file descriptor */
#ifdef DEBUG
    fprintf(stdout, "internal acp_abort\n");
    fflush(stdout);
#endif
    
#ifndef ACPBLONLY
    iacp_abort_cl();
    iacp_abort_dl();
#endif
    /* close IB resouce */
    if (res.mr != NULL) {
        ibv_dereg_mr(res.mr);
        res.mr = NULL;
    }
    for (i = 0;i < MAX_RM_SIZE;i++) {
        if (libvmrtb[i] != NULL) {
            ibv_dereg_mr(libvmrtb[i]);
            libvmrtb[i] = NULL;
        }
    }
    for (i = 0;i < acp_numprocs;i++) {
        if (qp[i] != NULL) {
            ibv_destroy_qp(qp[i]);
            qp[i] = NULL;
        }
    }
    if (qp != NULL) {
        free(qp);
        qp = NULL;
    }
    
    if (cq != NULL) {
        ibv_destroy_cq(cq);
        cq = NULL;
    }
    if (res.pd != NULL) {
        ibv_dealloc_pd(res.pd);
        res.pd = NULL;
    }
    if (res.ib_ctx != NULL) {
        ibv_close_device(res.ib_ctx);
        res.ib_ctx = NULL;
    }
    
    /* close socket file descritptor */
    if (sock_accept) {
        close(sock_accept);
    }
    if (sock_connect) {
        close(sock_connect);
    }
    
    /* free acp region */
    for (i = 0;i < rkey_cache_size ;i++) {
        if (rrmtb[i] != NULL) {
            free(rrmtb[i]);
            rrmtb[i] = NULL;
        }
    }
    if (rrmtb != NULL) {
        free(rrmtb);
        rrmtb = NULL;
    }
    if (sysmem != NULL) {
        free(sysmem);
        sysmem = NULL;
    }
    if (smi_tb != NULL) {
        free(smi_tb);
        smi_tb = NULL;
    }
    
    executed_acp_init = false;
    
    fprintf(stderr, "%d acp_abort: %s\n", acp_rank(), str);
#ifdef DEBUG
    fprintf(stdout, "%d internal acp_abort fin\n", acp_rank());
    fflush(stdout);
#endif
    abort(); 
}

double iacp_wtime(){
    struct timeval t;
    
    gettimeofday(&t, NULL);
    
    return (double)t.tv_sec + (double)t.tv_usec * 1e-6;
}

long iacp_memsize(){
    struct rusage ru;
    int rc;
    
    rc = getrusage(RUSAGE_SELF, &ru);
    if (rc) {
        perror("iapc_memsize: error getrusage");
        return -1;
    }
    return ru.ru_maxrss;
}

int iacp_internal_sync(void){
    
    int i; /* general index */
    char dummy1, dummy2; /* dummy buffer */
    int nprocs; /* my rank ID */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal iacp_internal_sync\n", acp_rank());
    fflush(stdout);
#endif
    
    /* get # of processes */
    nprocs = acp_procs();
    
    /* if nprocs <= 0, error */
    if ( nprocs <= 0 ) {
        return -1;
    }
    if ( nprocs >= 2 ) { /* if nprocs >= 2, */
        for ( i = 0; i < nprocs; i++ ) {
            if ( write( sock_connect, &dummy1, sizeof( char ) ) < 0 ) {
                perror( "iacp_internal_sync error: failed to write\n" );
                return -1;
            }
            if ( recv( sock_accept, &dummy2, sizeof( char ), 0 ) < 0 ) {
                perror( "iacp_internal_sync error: failed to recv\n" );
                return -1;
            }
        }
    }
    
#ifdef DEBUG
    fprintf( stdout, "%d: internal iacp_internal_sync fin\n", acp_rank() );
    fflush( stdout );
#endif
    
    return 0;
}

int iacp_init_rcdbsync()
{
    int i, myrank, procs, size, numstepga, thr, rem, offset, target;
    acp_ga_t stga;
    acp_ga_t *stla;
    acp_handle_t h;

    myrank = acp_rank();
    procs = acp_procs();

    if ( sync_status != ST_SYNC_INIT ) {
        fprintf( stderr, "%d: iacp_init_rcdbsync : Error. init sync called more than once\n", myrank );
        return -1;
    }

    i = 1; 
    sync_nsteps = 0;
    while ( i < procs ) {
        i <<= 1;
        sync_nsteps++;
    }

    if ( i != procs ) // there are remaining processes
        sync_nsteps--;
    thr = 1 << sync_nsteps; // threashold of remaining processes
    rem = procs - thr; // number of remaining processes

    if ( myrank < rem ) {
        sync_myrole = PARENT;
        size = sync_nsteps + 2;
        numstepga = sync_nsteps + 1;
        sync_peer = myrank + thr;
    }
    else if ( myrank >= thr ) {
        sync_myrole = REMAIN;
        size = 2;
        numstepga = 1;
        sync_peer = myrank - thr;
    }
    else {
        sync_myrole = NORMAL;
        size = sync_nsteps + 1;
        numstepga = sync_nsteps;
    }
    
    /* call temporal function to setup memory region for rcdbsync */
    sync_bodyla = ( uint64_t * ) malloc( size * sizeof( uint64_t ) );
    if ( sync_bodyla == NULL ) {
        fprintf( stderr, "%d: iacp_init_rcdbsync : Error. Malloc sync_bodyla failed \n", myrank );
        return -1;
    }
    sync_bodykey = acp_register_memory( ( void * ) sync_bodyla, size * sizeof( uint64_t) , 0 );
    if ( sync_bodykey == ACP_ATKEY_NULL ) {
        fprintf( stderr, "%d: iacp_init_rcdbsync : Error. Register failed \n", myrank );
        goto Error1;
    }
    sync_bodyga = acp_query_ga( sync_bodykey, ( void * ) sync_bodyla );
    if ( sync_bodyga == ACP_GA_NULL ) {
        fprintf( stderr, "%d: iacp_init_rcdbsync : Error. Query_GA failed\n", myrank );
        goto Error2;
    }
    sync_stepgatbl = ( acp_ga_t * )malloc( numstepga * sizeof( acp_ga_t ) );
    if ( sync_stepgatbl == NULL ) {
        fprintf( stderr, "%d: iacp_init_rcdbsync : Error. Malloc sync_stepgatbl failed\n", myrank );
        goto Error2;
    }
    
    for ( i = 0; i < size; i++ ) {
        sync_bodyla[i] = 0LL;
    }
    
    /* Exchange GAs: */
    stga = acp_query_starter_ga( myrank );
    stla = ( acp_ga_t * )acp_query_address( stga );
    stla[0] = sync_bodyga;
    
#ifdef DEBUG
    fprintf( stderr, "%d: sync info bodyla %p bodyga %lx bodykey %lx numstepga %d thr %d rem %d nsteps %d myrole %d\n", 
             myrank, sync_bodyla, sync_bodyga, sync_bodykey, numstepga, thr, rem, sync_nsteps, sync_myrole );
#endif
    
    iacp_internal_sync();
    
    if ( ( sync_myrole == PARENT ) || ( sync_myrole == NORMAL ) ) {
        offset = 1;
        for ( i = 0; i < sync_nsteps; i++ ) {
            target = myrank ^ offset;
            do {
                h = acp_copy( stga + ( i + 1 ) * sizeof( acp_ga_t ),
                              acp_query_starter_ga( target ), sizeof( acp_ga_t ), ACP_HANDLE_NULL );
            } while ( h == ACP_HANDLE_NULL );
            offset <<= 1;
        }
        acp_complete( ACP_HANDLE_ALL );
        for ( i = 0; i < sync_nsteps; i++ ) {
            sync_stepgatbl[i] = stla[i+1] + ( i + 1 ) * sizeof( uint64_t );
        }
        
        if ( sync_myrole == PARENT ) {
            target = myrank + thr;
            do {
                h = acp_copy( stga + sizeof( acp_ga_t ), 
                              acp_query_starter_ga( target ), sizeof( acp_ga_t ), ACP_HANDLE_NULL );
            } while ( h == ACP_HANDLE_NULL );
            acp_complete( ACP_HANDLE_ALL );
            sync_stepgatbl[sync_nsteps] = stla[1] + sizeof( uint64_t );
        }
    } else {
        target = myrank - thr;
        do {
            h = acp_copy( stga+sizeof( acp_ga_t ), 
                         acp_query_starter_ga( target ), sizeof( acp_ga_t ), ACP_HANDLE_NULL );
        } while ( h == ACP_HANDLE_NULL );
        acp_complete( ACP_HANDLE_ALL );
        sync_stepgatbl[0] = stla[1] + ( sync_nsteps + 1 ) * sizeof( uint64_t );
    }
    
    iacp_internal_sync();
    
    sync_status = ST_SYNC_IDLE;
    
    return 0;

Error3:
    if ( NULL != sync_stepgatbl ) {
        free( sync_stepgatbl );
    }
    
Error2:
    acp_unregister_memory( sync_bodykey );

Error1:
    if ( NULL != sync_bodyla ) {
        free( (void *) sync_bodyla );
    }
}

int iacp_start_rcdbsync()
{
    int myrank;
    acp_handle_t h;

    myrank = acp_rank();

    if ( sync_status != ST_SYNC_IDLE ) {
        fprintf( stderr, "%d: iacp_start_rcdbsync : Error. Irregular status %d\n", myrank, sync_status );
        return -1;
    }
    
    sync_status = ST_SYNC_BUSY;

   sync_bodyla[0]++;
    
    if ( ( sync_myrole == NORMAL ) || ( sync_myrole == REMAIN ) ) {
        do {
            h = acp_copy( sync_stepgatbl[0], sync_bodyga, sizeof( uint64_t ), ACP_HANDLE_NULL );
        } while ( h == ACP_HANDLE_NULL );
    }
    
    return 0;
}

int iacp_wait_rcdbsync()
{
    int i, myrank;
    uint64_t curr_idx;
    acp_handle_t h;
    
    myrank = acp_rank();
    
    curr_idx = sync_bodyla[0];
    
    if ( sync_status != ST_SYNC_BUSY ) {
        fprintf( stderr, "%d: iacp_wait_rcdbsync : Error. Irregular status %d\n", myrank, sync_status );
        return -1;
    }

    switch ( sync_myrole ) {
    case PARENT:
        while ( sync_bodyla[sync_nsteps+1] < curr_idx );
        
        for ( i = 0; i < sync_nsteps; i++ ) {
            do {
                h = acp_copy( sync_stepgatbl[i], sync_bodyga, sizeof( uint64_t ), ACP_HANDLE_NULL );
            } while ( h == ACP_HANDLE_NULL );
            
            while ( sync_bodyla[i+1] < curr_idx );
        }
        
        do {
            h = acp_copy( sync_stepgatbl[sync_nsteps], sync_bodyga, sizeof(uint64_t), ACP_HANDLE_NULL );
        } while ( h == ACP_HANDLE_NULL );
        break;
    case NORMAL:
        while ( sync_bodyla[1] < curr_idx );
        for ( i = 1; i < sync_nsteps; i++ ) {
            do {
                h = acp_copy( sync_stepgatbl[i], sync_bodyga, sizeof(uint64_t), ACP_HANDLE_NULL );
            } while ( h == ACP_HANDLE_NULL );
            while ( sync_bodyla[i+1] < curr_idx );
        }
        break;
    case REMAIN:
        while ( sync_bodyla[1] < curr_idx );
        break;
    default:
        fprintf( stderr, "%d: iacp_wait_rcdbsync : Error. Wrong role %d\n", sync_myrole );
    }
    acp_complete( ACP_HANDLE_ALL );
    
    sync_status = ST_SYNC_IDLE;
    
    return 0;
}

int iacp_free_rcdbsync()
{
    int myrank = acp_rank();
    
    if ( sync_status != ST_SYNC_IDLE ) {
        fprintf( stderr, "%d: iacp_free_rcdbsync : Error. Irregular status %d\n", myrank, sync_status );
        return -1;
    }
    
    acp_unregister_memory( sync_bodykey );
    
    if ( NULL != sync_bodyla ) {
        free( (void *) sync_bodyla );
    }
    if ( NULL != sync_stepgatbl ){
        free( sync_stepgatbl );
    }
}

int acp_sync( void )
{
    if ( acp_procs() == 1 ) return 0;
    
    iacp_start_rcdbsync();
    iacp_wait_rcdbsync();
    
    return 0;
}        

int acp_colors(void){
    
    return num_devices; /* # of devices */  
}

int acp_rank(void){
    
    return acp_myrank; /* my rank id */
}

int acp_procs(void){
    
    return acp_numprocs; /* # of processes */
}

int acp_query_rank(acp_ga_t ga){
    
    int rank; /* rank of ga */
    
    rank = (int)((ga >> (COLOR_BITS + GMTAG_BITS + OFFSET_BITS)) & RANK_MASK);
    rank--;
    
#ifdef DEBUG_L3
    fprintf(stdout, "%d: ga %lx rank %d\n", acp_rank(), ga, rank);
    fflush(stdout);
#endif
    
    return rank;
}

int acp_query_color(acp_ga_t ga){
    
    int color; /* color of ga */
    
    color = (uint32_t)((ga >> (GMTAG_BITS + OFFSET_BITS)) & COLOR_MASK);
    
#ifdef DEBUG_L3
    fprintf(stdout, "%d: ga %lx color %d\n", acp_rank(), ga, color);
    fflush(stdout);
#endif
    
    return color;
}

static inline uint32_t query_gmtag(acp_ga_t ga){
    
    uint32_t gmtag; /* general tag global memory */
    
    gmtag = (uint32_t)((ga >> OFFSET_BITS) & GMTAG_MASK);
    
#ifdef DEBUG_L3
    fprintf(stdout, "%d: ga %lx gmtag %d\n", acp_rank(), ga, gmtag);
    fflush(stdout);
#endif
    
    return gmtag;
}

static inline uint64_t query_offset(acp_ga_t ga){
    
    uint64_t offset; /* offset of ga */
    
    offset = (uint64_t)(ga & OFFSET_MASK);
    
#ifdef DEBUG_L3
    fprintf(stdout, "%d: ga %lx offset %lx\n", acp_rank(), ga, offset);
    fflush(stdout);
#endif
    
    return offset;
}

acp_ga_t acp_query_starter_ga(int rank){
    
    acp_ga_t ga; /* global address */
    uint32_t gmtag = TAG_SM; /* general tag of startar memory */
    uint32_t color = 0; /* color is 0 on starter memory */
    
    /* initialzie ga */
    ga = ACP_GA_NULL;
    
    /* set ga */
    ga = ((uint64_t)(rank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)gmtag << OFFSET_BITS);
    
#ifdef DEBUG_L3
    fprintf(stdout, "%d: rank %d starter memory ga %lx\n", 
            acp_rank(), rank, ga);
    fflush(stdout);
#endif
    
    return ga;
}

acp_ga_t iacp_query_starter_ga_dl(int rank){
    
    acp_ga_t ga; /* global address */
    uint32_t gmtag = TAG_SM; /* general tag of startar memory */
    uint32_t color = 0; /* color is 0 on starter memory */
    uint64_t offset; /* offset of ga */
    
    /* initialzie ga */
    ga = ACP_GA_NULL;
    
    /* set ga */
    offset = offset_acp_buf_dl;
    ga = ((uint64_t)(rank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)gmtag << OFFSET_BITS)
        + offset;
    
#ifdef DEBUG_L3
    fprintf(stdout, "%d: rank %d starter memory ga %lx\n", 
            acp_rank(), rank, ga);
    fflush(stdout);
#endif
    
    return ga;

}

acp_ga_t iacp_query_starter_ga_cl(int rank){
  
    acp_ga_t ga; /* global address */
    uint32_t gmtag = TAG_SM;/* general tag of startar memory */
    uint32_t color = 0;/* color is 0 on starter memory */
    uint64_t offset; /* offset of ga */
    
    /* initialzie ga */
    ga = ACP_GA_NULL;
    
    offset = offset_acp_buf_cl;
    /* set ga */
    ga = ((uint64_t)(rank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)gmtag << OFFSET_BITS)
        + offset;
    
#ifdef DEBUG_L3
    fprintf(stdout, "%d: rank %d starter memory ga %lx\n", 
            acp_rank(), rank, ga);
    fflush(stdout);
#endif
  
    return ga;
}

acp_ga_t acp_query_ga(acp_atkey_t atkey, void* addr){
  
    acp_ga_t ga; /* global address */
    uint64_t offset = 0; /* 0: offset of ga */
    uint32_t gmtag = 0; /* 0: tag of ga */
    int keyrank; /* rank of atkey */
    int myrank; /* rank id */
    
    /* initialize ga */
    ga = ACP_GA_NULL;
    
    /* get my rank */
    myrank = acp_rank();
    /* rank of atkey */
    keyrank = atkey >> (COLOR_BITS + GMTAG_BITS + OFFSET_BITS) & RANK_MASK;
    keyrank--;
    /* index of atkey */
    gmtag = (atkey >> OFFSET_BITS) & GMTAG_MASK;
    
    /* if my rank is equal to key rank */
    if (keyrank == myrank) {
        /* tag of atkey is not active */
        if (lrmtb[gmtag].valid == false) {
            return ACP_GA_NULL;
        }
        else {
#ifdef DEBUG
            fprintf(stdout, 
                    "%d: acp_query_ga key %lx, addr %p, rank %d gmtag %d faddr %p\n", 
                    myrank, atkey, addr, keyrank, gmtag, lrmtb[gmtag].addr);
            fflush(stdout);
#endif
            /* calc offset */
            offset = (char *)addr - (lrmtb[gmtag].addr) ;
            if (offset > lrmtb[gmtag].size){
                fprintf(stderr, "%d: acp_query_ga error: offset surpass memory region size.\n", myrank);
                return ACP_GA_NULL;
            }
            /* set ga. atkey is ga of offset 0 */
            ga = atkey + offset;
            
#ifdef DEBUG
            fprintf(stdout, "%d :rank %d acp_query_ga ga %lx\n", 
                    acp_rank(), myrank, ga);
            fflush(stdout);
#endif
            return ga;
        }
    }
    /* key rank is not equal to my rank */
    else {
        return ACP_GA_NULL;
    }
}

acp_atkey_t acp_register_memory(void* addr, size_t size, int color){
  
    int i; /* general index */
    
    struct ibv_mr *mr; /* memory register return data */
    int mr_flags; /* memory register flag */
    
    acp_atkey_t key; /* memory register key */
    uint32_t gmtag; /* tag in key */
    char found_empty_tag; /* found empty tag flag */
    char inserted_tag; /* inserted tag flag */
    int inst_tag_id; /* tag id will be inserted */
    volatile int nprocs; /* # of processes */
    
    acp_ga_t dst, src; /* ga of dst and src */
    int myrank; /* my rank ID */
    
    acp_handle_t handle; /* acp handle */

#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_register_memoery\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if color < 0 && color >= acp_colors(), return ACP_GA_NULL.*/
    if (color < 0 && color >= acp_colors()){
#ifdef DEBUG
        fprintf(stdout, "%d: color is not exist.\n", acp_rank());
        fflush(stdout);
#endif
        return ACP_ATKEY_NULL;
    }
    
    /* get myrank and nprocs */
    myrank = acp_rank();
    nprocs = acp_procs();
    
    /* set memory register flags */
    mr_flags = IBV_ACCESS_LOCAL_WRITE | 
        IBV_ACCESS_REMOTE_READ |
        IBV_ACCESS_REMOTE_WRITE |
        IBV_ACCESS_REMOTE_ATOMIC;
    
    /* execute register memory */
    mr = ibv_reg_mr(res.pd, addr, size, mr_flags);
    if (mr == NULL) {
#ifdef DEBUG
        perror("ibv_reg_mr is failed:");
#endif
        return ACP_ATKEY_NULL;
    }
#ifdef DEBUG
    fprintf(stdout, "%d: mr address %p size %ld\n", myrank, mr, sizeof(mr));
    fflush(stdout);
#endif
    if (NULL == mr) {
        fprintf(stderr, "%d acp_register_memory error\n", acp_rank());
        return ACP_ATKEY_NULL;
    }
#ifdef DEBUG
    fprintf(stdout, 
            "%d: MR was registered with addr=%lx, lkey=%u, rkey=%u, flags=%u\n",
            myrank, (uintptr_t)addr, mr->lkey, mr->rkey, mr_flags);
    fflush(stdout);
#endif
    
    /* search invalid tag, and set tag */
    found_empty_tag = false; /* initialze a founed empty tag flag */
    inserted_tag = false; /* initialize a inserted tag flag  */
    for (i = 0;i < MAX_RM_SIZE;i++) {
        if (lrmtb[i].valid == false) { /* found empty tag */
            found_empty_tag = true;
            inst_tag_id = i;
            if (lrmtb[i].lock == false) { /* find insertable tag */
                inserted_tag = true; /* inserted  */
                libvmrtb[i] = mr;
                lrmtb[i].rank = myrank;
                lrmtb[i].rkey = mr->rkey;
                lrmtb[i].addr = addr;
                lrmtb[i].size = size;
                lrmtb[i].valid = true;
                lrmtb[i].lock = true; /* locked this tag */
#ifdef DEBUG
                fprintf(stdout, 
                        "%d: insert data into lrmtb[%d] addr %p size %lu, lock flag %lu valid %lu \n", 
                        myrank, i, lrmtb[i].addr, lrmtb[i].size, lrmtb[i].lock, lrmtb[i].valid);
                fflush(stdout);
#endif
                break;
            }
        }
    }
    
    /* empty tag is not found */
    if (found_empty_tag == false) {
        ibv_dereg_mr(mr);
        fprintf(stdout, "%d: internal acp_register fin\n", myrank);
        return ACP_ATKEY_NULL;
    }
    else if (found_empty_tag == true && inserted_tag == false) {
        /* tell to flush rkey cache */
        for (i = 0;i < nprocs;i++) {
	    if ( true == rrm_get_flag_tb[i]) {
                dst = acp_query_starter_ga(i);
                src = acp_query_starter_ga(myrank);
                handle = acp_copy(dst + offset_rrm_reset_flag_tb + sizeof(char) * myrank,
                                  src + offset_rrm_get_flag_tb + sizeof(char) * i, 
                                  sizeof(char), ACP_HANDLE_NULL);
            }
        }
        /* complete until handle */
        acp_complete(handle);
        
        /* wait untitl get rrm flag table == ack rrm flag table */
        while (1) {
#ifdef DEBUG
            fprintf(stdout, "%d: getflag ", acp_rank());
            for (i = 0;i < nprocs;i++) {
                fprintf(stdout, "%d ", rrm_get_flag_tb[i]);
            }
            fprintf(stdout, "\n");
            fprintf(stdout, "%d: ackflag ", acp_rank());
            for (i = 0;i < nprocs;i++) {
                fprintf(stdout, "%d ", rrm_ack_flag_tb[i]);
            }
            fprintf(stdout, "\n");
            fflush(stdout);
	    fprintf(stdout, "%d: resetflag ", acp_rank());
            for (i = 0;i < nprocs;i++) {
	      fprintf(stdout, "%d ", rrm_reset_flag_tb[i]);
            }
            fprintf(stdout, "\n");
            fflush(stdout);
#endif
            if ( 0 == memcmp( (char *)rrm_get_flag_tb, (char *)rrm_ack_flag_tb, sizeof(char) * nprocs ) ) {
#ifdef DEBUG
	      fprintf(stdout, "%d: a getflag ", acp_rank());
	      for (i = 0;i < nprocs;i++) {
		fprintf(stdout, "%d ", rrm_get_flag_tb[i]);
	      }
	      fprintf(stdout, "\n");
	      fprintf(stdout, "%d:a ackflag ", acp_rank());
	      for (i = 0;i < nprocs;i++) {
		fprintf(stdout, "%d ", rrm_ack_flag_tb[i]);
	      }
	      fprintf(stdout, "\n");
	      fflush(stdout);
	      fprintf(stdout, "%d: resetflag ", acp_rank());
	      for (i = 0;i < nprocs;i++) {
		fprintf(stdout, "%d ", rrm_reset_flag_tb[i]);
	      }
	      fprintf(stdout, "\n");
	      fflush(stdout);
#endif
	      break;
            }
        }
        
        /* initialization rrm flag table */
	/* ack de get wo update suru. */
	for ( i = 0; i < nprocs; i++ ) {
	  if ( true == rrm_ack_flag_tb[i] ) {
	    rrm_get_flag_tb[i] = 0;
	    rrm_ack_flag_tb[i] = 0;
	  }
	}
        /*
	  memset((char *)rrm_get_flag_tb, 0, sizeof(char) * nprocs);
	  memset((char *)rrm_ack_flag_tb, 0, sizeof(char) * nprocs);
	*/
	/* enable empty tag */
        for (i = 0;i < MAX_RM_SIZE;i++) {
            if (lrmtb[i].valid == false) {
                lrmtb[i].lock = false;
            }
        }
    }
    /* insert new memory region */
    if (lrmtb[inst_tag_id].lock == false) {
        libvmrtb[inst_tag_id] = mr;
        lrmtb[inst_tag_id].rkey = mr->rkey;
        lrmtb[inst_tag_id].addr = addr;
        lrmtb[inst_tag_id].size = size;
        lrmtb[inst_tag_id].valid = true;
        lrmtb[inst_tag_id].lock = true;

#ifdef DEBUG
        fprintf(stdout, 
                "%d: insert data into lrmtb[%d] addr %p size %lu, lock flag %lu valid %lu lock %lu\n", 
                myrank, inst_tag_id, lrmtb[inst_tag_id].addr, lrmtb[inst_tag_id].size, lrmtb[inst_tag_id].lock, lrmtb[inst_tag_id].valid, lrmtb[inst_tag_id].lock);
        fflush(stdout);
#endif
    }

    /* set acp atkey for new memory region */
    gmtag = inst_tag_id;
    key = ((uint64_t)(myrank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)gmtag << OFFSET_BITS);

#ifdef DEBUG
        fprintf(stdout, "%d: atkey %lx gmtag %d\n", myrank, key, gmtag);
        fprintf(stdout, "%d: internal acp_register fin\n", myrank);
        fflush(stdout);
#endif
    return key;
}
    
int acp_unregister_memory(acp_atkey_t atkey){
    
    int rank; /* rank of atkey */
    uint32_t gmtag; /* tag of atkey */
    uint32_t color; /*color of atkey */

#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_unregister_memory\n", acp_rank());
#endif
    /* get global memory tag of atkey */
    gmtag = query_gmtag(atkey);
    
    /* get rank of atkey */
    rank = acp_query_rank(atkey);
    if (rank == -1){
        return -1;
    }
    
    /* get color of atkey */
    color = acp_query_color(atkey);
    
    /* if starter memory */
    if (gmtag == TAG_SM) {
        return -1;
    }
    else { /* if global memory */
        if (lrmtb[gmtag].valid == true) {
#ifdef DEBUG
            fprintf(stdout, 
                    "%d: atkey %llx unregister tag %d valid %d\n", 
                    acp_rank(), atkey, gmtag, lrmtb[gmtag].valid);
#endif
            lrmtb[gmtag].valid = false;
            ibv_dereg_mr(libvmrtb[gmtag]);
            libvmrtb[gmtag] = NULL;
            return 0;
        }
        else {
            return -1;
        }
    }
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_unregister_memory fin\n", acp_rank());
#endif

}

void *acp_query_address(acp_ga_t ga){
  
    char *addr; /* return address */
    char *base_addr; /* base address of ga */
    
    int rank; /* rank of ga */
    uint64_t offset; /* offset of ga */ 
    uint32_t gmtag; /* tag of ga */
    uint32_t color; /* color of ga */
    
    /* get rank, gm tag, offset, color from ga */
    gmtag = query_gmtag(ga);
    rank = acp_query_rank(ga);
    offset = query_offset(ga);
    color = acp_query_color(ga);

    /* print tag id */
#ifdef DEBUG_L3
    fprintf(stdout, "%d, gmtag = %d\n", acp_rank(), gmtag);
    fflush(stdout);
#endif
    /* if rank of ga is my rank, */
    if (rank == acp_rank()) {
        /* this ga tag is TAG_SM,  */
        if (gmtag == TAG_SM) {
            base_addr = sysmem;
        }
        /* this ga tag is general global memory */
        else {
            if (lrmtb[gmtag].valid == true) {
                base_addr = (char *)lrmtb[gmtag].addr;
#ifdef DEBUG_L3
                fprintf(stdout, 
                        "%d: LRMTB: tag %d addr %p size %lu rkey %lu valid %lu \n",
                        acp_rank(), gmtag, lrmtb[gmtag].addr, lrmtb[gmtag].size, 
                        lrmtb[gmtag].rkey, lrmtb[gmtag].valid);
                fflush(stdout);
#endif
            }
            else { /* this ga tag is not registered */
#ifdef DEBUG_L3
                fprintf(stdout, 
                        "%d: LRMTB: tag %d addr %p size %lu rkey %lu valid %lu \n",
                        acp_rank(), gmtag, lrmtb[gmtag].addr, lrmtb[gmtag].size, 
                        lrmtb[gmtag].rkey, lrmtb[gmtag].valid);
                fflush(stdout);
#endif
                return NULL;
            }
        }
    }
    else { /* this ga tag is not in local rank */
        return NULL;
    }
    /* set address */
    addr = base_addr + offset;
    
    return addr;
}

acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, size_t size, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
  
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_copy\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
        
    /* check my rank */
    myrank = acp_rank();
    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = (CMD *)&cmdq[tail4c];
 
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = COPY;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.copy_cmd.size = size;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;

#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx size = %lu\n", 
            myrank, tail, tail4c, pcmdq->wr_id, pcmdq->cmde.copy_cmd.size);
    fflush(stdout);
#endif
  
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_copy fin\n", myrank);
    fflush(stdout);
#endif
  
    return hdl;
}

acp_handle_t acp_cas4(acp_ga_t dst, acp_ga_t src, uint32_t oldval, uint32_t newval, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
  
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_cas4\n", acp_rank());
    fflush(stdout);
#endif

    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );

    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = CAS4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.cas4_cmd.data1 = oldval;
    pcmdq->cmde.cas4_cmd.data2 = newval;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx, data1 %u, data2 %u\n",
            myrank, tail, tail4c, pcmdq->wr_id,  pcmdq->cmde.cas4_cmd.data1, pcmdq->cmde.cas4_cmd.data2) ;
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_cas4 fin\n", myrank);
    fflush(stdout);
#endif
  
    return hdl;
}

acp_handle_t acp_cas8(acp_ga_t dst, acp_ga_t src, uint64_t oldval, uint64_t newval, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_cas8\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
    
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = CAS8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.cas8_cmd.data1 = oldval;
    pcmdq->cmde.cas8_cmd.data2 = newval;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;

#ifdef DEBUG
    fprintf(stdout, "%d: tail %lx cmdq[%lx].wr_id = %lx data1 %lu data2 %lu\n", 
            acp_rank(), tail, tail4c, pcmdq->wr_id,   pcmdq->cmde.cas8_cmd.data1, pcmdq->cmde.cas8_cmd.data2);
    fflush(stdout);
#endif
  
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_cas8 fin\n", acp_rank());
    fflush(stdout);
#endif
    
    return hdl;
}

acp_handle_t acp_swap4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_swap4\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
        
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }
   
    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = SWAP4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %u\n", 
            myrank, tail, tail4c, pcmdq->wr_id,  pcmdq->cmde.atomic4_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d, internal acp_swap4 fin\n", myrank);
    fflush(stdout);
#endif
    
    return hdl;
}

acp_handle_t acp_swap8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c; /* tail of cmdq */
    int myrank; /* my rank */
    CMD *pcmdq; /* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_swap8\n");
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
        
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
  
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = SWAP8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx valude %lu\n", 
            myrank, tail, tail4c, pcmdq->wr_id,   pcmdq->cmde.atomic8_cmd.data);
    fflush(stdout);
#endif
  
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_swap8 fin\n", myrank);
    fflush(stdout);
#endif
    
    return hdl;
}

acp_handle_t acp_add4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_add4\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
    
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = ADD4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %u\n", 
            myrank, tail, tail4c, pcmdq->wr_id,   pcmdq->cmde.atomic4_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_add4 fin\n", myrank);
    fflush(stdout);
#endif
  
    return hdl;
}

acp_handle_t acp_add8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_add8\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
        
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = ADD8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %lu\n", 
            myrank, tail, tail4c, pcmdq->wr_id,   pcmdq->cmde.atomic8_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_add8 fin\n", myrank);
    fflush(stdout);
#endif
  
    return hdl;
}

acp_handle_t acp_xor4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_xor4\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
    
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = XOR4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %u\n", 
            myrank, tail, tail4c, pcmdq->wr_id, pcmdq->cmde.atomic4_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_xor4 fin\n", myrank);
    fflush(stdout);
#endif
    
    return hdl;
}

acp_handle_t acp_xor8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_xor8\n", acp_rank());
    fflush(stdout);
#endif

    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
    
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = XOR8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %lu \n", 
            myrank, tail, tail4c, pcmdq->wr_id, pcmdq->cmde.atomic8_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_xor8 fin\n", myrank);
    fflush(stdout);
#endif
    
    return hdl;
}

acp_handle_t acp_or4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_or4\n", acp_rank());
    fflush(stdout);
#endif

    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
    
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = OR4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %u\n", 
            myrank, tail, tail4c, pcmdq->wr_id, pcmdq->cmde.atomic4_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_or4 fin\n", myrank);
    fflush(stdout);
#endif
  
    return hdl;
}

acp_handle_t acp_or8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_or8\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
    
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = OR8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %lu\n", 
            myrank, tail, tail4c, pcmdq->wr_id, pcmdq->cmde.atomic8_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_or8 fin\n", myrank);
    fflush(stdout);
#endif
    
    return hdl;
}

acp_handle_t acp_and4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_and4\n", acp_rank());
    fflush(stdout);
#endif

    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );
    
    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if(myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = AND4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %u\n", 
            myrank, tail, tail4c, pcmdq->wr_id, pcmdq->cmde.atomic4_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_and4 fin\n", myrank);
    fflush(stdout);
#endif
    
    return hdl;
}

acp_handle_t acp_and8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_and8\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if queue is full, wait issuing */
    while ( tail - head == MAX_CMDQ_ENTRY - 1 );

    /* check my rank */
    myrank = acp_rank();
    /* check myrank is equal to dst rank or not */
    if (myrank != acp_query_rank(dst)) {
        return ACP_HANDLE_NULL;
    }

    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = &cmdq[tail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = true;
    pcmdq->rank = myrank;
    pcmdq->type = AND8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tail;
    pcmdq->wr_id = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = true;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: tail %lx cmdq[%lx].wr_id = %lx value %lu\n", 
            myrank, tail, tail4c, pcmdq->wr_id, pcmdq->cmde.atomic8_cmd.data);
    fflush(stdout);
#endif
    
    /* update tail */
    tail++;
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_and8 fin\n", myrank);
    fflush(stdout);
#endif
    
    return hdl;
}

void acp_complete(volatile acp_handle_t handle){
  
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_complete\n", acp_rank()); 
    fflush(stdout);
#endif
    
    /* if cmdq have no command, return */
    if (head == tail) {
#ifdef DEBUG
        fprintf(stdout, "%d: acp_complet: cmdq is empty\n", acp_rank());
        fflush(stdout);
#endif
        return;
    }
    
    /* check handle */
    /* if disable handle, return */
    if (handle == ACP_HANDLE_NULL) {
#ifdef DEBUG
        fprintf(stdout, "%d: acp_complete: handle is ACP_HANDLE_NULL\n", acp_rank());
        fflush(stdout);
#endif
        return;
    }
    /* if handle is ACP_HANDLE_ALL or ACP_HANDLE_CONT, */
    /* wait complete all issued copy */
    if (handle == ACP_HANDLE_ALL || handle == ACP_HANDLE_CONT) {
        handle = tail - 1;
    }

    /* handle is finished  */
    if (handle < head) {
#ifdef DEBUG
        fprintf(stdout, "%d: acp_complete: handle is always finished head %lx handle %lx\n", acp_rank(), head, handle);
        fflush(stdout);
#endif
        return;
    }
    /* handle is not issued */
    if (handle >= tail) {
#ifdef DEBUG
        fprintf(stdout, "%d: acp_complete: handle is not issued handle %lx tail %lu\n", acp_rank(), handle, tail);
        fflush(stdout);
#endif
        return;
    }
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: acp_complete: head = %lx tail = %lx handle = %lx\n", 
            acp_rank(), head, tail, handle);
    fflush(stdout);
#endif
    /* check status of handle if it is COMPLETED or not. */
    while (head <= handle);
           
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_complete fin\n", acp_rank()); 
    fflush(stdout);
#endif
    
    return;
}

int acp_inquire(acp_handle_t handle){

#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_inquire\n", acp_rank()); 
    fflush(stdout);
#endif
    
    /* if cmdq have no command, return */
    if (head == tail) {
#ifdef DEBUG
        fprintf(stdout, "%d: acp_inquire: cmdq is empty\n", acp_rank());
        fflush(stdout);
#endif
        return 0;
    }
    /* if handle is ACP_HANDLE_ALL or ACP_HANDLE_CONT, */
    /*  wait complete all issued copy */
    if (handle == ACP_HANDLE_ALL || handle == ACP_HANDLE_CONT) {
        handle = tail - 1;
    }

    /* check */
    /* if disable handle, return */
    if (handle == ACP_HANDLE_NULL) {
#ifdef DEBUG
        fprintf(stdout, "%d: acp_inquire: handle is ACP_HANDLE_NULL\n", acp_rank());
        fflush(stdout);
#endif
        return 0;
    }
    /* handle is finished  */
    if (handle < head) {
#ifdef DEBUG
        fprintf(stdout, "%d: acp_inquire: handle is alway finished\n", acp_rank());
        fflush(stdout);
#endif
        return 0;
    }
    /* handle is not issued */
    if (handle >= tail) {
#ifdef DEBUG
        fprintf(stdout, "%d: acp_inquire: handle is not issed. handle %lx tail %lx\n", acp_rank(), handle, tail);
        fflush(stdout);
#endif
        return 0;
    }
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: acp_inquire: head = %lx tail = %lx handle = %lx\n", 
            acp_rank(), head, tail, handle);
    fflush(stdout);
#endif
    
    /* check status of handle if it is COMPLETED or not.*/ 
    if (head <= handle) {
#ifdef DEBUG
        fprintf(stdout, "%d: internal acp_inquire fin\n", acp_rank()); 
        fflush(stdout);
#endif
        return 1;
    }
    else {
#ifdef DEBUG
        fprintf(stdout, "%d:internal acp_inquire fin\n", acp_rank()); 
        fflush(stdout);
#endif
        return 0;
    }
}

/* get remote register memory table */
static inline int getlrm(uint64_t wr_id, int torank){
  
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    int rank; /* rank id */
    
    rank = acp_rank();

    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    
    /* set local buffer info  */
    sge.addr = (uintptr_t)recv_lrmtb; 
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(RM) * MAX_RM_SIZE;
    
#ifdef DEBUG
    fprintf(stdout, "%d: get lrm length %d\n", rank, sge.length);
    fflush(stdout);
#endif
  
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end or rcmdbuf end */
    sr.wr_id =  wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;

#ifdef DEBUG
    fprintf(stdout, "%d: get lrm sr.wr_id %lx wr_id %lx torank %d\n", rank, sr.wr_id, wr_id, torank);
    fflush(stdout);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_READ;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + offset_lrmtb;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    
#ifdef DEBUG
    fprintf(stdout, "%d: get lrm ibv_post_send return code = %d\n", rank, rc);
    fflush(stdout);
#endif
    
    return rc;
}

/* get remote register memory table */
static inline int writebackstat(uint64_t idx){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    uint64_t cmdqidx; /* index of cmdq from issued handle */
    int torank; /* target rank */
    
    /* target rank */
    torank = rcmdbuf[idx].rank;
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    
    /* set local buffer info  */
    sge.addr = (uintptr_t)finished_stat_buf;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(uint64_t);
    
#ifdef DEBUG
    fprintf(stdout, "%d: write back status length %d\n", acp_rank(), sge.length);
    fflush(stdout);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end*/
    sr.wr_id =  rcmdbuf[idx].wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: writeback status wr_id %lx handle %lx\n", 
            acp_rank(), sr.wr_id, rcmdbuf[idx].ishdl);
    fflush(stdout);
#endif
    
    /* get Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    cmdqidx = (rcmdbuf[idx].ishdl) % MAX_CMDQ_ENTRY;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: writeback status wr_id %lx handle %lx cmdqidx %lx torank %d, offset_stat %u, *finished_stat_buf %u \n",
            acp_rank(), sr.wr_id, rcmdbuf[idx].ishdl, 
            cmdqidx, torank, offset_stat, *finished_stat_buf);
    fflush(stdout);
#endif
    
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + offset_cmdq + sizeof(CMD) * cmdqidx + offset_stat;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
#if 0
    if (0 == rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
#endif

#ifdef DEBUG
    fprintf(stdout, 
            "%d: writeback cmdq  ibv_post_send return code = %d\n", 
            acp_rank(), rc);
    fflush(stdout);
#endif
    
    return rc;
}

static inline int gethead(acp_handle_t idx, int torank){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
   
    /* set local buffer info  */
    sge.addr = (uintptr_t)&cmdq[idx].head_buf;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(uint64_t);
#ifdef DEBUG
    fprintf(stdout, "%d: get head length %d\n", acp_rank(), sge.length);
    fflush(stdout);
#endif
  
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
  
    /* Work request ID is set by Acp_handle_queue end*/
    sr.wr_id =  cmdq[idx].wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "%d: get head sr.wr_id %lx wr_id %lx tail_buf %ld\n", acp_rank(), sr.wr_id, cmdq[idx].wr_id, cmdq[idx].tail_buf);
    fflush(stdout);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_READ;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + offset_rcmdbuf_head;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    
#ifdef DEBUG
    fprintf(stdout, "%d: get head ibv_post_send return code = %d\n", acp_rank(), rc);
    fflush(stdout);
#endif
    
    return rc;
}

static inline int gettail(acp_handle_t idx, int torank){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    
    /* set local buffer info  */
    sge.addr = (uintptr_t)&cmdq[idx].tail_buf;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(uint64_t);
    
#ifdef DEBUG
    fprintf(stdout, "%d: get tail length %d\n", acp_rank(), sge.length);
    fflush(stdout);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end */
    sr.wr_id =  cmdq[idx].wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "%d: get tail sr.wr_id %lx wr_id %lx\n", acp_rank(), sr.wr_id, cmdq[idx].wr_id);
    fflush(stdout);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
    sr.send_flags = IBV_SEND_SIGNALED;
   
    /* Set remote address and rkey in send work request */
    sr.wr.atomic.remote_addr = smi_tb[torank].addr + offset_rcmdbuf_tail;
    sr.wr.atomic.rkey = smi_tb[torank].rkey;
    sr.wr.atomic.compare_add = 1ULL;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    
#ifdef DEBUG
    fprintf(stdout, "%d: get tail ibv_post_send return code = %d\n", acp_rank(), rc);
    fflush(stdout);
#endif
    
    return rc;
}
 
static inline int putrrmgetedflag(uint64_t wr_id, int torank){  
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int myrank;/* my rank ID */
    int rc; /* return code */
    
    /* get my rank ID */
    myrank = acp_rank();
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
  
    /* set local buffer info */
    sge.addr = (uintptr_t)true_flag_buf;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(char);
    
#ifdef DEBUG
    fprintf(stdout, "%d: put rrm get flag length %d torank %d true %d\n", acp_rank(), sge.length, torank, *true_flag_buf);
    fflush(stdout);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end */
    sr.wr_id =  wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    
#ifdef DEBUG
    fprintf(stdout, "%d: put rrm get flag sr.wr_id %lx wr_id %lx\n", acp_rank(), sr.wr_id, wr_id);
    fflush(stdout);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + offset_rrm_get_flag_tb + sizeof(char) * myrank;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    
#ifdef DEBUG
    fprintf(stdout, "%d: put rrm get flag ibv_post_send return code = %d\n", acp_rank(), rc);
    fflush(stdout);
#endif
    
    return rc;
}

static inline int putrrmackflag(uint64_t wr_id, int torank){  
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
  
    int myrank;
    int rc; /* return code */
    
    /* get my rank ID */
    myrank = acp_rank();
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    /* if local tag point to starter memory */
    sge.addr = (uintptr_t)true_flag_buf;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(char);
    
#ifdef DEBUG
    fprintf(stdout, "%d: put rrm ack flag length %d true_flag %d torank %d\n", 
	    myrank, sge.length, *true_flag_buf, torank);
    fflush(stdout);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    /* Work request ID is set by ack_id */
    sr.wr_id =  wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    
#ifdef DEBUG
    fprintf(stdout, "%d: put rrm ack flag sr.wr_id %lx wr_id %lx\n", myrank, sr.wr_id, wr_id);
    fflush(stdout);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + offset_rrm_ack_flag_tb +  sizeof(char) * myrank;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    
#ifdef DEBUG
    fprintf(stdout, "%d: put rrm ack flag ibv_post_send return code = %d\n", myrank, rc);
    fflush(stdout);
#endif
    
    return rc;
}

static inline int putreplydata(acp_handle_t idx, int dstrank, int dstgmtag, uint64_t dstoffset, int flguint64){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));

    sge.addr = (uintptr_t)&rcmdbuf[idx].replydata;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    if (true == flguint64) {
        sge.length = sizeof(uint64_t);
#ifdef DEBUG
        fprintf(stdout, "%d: put replydata u64data %lu\n", acp_rank(), (uint64_t)rcmdbuf[idx].replydata);
        fflush(stdout);
#endif
    }
    else {
        sge.length = sizeof(uint32_t);
#ifdef DEBUG
        fprintf(stdout, "%d: put replydata u32data %u\n", acp_rank(), (uint32_t)rcmdbuf[idx].replydata);
        fflush(stdout);
#endif  
    }
#ifdef DEBUG
    fprintf(stdout, "%d: put replydata length %d\n", acp_rank(), sge.length);
    fflush(stdout);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by handle + 0x8000000000000000LLU */
    sr.wr_id =  rcmdbuf[idx].wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "%d: put replydata sr.wr_id %lx wr_id %lx\n", acp_rank(), sr.wr_id, rcmdbuf[idx].wr_id);
    fflush(stdout);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    /* using starter memory */
    if ( dstgmtag == TAG_SM) {
        sr.wr.rdma.remote_addr = smi_tb[dstrank].addr + dstoffset;
        sr.wr.rdma.rkey = smi_tb[dstrank].rkey;
    }
    /* using global memory */
    else {
        idx = dstrank % rkey_cache_size;
        sr.wr.rdma.remote_addr = (uintptr_t)(rrmtb[idx][dstgmtag].addr) + dstoffset;
        sr.wr.rdma.rkey = rrmtb[idx][dstgmtag].rkey;
    }
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[dstrank], &sr, &bad_wr);

#ifdef DEBUG
    fprintf(stdout, "%d: put replydata ibv_post_send return code = %d\n", acp_rank(), rc);
    fflush(stdout);
#endif
    return rc;
}

static inline int putcmd(acp_handle_t idx, int torank, uint64_t rcmdbid){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
        
    /* copy cmdq[index] into putcmdbuf */
    memcpy(&putcmdbuf[ idx], (const void *)&cmdq[idx], sizeof(CMD));
    putcmdbuf[idx].stat = CMD_UNISSUED;
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    /* local buffer info */
    sge.addr = (uintptr_t)&putcmdbuf[idx];
    sge.lkey = res.mr->lkey;
#ifdef DEBUG
    fprintf(stdout, "%d: putcmd rcmdbid %lx\n", acp_rank(), rcmdbid);
    fflush(stdout);
#endif
    /* set message size */
    sge.length = sizeof(CMD);
    
#ifdef DEBUG
    fprintf(stdout, "%d: putcmd length %d\n", acp_rank(), sge.length);
    fflush(stdout);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by ACP_handle queue end */
    sr.wr_id =  cmdq[idx].wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "%d: putcmd sr.wr_id %lx wr_id %lx\n", acp_rank(), sr.wr_id, cmdq[idx].wr_id);
    fflush(stdout);   
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;

    /* Set remote address and rkey in send work request */
    rcmdbid = rcmdbid % MAX_RCMDB_SIZE;
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + offset_rcmdbuf + sizeof(CMD) * rcmdbid;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
 
#ifdef DEBUG
    fprintf(stdout, "%d: putcmd ibv_post_send return code = %d\n", acp_rank(), rc);
    fflush(stdout);
#endif
    
    return rc;
}

static inline int icopy(uint64_t wr_id ,
                        int dstrank, uint32_t dstgmtag, uint64_t dstoffset, 
                        int srcrank, uint32_t srcgmtag, uint64_t srcoffset,
                        size_t size)
{
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL; /* return of send work reuqest */
    
    int torank = -1; /* remote rank */
    int rc; /* return code */
    
    uint64_t local_offset, remote_offset; /* local and remote offset */
    uint32_t local_gmtag, remote_gmtag; /* local and remote GMA tag */
    int myrank; /* my rank id */
    int idx; /* index for handle */

    /* get my rank*/
    myrank = acp_rank();
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal icopy\n", myrank);
    /* print ga rank, index, offset */
    fprintf(stdout, 
            "%d: sr %x st %x so %lx dr %x dt %x do %lx\n", 
            myrank, srcrank, srcgmtag, srcoffset, 
            dstrank, dstgmtag, dstoffset);
    fflush(stdout);
#endif
  
    /* set local/remote tag, local/remote offset and remote rank */
    if (myrank == srcrank) {
        local_offset = srcoffset;
        local_gmtag = srcgmtag;
        remote_offset = dstoffset;
        remote_gmtag = dstgmtag;
        torank = dstrank;
    }
    else if (myrank == dstrank) {
        local_offset = dstoffset;
        local_gmtag = dstgmtag;
        remote_offset = srcoffset;
        remote_gmtag = srcgmtag;
        torank = srcrank;
    }
#ifdef DEBUG
    fprintf(stdout, "%d: target rank %d\n", myrank, torank);
    fflush(stdout);
#endif
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));

    /* if local tag points to starter memory */
    if (local_gmtag == TAG_SM) {
        sge.addr = (uintptr_t)(sysmem) + local_offset;
        sge.lkey = res.mr->lkey;
    }
    else {
        /* if local tag does not point to starter memory */
        if (lrmtb[local_gmtag].valid == true) {
            sge.addr = (uintptr_t)(lrmtb[local_gmtag].addr) + local_offset;
            sge.lkey = libvmrtb[local_gmtag]->lkey;
        }
        else {
            rc = -1;
            return rc;
        }
        
    }
    
    /* set message size */
    sge.length = size;
   
#ifdef DEBUG
    fprintf(stdout, "%d: copy len %d\n", myrank, sge.length);
    fflush(stdout);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;

    /* Work request ID is set by Acp_handle_queue end */
    sr.wr_id =  wr_id;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    if (size == 0){
        sr.num_sge = 0;
    }
    
#ifdef DEBUG
    fprintf(stdout, "%d: icopy wr_id  %lx wr_id %lx\n", acp_rank(), sr.wr_id, wr_id);
    fflush(stdout);
#endif
    
    /* Set put opcode in send work request */
    if ( myrank == srcrank ) {
        sr.opcode = IBV_WR_RDMA_WRITE;
    }
    /* Set Get opcode in send work request */
    else if ( myrank == dstrank ) {
        sr.opcode = IBV_WR_RDMA_READ;
    }
    /* Set remote address and rkey in send work request */
    /* using starter memory */
    if ( remote_gmtag == TAG_SM ) {
        sr.wr.rdma.remote_addr = smi_tb[torank].addr + remote_offset;
        sr.wr.rdma.rkey = smi_tb[torank].rkey;
    }
    /* using global memory */
    else {
        idx = torank % rkey_cache_size;
        sr.wr.rdma.remote_addr = (uintptr_t)(rrmtb[idx][remote_gmtag].addr) + remote_offset;
        sr.wr.rdma.rkey = rrmtb[idx][remote_gmtag].rkey;
    }

#ifdef DEBUG    
    if (myrank == srcrank) {
        fprintf(stdout, "%d: put addr %lx rkey = %u\n", 
                acp_rank(), sr.wr.rdma.remote_addr, sr.wr.rdma.rkey);
        fflush(stdout);
    }
    else if (myrank == dstrank) {
        fprintf(stdout, "%d: get addr %lx rkey = %u\n", 
                acp_rank(), sr.wr.rdma.remote_addr, sr.wr.rdma.rkey);
        fflush(stdout);
    }
#endif

    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
        
#ifdef DEBUG
    fprintf(stdout, "%d: icopy ibv_post_send return code = %d\n", acp_rank(), rc);
    fprintf(stdout, "%d: internal icopy fin\n", acp_rank());
    fflush(stdout);
#endif
    
    return rc;
}

static inline void selectatomic(void *srcaddr, CMD *cmd){
    
    uint64_t *srcaddr8; /* 8 bytes src address */
    uint32_t *srcaddr4; /* 4 bytes src address */
    

#ifdef DEBUG
    fprintf(stdout, "%d: internal selectatomic\n", acp_rank());
    fflush(stdout);
#endif

    switch (cmd->type) {
    case CAS4:
        srcaddr4 = (uint32_t *)srcaddr;
        cmd->replydata = (uint32_t)sync_val_compare_and_swap_4(srcaddr4, cmd->cmde.cas4_cmd.data1, cmd->cmde.cas4_cmd.data2);
        break;
    case CAS8:
        srcaddr8 = (uint64_t *)srcaddr;
        cmd->replydata = sync_val_compare_and_swap_8(srcaddr8, cmd->cmde.cas8_cmd.data1, cmd->cmde.cas8_cmd.data2);
        break;
    case SWAP4:
        srcaddr4 = (uint32_t *)srcaddr;
        cmd->replydata = (uint32_t)sync_swap_4(srcaddr4, cmd->cmde.atomic4_cmd.data);
        break;
    case SWAP8:
        srcaddr8 = (uint64_t *)srcaddr;
        cmd->replydata = sync_swap_8(srcaddr8, cmd->cmde.atomic8_cmd.data);
        break;
    case ADD4:
        srcaddr4 = (uint32_t *)srcaddr;
        cmd->replydata = (uint32_t)sync_fetch_and_add_4(srcaddr4, cmd->cmde.atomic4_cmd.data);
        break;
    case ADD8:
        srcaddr8 = (uint64_t *)srcaddr;
        cmd->replydata = sync_fetch_and_add_8(srcaddr8, cmd->cmde.atomic8_cmd.data);
        break;
    case XOR4:
        srcaddr4 = (uint32_t *)srcaddr;
        cmd->replydata = (uint32_t)sync_fetch_and_xor_4(srcaddr4, cmd->cmde.atomic4_cmd.data);
        break;
    case XOR8:
        srcaddr8 = (uint64_t *)srcaddr;
        cmd->replydata = sync_fetch_and_xor_8(srcaddr8, cmd->cmde.atomic8_cmd.data);
        break;
    case OR4:
        srcaddr4 = (uint32_t *)srcaddr;
        cmd->replydata = (uint32_t)sync_fetch_and_or_4(srcaddr4, cmd->cmde.atomic4_cmd.data);
        break;
    case OR8:
        srcaddr8 = (uint64_t *)srcaddr;
        cmd->replydata = sync_fetch_and_or_8(srcaddr8, cmd->cmde.atomic8_cmd.data);
        break;
    case AND4:
        srcaddr4 = (uint32_t *)srcaddr;
        cmd->replydata = (uint32_t)sync_fetch_and_and_4(srcaddr4, cmd->cmde.atomic4_cmd.data);
        break;
    case AND8:
        srcaddr8 = (uint64_t *)srcaddr;
        cmd->replydata = sync_fetch_and_and_8(srcaddr8, cmd->cmde.atomic8_cmd.data);
        break;
    }
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal selectatomic fin cmd->replydata %u\n", acp_rank(), cmd->replydata);
    fflush(stdout);
#endif
    
    return;
}

static inline void check_cmdq_complete(uint64_t index){
    
    uint64_t idx; /* index for cmdq */
    
#ifdef DEBUG_L2
    fprintf(stdout, "%d: internal check_cmdq_complete\n", acp_rank());
    fflush(stdout);
#endif
    
    /* if after cmdq status is FINISHED,  */
    /* changr COMPLETE and update head to new index */
    /* which have a COMPLETE status. */
    idx = head % MAX_CMDQ_ENTRY;
    while (head <= tail) {
        /* if status FINISED */
        if (cmdq[idx].stat == FINISHED) {
            cmdq[idx].stat = COMPLETED;
            head++;
            idx = (idx + 1) % MAX_CMDQ_ENTRY;
#ifdef DEBUG
            fprintf(stdout, 
                    "%d: chcomp update idx %ld head %ld tail %ld\n", 
                    acp_rank(), idx, head, tail);
            fflush(stdout);
#endif
        }
        /* if status is not FINISED, break */
        else {
            break;
        }
    }
    
#ifdef DEBUG_L2
    fprintf(stdout, "%d: internal check_cmdq_complete fin\n", acp_rank());
    fflush(stdout);
#endif
}

static inline void rcmdbuf_update_head(){
    
    uint64_t idx; /* index for rcmdb */
    uint32_t count; /* check count of rcmdbuf elements */
    
    /* if after rcmd is not valid,  */
    /* update rcmdbuf_head */
#ifdef DEBUG
    fprintf(stdout, "%d: internal rcmdbuf_update_head\n", acp_rank()); 
    fflush(stdout);
#endif
    
    idx = *rcmdbuf_head % MAX_RCMDB_SIZE;
    count = 0;
    while (*rcmdbuf_head < *rcmdbuf_tail && count < MAX_RCMDB_SIZE) {
        /* if flag is valid */
        if (rcmdbuf[idx].valid_head == false && 
            rcmdbuf[idx].valid_tail == false && 
            rcmdbuf[idx].stat == CMD_WRITEBACK_FIN) {
            rcmdbuf[idx].stat = COMPLETED;
            (*rcmdbuf_head)++;
            idx = (idx + 1) % MAX_RCMDB_SIZE;
            count++;

#ifdef DEBUG
            fprintf(stdout, 
                    "%d: ch_rcmdbuf_comp update idx %ld rcmdbuf head %ld rcmdubuf tail %ld cnt %u\n", 
                    acp_rank(), idx, *rcmdbuf_head, *rcmdbuf_tail, count);
            fflush(stdout);
#endif
        }
        /* if flag is invalid, break */
        else {
            break;
        }
    }
    
#ifdef DEBUG
    fprintf(stdout, "%d: internal rcmdbuf_update_head fin\n", acp_rank()); 
    fflush(stdout);
#endif
}

static inline void setrrm(int torank){
  
    int i; /* general index */
    int idx; /* rkey index */

#ifdef DEBUG
    fprintf(stdout, "%d: internal setrrm\n", acp_rank()); 
    fflush(stdout);
#endif
    idx = torank % rkey_cache_size;
#ifdef DEBUG
    static int j = 0;
    j++;
    fprintf(stdout, "%d: set rrm table idx %d remote rank %d # of calling setrrm %d\n", acp_rank(), idx, torank, j); 
    fflush(stdout);
#endif
    /* set RM table */
    if (rrmtb[idx] == NULL) {
        rrmtb[idx] = (RM *)malloc(sizeof(RM) * MAX_RM_SIZE);
        if (rrmtb[idx] == NULL) {
            acp_abort("set rrm error\n");
        }
    }
    for (i = 0; i < MAX_RM_SIZE; i++) {
        rrmtb[idx][i].rank = recv_lrmtb[i].rank;
        rrmtb[idx][i].rkey = recv_lrmtb[i].rkey;
        rrmtb[idx][i].addr = recv_lrmtb[i].addr;
        rrmtb[idx][i].size = recv_lrmtb[i].size;
        rrmtb[idx][i].valid = recv_lrmtb[i].valid;
#ifdef DEBUG
        fprintf(stdout, 
                "%d: rrtb torank %d tag %d idx %d, rank %d, addr %p size %lu rkey %lu valid %lu\n", 
                acp_rank(), torank, i, idx, 
                rrmtb[idx][i].rank, rrmtb[idx][i].addr, 
                rrmtb[idx][i].size, rrmtb[idx][i].rkey, 
                rrmtb[idx][i].valid);
        fflush(stdout);
#endif
    }
#ifdef DEBUG
    fprintf(stdout, "%d: internal setrrm fin\n", acp_rank()); 
    fflush(stdout);
#endif
}

static void *comm_thread_func(void *dm){
  
    int rc; /* return code for cq */
    int ret; /* return code for ibv_post_send */
    int i, j; /* general index */
    
    struct ibv_wc wc; /* work completion for poll_cq */
    int myrank; /* my rank id */
    uint64_t idx; /* CMDQ index */
    acp_handle_t index; /* acp handle index */
    
    acp_ga_t src, dst; /* src and dst ga */
    size_t size; /* data size */
    int torank, dstrank, srcrank; /* target rank, dstga rank, srcga rank */
    uint32_t totag, dsttag, srctag; /* target tag, dst tag, src tag */
    uint64_t dstoffset, srcoffset; /* dst offset, src offset */
    int nprocs; /* # of processes */
    
    int count; /* check count for cmdq and rcmdbuf */
    int comp_cqe_flag = false; /* flag of completion of processing cqe */
    
    uint32_t rrmtb_idx; /* index of rrm table */
    uint32_t cur_rank; /* current of rank on rrmt table */
  
    
    uint64_t iter = 0, pre_iter = 0;
    uint64_t clk = 0, pre_clk = 0;
    int comm_work;
    int no_event_count;
    
#ifdef SETAFFINITY
    cpu_set_t mask;
    int cpuid;
    int cpumax;

    
    fprintf(stdout, "%d: getcpu %d\n", acp_rank(), sched_getcpu());
    
    rc = sched_getaffinity(0, sizeof(cpu_set_t), &mask);
    if (0 != rc) {
        perror("failed schecd_getaffinity");
        exit(1);
    }
    
#ifdef SETAFFINITY_LAST
    cpumax = CPU_COUNT(&mask);
    fprintf(stdout, "%d: getcpu %d maxcpu %d \n", acp_rank(), sched_getcpu(), cpumax);
    CPU_ZERO(&mask);
    cpuid = cpumax - 1;
#elif SETAFFINITY_FIRST
    CPU_ZERO(&mask);
    cpuid = 0;
#endif
    
    CPU_SET(cpuid, &mask);
    rc = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
    if (0 != rc) {
        perror("failed schecd_setaffinity");
        exit(1);
    }
    fprintf(stdout, "%d: setaffnity getcpu %d\n", acp_rank(), sched_getcpu());
#endif

    /* get my rank id */
    myrank = acp_rank();
    /* get # of rank */
    nprocs = acp_procs();
    comm_work = 0;
    no_event_count = 0;
    
    while (1) {
        if ( 0 == comm_work ) {
            if ( ECOUNT == no_event_count ){
                no_event_count = 0;
                sched_yield();
            }
            else {
                no_event_count ++;
            }
        } 
        else {
            comm_work = 0;
        }
        /* iter ++; */
        /* CHECK IB COMPLETION QUEUE section */
        rc = ibv_poll_cq(cq, 1, &wc);
        /* error of ibv poll cq*/
        if (rc < 0) {
            fprintf(stderr, "Fail poll cq\n");
            exit(-1);
        }
        /* get cqes */
        if (rc > 0) {
            comm_work ++;
            /* set index by cmdq head */
            index = head;
            if (wc.status == IBV_WC_SUCCESS) {
#ifdef DEBUG
                fprintf(stdout, "%d: qp section: wr_id %lx\n", myrank, wc.wr_id);
                fflush(stdout);
#endif
                /* when ibv_poll_cq is SUCCESS */
                if ((wc.wr_id & MASK_WRID_RCMDB) == 0) {
#ifdef DEBUG
                    fprintf(stdout, "%d: qp section: cmdq wr_id %lx mask %llx\n", 
                            myrank, wc.wr_id, wc.wr_id & MASK_WRID_RCMDB);
                    fflush(stdout);
#endif
                    comp_cqe_flag = false;
                    count = 0;
                    while (index < tail && comp_cqe_flag == false && count < MAX_CMDQ_ENTRY) {
                        /* check which acp handle command complete. */
                        idx = index % MAX_CMDQ_ENTRY;
                        if (cmdq[idx].wr_id == wc.wr_id) {
                            switch (cmdq[idx].stat) {
                            case ISSUED: /* issued gma command */
#ifdef DEBUG
                                fprintf(stdout, "%d: ISSUED\n", myrank);
                                fflush(stdout);
#endif
                                cmdq[idx].stat = FINISHED;
                                check_cmdq_complete(index);
                                comp_cqe_flag = true;
                                break;
                                
                            case FIRST_ISSUED: /* first issued gma command */
#ifdef DEBUG
                                fprintf(stdout, "%d: FIRST_ISSUED\n", myrank);
                                fflush(stdout);
#endif
				dstrank = acp_query_rank(cmdq[idx].gadst);
				srcrank = acp_query_rank(cmdq[idx].gasrc);
				if ( myrank == dstrank ){
				  torank = srcrank;
				}
				else if (myrank == srcrank ) {
				  torank = dstrank;
				}
#ifdef DEBUG
				fprintf(stdout, "%d: FIRST_ISSUED torank %d\n", myrank, torank);
                                fflush(stdout);
#endif

                                rrm_hav_flag_tb[torank] = true;
                                cmdq[idx].stat = PRE_PUTRRMFLAG;
                                comp_cqe_flag = true;
                                break;

                            case WAIT_RRM: /* geted rkey table */
#ifdef DEBUG
                                fprintf(stdout, "%d: WAIT RRM\n", myrank);
                                fflush(stdout);
#endif
                                cmdq[idx].stat = GETED_RRM;
                                comp_cqe_flag = true;
                                break;

                                
                            case WAIT_TAIL: /* geted tail of rcmdbuf */
#ifdef DEBUG
                                fprintf(stdout, "%d: WAIT_TAIL\n", myrank);
                                fprintf(stdout, "%d: tail_buf %lx, rcmdbuf_tail %lx\n",
                                        myrank, cmdq[idx].tail_buf, *rcmdbuf_tail);
                                fflush(stdout);
#endif
                                cmdq[idx].stat = PRE_GET_HEAD;
                                comp_cqe_flag = true;
                                break;
                              
                            case WAIT_HEAD: /* geted head of rcmdbuf */
#ifdef DEBUG		  
                                fprintf(stdout, "%d: WAIT_HEAD\n", myrank);
                                fflush(stdout);
#endif
                                cmdq[idx].stat = PRE_PUT_CMD;
                                comp_cqe_flag = true;
                                break;

                            case WAIT_PUT_CMD: /* completed put command into rcmdbuf */
#ifdef DEBUG
                                fprintf(stdout, "%d: WAIT_PUT_CMD\n", myrank);
                                fflush(stdout);
#endif
                                comp_cqe_flag = true;
                                break;
                                
                            default:
                                break;
                            }
                        }
                        /* update index for next cmdq queue */
                        index ++;
                        count ++;
#ifdef DEBUG_L2
                        fprintf(stdout, "%d: qp section: update cmdq index %lx\n", myrank, index);
                        fflush(stdout);
#endif
                    }
                }
                else if ((wc.wr_id & MASK_WRID_ACK) != MASK_WRID_ACK ) {
                    /* set index of rcmd buffer  */
                    index = *rcmdbuf_head;
                    comp_cqe_flag = false;
                    
#ifdef DEBUG
                    fprintf(stdout, "%d: qp section: rcmdbuf wr_id %lx\n", myrank, wc.wr_id);
                    fflush(stdout);
#endif 
                    count = 0;
                    while (index < *rcmdbuf_tail && comp_cqe_flag == false && count < MAX_RCMDB_SIZE) {
                        idx = index % MAX_RCMDB_SIZE; 
#ifdef DEBUG
                        fprintf(stdout, 
                                "%d: qp section: rcmdbuf wr_id %lx mask %llx st index %lx rcmdbuf[%lu] %lu\n", 
                                myrank, wc.wr_id, wc.wr_id & MASK_WRID_RCMDB, index, idx, rcmdbuf[idx].stat);
                        fflush(stdout);
#endif 
                        if ( rcmdbuf[idx].wr_id == wc.wr_id ) {
                            switch (rcmdbuf[idx].stat) {
                            case CMD_ISSUED: /* Complete copy in RCMDBUF: COPY only */
#ifdef DEBUG
                                fprintf(stdout, "%d: CMD_ISSUED\n", myrank);
                                fflush(stdout);
#endif
                                rcmdbuf[idx].stat = CMD_PRE_WRITEBACK_FIN;
                                comp_cqe_flag = true;
                                
                                break;
                                
                            case CMD_WAIT_RRM: /* Geted RRM: COPY and atomic */
#ifdef DEBUG	
                                fprintf(stdout, "%d: CMD_WAIT_RRM\n", myrank);
                                fflush(stdout);
#endif
                                rcmdbuf[idx].stat = CMD_GETED_RRM;
                                
                                comp_cqe_flag = true;
                                break;
                                
                            case CMD_FIRST_ISSUED:
#ifdef DEBUG
                                fprintf(stdout, "%d: CMD_FIRST_ISSUED\n", myrank);
                                fflush(stdout);
#endif
				torank = acp_query_rank(rcmdbuf[idx].gadst);
				rrm_hav_flag_tb[torank] = true;
#ifdef DEBUG
                                fprintf(stdout, "%d: CMD_FIRST_ISSUED torank %d\n", myrank, torank);
                                fflush(stdout);
#endif
                                rcmdbuf[idx].stat = CMD_PRE_PUTRRMGETEDFLAG_ISSUED;
                                comp_cqe_flag = true;
                                break;

                            case CMD_WAIT_FIRST_PUT_DST:
#ifdef DEBUG	
                                fprintf(stdout, "%d: CMD_WAIT_FIRST_PUT_DST\n", myrank);
                                fflush(stdout);
#endif
				torank = acp_query_rank(rcmdbuf[idx].gadst);
#ifdef DEBUG	
                                fprintf(stdout, "%d: CMD_WAIT_FIRST_PUT_DST torank %d\n", myrank, torank);
                                fflush(stdout);
#endif
				rrm_hav_flag_tb[torank] = true;
                                rcmdbuf[idx].stat = CMD_PRE_PUTRRMGETEDFLAG_PUT_DST;
                                comp_cqe_flag = true;
                                
                                break;
                                
                            case CMD_WAIT_PUT_DST: /* only atomic */
#ifdef DEBUG	
                                fprintf(stdout, "%d: CMD_WAIT_PUT_DST\n", myrank);
                                fflush(stdout);
#endif
                                rcmdbuf[idx].stat = CMD_PRE_WRITEBACK_FIN;
                                comp_cqe_flag = true;
                                
                                break;
                                
                            case CMD_WRITEBACK_FIN: /* copy and atomic */
#ifdef DEBUG	
                                fprintf(stdout, "%d: CMD_WRITEBACK_FIN tail_buf %lu \n", 
                                        myrank, rcmdbuf[idx].tail_buf);
                                fflush(stdout);
#endif
                                rcmdbuf[idx].valid_head = false;
                                rcmdbuf[idx].valid_tail = false;
                                rcmdbuf_update_head();
                                comp_cqe_flag = true;
                                break;
                                
                            default:
                                break;
                            }
                        }
                        /* update index for next rcmdbuf queue */
                        index ++;
                        count ++;
#ifdef DEBUG_L2
                        fprintf(stdout, "%d: qp section: update rcmdbuf index %lx\n",
                                myrank, index);
                        fflush(stdout);
#endif
                    } /* loop rcmdbuf index */
                }
                else { /* complete ack comm */
                    ack_comp_count++;
                    if (ack_comp_count > MAX_ACK_COUNT) {
                        ack_id = MASK_WRID_ACK;
                    }
#ifdef DEBUG
                    fprintf(stdout, 
                            "%d: qp section: put rrm ack: wc.wr_id %lx ack_comp_count %lu \n", 
                            myrank, wc.wr_id, ack_comp_count);
                    fflush(stdout);
#endif
                }/* wc.wr_id & MASK_WRID_ACK is equel to MASK_WRID_ACK */
            }/* wc.status is IBV_WC_SUCCESS */
            else {
		fprintf(stdout, 
			"%d: wc %lx is not SUCESS when check CQ %d stat %d type %d\n", 
			myrank, wc.wr_id, wc.status, cmdq[wc.wr_id].stat, cmdq[wc.wr_id].type);
		fflush(stdout);
		exit(-1);
	    }
        }
        /* CHECK COMMAND QUEUE section */
        /* cmdq is not empty */
        if ( tail > head ) {
            index = head;
            while ( index < tail ) {
                idx = index % MAX_CMDQ_ENTRY;
#ifdef DEBUG_L2
                fprintf(stdout, 
                        "%d: cmdq section: head %ld tail %ld cmdq[%ld].stat %lx\n", 
                        myrank, head, tail, index, cmdq[idx].stat);
                fflush(stdout);
#endif
                /* check if command is complete or not. */
                if ( cmdq[idx].stat != COMPLETED ) {
                    comm_work ++;
                    /* check command type */
                    /* command type is FIN */
                    if (cmdq[idx].type == FIN) {
#ifdef DEBUG
                        fprintf(stdout, "%d: CMD FIN\n", myrank);
                        fflush(stdout);
#endif
                        return 0;
                    }
                    /* command type is COPY and ATOMIC */
                    else if ( cmdq[idx].stat == UNISSUED ) {
#ifdef DEBUG
                        fprintf(stdout, "%d: GMA issued\n", myrank);
                        fflush(stdout);
#endif
                        /* order handling */
                        if ( cmdq[idx].ohdl == ACP_HANDLE_ALL || cmdq[idx].ohdl == ACP_HANDLE_CONT ) {
                            cmdq[idx].ohdl = cmdq[idx].ishdl - 1;
                        }
                        if ( cmdq[idx].ohdl >= head ) {
#ifdef DEBUG
                            fprintf(stdout, 
                                    "%d: index %ld ohndl %ld head %ld\n", 
                                    myrank, index, cmdq[idx].ohdl, head);
                            fflush(stdout);
#endif
                            index++;
                            continue;
                        }
                        /* set ga and size from cmdq */
                        src = cmdq[idx].gasrc;
                        dst = cmdq[idx].gadst;
                        /* get rank and tag of src ga */
                        srcrank = acp_query_rank(src);
                        srctag = query_gmtag(src);
                        srcoffset = query_offset(src);
                        /* get rank and tag of dst ga */
                        dstrank = acp_query_rank(dst);
                        dsttag = query_gmtag(dst);
                        dstoffset = query_offset(dst);
                        
                        if (cmdq[idx].type == COPY) {
                            size = cmdq[idx].cmde.copy_cmd.size;
                            /* local copy or local atomic */
                            if (myrank == srcrank && myrank == dstrank) {
                                void *srcaddr, *dstaddr;
                                srcaddr = acp_query_address(src);
                                dstaddr = acp_query_address(dst);
                                memcpy(dstaddr, srcaddr, size);
#ifdef DEBUG
                                fprintf(stdout, "%d: local copy dadr %p sadr %p\n", 
                                        myrank, dstaddr, srcaddr);
                                fflush(stdout);
#endif
                                cmdq[idx].stat = FINISHED;
                                check_cmdq_complete(index);
                                break;
                            }
                            else if (myrank == srcrank || myrank == dstrank) {
                                /* get */
                                if (myrank == dstrank ) {
                                    totag = srctag;
                                    torank = srcrank;
                                }
                                /* put */
                                else if (myrank == srcrank) {
                                    totag = dsttag;
                                    torank = dstrank;
                                }
                                /* check tag */
                                /* if tag point stater memory */
                                if (totag == TAG_SM) {
                                    ret = icopy(cmdq[idx].wr_id, dstrank, dsttag, dstoffset, 
                                                srcrank, srctag, srcoffset, size);
                                    if ( ret == 0 ) {
                                        cmdq[idx].stat = ISSUED;
                                    } 
                                }
                                else {/* if tag point globl memory */
                                    /* if have a remote rm table of target rank */
                                    rrmtb_idx = torank % rkey_cache_size;
                                    if (rrmtb[rrmtb_idx] != NULL) {
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "%d: CMD COPY have a rrmtb. rank %d torank %d, rrmtb_idx %d totag %d \n",
                                                myrank, myrank, torank, rrmtb_idx, totag);
                                        fflush(stdout);
#endif
                                        /* if tag entry is active */
                                        if (rrmtb[rrmtb_idx][totag].valid == true) {
                                            if (rrmtb[rrmtb_idx][totag].rank == torank) {
#ifdef DEBUG
                                                fprintf(stdout, 
                                                        "%d: CMD COPY have a entry rank %d torank %d, rrmtb_idx %d, totag %d\n",
                                                        myrank, myrank, torank, rrmtb_idx, totag);
                                                fflush(stdout);
#endif
                                                ret = icopy(cmdq[idx].wr_id, dstrank, dsttag, dstoffset, 
                                                      srcrank, srctag, srcoffset, size);
                                                if ( 0 == ret ) {
                                                    cmdq[idx].stat = ISSUED;
                                                }
                                            }
                                            else{/* tag entry have wrong data */
#ifdef DEBUG
                                                fprintf(stdout, 
                                                        "%d: CMD COPY tag entry has a wrong data.  rank %d torank %d, rrmtb_idx %d , totag %d\n",
                                                        myrank, myrank, torank, rrmtb_idx, totag);
                                                fflush(stdout);
#endif
                                                if (recv_rrm_flag == true){
                                                    ret = getlrm(cmdq[idx].wr_id, torank);
                                                    if ( 0 == ret ) {
                                                        recv_rrm_flag = false;
                                                        cmdq[idx].stat = WAIT_RRM;
                                                    }
                                                }
                                                else{
                                                    index++;
                                                continue;
                                                }
                                            }
                                        }
                                        else { /* if tag entry is non active */
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "%d: CMD COPY no entry.  rank %d torank %d, rrmtb_idx %d, totag %d\n",
                                                    myrank, myrank, torank, rrmtb_idx, totag);
                                            fflush(stdout);
#endif
                                            if (recv_rrm_flag == true){
                                                ret = getlrm(cmdq[idx].wr_id, torank);
                                                if ( 0 == ret ) {
                                                    recv_rrm_flag = false;
                                                    cmdq[idx].stat = WAIT_RRM;
                                                }
                                            }
                                            else{
                                                index++;
                                                continue;
                                            }
                                        }
                                    }
                                    else { /* if do not have rm table of target rank */
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "%d: CMD COPY no rrmtb.  rank %d torank %d, rrmtb_idx %d, totag %d\n",
                                                myrank, myrank, torank, rrmtb_idx, totag);
                                        fflush(stdout);
#endif
                                        if (recv_rrm_flag == true){
                                            ret = getlrm(cmdq[idx].wr_id, torank);
                                            if ( 0 == ret ) {
                                                recv_rrm_flag = false;
                                                cmdq[idx].stat = WAIT_RRM;
                                            }
                                        }
                                        else{
                                            index++;
                                            continue;
                                        }
                                    }
                                }
                            
                            }
                            /* remote to remote copy */
                            else if (myrank != srcrank && myrank !=dstrank) {
#ifdef DEBUG
                                fprintf(stdout, "%d: hdl %lx gettail srcrank %d dstrank %d \n",
                                       myrank, cmdq[idx].wr_id, srcrank, dstrank);
                                fflush(stdout);
#endif
                                ret = gettail(idx, srcrank);
                                if ( 0 == ret ) {
                                    cmdq[idx].stat = WAIT_TAIL;
                                }
                            }
                        }
                        else { /* type is atomic */
                            if (myrank == srcrank) {
                                void *srcaddr, *dstaddr;
                                
                                srcaddr = acp_query_address(src);
                                dstaddr = acp_query_address(dst);
                                selectatomic(srcaddr, (CMD *)&cmdq[idx]);
#ifdef DEBUG
                                fprintf(stdout, 
                                        "%d: remote atomic srcrank equal to dstrank : dadr %p sadr %p\n",
                                        myrank, dstaddr, srcaddr);
                                fflush(stdout);
#endif
                                if ((cmdq[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                    memcpy(dstaddr, (uint64_t *)&cmdq[idx].replydata, sizeof(uint64_t));
                                }
                                else {
                                    memcpy(dstaddr, (uint32_t *)&cmdq[idx].replydata, sizeof(uint32_t));
                                }
                                cmdq[idx].stat = FINISHED;
                                check_cmdq_complete(index);
                                break;
                            }
                            /* remote to remote atomic */
                            else {
                                ret = gettail(idx, srcrank);
                                if ( 0 == ret ) {
                                    cmdq[idx].stat = WAIT_TAIL;  
                                }
                            }
                        }
                        /* command issue, break */
                        break;
                    }
                    else if ( cmdq[idx].stat == GETED_RRM ) {
                        /* set ga of src and dst and size from cmdq */
                        src = cmdq[idx].gasrc;
                        dst = cmdq[idx].gadst;
                        size = cmdq[idx].cmde.copy_cmd.size;
                        
                        /* set offset, tag, size of src and dst */
                        srcrank = acp_query_rank(src);
                        srcoffset = query_offset(src);
                        srctag = query_gmtag(src);
                        dstrank = acp_query_rank(dst);		
                        dstoffset = query_offset(dst);
                        dsttag = query_gmtag(dst);
                        
                        /* set target rank */
                        if (myrank == dstrank) {
                            torank = srcrank;
                        }
                        else if (myrank == srcrank) {
                            torank = dstrank;
                        }
                        
                        /* set remote rkey mem table */
                        setrrm(torank);
                        recv_rrm_flag = true;
                        
                        /* issued copy */
                        ret = icopy(cmdq[idx].wr_id, dstrank, dsttag, 
                                    dstoffset, srcrank, srctag, srcoffset, size);
                        
                        if ( 0 == ret ) {
                            if (rrm_hav_flag_tb[torank] == true) {
                                cmdq[idx].stat = ISSUED;
                            }
                            else {
                                cmdq[idx].stat = FIRST_ISSUED;
                            }
                        }
                        else {
                            cmdq[idx].stat = GETED_RRM;
                        }
                    }
                    else if ( cmdq[idx].stat == PRE_PUTRRMFLAG ) {
                        /* get global address src and dst */
                        src = cmdq[idx].gasrc;
                        dst = cmdq[idx].gadst;
                                
                        /* get rank of src and dst */
                        srcrank = acp_query_rank(src);
                        dstrank = acp_query_rank(dst);
                        
                        /* set target rank */
                        if (myrank == dstrank) {
                            torank = srcrank;
                        }
                        else if (myrank == srcrank) {
                            torank = dstrank;
                        }
                        
                        /* put geted rrm flag */
#ifdef DEBUG
			fprintf(stdout, "%d: PRE_PUTRRMFLAG torank %d\n", acp_rank(), torank);
			fflush(stdout);
#endif
                        ret = putrrmgetedflag(cmdq[idx].wr_id, torank);
                        if ( 0 == ret ) {
                            cmdq[idx].stat = ISSUED;
                        }
                        else {
                            cmdq[idx].stat = PRE_PUTRRMFLAG;
                        }
                    }
                    else if ( cmdq[idx].stat == PRE_GET_HEAD ) {
                        /* set ga of src from cmdq */
                        src = cmdq[idx].gasrc;
                        /* ger rank of src ga*/
                        srcrank = acp_query_rank(src);
#ifdef DEBUG
                        if (myrank == srcrank) {
                            fprintf(stderr, "%d: rr opration tail wait error\n", myrank);
                        }
#endif
                        /* issued get head */
                        ret = gethead(idx, srcrank);
                        
                        if ( 0 == ret ) {
                            cmdq[idx].stat = WAIT_HEAD;
                        }
                        else { 
                            cmdq[idx].stat = PRE_GET_HEAD;
                        }
                    }
                    else if ( cmdq[idx].stat == PRE_PUT_CMD) {
                        /* set src ga from cmdq */
                        src = cmdq[idx].gasrc;
                        /* set srcrank */
                        srcrank = acp_query_rank(src);
                        
                        /* check enable putflag  */
#ifdef DEBUG		  
                        fprintf(stdout, "%d srcrank %d tail_buf %lx head_buf %lx\n",
                                myrank, srcrank, cmdq[idx].tail_buf, cmdq[idx].head_buf);
                        fflush(stdout);
#endif
                                
                        if (cmdq[idx].tail_buf - cmdq[idx].head_buf > MAX_RCMDB_SIZE - 1) {
#ifdef DEBUG
                            fprintf(stdout, 
                                    "%d not exec putcmd acp handle %lx idx %lx tail_buf %lx head_buf %lx\n", 
                                    acp_rank(), index, idx, cmdq[idx].tail_buf, cmdq[idx].head_buf);
                            fflush(stdout);
#endif
                            ret = gethead(idx, srcrank);
                            cmdq[idx].stat = WAIT_HEAD;
                            break;
                        }
                        
                        ret = putcmd(idx, srcrank, cmdq[idx].tail_buf);
                        if ( 0 == ret ) {
                            cmdq[idx].stat = WAIT_PUT_CMD;
                        }
                        else {
                            cmdq[idx].stat = PRE_PUT_CMD;
                        }
                    }
                    else {
                        check_cmdq_complete(index);
                    }
                }
                index ++;
#ifdef DEBUG_L2
                fprintf(stdout, 
                        "%d: cmdq section update index %lx\n", 
                        myrank, index);
                fflush(stdout);
#endif
            }
        }
        
        /* CHECK recv COMMAND QUEUE section */
        index = *rcmdbuf_head;
        while ( index <  *rcmdbuf_tail ) {
            
            idx = index % MAX_RCMDB_SIZE;
            if ( rcmdbuf[idx].valid_head == true && rcmdbuf[idx].valid_tail == true ) {
                comm_work ++;
                if ( rcmdbuf[idx].stat == CMD_UNISSUED ) {
#ifdef DEBUG_L2
                    fprintf(stdout, "%d: rcmdq section: CMD_UNISSUED\n", myrank);
                    fflush(stdout);
#endif
                    rcmdbuf[idx].wr_id = index | MASK_WRID_RCMDB;
#ifdef DEBUG_L2
                    fprintf(stdout, "rcmdq section: %d: *rcmdbuf_head %lx, *rcmdbuf_tail %lx\n", 
                            acp_myrank, *rcmdbuf_head, *rcmdbuf_tail);
                    fflush(stdout);
#endif
                    if (rcmdbuf[idx].type == COPY) {
                        /* set ga form rcmdbuf and set size */
                        src = rcmdbuf[idx].gasrc;
                        dst = rcmdbuf[idx].gadst;
                        size = rcmdbuf[idx].cmde.copy_cmd.size;
                        
                        /* get rank and tag of src ga */
                        srcrank = acp_query_rank(src);
                        srctag = query_gmtag(src);
                        srcoffset = query_offset(src);
                        
                        /* get rank and tag of dst ga */
                        dstrank = acp_query_rank(dst);
                        dsttag = query_gmtag(dst);
                        dstoffset = query_offset(dst);
                        
                        if (myrank == dstrank) { /* local copy in src rank */
                            void *srcaddr, *dstaddr;
                            srcaddr = acp_query_address(src);
                            dstaddr = acp_query_address(dst);
                            memcpy(dstaddr, srcaddr, size);
#ifdef DEBUG
                            fprintf(stdout, "%d: local copy:dadr %p sadr %p\n", 
                                    myrank, dstaddr, srcaddr);
                            fflush(stdout);
#endif
                            ret = writebackstat(idx);
                            if ( 0 == ret ) {
                                rcmdbuf[idx].stat = CMD_WRITEBACK_FIN;
                            }
                            else {
                                rcmdbuf[idx].stat = CMD_UNISSUED;
                            } 
                            break;
                        }
                        else { /* remote put */
                            if (dsttag == TAG_SM) {
#ifdef DEBUG
                                fprintf(stdout, 
                                        "%d: CMDRB COPY to starter memory dstrank %d dsttag %d\n",
                                        myrank, dstrank, dsttag);
                                fflush(stdout);
#endif
                                ret = icopy(rcmdbuf[idx].wr_id, dstrank, dsttag, dstoffset, 
                                            srcrank, srctag, srcoffset, size);
                                if ( 0 == ret ) {
                                    rcmdbuf[idx].stat = CMD_ISSUED;
                                }
                            }
                            else{
                                rrmtb_idx = dstrank % rkey_cache_size;
                                /* if have a remote rm table of target rank */
                                if (rrmtb[rrmtb_idx] != NULL) {
#ifdef DEBUG
                                    fprintf(stdout, 
                                            "%d: CMDRB COPY have a rrmtb. rank %d dstrank %d, rrmtb_idx %d, dsttag %d \n",
                                            myrank, myrank, dstrank, rrmtb_idx, dsttag);
                                    fflush(stdout);
#endif
                                    /* if tag entry is active */
                                    if (rrmtb[rrmtb_idx][dsttag].valid == true) {
                                        /* cache hit */
                                        if(rrmtb[rrmtb_idx][dsttag].rank == dstrank ) {
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "%d: CMDRB COPY have a entry rank %d dstrank %d, rrmtb_idx %d, dsttag %d\n",
                                                    myrank, myrank, dstrank, rrmtb_idx, dsttag);
                                            fflush(stdout);
#endif
                                            ret = icopy(rcmdbuf[idx].wr_id, dstrank, dsttag, dstoffset, 
                                                  srcrank, srctag, srcoffset, size);
                                            if ( 0 == ret ) {
                                                rcmdbuf[idx].stat = CMD_ISSUED;
                                            }
                                        }
                                        /* tag entry is wrong data */
                                        else{
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "%d: CMDRB COPY tag entry have wrong data.  rank %d dstrank %d, rrmtb_idx %d dsttag %d\n",
                                                    myrank, myrank, dstrank, rrmtb_idx, dsttag);
                                            fflush(stdout);
#endif
                                            if (recv_rrm_flag == true) {
                                                ret = getlrm(rcmdbuf[idx].wr_id, dstrank);
                                                if ( 0 == ret ) {
                                                    recv_rrm_flag = false;
                                                    rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                                }
                                            }
                                            else {
                                                index++;
                                                continue;
                                            }
                                        }
                                    }
                                    else { /* if tag entry is non active */
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "%d: CMDRB COPY no entry.  rank %d dstrank %d, rrmtb_idx %d, dsttag %d\n",
                                                myrank, myrank, dstrank, rrmtb_idx, dsttag);
                                        fflush(stdout);
#endif
                                        if (recv_rrm_flag == true) {
                                            ret = getlrm(rcmdbuf[idx].wr_id, dstrank);
                                            if ( 0 == ret ) { 
                                                recv_rrm_flag = false;
                                                rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                            }
                                        }
                                        else {
                                            index++;
                                            continue;
                                        }
                                    }
                                }
                                else { /* if do not have rm table of dst rank */
#ifdef DEBUG
                                    fprintf(stdout, 
                                            "%d: CMDRB COPY no rrmtb.  rank %d dstrank %d, rrmtb_idx %d, dsttag %d\n",
                                            myrank, myrank, torank, rrmtb_idx, totag);
                                    fflush(stdout);
#endif
                                    if (recv_rrm_flag == true) {
                                        ret = getlrm(rcmdbuf[idx].wr_id, dstrank);
                                        if ( 0 == ret ) {
                                            recv_rrm_flag = false;
                                            rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                        }
                                    }
                                    else {
                                        index++;
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                    else { /* atomic */
#ifdef DEBUG
                        fprintf(stdout, "%d: CMD_UNISSUED ATOMIC\n", myrank);
                        fflush(stdout);
#endif
                        /* set src and dst of ga from rcmd buffer */
                        src = rcmdbuf[idx].gasrc;
                        dst = rcmdbuf[idx].gadst;
                        
                        /* get rank of src ga */
                        srcrank = acp_query_rank(src);
                        
                        /* get rank, tag, offset of dst ga */
                        dstrank = acp_query_rank(dst);
                        dsttag = query_gmtag(dst);
                        dstoffset = query_offset(dst);
                        
                        if (myrank == srcrank) {
                            void *srcaddr; 
                            srcaddr = acp_query_address(src);

                            /* if tag point stater memory */
                            if (dsttag == TAG_SM) {
                                if ((rcmdbuf[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                    selectatomic(srcaddr, &rcmdbuf[idx]);
#ifdef DEBUG
                                    fprintf(stdout, 
                                            "%d: ATOMIC select atomic myrank %d , srcrank %d, rcmdbuf[%d].replydata %lu\n", 
                                            myrank, myrank, srcrank, idx, rcmdbuf[idx].replydata);
                                    fflush(stdout);
#endif
                                    ret = putreplydata(idx, dstrank, dsttag, dstoffset, true);
                                }
                                else {
                                    selectatomic(srcaddr, &rcmdbuf[idx]);
#ifdef DEBUG
                                    fprintf(stdout, 
                                            "%d: ATOMIC select atomic myrank %d , srcrank %d, rcmdbuf[%d].replydata %lu\n", 
                                            myrank, myrank, srcrank, idx, rcmdbuf[idx].replydata);
                                    fflush(stdout);
#endif
                                    ret = putreplydata(idx, dstrank, dsttag, dstoffset, false);
                                }
                                if ( 0 == ret ) {
                                    rcmdbuf[idx].stat =  CMD_WAIT_PUT_DST;
                                }
                            }
                            /* if tag point globl memory */
                            else {
                                rrmtb_idx = dstrank % rkey_cache_size;
                                /* if have a remote rm table of dst rank */
                                if (rrmtb[rrmtb_idx] != NULL ) {
#ifdef DEBUG
                                    fprintf(stdout, 
                                            "%d: CMDRB ATOMIC have a rrmtb. rank %d dstrank %d, rrmtb_idx %d, dsttag %d \n",
                                            myrank, myrank, dstrank, rrmtb_idx, dsttag);
                                    fflush(stdout);
#endif
                                    /* if tag entry is active */
                                    if (rrmtb[rrmtb_idx][dsttag].valid == true) {
                                        /* cache hit  */
                                        if(rrmtb[rrmtb_idx][dsttag].rank == dstrank){
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "%d: CMDRB ATOMIC have a entry rank %d dstrank %d, rrmtb_idx %d, dsttag %d\n",
                                                    myrank, myrank, dstrank, rrmtb_idx, dsttag);
                                            fflush(stdout);
#endif
                                            if ((rcmdbuf[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                                selectatomic(srcaddr, &rcmdbuf[idx]);
#ifdef DEBUG
                                                fprintf(stdout, 
                                                        "%d: ATOMIC select atomic myrank %d , srcrank %d, rcmdbuf[%d].replydata %lu\n", 
                                                        myrank, myrank, srcrank, idx, rcmdbuf[idx].replydata);
                                                fflush(stdout);
#endif
                                                ret = putreplydata(idx, dstrank, dsttag, dstoffset, true);
                                            }
                                            else {
                                                selectatomic(srcaddr, &rcmdbuf[idx]);
#ifdef DEBUG
                                                fprintf(stdout, 
                                                        "%d: ATOMIC select atomic myrank %d , srcrank %d, rcmdbuf[%d].replydata %lu\n", 
                                                        myrank, myrank, srcrank, idx, rcmdbuf[idx].replydata);
                                                fflush(stdout);
#endif
                                                ret = putreplydata(idx, dstrank, dsttag, dstoffset, false);
                                            }
                                            if ( 0 == ret ) {
                                                rcmdbuf[idx].stat = CMD_WAIT_PUT_DST;
                                            }
                                        }
                                        /* have rm wrong data in rrmtb_idx */
                                        else{
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "%d: CMDRB ATOMIC have a old entry.  rank %d dstrank %d, rrmtb_idx, dsttag %d\n",
                                                    myrank, myrank, dstrank, rrmtb_idx,dsttag);
                                            fflush(stdout);
#endif
                                            
                                            if (recv_rrm_flag == true) {
                                                selectatomic(srcaddr, &rcmdbuf[idx]);
#ifdef DEBUG
                                                fprintf(stdout, 
                                                        "%d: ATOMIC select atomic myrank %d , srcrank %d, rcmdbuf[%d].replydata %lu\n", 
                                                        myrank, myrank, srcrank, idx, rcmdbuf[idx].replydata);
                                                fflush(stdout);
#endif                                              
                                                ret = getlrm(rcmdbuf[idx].wr_id, dstrank);
                                                if ( 0 == ret ) {
                                                    recv_rrm_flag = false;
                                                    rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                                }
                                            }
                                            else {
                                                index++;
                                                continue;
                                            }
                                        }
                                    }
                                    /* if tag entry is non active */
                                    else {
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "%d: CMDRB ATOMIC no entry.  rank %d dstrank %d, rrmtb_idx, dsttag %d\n",
                                                myrank, myrank, dstrank, rrmtb_idx,dsttag);
                                        fflush(stdout);
#endif
                                        
                                        if (recv_rrm_flag == true) {
                                            recv_rrm_flag = false;
                                            //rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                            selectatomic(srcaddr, &rcmdbuf[idx]);
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "%d: ATOMIC select atomic myrank %d , srcrank %d, rcmdbuf[%d].replydata %lu\n", 
                                                    myrank, myrank, srcrank, idx, rcmdbuf[idx].replydata);
                                            fflush(stdout);
#endif
                                            ret = getlrm(rcmdbuf[idx].wr_id, dstrank);
					    if ( 0 == ret ) {
					      recv_rrm_flag = false;
					      rcmdbuf[idx].stat = CMD_WAIT_RRM;
					    }
					}
                                        else {
                                            index++;
                                            continue;
                                        }
                                    }
                                }
                                /* if do not have rm table of target rank */
                                else {
#ifdef DEBUG
                                    fprintf(stdout, 
                                            "%d: CMDRB ATOMIC no rrmtb.  rank %d dstrank %d, dsttag %d\n",
                                            myrank, myrank, dstrank, dsttag);
                                    fprintf(stdout, "rcmdbuf[idx].wr_id %lx\n", rcmdbuf[idx].wr_id);
                                    fflush(stdout);
#endif
                                    if (recv_rrm_flag == true) {
                                        recv_rrm_flag = false;
                                        //rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                        selectatomic(srcaddr, &rcmdbuf[idx]);
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "%d: ATOMIC select atomic myrank %d , srcrank %d, rcmdbuf[%d].replydata %lu\n", 
                                                myrank, myrank, srcrank, idx, rcmdbuf[idx].replydata);
                                        fflush(stdout);
#endif
                                        ret = getlrm(rcmdbuf[idx].wr_id, dstrank);
					if ( 0 == ret ) {
					  recv_rrm_flag = false;
					  rcmdbuf[idx].stat = CMD_WAIT_RRM;
					}
				    }
                                    else {
                                        index++;
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                }
                else if ( CMD_GETED_RRM == rcmdbuf[idx].stat ) {
                    /* get ga of dst from rcmd buffer  */
                    dst = rcmdbuf[idx].gadst;
                    /* get rank and tag, offset of dst */
                    dstrank = acp_query_rank(dst);
                    
                    /* set remote rkey memory table of dst rank */
                    setrrm(dstrank);
                    recv_rrm_flag = true;
                    
                    dsttag = query_gmtag(dst);
                    dstoffset = query_offset(dst);
                    
                    /* case COPY */
                    if ( rcmdbuf[idx].type == COPY ) {
                        /* set src ga from rcmd buffer */
                        src = rcmdbuf[idx].gasrc;
                        /* get rank, tag, offset of src */
                        srcrank = acp_query_rank(src);
                        srctag = query_gmtag(src);
                        srcoffset = query_offset(src);
                        
                        /* issued internal copy  */
                        ret = icopy(rcmdbuf[idx].wr_id, dstrank, dsttag, dstoffset, 
                                    srcrank, srctag, srcoffset, size);
                        if ( 0 == ret ) {
                            /* check if acess first time or not  */
                            if ( rrm_hav_flag_tb[dstrank] == true ) { 
                                rcmdbuf[idx].stat = CMD_ISSUED;
                            }
                            else {
                                rcmdbuf[idx].stat = CMD_FIRST_ISSUED;
                            }
                        }
                        else { 
                            rcmdbuf[idx].stat = CMD_GETED_RRM ;
                        }
                    }
                    /* case ATOMIC */
                    else {
                        if ( ( rcmdbuf[idx].type & MASK_ATOMIC ) == MASK_ATOMIC ) {                            
                            if ( ( rcmdbuf[idx].type & MASK_ATOMIC8 ) == MASK_ATOMIC8 ) {
                                ret = putreplydata(idx, dstrank, dsttag, dstoffset, true);
                            }
                            else {
                                ret = putreplydata(idx, dstrank, dsttag, dstoffset, false);
                            }

                            if ( 0 == ret ) {
                                /* check if acess first time or not  */
                                if (rrm_hav_flag_tb[dstrank] == true) { 
                                    rcmdbuf[idx].stat = CMD_WAIT_PUT_DST;
                                }
                                else {
                                    rcmdbuf[idx].stat = CMD_WAIT_FIRST_PUT_DST;
                                }
                            }
                            else {
                                rcmdbuf[idx].stat = CMD_GETED_RRM;
                            }
#ifdef DEBUG
                            fprintf(stdout,"%d: qp section: rcmdbuf[%lx] %lu\n", 
                                    myrank, idx, rcmdbuf[idx].stat);
                            fflush(stdout);
#endif
                        }
                    }
                }
                else if ( CMD_PRE_WRITEBACK_FIN == rcmdbuf[idx].stat ) {
                    ret = writebackstat(idx);
                    if ( 0 == ret ) {
                        rcmdbuf[idx].stat = CMD_WRITEBACK_FIN;
                    }
                    else {
                        rcmdbuf[idx].stat = CMD_PRE_WRITEBACK_FIN;
                    }
                }
                else if ( CMD_PRE_PUTRRMGETEDFLAG_ISSUED == rcmdbuf[idx].stat ) {
                    /* get ga of dst from rcmd buffer */
                    dst = rcmdbuf[idx].gadst;                                
                    /* get rank of dst ga */
                    dstrank = acp_query_rank(dst);
                    
                    /* issued put rrm get flag */
                    ret = putrrmgetedflag(rcmdbuf[idx].wr_id, dstrank);
                    
                    /* set status of rcmd buffer */
                    if ( 0 == ret ) {
                        rcmdbuf[idx].stat = CMD_ISSUED;
                    }
                    else {
                        rcmdbuf[idx].stat = CMD_PRE_PUTRRMGETEDFLAG_ISSUED;
                    }
                }
                else if ( CMD_PRE_PUTRRMGETEDFLAG_PUT_DST == rcmdbuf[idx].stat ) {
                    /* get ga of dst from rcmd buffer */
                    dst = rcmdbuf[idx].gadst;                                
                    /* get rank of dst ga */
                    dstrank = acp_query_rank(dst);
                    
                    /* issued put rrm get flag */
                    ret = putrrmgetedflag(rcmdbuf[idx].wr_id, dstrank);
                    
                    /* set status of rcmd buffer */
                    if ( 0 == ret ) {
                        rcmdbuf[idx].stat = CMD_WAIT_PUT_DST;
                    }
                    else {
                        rcmdbuf[idx].stat = CMD_PRE_PUTRRMGETEDFLAG_PUT_DST;
                    }
                }
            }
            /* increment index of rcmdbuf */
            index++;
        }
        /* PUT RRM ACK SECTION */
        for (i = 0; i < nprocs; i++) {
            /* check rrm reset flag table */
            if (true == rrm_reset_flag_tb[i]) {
                comm_work ++;
                /* free remote rkey table */
                for (j = 0; j < MAX_RM_SIZE; j++) {
                    rrmtb_idx = i % rkey_cache_size;
                    if (rrmtb[rrmtb_idx] != NULL) { 
                        if (rrmtb[rrmtb_idx][j].valid == true) {
                            cur_rank = rrmtb[rrmtb_idx][j].rank;
                            break;
                        }
                    }
                }
                if (cur_rank == i) {
                    rrmtb_idx = cur_rank % rkey_cache_size;
                    free(rrmtb[rrmtb_idx]);
                    rrmtb[rrmtb_idx] = NULL;
#ifdef DEBUG
		    fprintf(stdout, "%d: rrm_tb[%d] = %p cur_rank %d\n", 
			    acp_rank(), rrmtb_idx, rrmtb[rrmtb_idx], cur_rank);
		    fflush(stdout);
#endif
		}
		
                /* put rrm ack flag */
                ret = putrrmackflag(ack_id, i);
#ifdef DEBUG
		fprintf(stdout, "%d: putrrmackflag %d \n", acp_rank(), i);
		fflush(stdout);
#endif		
		/* set false rrm reset flag table */
                if ( 0 == ret ) { 
		    rrm_reset_flag_tb[i] = 0;
		    rrm_hav_flag_tb[i] = 0;
                    ack_id++;
                }
		else {
		  /* if send queue is full, 
		     then need to check completion qeueu on IB. */
		  break;
                }
            }
        }
    }
}

int iacp_init(void){
    
    int rc = 0; /* return code */
    int i, j; /* general index */
    
    int sock_s; /* socket server */
    socklen_t addrlen;/* address length */
    struct sockaddr_in myaddr, dstaddr, srcaddr; /* address */
    struct ibv_device **dev_list = NULL; /* IB device list */
    char *dev_name = NULL; /* device name */
    
    /* initalize queue pair attribute information*/
    struct ibv_qp_init_attr qp_init_attr;
    int mr_flags = 0; /* flag of memory registeration*/
    int ib_port = 1; /* ib port */
    int cq_size = MAX_CQ_SIZE; /* CQ size */
    
    /* post recieve in RTR */
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    
    uint32_t *local_qp_num; /* local each qp number */
    uint32_t *tmp_qp_num; /* temporary qp number */
    uint32_t *remote_qp_num; /* remote each qp number */
    
    CII local_data; /* local data for socket communication */
    CII remote_data; /* remote data for socket communication */
    CII tmp_data; /* temporary data for socket communication */
    int torank; /* rank ID */
    
    struct ibv_qp_attr attr; /* modify queue pair */
    int flags; /* flag of modify queue pair */
    
    /* allocate the starter memory adn register memory */
    size_t syssize;
    /* ajusting variable for alignment 8 bytes */
    int alm8_add; 
    /* enable address resuse, as soon as possible */
    const int on = 1;
    /* create iacp_starter_memory_size_dl_heap */
    uint64_t iacp_starter_memory_size_dl_heap = 256;

    /* initialize head, tail */
    head = 1;
    tail = 1;  
    
    recv_rrm_flag = true; /* get enable recv rrm falg */
    
    /* initialize ack ID  */
    ack_id = MASK_WRID_ACK;
    ack_comp_count = 0;
        
    /* adjust sysmem to 8 byte alignment */
    /* adjust starter memory for bl */
    alm8_add = acp_smsize & 7;
    alm8_add_func(alm8_add);
    acp_smsize_adj = acp_smsize + alm8_add ;

#ifdef ACPBLONLY
    iacp_starter_memory_size_dl = 0;
    iacp_starter_memory_size_dl_heap = 0;
    iacp_starter_memory_size_cl = 0;
#endif

    /* adjust starter memory for dl */
    alm8_add = iacp_starter_memory_size_dl & 7;
    alm8_add_func(alm8_add);
    acp_smdlsize_adj = iacp_starter_memory_size_dl + iacp_starter_memory_size_dl_heap + alm8_add;
             
    /* adjust starter memory for cl */
    alm8_add = iacp_starter_memory_size_cl & 7;
    alm8_add_func(alm8_add);
    acp_smclsize_adj = iacp_starter_memory_size_cl + alm8_add;
    
    /* ajust one byte flag */
    alm8_add = ((acp_numprocs * 4 + 1) & 7);
    alm8_add_func(alm8_add);
    ncharflagtb_adj = sizeof(char) * ((acp_numprocs * 4 + 1) + alm8_add);
    
#ifdef DEBUG
    fprintf(stdout, "RM %lu, CMD %lu\n ", sizeof(RM), sizeof(CMD));
    fflush(stdout);
#endif
        
    /* sysmem size */
    syssize = acp_smsize_adj + sizeof(RM) * (MAX_RM_SIZE) * 2  + sizeof(uint64_t) * 4 +
        sizeof(CMD) * MAX_CMDQ_ENTRY + sizeof(CMD) * MAX_RCMDB_SIZE + sizeof(CMD) * MAX_CMDQ_ENTRY +
        ncharflagtb_adj + acp_smdlsize_adj + acp_smclsize_adj;	
    
    /* malloc sysmem */
#ifdef DEBUG
    fprintf(stdout, "%d: pre sysmem  malloc %zu bytes to memory buffer\n", acp_myrank, syssize);
    fflush(stdout);
#endif
    sysmem = (char *) malloc(syssize);
    
    if (NULL == sysmem ) {
        fprintf(stderr, "failed to malloc %zu bytes to memory buffer\n", 
                syssize);
        rc = -1;
        goto exit;
    }
    
    /* initalize starter memory, local RM table, recv RM table */
    memset(sysmem, 0 , syssize);
    
    /* initalize local rkey memory */
    lrmtb = (RM *) ((char *)sysmem + acp_smsize_adj);
    offset_lrmtb = acp_smsize_adj;
    for (i = 0;i < MAX_RM_SIZE;i++) {
        /* entry non active */
        lrmtb[i].valid = false;
    }

    
    /* initalize revcieve local rkey memory */
    recv_lrmtb = (RM *) ((char *)lrmtb + sizeof(RM) * MAX_RM_SIZE );
    offset_recv_lrmtb = offset_lrmtb + sizeof(RM) * MAX_RM_SIZE;
    for (i = 0;i < MAX_RM_SIZE;i++) {
        /* entry non active */
        recv_lrmtb[i].valid = false;
    }
    
    /* initialize write flag of command recv buffer */
    rcmdbuf_head = (uint64_t *)((char *)recv_lrmtb +  sizeof(RM) * MAX_RM_SIZE );
    offset_rcmdbuf_head = offset_recv_lrmtb + sizeof(RM) * MAX_RM_SIZE;
    *rcmdbuf_head = 0;
    
    /* initialize read flag of command recv buffer */
    rcmdbuf_tail = (uint64_t *)((char *)rcmdbuf_head + sizeof(uint64_t));
    offset_rcmdbuf_tail = offset_rcmdbuf_head + sizeof(uint64_t);
    *rcmdbuf_tail = 0;
    
    finished_stat_buf = (uint64_t *)((char *)rcmdbuf_tail + sizeof(uint64_t));
    offset_finished_stat_buf = offset_rcmdbuf_tail + sizeof(uint64_t);
    *finished_stat_buf = FINISHED;

    /* initialize command q */
    cmdq = (CMD *)((char *)finished_stat_buf + sizeof(uint64_t)); 
    offset_cmdq = offset_finished_stat_buf + sizeof(uint64_t);
    offset_stat = (char *)&cmdq[0].stat - (char *)&cmdq[0];
        
    /* initialize command recv buffer */
    rcmdbuf = (CMD *)((char *)cmdq + sizeof(CMD) * MAX_CMDQ_ENTRY);
    offset_rcmdbuf = offset_cmdq + sizeof(CMD) * MAX_CMDQ_ENTRY;
    
    /* initilaize put cmd buffer */
    putcmdbuf = (CMD *)((char*)rcmdbuf + sizeof(CMD) * MAX_RCMDB_SIZE);
    offset_putcmdbuf = offset_rcmdbuf + sizeof(CMD) * MAX_RCMDB_SIZE;
    
    /* initilaize get flag table */
    rrm_get_flag_tb = (char *)((char*)putcmdbuf + sizeof(CMD) * MAX_CMDQ_ENTRY);
    offset_rrm_get_flag_tb = offset_putcmdbuf + sizeof(CMD) * MAX_CMDQ_ENTRY;
    
    /* initilaize reset flag table */
    rrm_reset_flag_tb = (char *)((char*)rrm_get_flag_tb + sizeof(char) * acp_numprocs);
    offset_rrm_reset_flag_tb = offset_rrm_get_flag_tb + sizeof(char) * acp_numprocs;

    /* initialize ack flag table */
    rrm_ack_flag_tb = (char *)((char*)rrm_reset_flag_tb + sizeof(char) * acp_numprocs);
    offset_rrm_ack_flag_tb = offset_rrm_reset_flag_tb + sizeof(char) * acp_numprocs;

    /* initialize ack flag table */
    rrm_hav_flag_tb = (char *)((char*)rrm_ack_flag_tb + sizeof(char) * acp_numprocs);
    offset_rrm_hav_flag_tb = offset_rrm_ack_flag_tb + sizeof(char) * acp_numprocs;
    
    /* set true flag buffer */
    true_flag_buf = (char *)((char*)rrm_hav_flag_tb + sizeof(char) * acp_numprocs);
    offset_true_flag_buf = offset_rrm_hav_flag_tb + sizeof(char) * acp_numprocs;
    *true_flag_buf = true;
    
    /* set starter memory for multi module */
    acp_buf_dl = (char *)((char *)rrm_get_flag_tb + ncharflagtb_adj);
    offset_acp_buf_dl = offset_rrm_get_flag_tb + ncharflagtb_adj;
    acp_buf_cl = (char *)((char *)acp_buf_dl + acp_smdlsize_adj);
    offset_acp_buf_cl = offset_acp_buf_dl + acp_smdlsize_adj;
    
#ifdef DEBUG
    fprintf(stdout, 
            "%d: sm %p acp_cl_buf %p syssize %zu sm + syssize %p, offset_acp_buf_vd %lu %d\n", 
            acp_myrank, 
	    sysmem, (char *)acp_buf_cl + acp_smclsize_adj, 
            syssize, sysmem + syssize, offset_acp_buf_cl, sizeof(CMD));
    fflush(stdout);
#endif

    /* remote register memory table */
    if(acp_numprocs > MAX_RKEY_CASH_SIZE){
        rkey_cache_size = MAX_RKEY_CASH_SIZE;
    }
    else{
        rkey_cache_size = acp_numprocs;
    }
    rrmtb = (RM **) malloc (sizeof (RM *) * rkey_cache_size);
    if (rrmtb == NULL) {
        fprintf(stderr, "failed to malloc rrmtb\n");
        rc = -1;
        goto exit;
    }
    memset(rrmtb, 0, sizeof(RM *) * rkey_cache_size);
    
    /* generate server socket */
    if ((sock_s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "sever socket() failed \n");
        rc = -1;
        goto exit;
    }

#ifdef TCP_DEBUG
    fprintf(stderr, "%d socekt scock_s create fin\n", acp_rank());   
#endif
 
    /* initialize myaddr of sockaddr_in  */
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(my_port);

    rc = setsockopt( sock_s, SOL_SOCKET, SO_REUSEADDR, (const void*)&on, sizeof(on) );  
    if(rc == -1){
        fprintf(stderr, "%d setsockopt rc %d\n", acp_rank(), rc);
        goto exit;
    }
#ifdef TCP_DEBUG
    fprintf(stderr, "%d setsockopt rc %d\n", acp_rank(), rc);
    char hostname[256];
    gethostname(hostname, 256);
    fprintf(stderr, "%d socekt sock_s bind in sp %u my %u %s \n",
            acp_rank(), myaddr.sin_port, my_port, hostname);
#endif   
    /* bind socket file descriptor*/
    while (bind(sock_s, (struct sockaddr*)&myaddr, sizeof(myaddr)) < 0) {
        if (errno != EADDRINUSE) {
            perror("bind() failed\n");
            rc = -1;
            goto exit;
        }
#ifdef TCP_DEBUG
        perror("bind() failed section 2");
        fprintf(stderr, "%d: bind() failed errno %d \n", acp_rank(), errno);
#endif
    }
#ifdef TCP_DEBUG
    fprintf(stderr, "%d socekt sock_s bind fin\n", acp_rank());   
#endif

    if (listen(sock_s, 10) < 0) {
        rc = -1;
        goto exit;
    }
#ifdef TCP_DEBUG
    fprintf(stderr, "%d socekt listen fin\n", acp_rank());   
#endif

    /* generate connection socket */
    if ((sock_connect = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "connect socket() failed \n");
        goto exit;
    }
#ifdef TCP_DEBUG
    fprintf(stderr, "%d socekt sock_s create for connect\n", acp_rank());    
#endif    

    /* generate destination address */
    memset(&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = dst_addr;
    dstaddr.sin_port = htons(dst_port);
    
    addrlen = sizeof(srcaddr);
#ifdef TCP_DEBUG
    fprintf(stderr, "%d: dp sp %u dp %u\n", acp_rank(), dstaddr.sin_port, dst_port);
#endif
    /* get the size of source address */
    if (acp_numprocs > 1) {
        if ((acp_myrank & 1) == 0) { /* even rank  */
            while ((sock_accept = accept(sock_s, (struct sockaddr *)&srcaddr, &addrlen)) < 0) {
                perror("even accept() failed:");
                goto exit;
            }
#ifdef TCP_DEBUG
            fprintf(stderr, "%d socekt accept fin for connect dp even.\n", acp_rank());
#endif
            while (connect(sock_connect, (struct sockaddr*)&dstaddr, sizeof(dstaddr)) < 0){
                if (errno != EINTR && errno != EAGAIN && errno != ECONNREFUSED) {
                    perror("even connect() failed:");
                    goto exit;
                }
                else{
                    sleep(1);
                }
            }
#ifdef TCP_DEBUG
            fprintf(stderr, "%d socekt connect for connect even.\n", acp_rank());
#endif
        }
        else { /* odd rank */
            while (connect(sock_connect, (struct sockaddr*)&dstaddr, sizeof(dstaddr)) < 0){
                if (errno != EINTR && errno != EAGAIN && errno != ECONNREFUSED) {
                    perror("even connect() failed:");
                    goto exit;
                }
                else{
                    sleep(1);
                }
            }
#ifdef TCP_DEBUG
            fprintf(stderr, "%d socekt connect fin for connect odd.\n", acp_rank());
#endif
            while ((sock_accept = accept(sock_s, (struct sockaddr *)&srcaddr, &addrlen)) < 0) {
                perror("odd accept() failed:");
                goto exit;
            }
#ifdef TCP_DEBUG
            fprintf(stderr, "%d socekt accept fin for connect odd.\n", acp_rank());   
#endif
        }
    }
#ifdef TCP_DEBUG
    fprintf(stderr, "%d connect establish.\n", acp_rank());   
#endif

    /* socket close */
    if (sock_s) {
        close(sock_s);
    }
#ifdef TCP_DEBUG
    fprintf(stderr, "%d close sochk_s.\n", acp_rank());   
#endif

    /* get device names in the system */
    dev_list = ibv_get_device_list(&num_devices);
    if (!dev_list) {
        fprintf(stderr, "failed to get IB devices list\n");
        rc = -1;
        goto exit;
    }
    
    /* if there isn't any IB device in host */
    if (num_devices < 1) {
        fprintf(stderr, "found %d device(s)\n", num_devices);
        rc = -1;
        goto exit;
    }
#ifdef DEBUG
    fprintf(stdout, "found %d device(s)\n", num_devices);
    fflush(stdout);
#endif
    
    if (!dev_list[0]) {
        fprintf(stderr, "IB device wasn't found\n");
        rc = -1;
        goto exit;
    }
    dev_name = strdup(ibv_get_device_name(dev_list[0]));
#ifdef DEBUG
    fprintf(stdout, "First IB device name is %s\n", dev_name);
    fflush(stdout);
#endif
    
    /* get device handle */
    res.ib_ctx = ibv_open_device(dev_list[0]);
    if (!res.ib_ctx) {
        fprintf(stderr, "failed to open device\n");
        rc = -1;
        goto exit;
    }
    
    /* free device list */
    ibv_free_device_list(dev_list);
    dev_list = NULL;
    
    /* get port attribution */
    if (ibv_query_port(res.ib_ctx, ib_port, &res.port_attr)) {
        fprintf(stderr, "ibv_query_port on port %u failed\n", ib_port);
        rc = -1;
        goto exit;
    }

    /* allocate Protection Domain */
    res.pd = ibv_alloc_pd(res.ib_ctx);
    if (!res.pd) {
        fprintf(stderr, "ibv_alloc_pd failed\n");
        rc = -1;
        goto exit;
    }
    
    /* each side will send only one WR, */
    /*   so Completion Queue with 1 entry is enough */
    cq = ibv_create_cq(res.ib_ctx, cq_size, NULL, NULL, 0);
    if (!cq) {
        fprintf(stderr, 
                "failed to create CQ with %u entries\n", cq_size);
        rc = -1;
        goto exit;
    }


    /* register the memory buffer */
    mr_flags = IBV_ACCESS_LOCAL_WRITE | 
        IBV_ACCESS_REMOTE_READ |
        IBV_ACCESS_REMOTE_WRITE |
        IBV_ACCESS_REMOTE_ATOMIC;
    res.mr = ibv_reg_mr(res.pd, sysmem, syssize, mr_flags);
    if (!res.mr) {
        fprintf(stderr, 
                "ibv_reg_mr failed with mr_flags=0x%x\n", 
                mr_flags);
        rc = -1;
        goto exit;
    }
#ifdef DEBUG
    fprintf(stdout, 
            "MR was registered with addr=%lx, lkey=%u, rkey=%u, flags=%u\n",
            (uintptr_t)sysmem, res.mr->lkey, res.mr->rkey, mr_flags);
    fflush(stdout);
#endif
    
    /* create the Queue Pair */
    qp = (struct ibv_qp **) malloc(sizeof(struct ibv_qp *) * acp_numprocs);
    if (qp == NULL) {
        fprintf(stderr, "failed to malloc qp\n");
        rc = -1;
        goto exit;
    }
    memset(qp, 0, sizeof(struct ibv_qp *) * acp_numprocs);
    
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.sq_sig_all = 1; /* if work request COMPLETE, CQE enqueue cq. */
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.cap.max_send_wr = MAX_WR_SIZE;
    qp_init_attr.cap.max_recv_wr = 0; /* use only first post recv */
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 0;
    
    /* local arays of queue pair number */
    local_qp_num = (uint32_t *)malloc(sizeof(uint32_t) * acp_numprocs);
    if (local_qp_num == NULL) {
        fprintf(stderr, "failed to malloc local_qp_num\n");
        rc = -1;
        goto exit;
    }
    /* temporary arays of queue pair number for data recieving */
    tmp_qp_num = (uint32_t *)malloc(sizeof(uint32_t) * acp_numprocs);
    if (tmp_qp_num == NULL) {
        fprintf(stderr, "failed to malloc tmp_qp_num\n");
        rc = -1;
        goto exit;
    }
    /* remote arays of queue pair number */
    remote_qp_num = (uint32_t *)malloc(sizeof(uint32_t) * acp_numprocs);
    if (remote_qp_num == NULL) {
        fprintf(stderr, "failed to malloc remote_qp_num\n");
        rc = -1;
        goto exit;
    }
    
    for (i = 0;i < acp_numprocs;i++) {
        qp[i] = ibv_create_qp(res.pd, &qp_init_attr);
        if (!qp[i]) {
            fprintf(stderr, "failed to create QP\n");
            rc = -1;
            goto exit;
        }
        local_qp_num[i] = qp[i]->qp_num;
        
#ifdef DEBUG
        fprintf(stdout, "QP[%d] was created, QP number=0x%x\n", 
                i, local_qp_num[i]);
        fflush(stdout);
#endif        
    }
    
    /* exchange using TCP sockets info required to connect QPs */
    local_data.addr = (uintptr_t)sysmem;
    local_data.rkey = res.mr->rkey;
    local_data.lid = res.port_attr.lid;
    local_data.rank = acp_myrank;
    smi_tb = (SMI *)malloc(sizeof(SMI) * acp_numprocs);
    if (smi_tb == NULL) {
        fprintf(stderr, "failed to malloc starter memory info table\n");
        rc = -1;
        goto exit;
    }
    memset(smi_tb, 0, sizeof(SMI) * acp_numprocs);
    
#ifdef DEBUG
    fprintf(stdout, "local address = %lx\n", local_data.addr);
    fprintf(stdout, "local rkey = %u\n", local_data.rkey);
    fprintf(stdout, "local LID = %u\n", local_data.lid);
    fflush(stdout);
#endif
    
    tmp_data.addr = local_data.addr;
    tmp_data.rkey = local_data.rkey;
    tmp_data.lid = local_data.lid;
    tmp_data.rank = local_data.rank;
    for (i = 0;i < acp_numprocs; i++) {
        tmp_qp_num[i] = local_qp_num[i];
    }
    
    /* address and rkey of my process */
    smi_tb[acp_myrank].addr = local_data.addr;
    smi_tb[acp_myrank].rkey = local_data.rkey;
    
    for (i = 0;i < acp_numprocs - 1;i++) {
        /* TCP sendrecv */
#ifdef DEBUG
        fprintf(stdout, "%d:-----------------------------\n", acp_rank());
        fflush(stdout);
#endif
        memset(&remote_data, 0, sizeof(remote_data));
        memset(remote_qp_num, 0, sizeof(uint32_t) * acp_numprocs);
        
        while (write(sock_connect, &tmp_data, sizeof(tmp_data)) < 0);
        while (recv(sock_accept, &remote_data, sizeof(remote_data), MSG_WAITALL) < 0) ;
        while (write(sock_connect, tmp_qp_num, sizeof(uint32_t) * acp_numprocs) < 0);
        while (recv(sock_accept, remote_qp_num, sizeof(uint32_t) * acp_numprocs, MSG_WAITALL) < 0);
        
        /* copy addr and rkey to start buffer information table */
        torank = remote_data.rank;
        smi_tb[torank].addr = remote_data.addr;
        smi_tb[torank].rkey = remote_data.rkey;
        
        /* save the remote side attributes, we will need it for the post SR */
#ifdef DEBUG
        fprintf(stdout, "%d: Remote address = %lx\n", acp_rank(), remote_data.addr);
        fprintf(stdout, "%d: Remote rkey = %u\n", acp_rank(), remote_data.rkey);
        fprintf(stdout, "%d: Remote rank = %d\n", acp_rank(), torank);
        fprintf(stdout, "%d: Remote LID = %u\n", acp_rank(), remote_data.lid);
        fprintf(stdout, "%d: Remote QP number = %d 0x%x\n", acp_rank(), torank, remote_qp_num[acp_myrank]);
        fflush(stdout);
#endif
    
        /* modify the QP to init */
        /* initialize ibv_qp_attr */
        memset(&attr, 0, sizeof(attr));
        
        /* set attribution for INIT */
        attr.qp_state = IBV_QPS_INIT;
        attr.port_num = ib_port; /* physical port number (1...n)*/
        attr.pkey_index = 0; /* normally 0 */
        attr.qp_access_flags = 
            IBV_ACCESS_LOCAL_WRITE | 
            IBV_ACCESS_REMOTE_READ |
            IBV_ACCESS_REMOTE_WRITE |
            IBV_ACCESS_REMOTE_ATOMIC;
        
        /* set flag for INIT */
        flags = IBV_QP_STATE |
            IBV_QP_PKEY_INDEX | 
            IBV_QP_PORT | 
            IBV_QP_ACCESS_FLAGS;
        
        rc = ibv_modify_qp(qp[torank], &attr, flags);
        
        if (rc) {
            fprintf(stderr, "%d: change QP state to INIT failed\n", acp_rank());
            goto exit;
        }
#ifdef DEBUG
        fprintf(stdout, "%d: QP state was change to INIT\n", acp_rank());
        fflush(stdout);
#endif
#if 0
        /* let the client post RR to be prepared for incoming messages */
        /* prepare the scatter/gather entry */
        memset(&sge, 0, sizeof(sge));
        
        sge.addr = (uintptr_t)sysmem;
        sge.length = syssize;
        sge.lkey = res.mr->lkey;
        
        /* prepare the receive work request */
        memset(&rr, 0, sizeof(rr));
        
        rr.next = NULL;
        rr.wr_id = ACP_HANDLE_NULL;
        rr.sg_list = &sge;
        rr.num_sge = 1;
        
        /* post the Receive Request to the RQ */
        rc = ibv_post_recv(qp[torank], &rr, &bad_wr);
        
        if (rc) {
            fprintf(stderr, "%d: failed to post RR\n", acp_rank());
            goto exit;
        }
#endif
#ifdef DEBUG
        fprintf(stdout, "%d: Post Receive Request\n", acp_rank());
        fflush(stdout);
#endif
        
        /* modify the QP to RTR */
        memset(&attr, 0, sizeof(attr));
        
        /* set attribution for RTR */
        attr.qp_state = IBV_QPS_RTR;
        attr.path_mtu = res.port_attr.active_mtu;
        attr.dest_qp_num = remote_qp_num[acp_myrank];
        attr.rq_psn = 0;
        attr.max_dest_rd_atomic = 1;
        /* recommend value 0x12. miminum RNR (receive not ready) NAK timer */
        attr.min_rnr_timer = 0x12; 
        /* minimally ah_attr.dlid needs t o be filled in ah for UD */
        attr.ah_attr.dlid = remote_data.lid; 
        attr.ah_attr.sl = 0;
        attr.ah_attr.src_path_bits = 0;
        attr.ah_attr.is_global = 0;
        attr.ah_attr.port_num = ib_port;
        
        /* set flags for RTR */
        flags = IBV_QP_STATE |
            IBV_QP_AV | 
            IBV_QP_PATH_MTU | 
            IBV_QP_DEST_QPN |
            IBV_QP_RQ_PSN | 
            IBV_QP_MAX_DEST_RD_ATOMIC | 
            IBV_QP_MIN_RNR_TIMER;
        
        rc = ibv_modify_qp(qp[torank], &attr, flags);
        
        if (rc) {
            fprintf(stderr, "%d: failed to modify QP state to RTR\n", acp_rank());
            goto exit;
        }
#ifdef DEBUG
        fprintf(stdout, "%d: QP state was change to RTR\n", acp_rank());
        fflush(stdout);
#endif
        
        /* modify the QP to RTS */
        memset(&attr, 0, sizeof(attr));
        
        /* set attribution for RTS */
        attr.qp_state = IBV_QPS_RTS;
        attr.timeout = 0x12;
        attr.retry_cnt = 7; /* recommended 7 */
        attr.rnr_retry = 7; /* recommended 7 */
        attr.sq_psn = 0; /* send queue starting packet sequence number (should match remote QP’s rq_psn) */
        attr.max_rd_atomic = 1; /* # of outstanding RDMA reads and atomic operations allowed.*/
        
        /* set flag for RTS */
        flags = IBV_QP_STATE | 
            IBV_QP_TIMEOUT | 
            IBV_QP_RETRY_CNT | 
            IBV_QP_RNR_RETRY |  
            IBV_QP_SQ_PSN | 
            IBV_QP_MAX_QP_RD_ATOMIC;
        
        rc = ibv_modify_qp(qp[torank], &attr, flags);
        
        if (rc) {
            fprintf(stderr, "%d: failed to modify QP state to RTR\n", acp_rank());
            goto exit;
        }
#ifdef DEBUG
        fprintf(stdout, "%d: QP state was change to RTS\n", acp_rank());
        fflush(stdout);
#endif
        
        /* copy remote data to temporary buffer for bukects relay */
        tmp_data.addr = remote_data.addr;
        tmp_data.rkey = remote_data.rkey;
        tmp_data.lid = remote_data.lid;
        tmp_data.rank = remote_data.rank;
#ifdef DEBUG
        fprintf(stdout, "%d: tmp Remote address = %lx\n", acp_rank(), tmp_data.addr);
        fprintf(stdout, "%d: tmp Remote rkey = %u\n", acp_rank(), tmp_data.rkey);
        fprintf(stdout, "%d: tmp Remote rank = %d\n", acp_rank(), tmp_data.rank);
        fprintf(stdout, "%d: tmp Remote LID = %u\n", acp_rank(), tmp_data.lid);
        fflush(stdout);
#endif
        for (j = 0;j < acp_numprocs; j++) {
            tmp_qp_num[j] = remote_qp_num[j];
        }
    }

    /* free acp_init tmporary */
    if (local_qp_num != NULL) {
        free(local_qp_num);
        local_qp_num = NULL;
    }
    if (tmp_qp_num != NULL) {
        free(tmp_qp_num);
        tmp_qp_num = NULL;
    }
    if (remote_qp_num != NULL) {
        free(remote_qp_num);
        remote_qp_num = NULL;
    }
    pthread_create(&comm_thread_id, NULL, comm_thread_func, NULL);
    
    iacp_internal_sync();
    iacp_init_rcdbsync();
    
    /* exec internel init function dl and cl */
#ifndef ACPBLONLY
    if (iacp_init_dl()) return -1;
    if (iacp_init_cl()) return -1;
#endif
        
    return rc;
    
exit:
    /* free acp_init tmporary */
    if (local_qp_num != NULL) {
        free(local_qp_num);
        local_qp_num = NULL;
    }
    if (tmp_qp_num != NULL) {
        free(tmp_qp_num);
        tmp_qp_num = NULL;
    }
    if (remote_qp_num != NULL) {
        free(remote_qp_num);
        remote_qp_num = NULL;
    }
    
    /* close IB resource */
    if (res.mr != NULL) {
        ibv_dereg_mr(res.mr);
        res.mr = NULL;
    }
    
    for (i = 0;i < acp_numprocs;i++) {
        if (qp[i] != NULL) {
            ibv_destroy_qp(qp[i]);
            qp[i] = NULL;
        }
    }
    if (qp != NULL) {
        free(qp);
        qp = NULL;
    }
    
    if (cq != NULL) {
        ibv_destroy_cq(cq);
        cq = NULL;
    }
    
    if (res.pd != NULL) {
        ibv_dealloc_pd(res.pd);
        res.pd = NULL;
    }
    
    if (res.ib_ctx != NULL) {
        ibv_close_device(res.ib_ctx);
        res.ib_ctx = NULL;
    }
    
    if (dev_list != NULL) {
        ibv_free_device_list(dev_list);
        dev_list = NULL;
    }

    /* close socket file descriptor */
    if (sock_accept) {
        close(sock_accept);
    }
    if (sock_connect) {
        close(sock_connect);
    }
    if (sock_s) {
        close(sock_s);
    }
    
    /* free acp region */
    for (i = 0;i < rkey_cache_size ;i++) {
        if (rrmtb[i] != NULL) {
            free(rrmtb[i]);
            rrmtb[i] = NULL;
        }
    }
    
    /* free rrm tables */
    if (rrmtb != NULL) {
        free(rrmtb);
        rrmtb = NULL;
    }
    
    /* free system memory */
    if (system != NULL) {
        free(sysmem);
        sysmem = NULL;
    }
    
    /* free starter memory info */
    if (smi_tb != NULL) {
        free(smi_tb);
        smi_tb = NULL;
    }
    
    return rc;
}

int acp_init(int *argc, char ***argv)
{
///
    int rc = 0 ; /* return code */
    acpbl_input_t *ait ;

#ifdef DEBUG    
    fprintf(stdout, "acp_init() start\n");
    fflush(stdout);
#endif
///
    ait = ( acpbl_input_t * )malloc( sizeof( acpbl_input_t ) ) ;
///
    iacp_connection_information( argc, argv, ait ) ;
    acp_myrank                  = ( int      ) ait->u_inputs[ IR_MYRANK    ] ;
    acp_numprocs                = ( int      ) ait->u_inputs[ IR_NPROCS    ] ;
    acp_taskid                  = ( uint32_t ) ait->u_inputs[ IR_TASKID    ] ;
    my_port                     = ( uint16_t ) ait->u_inputs[ IR_LPORT     ] ;
    dst_port                    = ( uint16_t ) ait->u_inputs[ IR_RPORT     ] ;
    dst_addr                    = ( uint32_t ) ait->u_inputs[ IR_RHOST     ] ;
    acp_smsize                  = ( size_t   ) ait->u_inputs[ IR_SZSMEM_BL ] ;
    iacp_starter_memory_size_cl = ( size_t   ) ait->u_inputs[ IR_SZSMEM_CL ] ;
    iacp_starter_memory_size_dl = ( size_t   ) ait->u_inputs[ IR_SZSMEM_DL ] ;
///
    ///fprintf( stdout, "myrank, nprocs, taskid, myport, parent_port, parent_addr, smem, smem_cl, smem_dl:\n" ) ;
    ///fprintf( stdout, "%u, %u, %u, %u, %u, %u, %lu, %lu, %lu\n",
    ///        acp_myrank, acp_numprocs, acp_taskid, my_port, dst_port, dst_addr, acp_smsize, iacp_starter_memory_size_cl, iacp_starter_memory_size_dl ) ;
    ///fflush(stdout);
///
#ifdef DEBUG
    fprintf(stdout, 
            "rk %d np %d ss %lu mp %u dp %u da %u\n", 
            acp_myrank, acp_numprocs, acp_smsize,
            my_port, dst_port, dst_addr);
    fflush(stdout);
#endif
    ///exit ( 1 ) ;
    rc = iacp_init();
    
    /* if acp_init successes, $executed_acp_init flag is set to true */
    if ( rc == 0) {
        executed_acp_init = true;
    }
    
    free( ait ) ;
    return rc;
}

int acp_finalize(){
    
    int i; /* genral index */
    int myrank; /* my rank for command*/
    acp_handle_t tail4c; /* tail for cmdq */
    CMD *pcmdq; /* pointer of cmd */
  
#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_finalize\n", acp_rank());
    fflush(stdout);
#endif
    
#ifndef ACPBLONLY
    iacp_finalize_cl();
    iacp_finalize_dl();
#endif

    /* Insert FIN command into cmdq */
    while (tail - head == MAX_CMDQ_ENTRY - 1) ;
    iacp_free_rcdbsync();
    /* wait all process execute acp_finalize */
    iacp_internal_sync();
    /* check my rank */
    myrank = acp_rank();
    /* make a FIN command, and enqueue command Queue. */
    tail4c = tail % MAX_CMDQ_ENTRY;
    pcmdq = (CMD *)&cmdq[tail4c];
    pcmdq->rank = myrank;
    pcmdq->type = FIN;
    pcmdq->ishdl = tail;
    pcmdq->wr_id = tail;
    pcmdq->stat = ISSUED;
    
    /* update tail */
    tail++ ;
    
    /* complete communication thread */
    pthread_join(comm_thread_id, NULL);
    
    /* close IB resouce */
    if (res.mr != NULL) {
        ibv_dereg_mr(res.mr);
        res.mr = NULL;
    }
    
    for (i = 0;i < MAX_RM_SIZE;i++) {
        if (libvmrtb[i] != NULL) {
            ibv_dereg_mr(libvmrtb[i]);
            libvmrtb[i] = NULL;
        }
    }
    
    for (i = 0;i < acp_numprocs;i++) {
        if (qp[i] != NULL) {
            ibv_destroy_qp(qp[i]);
            qp[i] = NULL;
        }
    }
    if (qp != NULL) {
        free(qp);
        qp = NULL;
    }

    if (cq != NULL) {
        ibv_destroy_cq(cq);
        cq = NULL;
    }
    
    if (res.pd != NULL) {
        ibv_dealloc_pd(res.pd);
        res.pd = NULL;
    }
    
    if (res.ib_ctx != NULL) {
        ibv_close_device(res.ib_ctx);
        res.ib_ctx = NULL;
    }
    
    /* close socket file descritptor */
    if (sock_accept) {
        close(sock_accept);
    }
    if (sock_connect) {
        close(sock_connect);
    }
    
    /* free acp region */
    for (i = 0;i < rkey_cache_size ;i++) {
        if (rrmtb[i] != NULL) {
            free(rrmtb[i]);
            rrmtb[i] = NULL;
        }
    }
    if (rrmtb != NULL) {
        free(rrmtb);
        rrmtb = NULL;
    }
    if (sysmem != NULL) {
        free(sysmem);
        sysmem = NULL;
    }
    if (smi_tb != NULL) {
        free(smi_tb);
        smi_tb = NULL;
    }

    executed_acp_init = false;

#ifdef DEBUG
    fprintf(stdout, "%d: internal acp_finalize fin\n", acp_rank());
    fflush(stdout);
#endif
    
    return 0;
}

int acp_reset(int rank){

    int rc = -1; /* return code is set by -1 */
    
    rc = acp_finalize();  
    if (rc == -1) {
        return rc;
    }
    acp_myrank = rank;
    rc = iacp_init();
    
    return rc;
}
