#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "acp.h"

int main(int argc, char** argv)
{
  int rank, procs, data, i, r, c;
  int rep = 1;
  char msg[60], buf[100000];
  acp_ch_t ch;
  acp_request_t req;
  
  acp_init(&argc, &argv);
  procs = acp_procs();
  rank = acp_rank();

  while ((c = getopt(argc, argv, "r:")) != -1){
	switch(c){
	case 'r': 
	  rep = atoi(optarg);
	  break;
	default:
	  fprintf(stderr, "usage: %s [-r num]\n", argv[0]);
	}
  }
  fprintf(stderr, "rank %d: r=%d-------------\n", rank, rep);
  /* from proc.0 to proc.i */
  if(rank == 0){
    for( i=1; i<procs; i++){
		ch = acp_create_ch( 0, i );   
		for (r = 0; r < rep; r++){
			data = i * 10000 + r;
			req = acp_nbsend_ch(ch, &data, sizeof(int));
			acp_wait_ch(req);
		}
		req = acp_nbfree_ch(ch);
		fprintf(stderr, "rank %2d to rank %2d |data: %6d |&data: %p\n", rank, i, data, &data);
	}
  }else{

    ch = acp_create_ch( 0, rank );
	for (r = 0; r < rep; r++){
		data = 0;
		req = acp_nbrecv_ch(ch, &data, sizeof(int));
		acp_wait_ch(req);
		if (data != (rank * 10000 + r)){
			fprintf(stderr, "rank: %d Wrong data %d (should be %d) i = %d\n", 
				  rank, data, rank*10000+r, rank);
		}
	}
    fprintf(stderr, "rank %2d received   |data: %6d |&data: %p\n", rank, data, &data);
	req = acp_nbfree_ch(ch);
    acp_wait_ch(req);

  }
  fflush(stderr);
  acp_sync();
  acp_finalize();
  return 0;
}
