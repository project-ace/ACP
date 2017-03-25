/*****************************************************************************/
/***** ACP Basic Layer / Tofu					         *****/
/*****   commumication thread						 *****/
/*****									 *****/
/***** Copyright FUJITSU LIMITED 2014					 *****/
/*****									 *****/
/***** Specification Version: ACP-140312				 *****/
/***** Version: 0.1							 *****/
/***** Module Version: 0.1						 *****/
/*****									 *****/
/***** Note:								 *****/
/*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "acp.h"
#include "acpbl.h"
#include "acpbl_sync.h"
#include "acpbl_tofu.h"
#include "acpbl_tofu_sys.h"

/*---------------------------------------------------------------------------*/
/*** external functions ******************************************************/
/*---------------------------------------------------------------------------*/
extern int _acpblTofu_copy(cq_t* command, int id);
extern int _acpblTofu_atomic(cq_t* command, int id);
extern int _acpblTofu_enable_localtag(int localtag);
extern int _acpblTofu_reply_delegation_end(cq_t *command);
//extern int _acpblTofu_fence(cq_t *command);
extern int _acpblTocu_complete_check(uint64_t index);

/*---------------------------------------------------------------------------*/
/*** variables ***************************************************************/
/*---------------------------------------------------------------------------*/
extern int	myrank_sys;
extern volatile cq_t *cq;				/* command queue */
extern volatile uint32_t cqwp, cqrp, cqcp, cqlk;	/* cq pointers */
extern volatile dq_t *dq;				/* delegation queue */
extern volatile uint32_t dqwp, dqrp, dqcp, dqlk;	/* dq pointers */
extern uint64_t ga_mask_color, ga_mask_rank, ga_mask_localtag, ga_mask_offset;
extern uint64_t ga_lsb_color,  ga_lsb_rank,  ga_lsb_localtag,  ga_lsb_offset;
extern volatile delegation_buff_t *delegation_buff;	/* delegation buffer */
extern int	print_level;
extern uint64_t profile[];

static pthread_t comm_thread_id;
volatile int communication_thread_status;
int rank_dst_prev, rank_src_prev, color_dst_prev, color_src_prev;
tofu_trans_stat_t tofu_trans_stat_save;
int tofu_trans_stat_save_flag;


/*---------------------------------------------------------------------------*/
/*** command queue operations ************************************************/
/*---------------------------------------------------------------------------*/
static inline int cq_poll(void)
{ return cqrp < cqwp; }

static inline volatile cq_t* cq_read(void)
{ return cq + (cqrp & CQ_MASK); }

static inline void cq_next(void)
{
  if (cqrp < cqwp)
    cqrp++;
  return;
}

int debug_point = 0;
static inline void cq_dequeue(cq_t *command)
{
  //  uint32_t p, q;

  //  p = ((char *)command - (char *)cq) / sizeof(cq_t);
  //  q = (cqcp + 1) & CQ_MASK;
  //  printf("p: 0x%08x, q: 0x%08x, cqcp: 0x%08x, command: %p, cq: %p\n", p, q, cqcp, command, cq); fflush(stdout); // debug
  //  if(((cqcp + 1) & CQ_MASK) == p)
  //    cqcp++;
  //  while(((cqcp+1) != cqwp) && 
  //  	(cq[(cqcp+1) & CQ_MASK].copy.base.run_stat == CMD_STAT_FINISHED ||
  //	 cq[(cqcp+1) & CQ_MASK].copy.base.run_stat == CMD_STAT_FREE)){
  //    cq[(cqcp+1) & CQ_MASK].copy.base.run_stat = CMD_STAT_FREE;
  //    cqcp++;
  //    POINT();
  //  }
  cqcp++;
  return;
}

void _acpblTofu_cq_next()
{ cq_next(); }


/*---------------------------------------------------------------------------*/
/*** delegation queue operations *********************************************/
/*---------------------------------------------------------------------------*/
static inline int dq_lock()
{
  int32_t wp;
  //  while (!sync_val_compare_and_swap_8(&dqlk, 0, 1)) ;
  while (sync_val_compare_and_swap_4(&dqlk, 0, 1)) ;
  wp = dqwp;
  while (dqcp + DQ_DEPTH <= wp);
  return (int)(wp & (DQ_DEPTH - 1));
}

static inline int64_t dq_unlock()
{ 
  int64_t wp;
  wp = (int64_t)dqwp++;
  sync_synchronize();
  dqlk = 0;
  return (wp + COMM_DELEGATED_OP);	/* add delegated operation flag */
}

static inline void dq_dequeue()
{
  if(dqcp < dqwp)
    dqcp++;
  return;
}

static inline int dq_enqueue(cq_t *command)
{
  int p = dq_lock();
  dq[p].command			= command;
  dq[p].comm_id			= command->copy.base.comm_id;
  return dq_unlock();
}


