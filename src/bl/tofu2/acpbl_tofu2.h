/*****************************************************************************/
/***** ACP Basic Layer / Tofu					         *****/
/*****   header								 *****/
/*****									 *****/
/***** Copyright FUJITSU LIMITED 2014					 *****/
/*****									 *****/
/***** Specification Version: ACP-140312				 *****/
/***** Version: 0.1							 *****/
/***** Module Version: 0.1						 *****/
/*****									 *****/
/***** Note:								 *****/
/*****************************************************************************/
#ifndef __ACPBL_TOFU_H__
#define __ACPBL_TOFU_H__

typedef int acp_size_t;

/*---------------------------------------------------------------------------*/
/*** system defaluts assignments                               ***************/
/*** these values can be changed depends on system environment ***************/
/*---------------------------------------------------------------------------*/
/* queue size */
#define CQ_DEPTH 		(4096*4)
#define CQ_MASK 		(CQ_DEPTH - 1)
#define DQ_DEPTH 		1024

/* local tag defines */
#if 0
#define LOCALTAG_CQ		1024			/* must be more than 0 (>0)
							   because of ACP_GA_NULL */
#define LOCALTAG_DLG_BUFF	(LOCALTAG_CQ + 1)
#define LOCALTAG_STARTER_BL	(LOCALTAG_CQ + 2)
#define LOCALTAG_STARTER_DL	(LOCALTAG_CQ + 3)
#define LOCALTAG_STARTER_CL	(LOCALTAG_CQ + 4)
#define LOCALTAG_USER		(LOCALTAG_CQ + 5)
#else
enum predefined_localtag{
  LOCALTAG_CQ = 1024,
  LOCALTAG_DLG_BUFF,
  LOCALTAG_SYNC_BUFF,
  LOCALTAG_STARTER_BL,
  LOCALTAG_STARTER_DL,
  LOCALTAG_STARTER_CL,
  LOCALTAG_USER
};
#endif

/* color defines */
#define NUM_COLORS		4
#define COLOR_CQ		0
#define COLOR_DLG_BUF		0
#define COLOR_STARTER		0

/* memory defines */
#define STARTER_SIZE_DEFAULT	1024

/*---------------------------------------------------------------------------*/
/*** system constants                   **************************************/
/*** Don't touch following constants !! **************************************/
/*---------------------------------------------------------------------------*/
/** ACPbl/Tofu end status **/
#define SUCCEEDED		0
#define FAILED			-1

/** system state **/
#define SYS_STAT_FREE		-1
#define SYS_STAT_INITIALIZE	1
#define SYS_STAT_RUN		2
#define SYS_STAT_RESET		3
#define SYS_STAT_FINALIZE	4

/** commannd queue, delegation command codes **/
#define CMD_FENCE		0x01
#define CMD_SYNC		0x02
#define CMD_COMPLETE		0x03

#define CMD_NEW_RANK		0x04
#define CMD_RANK_TABLE		0x05

#define CMD_COPY		0x0C

#define CMD_CAS4		0x10
#define CMD_SWAP4		0x14
#define CMD_ADD4		0x18
#define CMD_XOR4		0x1C
#define CMD_OR4			0x20
#define CMD_AND4		0x24

#define CMD_CAS8		0x40
#define CMD_SWAP8		0x44
#define CMD_ADD8		0x48
#define CMD_XOR8		0x4C
#define CMD_OR8			0x80
#define CMD_AND8		0x84

/** command running status **/
#define CMD_STAT_FREE		0x00
#define CMD_STAT_QUEUED		0x01
#define CMD_STAT_ORDER		0x02
#define CMD_STAT_ORDER_END	0x03
#define CMD_STAT_EXECUTING	0x04
#define CMD_STAT_MULTI_EXEC	0x05
#define CMD_STAT_DELEGATED	0x06
#define CMD_STAT_FINISHED	0x0F
#define CMD_STAT_EXED_AT_MAIN	0x14
#define CMD_STAT_DELE_AT_MAIN	0x16
#define CMD_STAT_FIN_AT_MAIN	0x1F
#define CMD_STAT_BUSY		0xFE
#define CMD_STAT_FAILED		0xFF

/** command properties **/
#define CMD_PROP_HANDLE_NULL	0xC000
#define CMD_PROP_HANDLE_CONT	0x8000
#define CMD_PROP_HANDLE_ALL	0x4000
#define CMD_PROP_TRNS_DIRECTION	0x0001	/* 0: to SRC, 1: to DST */

/** direction **/
#define SRC			1
#define DST			2

/** communication properties **/
#define COMM_REPLY		0x8000
#define COMM_DELEGATED_OP	0x4000

/** memory defines **/
#define MEMTYPE_STARTER		0
#define MEMTYPE_USER		1
#define NON			-1
#define NOT_ENABLED		0

