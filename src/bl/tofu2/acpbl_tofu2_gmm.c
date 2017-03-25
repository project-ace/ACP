/*****************************************************************************/
/***** ACP Basic Layer / Tofu					         *****/
/*****   gmm operations							 *****/
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
#include <math.h>
#include "acp.h"
#include "acpbl.h"
#include "acpbl_tofu2.h"
#include "acpbl_tofu2_sys.h"

/*---------------------------------------------------------------------------*/
/*** external functions ******************************************************/
/*---------------------------------------------------------------------------*/


/*---------------------------------------------------------------------------*/
/*** variables ***************************************************************/
/*---------------------------------------------------------------------------*/
extern int	sys_state;
extern int	myrank_sys;
extern uint64_t profile[];

int	   ga_bitwidth_color, ga_bitwidth_localtag, ga_bitwidth_rank,
	   ga_bitwidth_offset;
uint64_t   ga_lsb_color,  ga_lsb_localtag,  ga_lsb_rank,  ga_lsb_offset;
uint64_t   ga_mask_color, ga_mask_localtag, ga_mask_rank, ga_mask_offset;
uint32_t   max_num_localtag;
localtag_t **localtag_table = NULL;
int	   last_registered_localtag = NON; /* 0> registered but no ga 
					      assigned, 0< no registered and no
					      assigned */
int	   next_localtag;


/*---------------------------------------------------------------------------*/
/*** macros ******************************************************************/
/*---------------------------------------------------------------------------*/
#define ATKEY_GEN(_element,_lsb,_mask) \
  (((acp_ga_t)(_element) << (_lsb)) & (_mask))


/*---------------------------------------------------------------------------*/
/*** debug functions *********************************************************/
/*---------------------------------------------------------------------------*/
void _acpblTofu_dump_ga(acp_ga_t ga, int flag)
{
  if(flag)
    printf("%d: ga: ", myrank_sys);
  printf("0x%016lx (color=%ld, rank=%ld, localtag=%ld, offset=0x%lx)\n", ga,
	 GA2COLOR(ga), GA2RANK(ga), GA2LOCALTAG(ga), GA2OFFSET(ga));
}


/*---------------------------------------------------------------------------*/
/*** acpbl functions *********************************************************/
/*---------------------------------------------------------------------------*/
int _acpblTofu_atkey_init()
{
  int i;

  if(sys_state == SYS_STAT_INITIALIZE){
    localtag_table = malloc(max_num_localtag * sizeof(localtag_t*));
    if(localtag_table == NULL)
      ERROR_RETURN("malloc failed: localtag_table", -1);
    for(i=0; i<=max_num_localtag; i++)
      localtag_table[i] = NULL;
  }
  next_localtag = LOCALTAG_USER;
  return SUCCEEDED;
}

int _acpblTofu_atkey_free()
{
  int i, rc;

  for(i=LOCALTAG_USER; i<next_localtag; i++){
    if((localtag_table[i] != NULL) && localtag_table[i]->status){
      rc = _acpblTofu_sys_unregister_memory(i);
      if(rc) return rc;
    }
    free(localtag_table[i]);
    localtag_table[i] = NULL;
  }

  /* don't free localtag_table st acp_reset(), 
     because startar's localtag is remained */
  return 0;
}

/*---------------------------------------------------------------------------*/
int _acpblTofu_enable_localtag(int localtag)
{
  int i, color_bit, rc;

  color_bit = localtag_table[localtag]->color_bit;
  last_registered_localtag = NON;
  for(i=0; i<NUM_COLORS; i++){
    if(color_bit & 1){
      rc = _acpblTofu_sys_register_memory(localtag_table[localtag]->addr_head,
					  (char *)(localtag_table[localtag]->addr_tail) - 
					  (char *)(localtag_table[localtag]->addr_head) + 1,
					  i, localtag);
      if(rc)
	return rc;	/* memory registration to device is failed */
    }
    localtag_table[localtag]->status |= color_bit;
    color_bit >>= 1;
  }
  return 0;
}

