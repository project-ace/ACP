/*****************************************************************************/
/***** ACP Basic Layer / Tofu					         *****/
/*****   function thread						 *****/
/*****									 *****/
/***** Copyright FUJITSU LIMITED 2014					 *****/
/*****									 *****/
/***** Specification Version: ACP-140312				 *****/
/***** Version: 0.1							 *****/
/***** Module Version: 0.1						 *****/
/*****									 *****/
/***** Note:								 *****/
/*****************************************************************************/
#define _XOPEN_SOURCE 600
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <pthread.h>
#include "acp.h"
#include "acpbl.h"
#include "acpbl_sync.h"
#include "acpbl_tofu2.h"
#include <sys/time.h>

size_t iacp_starter_memory_size_dl = 64 * 1024 * 1024;
size_t iacp_starter_memory_size_cl = 1024;//64 * 1024 * 1024;

/*---------------------------------------------------------------------------*/
/*** external functions ******************************************************/
/*---------------------------------------------------------------------------*/
extern int  _acpblTofu_atkey_init();
extern int  _acpblTofu_atkey_free();
extern int  _acpblTofu_delegation_init();
extern void _acpblTofu_start_communication_thread();
extern int  _acpblTofu_stop_communication_thread();
extern int  _acpblTofu_register_memory(void *addr, acp_size_t size, 
				       int color, int localtag, int type);
extern int  _acpblTofu_unregister_memory(int localtag);
extern int  _acpblTofu_enable_localtag(int localtag);
extern int  _acpblTofu_sys_init(int rank);
extern void _acpblTofu_sys_ga_init();
extern int  _acpblTofu_sys_get_rank();
extern int  _acpblTofu_sys_num_colors();
extern int  _acpblTofu_sys_barrier();
extern acp_atkey_t _acpblTofu_gen_atkey(int rank, int color, int localtag);


/*---------------------------------------------------------------------------*/
/*** variables ***************************************************************/
/*---------------------------------------------------------------------------*/
extern int	  ga_bitwidth_color, ga_bitwidth_localtag, 
  		  ga_bitwidth_rank, ga_bitwidth_offset;
extern uint64_t   ga_lsb_color,  ga_lsb_localtag,  ga_lsb_rank,  ga_lsb_offset;
extern uint64_t   ga_mask_color, ga_mask_localtag, ga_mask_rank, ga_mask_offset;
extern int	  last_registered_localtag;
extern localtag_t **localtag_table;
extern uint32_t   max_num_localtag;
extern tofu_trans_stat_t tofu_trans_stat_save;
extern int 	  tofu_trans_stat_save_flag;
extern volatile delegation_buff_t *delegation_buff; /* delegation buffer */
extern int	  *rank_us_map;			    /* rank logical/physical map */

int	 ACP_ERRNO;
int	 myrank_sys = -1;			/* system defined rank */
int	 myrank;				/* user defined rank */
int	 jobid;
int	 num_procs;

int	 sys_state = SYS_STAT_FREE;
void	 *starter = NULL;
void	 *starter_dl = NULL;
void	 *starter_cl = NULL;
size_t	 starter_size;

volatile cq_t *cq = NULL;			/* command queue */
volatile uint32_t cqwp, cqrp, cqcp, cqlk;       /* command queue pointers */
volatile dq_t *dq = NULL;			/* delegation queue */
volatile uint32_t dqwp, dqrp, dqcp, dqlk;	/* delegation queue pointers */

int	 in_error_cleanup = 0;
int	 print_level = 0;
uint64_t profile[10];

/*---------------------------------------------------------------------------*/
/*** macros ******************************************************************/
/*---------------------------------------------------------------------------*/
#define  CMD_LOCATION(_cmd, _dst, _src) 				\
  ( _cmd |								\
    (((GA2RANK(_dst) == myrank_sys)? 2 : 0) + ((GA2RANK(_src) == myrank_sys)? 1 : 0)) )


