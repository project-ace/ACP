#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* write() */
#include <string.h> /* memset() */
#include <stdlib.h> /* malloc() */
#include <pthread.h>
#include <infiniband/verbs.h>
#include "acpbl.h"

/* debug */
//#define DEBUG

/* define size */
#define MAX_RM_SIZE      255U
#define MAX_CQ_SIZE       16U
#define MAX_CMDQ_ENTRY  4096U
#define MAX_RCMDB_SIZE  4096U
#define MAX_ACK_COUNT   0x3fffffffffffffffLLU

/* bits range GA format */
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

/* define command type*/
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
#define COMPLETED               0U
#define UNISSUED                1U
#define ISSUED                  2U
#define FINISHED                3U
#define WAIT_RRM                4U
#define WAIT_TAIL               5U
#define WAIT_HEAD               6U
#define WAIT_PUT_CMD            7U
#define WAIT_PUT_RRM_FLAG       8U   
#define WAIT_PUT_CMD_FLAG       9U
#define WAIT_PUT_DST           10U
#define WAIT_ACK               11U
#define CMD_UNISSUED           12U
#define CMD_ISSUED             13U
#define CMD_WAIT_RRM           14U
#define CMD_WAIT_PUT_RRM_FLAG  15U
#define CMD_WAIT_PUT_DST       16U
#define CMD_WRITEBACK_FIN      17U

/* defina bool */
#define TRUE    1U
#define FAULSE  0U

/* resouce info for starter memory */
typedef struct resource_info{
    int myrank; /* rank ID */
    struct ibv_port_attr port_attr; /* IB port attributes */
    struct ibv_context *ib_ctx; /* device handle */
    struct ibv_pd *pd; /* PD handle */
    struct ibv_mr *mr; /* MR handle for starter memory */
} RES;

typedef struct reg_mem{
    char *addr;  /* the front of memory region */
    uint64_t rkey;  /* memory registration key in IB */
    size_t size;  /* the size of memory region */
    int lock; /* flag utilization of RM */
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
}CCMD;

typedef struct cas4_command_format{
    uint32_t data1; /* data1(4) */
    uint32_t data2; /* data2(4) */
}CAS4CMD;

typedef struct cas8_command_format{
    uint64_t data1; /* data1(8) */
    uint64_t data2; /* data2(8) */
}CAS8CMD;

typedef struct atomic4_command_format{
    uint32_t data; /* data(4) */
}ATOMIC4CMD;

typedef struct atomic8_command_format{
    uint64_t data; /* data(8) */
}ATOMIC8CMD;

typedef union command_format_exstra{
    CCMD copy_cmd; /* copy command format */
    CAS4CMD cas4_cmd; /* cas4 command format */
    CAS8CMD cas8_cmd; /* cas8 command format */
    ATOMIC4CMD atomic4_cmd; /* atomic4 command format */
    ATOMIC8CMD atomic8_cmd; /* atomic4 command format */
}CMDE;

typedef struct command_format{
    uint32_t valid_head; /* validation of CMD for RCMDBUF */
    int rank; /* issued rank */
    uint32_t type; /* command type */
    acp_handle_t ishdl; /* issued handle */
    acp_handle_t ohdl; /* order handle */
    acp_handle_t hdl; /* handle */
    uint32_t stat; /* command status */
    acp_ga_t gadst; /* destination of ga */
    acp_ga_t gasrc; /* srouce of ga */
    CMDE cmde; /* Command format exstra for GMA */
    uint32_t valid_tail; /* validation of CMD for RCMDBUF */
}CMD;

/* socket file descripter */
static int sock_accept;
static int sock_connect;

static RES res; /* resource of IB */
static struct ibv_qp **qp; /* QP handle */
static struct ibv_cq *cq; /* CQ handle */

static int acp_myrank = -1; /* my rank on acp lib*/
static int acp_numprocs = -1; /* # of process on acp lib */
static size_t acp_smsize = -1; /* the size of starter memory on acp lib */
static uint32_t my_port; /* my port */
static uint32_t dst_port; /* destination port */
static uint32_t dst_addr; /* destination ip address */

static SMI *smi_tb; /* starter memory info table */
static CMD tmpcmdq[MAX_CMDQ_ENTRY]; /* temporary comand queue */
static acp_handle_t tmphead = 1; /* temporary of the head of command queue */
static acp_handle_t tmptail = 1; /* temporary of the tail of command queue */
static acp_handle_t head = 1; /* the head of command queue */
static acp_handle_t tail = 1; /* the tail of command queue */
static RM **rrmtb; /* Remote addr/rkey info table */
struct ibv_mr *lmrtb[MAX_RM_SIZE]; /* local ibv_mr table */ 

static uint64_t ack_id = MASK_WRID_ACK;
static uint64_t ack_comp_count = 0;

/* system memory is able to rdma access */
static char *sysmem; /* starter memory address*/
static CMD *cmdq; /* comand queue */

static CMD *rcmdbuf; /* recieve buffer for command */
static uint64_t *rcmdbuf_head; /* head of rcmdbuf */
static uint64_t *rcmdbuf_tail; /* tail of rcmdbuf */

static uint64_t *head_buf; /* head buffer */
static uint64_t *tail_buf; /* tail buffer */

static CMD *putcmdbuf; /* writeback buffer */

static uint64_t *replaydata; /* replay data for atomic */

static CMD *writebackbuf; /* writeback buffer */

static RM *lrmtb; /* Local addr/rkey info table */
static RM *recv_lrmtb; /* recv buffer for local addr/rkey table */

static char *rrm_get_flag_tb; /* rrm get flag tb */
static char *rrm_reset_flag_tb; /* rrm reset flag tb */
static char *rrm_ack_flag_tb; /* rrm ack flag tb */
static char *true_flag_buf; /* get rrm flag buf */

static char *acp_buf_dl; /* acp starter memory for dl */
static char *acp_buf_cl; /* acp starter memory for cl */
static char *acp_buf_vd; /* acp starter memory for vd */

/* flags */
static int writebackbuf_flag = TRUE; /* write enable writebackbuf flag */
static int tail_buf_flag = TRUE; /* write enable writebackbuf flag */
static int replaydata_flag = TRUE; /* write enable replaydata flag */
static int putcmd_flag = TRUE; /* write enable putcmd flag */

static pthread_t comm_thread_id; /* communcation thread ID */


