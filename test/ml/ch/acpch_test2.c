/*
 * ACP Middle Layer: Channel Interface test program
 * 
 * Copyright (c) 2014-2014 Kyushu University
 * Copyright (c) 2014      Institute of Systems, Information Technologies
 *                         and Nanotechnologies 2014
 * Copyright (c) 2014      FUJITSU LIMITED
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 *
 */
/* acpci_test2.c
 *
 * test bi-direction message passing 
 *
 *  rank 0
 *    ch0 = create_ch (0,1)
 *    ch1 = create_ch (1, 0)
 *    req0 = nbsend(ch0)
 *    req1 = nbrecv(ch1)
 *    wait(req0)
 *    wait(req1)
 *    req0 = nbfree(ch0) 
 *    req1 = nbfree(ch1) 
 *    wait(req0)
 *    wait(req1)
 *
 *  rank 1
 *    ch0 = create_ch (0, 1)
 *    ch1 = create_ch (1, 0)
 *    req0 = nbrecv(ch0)
 *    req1 = nbsend(ch1)
 *    wait(req0)
 *    wait(req1)
 *    req0 = nbfree(ch0) 
 *    req1 = nbfree(ch1) 
 *    wait(req0)
 *    wait(req1)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"

int main(int argc, char** argv)
{
  int rank, a, b;
  acp_ch_t ch[2];
  acp_request_t req[2];
  
  acp_init(&argc, &argv);

  fflush(stdout);  
  rank = acp_rank();

  if (rank < 2){
    ch[0] = acp_create_ch(0, 1);
    ch[1] = acp_create_ch(1, 0);
  fflush(stdout);  
  }
  if (rank == 0){
    a = 100;
    b = 0;
    req[0] = acp_nbsend_ch(ch[0], &a, sizeof(int));
    acp_wait_ch(req[0]);
    req[1] = acp_nbrecv_ch(ch[1], &b, sizeof(int));
    acp_wait_ch(req[1]);
  }
  //  sleep(5);
  //  acp_sync();
    if (rank == 1){
      a = 0;
      b = 200;
      req[0] = acp_nbrecv_ch(ch[0], &a, sizeof(int));
    acp_wait_ch(req[0]);
      req[1] = acp_nbsend_ch(ch[1], &b, sizeof(int));
    acp_wait_ch(req[1]);
    }
    //  acp_sync();
  fflush(stdout);  

  if (rank < 2){
    printf("%d: a %d b %d\n", rank, a, b);
    fflush(stdout);  
  }


  acp_sync();

  printf("%d sync 0 done\n", rank);
  fflush(stdout);  


  if (rank < 2){
    req[0] = acp_nbfree_ch(ch[0]);
    acp_wait_ch(req[0]);
  }
  acp_sync();

  printf("%d sync 1 done\n", rank);
  fflush(stdout);  

  if (rank < 2){
    req[1] = acp_nbfree_ch(ch[1]);

    acp_wait_ch(req[1]);
  }
  
  acp_sync();


  printf("%d sync 2 done\n", rank);
  fflush(stdout);  

  acp_finalize();
  printf("%d finalize done\n", rank);
  fflush(stdout);  

  return 0;
}

