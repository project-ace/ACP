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
#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>
#include "acp_tp.h"

#ifdef TSYNC
#include "mpi_time.h"
#endif

/* include tree generation test */
/*
#define APT_TST_TREE_GENERATE
*/

/* max size = 1GB */
#define APT_CPY_SZX (1<<30)
#define APT_CPY_OFX (APT_CPY_SZX-1)

/* error code */
#define ERR_APT_0020_1 0x00000100
#define ERR_APT_0020_2 0x00000200
#define ERR_APT_0020_5 0x00000500
#define ERR_APT_0020_6 0x00000600
#define ERR_APT_0020_7 0x00000700
#define ERR_APT_0020_8 0x00000800
#define ERR_APT_0020_9 0x00000900
#define ERR_APT_0020_10 0x00000a00
#define ERR_APT_0020_11 0x00000b00
#define ERR_APT_0020_12 0x00000c00
#define ERR_APT_0020_13 0x00000d00
#define ERR_APT_0020_14 0x00000e00

/* copy rank 0 starter buffer data to rank 1 starter buffer top */
int apt_p0020(
  struct APT_PTR_STR *apt
)
{
  int err_code = 0; /* error code */
  int res; /* result code */
  int x;

  int rank; /* my rank */
  int pno; /* number of process */
  int stg; /* stage number */

  int drnk; /* destination rank */
  acp_ga_t ssg; /* source starter memory ga */
  acp_ga_t dsg; /* destination starter memory ga */
  char *sml; /* starter memory address */
  acp_handle_t hnd;

  char *dsm; /* destination starter memory address */

  stg = 0;
  res = apt_ip(apt, &rank, &pno);
  if(res != 0) {err_code |= ERR_APT_0020_1;goto E_END;}

  /* stage 0: simple test */
  /* rank 0 : write to 1 */
  switch (rank)
  {
  case 0:
    /* set destination rank */
    if(pno == 1) {drnk = 0;} else {drnk = 1;}
    
    /* get source/destination rank starter memory ga */
    ssg = acp_query_starter_ga(rank);
    if(ssg == ACP_GA_NULL) {err_code=ERR_APT_0020_5;goto E_END;}
    dsg = acp_query_starter_ga(drnk);
    if(dsg == ACP_GA_NULL) {err_code=ERR_APT_0020_6;goto E_END;}

    /* convert to local memory address */
    sml = (char *)acp_query_address(ssg);
    if(sml == NULL) {err_code=ERR_APT_0020_14;goto E_END;}

    /* buffer initialize */
    sml[0]=0x01; /* source */
    sml[1]=0x00; /* refference */
    res = acp_sync(); /* wait buffer initialization of destination */
    if(res != 0) {err_code=ERR_APT_0020_13;goto E_END;}

    /* copy my starter memory to destination starter memory */
    hnd = acp_copy(dsg, ssg, sizeof(char), ACP_HANDLE_NULL );
    if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_0020_7;goto E_END;}
    acp_complete(hnd);
    /* copy destination starter memory to my starter memory(reffernce) */
    hnd = acp_copy(ssg + sizeof(char), dsg, sizeof(char), ACP_HANDLE_NULL );
    if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_0020_8;goto E_END;}
    acp_complete(hnd);

    /* data compare */
    if(sml[1] != 0x01) {err_code=ERR_APT_0020_9;goto E_END;}
    break;

  case 1:
    /* get destination rank starter memory ga */
    dsg = acp_query_starter_ga(1);
    if(dsg == ACP_GA_NULL) {err_code=ERR_APT_0020_10;goto E_END;}
    /* convert to local memory address */
    dsm = (char *)acp_query_address(dsg);
    if(dsm == NULL) {err_code=ERR_APT_0020_11;goto E_END;}

    /* buffer initialize */
    dsm[0]=0x00; /* source */
    /* !!! no need break */
  default:
    res = acp_sync();
    if(res != 0) {err_code=ERR_APT_0020_12;goto E_END;}
    break;
  }

  stg++;
  res = acp_finalize();
  if(res != 0) {err_code=ERR_APT_0020_2;goto E_END;}

N_END:
  if(rank == 0)
  {
    if(err_code == 0) {printf("STT: 0 PASS PRG %d\n",APT_PID_0020);}
    else {printf("STT: 1 FAIL PRG %d-%d\n",APT_PID_0020,stg);}
  }
  return err_code;

