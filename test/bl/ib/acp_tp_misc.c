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

/* apt_0000 : acp_rank test */
#define ERR_APT_0000_1 0x00000100
#define ERR_APT_0000_2 0x00000200
#define ERR_APT_0000_3 0x00000300
#define ERR_APT_0000_4 0x00000400
#define ERR_APT_0000_5 0x00000500
#define ERR_APT_0000_6 0x00000600
#define ERR_APT_0000_7 0x00000700
#define ERR_APT_0000_8 0x00000800
#define ERR_APT_0000_9 0x00000900
#define ERR_APT_0000_10 0x00000a00

int apt_p0000(
  struct APT_PTR_STR *apt
)
{
  int err_code = 0; /* error code */
  int res; /* result code */
  int i,j,x; /* for general */

  int rank; /* my rank */
  int pno; /* number of process */
  int stg; /* stage number */

  acp_ga_t sga; /* starter buffer global address */
  int *sla; /* starter buffer logical address */

  int *buf; /* buffer for gather */

  stg = 0;
  res = acp_init(&apt->argc,&apt->argv);
  if(res < 0) {err_code=ERR_APT_0000_1;goto E_END;}
  rank = acp_rank();
  if(rank < 0) {err_code=ERR_APT_0000_3;goto E_END;}
  apt->id=rank;
  pno = acp_procs();
  if(pno < 0) {err_code=ERR_APT_0000_4;goto E_END;}

  switch(apt->cfg->stg_sta)
  {
  case 1: stg = 1;goto STG_1;
  }
  // stage 0: rank number check
  if(rank >= pno) {err_code=ERR_APT_0000_5;goto E_END;}
  sga = acp_query_starter_ga(rank);
  if(sga == ACP_GA_NULL) {err_code=ERR_APT_0000_8;goto E_END;}
  sla = acp_query_address(sga);
  if(sla == NULL) {err_code=ERR_APT_0000_6;goto E_END;}
  
  stg++;

STG_1:
  // stage 1: illegal rank number check
  if(rank == 0)
  {
	buf = malloc(pno*sizeof(int));
	if(buf == NULL) {err_code=ERR_APT_0000_10;goto E_END;}
	for(i=0;i<pno;i++) {buf[i]=0;}
  }
  res = apt_gather(&rank,buf,sizeof(int),0);
  if(res != 0) {err_code |= ERR_APT_0000_9;goto E_END;}

  // check illegal
  if(rank == 0)
  {
#ifdef APT_DEBUG
	printf("RBF : %08d stg %d pno %d",rank,stg,pno);
	for(i=0;i<pno;i++) {printf(" %08x",buf[i]);}
	printf("\n");
#endif
	for(i=0;i<pno;i++)
	{
	  x=buf[i];
	  for(j=i+1;j<pno;j++)
	  {
		if(x==buf[j]) {err_code=ERR_APT_0000_7;goto E_END;}
	  }
	}
	free(buf);
  }
  stg++;
  res = acp_finalize();
  if(res < 0) {err_code=ERR_APT_0000_2;goto E_END;}

N_END:
  if(rank == 0)
  {
	if(err_code == 0) {printf("STT: 0 PASS PRG %d\n",APT_PID_0000);}
	else {printf("STT: 1 FAIL PRG %d-%d\n",APT_PID_0000,stg);}
  }
  return err_code;

E_END:
  x = err_code | (APT_PID_0000<<APT_ERR_PRG_SFT);
  switch (err_code & APT_ERR_EL1_MSK)
  {
  case ERR_APT_0000_1:
	printf("E%08ud : STG %03d code %08x acp_init\n",-1,stg,x);
	break;
  case ERR_APT_0000_2:
	printf("E%08ud : STG %03d code %08x acp_finalize\n",rank,stg,x);
	break;
  case ERR_APT_0000_3:
	printf("E%08ud : STG %03d code %08x acp_rank\n",-1,stg,x);
	break;
  case ERR_APT_0000_4:
	printf("E%08ud : STG %03d code %08x acp_procs\n",rank,stg,x);
	break;
  case ERR_APT_0000_5:
	printf("E%08ud : STG %03d code %08x rank > procs\n",rank,stg,x);
	break;
  case ERR_APT_0000_6:
	printf("E%08ud : STG %03d code %08x rank no.\n",rank,stg,x);
	break;
  case ERR_APT_0000_7:
	printf("E%08ud : STG %03d code %08x illegal rank\n",rank,stg,x);
	break;
  case ERR_APT_0000_8:
	printf("E%08ud : STG %03d code %08x acp_query_ga\n",rank,stg,x);
	break;
  case ERR_APT_0000_9:
	printf("E%08ud : STG %03d code %08x acp_gather\n",rank,stg,x);
	break;
  case ERR_APT_0000_10:
	printf("E%08ud : STG %03d code %08x malloc\n",rank,stg,x);
	break;
  }

  switch (err_code & APT_ERR_EL1_MSK)
  {
  case ERR_APT_0000_1:
  case ERR_APT_0000_2:
	printf("STL: 1 FAIL PRG %d-%d\n",0,stg);
	abort();

  case ERR_APT_0000_3:
  case ERR_APT_0000_4:
  case ERR_APT_0000_5:
  case ERR_APT_0000_6:
  case ERR_APT_0000_8:
  case ERR_APT_0000_9:
  case ERR_APT_0000_10:
	printf("STL: 1 FAIL PRG %d-%d\n",0,stg);
	acp_abort(NULL);
	abort();
  }

  acp_finalize();
  err_code = x;
  goto N_END;
}