/*---------------------------------------------------------------------------*/
/*** local functions *********************************************************/
/*---------------------------------------------------------------------------*/
int queue_init(void)
{
  int i, rc;

  switch(sys_state){
  case SYS_STAT_INITIALIZE:
    /*** allocate and register  queue memory ***/
    /** command queue **/
    /* command queue memory resion must be registered and enabeled,
       because command queue is used as delegation request buffer */

    /* allocate memory */
    rc = posix_memalign((void **)&cq, 256, sizeof(cq_t)*CQ_DEPTH);
    if(rc) ERROR_RETURN("cq: posix_memalign failed", rc);

    /* register allocated memory */
    rc = _acpblTofu_register_memory((void*)cq, sizeof(cq_t)*CQ_DEPTH,
				     COLOR_CQ, LOCALTAG_CQ, 
				     MEMTYPE_STARTER);
    if(rc == FAILED) return rc;

    /* enable registerd memory */
    rc = _acpblTofu_enable_localtag(LOCALTAG_CQ);
    if(rc) return rc;

    /** delegation queue **/
    /* delagation queue itself is not used for data transfer */
    /* allocate memory */
    dq = malloc(sizeof(dq_t)*DQ_DEPTH);
    if(dq == NULL)
      ERROR_RETURN("dq: malloc failed", -1);

  case SYS_STAT_RESET:
    /*** initialize queue and queue pointer ***/
    /** command queue **/
    for(i=0; i<CQ_DEPTH; i++)
      cq[i].copy.base.run_stat = 0;
    //cqwp = 0xFFFFFFF1, cqrp = 0xFFFFFFF1, cqcp = 0xFFFFFFF0; // debug
    dprintf("addressof(cqwp) = %x, addressof(cqrp) = %x, addressof(cqcp) = %x\n", &cqwp, &cqrp, &cqcp);
    cqwp = 1, cqrp = 1, cqcp = 0;

    /** delegation queue **/
    for(i=0; i<DQ_DEPTH; i++){
      dq[i].command = NULL;
      dq[i].comm_id = 0;
    }
    dqwp = 1, dqrp = 1, dqcp = 0;

    sync_synchronize();
    cqlk = 0;
    dqlk = 0;
    return SUCCEEDED;
  }
  ERROR_RETURN("queue initialization: unacceptable system state", sys_state);
}

int queue_free()
{
  int rc;

  switch(sys_state){
  case SYS_STAT_FINALIZE:
    rc = _acpblTofu_unregister_memory(LOCALTAG_CQ);
    if(rc)
      return rc;
    free((void *)cq);
    cq = NULL;
    free((void *)dq);
    dq = NULL;

  case SYS_STAT_RESET:
    return SUCCEEDED;
  }
  ERROR_RETURN("queue finalization: unacceptable system state", sys_state);
}

/*---------------------------------------------------------------------------*/
int starter_init()
{
  int rc;
  char *str;

  switch(sys_state){
  case SYS_STAT_INITIALIZE:
    /*** acpbl sterter memory ***/
    /** obtain starter size **/
    str = getenv("ACPBL_TOFU_STARTER_SIZE");
    if(str != NULL && str[0] != '\0'){
      starter_size = atol(str);
      if(starter_size < STARTER_SIZE_DEFAULT)
	starter_size = STARTER_SIZE_DEFAULT;
    }
    if(starter_size > (ga_mask_offset >> ga_lsb_offset))
      ERROR_RETURN("starter initialization: stater memory out of range",
		   starter_size);
    /** allocate memory **/
    rc = posix_memalign(&starter, 256, starter_size);
    if(rc) ERROR_RETURN("starter initialization: posix_memalign failed", rc);
    /** register allocated memory **/
    rc = _acpblTofu_register_memory(starter, starter_size, COLOR_STARTER, 
				    LOCALTAG_STARTER_BL, MEMTYPE_STARTER);
    if(rc == FAILED) return rc;
    /** enable registered memory **/
    rc = _acpblTofu_enable_localtag(LOCALTAG_STARTER_BL);
    if(rc) return rc;

    /*** acpdl sterter memory ***/
    /** allocate memory **/
    rc = posix_memalign(&starter_dl, 256, iacp_starter_memory_size_dl);
    if(rc) ERROR_RETURN("starter_dl initialization: posix_memalign failed", rc);
    /** register allocated memory **/
    rc = _acpblTofu_register_memory(starter_dl, iacp_starter_memory_size_dl, COLOR_STARTER, 
				    LOCALTAG_STARTER_DL, MEMTYPE_STARTER);
    if(rc == FAILED) return rc;
    /** enable registered memory **/
    rc = _acpblTofu_enable_localtag(LOCALTAG_STARTER_DL);
    if(rc) return rc;

    /*** acpcl sterter memory ***/
    /** allocate memory **/
    rc = posix_memalign(&starter_cl, 256, iacp_starter_memory_size_cl);
    if(rc) ERROR_RETURN("starter_cl initialization: posix_memalign failed", rc);
    /** register allocated memory **/
    rc = _acpblTofu_register_memory(starter_cl, iacp_starter_memory_size_cl, COLOR_STARTER, 
				    LOCALTAG_STARTER_CL, MEMTYPE_STARTER);
    if(rc == FAILED) return rc;
    /** enable registered memory **/
    rc = _acpblTofu_enable_localtag(LOCALTAG_STARTER_CL);
    if(rc) return rc;

  case SYS_STAT_RESET:
    return SUCCEEDED;
  }
  ERROR_RETURN("starter initialization: unacceptable system state", sys_state);
}

