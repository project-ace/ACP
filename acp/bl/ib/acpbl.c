#include"acpbl.h"

/* define size */
#define MAX_RM_SIZE     255U
#define MAX_CQ_SIZE       1U
#define MAX_CMDQ_ENTRY 4096U

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
/* define command type*/
#define FIN  15U
#define COPY 1U

/* define STATUSs */
#define COMPLETED 0U
#define UNISSUED  1U
#define ISSUED    2U
#define FINISHED  3U
#define GET_WAIT  4U
#define GET_COMP  5U

/* ACP constants */
#define ACP_GA_NULL      0xffffffffffffffffLLU /* 0LLU ? */
#define ACP_ATKEY_NULL   0xffffffffffffffffLLU /* 0LLU ? */
#define ACP_HANDLE_ALL   0xffffffffffffffffLLU
#define ACP_HANDLE_NULL  0x0LLU

/* resouce info for starter memory */
typedef struct resource_info{
  uint32_t myrank; /* rank ID */
  struct ibv_port_attr port_attr; /* IB port attributes */
  struct ibv_context *ib_ctx; /* device handle */
  struct ibv_pd *pd; /* PD handle */
  struct ibv_mr *mr; /* MR handle for starter memory */
} RES;

typedef struct reg_mem{
  char *addr;  /* the front of memory region */
  uint64_t rkey;  /* memory registration key in IB */
  acp_size_t size;  /* the size of memory region */
} RM;

typedef struct connect_ib_info{
  uint64_t addr;/* address of starter memory */
  uint32_t rkey;  /* rkery of starter memory */
  uint16_t lid;  /* local ID of Local IB */
  uint32_t rank;  /* local rank */
} CII;

typedef struct starter_memroy_info{
  uint64_t addr;/* starter memory address */
  uint32_t rkey;/* the rkey of starter memory */
} SMI;

typedef struct command{
  uint32_t rank;/* issued rank */
  uint32_t type;/* command type */
  acp_handle_t ohdl;/* order handle */
  acp_handle_t hdl;/* handle */
  uint32_t stat;/* command status */
  acp_ga_t gadst;/* destination of ga */
  acp_ga_t gasrc;/* srouce of ga */
  acp_size_t size;/* copy size */
}CMD;

/* socket file descripter */
static int sock_accept;
static int sock_connect;

static RES res;/* resource of IB */
static struct ibv_qp **qp;/* QP handle */
static struct ibv_cq *cq;/* CQ handle */

static int acp_myrank = -1; /* my rank on acp lib*/
static int acp_numprocs = -1; /* # of process on acp lib */
static acp_size_t acp_smsize = -1; /* the size of starter memory on acp lib */
static char *sm;/* starter memory address*/
static SMI *smi_tb;/* starter memory info table */
static CMD cmdq[MAX_CMDQ_ENTRY];/* comand queue */
static int head = 0;/* the head of command queue */
static int tail = 0;/* the tail of command queue */
static int chdl = 1;/* latest issued handle number */

struct ibv_mr *lmrtb[MAX_RM_SIZE];/* local ibv_mr table */ 
RM *lrmtb;/* Local addr/rkey info table */
RM **rrmtb;/* Remote addr/rkey info table */
RM *recv_lrmtb;/* recv buffer for local addr/rkey table */

pthread_t comm_thread_id;/* communcation thread ID */

int acp_sync(void){
  int i;/* general index */
  char dummy1, dummy2;/* dummy buffer */
  int nprocs;/* # of processes */ 
  
  fprintf(stdout, "internal sync\n");
  nprocs = acp_procs();
  if(nprocs <= 0){
    return -1;
  }
  if(nprocs > 1){
    for(i=0;i < nprocs;i++){
      write(sock_connect, &dummy1, sizeof(char));
      recv(sock_accept, &dummy2, sizeof(char), 0);
    }
  }
  fprintf(stdout, "internal sync fin\n");
  
  return 0;
}


int acp_colors(void){

  return 1;/* # of color is one  */
}

int acp_rank(void){

  return acp_myrank;
}

int acp_procs(void){
  
  return acp_numprocs;
}

int acp_query_rank(acp_ga_t ga){
  
  int rank;
  
  rank = (int)((ga >> (COLOR_BITS + GMTAG_BITS + OFFSET_BITS)) & RANK_MASK);
  fprintf(stdout, "ga %lx rank %d\n", ga, rank);
  
  return rank;
}

uint32_t acp_query_color(acp_ga_t ga){
  
  uint32_t color;
  
  color = (uint32_t)((ga >> (GMTAG_BITS + OFFSET_BITS)) & COLOR_MASK);
  fprintf(stdout, "ga %lx color %d\n", ga, color);
  
  return color;
}

uint32_t query_gmtag(acp_ga_t ga){
  
  uint32_t gmtag;
  
  gmtag = (uint32_t)((ga >> OFFSET_BITS) & GMTAG_MASK);
  //fprintf(stdout, "ga >> offset %lx gmtag %ld\n", ga >> OFFSET_BITS, gmtag);
  fprintf(stdout, "ga %lx gmtag %d\n", ga, gmtag);
  
  return gmtag;
}


uint64_t query_offset(acp_ga_t ga){

  uint64_t offset;/* offset of ga */
  
  offset = (uint64_t)(ga & OFFSET_MASK);
  
  return offset;
}