/*---------------------------------------------------------------------------*/
/*** command operations ******************************************************/
/*---------------------------------------------------------------------------*/
static inline void execute_order(cq_t* command)
{
  int rank_dst, rank_src, color_dst, color_src;
  uint16_t props;

  props = command->copy.props & 0xC000;

  //printf("cmd: 0x%02x\n", command->noarg.base.cmd); fflush(stdout); // debug
  /*** check fence for specifed handle ***/
  if(command->noarg.base.cmd < CMD_COPY){	/* control command */
    rank_dst_prev = rank_src_prev = color_dst_prev = color_src_prev = -1;
  } else {					/* copy or atomic */
    rank_dst = GA2RANK(command->copy.ga_dst);	/* ga location extraction */
    rank_src = GA2RANK(command->copy.ga_src);

    if(props == CMD_PROP_HANDLE_CONT){		/* check GMA continuation */
      color_dst = GA2COLOR(command->copy.ga_dst);
      color_src = GA2COLOR(command->copy.ga_src);
      if((rank_dst != rank_dst_prev)    ||	/* same with previous GMA ? */
	 (rank_src != rank_src_prev)    ||
	 (color_dst != color_dst_prev)  ||
	 (color_src != color_src_prev))
	command->copy.props = (command->copy.props & 0x3FFF) + CMD_PROP_HANDLE_ALL;
    }
    rank_dst_prev = rank_dst;			/* save as previous values */
    rank_src_prev = rank_src;
    color_dst_prev = color_dst;
    color_src_prev = color_src;
  }

  props = command->copy.props & 0xC000;
  if((props == CMD_PROP_HANDLE_NULL) || (props == CMD_PROP_HANDLE_CONT))
    command->copy.base.run_stat = CMD_STAT_ORDER_END;
  else {
    if(props == CMD_PROP_HANDLE_ALL)
      command->copy.order = (command->copy.base.comm_id - 1) & CQ_MASK;
    command->copy.base.run_stat = CMD_STAT_ORDER;
  } // // is this ok ???
}

static inline int check_order_end(cq_t* command)
{
  /*** fence for specifed handle ***/
  if(command->copy.base.run_stat != CMD_STAT_ORDER)
    return 0;
  if(_acpblTofu_complete_check(command->copy.order) == 0){
    command->copy.base.run_stat = CMD_STAT_ORDER_END;
    return 0;
  } else {
    return 1;
  }
}

static inline void command_station(cq_t *command)
{
  int rc;

  DUMP_CQXP(); // debug

  switch(command->copy.base.run_stat){
  case CMD_STAT_QUEUED:
    DUMP_CQXP(); // debug
    execute_order(command);
  case CMD_STAT_ORDER:
    if(check_order_end(command))
      return;						/* wait sync end */
  case CMD_STAT_ORDER_END:
  case CMD_STAT_MULTI_EXEC:
    if(command->copy.base.cmd >= CMD_CAS4){		/* atomic command */
      rc = _acpblTofu_atomic(command, command->copy.base.comm_id);
      if(rc == CMD_STAT_BUSY)
	break;
      command->atomic4.base.run_stat = rc;
    } else if(command->copy.base.cmd >= CMD_COPY){	/* copy command */
      rc = _acpblTofu_copy(command, command->copy.base.comm_id);
      if(rc == CMD_STAT_BUSY)
	break;
      command->copy.base.run_stat = rc;
    }
    //else if(command->copy.base.cmd == CMD_FENCE)	/* fence command */
    //  command->noarg.base.run_stat = _acpblTofu_fence(command);
    else if(command->copy.base.cmd == CMD_SYNC)		/* sync command */
      command->noarg.base.run_stat = CMD_STAT_FINISHED;
    else if(command->copy.base.cmd == CMD_NEW_RANK)	/* fence command */
      command->noarg.base.run_stat = _acpblTofu_newrank(command);
    else if(command->copy.base.cmd == CMD_COMPLETE)	/* complete command */
      command->noarg.base.run_stat = CMD_STAT_FINISHED;

    /** queue pointer update **/
    if(command->copy.base.run_stat != CMD_STAT_MULTI_EXEC)
      cq_next();	/* read pointer */
    if(command->copy.base.run_stat == CMD_STAT_FINISHED)
      cq_dequeue(command);	/* completion pointer */
    break;
  case CMD_STAT_EXED_AT_MAIN:
    break;
  default:
    _acpblTofu_die("command station: unacceptable state", 
		   command->copy.base.run_stat);
    break;
  }
}


