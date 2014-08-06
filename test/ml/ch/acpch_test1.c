/* acpci_test1.c
 *
 * test one direction message passing 
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
#include "acpbl.h"
#include "acpci.h"
#include "acpbl_sync.h"

int main(int argc, char** argv)
{
  int rank, a;
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
    a = 100;
    req = acp_nbsend(ch, &a, sizeof(int));
  }
  //  sleep(5);
  //  acp_sync();
    if (rank == 1){
      a = 0;
      req = acp_nbrecv(ch, &a, sizeof(int));
    }
    //  acp_sync();
  fflush(stdout);  

    acp_wait(req);
  fflush(stdout);  

  if (rank == 1){
      printf("got %d\n", a);
  }

  fflush(stdout);  

  acp_sync();

  printf("%d sync 0 done\n", rank);
  fflush(stdout);  


  if ((rank == 1) || (rank == 0)){
    req = acp_nbfree_ch(ch);

    acp_wait(req);
  }
  


  acp_sync();

  printf("%d sync 1 done\n", rank);
  fflush(stdout);  

  acp_finalize();
  printf("%d finalize done\n", rank);
  fflush(stdout);  

}

int iacp_init_ds(void) { return 0; };
int iacp_init_vd(void) { return 0; };
int iacp_finalize_ds(void) { return 0; };
int iacp_finalize_vd(void) { return 0; };
void iacp_abort_ds(void) { return; };
void iacp_abort_vd(void) { return; };
  