acp_ga_t acp_query_starter_ga(int rank){
  
  acp_ga_t ga;
  uint64_t offset = 0;
  uint32_t gmtag = TAG_SM;
  uint32_t color = 0;
  
  ga = ((uint64_t)rank << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
    + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
    + ((uint64_t)gmtag << OFFSET_BITS) + offset;
  
  fprintf(stdout, "rank %d starter memory ga %lx\n", rank, ga);
  
  return ga;
}

acp_ga_t acp_query_ga(acp_atkey_t atkey, void* addr){
  
  acp_ga_t ga;
  uint64_t offset = 0;
  uint32_t gmtag = 0;
  uint32_t color = 0;
  int keyrank;
  int myrank;

  /* get my rank */
  myrank = acp_rank();
  /* rank of atkey */
  keyrank = atkey >> (COLOR_BITS + GMTAG_BITS + OFFSET_BITS) & RANK_MASK;
  /* index of atkey */
  gmtag = (atkey >> OFFSET_BITS) & GMTAG_MASK;
  
  /* if my rank is equal to keyrank */
  if(keyrank == myrank){
    if(lrmtb[gmtag].size == 0){
      return ACP_GA_NULL;
    }
    else{
      fprintf(stdout, "acp_query_ga key %lx, addr %p, rank %d gmtag %d front addr %p\n", 
	      atkey, addr, keyrank, gmtag, lrmtb[gmtag].addr);
      offset = (char *)addr - (lrmtb[gmtag].addr) ;
      ga = ((uint64_t)keyrank << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
	+ ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
	+ ((uint64_t)gmtag << OFFSET_BITS)
	+ offset;
      
      fprintf(stdout, "rank %d acp_query_ga ga %lx\n", myrank, ga);
      return ga;
    }
    }
  else{
    return ACP_GA_NULL;
  }
}

acp_atkey_t acp_register_memory(void* addr, acp_size_t size, int color){
  
  int i; /* general index */
  
  struct ibv_mr *mr; /* memory register return data */
  int mr_flags; /* memory register flag */
  
  acp_atkey_t key; /* memory register key */
  uint32_t gmtag; /* tag in GA */
  uint64_t offset = 0;/* offset in ga*/
  char found;
  
  /* color is only 0 */
  color = 0; 
  
  /* set memory register flags */
  mr_flags = IBV_ACCESS_LOCAL_WRITE | 
    IBV_ACCESS_REMOTE_READ |
    IBV_ACCESS_REMOTE_WRITE ;
  
  /* execute register memory */
  mr = ibv_reg_mr(res.pd, addr, size, mr_flags);
  
  fprintf(stdout, "mr address %p size %ld\n", mr, sizeof(mr));
  if (NULL == mr){
    fprintf(stderr, "acp_register_memory error\n");
    return ACP_ATKEY_NULL;
  }
  fprintf(stdout, 
	  "MR was registered with addr=0x%x, lkey=0x%x, rkey=0x%x, flags=0x%x\n",
	  (uintptr_t)addr, mr->lkey, mr->rkey, mr_flags);
  
  /* search invalid tag, and set tag */
  found = 0;
  for(i = 0;i < MAX_RM_SIZE;i++){
    if(lrmtb[i].size == 0){
      found = 1;
      lmrtb[i] = mr;
      lrmtb[i].rkey = mr->rkey;
      lrmtb[i].addr = addr;
      lrmtb[i].size = size;
      fprintf(stdout, 
	      "insert data into lrmtb[%d] addr %lx size %ld\n", 
	      i, lrmtb[i].addr, lrmtb[i].size);
      break;
    }
  }
  if(found == 0){
    return ACP_ATKEY_NULL;
  }
  else{
    /* set gmtag */
    gmtag = i;
    key = ((uint64_t)acp_myrank << (COLOR_BITS + GMTAG_BITS + OFFSET_BITS))
      + ((uint64_t)color << (GMTAG_BITS + OFFSET_BITS))
      + ((uint64_t)gmtag << OFFSET_BITS)
      + offset;
    fprintf(stdout, "rank %d atkey %lx gmtag %d\n", acp_myrank, key, gmtag);
    
    return key;
  }
}

void *acp_query_address(acp_ga_t ga){
  
  char *addr;
  char *base_addr;
  
  uint32_t rank;
  uint64_t offset;
  uint32_t gmtag;
  uint32_t color;
  
  /* get rank, gm tag, offset, color from ga */
  gmtag = query_gmtag(ga);
  rank = acp_query_rank(ga);
  offset = query_offset(ga);
  color = acp_query_color(ga);

  /* print tag id */
  fprintf(stdout, "gmtag = %d\n", gmtag);
  /* if rank of ga is my rank, */
  if(rank == acp_myrank){
    /* ga tag is TAG_SM,  */
    if(gmtag == TAG_SM){
      base_addr = sm;
    }
    else{
      if(lrmtb[gmtag].size != 0){
	base_addr = (char *)lrmtb[gmtag].addr;
	fprintf(stdout, 
		"LRMTB: tag %d addr %p size %d\n,",
		gmtag, lrmtb[gmtag].addr, lrmtb[gmtag].size);
      }
      else{
	return NULL;
      }
    }
  }
  else{
    return NULL;
  }
  
  addr = base_addr + offset;
  
  return addr;
}

acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, acp_size_t size, acp_handle_t order){
  
  acp_handle_t hdl; /* handle of copy */
  int myrank;/* my rank*/

  fprintf(stdout, "internal acp_copy\n");

  /* if queue is full, return ACP_HANDLE_NULL */
  if(head - (tail + 1) % MAX_CMDQ_ENTRY == 0){
    return ACP_HANDLE_NULL;
  }
  
  /* check my rank */
  myrank = acp_rank();
  /* make a command, and enqueue command Queue. */ 
  cmdq[tail].rank = myrank;
  cmdq[tail].type = COPY;
  cmdq[tail].ohdl = order;
  hdl = chdl;
  cmdq[tail].hdl = hdl;
  fprintf(stdout, "cmdq[%d].hdl = %d\n", tail, cmdq[tail].hdl);
  chdl++;
  cmdq[tail].stat = UNISSUED;
  cmdq[tail].gasrc = src;
  cmdq[tail].gadst = dst;
  cmdq[tail].size = size;
  
  /* update tail */
  tail = (tail + 1)% MAX_CMDQ_ENTRY;
  
  fprintf(stdout, "internal acp_copy fin\n");
  
  return hdl;
}

void acp_complete(acp_handle_t handle){
  
  int index;/* index of cmdq */
  int head_hdl; /* handle of head of cmdq */
  
  fprintf(stdout, "internal acp_complete\n"); 
  
  /* if disable handle, return */
  if(handle == ACP_HANDLE_NULL){
    return;
  }
  
  /* if cmdq have no command, return */
  if(head == tail){
    return;
  }
  /* set head to index */
  index = head;
  /* get hanlde of head of cmdq */
  head_hdl = cmdq[head].hdl; 
  if(handle < head_hdl){
    fprintf(stdout, "handle is alway finished\n");
    return;
  }

  fprintf(stdout, "head = %d tail = %d index = %d head_hdl = %d handle = %d\n", 
	  head, tail, index, head_hdl, handle);
  
  /* check status of handle if it is COMPLETED or not.*/
  while(1){
    if(cmdq[index].hdl < handle){
      if(COMPLETED == cmdq[index].stat){
	index = (index + 1) % MAX_CMDQ_ENTRY;
	fprintf(stdout, "head = %d tail = %d index = %d head_hdl = %d handle = %d\n", 
	       head, tail, index, head_hdl, handle);
      }
    }
    else{
      if(COMPLETED == cmdq[index].stat){
	head = (index + 1) % MAX_CMDQ_ENTRY;
	break;
      }
    }
    /* fprintf(stdout, 
       "head = %d tail = %d index = %d, head_hdl = %d handle = %d type %d stat %d\n", 
       head, tail, index, head_hdl, handle, cmdq[index].type, cmdq[index].stat);*/
  }
  
  fprintf(stdout, "internal acp_complete fin\n"); 
  
  return;
}

/* get remote register memory table */
int getlrm(acp_handle_t handle, int torank){
  
  struct ibv_sge sge; /* scatter/gather entry */
  struct ibv_send_wr sr; /* send work reuqest */
  struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
  
  uint64_t remote_offset;/* the offset of remote RM */
  
  int rc; /* return code */
  
  /* prepare the scatter/gather entry */
  memset(&sge, 0, sizeof(sge));
  /* if local tag point to starter memory */
  sge.addr = (uintptr_t)sm + acp_smsize + sizeof(RM) * MAX_RM_SIZE;
  sge.lkey = res.mr->lkey;
  
  /* set message size */
  sge.length = sizeof(RM) * MAX_RM_SIZE;
  fprintf(stdout, "get lrm length %d\n", sge.length);
  
  /* prepare the send work request */
  memset(&sr, 0, sizeof(sr));
  sr.next = NULL;
  /* Work request ID is set by Acp_handle_queue end*/
  sr.wr_id =  handle;
  sr.sg_list = &sge;
  sr.num_sge = 1;
  fprintf(stdout, "get lrm wr_id %ld handle %ld\n", sr.wr_id, handle);

  /* Set Get opcode in send work request */
  sr.opcode = IBV_WR_RDMA_READ;
  /* Set send flag in send work request */
  sr.send_flags = IBV_SEND_SIGNALED;
  
  remote_offset = acp_smsize;
  /* Set remote address and rkey in send work request */
  sr.wr.rdma.remote_addr = smi_tb[torank].addr + remote_offset;
  sr.wr.rdma.rkey = smi_tb[torank].rkey;
  
  /* post send by ibv_post_send */
  rc = ibv_post_send(qp[torank], &sr, &bad_wr);
  if (rc){
    fprintf(stderr, "failed to post SR\n");
    exit(rc);
  }
  fprintf(stdout, "getlrm ibv_post_send return code = %d\n", rc);

  return rc;
}


int icopy(acp_handle_t handle, acp_ga_t dst, acp_ga_t src, acp_size_t size, int index)
{
  struct ibv_sge sge; /* scatter/gather entry */
  struct ibv_send_wr sr; /* send work reuqest */
  struct ibv_send_wr *bad_wr = NULL;/* return of send work reuqest */
  
  int torank = -1; /* remote rank */
  int rc; /* return code */
  
  int srcrank, dstrank; /* srouce rank, and destination rank */
  uint64_t srcoffset, dstoffset;/* soruce and destination offset*/
  uint64_t local_offset, remote_offset;/* local and remote offset */
  uint32_t srcgmtag, dstgmtag; /* soruce and destination offset*/
  uint32_t local_gmtag, remote_gmtag;/* local and remote GMA tag */
  int myrank;/* my rank id */
  
  /* get my rank*/
  myrank = acp_rank();
  fprintf(stdout, "internal icopy\n");
  /* get srouce rank, global tag and offset */
  srcrank = acp_query_rank(src);
  srcgmtag = query_gmtag(src);
  srcoffset = query_offset(src);
  /* get destination rank, global tag and offset */
  dstrank = acp_query_rank(dst);  
  dstgmtag = query_gmtag(dst);
  dstoffset = query_offset(dst);

  /* print ga rank, index, offset */
  fprintf(stdout, "mr %d src %lx sr %x st %x so %lx dst %lx dr %x dt %x do %lx\n", 
	  myrank, src, srcrank, srcgmtag, srcoffset, dst, dstrank, dstgmtag, dstoffset);
  
  /* set local/remote tag, local/remote offset and remote rank */
  if(myrank == srcrank){
    local_offset = srcoffset;
    local_gmtag = srcgmtag;
    remote_offset = dstoffset;
    remote_gmtag = dstgmtag;
    torank = dstrank;
  }
  else if(myrank == dstrank){
    local_offset = dstoffset;
    local_gmtag = dstgmtag;
    remote_offset = srcoffset;
    remote_gmtag = srcgmtag;
    torank = srcrank;
  }
  fprintf(stdout, "target rank %d\n", torank);
  
  if(myrank == srcrank && myrank == dstrank){
    void *srcaddr, *dstaddr;
    srcaddr = acp_query_address(src);
    dstaddr = acp_query_address(dst);
    fprintf(stdout, "dadr %p sadr %p\n", dstaddr, srcaddr);
    memcpy(dstaddr, srcaddr, size);
    cmdq[index].stat = COMPLETED;
    return 0;
  }
  
  /* prepare the scatter/gather entry */
  memset(&sge, 0, sizeof(sge));
  /* if local tag point to starter memory */
  if(local_gmtag == TAG_SM){
    sge.addr = (uintptr_t)sm + local_offset;
    sge.lkey = res.mr->lkey;
  }
  else{
    /* if local tag do not point to starter memory */
    if(lrmtb[local_gmtag].size != 0){
      sge.addr = (uintptr_t)(lrmtb[local_gmtag].addr) + local_offset;
      sge.lkey = lmrtb[local_gmtag]->lkey;
    }
    else{
      fprintf(stderr, "Invalid TAG of This GA");
      rc = -1;
      return rc;
    }
  }
  /* set message size */
  sge.length = size;
  fprintf(stdout, "copy len %d\n", sge.length);
  
  /* prepare the send work request */
  memset(&sr, 0, sizeof(sr));
  sr.next = NULL;
  /* Work request ID is set by Acp_handle_queue end*/
  sr.wr_id =  handle;
  sr.sg_list = &sge;
  sr.num_sge = 1;
  fprintf(stdout, "icopy wr_id  %ld handle %ld\n", sr.wr_id, handle);
  
  /* Set put opcode in send work request */
  if(myrank == srcrank){
    sr.opcode = IBV_WR_RDMA_WRITE;
  }
  /* Set Get opcode in send work request */
  else if(myrank == dstrank){
    sr.opcode = IBV_WR_RDMA_READ;
  }
  /* Set send flag in send work request */
  sr.send_flags = IBV_SEND_SIGNALED;
  
  /* Set remote address and rkey in send work request */
  /* using starter memory */
  if( remote_gmtag == TAG_SM){
    sr.wr.rdma.remote_addr = smi_tb[torank].addr + remote_offset;
    sr.wr.rdma.rkey = smi_tb[torank].rkey;
  }
  /* using general memory */
  else{
    sr.wr.rdma.remote_addr = (uintptr_t)(rrmtb[torank][remote_gmtag].addr) + remote_offset;
    sr.wr.rdma.rkey = rrmtb[torank][remote_gmtag].rkey;
  }
  
  if(myrank == srcrank){
    fprintf(stdout, "put addr %x rkey = %x\n", sr.wr.rdma.remote_addr, sr.wr.rdma.rkey);
  }
  else if(myrank == dstrank){
    fprintf(stdout, "get addr %x rkey = %x\n", sr.wr.rdma.remote_addr, sr.wr.rdma.rkey);
  }
  
  /* post send by ibv_post_send */
  rc = ibv_post_send(qp[torank], &sr, &bad_wr);
  if (rc){
    fprintf(stderr, "failed to post SR\n");
    exit(rc);
  }
  fprintf(stdout, "icopy ibv_post_send return code = %d\n",rc);
  fprintf(stdout, "internal icopy fin\n");
  
  return rc;
}

void *comm_thread_func(void *dm){
  
  int i; /* general index */
  int rc; /* return code for cq */
  struct ibv_wc wc; /* work completion for poll_cq*/
  int index; /* cmdq index */
  char prevstatcomp = 1; /* previous cmdq handle status*/
  char chcomp_fcqe = 0; /* complete to change status from CQE */
  int myrank; /* my rank id */
  
  /* get my rank id */
  myrank = acp_rank();
  
  while(1){
    /* CHECK IB COMPLETION QUEUE section */
    rc = ibv_poll_cq(cq, 1, &wc);
    /* error of ibv poll cq*/
    if(rc < 0){
      fprintf(stderr, "Fail poll cq\n");
      exit(-1);
    }
    /* if(rc == 1)fprintf(stdout, "poll ct %d, wc.wr_id %d\n", rc, wc.wr_id);*/
    if(rc > 0){
      index = head;
      chcomp_fcqe = 0;
      prevstatcomp = 1;
      /* fprintf(stdout, "wr_id %ld\n", wc.wr_id);*/
      if(wc.status == IBV_WC_SUCCESS){
	while(1){
	  /* check which handle command complete. */
	  if(cmdq[index].hdl == wc.wr_id){
	    switch(cmdq[index].stat){
	    case ISSUED: /* issueing gma command */
	      cmdq[index].stat = FINISHED;
	      /* if index is head, set COMPLETED status */
	      if(index == head){
		cmdq[index].stat = COMPLETED;
		/* set TRUE to flag of previous status completion  */
		prevstatcomp = 1;
	      }
	      /* set TRUE to flag of changing status by CEQ */
	      chcomp_fcqe = 1;
	      break;
	    case GET_WAIT:/* waiting for get rkey table */
	      cmdq[index].stat = GET_COMP;
	      /* set TRUE to flag of changing status by CEQ */
	      chcomp_fcqe = 1;
	      /* set FALSE to flag of previous completion status */
	      prevstatcomp = 0;
	      break;
	    default:
	      /* ignore except handle of acp_copy  */
	      break;
	    }
	  }
	  /* check command completion */
	  else if(prevstatcomp == 1){
	    if(cmdq[index].stat == FINISHED){
	      cmdq[index].stat = COMPLETED;
	    }
	    else{
	      /* cmdq has a incomplete command until a command tail */
	      prevstatcomp = 0;
	    }
	  }
	  /* next cmd queue */
	  index = (index + 1 ) % MAX_CMDQ_ENTRY;
	  /* when ibv_poll_cq is SUCCESS,
	     if cmdq index toutch tail and change at least one command by CQE, 
	     break;*/
	  if(index == tail && chcomp_fcqe == 1) break;
	}
      }
      else {
	fprintf(stderr, "wc is not SUCESS when check CQ %ld\n", wc.status);
	exit(-1);
      }
    }
    /* CHECK COMMAND QUEUE section */
    /* if cmdq is empty, continue */
    if(head == tail){
      continue;
    }
    /* cmdq is not empty */
    else{
      index = head;
      while(1){
	fprintf(stdout, 
		"head %d tail %d, cmdq[%d].stat %d\n", 
		head, tail, index, cmdq[index].stat);
	/* check if comand is complete or not. */
	if(cmdq[index].stat != COMPLETED){
	  /* check command type */
	  /* command type is FIN */
	  if(cmdq[index].type == FIN){
	    fprintf(stdout, "CMD FIN\n");
	    return 0;
	  }
	  /* command type is COPY */
	  else if(cmdq[index].type == COPY){
	    if(cmdq[index].stat == UNISSUED){
	      
	      acp_ga_t src, dst;
	      acp_size_t size;
	      int torank, dstrank, srcrank;
	      int totag, dsttag, srctag;
	      
	      fprintf(stdout, "CMD COPY issued\n");
	      src = cmdq[index].gasrc;
	      dst = cmdq[index].gadst;
	      size = cmdq[index].size;
	      /* get rank and tag of src ga */
	      srcrank = acp_query_rank(src);
	      srctag = query_gmtag(src);
	      /* get rank and tag of dst ga */
	      dstrank = acp_query_rank(dst);
	      dsttag = query_gmtag(dst);
	      
	      /* local copy */
	      if(myrank == dstrank && myrank == srcrank){
		torank = dstrank;
	      }
	      /* get */
	      else if(myrank == dstrank){
		totag = srctag;
		torank = srcrank;
	      }
	      /* put */
	      else if (myrank == srcrank){
		totag = dsttag;
		torank = dstrank;
	      }

	      /* chekc tag */
	      /* if tag point stater memory */
	      if(totag == TAG_SM){
		icopy(cmdq[index].hdl, dst, src, size, index);
		if(cmdq[index].stat != COMPLETED){
		  cmdq[index].stat = ISSUED;
		}
	      }
	      /* if tag point globl memory */
	      else{
		/* if have a remote rm table of target rank */
		if(rrmtb[torank] != NULL){
		  fprintf(stdout, 
			  "CMD COPY have a rrmtb. rank %d torank %d, totag %d \n",
			  myrank, torank, totag);
		  /* if tag entry is active */
		  if(rrmtb[torank][totag].size != 0){
		    fprintf(stdout, 
			    "CMD COPY have a entry rank %d torank %d, totag %d\n",
			    myrank, torank, totag);
		    icopy(cmdq[index].hdl, dst, src, size, index);
		    if(cmdq[index].stat != COMPLETED){
		      cmdq[index].stat = ISSUED;
		    }
		  }
		  /* if tag entry is non active */
		  else{
		    fprintf(stdout, 
			    "CMD COPY no entry.  rank %d torank %d, totag %d\n",
			    myrank, torank, totag);
		    getlrm(cmdq[index].hdl, torank);
		    cmdq[index].stat = GET_WAIT;
		  }
		}
		/* if do not have rm table of target rank */
		else{
		  /* if target rank is not myrank */
		  if(torank != myrank){
		    fprintf(stdout, 
			    "CMD COPY no rrmtb.  rank %d torank %d, totag %d\n",
			    myrank, torank, totag);
		    getlrm(cmdq[index].hdl, torank);
		    cmdq[index].stat = GET_WAIT;
		  }
		  /* if target rank is myrank (local copy) */
		  else{
		    icopy(cmdq[index].hdl, dst, src, size, index);
		    if(cmdq[index].stat != COMPLETED){
		      cmdq[index].stat = ISSUED;
		    }
		  }
		}
	      }
	      /* command issue, break*/
	      break;
	    }
	    /* command status is GET_COMP */
	    else if(cmdq[index].stat == GET_COMP){
	      int torank, dstrank, srcrank;
	    
	      printf("CMD COPY GET RMtb\n");
	      /* get logical address src and dst */
	      srcrank = acp_query_rank(cmdq[index].gasrc);
	      dstrank = acp_query_rank(cmdq[index].gadst);
	      
	      /* set target rank */
	      if(myrank == dstrank){
		torank = srcrank;
	      }
	      else if (myrank == srcrank){
		torank = dstrank;
	      }
	      /* set RM table */
	      for(i = 0; i< MAX_RM_SIZE; i++){
		if(rrmtb[torank] == NULL){
		  rrmtb[torank] = (RM *)malloc(sizeof(RM) * MAX_RM_SIZE);
		}
		rrmtb[torank][i].rkey = recv_lrmtb[i].rkey;
		rrmtb[torank][i].addr = recv_lrmtb[i].addr;
		rrmtb[torank][i].size = recv_lrmtb[i].size;
		fprintf(stdout, "torank %d tag %d size %d\n", torank, i, rrmtb[torank][i].size);
	      }
	      /* set command status UNISSUDE */
	      cmdq[index].stat = UNISSUED;
	      break;
	    }
	  }
	}
	index = (index + 1) % MAX_CMDQ_ENTRY;
	if(index == tail) break;
      }
    }
  }
}

int acp_init(int *argc, char ***argv){
 
  int rc = 0;/* return code */
  int i, j; /* general index */
  
  uint32_t my_port;/* my port */
  uint32_t dst_port;/* destination port */
  uint32_t dst_addr;/* destination ip address */
  
  int sock_s;  /* socket server */
  int addrlen;/* address length */
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
    
  if(*argc < 7) return -1;

  acp_myrank = strtol((*argv)[1], NULL, 0);
  acp_numprocs = strtol((*argv)[2], NULL, 0);
  acp_smsize = strtol((*argv)[3], NULL, 0);
  my_port = strtol((*argv)[4], NULL, 0);
  dst_port = strtol((*argv)[5], NULL, 0);
  dst_addr = inet_addr((*argv)[6], NULL, 0);
  
  /* print acp_init argument */
  fprintf(stdout, 
	  "rk %ld np %ld ss %ld mp %d dp %d da %d\n", 
	  acp_myrank, acp_numprocs, acp_smsize, 
	  my_port, dst_port, dst_addr);
  
  /* allocate the starter memory adn register memory */
  int smsize;
  smsize = acp_smsize + sizeof(RM) * (MAX_RM_SIZE) * 2;
  sm = (char *) malloc(smsize);
  if (!sm ){
    fprintf(stderr, "failed to malloc %Zu bytes to memory buffer\n", 
	    smsize);
    rc = -1;
    goto exit;
  }
  /* initalize starter memory, local RM table, recv RM table */
  memset(sm, 0 , smsize);
  /* initalize CMD queue, set COMPLETED status, set not FIN type */
  memset(cmdq, 0, sizeof(CMD)*MAX_CMDQ_ENTRY);
  /* initalize local register memory */
  lrmtb = (RM *) &sm[acp_smsize];
  for(i = 0;i < MAX_RM_SIZE;i++){
    /* entry non active */
    lrmtb[i].size = 0;
  }
  /* initalize recv local register memory */
  recv_lrmtb = (RM *) (sm + acp_smsize + sizeof(RM) * MAX_RM_SIZE);
  for(i = 0;i < MAX_RM_SIZE;i++){
    /* entry non active */
    recv_lrmtb[i].size = 0;
  }
  
  /* remote register memory table */
  rrmtb = (RM **) malloc (sizeof (RM *) * acp_numprocs);
  
  /* generate server socket */
  if((sock_s = socket (AF_INET, SOCK_STREAM, 0)) < 0){
    fprintf(stderr, "sever socket() failed \n");
    rc = -1;
    goto exit;
  }
  int on;
  on = 1;/* enable address resuse, as soon as possible */
  rc = setsockopt( sock_s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on) );
  
  /* initialize myaddr of sockaddr_in  */
  memset(&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = INADDR_ANY;
  myaddr.sin_port = my_port;
  
  /* bind socket file descriptor*/
  if(bind(sock_s, (struct sockaddr*)&myaddr, sizeof(myaddr)) < 0){
    fprintf(stderr, "bind() failed\n");
    rc = -1;
    goto exit;
  }
  
  if(listen(sock_s, 2) < 0){
    rc = -1;
    goto exit;
  }
  
  /* generate connection socket */
  if((sock_connect = socket (AF_INET, SOCK_STREAM, 0)) < 0){
    fprintf(stderr, "connect socket() failed \n");
  }
  
  /* generate destination address */
  memset(&dstaddr, 0, sizeof(dstaddr));
  dstaddr.sin_family = AF_INET;
  dstaddr.sin_addr.s_addr = dst_addr;
  dstaddr.sin_port = dst_port;
  
    /* get the size of source address */
  addrlen = sizeof(srcaddr);
  if(acp_numprocs > 1){
    if((acp_myrank & 1) == 0){/* odd rank  */
      if((sock_accept = accept(sock_s, (struct sockaddr *)&srcaddr, &addrlen)) < 0){
	fprintf(stderr, "accept() failed\n");
	goto exit;
      }
      while(connect(sock_connect, (struct sockaddr*)&dstaddr, sizeof(dstaddr)) < 0);
    }
    else{/* even rank */
      while(connect(sock_connect, (struct sockaddr*)&dstaddr, sizeof(dstaddr)) < 0);
      if((sock_accept = accept(sock_s, (struct sockaddr *)&srcaddr, &addrlen)) < 0){
	fprintf(stderr, "accept() failed\n");
	goto exit;
      }
    }
  }
  
  /* socket close */
  if(sock_s){
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
  if (!num_devices){
    fprintf(stderr, "found %d device(s)\n", num_devices);
    rc = -1;
    goto exit;
  }
  fprintf(stdout, "found %d device(s)\n", num_devices);
  
  dev_name = strdup(ibv_get_device_name(dev_list[0]));
  if(!dev_list[0]){
    fprintf(stderr, "IB device wasn't found\n");
    rc = -1;
    goto exit;
  }
  fprintf(stdout, "First IB device name is %s\n", dev_name);
  /* get device handle */
  res.ib_ctx = ibv_open_device(dev_list[0]);
  if (!res.ib_ctx){
    fprintf(stderr, "failed to open device\n");
    rc = -1;
    goto exit;
  }
  
  /* free device list */
  ibv_free_device_list(dev_list);
  dev_list = NULL;
  
  if(ibv_query_port(res.ib_ctx, ib_port, &res.port_attr)){
    fprintf(stderr, "ibv_query_port on port %u failed\n", ib_port);
    rc = -1;
    goto exit;
  }
  
  res.pd = ibv_alloc_pd(res.ib_ctx);
  if (!res.pd){
    fprintf(stderr, "ibv_alloc_pd failed\n");
    rc = -1;
    goto exit;
  }
  
  /* allocate Protection Domain */
  res.pd = ibv_alloc_pd(res.ib_ctx);
  if (!res.pd){
    fprintf(stderr, "ibv_alloc_pd failed\n");
    rc = -1;
    goto exit;
  }
  
  /* each side will send only one WR, so Completion Queue with 1 entry is enough */
  cq = NULL;
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
    IBV_ACCESS_REMOTE_WRITE ;
  res.mr = ibv_reg_mr(res.pd, sm, smsize, mr_flags);
  if (!res.mr){
    fprintf(stderr, 
	    "ibv_reg_mr failed with mr_flags=0x%x\n", 
	    mr_flags);
    rc = -1;
    goto exit;
  }
  fprintf(stdout, 
	  "MR was registered with addr=0x%x, lkey=0x%x, rkey=0x%x, flags=0x%x\n",
	  (uintptr_t)sm, res.mr->lkey, res.mr->rkey, mr_flags);
  
  /* create the Queue Pair */
  qp = (struct ibv_qp **) malloc(sizeof(struct ibv_qp *) * acp_numprocs);
  
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
  /* temporary arays of queue pair number for data recieving */
  tmp_qp_num = (uint32_t *)malloc(sizeof(uint32_t) * acp_numprocs);
  /* remote arays of queue pair number */
  remote_qp_num = (uint32_t *)malloc(sizeof(uint32_t) * acp_numprocs);
  
  for(i = 0;i < acp_numprocs;i++){
    qp[i] = ibv_create_qp(res.pd, &qp_init_attr);
    if (!qp[i]) {
      fprintf(stderr, "failed to create QP\n");
      rc = -1;
      goto exit;
    }
    local_qp_num[i] = qp[i]->qp_num;
    fprintf(stdout, "QP[%d] was created, QP number=0x%x\n", 
	    i, local_qp_num[i]);
  }
  
  /* exchange using TCP sockets info required to connect QPs */
  local_data.addr = (uintptr_t)sm;
  local_data.rkey = res.mr->rkey;
  local_data.lid = res.port_attr.lid;
  local_data.rank = acp_myrank;
  smi_tb = (SMI *)malloc(sizeof(SMI) * acp_numprocs);
  
  fprintf(stdout, "local address = 0x%x\n", local_data.addr);
  fprintf(stdout, "local rkey = 0x%x\n", local_data.rkey);
  fprintf(stdout, "local LID = 0x%x\n", local_data.lid);
  
  tmp_data.addr = local_data.addr;
  tmp_data.rkey = local_data.rkey;
  tmp_data.lid = local_data.lid;
  tmp_data.rank = local_data.rank;
  for(i = 0;i < acp_numprocs; i++){
    tmp_qp_num[i] = local_qp_num[i];
  }
  
  /* address and rkey of my process */
  smi_tb[acp_myrank].addr = local_data.addr;
  smi_tb[acp_myrank].rkey = local_data.rkey;
  
  for(i = 0;i < acp_numprocs - 1;i++){
    /* TCP sendrecv */
    memset(&remote_data, 0, sizeof(remote_data));
    memset(remote_qp_num, 0, sizeof(uint32_t) * acp_numprocs);
    
    write(sock_connect, &tmp_data, sizeof(tmp_data));
    recv(sock_accept, &remote_data, sizeof(remote_data),0 );
    write(sock_connect, tmp_qp_num, sizeof(uint32_t) * acp_numprocs);
    recv(sock_accept, remote_qp_num, sizeof(uint32_t) * acp_numprocs,0 );
    
    /* copy addr and rkey to start buffer information table */
    torank = remote_data.rank;
    smi_tb[torank].addr = remote_data.addr;
    smi_tb[torank].rkey = remote_data.rkey;
    
    /* save the remote side attributes, we will need it for the post SR */
    fprintf(stdout, "Remote address = 0x%x\n", remote_data.addr);
    fprintf(stdout, "Remote rkey = 0x%x\n", remote_data.rkey);
    fprintf(stdout, "Remote rank = %d\n", torank);
    fprintf(stdout, "Remote LID = 0x%x\n", remote_data.lid);
    fprintf(stdout, "Remote QP number = %d 0x%x\n", torank, remote_qp_num[acp_myrank]);
    
    /* modify the QP to init */
    /* initialize ibv_qp_attr */
    memset(&attr, 0, sizeof(attr));
    
    /* set attribution for RTS */
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = ib_port;
    attr.pkey_index = 0;
    attr.qp_access_flags = 
      IBV_ACCESS_LOCAL_WRITE | 
      IBV_ACCESS_REMOTE_READ |
      IBV_ACCESS_REMOTE_WRITE;
    
    /* set flag for RTS */
    flags = IBV_QP_STATE |
      IBV_QP_PKEY_INDEX | 
      IBV_QP_PORT | 
      IBV_QP_ACCESS_FLAGS;
    
    rc = ibv_modify_qp(qp[torank], &attr, flags);
    
    if (rc){
      fprintf(stderr, "change QP state to INIT failed\n");
      goto exit;
    }
    fprintf(stdout, "QP state was change to INIT\n");
    
    /* let the client post RR to be prepared for incoming messages */
    /* prepare the scatter/gather entry */
    memset(&sge, 0, sizeof(sge));
    
    sge.addr = (uintptr_t)sm;
    sge.length = acp_smsize;
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
    fprintf(stdout, "Post Receive Request\n");
    
    /* modify the QP to RTR */
    memset(&attr, 0, sizeof(attr));
   
    /* set attribution for RTR */
    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_256;
    attr.dest_qp_num = remote_qp_num[acp_myrank];
    attr.rq_psn = 0;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;/* recommend value 0x12. miminum RNR(receive not ready) NAK timer */
    attr.ah_attr.is_global = 0;/* global address, use global routing header */
    attr.ah_attr.dlid = remote_data.lid;
    attr.ah_attr.sl = 0;/* ??? */
    attr.ah_attr.src_path_bits = 0;/* ??? */
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
    fprintf(stdout, "QP state was change to RTR\n");
    
    /* modify the QP to RTS */
    memset(&attr, 0, sizeof(attr));
    
    /* set attribution for RTS */
    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 0x12;
    attr.retry_cnt = 7;/* recommended 7 */
    attr.rnr_retry = 7;/* recommended 7 */
    attr.sq_psn = 0;/* send queue starting packet sequence number (should match remote QP’s rq_psn) */
    attr.max_rd_atomic = 1;/* # of outstanding RDMA reads and atomic operations allowed.*/
  
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
    fprintf(stdout, "QP state was change to RTS\n");
    
    /* copy remote data to temporary buffer for bukects relay */
    tmp_data.addr = remote_data.addr;
    tmp_data.rkey = remote_data.rkey;
    tmp_data.lid = remote_data.lid;
    tmp_data.rank = remote_data.rank;
    for(j = 0;j < acp_numprocs; j++){
      tmp_qp_num[j] = remote_qp_num[j];
    }
  }

  pthread_create(&comm_thread_id, NULL, comm_thread_func, NULL);
  
  return rc;
  
 exit:
  return rc;

}

int acp_finalize(){
  
  int myrank;/* my rank for command*/
  
  fprintf(stdout, "internal acp_finalize\n");
  
  /* Insert FIN command into cmdq */
  /* if queue is full, return ACP_HANDLE_NULL */
  while(head - (tail + 1) % MAX_CMDQ_ENTRY == 0);
   
  /* check my rank */
  myrank = acp_rank();
  /* make a FIN command, and enqueue command Queue. */ 
  cmdq[tail].rank = myrank;
  cmdq[tail].type = FIN;
  cmdq[tail].hdl = chdl;
  cmdq[tail].stat = ISSUED;
  
  /* update tail */
  tail = (tail + 1) % MAX_CMDQ_ENTRY;
  
  /* complete hdl */
  // acp_complete(chdl);
  // sync();
  pthread_join(comm_thread_id, NULL);
  
  if(sock_accept){
    close(sock_accept);
  }
  if(sock_connect){
    close(sock_connect);
  }
  
  /* initailize acpbl variables */
  acp_myrank = -1;
  acp_numprocs = -1;
  acp_smsize = -1;
  chdl = 1;
  head = 0;
  tail = 0;
  
  fprintf(stdout, "internal acp_finalize fin\n");

  return 0;
}