int starter_free()
{
  int rc;

  switch(sys_state){
  case SYS_STAT_FINALIZE:
    rc = _acpblTofu_unregister_memory(LOCALTAG_STARTER_BL);
    if(rc) return rc;
    free(starter);
    starter = NULL;

  case SYS_STAT_RESET:
    return SUCCEEDED;
  }
  ERROR_RETURN("starter finalization: unacceptable system state", sys_state);
}

/*---------------------------------------------------------------------------*/
int tofu_init(int rank)
{
  int rc;

  /** tofu initialization, num_procs is set in _acpblTofu_sys_init() **/
  rc = _acpblTofu_sys_init(rank);
  if(rc){ POINT(); return rc; }

  /** ga intialization, should be done berefore atkey and ga init. **/
  _acpblTofu_sys_ga_init();
  max_num_localtag = ga_mask_localtag >> ga_lsb_localtag;

  /** initialize atkey **/
  rc = _acpblTofu_atkey_init();
  if(rc){ POINT(); return rc; }

  /** command queue initialization **/
  rc = queue_init();
  if(rc){ POINT(); return rc; }

  /** delegation buffer initialization **/
  rc = _acpblTofu_delegation_init();
  if(rc){ POINT(); return rc; }

  /** starter memory initialization **/
  rc = starter_init();
  if(rc){ POINT(); return rc; }

  /** start commumication thread **/
  _acpblTofu_start_communication_thread();

  sleep(5);
  /** wait all ranks are ready **/
  return acp_sync();
}

int tofu_finalize()
{
  int rc;
  /** terminate communication thread **/
  rc = _acpblTofu_stop_communication_thread();
  if(rc) return rc;

  /** starter memory free **/
  rc = starter_free();
  if(rc) return rc;

  /** queue free **/
  rc = queue_free();
  if(rc) return rc;

  /** atkey free **/
  rc = _acpblTofu_atkey_free();
  if(rc) return rc;

  /** tofu finalize **/
  return _acpblTofu_sys_finalize();
}


/*---------------------------------------------------------------------------*/
/*** command queue functions *************************************************/
/*---------------------------------------------------------------------------*/
static inline int complete_check(uint32_t index)
{
  if((cqcp < cqwp  && (index <= cqcp || cqwp < index)) ||
     (cqwp <= cqcp && (cqwp < index  && index <= cqcp))){
    dprintf("complete_check@rank%d: CMPL: cqcp=%08x, cqwp=%08x, index=%08x\n", myrank_sys, cqcp, cqwp, index);
    return 0;	/* complete */
  }else{
    return 1;	/* not complete */
  }
}

int _acpblTofu_complete_check(uint32_t index) { return complete_check(index); }

static inline int cq_lock(void)
{
  uint32_t wp;
#if THREAD_SAFE
  while (sync_val_compare_and_swap_4(&cqlk, 0, 1)) ;
#endif
  wp = cqwp;
  return (int)(wp & CQ_MASK);
}

static inline acp_handle_t cq_unlock(void)
{ 
  acp_handle_t wp;
  wp = (acp_handle_t)cqwp++;
  while ((cqcp & CQ_MASK) == (cqwp & CQ_MASK));
#if THREAD_SAFE
  sync_synchronize();
#endif
  cqlk = 0;
  return wp;
}

static inline void set_props_order(acp_handle_t handle, 
				   volatile uint16_t *props, volatile uint32_t *order)
{
  /* called only under cq_locked */
  if(handle == ACP_HANDLE_ALL){
    *props = CMD_PROP_HANDLE_ALL;
    *order = cqwp - 1;
  } else if(handle == ACP_HANDLE_CONT){
    *props = CMD_PROP_HANDLE_CONT;
    *order = cqwp - 1;
  } else if(handle == ACP_HANDLE_NULL || handle >= 0x100000000LL){
    *props = CMD_PROP_HANDLE_NULL;
    *order = 0;
  } else {			/* a handle specified as an order */
    *props = CMD_PROP_HANDLE_ALL;
    *order = (uint32_t)handle;
  }
}