/* apt_0001 : acp_procs test */
#define ERR_APT_0001_MSK 0x00000f00
#define ERR_APT_0001_1 0x00000100
#define ERR_APT_0001_2 0x00000200
#define ERR_APT_0001_3 0x00000300
#define ERR_APT_0001_4 0x00000400
#define ERR_APT_0001_5 0x00000500
#define ERR_APT_0001_6 0x00000600

int apt_p0001(
  struct APT_PTR_STR *apt
)
{
  int err_code = 0; /* error code */
  int res; /* result code */
  int i,x; /* for general */

  int rank; /* my rank */
  int pno; /* number of process */
  int stg; /* stage number */

  int *buf; /* buffer for gather */

  stg = 0;
  res = acp_init(&apt->argc,&apt->argv);
  if(res < 0) {err_code=ERR_APT_0001_1;goto E_END;}
  rank = acp_rank();
  if(rank < 0) {err_code=ERR_APT_0001_3;goto E_END;}
  apt->id=rank;
  pno = acp_procs();
  if(pno < 0) {err_code=ERR_APT_0001_4;goto E_END;}

  // stage 0: procs match test
  if(rank == 0) {buf = malloc(pno*sizeof(int));}
  res = apt_gather(&pno,buf,sizeof(int),0);
  if(res != 0) {err_code |= ERR_APT_0000_6;goto E_END;}

  // check illegal
  if(rank == 0)
  {
#ifdef APT_DEBUG
	printf("RBF : %08d stg %d pno %d",rank,stg,pno);
	for(i=0;i<pno;i++) {printf(" %08x",buf[i]);}
	printf("\n");
#endif
	for(i=1;i<pno;i++)  {
	  if(pno != buf[i]) {err_code=ERR_APT_0001_5;goto E_END;}
	}
	free(buf);
  }
  stg++;

  res = acp_finalize();
  if(res < 0) {err_code=ERR_APT_0001_2;goto E_END;}

N_END:
  if(rank == 0)
  {
	if(err_code == 0) {printf("STT: 0 PASS PRG %d\n",APT_PID_0001);}
	else {printf("STT: 1 FAIL PRG %d-%d\n",APT_PID_0001,stg);}
  }
  return err_code;

E_END:
  x = err_code | (APT_PID_0001<<APT_ERR_PRG_SFT);
  switch (err_code & APT_ERR_EL1_MSK)
  {
  case ERR_APT_0001_1:
	printf("E%08ud : STG %03d code %08x acp_init\n",-1,stg,x);
	break;
  case ERR_APT_0001_2:
	printf("E%08ud : STG %03d code %08x acp_finalize\n",rank,stg,x);
	break;
  case ERR_APT_0001_3:
	printf("E%08ud : STG %03d code %08x acp_rank\n",-1,stg,x);
	break;
  case ERR_APT_0001_4:
	printf("E%08ud : STG %03d code %08x acp_procs\n",rank,stg,x);
	break;
  case ERR_APT_0001_5:
	printf("E%08ud : STG %03d code %08x illegal rank\n",rank,stg,x);
	break;
  }

  switch (err_code & APT_ERR_EL1_MSK)
  {
  case ERR_APT_0001_1:
  case ERR_APT_0001_2:
	printf("STL: 1 FAIL PRG %d-%d\n",0,stg);
	abort();

  case ERR_APT_0001_3:
  case ERR_APT_0001_4:
  case ERR_APT_0001_5:
	printf("STL: 1 FAIL PRG %d-%d\n",0,stg);
	acp_abort(NULL);
	abort();
  }

  acp_finalize();
  err_code = x;
  goto N_END;
}