/*** Tofu system definitions ***/
#define TOFU_SYS_SUCCEEDED	0
#define TOFU_SYS_FAILED		-1

#define TOFU_SYS_STAT_TRANSEND	1
#define TOFU_SYS_STAT_RECEIVED	2
#define TOFU_SYS_STAT_TRANSERR	-1

#define TOFU_SYS_FLAG_NOTIFY	1
#define TOFU_SYS_FLAG_CONTINUE	2

/*** debug ***/
#define PRINT_INFO		0x0001
#define PRINT_QP		0x0002
#define PRINT_TOFU		0x0004
#define PRINT_DEBUG		0x0008

/*---------------------------------------------------------------------------*/
/*** type defines ************************************************************/
/*---------------------------------------------------------------------------*/
typedef struct{
  void		*addr_head;	/* head of logical address */
  void		*addr_tail;	/* tail of logical address */
  int		count; 		/* count of acp_register_memory() */
  int		color_bit;	/* bit0: color-0, bit1: color-1, 
				   bit2: color-2, bit3: color-3 */
  int		type;		/* 0: special segment, 1: user segment */ 
  int		status;		/* 0: not registered in device, 
				   1: registered in device */
} localtag_t;

typedef struct{			/** tofu transfer status **/
  int		status;		/* transfer status, 0: success, other: fail */
  int		comm_id;	/* user defined communication id */
  int		localtag;	/* local tag */
  uint64_t	offset;		/* offset in delegation buffer */
  uint64_t      data;           /* ARMW return value */
} tofu_trans_stat_t;

/*** command format ***/
typedef struct {		/** base **/
  uint32_t	jobid;		/* parallel job id 	will be delegated */
  uint8_t	cmd;		/* command 		will be delegated */
  uint8_t	run_stat;	/* running status 	will be delegated */
  uint16_t	comm_id;	/* initiater handle 	will be delegated */
} base_t;

struct cmd_type01 {		/** fence **/
  uint16_t	props;		/* properties		no delegated */
				/* bit15-14: 11 ACP_HANDLE_NULL,
					     10 ACP_HANDLE_CONT,
					     01 ACP_HANDLE_ALL,
					     00 ORDER specified
				   bit0:     delegate to 0: SRC, 1: DST */    
  uint16_t	reserve;	/* reserve		no delegated */
  uint32_t	order;		/* order 		no delegated */
  base_t	base;		/* command base */
  acp_ga_t	ga_dst;
};

struct cmd_type02 {		/** noarg **/
  uint16_t	props;		/* properties		no delegated */
  uint16_t	reserve;	/* reserve		no delegated */
  uint32_t	order;		/* order 		no delegated */
  base_t	base;		/* command base */
};

struct cmd_type04 {		/** newrank **/
  uint16_t	props;		/* properties		no delegated */
  uint16_t	reserve;	/* reserve		no delegated */
  uint32_t	order;		/* order 		no delegated */
  base_t	base;		/* command base */
  int		value;
  uint32_t	count;
};

struct cmd_type0C {		/** copy **/
  uint16_t	props;		/* properties		no delegated */
  uint16_t	reserve;	/* reserve		no delegated */
  uint32_t	order;		/* order 		no delegated */
  base_t	base;		/* command base */
  acp_ga_t	ga_dst;
  acp_ga_t	ga_src;
  size_t	size;
};

struct cmd_type10 {		/** cas4 **/
  uint16_t	props;		/* properties		no delegated */
  uint16_t	reserve;	/* reserve		no delegated */
  uint32_t	order;		/* order 		no delegated */
  base_t	base;		/* command base */
  acp_ga_t	ga_dst;
  acp_ga_t	ga_src;
  uint32_t	oldval;
  uint32_t	newval;
};

struct cmd_type14 {		/** atomic4 **/
  uint16_t	props;		/* properties		no delegated */
  uint16_t	reserve;	/* reserve		no delegated */
  uint32_t	order;		/* order 		no delegated */
  base_t	base;		/* command base */
  acp_ga_t	ga_dst;
  acp_ga_t	ga_src;
  uint32_t	value;
};

struct cmd_type40 {		/** cas8 **/
  uint16_t	props;		/* properties		no delegated */
  uint16_t	reserve;	/* reserve		no delegated */
  uint32_t	order;		/* order 		no delegated */
  base_t	base;		/* command base */
  acp_ga_t	ga_dst;
  acp_ga_t	ga_src;
  uint64_t	oldval;
  uint64_t	newval;
};

struct cmd_type44 {		/** atomic8 **/
  uint16_t	props;		/* properties		no delegated */
  uint16_t	reserve;	/* reserve		no delegated */
  uint32_t	order;		/* order 		no delegated */
  base_t	base;		/* command base */
  acp_ga_t	ga_dst;
  acp_ga_t	ga_src;
  uint64_t	value;
};

