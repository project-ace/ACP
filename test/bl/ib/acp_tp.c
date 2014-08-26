/*
 * Copyright (c) 2008      FUJITSU LIMITED.  All rights reserved.
 * 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <acp.h>
#include "acp_tp.h"

#ifdef TSYNC
#include "mpi_time.h"
#endif

size_t iacp_starter_memory_size_dl = 0;
size_t iacp_starter_memory_size_cl = 0;

/* include tree generation test */
/*
#define APT_TST_TREE_GENERATE
*/

void *ptbl[]={apt_p0000,apt_p0001,apt_p0020,apt_p0021,apt_p0022,apt_p0024};

int main(int argc,char **argv)
{
  int err_code = 0; /* error code */

  /* stdin */
  int aptr; /* argc pointer */
  int res; /* temporary result */

  char ifile[FNMAX]; /* simulator control file name */
  char f1[FNMAX]; /* temporary use */
  char str[STRMAX]; /* temporary string area */

  struct APT_PTR_STR apt; /* pointer table */
  struct APT_CFG_STR cfg; /* configuration table */

  /* initial */
  apt.cfg = &cfg;

  apt.opt = 0x0;
  ifile[0] = '\0';

  apt.argc = argc;
  apt.id = -1;
  apt.argv = argv;
  apt.ifile = &ifile[0];

  cfg.prg_sta = 0;
  cfg.prg_end = (APT_PID_NUM-1);
  cfg.stg_sta = 0;

  /* argv get */
  aptr=0;
  while(1)
  {
    aptr++;
    if (argc <= aptr)
    {
      break;
    }

    strcpy(f1,argv[aptr]);

	/* option check */
	if(strcasecmp(f1,"-f") == 0)
	{
	  /* need next field */
	  aptr++;
	  if(argc<=aptr)
	  {
		goto USAGE;
	  }
	  /* next field get */
	  strcpy(ifile,argv[aptr]);

	  apt.opt |= APT_OPT_CFG;
	  if((apt.opt & APT_OPT_VBS) != 0)
	  {
		printf ("### Config file = %s ###\n",ifile);
	  }
	  continue;
	}

	if((strcasecmp(f1,"-ps") == 0) || (strcasecmp(f1,"-prg_sta") == 0))
	{
	  /* need next field */
	  aptr++;
	  if(argc<=aptr)
	  {
		goto USAGE;
	  }

	  /* next field get */
	  res=sscanf(argv[aptr],"%d",&cfg.prg_sta);
	  if(res != 1)
	  {
		printf("ERR : Illegal PrgSta = %s, ignored.\n",str);
		continue;
	  }
	  apt.opt |= APT_OPT_PGS;
	  if(cfg.prg_end < cfg.prg_sta) {cfg.prg_end = cfg.prg_sta;}
	  if((apt.opt & APT_OPT_VBS) != 0)
	  {
		printf ("### PrgSta = %d ###\n",cfg.prg_sta);
	  }
	  continue;
	}

	if((strcasecmp(f1,"-pe") == 0) || (strcasecmp(f1,"-prg_end") == 0))
	{
	  /* need next field */
	  aptr++;
	  if(argc<=aptr)
	  {
		goto USAGE;
	  }

	  /* next field get */
	  res=sscanf(argv[aptr],"%d",&cfg.prg_end);
	  if(res != 1)
	  {
		printf("ERR : Illegal PrgEnd = %s, ignored.\n",str);
		continue;
	  }
	  apt.opt |= APT_OPT_PGS;
	  if((apt.opt & APT_OPT_VBS) != 0)
	  {
		printf ("### PrgEnd = %d ###\n",cfg.prg_end);
	  }
	  continue;
	}

	if((strcasecmp(f1,"-ss") == 0) || (strcasecmp(f1,"-stg_sta") == 0))
	{
	  /* need next field */
	  aptr++;
	  if(argc<=aptr)
	  {
		goto USAGE;
	  }

	  /* next field get */
	  res=sscanf(argv[aptr],"%d",&cfg.stg_sta);
	  if(res != 1)
	  {
		printf("ERR : Illegal StgSta = %s, ignored.\n",str);
		continue;
	  }
	  apt.opt |= APT_OPT_SGS;
	  if((apt.opt & APT_OPT_VBS) != 0)
	  {
		printf ("### StgSta = %d ###\n",cfg.stg_sta);
	  }
	  continue;
	}

	if(strcasecmp(f1,"-v") == 0)
	{
	  printf ("### verbose mode ###\n");
	  apt.opt |= APT_OPT_VBS;
	  continue;
	}

	if(strcasecmp(f1,"-h") == 0)
	{
	  goto USAGE;
	}

	printf("ERR : Illegal option = %s, ignored.\n",f1);
	continue;
  }

  //  SKIP_OPT_CHK:  
  res=apt_main(&apt);
  if(res != 0)
  {
    err_code = res;
    goto PRG_E_END;
  }

PRG_N_END:
  if(apt.id == 0)
  {
	printf("code = %x\n",err_code);
  }

  fflush(stdout);
  return(err_code);

PRG_E_END:
  printf("ERR: id %d code %x\n",apt.id,err_code);
  goto PRG_N_END;

USAGE:
  printf("apt         : acp tester\n");
  printf("Usage       : aptne [-h] [-v] [-f Cfg]");
  printf(" [-ps PrgSta] [-pe PrgEnd] [-ss StgSta]");
  printf(" -h         : display this help\n");
  printf(" -v         : verbose mode\n");
  printf(" -f         : configuration file read\n");
  printf(" -ps        : test start prognum number set\n");
  printf(" -pe        : test end prognum number set\n");
  printf(" -ss        : test start stage number set\n");
  printf(" Cfg        : configuration file name\n");
  printf(" PrgSta     : test start progrum number (default=0)\n");
  printf(" PrgEnd     : test end progrum number (default=MAX)\n");
  printf(" StgSta     : test start stage number (default=0)\n");

  return(2);

}

