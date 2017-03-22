#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <acp.h>

int value;
int result;

acp_ga_t get_dst_ga(void *ptr)
{
  acp_atkey_t key;
  key = acp_register_memory(ptr, sizeof(int), 0);
  return acp_query_ga(key, ptr);
}

acp_ga_t get_src_ga(int rank, void *ptr)
{
  void *p;
  acp_atkey_t key;
  acp_ga_t addrbuf_local;

  if(rank == 0){
    addrbuf_local  = acp_query_starter_ga(0);
    p = acp_query_address(addrbuf_local);
    return *(acp_ga_t*)p;
  }else{
    key = acp_register_memory(ptr, sizeof(int), 0);
    return acp_query_ga(key, ptr);
  }
}

void xfer_src_to_rank0(acp_ga_t src)
{
  acp_handle_t h;
  acp_ga_t srcbuf_local, srcbuf_remote;
  void *p;
  srcbuf_local  = acp_query_starter_ga(1);
  srcbuf_remote = acp_query_starter_ga(0);
  p = acp_query_address(srcbuf_local);
  *((acp_ga_t*)p) = src;

  h = acp_copy(srcbuf_remote, srcbuf_local, sizeof(acp_ga_t), ACP_HANDLE_NULL);
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

  acp_sync();

  acp_handle_t h;
  acp_ga_t src, dst;
  if(rank == 0){
    result = 0xdeadbeef;
    dst = get_dst_ga(&result);

    acp_sync();

    src = get_src_ga(rank, NULL);

    h = acp_add4(dst, src, 42, ACP_HANDLE_NULL);
    acp_complete(h);

    printf("rank0: result = %x\n", result);
  }else{
    value = 0xcafebabe;
    src = get_src_ga(rank, &value);
    xfer_src_to_rank0(src);

    acp_sync();

    while(value != 0xcafebabe + 42){};

    printf("rank1: value = %x\n", value);
  }

  printf("rank%d: finalize\n", rank);
  fflush(stdout);

  acp_finalize();
  return 0;
}