E_END:
  x = err_code | (APT_PID_0020<<APT_ERR_PRG_SFT);
  switch (err_code)
  {
  case ERR_APT_0020_1:
	printf("E%08ud : STG %03d code %08x initial process\n",-1,stg,err_code);
	break;
  case ERR_APT_0020_2:
	printf("E%08ud : STG %03d code %08x acp_finalize\n",rank,stg,err_code);
	break;
  case ERR_APT_0020_5:
  case ERR_APT_0020_6:
  case ERR_APT_0020_10:
	printf("E%08ud : STG %03d code %08x query_starter_ga\n",rank,stg,err_code);
	break;
  case ERR_APT_0020_7:
  case ERR_APT_0020_8:
	printf("E%08ud : STG %03d code %08x acp_copy\n",rank,stg,err_code);
	break;
  case ERR_APT_0020_9:
	printf("E%08ud : STG %03d code %08x data compare\n",rank,stg,err_code);
	break;
  case ERR_APT_0020_11:
  case ERR_APT_0020_14:
	printf("E%08ud : STG %03d code %08x query_address\n",rank,stg,err_code);
	break;
  case ERR_APT_0020_12:
  case ERR_APT_0020_13:
	printf("E%08ud : STG %03d code %08x acp_sync\n",rank,stg,err_code);
	break;
  }

  switch (err_code)
  {
  case ERR_APT_0020_1:
  case ERR_APT_0020_2:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0020,stg);
	fflush(stdout);
    abort();

  case ERR_APT_0020_5:
  case ERR_APT_0020_6:
  case ERR_APT_0020_7:
  case ERR_APT_0020_8:
  case ERR_APT_0020_10:
  case ERR_APT_0020_11:
  case ERR_APT_0020_12:
  case ERR_APT_0020_13:
  case ERR_APT_0020_14:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0020,stg);
	fflush(stdout);
    acp_abort(NULL);
    abort();

  case ERR_APT_0020_9:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0020,stg);
    break;
  }
  acp_finalize();
  err_code = x;
  goto N_END;
}

/* acp_copy function test (offset change) */
/* src rank = 0,dst rank = 1 */
/* src/dst offset = {0,1,2,MAX-1,MAX} */
/* src/dst color = 0 */
/* size = 1 */
/* error code */
#define ERR_APT_0021_1 0x00000100
#define ERR_APT_0021_2 0x00000200
#define ERR_APT_0021_3 0x00000300
#define ERR_APT_0021_4 0x00000400
#define ERR_APT_0021_5 0x00000500
#define ERR_APT_0021_6 0x00000600
#define ERR_APT_0021_7 0x00000700
#define ERR_APT_0021_8 0x00000800
#define ERR_APT_0021_9 0x00000900
#define ERR_APT_0021_10 0x00000a00
#define ERR_APT_0021_11 0x00000b00
#define ERR_APT_0021_12 0x00000c00
#define ERR_APT_0021_13 0x00000d00
#define ERR_APT_0021_14 0x00000e00