int apt_main(
  struct APT_PTR_STR *apt
)
{
  int err_code; /* error code */
  int i; /* for general */
  int (*tp)(struct APT_PTR_STR *);
  /*
  apt->cfg->prg_sta = 0;
  apt->cfg->prg_end = 1;
  apt->cfg->stg_sta = 0;
  */

  for(i=apt->cfg->prg_sta;i<=apt->cfg->prg_end;i++)
  {
	tp=ptbl[i];
	err_code=tp(apt);
	if(err_code != 0) {break;}
  }

  return(err_code);
}

/* apt_sndrcv_ga : copy srnk sga to rrnk rga */
#define ERR_APT_SR_1 0x00000001
#define ERR_APT_SR_2 0x00000002
#define ERR_APT_SR_3 0x00000003
#define ERR_APT_SR_4 0x00000004
#define ERR_APT_SR_5 0x00000005
#define ERR_APT_SR_6 0x00000006
#define ERR_APT_SR_7 0x00000007
#define ERR_APT_SR_8 0x00000008
#define ERR_APT_SR_9 0x00000009
#define ERR_APT_SR_10 0x0000000a
#define ERR_APT_SR_11 0x0000000b
#define ERR_APT_SR_12 0x0000000c

/* srnk : ga send rank */
/* sga  : send ga */
/* rrnk : ga receive rank */
/* rga  : receive ga */
/* op   : operation code */
/*      : APT_SR_OP_PUT : srnk copy sga from srnk to rrnk */
/*      : APT_SR_OP_GET : rrnk copy sga from srnk to rrnk */
/* ### collective operation ### */
int apt_sndrcv_ga(int srnk, acp_ga_t sga, int rrnk, acp_ga_t *rga, int op)
{
  int err_code = 0; /* error code */
  int res; /* result code */
  int rank;/* rank */

  acp_ga_t sbg;/* send buffer(starter memory) ga */
  acp_ga_t *sbl;/* send buffer(starter memory) local address */

  acp_ga_t rrg;/* remote receive buffer(starter memory) ga */
  acp_ga_t rsg;/* remote send buffer(starter memory) ga */

  acp_ga_t rbg;/* receive buffer(starter memory) ga */
  acp_ga_t *rbl;/* receive buffer(starter memory) local address */

  acp_handle_t hnd;

  rank = acp_rank();
  if(rank < 0) {err_code = ERR_APT_SR_1;goto E_END;}

  /* send buffer operation */
  if(rank == srnk) {
	/* get send buffer ga */
	sbg = acp_query_starter_ga(srnk);
	if(sbg == ACP_GA_NULL) {err_code = ERR_APT_SR_2;goto E_END;}
	sbl = (acp_ga_t *)acp_query_address(sbg);
	if(sbl == NULL) {err_code = ERR_APT_SR_3;goto E_END;}
	sbl[0]=sga;
  }

  if(rank == rrnk) {
	/* get receive buffer local address */
	rbg = acp_query_starter_ga(rank);
	if(rbg == ACP_GA_NULL) {err_code = ERR_APT_SR_4;goto E_END;}
	rbl = (acp_ga_t *)acp_query_address(rbg);
	if(rbl == NULL) {err_code=ERR_APT_SR_5;goto E_END;}
  }

  switch(op)
  {
  case APT_SR_OP_PUT:
	/* send buf process do copy */
	if(rank == srnk) {
	  /* get remote receive buffer ga */
	  rrg = acp_query_starter_ga(rrnk);
	  if(rrg == ACP_GA_NULL) {err_code=ERR_APT_SR_6;goto E_END;}
	  /* put ga */
	  hnd = acp_copy(rrg + sizeof(acp_ga_t), sbg, sizeof(acp_ga_t)
					 , ACP_HANDLE_NULL);
	  if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_SR_7;goto E_END;}
	  acp_complete(hnd);
	}

	res = acp_sync();
	if(res != 0) {err_code=ERR_APT_SR_8;goto E_END;}
	break;

  case APT_SR_OP_GET:
	/* recieve buf process do copy */
	res = acp_sync(); /* wait send ga set */
	if(res != 0) {err_code=ERR_APT_SR_9;goto E_END;}
	if(rank == rrnk) {
	  /* get remote send buffer ga */
	  rsg = acp_query_starter_ga(srnk);
	  if(rsg == ACP_GA_NULL) {err_code=ERR_APT_SR_10;goto E_END;}
	  /* get ga */
	  hnd = acp_copy(rbg + sizeof(acp_ga_t), rsg, sizeof(acp_ga_t)
					 , ACP_HANDLE_NULL);
	  if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_SR_11;goto E_END;}
	  acp_complete(hnd);
	}
	break;
  }

  if(rank == rrnk) {
	/* copy to rga */
	rga[0] = rbl[1];
  }

  /* start buffer reuse is available after sync */
  res = acp_sync();
  if(res != 0) {err_code=ERR_APT_SR_12;goto E_END;}

  E_END:

  return err_code;
}