void acp_abort(const char *str){
  
    int i;/* general index */
    
    /* release socket file descriptor */
#ifdef DEBUG
    fprintf(stdout, "internal acp_abort\n");
#endif
    
#ifdef MUL_MOD
    iacp_abort_vd();
    iacp_abort_cl();
    iacp_abort_dl();
#endif
    /* close IB resouce */
    if (res.mr != NULL) {
        ibv_dereg_mr(res.mr);
        res.mr = NULL;
    }
    for (i = 0;i < MAX_RM_SIZE;i++) {
        if (lmrtb[i] != NULL) {
            ibv_dereg_mr(lmrtb[i]);
            lmrtb[i] = NULL;
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
    for (i = 0;i < acp_numprocs ;i++) {
        if (rrmtb[i] != NULL) {
            free(rrmtb[i]);
            rrmtb[i] = NULL;
        }
    }
    if (rrmtb != NULL) {
        free(rrmtb);
        rrmtb = NULL;
    }
    if (system != NULL) {
        free(sysmem);
        sysmem = NULL;
    }
    if (smi_tb != NULL) {
        free(smi_tb);
        smi_tb = NULL;
    }
    
    /* set NULL pointer for  sysmem */
    rcmdbuf = NULL;
    rcmdbuf_head = NULL; 
    rcmdbuf_tail = NULL; 
    head_buf = NULL; 
    tail_buf = NULL;
    putcmdbuf = NULL; 
    replaydata = NULL;
    writebackbuf = NULL; 
    lrmtb = NULL; 
    recv_lrmtb = NULL; 
    rrm_get_flag_tb = NULL;
    rrm_reset_flag_tb = NULL;
    rrm_ack_flag_tb = NULL;
    true_flag_buf = NULL; 
    
    /* initailize acpbl variables */
    acp_myrank = -1;
    tmphead = 1;
    tmptail = 1;
    head = 1;
    tail = 1;
    
    ack_id = MASK_WRID_ACK;
    ack_comp_count = 0;
    
    fprintf(stderr, "acp_abort: %s\n", str);
    /* abort(); */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_abort fin\n");
#endif
    
    return;
}

int acp_sync(void){
    
    int i;/* general index */
    char dummy1, dummy2;/* dummy buffer */
    int nprocs;/* my rank ID */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_sync\n");
#endif
    /* get # of processes */
    nprocs = acp_procs();
    
    /* if nprocs <= 0, error */
    if (nprocs <= 0) {
        return -1;
    }
    if (nprocs > 0) { /* if nprocs > 0, */
        for (i = 0; i < nprocs; i++) {
            write(sock_connect, &dummy1, sizeof(char));
            recv(sock_accept, &dummy2, sizeof(char), 0);
        }
    }
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_sync fin\n");
#endif
    
    return 0;
}

int acp_colors(void){
    
    return 1;  /* # of color is one  */
}

int acp_rank(void){
    
    return acp_myrank; /* my rank id */
}

int acp_procs(void){
    
    return acp_numprocs; /* # of processes */
}

int acp_query_rank(acp_ga_t ga){
    
    int rank = -1; /* rank of ga */
    
    rank = (int)((ga >> (COLOR_BITS + GMTAG_BITS + OFFSET_BITS)) & RANK_MASK);
    rank--;
#ifdef DEBUG_L3
    fprintf(stdout, "ga %lx rank %d\n", ga, rank);
#endif
    
    return rank;
}

int acp_query_color(acp_ga_t ga){
    
    int color = -1; /* color of ga */
    
    color = (uint32_t)((ga >> (GMTAG_BITS + OFFSET_BITS)) & COLOR_MASK);
    
#ifdef DEBUG_L3
    fprintf(stdout, "ga %lx color %d\n", ga, color);
#endif
  
    return color;
}

uint32_t query_gmtag(acp_ga_t ga){
    
    uint32_t gmtag; /* general tag global memory */
    
    gmtag = (uint32_t)((ga >> OFFSET_BITS) & GMTAG_MASK);
    
#ifdef DEBUG_L3
    fprintf(stdout, "ga %lx gmtag %d\n", ga, gmtag);
#endif
    
    return gmtag;
}


uint64_t query_offset(acp_ga_t ga){
    
    uint64_t offset; /* offset of ga */
    
    offset = (uint64_t)(ga & OFFSET_MASK);
    
#ifdef DEBUG_L3
    fprintf(stdout, "ga %lx offset %lx\n", ga, offset);
#endif
    
    return offset;
}

acp_ga_t acp_query_starter_ga(int rank){
    
    acp_ga_t ga; /* global address */
    uint32_t gmtag = TAG_SM;/* general tag of startar memory */
    uint32_t color = 0;/* color of ga */
    
    /* initialzie ga */
    ga = ACP_GA_NULL;
    
    /* set ga */
    ga = ((uint64_t)(rank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)gmtag << OFFSET_BITS);
    
#ifdef DEBUG_L3
    fprintf(stdout, "rank %d starter memory ga %lx\n", rank, ga);
#endif
    
    return ga;
}

acp_ga_t iacp_query_starter_ds_ga(int rank){
    
    acp_ga_t ga; /* global address */
    uint32_t gmtag = TAG_SM; /* general tag of startar memory */
    uint32_t color = 0; /* color of ga */
    uint64_t offset; /* offset of ga */
    
    /* initialzie ga */
    ga = ACP_GA_NULL;
    
    /* set ga */
    offset = acp_smsize + sizeof(RM) * (MAX_RM_SIZE) * 2  +
        sizeof(uint64_t) * 5 + sizeof(CMD) * MAX_CMDQ_ENTRY + 
        sizeof(CMD) * MAX_RCMDB_SIZE + sizeof(CMD) * 2  + sizeof(char) * acp_numprocs * 3 + sizeof(char);
    ga = ((uint64_t)(rank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)gmtag << OFFSET_BITS)
        + offset;
    
#ifdef DEBUG_L3
    fprintf(stdout, "rank %d starter memory ga %lx\n", rank, ga);
#endif
    
    return ga;
}

acp_ga_t iacp_query_starter_ga_ch(int rank){
  
    acp_ga_t ga; /* global address */
    uint32_t gmtag = TAG_SM;/* general tag of startar memory */
    uint32_t color = 0;/* color of ga */
    uint64_t offset; /* offset of ga */
    
    /* initialzie ga */
    ga = ACP_GA_NULL;
    
    offset = acp_smsize + sizeof(RM) * (MAX_RM_SIZE) * 2  +
        sizeof(uint64_t) * 5 + sizeof(CMD) * MAX_CMDQ_ENTRY + 
        sizeof(CMD) * MAX_RCMDB_SIZE + sizeof(CMD) * 2  + sizeof(char) * acp_numprocs * 3 + sizeof(char) +
        iacp_starter_memory_size_dl;
    /* set ga */
    ga = ((uint64_t)(rank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)gmtag << OFFSET_BITS)
        + offset;
    
#ifdef DEBUG_L3
    fprintf(stdout, "rank %d starter memory ga %lx\n", rank, ga);
#endif
  
    return ga;
}

acp_ga_t iacp_query_starter_ga_vd(int rank){
  
    acp_ga_t ga; /* global address */
    uint32_t gmtag = TAG_SM;/* general tag of startar memory */
    uint32_t color = 0;/* color of ga */
    uint64_t offset; /* offset of ga */
    
    /* initialzie ga */
    ga = ACP_GA_NULL;
    
    /* set ga */
    offset = acp_smsize + sizeof(RM) * (MAX_RM_SIZE) * 2  +
        sizeof(uint64_t) * 5 + sizeof(CMD) * MAX_CMDQ_ENTRY + 
        sizeof(CMD) * MAX_RCMDB_SIZE + sizeof(CMD) * 2  + sizeof(char) * acp_numprocs * 3 + sizeof(char) +
        iacp_starter_memory_size_dl + iacp_starter_memory_size_cl;
    ga = ((uint64_t)(rank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
        + ((uint64_t)gmtag << OFFSET_BITS)
        + offset;
  
#ifdef DEBUG_L3
    fprintf(stdout, "rank %d starter memory ga %lx\n", rank, ga);
#endif
  
    return ga;
}

acp_ga_t acp_query_ga(acp_atkey_t atkey, void* addr){
  
    acp_ga_t ga; /* global address */
    uint64_t offset = 0; /* 0: offset of ga */
    uint32_t gmtag = 0; /* 0: tag of ga */
    uint32_t color = 0; /* 0: color of ga */
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
    
    /* if my rank is equal to keyrank */
    if (keyrank == myrank) {
        if (lrmtb[gmtag].size == 0) {
            return ACP_GA_NULL;
        }
        else {
#ifdef DEBUG
            fprintf(stdout, 
                    "acp_query_ga key %lx, addr %p, rank %d gmtag %d faddr %p\n", 
                    atkey, addr, keyrank, gmtag, lrmtb[gmtag].addr);
#endif
            offset = (char *)addr - (lrmtb[gmtag].addr) ;
            ga = ((uint64_t)(keyrank + 1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
                + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
                + ((uint64_t)gmtag << OFFSET_BITS)
                + offset;
      
#ifdef DEBUG
            fprintf(stdout, "rank %d acp_query_ga ga %lx\n", myrank, ga);
#endif
            return ga;
        }
    }
    else {
        return ACP_GA_NULL;
    }
}

acp_atkey_t acp_register_memory(void* addr, size_t size, int color){
  
    int i; /* general index */
    
    struct ibv_mr *mr; /* memory register return data */
    int mr_flags; /* memory register flag */
    
    acp_atkey_t key; /* memory register key */
    uint32_t gmtag; /* tag in GA */
    uint64_t offset = 0; /* offset in ga */
    uint64_t base_offset = 0; /* offset of flag table */
    char found; /* check flag for key space */
    char insert; /* check flag for insert RM */
    int nprocs; /* # of processes */
    acp_ga_t dst, src; /* ga of dst and src */
    int myrank; /* my rank ID */
    acp_handle_t handle; /* acp handle */

#ifdef DEBUG
    fprintf(stdout, "internal acp_register_memoery\n");
#endif
    
    
    /* color is only 0 */
    color = 0; 
    
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
    
#ifdef DEBUG
    fprintf(stdout, "mr address %p size %ld\n", mr, sizeof(mr));
#endif
    if (NULL == mr) {
        fprintf(stderr, "acp_register_memory error\n");
        return ACP_ATKEY_NULL;
    }
#ifdef DEBUG
    fprintf(stdout, 
            "MR was registered with addr=%lx, lkey=%u, rkey=%u, flags=%u\n",
            (uintptr_t)addr, mr->lkey, mr->rkey, mr_flags);
#endif
  
    /* search invalid tag, and set tag */
    found = FAULSE; /* empty tag flag */
    insert = FAULSE; /* insert tag flag  */
    for (i = 0;i < MAX_RM_SIZE;i++) {
        if (lrmtb[i].size == 0) { /* find empty tag */
            found = TRUE;
            if (lrmtb[i].lock == FAULSE) { /* find insertable tag */
                insert = TRUE; /* inserted  */
                lmrtb[i] = mr;
                lrmtb[i].rkey = mr->rkey;
                lrmtb[i].addr = addr;
                lrmtb[i].size = size;
                lrmtb[i].lock = TRUE; /* locked this tag */
#ifdef DEBUG
                fprintf(stdout, 
                        "insert data into lrmtb[%d] addr %p size %lu, lock flag %d\n", 
                        i, lrmtb[i].addr, lrmtb[i].size, lrmtb[i].lock);
#endif
                break;
            }
        }
    }
  
    /* not found key space */
    if (found == FAULSE) {
        ibv_dereg_mr(mr);
        return ACP_ATKEY_NULL;
    }
    else if (found == TRUE && insert == FAULSE) {
        /* set base offset in order to set a reset flag */
        base_offset = acp_smsize + sizeof(RM) * (MAX_RM_SIZE) * 2  +
            sizeof(uint64_t) * 5 + sizeof(CMD) * MAX_CMDQ_ENTRY + 
            sizeof(CMD) * MAX_RCMDB_SIZE + sizeof(CMD) * 2 ;
        
        /* tell to flush rkey cache */
        for (i = 0;i < nprocs;i++) {
            if ( TRUE == rrm_get_flag_tb[i]) {
                dst = acp_query_starter_ga(i);
                src = acp_query_starter_ga(myrank);
                handle = acp_copy(dst + base_offset + sizeof(char) * (acp_numprocs  + myrank),
                                  src + base_offset + sizeof(char) * i, 
                                  sizeof(char), ACP_HANDLE_NULL);
            }
        }
        /* complete until handle */
        acp_complete(handle);
        
        /* enable empty tag */
        for (i = 0;i < MAX_RM_SIZE;i++) {
            if (lrmtb[i].size == 0) {
                ibv_dereg_mr(lmrtb[i]);
                lmrtb[i] = NULL;
                lrmtb[i].lock = FAULSE;
            }
        }
#ifdef DEBUG 
        fprintf(stdout, "acp_regsiter_memory: nprocs %d\n", nprocs);
#endif
        /* wait untitl get rrm flag table == reset rrm flag table */
        while (1) {
#ifdef DEBUG
                fprintf(stdout, "getflag ");
                for (i = 0;i < nprocs;i++) {
                    fprintf(stdout, "%d ", rrm_get_flag_tb[i]);
                }
                fprintf(stdout, "\n");
                fprintf(stdout, "ackflag ");
                for (i=0;i < nprocs;i++) {
                    fprintf(stdout, "%d ", rrm_ack_flag_tb[i]);
                }
                fprintf(stdout, "\n");
#endif
                if (0 == memcmp(rrm_get_flag_tb, rrm_ack_flag_tb, sizeof(char) * nprocs)) {
                    break;
                }
            }
            /* initialization rrm flag table */
            memset(rrm_get_flag_tb, 0, sizeof(char) * nprocs);
            memset(rrm_ack_flag_tb, 0, sizeof(char) * nprocs);
            
            /* insesrt new memory region */
            for (i = 0;i < MAX_RM_SIZE;i++) {
                if (lrmtb[i].size == 0) {
                    found = TRUE;
                    if (lrmtb[i].lock == FAULSE) {
                        insert = TRUE;
                        lmrtb[i] = mr;
                        lrmtb[i].rkey = mr->rkey;
                        lrmtb[i].addr = addr;
                        lrmtb[i].size = size;
                        lrmtb[i].lock = TRUE;
#ifdef DEBUG
                        fprintf(stdout, 
                                "insert data into lrmtb[%d] addr %p size %lu, lock flag %d\n", 
                                i, lrmtb[i].addr, lrmtb[i].size, lrmtb[i].lock);
#endif
                        break;
                    }
                }
            }
            /* set acp atkey for new memory region */
            gmtag = i;
            key = ((uint64_t)(acp_rank()+1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
            + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
            + ((uint64_t)gmtag << OFFSET_BITS)
            + offset;
#ifdef DEBUG
            fprintf(stdout, "rank %d atkey %lx gmtag %d\n", acp_myrank, key, gmtag);
#endif
            
            return key;
    }
    else {
        /* set gmtag */
        gmtag = i;
        /* set acp atkey */
        key = ((uint64_t)(acp_rank()+1) << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
            + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
            + ((uint64_t)gmtag << OFFSET_BITS)
            + offset;
#ifdef DEBUG
        fprintf(stdout, "rank %d atkey %lx gmtag %d\n", acp_myrank, key, gmtag);
#endif
        
        return key;
    }
#ifdef DEBUG
    fprintf(stdout, "internal acp_register_memoery fin\n");
#endif
}
    
int acp_unregister_memory(acp_atkey_t atkey){
    
    int rank; /* rank of atkey */
    uint32_t gmtag; /* tag of atkey */
    uint32_t color; /*color of atkey */
    
    /* get global memory tag of atkey */
    gmtag = query_gmtag(atkey);
    /* get rank of atkey */
    rank = acp_query_rank(atkey);
    /* get color of atkey */
    color = acp_query_color(atkey);
    
    /* if starter memory */
    if (gmtag == TAG_SM) {
        return -1;
    }
    else { /* if global memory */
        if (lrmtb[gmtag].size != 0) {
            lrmtb[gmtag].size = 0;
            return 0;
        }
        else {
            return -1;
        }
    }
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
    fprintf(stdout, "gmtag = %d\n", gmtag);
#endif
    /* if rank of ga is my rank, */
    if (rank == acp_myrank) {
        /* ga tag is TAG_SM,  */
        if (gmtag == TAG_SM) {
            base_addr = sysmem;
        }
        /* ga tag is general global memory */
        else {
            if (lrmtb[gmtag].size != 0) {
                base_addr = (char *)lrmtb[gmtag].addr;
#ifdef DEBUG_L3
                fprintf(stdout, 
                        "LRMTB: tag %d addr %p size %lu\n",
                        gmtag, lrmtb[gmtag].addr, lrmtb[gmtag].size);
#endif
            }
            else {
                return NULL;
            }
        }
    }
    else {
        return NULL;
    }
    /* set address */
    addr = base_addr + offset;
    
    return addr;
}

acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, size_t size, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
  
#ifdef DEBUG
    fprintf(stdout, "internal acp_copy\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
  
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = COPY;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.copy_cmd.size = size;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx size = %lu\n", 
            tmptail, tmptail4c, pcmdq->hdl, pcmdq->cmde.copy_cmd.size);
#endif
  
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_copy fin\n");
#endif
  
    return hdl;
}

acp_handle_t acp_cas4(acp_ga_t dst, acp_ga_t src, uint32_t oldval, uint32_t newval, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
  
#ifdef DEBUG
    fprintf(stdout, "internal acp_cas4\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = CAS4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.cas4_cmd.data1 = oldval;
    pcmdq->cmde.cas4_cmd.data2 = newval;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx, data1 %u, data2 %u\n",
            tmptail, tmptail4c, pcmdq->hdl,  pcmdq->cmde.cas4_cmd.data1, pcmdq->cmde.cas4_cmd.data2) ;
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_cas4 fin\n");
#endif
  
    return hdl;
}

acp_handle_t acp_cas8(acp_ga_t dst, acp_ga_t src, uint64_t oldval, uint64_t newval, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_cas8\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = CAS8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.cas8_cmd.data1 = oldval;
    pcmdq->cmde.cas8_cmd.data2 = newval;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;

#ifdef DEBUG
    fprintf(stdout, "tail %lx tmpcmdq[%lx].hdl = %lx data1 %lu data2 %lu\n", 
            tmptail, tmptail4c, pcmdq->hdl,   pcmdq->cmde.cas8_cmd.data1, pcmdq->cmde.cas8_cmd.data2);
#endif
  
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_cas8 fin\n");
#endif
    
    return hdl;
}

acp_handle_t acp_swap4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_swap4\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = SWAP4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %u\n", 
            tmptail, tmptail4c, pcmdq->hdl,  pcmdq->cmde.atomic4_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_swap4 fin\n");
#endif
    
    return hdl;
}

acp_handle_t acp_swap8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_swap8\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
  
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = SWAP8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx valude %lu\n", 
            tmptail, tmptail4c, pcmdq->hdl,   pcmdq->cmde.atomic8_cmd.data);
#endif
  
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_swap8 fin\n");
#endif
    
    return hdl;
}

acp_handle_t acp_add4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_add4\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = ADD4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %u\n", 
            tmptail, tmptail4c, pcmdq->hdl,   pcmdq->cmde.atomic4_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_add4 fin\n");
#endif
  
    return hdl;
}

acp_handle_t acp_add8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_add8\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = ADD8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %lu\n", 
            tmptail, tmptail4c, pcmdq->hdl,   pcmdq->cmde.atomic8_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_add8 fin\n");
#endif
  
    return hdl;
}

acp_handle_t acp_xor4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_xor4\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = XOR4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %u\n", 
            tmptail, tmptail4c, pcmdq->hdl, pcmdq->cmde.atomic4_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_xor4 fin\n");
#endif
    
    return hdl;
}

acp_handle_t acp_xor8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_xor8\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = XOR8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %lu \n", 
            tmptail, tmptail4c, pcmdq->hdl, pcmdq->cmde.atomic8_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_xor8 fin\n");
#endif
    
    return hdl;
}

acp_handle_t acp_or4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_or4\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = OR4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %u\n", 
            tmptail, tmptail4c, pcmdq->hdl, pcmdq->cmde.atomic4_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_or4 fin\n");
#endif
  
    return hdl;
}