static inline acp_handle_t cq_enqueue_fence(acp_ga_t dst)
{
  int p = cq_lock();
  cq[p].fence.props             = CMD_PROP_HANDLE_NULL;	/* ACP_HANDLE_NULL */
  cq[p].fence.order		= 0;
  cq[p].fence.base.jobid	= jobid;
  cq[p].fence.base.cmd		= CMD_FENCE;
  cq[p].fence.base.run_stat	= CMD_STAT_QUEUED;
  cq[p].fence.base.comm_id	= p;
  cq[p].fence.ga_dst		= dst;
  return cq_unlock();
}

static inline acp_handle_t cq_enqueue_noarg(int cmd, acp_handle_t order)
{
  int p = cq_lock();
  set_props_order(order, &cq[p].noarg.props, &cq[p].noarg.order);
  dprintf("cq_enqueue_noarg@rank%d: props=0x%04x, order=0x%08x\n", myrank_sys, cq[p].noarg.props, cq[p].noarg.order); // debug
  cq[p].noarg.base.jobid	= jobid;
  cq[p].noarg.base.cmd		= cmd;
  cq[p].noarg.base.run_stat	= CMD_STAT_QUEUED;
  cq[p].noarg.base.comm_id	= p;
  return cq_unlock();
}

static inline acp_handle_t cq_enqueue_newrank(uint32_t value)
{
  int p = cq_lock();
  cq[p].newrank.props		= CMD_PROP_HANDLE_ALL;	/* ACP_HANDLE_ALL */
  cq[p].newrank.order		= 0;
  cq[p].newrank.base.jobid	= jobid;
  cq[p].newrank.base.cmd	= CMD_NEW_RANK;
  cq[p].newrank.base.run_stat	= CMD_STAT_QUEUED;
  cq[p].newrank.base.comm_id	= p;
  cq[p].newrank.value		= value;
  cq[p].newrank.count		= num_procs;
  return cq_unlock();
}

static inline acp_handle_t cq_enqueue_copy(acp_ga_t dst, acp_ga_t src, 
					   size_t size, acp_handle_t order)
{
  uint16_t props;
  int p = cq_lock();
  int rc;

  set_props_order(order, &cq[p].copy.props, &cq[p].copy.order);
  cq[p].copy.base.jobid		= jobid;
  cq[p].copy.base.cmd		= CMD_LOCATION(CMD_COPY, dst, src);
  cq[p].copy.base.run_stat	= CMD_STAT_QUEUED;
  cq[p].copy.base.comm_id	= p;
  cq[p].copy.ga_dst		= dst;
  cq[p].copy.ga_src		= src;
  cq[p].copy.size		= size;

#if EMPTY_BYPASS
  if(cqrp == cqwp){
    props = cq[p].copy.props & 0xC000;
    if((props == CMD_PROP_HANDLE_NULL) || (props == CMD_PROP_HANDLE_CONT)){
      cq[p].copy.base.run_stat = CMD_STAT_ORDER_END;
      rc = _acpblTofu_copy(&cq[p], cq[p].copy.base.comm_id);
      if(rc == CMD_STAT_BUSY)
	goto out;
      cq[p].copy.base.run_stat = rc;
    }
  }
 out:
#endif
  return cq_unlock();
}

static inline acp_handle_t cq_enqueue_cas4(acp_ga_t dst, acp_ga_t src, 
					   uint32_t oldval, uint32_t newval, 
					   acp_handle_t order)
{
  int p = cq_lock();
  set_props_order(order, &cq[p].cas4.props, &cq[p].cas4.order);
  cq[p].cas4.base.jobid		= jobid;
  cq[p].cas4.base.cmd		= CMD_LOCATION(CMD_CAS4, dst, src);
  cq[p].cas4.base.run_stat	= CMD_STAT_QUEUED;
  cq[p].cas4.base.comm_id	= p;
  cq[p].cas4.ga_dst		= dst;
  cq[p].cas4.ga_src		= src;
  cq[p].cas4.oldval		= oldval;
  cq[p].cas4.newval		= newval;

  return cq_unlock();
}

static inline acp_handle_t cq_enqueue_cas8(acp_ga_t dst, acp_ga_t src, 
					   uint64_t oldval, uint64_t newval, 
					   acp_handle_t order)
{
  int p = cq_lock();
  set_props_order(order, &cq[p].cas8.props, &cq[p].cas8.order);
  cq[p].cas8.base.jobid		= jobid;
  cq[p].cas8.base.cmd		= CMD_LOCATION(CMD_CAS8, dst, src);
  cq[p].cas8.base.run_stat	= CMD_STAT_QUEUED;
  cq[p].cas8.base.comm_id	= p;
  cq[p].cas8.ga_dst		= dst;
  cq[p].cas8.ga_src		= src;
  cq[p].cas8.oldval		= oldval;
  cq[p].cas8.newval		= newval;
  return cq_unlock();
}