int apt_p0021(
  struct APT_PTR_STR *apt
)
{
  int err_code = 0; /* error code */
  int res; /* result code */
  int i,j,x; /* for general */

  int rank; /* my rank */
  int pno; /* number of process */
  int stg; /* stage number */
  int ssi; /* start stage index i */
  int ssj; /* start stage index j */

  char *sbl; /* send buffer logical address */
  acp_atkey_t skey; /* send buffer registration key */
  acp_ga_t sbg; /* send buffer ga */

  char *rbl; /* receive buffer logical address */
  acp_atkey_t rkey; /* receive buffer registration key */
  acp_ga_t rbg; /* receive buffer ga */

  acp_ga_t rrg; /* remote receive buffer ga */

  int sbo; /* send buffer offset */
  int rro; /* remote receive buffer offset */

  acp_handle_t hnd;

  /* stage set */
  stg = apt->cfg->stg_sta;ssi=0;ssj=0;
  if(apt->cfg->stg_sta != 0)
  {
	ssi=stg/5;
	ssj=stg%5;
  }
  res = apt_ip(apt, &rank, &pno);
  if(res != 0) {err_code |= ERR_APT_0021_1;goto E_END;}
  printf("%d : pno=%d\n",rank,pno);

  /* rank 0 use send buffer */
  if(rank == 0) {
	/* send buffer make */
	/* malloc(local addr.) -> regist -> ga get */
    sbl = malloc(APT_CPY_SZX);
    if(sbl == NULL) {err_code = ERR_APT_0021_3;goto E_END;}
    skey = acp_register_memory(sbl,APT_CPY_SZX,0);
    if(skey == ACP_ATKEY_NULL) {err_code = ERR_APT_0021_4;goto E_END;}
    sbg = acp_query_ga(skey,sbl);
    if(sbg == ACP_GA_NULL) {err_code=ERR_APT_0021_5;goto E_END;}

	/* initialize send buffer */
	/* set stage number */
  }

  /* rank 0 and 1 use receive buffer */
  if(rank <= 1) {
	/* receive buffer make */
	/* malloc(local addr.) -> regist -> ga get */
    rbl = malloc(APT_CPY_SZX);
    if(rbl == NULL) {err_code = ERR_APT_0021_6;goto E_END;}
    rkey = acp_register_memory(rbl,APT_CPY_SZX,0);
    if(rkey == ACP_ATKEY_NULL) {err_code = ERR_APT_0021_7;goto E_END;}
    rbg = acp_query_ga(rkey,rbl);
    if(rbg == ACP_GA_NULL) {err_code=ERR_APT_0021_8;goto E_END;}

	/* initialize receive buffer */
	rbl[0]=0;
	if(rank == 1) {rbl[1]=rbl[2]=rbl[APT_CPY_OFX-1]=rbl[APT_CPY_OFX]=0;}
  }

  /* rank 0 get rank 1 receive buffer ga */
  res = apt_sndrcv_ga(1, rbg, 0, &rrg, 0);
  if(res != 0) {err_code |= ERR_APT_0021_9;goto E_END;}
  /* copy test, use rank 0 process */
  /* i : from , j : to */
  /* 0->0, 1->1, 2->2, 3->MAX-1, 4->MAX */
  if(rank == 0) {
	for(i = ssi;i < 5;i++) {
	  switch(i)
	  {
	  case 3: sbo = (APT_CPY_OFX-1);break;
	  case 4: sbo = APT_CPY_OFX;break;
	  default: sbo = i;break;
	  }

	  for(j = ssj;j < 5;j++) {
		switch(j)
		{
		case 3: rro = (APT_CPY_OFX-1);break;
		case 4: rro = APT_CPY_OFX;break;
		default: rro = j;break;
		}

#ifdef APT_VERBOSE
		printf("INF : %d stg %d sbo %d rro %d\n",rank,stg,sbo,rro);
#endif
		/* set send data */
		sbl[sbo]=(char)(stg+1);

		/* copy : rank 0 send buf. -> rank 1 receive buf. */
		hnd = acp_copy(rrg + rro, sbg + sbo, 1, ACP_HANDLE_NULL);
		if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_0021_10;goto E_END;}
		acp_complete(hnd);

		/* copy : rank 1 receive buf. -> rank 0 receive buf. */
		hnd = acp_copy(rbg, rrg + rro, 1, ACP_HANDLE_NULL);
		if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_0021_11;goto E_END;}
		acp_complete(hnd);

		/* data compare */
		if(rbl[0] != (char)(stg+1)) {err_code=ERR_APT_0021_12;goto E_END;}
		stg++;
	  }
	  ssj = 0; /* start from j=0 */
	}
  }
  else {stg+=25;}

  if(rank == 0) {
	res = acp_unregister_memory(skey);
	if(res != 0) {err_code=ERR_APT_0021_13;goto E_END;}
	free(sbl);
  }
  if(rank <= 1) {
	res = acp_unregister_memory(rkey);
	if(res != 0) {err_code=ERR_APT_0021_14;goto E_END;}
	free(rbl);
  }
  res = acp_finalize();
  if(res != 0) {err_code=ERR_APT_0021_2;goto E_END;}

N_END:
  if(rank == 0)
  {
    if(err_code == 0) {printf("STT: 0 PASS PRG %d\n",APT_PID_0021);}
    else {printf("STT: 1 FAIL PRG %d-%d\n",APT_PID_0021,stg);}
  }
  return err_code;