acp_handle_t acp_or8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
  
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_or8\n");
#endif
  
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = OR8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %lu\n", 
            tmptail, tmptail4c, pcmdq->hdl, pcmdq->cmde.atomic8_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_or8 fin\n");
#endif
    
    return hdl;
}

acp_handle_t acp_and4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_and4\n");
#endif

    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = AND4;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic4_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %u\n", 
            tmptail, tmptail4c, pcmdq->hdl, pcmdq->cmde.atomic4_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_and4 fin\n");
#endif
    
    return hdl;
}

acp_handle_t acp_and8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order){
    
    acp_handle_t hdl; /* handle of copy */
    acp_handle_t tmptail4c;/* tail of cmdq */
    int myrank;/* my rank */
    CMD *pcmdq;/* pointer of cmdq */
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_and8\n");
#endif
    
    /* if queue is full, return ACP_HANDLE_NULL */
    if (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) {
        return ACP_HANDLE_NULL;
    }
    
    /* check my rank */
    myrank = acp_rank();
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    
    /* make a command, and enqueue command Queue. */ 
    pcmdq->valid_head = TRUE;
    pcmdq->rank = myrank;
    pcmdq->type = AND8;
    pcmdq->ohdl = order;
    pcmdq->stat = UNISSUED;
    pcmdq->gasrc = src;
    pcmdq->gadst = dst;
    pcmdq->cmde.atomic8_cmd.data = value;
    hdl = tmptail;
    pcmdq->hdl = hdl;
    pcmdq->ishdl = hdl;
    pcmdq->valid_tail = TRUE;
    
#ifdef DEBUG
    fprintf(stdout, 
            "tail %lx tmpcmdq[%lx].hdl = %lx value %lu\n", 
            tmptail, tmptail4c, pcmdq->hdl, pcmdq->cmde.atomic8_cmd.data);
#endif
    
    /* update tail */
    tmptail++;
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_and8 fin\n");
#endif
    
    return hdl;
}

void acp_complete(acp_handle_t handle){
  
#ifdef DEBUG
    fprintf(stdout, "internal acp_complete\n"); 
#endif
    
    /* if cmdq have no command, return */
    if (head == tail && tmphead == tmptail) {
#ifdef DEBUG
        fprintf(stdout, "acp_complet: cmdq is empty\n");
#endif
        return;
    }
    
    /* check handle */
    /* if disable handle, return */
    if (handle == ACP_HANDLE_NULL) {
#ifdef DEBUG
        fprintf(stdout, "acp_complete: handle is ACP_HANDLE_NULL\n");
#endif
        return;
    }
    /* handle is finished  */
    if (handle < head) {
#ifdef DEBUG
        fprintf(stdout, "acp_complete: handle is alway finished\n");
#endif
        return;
    }
    /* handle is not issued */
    if (handle >= tmptail) {
#ifdef DEBUG
        fprintf(stdout, "acp_complete: handle is not issued handle %lx tail %lu\n", handle, tail);
#endif
        return;
    }
    /* if handle is ACP_HANDLE_ALL, */
    /* wait complete all issued copy */
    if (handle == ACP_HANDLE_ALL) {
        handle = tmptail;
    }
    
#ifdef DEBUG
    fprintf(stdout, 
            "acp_complete: head = %lx tail = %lx handle = %lx\n", 
            head, tail, handle);
#endif
  
    /* check status of handle if it is COMPLETED or not. */
    while (head <= handle);
           
#ifdef DEBUG
    fprintf(stdout, "internal acp_complete fin\n"); 
#endif
    
    return;
}

int acp_inquire(acp_handle_t handle){

#ifdef DEBUG
    fprintf(stdout, "internal acp_inquire\n"); 
#endif
    
    /* if cmdq have no command, return */
    if (head == tail && tmphead == tmptail) {
#ifdef DEBUG
        fprintf(stdout, "acp_inquire: cmdq is empty\n");
#endif
        return 0;
    }
    
    /* check */
    /* if disable handle, return */
    if (handle == ACP_HANDLE_NULL) {
#ifdef DEBUG
        fprintf(stdout, "acp_inquire: handle is ACP_HANDLE_NULL\n");
#endif
        return 0;
    }
    /* handle is finished  */
    if (handle < head) {
#ifdef DEBUG
        fprintf(stdout, "acp_inquire: handle is alway finished\n");
#endif
        return 0;
    }
    /* handle is not issued */
    if (handle >= tmptail) {
#ifdef DEBUG
        fprintf(stdout, "acp_inquire: handle is not issued. handle %lx tmptail %lx\n", handle, tmptail);
#endif
        return 0;
    }
    /* if handle is ACP_HANDLE_ALL, */
    /*   wait complete all issued copy */
    if (handle == ACP_HANDLE_ALL) {
        handle = tmptail;
    }
    
#ifdef DEBUG
    fprintf(stdout, 
            "acp_inquire: head = %lx tail = %lx handle = %lx\n", 
            head, tail, handle);
#endif
    
    /* check status of handle if it is COMPLETED or not.*/ 
    if (head <= handle) {
        return 1;
    }
    else {
        return 0;
    }
    
#ifdef DEBUG
    fprintf(stdout, "internal acp_inquire fin\n"); 
#endif
    
}

/* get remote register memory table */
int getlrm(acp_handle_t handle, int torank){
  
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    
    /* set local buffer info  */
    sge.addr = (uintptr_t)recv_lrmtb; 
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(RM) * MAX_RM_SIZE;
    
#ifdef DEBUG
    fprintf(stdout, "get lrm length %d\n", sge.length);
#endif
  
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end*/
    sr.wr_id =  handle;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "get lrm wr_id %lx handle %lx\n", sr.wr_id, handle);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_READ;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + acp_smsize;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }

#ifdef DEBUG
    fprintf(stdout, "getlrm ibv_post_send return code = %d\n", rc);
#endif

    return rc;
}

/* get remote register memory table */
int writebackcmdq(uint64_t idx){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    uint64_t rcmdqidx; /* index of rcmdq */
    int torank; /* target rank */
    
    /* target rank */
    torank = rcmdbuf[idx].rank;
    
    /* copy rcmdq[index] to writeback buffer*/
    memcpy(writebackbuf, &rcmdbuf[idx], sizeof(CMD));
    writebackbuf->stat = FINISHED;
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    
    /* set local buffer info  */
    sge.addr = (uintptr_t)writebackbuf;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(CMD);
    
#ifdef DEBUG
    fprintf(stdout, "write back cmdq length %d\n", sge.length);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end*/
    sr.wr_id =  writebackbuf->hdl;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "writeback cmdq  wr_id %lx handle %lx\n", sr.wr_id, writebackbuf->ishdl);
#endif
    
    /* get Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    rcmdqidx = (writebackbuf->ishdl) % MAX_CMDQ_ENTRY;
#ifdef DEBUG
    fprintf(stdout, "writeback cmdq  wr_id %lx handle %lx rcmdqidx %lx torank %d\n",
            sr.wr_id, writebackbuf->ishdl, rcmdqidx, torank);
#endif
    
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + 
        acp_smsize + sizeof(RM) * MAX_RM_SIZE * 2 + sizeof(uint64_t) * 5 + sizeof(CMD) * rcmdqidx;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
#ifdef DEBUG
    fprintf(stdout, "writeback cmdq  ibv_post_send return code = %d\n", rc);
#endif
    
    return rc;
}

int gethead(acp_handle_t handle, int torank){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
   
    /* set local buffer info  */
    sge.addr = (uintptr_t)head_buf;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(uint64_t);
#ifdef DEBUG
    fprintf(stdout, "get head length %d\n", sge.length);
#endif
  
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
  
    /* Work request ID is set by Acp_handle_queue end*/
    sr.wr_id =  handle;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "get head wr_id %lx handle %lx\n", sr.wr_id, handle);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_READ;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr 
        + acp_smsize + sizeof(RM) * MAX_RM_SIZE * 2; 
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
#ifdef DEBUG
    fprintf(stdout, "get head ibv_post_send return code = %d\n", rc);
#endif
    
    return rc;
}

int gettail(acp_handle_t handle, int torank){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    
    /* set local buffer info  */
    sge.addr = (uintptr_t)tail_buf;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    sge.length = sizeof(uint64_t);
    
#ifdef DEBUG
    fprintf(stdout, "get tail length %d\n", sge.length);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end */
    sr.wr_id =  handle;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "get tail wr_id %lx handle %lx\n", sr.wr_id, handle);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
    sr.send_flags = IBV_SEND_SIGNALED;
   
    /* Set remote address and rkey in send work request */
    sr.wr.atomic.remote_addr = smi_tb[torank].addr + 
        acp_smsize + sizeof(RM) * MAX_RM_SIZE * 2 + sizeof(uint64_t);
    sr.wr.atomic.rkey = smi_tb[torank].rkey;
    sr.wr.atomic.compare_add = 1ULL;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
#ifdef DEBUG
    fprintf(stdout, "get tail ibv_post_send return code = %d\n", rc);
#endif
    
    return rc;
}
 
