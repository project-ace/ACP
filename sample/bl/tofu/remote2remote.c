#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "acp.h"
#include "acpbl.h"

int dstbuf __attribute__((aligned(256)));
int srcbuf __attribute__((aligned(256)));

acp_ga_t get_dst_ga(int rank, void *ptr)
{
  acp_atkey_t key;
  acp_ga_t *ga_array;

  if(rank == 0){
    ga_array = (acp_ga_t*)acp_query_address(acp_query_starter_ga(0));
    return ga_array[2];
  }else{
    key = acp_register_memory(ptr, sizeof(int), 0);
    return acp_query_ga(key, ptr);
  }
}

acp_ga_t get_src_ga(int rank, void *ptr)
{
  acp_atkey_t key;
  acp_ga_t *ga_array;

  if(rank == 0){
    ga_array = (acp_ga_t*)acp_query_address(acp_query_starter_ga(0));
    return ga_array[1];
  }else{
    key = acp_register_memory(ptr, sizeof(int), 0);
    return acp_query_ga(key, ptr);
  }
}

void send_ga_to_rank0(acp_ga_t ga, int rank)
{
  acp_handle_t h;
  acp_ga_t gabuf_local, gabuf_remote;
  acp_ga_t *ga_array;

  gabuf_local  = acp_query_starter_ga(rank);
  gabuf_remote = acp_query_starter_ga(0);
  ga_array = (acp_ga_t*)acp_query_address(gabuf_local);
  ga_array[0] = ga;

  h = acp_copy(gabuf_remote + sizeof(acp_ga_t)*rank, gabuf_local, sizeof(acp_ga_t), ACP_HANDLE_NULL);
  acp_complete(h);
}

int main(int argc, char **argv)
{
  int ret = acp_init(&argc, &argv);
  if(ret != 0){
    printf("init failed: [%d]\n", ret);
  }
  int size, rank;
  rank = acp_rank();
  size = acp_procs();

  acp_sync();

  printf("rank = %d, size=%d\n", rank, size);
  if(size != 3){
    printf("The size must be 3.\n");
    exit(-1);
  }

  acp_sync();

  acp_handle_t h;
  acp_ga_t src, dst;

  if(rank == 0){

    acp_sync();

    dst = get_dst_ga(rank, NULL);
    src = get_src_ga(rank, NULL);

    h = acp_copy(dst, src, sizeof(int), 0);
    acp_complete(h);

    acp_sync();

  }else if(rank == 1){
    srcbuf = 42;
    src = get_src_ga(rank, &srcbuf);
    send_ga_to_rank0(src, rank);

    acp_sync();
    /* do nothing */
    acp_sync();


  }else{ // rank = 2
    dstbuf = 0xbaadf00d;
    dst = get_dst_ga(rank, &dstbuf);
    send_ga_to_rank0(dst, rank);

    acp_sync();
    /* do nothing */
    acp_sync();

    printf("rank2: dstbuf = %d\n", dstbuf);
  }

  printf("rank%d: finalize\n", rank);
  fflush(stdout);

  acp_finalize();
  return 0;
}