E_END:
  x = err_code | (APT_PID_0021<<APT_ERR_PRG_SFT);
  switch (err_code)
  {
  case ERR_APT_0021_1:
	printf("E%08ud : STG %03d code %08x initial process\n",-1,stg,err_code);
	break;
  case ERR_APT_0021_2:
	printf("E%08ud : STG %03d code %08x acp_finalize\n",rank,stg,err_code);
	break;
  case ERR_APT_0021_3:
  case ERR_APT_0021_6:
	printf("E%08ud : STG %03d code %08x malloc\n",rank,stg,err_code);
	break;
  case ERR_APT_0021_4:
  case ERR_APT_0021_7:
	printf("E%08ud : STG %03d code %08x acp_register_memory\n",rank,stg,err_code);
	break;
  case ERR_APT_0021_5:
  case ERR_APT_0021_8:
	printf("E%08ud : STG %03d code %08x acp_query_ga\n",rank,stg,err_code);
	break;
  case ERR_APT_0021_9:
	printf("E%08ud : STG %03d code %08x sndrcv_ga\n",rank,stg,err_code);
	break;
  case ERR_APT_0021_10:
  case ERR_APT_0021_11:
	printf("E%08ud : STG %03d code %08x acp_copy\n",rank,stg,err_code);
	break;
  case ERR_APT_0021_12:
	printf("E%08ud : STG %03d code %08x data compare\n",rank,stg,err_code);
	break;
  case ERR_APT_0021_13:
  case ERR_APT_0021_14:
	printf("E%08ud : STG %03d code %08x acp_unregister_memory\n",rank,stg,err_code);
	break;
  }

  switch (err_code)
  {
  case ERR_APT_0021_1:
  case ERR_APT_0021_2:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0021,stg);
    abort();

  case ERR_APT_0021_3:
  case ERR_APT_0021_4:
  case ERR_APT_0021_5:
  case ERR_APT_0021_6:
  case ERR_APT_0021_7:
  case ERR_APT_0021_8:
  case ERR_APT_0021_9:
  case ERR_APT_0021_10:
  case ERR_APT_0021_11:
  case ERR_APT_0021_13:
  case ERR_APT_0021_14:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0021,stg);
    acp_abort(NULL);
    abort();

  case ERR_APT_0021_12:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0021,stg);
    break;
  }
  acp_finalize();
  err_code = x;
  goto N_END;
}

/* acp_copy function test (node change) */
/* src/dst rank = {0,1,2,n} */
/* src/dst offset = 0 */
/* src/dst color = 0 */
/* size = 1 */
/* error code */
#define ERR_APT_0022_1 0x00000100
#define ERR_APT_0022_2 0x00000200
#define ERR_APT_0022_3 0x00000300
#define ERR_APT_0022_4 0x00000400
#define ERR_APT_0022_5 0x00000500
#define ERR_APT_0022_6 0x00000600
#define ERR_APT_0022_7 0x00000700
#define ERR_APT_0022_8 0x00000800
#define ERR_APT_0022_9 0x00000900
#define ERR_APT_0022_10 0x00000a00
#define ERR_APT_0022_11 0x00000b00
#define ERR_APT_0022_12 0x00000c00
#define ERR_APT_0022_13 0x00000d00
#define ERR_APT_0022_14 0x00000e00
#define ERR_APT_0022_15 0x00000f00
#define ERR_APT_0022_16 0x00001000