typedef union {
  struct cmd_type01 fence;
  struct cmd_type02 noarg;
  struct cmd_type04 newrank;
  struct cmd_type0C copy;
  struct cmd_type10 cas4;
  struct cmd_type14 atomic4;
  struct cmd_type40 cas8;
  struct cmd_type44 atomic8;
} cq_t;

typedef struct {
  cq_t* command;
  int	comm_id;
} dq_t;

/* delegation buffer format */
typedef struct {
  cq_t		command;	/* delegated command, sholud be palced first
				   in this struct */
  uint32_t	flag;
  int32_t	end_status;
} delegation_buff_t;


/*---------------------------------------------------------------------------*/
/*** common macros ***********************************************************/
/*---------------------------------------------------------------------------*/
#define GA2COLOR(_ga) ((_ga & ga_mask_color) >> ga_lsb_color)
#define GA2LOCALTAG(_ga) ((_ga & ga_mask_localtag) >> ga_lsb_localtag)
#define GA2RANK(_ga) ((_ga & ga_mask_rank) >> ga_lsb_rank)
#define GA2OFFSET(_ga) ((_ga & ga_mask_offset) >> ga_lsb_offset)
#define ATKEY2LOCALTAG GA2LOCALTAG
#define MAX(_val, _ref) ((_val > _ref) ? _val : _ref)
#define MIN(_val, _ref) ((_val < _ref) ? _val : _ref)
#define _acpblTofu_print(_msg, _code) { printf("%d: %s: %d\n", 		\
					       myrank_sys, (_msg), (int)(_code)); }
#define ERROR_RETURN(_str, _ival) { \
    printf("ERROR[%d]: %s (%d)\n", myrank_sys, (_str), (_ival));		\
    return _ival; }

/****************Error/termination macros************************/
/**Debug assert macro. To be used instead of assert for more user
 * informative and cleaner death. Also allows individualized
 * enabling/disabling of assert statements.
 * @param _enable Ignore this assertion if _enable==0
 * @param _cond   Condition to be evaluated (assert that it is true)
 * @param _plist  Information to be printed using printf, should be
 * within parenthesis (eg., dassert(1,0,("%d:test n=%d\n",me,0));
 * ). This is fed directly to printf.  
 */
void dassertp_fail(const char *cond_string, const char *file, 
		   const char *func, unsigned int line) __attribute__((noreturn));
#undef dassertp
#define dassertp(_enable,_cond,_plist)  do {  				\
    if((_enable) && !(_cond)) {						\
      printf _plist;							\
      dassertp_fail(#_cond,__FILE__,__FUNCTION__,__LINE__);		\
    }} while(0)

#define _acpblTofu_die(_msg,_code) {					\
    _acpblTofu_sys_finalize();						\
    dassertp(1,0, ("%d:%s: %d\n", myrank_sys,(_msg),(int)(_code))); }

/*---------------------------------------------------------------------------*/
/*** debug macros ************************************************************/
/*---------------------------------------------------------------------------*/
#define BACKTRACP() {							\
    printf("%d: BackTrace\n", myrank_sys);				\
    fflush(stdout);							\
    void *__trace[128];							\
    int __n = backtrace(__trace, sizeof(__trace) / sizeof(__trace[0]));	\
    backtrace_symbols_fd(__trace, __n, 1);				\
    fflush(stdout);							\
  }

#define DUMP_CQP() {							\
    printf("cqwp: %ld, cqrp: %ld, cqcp: %ld\n", 			\
	   cqwp, cqrp, cqcp); fflush(stdout); }

#define POINT() 
/*
{ printf("DEBUG POINT[%d]: %d - %s @ %s\n",				\
		    myrank_sys, __LINE__, __FUNCTION__, __FILE__); fflush(stdout); }
*/

#define POINT_NUM(_num) //{ printf("DEBUG POINT: %d\n", _num); fflush(stdout); }

#define GET_CLOCK(_prof) { profile[_prof] = get_clock(); }

#define DUMP_CQXP() 
/*									\
 {									\
  printf("%d-%s@%s: cqwp: 0x%08x, cqrp: 0x%08x, cqcp: 0x%08x\n",	\
	 __LINE__, __FUNCTION__, __FILE__, cqwp, cqrp, cqcp);		\
  fflush(stdout); }
*/

#define CLK_DUR_MAX 10000

#define dprintf(...) //do{printf(__VA_ARGS__);fflush(stdout);}while(0)

#define USE_TOFU2_ATOMIC 1
#define ARMW_OPT         1
#define THREAD_SAFE      1
#define EMPTY_BYPASS     0
#define PERF_BUG_DTIME   0

#endif /* acpbl_tofu.h */