int putrrmgetflag(acp_handle_t handle, int torank){  
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
    fprintf(stdout, "put rrm get flag length %d\n", sge.length);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end */
    sr.wr_id =  handle;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    
#ifdef DEBUG
    fprintf(stdout, "put rrm get flag wr_id %lx handle %lx\n", sr.wr_id, handle);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + 
        acp_smsize + sizeof(RM) * MAX_RM_SIZE * 2 + sizeof(uint64_t) * 5 + sizeof(CMD) * MAX_CMDQ_ENTRY + 
        sizeof(CMD) * MAX_RCMDB_SIZE + sizeof(CMD) * 2  + sizeof(char) * myrank;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
    
#ifdef DEBUG
    fprintf(stdout, "put rrm get flag ibv_post_send return code = %d\n", rc);
#endif
    
    return rc;
}

int putrrmackflag(acp_handle_t handle, int torank){  
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
    fprintf(stdout, "put rrm ack flag length %d ture_flag %d torank %d\n", sge.length, *true_flag_buf, torank);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    /* Work request ID is set by ack_id */
    sr.wr_id =  handle;
    sr.sg_list = &sge;
    sr.num_sge = 1;
    
#ifdef DEBUG
    fprintf(stdout, "put rrm ack flag wr_id %lx handle %lx\n", sr.wr_id, handle);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + 
        acp_smsize + sizeof(RM) * MAX_RM_SIZE * 2 + sizeof(uint64_t) * 5 + sizeof(CMD) * MAX_CMDQ_ENTRY + 
        sizeof(CMD) * MAX_RCMDB_SIZE + sizeof(CMD) * 2  + sizeof(char) * acp_numprocs * 2 + sizeof(char) * myrank;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
    
#ifdef DEBUG
    fprintf(stdout, "put rrm ack flag ibv_post_send return code = %d\n", rc);
#endif
    
    return rc;
}

int putreplaydata(acp_handle_t handle, int dstrank, int dstgmtag, uint64_t dstoffset, int flguint64){
    
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));

    /* local buffer info */
    sge.addr = (uintptr_t)replaydata;
    sge.lkey = res.mr->lkey;
    
    /* set message size */
    if (TRUE == flguint64) {
        sge.length = sizeof(uint64_t);
#ifdef DEBUG
        fprintf(stdout, "put replaydata u64data %lu\n", (uint64_t)*replaydata);
#endif
    }
    else {
        sge.length = sizeof(uint32_t);
#ifdef DEBUG
        fprintf(stdout, "put replaydata u32data %u\n", (uint32_t)*replaydata);
#endif  
    }
#ifdef DEBUG
    fprintf(stdout, "put replaydata length %d\n", sge.length);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by handle + 0x8000000000000000LLU */
    sr.wr_id =  handle;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "put replaydata wr_id %lx handle %lx\n", sr.wr_id, handle);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    /* using starter memory */
    if ( dstgmtag == TAG_SM) {
        sr.wr.rdma.remote_addr = smi_tb[dstrank].addr + dstoffset;
        sr.wr.rdma.rkey = smi_tb[dstrank].rkey;
    }
    /* using general memory */
    else {
        sr.wr.rdma.remote_addr = (uintptr_t)(rrmtb[dstrank][dstgmtag].addr) + dstoffset;
        sr.wr.rdma.rkey = rrmtb[dstrank][dstgmtag].rkey;
    }
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[dstrank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
    
#ifdef DEBUG
    fprintf(stdout, "put replaydata ibv_post_send return code = %d\n", rc);
#endif

    return rc;
}

int putcmd(acp_handle_t handle, int torank, uint64_t rcmdbid){
  
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int rc; /* return code */
    int idx;/* index of cmdq */
    
    if (*tail_buf - *head_buf > MAX_RCMDB_SIZE) {
#ifdef DEBUG
        fprintf(stdout, "putcmd wr_id %lx handle %lx *tail_buf %lx *head_buf %lx\n", sr.wr_id, handle, *tail_buf, *head_buf);
#endif
        return -1;
    }
    
    /* set index for cmdq */
    idx = handle % MAX_CMDQ_ENTRY;
    
    /* copy cmdq[index] into putcmdbuf */
    memcpy(putcmdbuf, &cmdq[idx], sizeof(CMD));
    putcmdbuf->stat = CMD_UNISSUED;
    
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    /* local buffer info */
    sge.addr = (uintptr_t)putcmdbuf;
    sge.lkey = res.mr->lkey;
#ifdef DEBUG
    fprintf(stdout, "putcmd rcmdbid %lx\n", rcmdbid);
#endif
    /* set message size */
    sge.length = sizeof(CMD);
    
#ifdef DEBUG
    fprintf(stdout, "putcmd length %d\n", sge.length);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by ACP_handle queue end */
    sr.wr_id =  handle;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "putcmd wr_id %lx handle %lx\n", sr.wr_id, handle);
#endif
    
    /* Set Get opcode in send work request */
    sr.opcode = IBV_WR_RDMA_WRITE;
    
    /* Set remote address and rkey in send work request */
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + acp_smsize + 
        sizeof(RM) * MAX_RM_SIZE * 2 + sizeof(uint64_t) * 5 + sizeof(CMD) * MAX_CMDQ_ENTRY + sizeof(CMD) * rcmdbid;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
#ifdef DEBUG
    fprintf(stdout, "putcmd ibv_post_send return code = %d\n", rc);
#endif
    
    return rc;
}

int icopy(acp_handle_t handle, 
	  int dstrank, uint32_t dstgmtag, uint64_t dstoffset, 
	  int srcrank, uint32_t srcgmtag, uint64_t srcoffset,
	  size_t size)
{
    struct ibv_sge sge; /* scatter/gather entry */
    struct ibv_send_wr sr; /* send work reuqest */
    struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
    
    int torank = -1; /* remote rank */
    int rc; /* return code */
    
    uint64_t local_offset, remote_offset;/* local and remote offset */
    uint32_t local_gmtag, remote_gmtag;/* local and remote GMA tag */
    int myrank;/* my rank id */
    
    /* get my rank*/
    myrank = acp_rank();
    
#ifdef DEBUG
    fprintf(stdout, "internal icopy\n");
#endif
    
    /* print ga rank, index, offset */
#ifdef DEBUG
    fprintf(stdout, 
            "mr %d sr %x st %x so %lx dr %x dt %x do %lx\n", 
            myrank, srcrank, srcgmtag, srcoffset, 
            dstrank, dstgmtag, dstoffset);
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
    fprintf(stdout, "target rank %d\n", torank);
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
        if (lrmtb[local_gmtag].size != 0) {
            sge.addr = (uintptr_t)(lrmtb[local_gmtag].addr) + local_offset;
            sge.lkey = lmrtb[local_gmtag]->lkey;
        }
        else {
            fprintf(stderr, "Invalid TAG of This GA");
            rc = -1;
            return rc;
        }
    }
    
    /* set message size */
    sge.length = size;
#ifdef DEBUG
    fprintf(stdout, "copy len %d\n", sge.length);
#endif
    
    /* prepare the send work request */
    memset(&sr, 0, sizeof(sr));
    sr.next = NULL;
    
    /* Work request ID is set by Acp_handle_queue end */
    sr.wr_id =  handle;
    sr.sg_list = &sge;
    sr.num_sge = 1;
#ifdef DEBUG
    fprintf(stdout, "icopy wr_id  %lx handle %lx\n", sr.wr_id, handle);
#endif
    
    /* Set put opcode in send work request */
    if (myrank == srcrank) {
        sr.opcode = IBV_WR_RDMA_WRITE;
    }
    /* Set Get opcode in send work request */
    else if (myrank == dstrank) {
        sr.opcode = IBV_WR_RDMA_READ;
    }
    /* Set remote address and rkey in send work request */
    /* using starter memory */
    if ( remote_gmtag == TAG_SM) {
        sr.wr.rdma.remote_addr = smi_tb[torank].addr + remote_offset;
        sr.wr.rdma.rkey = smi_tb[torank].rkey;
    }
    /* using general memory */
    else {
        sr.wr.rdma.remote_addr = (uintptr_t)(rrmtb[torank][remote_gmtag].addr) + remote_offset;
        sr.wr.rdma.rkey = rrmtb[torank][remote_gmtag].rkey;
    }
    
    if (myrank == srcrank) {
#ifdef DEBUG
        fprintf(stdout, "put addr %lx rkey = %u\n", sr.wr.rdma.remote_addr, sr.wr.rdma.rkey);
#endif
    }
    else if (myrank == dstrank) {
#ifdef DEBUG
        fprintf(stdout, "get addr %lx rkey = %u\n", sr.wr.rdma.remote_addr, sr.wr.rdma.rkey);
#endif
    }
    
    /* post send by ibv_post_send */
    rc = ibv_post_send(qp[torank], &sr, &bad_wr);
    if (rc) {
        fprintf(stderr, "failed to post SR\n");
        exit(rc);
    }
#ifdef DEBUG
    fprintf(stdout, "icopy ibv_post_send return code = %d\n",rc);
#endif
    
#ifdef DEBUG
    fprintf(stdout, "internal icopy fin\n");
#endif
    
    return rc;
}

void selectatomic(void *srcaddr, CMD *cmdq){
    
    uint64_t *srcaddr8;
    uint32_t *srcaddr4;
    
#ifdef DEBUG
    fprintf(stdout, "internal selectatomic\n");
#endif
    
    switch (cmdq->type) {
    case CAS4:
        srcaddr4 = (uint32_t *)srcaddr;
        *replaydata = sync_val_compare_and_swap_4(srcaddr4, cmdq->cmde.cas4_cmd.data1, cmdq->cmde.cas4_cmd.data2);
        break;
    case CAS8:
        srcaddr8 = (uint64_t *)srcaddr;
        *replaydata = sync_val_compare_and_swap_8(srcaddr8, cmdq->cmde.cas8_cmd.data1, cmdq->cmde.cas8_cmd.data2);
        break;
    case SWAP4:
        srcaddr4 = (uint32_t *)srcaddr;
        *replaydata = sync_swap_4(srcaddr4, cmdq->cmde.atomic4_cmd.data);
        break;
    case SWAP8:
        srcaddr8 = (uint64_t *)srcaddr;
        *replaydata = sync_swap_8(srcaddr8, cmdq->cmde.atomic8_cmd.data);
        break;
    case ADD4:
        srcaddr4 = (uint32_t *)srcaddr;
        *replaydata = sync_fetch_and_add_4(srcaddr4, cmdq->cmde.atomic4_cmd.data);
        break;
    case ADD8:
        srcaddr8 = (uint64_t *)srcaddr;
        *replaydata = sync_fetch_and_add_8(srcaddr8, cmdq->cmde.atomic8_cmd.data);
        break;
    case XOR4:
        srcaddr4 = (uint32_t *)srcaddr;
        *replaydata = sync_fetch_and_xor_4(srcaddr4, cmdq->cmde.atomic4_cmd.data);
        break;
    case XOR8:
        srcaddr8 = (uint64_t *)srcaddr;
        *replaydata = sync_fetch_and_xor_8(srcaddr8, cmdq->cmde.atomic8_cmd.data);
        break;
    case OR4:
        srcaddr4 = (uint32_t *)srcaddr;
        *replaydata = sync_fetch_and_or_4(srcaddr4, cmdq->cmde.atomic4_cmd.data);
        break;
    case OR8:
        srcaddr8 = (uint64_t *)srcaddr;
        *replaydata = sync_fetch_and_or_8(srcaddr8, cmdq->cmde.atomic8_cmd.data);
        break;
    case AND4:
        srcaddr4 = (uint32_t *)srcaddr;
        *replaydata = sync_fetch_and_and_4(srcaddr4, cmdq->cmde.atomic4_cmd.data);
        break;
    case AND8:
        srcaddr8 = (uint64_t *)srcaddr;
        *replaydata = sync_fetch_and_and_8(srcaddr8, cmdq->cmde.atomic8_cmd.data);
        break;
    }
    
#ifdef DEBUG
    fprintf(stdout, "internal selectatomic fin\n");
#endif
    
    return;
}