int apt_p0022(
  struct APT_PTR_STR *apt
)
{
  int err_code = 0; /* error code */
  int res; /* result code */
  int i,j,x; /* for general */

  int rank; /* my rank */
  int pno; /* number of process */
  int stg; /* stage number */

  char *sbl; /* send buffer logical address */
  acp_atkey_t skey; /* send buffer registration key */
  acp_ga_t sbg; /* send buffer ga */

  char *rbl; /* receive buffer logical address */
  acp_atkey_t rkey; /* receive buffer registration key */
  acp_ga_t rbg; /* receive buffer ga */

  int cmax; /* check index max. */
  int ssi; /* start stage index i */
  int ssj; /* start stage index j */

  int srnk; /* send buffer rank */
  int rrnk; /* receive buffer rank */

  acp_ga_t rsg; /* remote send buffer ga */
  acp_ga_t rrg; /* remote receive buffer ga */

  acp_handle_t hnd;

  stg = apt->cfg->stg_sta;
  res = apt_ip(apt, &rank, &pno);
  if(res != 0) {err_code |= ERR_APT_0022_1;goto E_END;}

  /* make send/receive buffer */
  /* send buffer make */
  /* malloc(local addr.) -> regist -> ga get */
  sbl = malloc(sizeof(char));
  if(sbl == NULL) {err_code = ERR_APT_0022_3;goto E_END;}
  skey = acp_register_memory(sbl,1,0);
  if(skey == ACP_ATKEY_NULL) {err_code = ERR_APT_0022_4;goto E_END;}
  sbg = acp_query_ga(skey,sbl);
  if(sbg == ACP_GA_NULL) {err_code=ERR_APT_0022_5;goto E_END;}

  /* initialize send buffer */
  /* set initial i/j index from stgge number */
  if(pno < 4) {cmax = pno;} else {cmax = 4;}
  ssi=0;ssj=0;
  if(stg != 0)
  {
	ssi=stg/cmax;
	ssj=stg%cmax;
  }

  /* receive buffer make */
  /* malloc(local addr.) -> regist -> ga get */
  rbl = malloc(sizeof(char));
  if(rbl == NULL) {err_code = ERR_APT_0022_6;goto E_END;}
  rkey = acp_register_memory(rbl,1,0);
  if(rkey == ACP_ATKEY_NULL) {err_code = ERR_APT_0022_7;goto E_END;}
  rbg = acp_query_ga(rkey,rbl);
  if(rbg == ACP_GA_NULL) {err_code=ERR_APT_0022_8;goto E_END;}

  /* initialize receive buffer */
  rbl[0]=0;

  /* copy test, use rank 0 process */
  /* i : from , j : to */
  /* 0->0, 1->1, 2->2, 3->MAX */
  for(i = ssi;i < cmax;i++) {
	switch(i)
	{
	case 3: srnk = (pno-1);break;
	default: srnk = i;break;
	}
	/* rank 0 get rank srnk send buffer ga */
	res = apt_sndrcv_ga(srnk, sbg, 0, &rsg, 0);
	if(res != 0) {err_code |= ERR_APT_0022_9;goto E_END;}
	for(j = ssj;j < cmax;j++) {
	  switch(j)
	  {
	  case 3: rrnk = (pno-1);break;
	  default: rrnk = j;break;
	  }

#ifdef APT_VERBOSE
	  if(rank == 0) {
		printf("INF : %d stg %d srnk %d rrnk %d\n",rank,stg,srnk,rrnk);
	  }
#endif
	  if(rank == srnk) {
		/* set send data */
		sbl[0]=(char)(stg+1);
	  }

#ifdef APT_DEBUG
	  printf("SBP : %08d stg %d sbl %d rbl %d\n",rank,stg,sbl[0],rbl[0]);
	  printf("GAP : %08d stg %d sbg %016lx rbg %016lx rsg %016lx rrg%016lx\n"
			 ,rank,stg,sbg,rbg,rsg,rrg);
#endif
	  /* rank 0 get rank rrnk receive buffer ga */
	  res = apt_sndrcv_ga(rrnk, rbg, 0, &rrg, 0);
	  if(res != 0) {err_code |= ERR_APT_0022_10;goto E_END;}
#ifdef APT_DEBUG
	  printf("SBA : %08d stg %d sbl %d rbl %d\n",rank,stg,sbl[0],rbl[0]);
	  printf("GAA : %08d stg %d sbg %016lx rbg %016lx rsg %016lx rrg%016lx\n"
			 ,rank,stg,sbg,rbg,rsg,rrg);
#endif

	  if(rank == 0) {
		/* copy : rank srnk send buf. -> rrnk receive buf. */
		hnd = acp_copy(rrg, rsg, 1, ACP_HANDLE_NULL);
		if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_0022_11;goto E_END;}
		acp_complete(hnd);

		/* copy : rank rrnk receive buf. -> rank 0 receive buf. */
		hnd = acp_copy(rbg, rrg, 1, ACP_HANDLE_NULL);
		if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_0022_12;goto E_END;}
		acp_complete(hnd);

		/* data compare */
		if(rbl[0] != (char)(stg+1)) {err_code=ERR_APT_0022_13;goto E_END;}
	  }
	  /* next send data set is available after send data copy */
	  res = acp_sync(); /* wait initialize */
	  if(res != 0) {err_code=ERR_APT_0022_16;goto E_END;}
	  stg++;
	}
	ssj = 0; /* start from j=0 */
  }

  res = acp_unregister_memory(skey);
  if(res != 0) {err_code=ERR_APT_0022_14;goto E_END;}
  res = acp_unregister_memory(rkey);
  if(res != 0) {err_code=ERR_APT_0022_15;goto E_END;}
  free(sbl);free(rbl);
  res = acp_finalize();
  if(res != 0) {err_code=ERR_APT_0022_2;goto E_END;}