/* test initial process */
#define ERR_APT_IP_1 0x00000001
#define ERR_APT_IP_2 0x00000002
#define ERR_APT_IP_3 0x00000003
int apt_ip(struct APT_PTR_STR *apt, int *rank, int *pno)
{
  int err_code = 0; /* error code */
  int res; /* result code */

  res = acp_init(&apt->argc,&apt->argv);
  if(res != 0) {err_code = ERR_APT_IP_1;goto E_END;}
  *rank = acp_rank();
  if(*rank < 0) {err_code = ERR_APT_IP_2;goto E_END;}
  apt->id=*rank;
  *pno = acp_procs();
  if(*pno <= 0) {err_code = ERR_APT_IP_3;goto E_END;}

  E_END:
  return err_code;
}

/* gather operation */
#define ERR_APT_GAT_1 0x00000010
#define ERR_APT_GAT_2 0x00000020
#define ERR_APT_GAT_3 0x00000030
#define ERR_APT_GAT_4 0x00000040
#define ERR_APT_GAT_5 0x00000050
#define ERR_APT_GAT_6 0x00000060
#define ERR_APT_GAT_7 0x00000070
#define ERR_APT_GAT_8 0x00000080
#define ERR_APT_GAT_9 0x00000090
#define ERR_APT_GAT_10 0x000000a0
#define ERR_APT_GAT_11 0x000000b0

/* sbuf : send buffer */
/* rbuf : receive buffer */
/* msz  : send buffer size */
/* root : root rank number */
int apt_gather(void *sbuf, void *rbuf, int msz, int root)
{
  int err_code = 0; /* error code */
  int res; /* result code */

  int rank; /* my rank */
  int pno; /* number of process */

  acp_atkey_t skey; /* send buffer registration key */
  acp_ga_t sbg; /* send buffer ga */

  acp_atkey_t rkey; /* receive buffer registration key */
  acp_ga_t rbg; /* receive buffer ga */

  acp_ga_t rrg; /* remote receive buffer ga */

  acp_handle_t hnd;

  rank = acp_rank();
  if(rank < 0) {err_code = ERR_APT_GAT_1;goto E_END;}
  pno = acp_procs();
  if(pno < 0) {err_code = ERR_APT_GAT_2;goto E_END;}

  /* send buffer */
  skey = acp_register_memory(sbuf,msz,0);
  if(skey == ACP_ATKEY_NULL) {err_code = ERR_APT_GAT_3;goto E_END;}
  sbg = acp_query_ga(skey,sbuf);
  if(sbg == ACP_GA_NULL) {err_code = ERR_APT_GAT_4;goto E_END;}

  /* receive buffer */
  if(rank == root)
  {
	rkey = acp_register_memory(rbuf,msz*pno,0);
	if(rkey == ACP_ATKEY_NULL) {err_code = ERR_APT_GAT_5;goto E_END;}
	rbg = acp_query_ga(rkey,rbuf);
	if(rbg == ACP_GA_NULL) {err_code=ERR_APT_GAT_6;goto E_END;}
  }

  res = apt_sndrcv_ga(root, rbg, rank, &rrg, APT_SR_OP_GET);
  if(res != 0) {err_code |= ERR_APT_GAT_7;goto E_END;}

  /* put sbuf to rbuf */
  hnd = acp_copy(rrg + (msz*rank), sbg, msz, ACP_HANDLE_NULL);
  if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_GAT_8;goto E_END;}
  acp_complete(hnd);

  res = acp_sync();
  if(res != 0) {err_code=ERR_APT_GAT_9;goto E_END;}

  res = acp_unregister_memory(skey);
  if(res != 0) {err_code=ERR_APT_GAT_10;goto E_END;}

  if(rank == root) {
	res = acp_unregister_memory(rkey);
	if(res != 0) {err_code=ERR_APT_GAT_11;goto E_END;}
  }

E_END:
  return err_code;
}

int iacp_init_dl(){return 0;}
int iacp_init_cl(){return 0;}
int iacp_finalize_dl(){return 0;}
int iacp_finalize_cl(){return 0;}
void iacp_abort_cl(){return;}
void iacp_abort_dl(){return;}