void check_cmdq_complete(uint64_t index){
    
    uint64_t idx;/* index for cmdq */
    
#ifdef DEBUG_L2
    fprintf(stdout, "internal check_cmdq_complete\n");
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
            idx++;
#ifdef DEBUG
            fprintf(stdout, 
                    "chcomp update idx %ld head %ld tail %ld\n", 
                    idx, head, tail);
#endif
        }
        /* if status is not FINISED, break */
        else {
            break;
        }
    }
    
#ifdef DEBUG_L2
    fprintf(stdout, "internal check_cmdq_complete fin\n");
#endif
}

void rcmdbuf_update_head(){
    
    uint64_t idx;/* index for rcmdb */
    
    /* if after rcmd is not valid,  */
    /* update rcmdbuf_head */
#ifdef DEBUG
    fprintf(stdout, "intenrail rcmdbuf_update_head\n"); 
#endif
    
    idx = *rcmdbuf_head % MAX_RCMDB_SIZE;
    while (*rcmdbuf_head < *rcmdbuf_tail) {
        /* if flag is valid */
        if (rcmdbuf[idx].valid_head == FAULSE && rcmdbuf[idx].valid_tail == FAULSE && rcmdbuf[idx].stat == CMD_WRITEBACK_FIN) {
            (*rcmdbuf_head)++;
            idx++;
#ifdef DEBUG
            fprintf(stdout, 
                    "ch_rcmdbuf_comp update idx %ld rcmdbuf head %ld rcmdubuf tail %ld\n", 
                    idx, *rcmdbuf_head, *rcmdbuf_tail);
#endif
        }
        /* if flag is invalid, break */
        else {
            break;
        }
    }
#ifdef DEBUG
    fprintf(stdout, "internal rcmdbuf_update_head fin\n"); 
#endif
}

void setrrm(int torank){
  
    int i; /* general index */
#ifdef DEBUG
    fprintf(stdout, "internal setrrm\n"); 
#endif
    
    /* set RM table */
    if (rrmtb[torank] == NULL) {
        rrmtb[torank] = (RM *)malloc(sizeof(RM) * MAX_RM_SIZE);
        if (rrmtb[torank] == NULL) {
            acp_abort("set rrm error\n");
            exit(-1);
        }
    }
    for (i = 0; i< MAX_RM_SIZE; i++) {
        rrmtb[torank][i].rkey = recv_lrmtb[i].rkey;
        rrmtb[torank][i].addr = recv_lrmtb[i].addr;
        rrmtb[torank][i].size = recv_lrmtb[i].size;
#ifdef DEBUG
        fprintf(stdout, 
                "rrtb torank %d tag %d addr %p size %lu\n", 
                torank, i, rrmtb[torank][i].addr, rrmtb[torank][i].size);
#endif
    }
#ifdef DEBUG
    fprintf(stdout, "internal setrrm fin\n"); 
#endif
}