N_END:
  if(rank == 0)
  {
    if(err_code == 0) {printf("STT: 0 PASS PRG %d\n",APT_PID_0022);}
    else {printf("STT: 1 FAIL PRG %d-%d\n",APT_PID_0022,stg);}
  }
  return err_code;

E_END:
  x = err_code | (APT_PID_0022<<APT_ERR_PRG_SFT);
  switch (err_code)
  {
  case ERR_APT_0022_1:
	printf("E%08ud : STG %03d code %08x initial process\n",-1,stg,err_code);
	break;
  case ERR_APT_0022_2:
	printf("E%08ud : STG %03d code %08x acp_finalize\n",rank,stg,err_code);
	break;
  case ERR_APT_0022_3:
  case ERR_APT_0022_6:
	printf("E%08ud : STG %03d code %08x malloc\n",rank,stg,err_code);
	break;
  case ERR_APT_0022_4:
  case ERR_APT_0022_7:
	printf("E%08ud : STG %03d code %08x acp_register_memory\n",rank,stg,err_code);
	break;
  case ERR_APT_0022_5:
  case ERR_APT_0022_8:
	printf("E%08ud : STG %03d code %08x acp_query_ga\n",rank,stg,err_code);
	break;
  case ERR_APT_0022_9:
  case ERR_APT_0022_10:
	printf("E%08ud : STG %03d code %08x sndrcv_ga\n",rank,stg,err_code);
	break;
  case ERR_APT_0022_11:
  case ERR_APT_0022_12:
	printf("E%08ud : STG %03d code %08x acp_copy\n",rank,stg,err_code);
	break;
  case ERR_APT_0022_13:
	printf("E%08ud : STG %03d code %08x data compare\n",rank,stg,err_code);
	break;
  case ERR_APT_0022_14:
  case ERR_APT_0022_15:
	printf("E%08ud : STG %03d code %08x acp_unregister_memory\n",rank,stg,err_code);
  case ERR_APT_0022_16:
	printf("E%08ud : STG %03d code %08x acp_sync\n",rank,stg,err_code);
	break;
  }

  switch (err_code)
  {
  case ERR_APT_0022_1:
  case ERR_APT_0022_2:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0022,stg);
    abort();

  case ERR_APT_0022_3:
  case ERR_APT_0022_4:
  case ERR_APT_0022_5:
  case ERR_APT_0022_6:
  case ERR_APT_0022_7:
  case ERR_APT_0022_8:
  case ERR_APT_0022_9:
  case ERR_APT_0022_10:
  case ERR_APT_0022_11:
  case ERR_APT_0022_12:
  case ERR_APT_0022_14:
  case ERR_APT_0022_15:
  case ERR_APT_0022_16:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0022,stg);
    acp_abort(NULL);
    abort();

  case ERR_APT_0022_13:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0022,stg);
    break;
  }
  acp_finalize();
  err_code = x;
  goto N_END;
}

