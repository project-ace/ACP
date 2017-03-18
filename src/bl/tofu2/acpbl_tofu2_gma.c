/*****************************************************************************/
/***** ACP Basic Layer / Tofu					         *****/
/*****   gma operations							 *****/
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
#include <stdint.h>
#include <string.h>
#include "acp.h"
#include "acpbl.h"
#include "acpbl_sync.h"
#include "acpbl_tofu2.h"

/*---------------------------------------------------------------------------*/
/*** external functions ******************************************************/
/*---------------------------------------------------------------------------*/
extern int _acpblTofu_register_memory(void *addr, acp_size_t size, int color, 
				      int localtag, int type);
extern int _acpblTofu_enable_localtag(int localtag);
extern acp_atkey_t _acpblTofu_gen_atkey(int rank, int color, int localtag);
extern int _acpblTofu_sys_armw_data(int flags, acp_ga_t remote, acp_ga_t local, 
				    int cmd, uint64_t write_val, uint64_t comp_val,
				    int length, int id);
extern int _acpblTofu_sys_get_data(int flags, acp_ga_t remote, acp_ga_t local, 
				   int length, int id);
extern int _acpblTofu_sys_put_data(int flags, acp_ga_t remote, acp_ga_t local, 
				   int length, int id);
extern int _acpblTofu_sys_put_data_imd(int flags, acp_ga_t remote, void *local, 
				       int length, int id);
extern int _acpblTofu_sys_put_data_imd2(int flags0, acp_ga_t remote0, 
					void *local0, int length0, int id0,
					int flags1, acp_ga_t remote1, 
					void *local1, int length1, int id1);


/*---------------------------------------------------------------------------*/
/*** variables ***************************************************************/
/*---------------------------------------------------------------------------*/
extern int	sys_state;
extern int	myrank_sys;
extern int	num_procs;
extern uint64_t ga_lsb_color,  ga_lsb_localtag,  ga_lsb_rank,  ga_lsb_offset;
extern uint64_t ga_mask_color, ga_mask_localtag, ga_mask_rank, ga_mask_offset;
extern uint64_t profile[];

volatile delegation_buff_t *delegation_buff = NULL;	/* delegation buffer */

/*---------------------------------------------------------------------------*/
/*** initialization **********************************************************/
/*---------------------------------------------------------------------------*/
int _acpblTofu_delegation_init()
{
  int i, rc;

  if(sys_state == SYS_STAT_INITIALIZE){
    /* allocate and register delegation buffer memory */
    if(delegation_buff == NULL){
      rc = posix_memalign((void **)&delegation_buff, 256, 
			  sizeof(delegation_buff_t)*num_procs);
      if(rc) ERROR_RETURN("delegation_buff: posix_memalign failed", rc);
      rc = _acpblTofu_register_memory((void*)delegation_buff, 
				      sizeof(delegation_buff_t)*num_procs, 
				      COLOR_DLG_BUF, LOCALTAG_DLG_BUFF, 
				      MEMTYPE_STARTER);
      if(rc == FAILED) return rc;
    }
    rc = _acpblTofu_enable_localtag(LOCALTAG_DLG_BUFF);
    if(rc) return rc;
  }

  /* initialize delegation buffer */
  for(i=0; i<num_procs; i++){
    delegation_buff[i].flag = 0;
    delegation_buff[i].end_status = 0;
    delegation_buff[i].command.copy.base.cmd = 0;
  }
  return SUCCEEDED;
}


/*---------------------------------------------------------------------------*/
/*** delegation **************************************************************/
/*---------------------------------------------------------------------------*/
int delegate_command(cq_t *command, int target)
{
  int      rank;
  acp_ga_t ga_cq, ga_target;

  command->copy.base.cmd &= 0xFC;	/* remove location */

  if(target == DST){
    rank = GA2RANK(command->copy.ga_dst);
    command->copy.props |= 1;
  } else {
    rank = GA2RANK(command->copy.ga_src);
    command->copy.props &= 0xFFFFFFFFE;
  }

  if(sync_val_compare_and_swap_4(&(delegation_buff[rank].flag), 0, 1))
    return FAILED;

  ga_target = _acpblTofu_gen_atkey(rank, COLOR_DLG_BUF, LOCALTAG_DLG_BUFF) + 
    (uint64_t)(myrank_sys * sizeof(delegation_buff_t));
  ga_cq     = _acpblTofu_gen_atkey(myrank_sys, COLOR_CQ, LOCALTAG_CQ) + 
    (uint64_t)(command->copy.base.comm_id * sizeof(cq_t)); /* comm_id = #cq entry */

  //  printf("delegation target: cqga = 0x%016lx, ga = 0x%016lx, ga_dst = 0x%016lx, ga_src = 0x%016lx\n",
  //  	 ga_cq, ga_target, command->copy.ga_dst, command->copy.ga_src); fflush(stdout); // debug

  _acpblTofu_sys_put_data(TOFU_SYS_FLAG_NOTIFY, ga_target, ga_cq, sizeof(cq_t), 
			  command->copy.base.comm_id);
  return SUCCEEDED;
}