void *comm_thread_func(void *dm){
  
    int rc; /* return code for cq */
    int i; /* general index */
    
    struct ibv_wc wc; /* work completion for poll_cq*/
    int myrank; /* my rank id */
    acp_handle_t idx;/* CMDQ index */
    acp_handle_t index;/* handle index */
    
    /* icopy and getlrm */
    acp_ga_t src, dst;/* src and dst ga */
    size_t size;/* data size */
    int torank, dstrank, srcrank;/* target rank, dstga rank, srcga rank*/
    uint32_t totag, dsttag, srctag;/* target tag, dst tag, src tag */
    uint64_t dstoffset, srcoffset;/* dst offset, src offset */
    int nprocs; /* # of processes */
    
    /* get my rank id */
    myrank = acp_rank();
    /* get # of rank */
    nprocs = acp_procs();
  
    while (1) {
        /* CHECK IB COMPLETION QUEUE section */
        rc = ibv_poll_cq(cq, 1, &wc);
        /* error of ibv poll cq*/
        if (rc < 0) {
            fprintf(stderr, "Fail poll cq\n");
            exit(-1);
        }
        /* get cqes */
        if (rc > 0) {
            /* set index by cmdq head */
            index = head;
            if (wc.status == IBV_WC_SUCCESS) {
                /* when ibv_poll_cq is SUCCESS */
                if ((wc.wr_id & MASK_WRID_RCMDB) == 0) {
#ifdef DEBUG
                    fprintf(stdout, "wr_id %lx mask %llx\n", wc.wr_id, wc.wr_id & MASK_WRID_RCMDB);
#endif
                    while (index < tail ) {
                        /* check which handle command complete. */
                        idx = index % MAX_CMDQ_ENTRY;
                        if (cmdq[idx].hdl == wc.wr_id) {
                            switch (cmdq[idx].stat) {
                            case ISSUED: /* issueing gma command */
                                cmdq[idx].stat = FINISHED;
                                check_cmdq_complete(index);
                                break;
                                
                            case WAIT_RRM:/* waiting for get rkey table */
#ifdef DEBUG
                                fprintf(stdout, "WAIT RRM\n");
#endif
                                
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
                                
                                /* set remote rkey mem table */
                                setrrm(torank);
                                
                                /* put get rrm flag */
                                putrrmgetflag(cmdq[idx].hdl, torank);
                                cmdq[idx].stat = WAIT_PUT_RRM_FLAG;
                                break;
                                
                            case WAIT_PUT_RRM_FLAG:
#ifdef DEBUG
                                fprintf(stdout, "WAIT_PUT_RRM_FLAG\n");
#endif		
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
                                
                                /* issued copy */
                                icopy(cmdq[idx].hdl, dstrank, dsttag, 
                                      dstoffset, srcrank, srctag, srcoffset, size);
                                
                                /* set command status ISSUED */
                                cmdq[idx].stat = ISSUED;
                                break;
                                
                            case WAIT_TAIL:
#ifdef DEBUG
                                fprintf(stdout, "WAIT_TAIL\n");
                                fprintf(stdout, "tail_buf %lx, rcmdbuf_tail %lx\n",
                                        *tail_buf, *rcmdbuf_tail);
#endif
                                /* set ga of src from cmdq */
                                src = cmdq[idx].gasrc;
                                /* ger rank of src ga*/
                                srcrank = acp_query_rank(src);
                                if (myrank == srcrank) {
                                    fprintf(stderr, "rr opration tail wait error\n");
                                    exit(-1);
                                }
                                /* issued get head */
                                gethead(cmdq[idx].hdl, srcrank);
                                cmdq[idx].stat = WAIT_HEAD;
                                break;
                                
                            case WAIT_HEAD:
#ifdef DEBUG		  
                                fprintf(stdout, "WAIT_HEAD\n");
#endif
                                /* set src ga from cmdq */
                                src = cmdq[idx].gasrc;
                                /* set srcrank */
                                srcrank = acp_query_rank(src);
                                
                                /* check enable putflag  */
                                if (putcmd_flag == TRUE) {
#ifdef DEBUG		  
                                    fprintf(stdout, "idx %lx putcmd_flag %d\n",idx, putcmd_flag);
                                    fprintf(stdout, "myrank %d srcrank %d\n", myrank, srcrank);
#endif
                                    putcmd_flag = FAULSE; 
                                    putcmd(cmdq[idx].hdl, srcrank, *tail_buf);
                                    tail_buf_flag = TRUE;
                                }
                                else {
                                    cmdq[idx].stat = WAIT_PUT_CMD_FLAG;
                                    break;
                                }
                                cmdq[idx].stat = WAIT_PUT_CMD;
                                break;
                                
                            case WAIT_PUT_CMD:
#ifdef DEBUG
                                fprintf(stdout, "WAIT_PUT_CMD\n");
#endif
                                putcmd_flag = TRUE;
                                cmdq[idx].stat = WAIT_ACK;
                                break;
                                
                            case WAIT_PUT_DST:/* only atomic */
#ifdef DEBUG
                                fprintf(stdout, "WAIT_PUT_DST\n");
#endif
                                cmdq[idx].stat = FINISHED;
                                break;
                                
                            default:
                                break;
                            }
                        }
                        /* update index for next cmdq queue */
                        index ++;
#ifdef DEBUG_L2
                        fprintf(stdout, "qp section: update cmdq index %lx\n", index);
#endif
                    }
                }
                else if ((wc.wr_id & MASK_WRID_ACK) != MASK_WRID_ACK ) {
                    /* set index of rcmd buffer  */
                    index = *rcmdbuf_head;
                    while (index < *rcmdbuf_tail ) {
                        idx = index % MAX_RCMDB_SIZE;
#ifdef DEBUG
                        fprintf(stdout, "qp section: rcmdbuf wr_id %lx mask %llx\n", wc.wr_id, wc.wr_id & MASK_WRID_RCMDB);
#endif 
                        if (rcmdbuf[idx].hdl == wc.wr_id) {
                            switch (rcmdbuf[idx].stat) {
                            case CMD_ISSUED: /* COPY only */
#ifdef DEBUG
                                fprintf(stdout, "CMD_ISSUED\n");
#endif
                                if (writebackbuf_flag == TRUE) {
                                    writebackbuf_flag = FAULSE;
                                    writebackcmdq(idx);
                                }
                                else {
                                    break;
                                }
                                rcmdbuf[idx].stat = CMD_WRITEBACK_FIN;
                                break;
                                
                            case CMD_WAIT_RRM: /* COPY and atomic */
#ifdef DEBUG	
                                fprintf(stdout, "CMD_WAIT_RRM\n");
#endif
                                /* get ga of dst from rcmd buffer */
                                dst = rcmdbuf[idx].gadst;
                                
                                /* get rank of dst ga */
                                dstrank = acp_query_rank(dst);
                                
                                /* set remote rkey memory table of dst rank */
                                setrrm(dstrank);
                                
                                /* issued put rrm get flag */
                                putrrmgetflag(rcmdbuf[idx].hdl, dstrank);
                                
                                /* set status of rcmd buffer */
                                rcmdbuf[idx].stat = CMD_WAIT_PUT_RRM_FLAG;
                                break;
                                
                            case CMD_WAIT_PUT_RRM_FLAG:
                                /* get ga of dst from rcmd buffer  */
                                dst = rcmdbuf[idx].gadst;
                                
                                /* get rank and tag, offset of dst */
                                dstrank = acp_query_rank(dst);
                                dsttag = query_gmtag(dst);
                                dstoffset = query_offset(dst);
                                
                                /* COPY */
                                if (rcmdbuf[idx].type == COPY) {
                                    /* set src ga from rcmd buffer */
                                    src = rcmdbuf[idx].gasrc;
                                    /* get rank, tag, offset of src */
                                    srcrank = acp_query_rank(src);
                                    srctag = query_gmtag(src);
                                    srcoffset = query_offset(src);
                                    
                                    /* issued internal copy  */
                                    icopy(rcmdbuf[idx].hdl, dstrank, dsttag, dstoffset, 
                                          srcrank, srctag, srcoffset, size);
                                    rcmdbuf[idx].stat = CMD_ISSUED;
                                }
                                /* ATOMIC */
                                else if ((rcmdbuf[idx].type & MASK_ATOMIC) == MASK_ATOMIC) {
                                    if ((rcmdbuf[idx].type & MASK_ATOMIC8 ) == MASK_ATOMIC8 ) {
                                        putreplaydata(rcmdbuf[idx].hdl, dstrank, dsttag, dstoffset, TRUE);
                                    }
                                    else {
                                        putreplaydata(rcmdbuf[idx].hdl, dstrank, dsttag, dstoffset, FAULSE);
                                    }
                                    rcmdbuf[idx].stat = CMD_WAIT_PUT_DST;
#ifdef DEBUG
                                    fprintf(stdout,"qp section: rcmdbuf[%lx] %d\n", idx, rcmdbuf[idx].stat);
#endif
                                }
                                break;
                                
                            case CMD_WAIT_PUT_DST: /* only atomic */
#ifdef DEBUG	
                                fprintf(stdout, "CMD_WAIT_PUT_DST\n");
#endif
                                replaydata_flag = TRUE;
                                if (writebackbuf_flag == TRUE) {
                                    writebackbuf_flag = FAULSE;
                                    writebackcmdq(idx);
                                }
                                else {
                                    break;
                                }
                                rcmdbuf[idx].stat = CMD_WRITEBACK_FIN;
                                break;
                                
                            case CMD_WRITEBACK_FIN: /* copy and atomic */
#ifdef DEBUG	
                                fprintf(stdout, "CMD_WRITEBACK_FIN\n");
#endif
                                writebackbuf_flag = TRUE;
                                rcmdbuf[idx].valid_head = FAULSE;
                                rcmdbuf[idx].valid_tail = FAULSE;
                                rcmdbuf_update_head();
                                break;
                                
                            default:
                                break;
                            }
                        }
                        /* update index for next rcmdbuf queue */
                        index++;
#ifdef DEBUG_L2
                        fprintf(stdout, "qp section: update rcmdbuf index %lx\n", index);
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
                            "qp section: put rrm ack: wc.wr_id %lx ack_comp_count %lu \n", 
                            wc.wr_id, ack_comp_count);
#endif
                }/* wc.wr_id & MASK_WRID_ACK is equel to MASK_WRID_ACK */
            }/* wc.status is IBV_WC_SUCCESS */
            else {
                fprintf(stderr, 
                        "wc %lx is not SUCESS when check CQ %d\n", 
                        wc.wr_id, wc.status);
                exit(-1);
            }
        }
        /* CHECK COMMAND QUEUE section */
        /* if temporary cmdq is not empty, */ 
        /* enque the cmd of head of temporary cmdq into tail of cmdq */
        if (tmptail > tmphead) {
            memcpy(&cmdq[tail % MAX_CMDQ_ENTRY], &tmpcmdq[tmphead % MAX_CMDQ_ENTRY], sizeof(CMD));
            /* update head and tail */
            tail++;
            tmphead++;
#ifdef DEBUG
            fprintf(stdout, "cmdq section: tmphead %lx, tmptail %lx head %lx tail %lx\n", tmphead, tmptail, head, tail);
#endif
        }
        /* cmdq is not empty */
        if (tail > head) {
            index = head;
            while (index < tail) {
                idx = index % MAX_CMDQ_ENTRY;
#ifdef DEBUG_L2
                fprintf(stdout, 
                        "cmdq setion: head %ld tail %ld cmdq[%ld].stat %d\n", 
                        head, tail, index, cmdq[idx].stat);
#endif
                /* check if command is complete or not. */
                if (cmdq[idx].stat != COMPLETED) {
                    /* check command type */
                    /* command type is FIN */
                    if (cmdq[idx].type == FIN) {
#ifdef DEBUG
                        fprintf(stdout, "CMD FIN\n");
#endif
                        return 0;
                    }
                    /* command type is COPY */
                    else if (cmdq[idx].stat == UNISSUED) {
#ifdef DEBUG
                        fprintf(stdout, "GMA issued\n");
#endif
                        /* order handling */
                        if (cmdq[idx].ohdl >= head) {
#ifdef DEBUG
                            fprintf(stdout, 
                                    "index %ld ohndl %ld head %ld\n", 
                                    index, cmdq[idx].ohdl, head);
#endif
                            index++;
                            continue;
                        }
                        /* set ga and size from cmdq */
                        src = cmdq[idx].gasrc;
                        dst = cmdq[idx].gadst;
                        size = cmdq[idx].cmde.copy_cmd.size;
                        
                        /* get rank and tag of src ga */
                        srcrank = acp_query_rank(src);
                        srctag = query_gmtag(src);
                        srcoffset = query_offset(src);
                        
                        /* get rank and tag of dst ga */
                        dstrank = acp_query_rank(dst);
                        dsttag = query_gmtag(dst);
                        dstoffset = query_offset(dst);
                        
                        if (cmdq[idx].type == COPY) {
                            /* local copy or local atomic */
                            if (myrank == srcrank && myrank == dstrank) {
                                void *srcaddr, *dstaddr;
                                srcaddr = acp_query_address(src);
                                dstaddr = acp_query_address(dst);
                                memcpy(dstaddr, srcaddr, size);
#ifdef DEBUG
                                fprintf(stdout, "local copy dadr %p sadr %p\n", dstaddr, srcaddr);
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
                                    icopy(cmdq[idx].hdl, dstrank, dsttag, dstoffset, 
                                          srcrank, srctag, srcoffset, size);
                                    cmdq[idx].stat = ISSUED;
                                }
                                else {/* if tag point globl memory */
                                    /* if have a remote rm table of target rank */
                                    if (rrmtb[torank] != NULL) {
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "CMD COPY have a rrmtb. rank %d torank %d, totag %d \n",
                                                myrank, torank, totag);
#endif
                                        /* if tag entry is active */
                                        if (rrmtb[torank][totag].size != 0) {
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "CMD COPY have a entry rank %d torank %d, totag %d\n",
                                                    myrank, torank, totag);
#endif
                                            icopy(cmdq[idx].hdl, dstrank, dsttag, dstoffset, 
                                                  srcrank, srctag, srcoffset, size);
                                            cmdq[idx].stat = ISSUED;
                                        }
                                        else { /* if tag entry is non active */
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "CMD COPY no entry.  rank %d torank %d, totag %d\n",
                                                    myrank, torank, totag);
#endif
                                            getlrm(cmdq[idx].hdl, torank);
                                            cmdq[idx].stat = WAIT_RRM;
                                        }
                                    }
                                    else { /* if do not have rm table of target rank */
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "CMD COPY no rrmtb.  rank %d torank %d, totag %d\n",
                                                myrank, torank, totag);
#endif
                                        getlrm(cmdq[idx].hdl, torank);
                                        cmdq[idx].stat = WAIT_RRM;
                                    }
                                }
                            }
                            /* remote to remote copy */
                            else if (myrank != srcrank && myrank !=dstrank) {
                                if (tail_buf_flag == TRUE) {
                                    tail_buf_flag = FAULSE;
                                    gettail(cmdq[idx].hdl, srcrank);
                                }
                                else {
                                    break;
                                }
#ifdef DEBUG
                                fprintf(stdout, "hdl %lx gettail srcrank %d dstrank %d \n",
                                        cmdq[idx].hdl, srcrank, dstrank);
#endif
                                cmdq[idx].stat = WAIT_TAIL;
                            }
                        }
                        else { /* type is atomic */
                            if (myrank == srcrank && myrank == dstrank) {
                                void *srcaddr, *dstaddr;
                                srcaddr = acp_query_address(src);
                                dstaddr = acp_query_address(dst);
                                selectatomic(srcaddr, &cmdq[idx]);
#ifdef DEBUG
                                fprintf(stdout, "remote atomic srcrank equal to dstrank : dadr %p sadr %p\n", dstaddr, srcaddr);
#endif
                                if ((rcmdbuf[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                    memcpy(dstaddr, replaydata, sizeof(uint64_t));
                                }
                                else {
                                    memcpy(dstaddr, replaydata, sizeof(uint32_t));
                                }
                                cmdq[idx].stat = FINISHED;
                                check_cmdq_complete(index);
                                break;
                            }
                            else if (myrank == srcrank && myrank != dstrank) {
                                void *srcaddr;
                                srcaddr = acp_query_address(src);
                                if (replaydata_flag == TRUE) {
                                    replaydata_flag = FAULSE;
                                    selectatomic(srcaddr, &cmdq[idx]);
                                }
                                else {
                                    index++;
                                    continue;
                                }
                                /* if tag point stater memory */
                                if (dsttag == TAG_SM) {
                                    if ((rcmdbuf[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                        putreplaydata(cmdq[idx].hdl, dstrank, dsttag, dstoffset, TRUE);
                                    }
                                    else {
                                        putreplaydata(cmdq[idx].hdl, dstrank, dsttag, dstoffset, FAULSE);
                                    }
                                    cmdq[idx].stat =  WAIT_PUT_DST;
                                }
                                /* if tag point globl memory */
                                else {
                                    /* if have a remote rm table of target rank */
                                    if (rrmtb[dstrank] != NULL) {
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "CMD ATOMIC have a rrmtb. rank %d dstrank %d, dsttag %d \n",
                                                myrank, dstrank, dsttag);
#endif
                                        /* if tag entry is active */
                                        if (rrmtb[dstrank][dsttag].size != 0) {
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "CMD ATOMIC have a entry rank %d dstrank %d, dsttag %d\n",
                                                    myrank, dstrank, dsttag);
#endif
                                            if ((rcmdbuf[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                                putreplaydata(cmdq[idx].hdl, dstrank, dsttag, dstoffset, TRUE);
                                            }
                                            else {
                                                putreplaydata(cmdq[idx].hdl, dstrank, dsttag, dstoffset, FAULSE);
                                            }
                                            cmdq[idx].stat = WAIT_PUT_DST;
                                        }
                                        /* if tag entry is non active */
                                        else {
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "CMD ATOMIC no entry.  rank %d dstrank %d, dsttag %d\n",
                                                    myrank, dstrank, dsttag);
#endif
                                            getlrm(cmdq[idx].hdl, dstrank);
                                            cmdq[idx].stat = WAIT_RRM;
                                        }
                                    }
                                    /* if do not have rm table of target rank */
                                    else {
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "CMD ATOMIC no rrmtb.  rank %d dstrank %d, dsttag %d\n",
                                                myrank, dstrank, dsttag);
#endif
                                        getlrm(cmdq[idx].hdl, dstrank);
                                        cmdq[idx].stat = WAIT_RRM;
                                    }
                                }
                            }
                            /* remote to remote atomic */
                            else if (myrank != srcrank) {
                                if (tail_buf_flag == TRUE) {
                                    tail_buf_flag = FAULSE;
                                    gettail(cmdq[idx].hdl, srcrank);
                                }
                                else {
                                    break;
                                }
                                cmdq[idx].stat = WAIT_TAIL;  
                            }
                        }
                        /* command issue, break */
                        break;
                    }
                    else if (cmdq[idx].stat == WAIT_PUT_CMD_FLAG) {
#ifdef DEBUG
                        fprintf(stdout, "wait_put_cmd_flag bool %d\n", putcmd_flag);
#endif
                        if (putcmd_flag == TRUE) {
                            putcmd_flag = FAULSE; 
                            putcmd(cmdq[idx].hdl, srcrank, *tail_buf);
                            tail_buf_flag = TRUE;
                            cmdq[idx].stat = WAIT_PUT_CMD;
                        }
                    }
                    else {
                        check_cmdq_complete(index);
                    }
                }
                index++;
#ifdef DEBUG_L2
                fprintf(stdout, 
                        "cmdq section update index %lx\n", 
                        index);
#endif
            }
        }
        
        /* CHECK recv COMMAND QUEUE section */
        index = *rcmdbuf_head;
        while (index <  *rcmdbuf_tail) {
            idx = index % MAX_RCMDB_SIZE;
            if (rcmdbuf[idx].valid_head == TRUE && rcmdbuf[idx].valid_tail == TRUE) {
                if (rcmdbuf[idx].stat == CMD_UNISSUED) {
#ifdef DEBUG_L2
                    fprintf(stdout, "rcmdq section: CMD_UNISSUED\n");
#endif
                    rcmdbuf[idx].hdl = index | MASK_WRID_RCMDB;
#ifdef DEBUG_L2
                    fprintf(stdout, "rcmdq section: %d: *rcmdbuf_head %lx, *rcmdbuf_tail %lx\n", 
                            acp_myrank, *rcmdbuf_head, *rcmdbuf_tail);
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
                            fprintf(stdout, "local copy:dadr %p sadr %p\n", dstaddr, srcaddr);
#endif
                            if (writebackbuf_flag == TRUE) {
                                writebackbuf_flag = FAULSE;
                                writebackcmdq(idx);
                            }
                            else {
                                index++;
                                continue;
                            }
                            rcmdbuf[idx].stat = CMD_WRITEBACK_FIN;
                            break;
                        }
                        else { /* remote put */
                            /* if have a remote rm table of target rank */
                            if (rrmtb[dstrank] != NULL) {
#ifdef DEBUG
                                fprintf(stdout, 
                                        "CMDRB COPY have a rrmtb. rank %d dstrank %d, dsttag %d \n",
                                        myrank, dstrank, dsttag);
#endif
                                /* if tag entry is active */
                                if (rrmtb[dstrank][dsttag].size != 0) {
#ifdef DEBUG
                                    fprintf(stdout, 
                                            "CMDRB COPY have a entry rank %d dstrank %d, dsttag %d\n",
                                            myrank, dstrank, dsttag);
#endif
                                    icopy(rcmdbuf[idx].hdl, dstrank, dsttag, dstoffset, 
                                          srcrank, srctag, srcoffset, size);
                                    rcmdbuf[idx].stat = CMD_ISSUED;
                                }
                                else { /* if tag entry is non active */
#ifdef DEBUG
                                    fprintf(stdout, 
                                            "CMDRB COPY no entry.  rank %d dstrank %d, dsttag %d\n",
                                            myrank, dstrank, dsttag);
#endif
                                    getlrm(rcmdbuf[idx].hdl, dstrank);
                                    rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                }
                            }
                            else { /* if do not have rm table of dst rank */
#ifdef DEBUG
                                fprintf(stdout, 
                                        "CMDRB COPY no rrmtb.  rank %d dstrank %d, dsttag %d\n",
                                        myrank, torank, totag);
#endif
                                getlrm(rcmdbuf[idx].hdl, dstrank);
                                rcmdbuf[idx].stat = CMD_WAIT_RRM;
                            }
                        }
                    }
                    else { /* atomic */
#ifdef DEBUG
                        fprintf(stdout, "CMD_UNISSUED ATOMIC\n");
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
                            if (replaydata_flag == TRUE) {
                                replaydata_flag = FAULSE;
                                selectatomic(srcaddr, &rcmdbuf[idx]);
#ifdef DEBUG
                                fprintf(stdout, 
                                        "ATOMIC select atomic myrank %d , srcrank %d\n", 
                                        myrank, srcrank);
#endif
                            }
                            else {
                                index++;
                                continue;
                            }
                            if (myrank != dstrank) {
                                /* if tag point stater memory */
                                if (dsttag == TAG_SM) {
                                    if ((rcmdbuf[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                        putreplaydata(rcmdbuf[idx].hdl, dstrank, dsttag, dstoffset, TRUE);
                                    }
                                    else {
                                        putreplaydata(rcmdbuf[idx].hdl, dstrank, dsttag, dstoffset, FAULSE);
                                    }
                                    rcmdbuf[idx].stat =  CMD_WAIT_PUT_DST;
                                }
                                /* if tag point globl memory */
                                else {
                                    /* if have a remote rm table of dst rank */
                                    if (rrmtb[dstrank] != NULL) {
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "CMDRB ATOMIC have a rrmtb. rank %d dstrank %d, dsttag %d \n",
                                                myrank, dstrank, dsttag);
#endif
                                        /* if tag entry is active */
                                        if (rrmtb[dstrank][dsttag].size != 0) {
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "CMDRB ATOMIC have a entry rank %d dstrank %d, dsttag %d\n",
                                                    myrank, dstrank, dsttag);
#endif
                                            if ((rcmdbuf[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                                putreplaydata(rcmdbuf[idx].hdl, dstrank, dsttag, dstoffset, TRUE);
                                            }
                                            else {
                                                putreplaydata(rcmdbuf[idx].hdl, dstrank, dsttag, dstoffset, FAULSE);
                                            }
                                            rcmdbuf[idx].stat = CMD_WAIT_PUT_DST;
                                        }
                                        /* if tag entry is non active */
                                        else {
#ifdef DEBUG
                                            fprintf(stdout, 
                                                    "CMDRB ATOMIC no entry.  rank %d dstrank %d, dsttag %d\n",
                                                    myrank, dstrank, dsttag);
#endif
                                            getlrm(rcmdbuf[idx].hdl, dstrank);
                                            rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                        }
                                    }
                                    /* if do not have rm table of target rank */
                                    else {
#ifdef DEBUG
                                        fprintf(stdout, 
                                                "CMDRB ATOMIC no rrmtb.  rank %d dstrank %d, dsttag %d\n",
                                                myrank, dstrank, dsttag);
                                        fprintf(stdout, "rcmdbuf[idx].hdl %lx\n", rcmdbuf[idx].hdl);
#endif
                                        getlrm(rcmdbuf[idx].hdl, dstrank);
                                        rcmdbuf[idx].stat = CMD_WAIT_RRM;
                                    }
                                }
                            }
                            else {
                                void *dstaddr;
                                dstaddr = acp_query_address(dst);
                                
                                if ((rcmdbuf[idx].type & MASK_ATOMIC8) == MASK_ATOMIC8) {
                                    memcpy(&dstaddr, &replaydata, sizeof(uint64_t));
                                }
                                else {
                                    memcpy(&dstaddr, &replaydata, sizeof(uint32_t));
                                }
                                
                                replaydata_flag = TRUE;
                                if (writebackbuf_flag == TRUE) {
                                    writebackbuf_flag = FAULSE;
                                    writebackcmdq(idx);
                                }
                                else {
                                    index++;
                                    continue;
                                }
                                rcmdbuf[idx].stat = CMD_WRITEBACK_FIN;
                            }
                        }
                        else {
                            fprintf(stderr, "atomic execute in src rank %d: myrank %d\n",srcrank, myrank);
                            exit(-1);
                        }
                    }
                }
            }
                /* increment index of rcmdbuf */
            index++;
        }
        /* PUT RRM ACK SECTION */
        for (i = 0;i < nprocs;i++) {
            /* check rrm reset flag table */
            if (TRUE == rrm_reset_flag_tb[i]) {
                /* free remote rkey table */
                free(rrmtb[i]);
                rrmtb[i] = NULL;
                /* put rrm ack flag */
                putrrmackflag(ack_id, i);
                /* set FAULSE rrm reset flag table */
                rrm_reset_flag_tb[i] = FAULSE;
                ack_id++;
            }
        }
    }
}

