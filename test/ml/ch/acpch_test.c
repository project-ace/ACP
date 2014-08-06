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
    req[0] = acp_nbsend(ch[0], &a, sizeof(int));
    acp_wait(req[0]);
    req[1] = acp_nbrecv(ch[1], &b, sizeof(int));
    acp_wait(req[1]);
  }
  //  sleep(5);
  //  acp_sync();
    if (rank == 1){
      a = 0;
      a = 200;
      req[0] = acp_nbrecv(ch[0], &a, sizeof(int));
    acp_wait(req[0]);
      req[1] = acp_nbsend(ch[1], &b, sizeof(int));
    acp_wait(req[1]);
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
    req[1] = acp_nbfree_ch(ch[1]);

    acp_wait(req[0]);
    acp_wait(req[1]);
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
  