/*---------------------------------------------------------------------------*/
/*** tofu status operations **************************************************/
/*---------------------------------------------------------------------------*/
static inline void tofu_receive_delegation(tofu_trans_stat_t *tofu_trans_stat)
{
  /*** command received from another process ***/
  cq_t *command;
  int  rc, comm_id, id, sender;

  comm_id = tofu_trans_stat->comm_id;

  if((comm_id & COMM_REPLY) == 0){
    sender = (tofu_trans_stat->offset - 1) / sizeof(delegation_buff_t);
    command = (cq_t *)((char *)delegation_buff + sender * sizeof(delegation_buff_t));

    id = dq_enqueue(command);
    /** incomming command **/
    /* add location to command */
    command->copy.base.cmd += 
      ((GA2RANK(command->copy.ga_dst) == myrank_sys)? 2 : 0) + 
      ((GA2RANK(command->copy.ga_src) == myrank_sys)? 1 : 0);

    if(command->copy.base.cmd >= CMD_CAS4){		/* atomic command */
      rc = _acpblTofu_atomic(command, id);
    } else if(command->copy.base.cmd >= CMD_COPY){	/* copy command */
      rc = _acpblTofu_copy(command, id);
      if(rc == CMD_STAT_FINISHED)
	_acpblTofu_reply_delegation_end(command);
    } else {
      _acpblTofu_die("unknown command delegated", command->copy.base.cmd);
    }
  } else {
    /** delegated operation finished **/
    command = (cq_t *)&cq[comm_id & 0x3FFF];
    if(command->copy.props & 1)			/* delegation to 0: SRC, 1: DST */
      sender = GA2RANK(command->copy.ga_dst);
    else
      sender = GA2RANK(command->copy.ga_src);
    delegation_buff[sender].flag = 0;
    command->copy.base.run_stat = CMD_STAT_FINISHED;
    cq_dequeue(command);
  }
}

static inline void tofu_transfer_end(tofu_trans_stat_t *tofu_trans_stat)
{
  /*** initiated transfer finished (tofu buffer ready) ***/
  cq_t *command;
  int  id;

  /*** id format *****************************/
  /* bit0-11:  handle  			     */
  /* bit12-13: reserve 			     */
  /* bit14:    0: command, 1: delegated copy */
  /* bit15:    delegation reply		     */
  /*******************************************/

  id = tofu_trans_stat->comm_id;

  if((id & COMM_REPLY) == 0){
    /** transfer end **/
    if((id & COMM_DELEGATED_OP) == 0){
      /** command queue **/
      command = (cq_t *)&(cq[id]);
      if(command->copy.base.run_stat == CMD_STAT_EXECUTING){
	command->copy.base.run_stat = (uint8_t)CMD_STAT_FINISHED;
	cq_dequeue(command);
      } else if(command->copy.base.run_stat == CMD_STAT_EXED_AT_MAIN){
	command->copy.base.run_stat = (uint8_t)CMD_STAT_FINISHED;
	cq_next();
	cq_dequeue(command);
      }
    } else {
      //      printf("id: 0x%04x\n", id); fflush(stdout); // debug
      /** delagation queue **/
      command = (cq_t *)dq[id&0x3FF].command;
      _acpblTofu_reply_delegation_end(command);	/* reply delegation end */
    }
  } else {
    /** delagatin reply end **/
    dq_dequeue();
  }
}


/*---------------------------------------------------------------------------*/
/*** communication thread ****************************************************/
/*---------------------------------------------------------------------------*/
static void* communication_thread(void *param)
{
  int         rc;
  static tofu_trans_stat_t tofu_trans_stat;	/* tofu transfer status */
  volatile cq_t *command;

  /*** thread initalization ***/
  sync_synchronize();
  communication_thread_status = 1;
  if(print_level & PRINT_INFO)
    printf("[info: %d] communication thread started\n", myrank_sys);

  /*** communication thread loop ***/
  do{
    /*** command queue ***/
    if(cq_poll()) {
      command = cq_read();		/* command from local */
      command_station((cq_t *)command);
    }

    /*** device status ***/
    if(tofu_trans_stat_save_flag)
      tofu_trans_stat = tofu_trans_stat_save;

    rc = _acpblTofu_sys_read_status(&tofu_trans_stat.comm_id, &tofu_trans_stat.offset); /* tofu status */

    if(tofu_trans_stat_save_flag || rc){
      //    if(rc){					/* something received */
//      if(tofu_trans_stat.status != TOFU_SYS_SUCCEEDED)
//	_acpblTofu_die("tofu_sys_command failed", tofu_trans_stat.status);

      //      printf("trans_stat: status = %s, comm_id = 0x%04x, localtag %d, offset = 0x%016lx\n",
      //	     tofu_trans_stat.status, tofu_trans_stat.comm_id,
      //	     tofu_trans_stat.localtag, tofu_trans_stat.offset); fflush(stdout); // debug

      if(rc == TOFU_SYS_STAT_TRANSEND){		/* initiated trans. finished */
	tofu_transfer_end(&tofu_trans_stat);
      } else if(rc == TOFU_SYS_STAT_RECEIVED){	/* delegation received */
	tofu_receive_delegation(&tofu_trans_stat);
      }
    }
  } while(communication_thread_status == 1);

  /*** thead finalization ***/
  pthread_exit(NULL);
  communication_thread_status = 0;
  return NULL;
}

void _acpblTofu_start_communication_thread()
{
  rank_dst_prev = rank_src_prev = color_dst_prev = color_src_prev = -1;
  communication_thread_status = 0;
  pthread_create(&comm_thread_id, NULL, communication_thread, NULL);
  while(!communication_thread_status);
}

int _acpblTofu_stop_communication_thread()
{
  int rc;

  communication_thread_status = 2;
  while(!communication_thread_status);

  rc = pthread_join(comm_thread_id, NULL);
  if(print_level & PRINT_INFO) printf("[info: %d] communication thread stoped\n",
				      myrank_sys);
  return rc;
}