int iacp_init(void){
    
    int rc = 0;/* return code */
    int i, j; /* general index */
    
    int sock_s;  /* socket server */
    socklen_t addrlen;/* address length */
    struct sockaddr_in myaddr, dstaddr, srcaddr;/* address */
    
    struct ibv_device **dev_list = NULL; /* IB device list */
    int num_devices = 0; /* # of IB devices */
    char *dev_name = NULL;/* device name */
    
    /* initalize queue pair attribute information*/
    struct ibv_qp_init_attr qp_init_attr;
    int mr_flags = 0;/* flag of memory registeration*/
    int ib_port = 1;/* ib port */
    int cq_size = MAX_CQ_SIZE;
    
    /* post recieve in RTR */
    struct ibv_recv_wr rr;
    struct ibv_sge sge;
    struct ibv_recv_wr *bad_wr;
    
    uint32_t *local_qp_num;/* local each qp number */
    uint32_t *tmp_qp_num;/* temporary qp number */
    uint32_t *remote_qp_num;/* remote each qp number */
    
    CII local_data; /* local data for socket communication */
    CII remote_data; /* remote data for socket communication */
    CII tmp_data; /* temporary data for socket communication */
    int torank; /* rank ID */
    
    struct ibv_qp_attr attr;/* modify queue pair */
    int flags;/* flag of modify queue pair */
    
    /* allocate the starter memory adn register memory */
    int syssize;
    
    syssize = acp_smsize + sizeof(RM) * (MAX_RM_SIZE) * 2  +
        sizeof(uint64_t) * 5 + sizeof(CMD) * MAX_CMDQ_ENTRY + 
        sizeof(CMD) * MAX_RCMDB_SIZE + sizeof(CMD) * 2  + sizeof(char) * acp_numprocs * 3 + sizeof(char) +
        iacp_starter_memory_size_dl + iacp_starter_memory_size_cl + iacp_starter_memory_size_vd;	
    sysmem = (char *) malloc(syssize);
    if (NULL == sysmem ) {
        fprintf(stderr, "failed to malloc %d bytes to memory buffer\n", 
                syssize);
        rc = -1;
        goto exit;
    }
    
    /* initalize starter memory, local RM table, recv RM table */
    memset(sysmem, 0 , syssize);
    
    /* initalize local rkey memory */
    lrmtb = (RM *) ((char *)sysmem + acp_smsize);
    for (i = 0;i < MAX_RM_SIZE;i++) {
        /* entry non active */
        lrmtb[i].size = 0;
    }
    /* initalize revcieve local rkey memory */
    recv_lrmtb = (RM *) ((char *)lrmtb + sizeof(RM) * MAX_RM_SIZE );
    for (i = 0;i < MAX_RM_SIZE;i++) {
        /* entry non active */
        recv_lrmtb[i].size = 0;
    }
    
    /* initialize write flag of command recv buffer */
    rcmdbuf_head = (uint64_t *)((char *)recv_lrmtb +  sizeof(RM) * MAX_RM_SIZE );
    *rcmdbuf_head = 0;
    
    /* initialize read flag of command recv buffer */
    rcmdbuf_tail = (uint64_t *)((char *)rcmdbuf_head + sizeof(uint64_t));
    *rcmdbuf_tail = 0;
    
    /* initialize local head buffer */
    head_buf = (uint64_t *)((char * )rcmdbuf_tail + sizeof(uint64_t));
    
    /* initialize local tail buffer */
    tail_buf = (uint64_t *)((char *)head_buf + sizeof(uint64_t));
    
    /* initialize replay data buffer */
    replaydata = (uint64_t *)((char *)tail_buf + sizeof(uint64_t));
    
    /* initialize command q */
    cmdq = (CMD *)((char *)replaydata + sizeof(uint64_t)); 
    
    /* initialize command recv buffer */
    rcmdbuf = (CMD *)((char *)cmdq + sizeof(CMD) * MAX_CMDQ_ENTRY);
    
    /* initialize writeback buffer */
    writebackbuf = (CMD *)((char *)rcmdbuf + sizeof(CMD) * MAX_RCMDB_SIZE);
    
    /* initilaize put cmd buffer */
    putcmdbuf = (CMD *)((char*)writebackbuf + sizeof(CMD));
    
    /* initilaize get flag table */
    rrm_get_flag_tb = (char *)((char*)putcmdbuf + sizeof(CMD));
    
    /* initilaize reset flag table */
    rrm_reset_flag_tb = (char *)((char*)rrm_get_flag_tb + sizeof(char) * acp_numprocs);
    
    /* initialize ack flag table */
    rrm_ack_flag_tb = (char *)((char*)rrm_reset_flag_tb + sizeof(char) * acp_numprocs);
    
    /* set true flag buffer */
    true_flag_buf = (char *)((char*)rrm_ack_flag_tb + sizeof(char) * acp_numprocs);
    *true_flag_buf = TRUE;
    
    /* set starter memory for multi module */
    acp_buf_dl = (char *)((char *)true_flag_buf + sizeof(char));
    acp_buf_cl = (char *)((char *)acp_buf_dl + iacp_starter_memory_size_dl);
    acp_buf_vd = (char *)((char *)acp_buf_cl + iacp_starter_memory_size_cl);
  
#ifdef DEBUG
    fprintf(stdout, 
            "sm %p acp_vd_buf %p syssize %d sm + syssize %p\n", 
            sysmem, (char *)acp_buf_vd + iacp_starter_memory_size_vd, syssize, sysmem + syssize);
#endif
    /* remote register memory table */
    rrmtb = (RM **) malloc (sizeof (RM *) * acp_numprocs);
    if (rrmtb == NULL) {
        fprintf(stderr, "failed to malloc rrmtb\n");
        rc = -1;
        goto exit;
    }
    
    /* generate server socket */
    if ((sock_s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "sever socket() failed \n");
        rc = -1;
        goto exit;
    }
    
    /* enable address resuse, as soon as possible */
    int on;
    on = 1;
    rc = setsockopt( sock_s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
    
    /* initialize myaddr of sockaddr_in  */
    memset(&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = INADDR_ANY;
    myaddr.sin_port = my_port;
  
    /* bind socket file descriptor*/
    if (bind(sock_s, (struct sockaddr*)&myaddr, sizeof(myaddr)) < 0) {
        fprintf(stderr, "bind() failed\n");
        rc = -1;
        goto exit;
    }
    
    if (listen(sock_s, 2) < 0) {
        rc = -1;
        goto exit;
    }
    
    /* generate connection socket */
    if ((sock_connect = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
        fprintf(stderr, "connect socket() failed \n");
    }
    
    /* generate destination address */
    memset(&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = dst_addr;
    dstaddr.sin_port = dst_port;
    
    addrlen = sizeof(srcaddr);
    
    /* get the size of source address */
    if (acp_numprocs > 1) {
        if ((acp_myrank & 1) == 0) { /* even rank  */
            if ((sock_accept = accept(sock_s, (struct sockaddr *)&srcaddr, &addrlen)) < 0) {
                perror("even accept() failed:");
                goto exit;
            }
            while (connect(sock_connect, (struct sockaddr*)&dstaddr, sizeof(dstaddr)) < 0);
        }
        else { /* odd rank */
            while (connect(sock_connect, (struct sockaddr*)&dstaddr, sizeof(dstaddr)) < 0);
            if ((sock_accept = accept(sock_s, (struct sockaddr *)&srcaddr, &addrlen)) < 0) {
                perror("odd accept() failed:");
                goto exit;
            }
        }
    }
    
    /* socket close */
    if (sock_s) {
        close(sock_s);
    }
    
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
#endif
    
    if (!dev_list[0]) {
        fprintf(stderr, "IB device wasn't found\n");
        rc = -1;
        goto exit;
    }
    dev_name = strdup(ibv_get_device_name(dev_list[0]));
#ifdef DEBUG
    fprintf(stdout, "First IB device name is %s\n", dev_name);
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
#endif
    
    /* create the Queue Pair */
    qp = (struct ibv_qp **) malloc(sizeof(struct ibv_qp *) * acp_numprocs);
    if (qp == NULL) {
        fprintf(stderr, "failed to malloc qp\n");
        rc = -1;
        goto exit;
    }
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.sq_sig_all = 1; /* if work request COMPLETE, CQE enqueue cq. */
    qp_init_attr.send_cq = cq;
    qp_init_attr.recv_cq = cq;
    qp_init_attr.cap.max_send_wr = cq_size;
    qp_init_attr.cap.max_recv_wr = 1; /* use only first post recv */
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    
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
    
#ifdef DEBUG
    fprintf(stdout, "local address = %lx\n", local_data.addr);
    fprintf(stdout, "local rkey = %u\n", local_data.rkey);
    fprintf(stdout, "local LID = %u\n", local_data.lid);
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
        fprintf(stdout, "-----------------------------\n");
        memset(&remote_data, 0, sizeof(remote_data));
        memset(remote_qp_num, 0, sizeof(uint32_t) * acp_numprocs);
#endif
        
        write(sock_connect, &tmp_data, sizeof(tmp_data));
        recv(sock_accept, &remote_data, sizeof(remote_data),0 );
        write(sock_connect, tmp_qp_num, sizeof(uint32_t) * acp_numprocs);
        recv(sock_accept, remote_qp_num, sizeof(uint32_t) * acp_numprocs,0 );
        
        /* copy addr and rkey to start buffer information table */
        torank = remote_data.rank;
        smi_tb[torank].addr = remote_data.addr;
        smi_tb[torank].rkey = remote_data.rkey;
        
        /* save the remote side attributes, we will need it for the post SR */
#ifdef DEBUG
        fprintf(stdout, "Remote address = %lx\n", remote_data.addr);
        fprintf(stdout, "Remote rkey = %u\n", remote_data.rkey);
        fprintf(stdout, "Remote rank = %d\n", torank);
        fprintf(stdout, "Remote LID = %u\n", remote_data.lid);
        fprintf(stdout, "Remote QP number = %d 0x%x\n", torank, remote_qp_num[acp_myrank]);
#endif
    
        /* modify the QP to init */
        /* initialize ibv_qp_attr */
        memset(&attr, 0, sizeof(attr));
        
        /* set attribution for INIT */
        attr.qp_state = IBV_QPS_INIT;
        attr.port_num = ib_port;/* physical port number (1...n)*/
        attr.pkey_index = 0;/* normally 0 */
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
            fprintf(stderr, "change QP state to INIT failed\n");
            goto exit;
        }
#ifdef DEBUG
        fprintf(stdout, "QP state was change to INIT\n");
#endif
        
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
            fprintf(stderr, "failed to post RR\n");
            goto exit;
        }
#ifdef DEBUG
        fprintf(stdout, "Post Receive Request\n");
#endif
        
        /* modify the QP to RTR */
        memset(&attr, 0, sizeof(attr));
        
        /* set attribution for RTR */
        attr.qp_state = IBV_QPS_RTR;
        attr.path_mtu = IBV_MTU_256;
        attr.dest_qp_num = remote_qp_num[acp_myrank];
        attr.rq_psn = 0;
        attr.max_dest_rd_atomic = 1;
        attr.min_rnr_timer = 0x12;/* recommend value 0x12. miminum RNR(receive not ready) NAK timer */
        attr.ah_attr.dlid = remote_data.lid;/* minimally ah_attr.dlid needs t o be filled in ah for UD */
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
            fprintf(stderr, "failed to modify QP state to RTR\n");
            goto exit;
        }
#ifdef DEBUG
        fprintf(stdout, "QP state was change to RTR\n");
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
            fprintf(stderr, "failed to modify QP state to RTR\n");
            goto exit;
        }
#ifdef DEBUG
        fprintf(stdout, "QP state was change to RTS\n");
#endif
        
        /* copy remote data to temporary buffer for bukects relay */
        tmp_data.addr = remote_data.addr;
        tmp_data.rkey = remote_data.rkey;
        tmp_data.lid = remote_data.lid;
        tmp_data.rank = remote_data.rank;
        for (j = 0;j < acp_numprocs; j++) {
            tmp_qp_num[j] = remote_qp_num[j];
        }
    }
    
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
    
