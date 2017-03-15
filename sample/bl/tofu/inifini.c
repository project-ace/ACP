#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "acp.h"
#include "acpbl.h"

int main(int argc, char **argv)
{
  int ret = acp_init(&argc, &argv);
  if(ret != 0){
    printf("init failed: [%d]\n", ret);
  }
  int size, rank;
  rank = acp_rank();

  acp_sync();
  printf("rank = %d\n", rank);
  fflush(stdout);

  acp_finalize();
  return 0;
}