/* acp_copy function test (size change) */
/* src rank = 0,dst rank = 1 */
/* src/dst offset = 0 */
/* src/dst color = 0 */
/* size = {0,1,2,max-1,max} */
/* error code */
#define ERR_APT_0024_1 0x00000100
#define ERR_APT_0024_2 0x00000200
#define ERR_APT_0024_3 0x00000300
#define ERR_APT_0024_4 0x00000400
#define ERR_APT_0024_5 0x00000500
#define ERR_APT_0024_6 0x00000600
#define ERR_APT_0024_7 0x00000700
#define ERR_APT_0024_8 0x00000800
#define ERR_APT_0024_9 0x00000900
#define ERR_APT_0024_10 0x00000a00
#define ERR_APT_0024_11 0x00000b00
#define ERR_APT_0024_12 0x00000c00
#define ERR_APT_0024_13 0x00000d00
#define ERR_APT_0024_14 0x00000e00
#define ERR_APT_0024_15 0x00000f00

int apt_p0024(
  struct APT_PTR_STR *apt
)
{
  int err_code = 0; /* error code */
  int res; /* result code */
  int i,j,x; /* for general */

  int rank; /* my rank */
  int pno; /* number of process */
  int stg; /* stage number */

  char *sbl; /* send buffer logical address */
  acp_atkey_t skey; /* send buffer registration key */
  acp_ga_t sbg; /* send buffer ga */

  char *rbl; /* receive buffer logical address */
  acp_atkey_t rkey; /* receive buffer registration key */
  acp_ga_t rbg; /* receive buffer ga */

  acp_ga_t rrg; /* remote receive buffer ga */

  int sz; /* copy size */

  char dat; /* for initial value generation */

  acp_handle_t hnd;

  stg = apt->cfg->stg_sta;
  res = apt_ip(apt, &rank, &pno);
  if(res != 0) {err_code |= ERR_APT_0024_1;goto E_END;}

  /* rank 0 use send buffer */
  if(rank == 0) {
	/* send buffer make */
	/* malloc(local addr.) -> regist -> ga get */
    sbl = malloc(APT_CPY_SZX);
    if(sbl == NULL) {err_code = ERR_APT_0024_3;goto E_END;}
    skey = acp_register_memory(sbl,APT_CPY_SZX,0);
    if(skey == ACP_ATKEY_NULL) {err_code = ERR_APT_0024_4;goto E_END;}
    sbg = acp_query_ga(skey,sbl);
    if(sbg == ACP_GA_NULL) {err_code=ERR_APT_0024_5;goto E_END;}
  }

  /* rank 0 and 1 use receive buffer */
  if(rank <= 1) {
	/* receive buffer make */
	/* malloc(local addr.) -> regist -> ga get */
    rbl = malloc(APT_CPY_SZX);
    if(rbl == NULL) {err_code = ERR_APT_0024_6;goto E_END;}
    rkey = acp_register_memory(rbl,APT_CPY_SZX,0);
    if(rkey == ACP_ATKEY_NULL) {err_code = ERR_APT_0024_7;goto E_END;}
    rbg = acp_query_ga(rkey,rbl);
    if(rbg == ACP_GA_NULL) {err_code=ERR_APT_0024_8;goto E_END;}
	/* initialize receive buffer */
	for(i = 0;i < APT_CPY_SZX;i++) {rbl[i]=0;}
  }

  /* rank 0 get rank 1 receive buffer ga */
  res = apt_sndrcv_ga(1, rbg, 0, &rrg, 0);
  if(res != 0) {err_code |= ERR_APT_0024_9;goto E_END;}

  /* copy test, use rank 0 process */
  /* i : from , j : to */
  /* 0->0, 1->1, 2->2, 3->MAX-1, 4->MAX */
  for(i = stg;i < 5;i++) {
	switch(i)
	{
	case 3: sz = (APT_CPY_SZX-1);break;
	case 4: sz = APT_CPY_SZX;break;
	default: sz = i;break;
	}

#ifdef APT_VERBOSE
	if(rank == 0) {
	  printf("INF : %d stg %d sz %016x\n",rank,stg,sz);
	}
#endif
	/* initialize send buffer */
	if(rank == 0)
	{
	  dat=stg;
	  for(j=0;j<sz;j++) {
		if(dat==255) {dat=1;}else{dat++;};sbl[j]=dat;
	  }
	}
    res = acp_sync(); /* wait initialize */
    if(res != 0) {err_code=ERR_APT_0024_15;goto E_END;}

	if(rank == 0) {
	  /* copy : rank 0 send buf. -> rank 1 receive buf. */
	  hnd = acp_copy(rrg, sbg, sz, ACP_HANDLE_NULL);
	  if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_0024_10;goto E_END;}
	  acp_complete(hnd);

	  /* copy : rank 1 receive buf. -> rank 0 receive buf. */
	  hnd = acp_copy(rbg, rrg, sz, ACP_HANDLE_NULL);
	  if(hnd == ACP_HANDLE_NULL) {err_code=ERR_APT_0024_11;goto E_END;}
	  acp_complete(hnd);

	  /* data compare */
	  for(j = 0;j < sz;j++) {
		if(rbl[j] != sbl[j]) {
#ifdef APT_DEBUG
		  printf("DCE : %08d stg %d sz %d j %d rbl=%d sbl=%d\n"
				 ,rank,stg,sz,j,rbl[j],sbl[j]);
#endif
		  err_code=ERR_APT_0024_12;goto E_END;
		}
	  }
	}
	stg++;
  }

  if(rank == 0) {
	res = acp_unregister_memory(skey);
	if(res != 0) {err_code=ERR_APT_0024_13;goto E_END;}
	free(sbl);
  }
  if(rank <= 1) {
	res = acp_unregister_memory(rkey);
	if(res != 0) {err_code=ERR_APT_0024_14;goto E_END;}
	free(rbl);
  }
  res = acp_finalize();
  if(res != 0) {err_code=ERR_APT_0024_2;goto E_END;}