static inline acp_handle_t cq_enqueue_atomic4(int cmd, acp_ga_t dst, 
					      acp_ga_t src, uint32_t value, 
					      acp_handle_t order)
{
  uint16_t props;
  int rc;
  volatile cq_t *command;  
  int p = cq_lock();
  set_props_order(order, &cq[p].atomic4.props, &cq[p].atomic4.order);

#if 0
  cq[p].atomic4.base.jobid	= jobid;
  cq[p].atomic4.base.cmd	= CMD_LOCATION(cmd, dst, src);
  cq[p].atomic4.base.run_stat	= CMD_STAT_QUEUED;
  cq[p].atomic4.base.comm_id	= p;
  cq[p].atomic4.ga_dst		= dst;
  cq[p].atomic4.ga_src		= src;
  cq[p].atomic4.value		= value;
#if EMPTY_BYPASS
  if(cqrp == cqwp){
    props = cq[p].copy.props & 0xC000;
    if((props == CMD_PROP_HANDLE_NULL) || (props == CMD_PROP_HANDLE_CONT)){
      cq[p].copy.base.run_stat = CMD_STAT_ORDER_END;
      rc = _acpblTofu_atomic(&cq[p], cq[p].copy.base.comm_id);
      if(rc == CMD_STAT_BUSY)
	goto out;
      cq[p].atomic4.base.run_stat = rc;
    }
  }
 out:
#endif

#else
  command = &cq[p];
  command->atomic4.base.jobid   	= jobid;
  command->atomic4.base.cmd     	= CMD_LOCATION(cmd, dst, src);
  command->atomic4.base.run_stat	= CMD_STAT_QUEUED;
  command->atomic4.base.comm_id 	= p;
  command->atomic4.ga_dst		= dst;
  command->atomic4.ga_src		= src;
  command->atomic4.value		= value;
#if EMPTY_BYPASS
  if(cqrp == cqwp){
    props = command->copy.props & 0xC000;
    if((props == CMD_PROP_HANDLE_NULL) || (props == CMD_PROP_HANDLE_CONT)){
      command->copy.base.run_stat = CMD_STAT_ORDER_END;
      rc = _acpblTofu_atomic(command, command->copy.base.comm_id);
      if(rc == CMD_STAT_BUSY)
	goto out;
      command->atomic4.base.run_stat = rc;
    }
  }
 out:
#endif
#endif

  return cq_unlock();
}

static inline acp_handle_t cq_enqueue_atomic8(int cmd, acp_ga_t dst,
					      acp_ga_t src, uint64_t value, 
					      acp_handle_t order)
{
  int p = cq_lock();
  set_props_order(order, &cq[p].atomic8.props, &cq[p].atomic8.order);
  cq[p].atomic8.base.jobid	= jobid;
  cq[p].atomic8.base.cmd	= CMD_LOCATION(cmd, dst, src);
  cq[p].atomic8.base.run_stat	= CMD_STAT_QUEUED;
  cq[p].atomic8.base.comm_id	= p;
  cq[p].atomic8.ga_dst		= dst;
  cq[p].atomic8.ga_src		= src;
  cq[p].atomic8.value		= value;
  return cq_unlock();
}


/*---------------------------------------------------------------------------*/
/*** acp infrastructure functions ********************************************/
/*---------------------------------------------------------------------------*/
int acp_init(int* argc, char*** argv)
{
  int rc;

  if (*argc < 8) return -1;

  /*** acpbl argments ***
   (*argv)[1] = my rank number (myrank)
   (*argv)[2] = number of prodcesses (num_procs)
   (*argv)[3] = task id (jobid)
   (*argv)[4] = starter memory size (starter_size)
   (*argv)[5] = my port
   (*argv)[6] = parent port
   (*argv)[7] = parent address

   only starter memory size is effective in this tofu implementation.
   myrank, num_procs, and jobid are obtained from MP/FX system software.
   my port number, parent port, and parent address are ignored.
  */

  starter_size = strtol((*argv)[4], NULL, 0);
  (*argv)[7] = (*argv)[0];
  *argc -= 7;
  *argv += 7;

  if(starter_size < STARTER_SIZE_DEFAULT)
    starter_size = STARTER_SIZE_DEFAULT;

  myrank = myrank_sys = _acpblTofu_sys_get_rank();	/* obtain system assigned rank */

  /*** first initialization ***/
  sys_state = SYS_STAT_INITIALIZE;
  rc = tofu_init(myrank_sys);		/* ACPbl/Tofu initialization */
  if(rc){
    dprintf("acp_init: rc = %d\n", rc);
    return rc;
  }

  /*** Initialize Middle Layer ***/
  if (iacp_init_dl()) return -1;
  if (iacp_init_cl()) return -1;
  
  sys_state = SYS_STAT_RUN;

  return SUCCEEDED;
}

