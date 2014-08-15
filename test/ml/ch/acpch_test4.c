/* acpci_test4.c
 *
 * test one direction message passing (rendezvous)
 *
 *  rank 0
 *    ch = create_ch (0,1)
 *    req = nbsend(ch)
 *    wait(req)
 *    req = nbfree(ch) 
 *    wait(req)
 * 
 *  rank 1
 *    ch = create_ch (0,1)
 *    req = nbrecv(ch)
 *    wait(req)
 *    req = nbfree(ch) 
 *    wait(req)
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
  int rank, a[1024], i;
  acp_ch_t ch;
  acp_request_t req;
  
  acp_init(&argc, &argv);

  fflush(stdout);  
  rank = acp_rank();

  if ((rank == 1) || (rank == 0)){
    ch = acp_create_ch(0, 1);
  fflush(stdout);  
  }
  if (rank == 0){
    for (i = 0; i < 1024; i++){
	a[i] = i;
      }
    req = acp_nbsend_ch(ch, a, sizeof(int)*1024);
  }
  //  sleep(5);
  //  acp_sync();
    if (rank == 1){
      for (i = 0; i < 1024; i++){
	a[i] = 0;
      }
      req = acp_nbrecv_ch(ch, a, sizeof(int)*1024);
    }
    //  acp_sync();
  fflush(stdout);  

    acp_wait_ch(req);
  fflush(stdout);  

  if (rank == 1){
      printf("got %d\n", a[1023]);
  }

  fflush(stdout);  

  acp_sync();

  printf("%d sync 0 done\n", rank);
  fflush(stdout);  


  if ((rank == 1) || (rank == 0)){
    req = acp_nbfree_ch(ch);

    acp_wait_ch(req);
  }
  


  acp_sync();

  printf("%d sync 1 done\n", rank);
  fflush(stdout);  

  acp_finalize();
  printf("%d finalize done\n", rank);
  fflush(stdout);  

}