N_END:
  if(rank == 0) {
    if(err_code == 0) {printf("STT: 0 PASS PRG %d\n",APT_PID_0024);}
    else {printf("STT: 1 FAIL PRG %d-%d\n",APT_PID_0024,stg);}
  }
  return err_code;

E_END:
  x = err_code | (APT_PID_0024<<APT_ERR_PRG_SFT);
  switch (err_code)
  {
  case ERR_APT_0024_1:
	printf("E%08ud : STG %03d code %08x initial process\n",-1,stg,err_code);
	break;
  case ERR_APT_0024_2:
	printf("E%08ud : STG %03d code %08x acp_finalize\n",rank,stg,err_code);
	break;
  case ERR_APT_0024_3:
  case ERR_APT_0024_6:
	printf("E%08ud : STG %03d code %08x malloc\n",rank,stg,err_code);
	break;
  case ERR_APT_0024_4:
  case ERR_APT_0024_7:
	printf("E%08ud : STG %03d code %08x acp_register_memory\n",-1,stg,err_code);
	break;
  case ERR_APT_0024_5:
  case ERR_APT_0024_8:
	printf("E%08ud : STG %03d code %08x acp_query_ga\n",rank,stg,err_code);
	break;
  case ERR_APT_0024_9:
	printf("E%08ud : STG %03d code %08x sndrcv_ga\n",rank,stg,err_code);
	break;
  case ERR_APT_0024_10:
  case ERR_APT_0024_11:
	printf("E%08ud : STG %03d code %08x acp_copy\n",rank,stg,err_code);
	break;
  case ERR_APT_0024_12:
	printf("E%08ud : STG %03d code %08x data compare\n",rank,stg,err_code);
	break;
  case ERR_APT_0024_13:
  case ERR_APT_0024_14:
	printf("E%08ud : STG %03d code %08x acp_unregister_memory\n",rank,stg,err_code);
	break;
  case ERR_APT_0024_15:
	printf("E%08ud : STG %03d code %08x acp_sync\n",rank,stg,err_code);
	break;
  }

  switch (err_code)
  {
  case ERR_APT_0024_1:
  case ERR_APT_0024_2:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0024,stg);
    abort();

  case ERR_APT_0024_3:
  case ERR_APT_0024_4:
  case ERR_APT_0024_5:
  case ERR_APT_0024_6:
  case ERR_APT_0024_7:
  case ERR_APT_0024_8:
  case ERR_APT_0024_9:
  case ERR_APT_0024_10:
  case ERR_APT_0024_11:
  case ERR_APT_0024_13:
  case ERR_APT_0024_14:
  case ERR_APT_0024_15:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0024,stg);
    acp_abort(NULL);
    abort();

  case ERR_APT_0024_12:
    printf("STL: 1 FAIL PRG %d-%d\n",APT_PID_0024,stg);
    break;
  }
  acp_finalize();
  err_code = x;
  goto N_END;
}
