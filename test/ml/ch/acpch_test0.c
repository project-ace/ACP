/* acpci_test0.c
 *
 * test init, sync and finalize (without channels)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acpbl.h"

int main(int argc, char** argv)
{
  int rank, a;
  acp_ch_t ch;
  acp_request_t req;
  
  acp_init(&argc, &argv);
  rank = acp_rank();

  acp_sync();

  printf("%d sync 1 done\n", rank);
  fflush(stdout);  

  acp_finalize();
  printf("%d finalize done\n", rank);
  fflush(stdout);  

}


int iacp_init_dl(void) { return 0; };
int iacp_init_ds(void) { return 0; };
int iacp_init_vd(void) { return 0; };
int iacp_finalize_dl(void) { return 0; };
int iacp_finalize_ds(void) { return 0; };
int iacp_finalize_vd(void) { return 0; };
void iacp_abort_dl(void) { return; };
void iacp_abort_ds(void) { return; };
void iacp_abort_vd(void) { return; };
size_t iacp_starter_memory_size_dl = 1024;
size_t iacp_starter_memory_size_cl = 1024;
size_t iacp_starter_memory_size_vd = 1024;