int acp_finalize(void)
{
  int rc;

  dprintf("cqwp: 0x%08x, cqrp: 0x%08x, cqcp: 0x%08x\n",
	  cqwp, cqrp, cqcp); fflush(stdout); // debug

  /*** finalize upper layer modules ***/
  iacp_finalize_cl();
  iacp_finalize_dl();
  
  /*** complete and sync all nodes befor reset ***/
  acp_complete(ACP_HANDLE_ALL);
  acp_sync();

  int i;

  /*** finalize mode finalization ***/
  sys_state = SYS_STAT_FINALIZE;
  rc = tofu_finalize();
  sys_state = SYS_STAT_FREE;
  return rc;
}

int acp_reset(int rank)
{
  int i, rc, cnt;

  /*** rank range check ***/
  if(rank >= num_procs || rank < 0)
    ERROR_RETURN("specified rank number exceeds number of processes", rank);

  /*** notify new rank to delegation buffer of all nodes ***/
  acp_complete(cq_enqueue_newrank(rank));
  acp_sync();

  /** every node checks rank duplication **/
  for(cnt=0,i=0; i<num_procs; i++)
    if(delegation_buff[i].command.newrank.value == rank)
      cnt++;
  if(cnt != 1)
    ERROR_RETURN("duplicated rank number specified", rank);
  acp_sync();

  /*** set user defined rank into rank map table ***/
  for(i=0; i<num_procs; i++)
    rank_us_map[delegation_buff[i].command.newrank.value] = i;

  /*** reset mode finalization ***/
  sys_state = SYS_STAT_RESET;
  rc = tofu_finalize();
  if(rc) return rc;

  /*** reset mode initialization ***/
  rc = tofu_init(rank);
  if(rc) return rc;
  sys_state = SYS_STAT_RUN;
  myrank = rank;
  return SUCCEEDED;
}

void acp_abort(const char* str)
{
  /*** abort upper layer modules ***/
  iacp_abort_cl();
  iacp_abort_dl();

  /*** finalize mode finalization ***/
  sys_state = SYS_STAT_FINALIZE;
  tofu_finalize();

  return;
}

int acp_sync(void)
{
  dprintf("acp_sync@rank%d: in\n", myrank_sys);
  acp_complete(cq_enqueue_noarg(CMD_SYNC, ACP_HANDLE_ALL));
  dprintf("acp_sync@rank%d: complete\n", myrank_sys);
  return _acpblTofu_sys_barrier();
}

int acp_rank(void)
{ return myrank; }

int acp_procs(void)
{
  if(num_procs > 0)
    return num_procs;
  else
    return -1;
}


/*---------------------------------------------------------------------------*/
/*** acp global memory management functions **********************************/
/*---------------------------------------------------------------------------*/
acp_ga_t acp_query_starter_ga(int rank)
{ return _acpblTofu_gen_atkey(rank_us_map[rank], COLOR_STARTER, LOCALTAG_STARTER_BL); }

acp_ga_t iacp_query_starter_ga_dl(int rank)
{ return _acpblTofu_gen_atkey(rank_us_map[rank], COLOR_STARTER, LOCALTAG_STARTER_DL); }

acp_ga_t iacp_query_starter_ga_cl(int rank)
{ return _acpblTofu_gen_atkey(rank_us_map[rank], COLOR_STARTER, LOCALTAG_STARTER_CL); }

acp_atkey_t acp_register_memory(void* addr, size_t size, int color)
{
  int localtag;
  int head_padding = (uintptr_t)addr & 255;
  int tail_padding = (~((uintptr_t)addr + size) + 1) & 255;
  addr -= head_padding;
  size += head_padding + tail_padding;

  // !! should be thread safed
  localtag = _acpblTofu_register_memory(addr, size, color, NON, MEMTYPE_USER);

  dprintf("acp_register_memory@rank%d: addr=%p, size=%zd, color=%d, localtag=%d\n", myrank_sys, addr, size, color, localtag);

  return _acpblTofu_gen_atkey(myrank_sys, color, localtag);
}

int acp_unregister_memory(acp_atkey_t atkey)
{
  int localtag, rc;

  // !! should be thread safed
  localtag = ATKEY2LOCALTAG(atkey);
  if(localtag_table[localtag] == NULL)
    return -1;			/* no such an atkey */
  rc = _acpblTofu_unregister_memory(localtag);
  if(rc >= 0)
    return SUCCEEDED;
  else
    return FAILED;
}