/*---------------------------------------------------------------------------*/
int _acpblTofu_register_memory(void *addr, acp_size_t size, int color, 
			       int localtag, int type)
{
  /*** assign localtag and register memory into localtag_table, but not 
       register to the device at this time ***/
  /*** this returns localtag ***/
  int i, color_bit;

  color_bit = 1 << color;

  /** merge check **/
  if(((localtag == NON)  && (last_registered_localtag >= LOCALTAG_USER)) ||
     ((localtag >= 0) && (last_registered_localtag == localtag) &&
      (color_bit == localtag_table[last_registered_localtag]->color_bit))){
    /** substance of merging atkey **/
    if(addr == (char *)(localtag_table[last_registered_localtag]->addr_tail)+1){
      /* merge specified memory to tail of last_registered memory */
      localtag_table[last_registered_localtag]->addr_tail =
	(char *)(localtag_table[last_registered_localtag]->addr_tail) + size;
      //      last_registered_localtag = localtag;
      localtag_table[last_registered_localtag]->count++;
      return last_registered_localtag;
    } else if((char *)addr+size == localtag_table[last_registered_localtag]->addr_head){
      /* merge specified memory to head of last_registered memory */
      localtag_table[last_registered_localtag]->addr_head = (char *)addr;
      //      last_registered_localtag = localtag;
      localtag_table[last_registered_localtag]->count++;
      return last_registered_localtag;
    }
  }
  last_registered_localtag = NON;

  /* look for a localtag which addr range involves this addr range */
  for(i=0; i<next_localtag; i++){	// TODO: too slow, shuld be modified !!
    if(localtag_table[i] != NULL){
      if(((char *)(localtag_table[i]->addr_head) >= (char *)addr) &&
	 ((char *)(localtag_table[i]->addr_tail) < (char *)addr + size)){
	localtag_table[i]->count++;	/* larger range localtag found */
	return i;
      }
    }
  }

  /* new localtag */
  if(localtag == NON){
    if(next_localtag == max_num_localtag + 1)
      ERROR_RETURN("number of localtag exceeds maximum", FAILED);
    localtag = next_localtag++;
  } else if((localtag_table[localtag] != NULL) &&
	    (localtag_table[localtag]->status)){
    ERROR_RETURN("specified localtag is already used in the device", FAILED);
  }
  localtag_table[localtag] = malloc(sizeof(localtag_t));
  if(localtag_table[localtag] == NULL)
    ERROR_RETURN("localtag: malloc failed", FAILED);

  localtag_table[localtag]->addr_head = addr;
  localtag_table[localtag]->addr_tail = (char *)addr + size - 1;
  localtag_table[localtag]->count = 1;
  localtag_table[localtag]->color_bit = color_bit;
  localtag_table[localtag]->type = type;
  localtag_table[localtag]->status = NOT_ENABLED;
  last_registered_localtag = localtag;

  return localtag;
}

int _acpblTofu_unregister_memory(int localtag)
{
  int rc;

  last_registered_localtag = NON;	// TODO: check compatibility with other implementations

  if(localtag < LOCALTAG_USER){
    /** system memory (starter, cq, ...) **/
    if(sys_state != SYS_STAT_FINALIZE)
      ERROR_RETURN("system memory cannot be unregistered except acp_finalize()",
		   localtag);
  } else {
    /** user memory **/
    if(--localtag_table[localtag]->count)
      return localtag_table[localtag]->count;
  }

  rc = _acpblTofu_sys_unregister_memory(localtag);
  free(localtag_table[localtag]);
  localtag_table[localtag] = NULL;
  return rc;
}

acp_atkey_t _acpblTofu_gen_atkey(int rank, int color, int localtag)
{
#if 0
  dprintf("_acpblTofu_gen_atkey@rank%d: rank=%d, color=%d, localtag=%d, part0=0x%016lx, part1=0x%016lx, part2=0x%016lx\n",
	 myrank_sys, rank, color, localtag,
	 ATKEY_GEN((uint64_t)color, ga_lsb_color, ga_mask_color),
	 ATKEY_GEN((uint64_t)rank, ga_lsb_rank, ga_mask_rank),
	 ATKEY_GEN((uint64_t)localtag, ga_lsb_localtag, ga_mask_localtag));

  dprintf("_acpblTofu_gen_atkey@rank%d: ga_lsb_color=%d, ga_mask_color=0x%016lx, ga_lsb_rank=%d, ga_mask_rank=0x%016lx, ga_lsb_localtag=%d, ga_mask_localtag=0x%016lx\n",
	 myrank_sys, ga_lsb_color, ga_mask_color, ga_lsb_rank, ga_mask_rank, ga_lsb_localtag, ga_mask_localtag);
#endif

  /*** genelates atkey from color, rank, and localtag ***/
  return (ATKEY_GEN((uint64_t)color, ga_lsb_color, ga_mask_color) |
	  ATKEY_GEN((uint64_t)rank, ga_lsb_rank, ga_mask_rank)    |
	  ATKEY_GEN((uint64_t)localtag, ga_lsb_localtag, ga_mask_localtag));
}