#ifdef MUL_MOD  
    if (iacp_init_dl()) return -1;
    if (iacp_init_cl()) return -1;
    if (iacp_init_vd()) return -1;
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
    for (i = 0;i < acp_numprocs ;i++) {
        if (rrmtb[i] != NULL) {
            free(rrmtb[i]);
            rrmtb[i] = NULL;
        }
    }
    
    if (rrmtb != NULL) {
        free(rrmtb);
        rrmtb = NULL;
    }
    
    if (system != NULL) {
        free(sysmem);
        sysmem = NULL;
    }
    if (smi_tb != NULL) {
        free(smi_tb);
        smi_tb = NULL;
    }
    
    /* set NULL pointer for  sysmem */
    rcmdbuf = NULL;
    rcmdbuf_head = NULL; 
    rcmdbuf_tail = NULL; 
    head_buf = NULL; 
    tail_buf = NULL;
    putcmdbuf = NULL; 
    replaydata = NULL;
    writebackbuf = NULL; 
    lrmtb = NULL; 
    recv_lrmtb = NULL; 
    rrm_get_flag_tb = NULL;
    rrm_reset_flag_tb = NULL;
    rrm_ack_flag_tb = NULL;
    true_flag_buf = NULL; 
    
    /* initailize acpbl variables */
    acp_myrank = -1;
    tmphead = 1;
    tmptail = 1;
    head = 1;
    tail = 1;
  
    ack_id = MASK_WRID_ACK;
    ack_comp_count = 0;

    return rc;
    
}

int acp_init(int *argc, char ***argv){
    int rc;/* return code */
    
    if (*argc < 7) return -1;
    
    acp_myrank = strtol((*argv)[1], NULL, 0);
    acp_numprocs = strtol((*argv)[2], NULL, 0);
    acp_smsize = strtol((*argv)[3], NULL, 0);
    my_port = strtol((*argv)[4], NULL, 0);
    dst_port = strtol((*argv)[5], NULL, 0);
    dst_addr = inet_addr((*argv)[6]);
  
    /* print acp_init argument */
#ifdef DEBUG
    fprintf(stdout, 
            "rk %d np %d ss %lu mp %u dp %u da %u\n", 
            acp_myrank, acp_numprocs, acp_smsize, 
            my_port, dst_port, dst_addr);
#endif
    
    rc = iacp_init();
    
    return rc;
}

int acp_finalize(){
    
    int i; /* genral index */
    int myrank;/* my rank for command*/
    acp_handle_t tmptail4c;/* tail for cmdq */
    CMD *pcmdq; /* pointer of cmd */
  
#ifdef DEBUG
    fprintf(stdout, "internal acp_finalize\n");
#endif
    
#ifdef MUL_MOD
    iacp_finalize_vd();
    iacp_finalize_cl();
    iacp_finalize_dl();
#endif
    /* Insert FIN command into cmdq */
    /* if queue is full, return ACP_HANDLE_NULL */
    while (tmptail - tmphead == MAX_CMDQ_ENTRY - 1) ;
    
    /* wait all process execute acp_finalize */
    acp_sync();
    /* check my rank */
    myrank = acp_rank();
    /* make a FIN command, and enqueue command Queue. */
    tmptail4c = tmptail % MAX_CMDQ_ENTRY;
    pcmdq = &tmpcmdq[tmptail4c];
    pcmdq->rank = myrank;
    pcmdq->type = FIN;
    pcmdq->ishdl = tmptail;
    pcmdq->hdl = tmptail;
    pcmdq->stat = ISSUED;
    
    /* update tmptail */
    tmptail++ ;
    
    /* complete hdl */
    pthread_join(comm_thread_id, NULL);
    
    /* close IB resouce */
    if (res.mr != NULL) {
        ibv_dereg_mr(res.mr);
        res.mr = NULL;
    }
    
    for (i = 0;i < MAX_RM_SIZE;i++) {
        if (lmrtb[i] != NULL) {
            ibv_dereg_mr(lmrtb[i]);
            lmrtb[i] = NULL;
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
    for (i = 0;i < acp_numprocs ;i++) {
        if (rrmtb[i] != NULL) {
            free(rrmtb[i]);
            rrmtb[i] = NULL;
        }
    }
    if (rrmtb != NULL) {
        free(rrmtb);
        rrmtb = NULL;
    }
    if (system != NULL) {
        free(sysmem);
        sysmem = NULL;
    }
    if (smi_tb != NULL) {
        free(smi_tb);
        smi_tb = NULL;
    }
    
    /* set NULL pointer for  sysmem */
    rcmdbuf = NULL;
    rcmdbuf_head = NULL; 
    rcmdbuf_tail = NULL; 
    head_buf = NULL; 
    tail_buf = NULL;
    putcmdbuf = NULL; 
    replaydata = NULL;
    writebackbuf = NULL; 
    lrmtb = NULL; 
    recv_lrmtb = NULL; 
    rrm_get_flag_tb = NULL;
    rrm_reset_flag_tb = NULL;
    rrm_ack_flag_tb = NULL;
    true_flag_buf = NULL; 
    
    /* initailize acpbl variables */
    acp_myrank = -1;
    tmphead = 1;
    tmptail = 1;
    head = 1;
    tail = 1;
  
    ack_id = MASK_WRID_ACK;
    ack_comp_count = 0;

#ifdef DEBUG
    fprintf(stdout, "internal acp_finalize fin\n");
#endif
    
    return 0;
}

int acp_reset(int rank){
    int rc = -1;
    
    rc = acp_finalize();  
    if (rc == -1) {
        return rc;
    }
    
    rc = iacp_init();
    
    return rc;
}