int _acpblTofu_reply_delegation_end(cq_t *command)
{
  int	   target;
  acp_ga_t ga_target;

  //  command->copy.base.comm_id = delegation->comm_id;

  target = (int)(((char *)command - (char *)delegation_buff) / 
		 sizeof(delegation_buff_t));
  ga_target = _acpblTofu_gen_atkey(target, COLOR_CQ, LOCALTAG_CQ) + 
    (uint64_t)(command->copy.base.comm_id * sizeof(cq_t));

  return _acpblTofu_sys_put_data_imd(TOFU_SYS_FLAG_NOTIFY, ga_target, NULL, 0, 
				     command->copy.base.comm_id + COMM_REPLY);
}

int reply_delegation_data_and_end(cq_t *command, void *addr, int size)
{
  int      target;
  acp_ga_t ga_target;

  //  command->copy.base.comm_id = delegation->comm_id;

  target = (int)(((char *)command - (char *)delegation_buff) / 
		 sizeof(delegation_buff_t));

  ga_target = _acpblTofu_gen_atkey(target, COLOR_CQ, LOCALTAG_CQ) + 
    (uint64_t)(command->copy.base.comm_id * sizeof(cq_t));

  if((command->copy.ga_dst & ga_mask_rank) == (ga_target & ga_mask_rank)){
    return _acpblTofu_sys_put_data_imd(TOFU_SYS_FLAG_NOTIFY, 
				       command->copy.ga_dst, addr, size, 
				       command->copy.base.comm_id + COMM_REPLY);
  } else
    return _acpblTofu_sys_put_data_imd2(0, command->copy.ga_dst, addr, size, 
					command->copy.base.comm_id + COMM_REPLY,
					TOFU_SYS_FLAG_NOTIFY, ga_target, NULL, 0, 
					command->copy.base.comm_id + COMM_REPLY);
}


/*---------------------------------------------------------------------------*/
/*** acpbl functions *********************************************************/
/*---------------------------------------------------------------------------*/
int _acpblTofu_copy(cq_t* command, int id)
{
  int  location, flag;
  void *dst, *src;

  location = (int)command->copy.base.cmd & 0x3;

  if(command->copy.order == ACP_HANDLE_CONT)
    flag = TOFU_SYS_FLAG_CONTINUE;
  else
    flag = 0;

  switch(location){
  case 2:			/*  remote to local copy */
    _acpblTofu_sys_get_data(flag, command->copy.ga_src, command->copy.ga_dst,
			    command->copy.size, id);
    return CMD_STAT_EXECUTING;
  case 1:			/*  local to remote copy */
    _acpblTofu_sys_put_data(flag, command->copy.ga_dst, command->copy.ga_src,
			    command->copy.size, id);
    return CMD_STAT_EXECUTING;
  case 3:			/*  local to local copy */
    dst = acp_query_address(command->copy.ga_dst);
    src = acp_query_address(command->copy.ga_src);
    memmove(dst, src, command->copy.size);
    return CMD_STAT_FINISHED;
  default:			/* remote to remote copy */
    if(delegate_command(command, DST))
      return CMD_STAT_BUSY;
    else
      return CMD_STAT_DELEGATED;
  }
}

