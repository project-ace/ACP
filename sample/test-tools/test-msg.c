#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"
#include "msg.h"

#define N 10
#define BUF_SIZ 1024

//extern void show_all_msg();

typedef struct XLA_STR
{
  acp_ga_t hga;
  acp_ga_t bga;
  size_t bsz;
} xla_t;

typedef struct HND_STR
{
  acp_atkey_t xkey;
  xla_t *xla;
  acp_ga_t xga;
  acp_ga_t bga;
  size_t bsz;
} hnd_t;

int main(int argc, char *argv[])
{
    int myrank, procs, dst;
    int i;
    int *a, ret;
    acp_ga_t localga, remotega;
    acp_atkey_t localkey;

    acp_ga_t xga;
    acp_atkey_t xkey;

    size_t tx;

    xla_t sbuf;
    xla_t rbuf;

    hnd_t *hnd;

    char *buf;

    acp_init(&argc, &argv);

    procs = acp_procs ( ) ;
    myrank = acp_rank ( ) ;

    //a = (int *)malloc(N*sizeof(int));
    //    localkey = acp_register_memory(a, N*sizeof(int), 0);
    //    localga = acp_query_ga(localkey, a);
    buf = (char *)malloc(BUF_SIZ);
    localkey = acp_register_memory(buf,BUF_SIZ,0);
    localga = acp_query_ga(localkey,buf);

    hnd = (hnd_t *)malloc(sizeof(hnd_t));
    hnd->xla = (xla_t *)malloc(sizeof(xla_t));
    hnd->xkey = acp_register_memory(hnd->xla,sizeof(xla_t),0);
    hnd->xga = acp_query_ga(hnd->xkey,hnd->xla);
    hnd->bga = localga;
    hnd->bsz = BUF_SIZ;

    msg_setup();
    acp_sync();

    for(i = 0;i<2;i++)
    {
      switch(myrank)
      {
      case 2:
        dst = 3;
        break;
      case 3:
        dst = 2;
        break;
      }
      switch(myrank)
      {
      case 2:
      case 3:
        tx = sizeof(acp_ga_t)+sizeof(acp_ga_t)+sizeof(size_t);

        ret = -1;
        while(ret == -1)
        {
          printf("%d : start push\n",myrank);fflush(stdout);
          ret = msg_push(dst, 0, &hnd->xga, tx);
          printf("%d : end push %d\n",myrank,ret);fflush(stdout);
        }

        ret = -1;
        while (ret == -1)
        {
          printf("%d : start pop\n",myrank);fflush(stdout);
          ret = msg_pop(dst, 0, hnd->xla, tx);
          printf("%d : end pop %d\n",myrank,ret);fflush(stdout);
        }
        
        printf("%d: lxga %p lbga %p rxga %p rbga %p rbsz %d\n"
               , myrank,hnd->xga,hnd->bga
               ,hnd->xla->hga,hnd->xla->bga,hnd->xla->bsz);
      }

      acp_sync();
    }

    msg_free();

    acp_finalize();

}