acp_ga_t acp_query_ga(acp_atkey_t atkey, void* addr)
{
  int	   localtag;

  last_registered_localtag = NON;
  localtag = ATKEY2LOCALTAG(atkey);
  dprintf("acp_query_ga@rank%d: localtag=%d\n", myrank_sys, localtag);

  if(localtag_table[localtag] == NULL){
    dprintf("acp_query_ga@rank%d: GANULL(0), atkey=%u, addr=%p\n", myrank_sys, atkey, addr);
    return ACP_GA_NULL;		/* no such an atkey */
  }
  if((addr < localtag_table[localtag]->addr_head) || 
     ((localtag_table[localtag]->addr_tail) < addr)){
    dprintf("acp_query_ga@rank%d: GANULL(1), atkey=%u, addr=%p\n", myrank_sys, atkey, addr);
    return ACP_GA_NULL;		/* out of addr range */
  }
  if(localtag_table[localtag]->status == NOT_ENABLED){
    if(_acpblTofu_enable_localtag(localtag)){
      dprintf("acp_query_ga@rank%d: GANULL(2), atkey=%u, addr=%p\n", myrank_sys, atkey, addr);
      return ACP_GA_NULL;	/* ragister to device failed */
    }
  }
  return (atkey + ((char *)addr - 
		   (char *)localtag_table[localtag]->addr_head));
}

void* acp_query_address(acp_ga_t ga)
{
  if(GA2RANK(ga) != myrank_sys)
    return NULL;
  if(localtag_table[GA2LOCALTAG(ga)] == NULL)
    return NULL;
  return (void *)((char *)localtag_table[GA2LOCALTAG(ga)]->addr_head + 
		  (ga & ga_mask_offset));
}

int acp_query_rank(acp_ga_t ga)
{ 
  int i, rank;

  if(ga == ACP_GA_NULL) //!!! need debug
    return -1;

  rank = GA2RANK(ga);
  for(i=0; i<num_procs; i++)
    if(rank_us_map[i] == rank)
      break;
  return i;
}

int acp_query_color(acp_ga_t ga)
{
  if(ga == ACP_GA_NULL)
    return -1;
  return GA2COLOR(ga);
}

int acp_colors()
{ return _acpblTofu_sys_num_colors(); }


/*---------------------------------------------------------------------------*/
/*** acp global memory access functions **************************************/
/*---------------------------------------------------------------------------*/
acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, size_t size, 
		      acp_handle_t order)
{
  acp_handle_t handle;
  handle = cq_enqueue_copy(dst, src, size, order);

  DUMP_CQXP(); // debug
//   if(cq[handle].copy.base.run_stat == CMD_STAT_EXED_AT_MAIN){
//     tofu_lock();
//     cq[handle].copy.base.run_stat = 
//       _acpblTofu_copy(&cq[handle], cq[handle].copy.base.comm_id) | 0x10;
//     tofu_unlock();
  
//     /*** device status ***/
//     int rc;
//     tofu_trans_stat_save_flag = 0;
//     rc = _acpblTofu_sys_read_status(&tofu_trans_stat_save);	/* tofu status */
//     if(rc){						/* something received */
//       if(tofu_trans_stat_save.status != TOFU_SYS_SUCCESS)
// 	_acpblTofu_die("tofu_sys_command failed", tofu_trans_stat_save.status);
//       if(rc == TOFU_SYS_STAT_TRANSEND){			/* initiated transfer finished */
//     	if(h == tofu_trans_stat_save.comm_id){
//     	  cq[h].copy.base.run_stat = (uint8_t)CMD_STAT_FINISHED;
//     	  cq_next();
//     	  cq_dequeue();
//     	}
//       } else {
//     	tofu_trans_stat_save_flag = 1;
//       }
//     }
//   }
  return handle;
}

acp_handle_t acp_cas4(acp_ga_t dst, acp_ga_t src, 
		      uint32_t oldval, uint32_t newval, acp_handle_t order)
{ return cq_enqueue_cas4(dst, src, oldval, newval, order); }

acp_handle_t acp_cas8(acp_ga_t dst, acp_ga_t src, 
		      uint64_t oldval, uint64_t newval, acp_handle_t order)
{ return cq_enqueue_cas8(dst, src, oldval, newval, order); }

acp_handle_t acp_swap4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{ return cq_enqueue_atomic4(CMD_SWAP4, dst, src, value, order); }

acp_handle_t acp_swap8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{ return cq_enqueue_atomic8(CMD_SWAP8, dst, src, value, order); }