int _acpblTofu_atomic(cq_t* command, int id)
{
  int location, flag;
#if USE_TOFU2_ATOMIC
  int cmd;
  uint64_t write_val, comp_val;
#endif

  if(command->copy.order == ACP_HANDLE_CONT)
    flag = TOFU_SYS_FLAG_CONTINUE;
  else
    flag = 0;

  location = (int)command->atomic8.base.cmd & 0x3;

#if USE_TOFU2_ATOMIC /* FX100 */
  switch(location){
  case 2:                       /*  modify remote value and store oldval to local */
    //とりあえず手抜き。共通のやりかたで実装可能。
    if(location == 2){
      dprintf("_acpblTofu_atomic@rank%d: modify remote value and store oldval to local\n", myrank_sys);

    }else{
      dprintf("_acpblTofu_atomic@rank%d: modify local value and store oldval to local\n", myrank_sys);
    }

    if (command->atomic8.base.cmd >= CMD_CAS8) { /* atomic8 */
      cmd = command->atomic8.base.cmd & 0xFC;
      dprintf("_acpblTofu_atomic@rank%d: id=%d, length=8, cmd=%d\n", myrank_sys, id, cmd);
      if(cmd == CMD_CAS8){
	write_val = command->cas8.newval;
	comp_val  = command->cas8.oldval;
	//abort(); /* CAS8はハードウェア実装できない！！！ */
      }else{
	write_val = command->atomic8.value;
	comp_val  = 0; /* ignored */
      }
      _acpblTofu_sys_armw_data(flag, command->copy.ga_src, command->copy.ga_dst,
			       cmd, write_val, comp_val, 8, id);
    } else { /* atomic4 */
      dprintf("_acpblTofu_atomic@rank%d: id=%d, length=4, cmd=%d\n", myrank_sys, id, cmd);
      cmd = command->atomic4.base.cmd & 0xFC;
      if(cmd == CMD_CAS4){
	write_val = command->cas4.newval;
	comp_val  = command->cas4.oldval;
      }else{
	write_val = command->atomic4.value;
	comp_val  = 0; /* ignored */
      }
      _acpblTofu_sys_armw_data(flag, command->copy.ga_src, command->copy.ga_dst,
			       cmd, write_val, comp_val, 4, id);
    }
    dprintf("_acpblTofu_atomic@rank%d: return\n", myrank_sys);
    return CMD_STAT_EXECUTING;

  case 3:                       /*  modify local value and store oldval to local */
    if(command->atomic8.base.cmd >= CMD_CAS8){	/* atomic8 */
      volatile uint64_t *ptr;
      uint64_t oldval;
      ptr = (volatile uint64_t*)acp_query_address(command->atomic8.ga_src);
      switch(command->atomic8.base.cmd & 0xFC){
      case CMD_CAS8:
	oldval = sync_val_compare_and_swap_8(ptr, command->cas8.oldval, 
					     command->cas8.newval);
	break;
      case CMD_SWAP8:
	oldval = sync_swap_8(ptr, command->atomic8.value);
	break;
      case CMD_ADD8:
	oldval = sync_fetch_and_add_8(ptr, command->atomic8.value);
	break;
      case CMD_XOR8:
	oldval = sync_fetch_and_xor_8(ptr, command->atomic8.value);
	break;
      case CMD_OR8:
	oldval = sync_fetch_and_or_8(ptr, command->atomic8.value);
	break;
      case CMD_AND8:
	oldval = sync_fetch_and_and_8(ptr, command->atomic8.value);
	break;
      }
      *((uint64_t *)acp_query_address(command->atomic8.ga_dst)) = oldval;
      if(command->atomic8.base.run_stat == CMD_STAT_DELEGATED)
	_acpblTofu_reply_delegation_end(command); /* reply delegation fin. */
      return CMD_STAT_FINISHED;
    } else {
      volatile uint32_t *ptr;
      uint32_t oldval;
      ptr = acp_query_address(command->atomic4.ga_src);
      switch(command->atomic4.base.cmd & 0xFC){
      case CMD_CAS4:
	oldval = sync_val_compare_and_swap_4(ptr, command->cas4.oldval,
					     command->cas4.newval);
	break;
      case CMD_SWAP4:
	oldval = sync_swap_4(ptr, command->atomic4.value);
	break;
      case CMD_ADD4:
	oldval = sync_fetch_and_add_4(ptr, command->atomic4.value);
	break;
      case CMD_XOR4:
	oldval = sync_fetch_and_xor_4(ptr, command->atomic4.value);
	break;
      case CMD_OR4:
	oldval = sync_fetch_and_or_4(ptr, command->atomic4.value);
	break;
      case CMD_AND4:
	oldval = sync_fetch_and_and_4(ptr, command->atomic4.value);
	break;
      }

      *((uint32_t *)acp_query_address(command->atomic4.ga_dst)) = oldval;
      if(command->atomic4.base.run_stat == CMD_STAT_DELEGATED)
	_acpblTofu_reply_delegation_end(command); /* reply delegation fin. */
      return CMD_STAT_FINISHED;
    }
  default:
    /* 仕様上サポートしてはいけないはずだ */
    abort();
  }
#else /* FX10 */
  if(location & 1){		/** execute atomic_op. on local **/
    if(command->atomic8.base.cmd >= CMD_CAS8){	/* atomic8 */
      volatile uint64_t *ptr;
      uint64_t oldval;
      ptr = (volatile uint64_t*)acp_query_address(command->atomic8.ga_src);
      switch(command->atomic8.base.cmd & 0xFC){
      case CMD_CAS8:
	oldval = sync_val_compare_and_swap_8(ptr, command->cas8.oldval, 
					     command->cas8.newval);
	break;
      case CMD_SWAP8:
	oldval = sync_swap_8(ptr, command->atomic8.value);
	break;
      case CMD_ADD8:
	oldval = sync_fetch_and_add_8(ptr, command->atomic8.value);
	break;
      case CMD_XOR8:
	oldval = sync_fetch_and_xor_8(ptr, command->atomic8.value);
	break;
      case CMD_OR8:
	oldval = sync_fetch_and_or_8(ptr, command->atomic8.value);
	break;
      case CMD_AND8:
	oldval = sync_fetch_and_and_8(ptr, command->atomic8.value);
	break;
      }
      if(location == 3){	/* location is 3 or 1, no other cases */
	*((uint64_t *)acp_query_address(command->atomic8.ga_dst)) = oldval;
	if(command->atomic8.base.run_stat == CMD_STAT_DELEGATED)
	  _acpblTofu_reply_delegation_end(command); /* reply delegation fin. */
	return CMD_STAT_FINISHED;
      } else {
	if(command->atomic8.base.run_stat == CMD_STAT_DELEGATED)
	  reply_delegation_data_and_end(command, &oldval, 8);
	else
	  _acpblTofu_sys_put_data_imd(0, command->copy.ga_dst,
				      (void *)(&oldval), 8, id);
	return CMD_STAT_EXECUTING;
      }
    } else {
      volatile uint32_t *ptr;
      uint32_t oldval;
      ptr = acp_query_address(command->atomic4.ga_src);
      switch(command->atomic4.base.cmd & 0xFC){
      case CMD_CAS4:
	oldval = sync_val_compare_and_swap_4(ptr, command->cas4.oldval,
					     command->cas4.newval);
	break;
      case CMD_SWAP4:
	oldval = sync_swap_4(ptr, command->atomic4.value);
	break;
      case CMD_ADD4:
	oldval = sync_fetch_and_add_4(ptr, command->atomic4.value);
	break;
      case CMD_XOR4:
	oldval = sync_fetch_and_xor_4(ptr, command->atomic4.value);
	break;
      case CMD_OR4:
	oldval = sync_fetch_and_or_4(ptr, command->atomic4.value);
	break;
      case CMD_AND4:
	oldval = sync_fetch_and_and_4(ptr, command->atomic4.value);
	break;
      }
      if(location == 3){	/* location is 3 or 1, no other cases */
	*((uint32_t *)acp_query_address(command->atomic4.ga_dst)) = oldval;
	if(command->atomic4.base.run_stat == CMD_STAT_DELEGATED)
	  _acpblTofu_reply_delegation_end(command); /* reply delegation fin. */
	return CMD_STAT_FINISHED;
      } else {
	if(command->atomic4.base.run_stat == CMD_STAT_DELEGATED)
	  reply_delegation_data_and_end(command, &oldval, 4);
	else
	  _acpblTofu_sys_put_data_imd(0, command->copy.ga_dst,
				      (void *)(&oldval), 4, id);
	return CMD_STAT_EXECUTING;
      }
    }
  } else {
    if(delegate_command(command, SRC))
      return CMD_STAT_BUSY;
    else
      return CMD_STAT_DELEGATED;
  }
#endif

}

int _acpblTofu_fence(cq_t *command)
{
  _acpblTofu_sys_fence(GA2RANK(command->fence.ga_dst), command->fence.base.comm_id);
  return CMD_STAT_EXECUTING;
}

int _acpblTofu_newrank(cq_t *command)
{
  int      rank;
  acp_ga_t ga_cq, ga_target;

  rank = --(command->newrank.count);
  ga_target = _acpblTofu_gen_atkey(rank, COLOR_DLG_BUF, LOCALTAG_DLG_BUFF) + 
    (uint64_t)(myrank_sys * sizeof(delegation_buff_t));
  ga_cq     = _acpblTofu_gen_atkey(myrank_sys, COLOR_CQ, LOCALTAG_CQ) + 
    (uint64_t)(command->newrank.base.comm_id * sizeof(cq_t)); /* comm_id = #cq entry */
  _acpblTofu_sys_put_data(0, ga_target, ga_cq, sizeof(cq_t), 
			  command->newrank.base.comm_id);

  if(rank == 0)
    return CMD_STAT_EXECUTING;
  else
    return CMD_STAT_MULTI_EXEC;
}
