#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <xos_sft/xos_tofu.h>
#include "tofu.h"
#include "acp.h"
#include "acpbl.h"
#define HOST_NAME_MAX 255
extern int tlib_glfd[44];
extern int jobid;
#define TNICQ(tni, cq) (((cq)<<(2))|(tni))

/*void handler(int signal) {
  //ioctl(TOF_GET_ERROR)
  printf("signal=%d\n", signal);
  fflush(stdout);
  }*/
void handler(int signal, siginfo_t *info, void *ctx)
{
  int tni, ret;
  uint64_t rv=0;

  printf("signo = %d, errno = %d, int = %d\n", info->si_signo, info->si_errno, info->si_int);

  for(tni=0; tni<4; tni++){
    ret = ioctl(tlib_glfd[TNICQ(tni, 1)], TOF_GET_ERROR, &rv);
    printf("tni%d: fd=%d, ret = %d, bitmap = 0x%lx\n", tni, tlib_glfd[TNICQ(tni, 1)], ret, rv);
    fflush(stdout);
    rv = 0;
  }

  exit(-1);
}

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
  setbuf(stdout, NULL);

  char buf[HOST_NAME_MAX];
  gethostname(buf, HOST_NAME_MAX);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_ONESHOT | SA_SIGINFO;
    sigaction(SIGTERM, &sa, NULL);

  int ret = acp_init(&argc, &argv);
  if(ret != 0){
    printf("init failed: [%d]\n", ret);
  }
  int size, rank;
  rank = acp_rank();
  size = acp_procs();

  acp_sync();

  struct tlib_physical_addr myaddr;
  tlib_query_6d_phys_addr(jobid, 0, rank, &myaddr);

  printf("rank = %d, size=%d, hostname=%s, addr=(%d,%d,%d,%d,%d,%d)\n",
	 rank,       size,    buf,         myaddr.x, myaddr.y, myaddr.z, myaddr.a, myaddr.b, myaddr.c);
  if(size < 3){
    printf("The size must be larger than 2.\n");
    exit(-1);
  }

  acp_sync();

  acp_handle_t h;
  acp_ga_t src, dst;

  if(rank == 0){
    sleep(1);
    acp_sync();

    dst = get_dst_ga(rank, NULL);
    printf("got dst!!!\n");fflush(stdout);
    src = get_src_ga(rank, NULL);
    printf("got src!!!\n");fflush(stdout);

    h = acp_copy(dst, src, sizeof(int), 0);
    printf("issued copy!!!\n");fflush(stdout);

    acp_complete(h);
    printf("cooooompleeeeete!!!\n");fflush(stdout);

    sleep(1);
    acp_sync();

  }else if(rank == 1){
    srcbuf = 42;
    src = get_src_ga(rank, &srcbuf);
    printf("got src!!!\n");fflush(stdout);
    send_ga_to_rank0(src, rank);
    printf("sent src to rank0!!!\n");fflush(stdout);

    sleep(1);
    acp_sync();
    /* do nothing */
    sleep(1);
    acp_sync();


  }else{ // rank = 2
    dstbuf = 0xbaadf00d;
    dst = get_dst_ga(rank, &dstbuf);
    printf("got dst!!!\n");fflush(stdout);
    send_ga_to_rank0(dst, rank);
    printf("sent dst to rank0!!!\n");fflush(stdout);

    sleep(1);
    acp_sync();
    /* do nothing */
    sleep(1);
    acp_sync();

    printf("rank2: dstbuf = %d\n", dstbuf);
  }

  printf("rank%d: finalize\n", rank);
  fflush(stdout);

  acp_finalize();
  return 0;
}