acp_handle_t acp_add4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{ return cq_enqueue_atomic4(CMD_ADD4, dst, src, value, order); }

acp_handle_t acp_add8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{ return cq_enqueue_atomic8(CMD_ADD8, dst, src, value, order); }

acp_handle_t acp_xor4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{ return cq_enqueue_atomic4(CMD_XOR4, dst, src, value, order); }

acp_handle_t acp_xor8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{ return cq_enqueue_atomic8(CMD_XOR8, dst, src, value, order); }

acp_handle_t acp_or4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{ return cq_enqueue_atomic4(CMD_OR4, dst, src, value, order); }

acp_handle_t acp_or8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{ return cq_enqueue_atomic8(CMD_OR8, dst, src, value, order); }

acp_handle_t acp_and4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order)
{ return cq_enqueue_atomic4(CMD_AND4, dst, src, value, order); }

acp_handle_t acp_and8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order)
{ return cq_enqueue_atomic4(CMD_AND8, dst, src, value, order); }

void acp_complete(acp_handle_t handle)
{
  //  uint64_t index;
  //  uint32_t index, ix;

  //??? should be tread safe

  //  if(handle == ACP_HANDLE_NULL || handle >= 0x100000000LL){
  //    return;
  //  } else if(handle == ACP_HANDLE_ALL || handle == ACP_HANDLE_CONT){
  //    index = cqwp - 1;
  //    printf("index: 0x%08x, ", index); // debug
  //    DUMP_CQXP(); // debug
  //  } else {				/* handle as an order specified */
  //    index = (uint32_t)handle;
  //    if(complete_check(index) == 0)
  //      return;				/* out of cq range */
  //    }
  //  }

  //  ix = index & CQ_MASK;
  //  if((cq[ix].copy.base.cmd & 0xfc == CMD_COPY) &&
  //     (GA2RANK(cq[ix].copy.ga_dst) != myrank_sys) &&
  //     (GA2RANK(cq[ix].copy.ga_src) == myrank_sys)){
  //    /* local to remote copy: put */
  //    index = (uint32_t)cq_enqueue_fence(GA2RANK(cq[index].copy.ga_dst));
  //  }	      

  uint32_t index;

  if(handle == ACP_HANDLE_NULL || handle >= 0x100000000LL){
    return;
  } else if(handle != ACP_HANDLE_ALL && handle != ACP_HANDLE_CONT){
    if(complete_check((uint32_t)handle) == 0)
      return;
  }
  index = (uint32_t)cq_enqueue_noarg(CMD_COMPLETE, ACP_HANDLE_ALL);
  dprintf("acp_complete@rank%d: index=0x%08x\n", myrank_sys, index); //fflush(stdout); // debug

  //  ix = index & CQ_MASK;
  //  if((cq[ix].copy.base.cmd & 0xfc == CMD_COPY) &&
  //     (GA2RANK(cq[ix].copy.ga_dst) != myrank_sys) &&
  //     (GA2RANK(cq[ix].copy.ga_src) == myrank_sys)){
  //    /* local to remote copy: put */
  //    index = (uint32_t)cq_enqueue_fence(GA2RANK(cq[index].copy.ga_dst));
  //  }	      


  POINT(); // debug
  //if((cqcp < cqwp  && (index <= cqcp || cqwp < index)) ||
  //   (cqwp <= cqcp && (cqwp < index  && index <= cqcp)))
  //previous_tick = get_dtime();
  while(complete_check(index));
  dprintf("acp_complete@rank%d: complete_check=OK, index=0x%08x\n", myrank_sys, index); //fflush(stdout); // debug
  POINT(); // debug
  return;
}

int acp_inquire(acp_handle_t handle)
{
  /* cq range check */
  if((uint32_t)handle <= cqwp){
    if(((uint32_t)handle+CQ_DEPTH > cqwp) || 
       ((uint32_t)handle+CQ_DEPTH <= CQ_DEPTH))
      return complete_check((uint32_t)handle);
  }
  return 0;
}


/*---------------------------------------------------------------------------*/
/*** abort *******************************************************************/
/*---------------------------------------------------------------------------*/
char abort_str[256];
void dassertp_fail(const char *cond_string, const char *file, 
		  const char *func, unsigned int line) {
  if(!in_error_cleanup) {
    in_error_cleanup=1;
    sprintf(abort_str,
	    "(rank:%d pid:%d):acpbl_tofu DASSERT fail. %s:%s():%d cond:%s\n",
	    myrank_sys, getpid(), file, func,line, cond_string);
    printf("%s\n", abort_str);
    fflush(stdout);
  }
  exit(1);
}
