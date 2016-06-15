
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "acp.h"

int main(int argc, char** argv)
{
  int rank, procs, data, i, r, c;
  int *recv_data;
  char msg[200], buf[10000000];
  acp_ch_t ch;
  acp_request_t req;
  int rep = 1;

  acp_init(&argc, &argv);
  procs = acp_procs();
  rank = acp_rank();

  recv_data = (int *)calloc(procs, sizeof(int));  
  
  while ((c = getopt(argc, argv, "r:")) != -1){
	switch(c){
	case 'r': 
	  rep = atoi(optarg);
	  break;
	default:
	  fprintf(stderr, "usage: %s [-r num]\n", argv[0]);
	}
  }
/* from proc.i to proc.0 */
  if(rank != 0){
	ch = acp_create_ch(rank, 0 );
	for (r = 0; r < rep; r++){
	  data = rank * 10000 + r;
	  req = acp_nbsend_ch(ch, &data, sizeof(int));
	  acp_wait_ch(req);
	  fprintf(stderr, "rank %02d | send   to rank 00 : %d &send_data    : %p\n", rank, data, &data);
	}
	req = acp_nbfree_ch(ch);
	acp_wait_ch(req);

  }else{

	for( i=1; i<procs; i++){
		ch = acp_create_ch(i, 0 );   
		for (r = 0; r < rep; r++){
			req = acp_nbrecv_ch(ch, &recv_data[i], sizeof(int));
			acp_wait_ch(req);
			if (recv_data[i] != (i*10000 + r)){
				sprintf(msg, "rank: 0 Wrong data %d (should be %d) i = %d\n",				    recv_data[i], i*10000+r, i);
			}else{
				sprintf(msg, "rank %02d | recv from rank %02d : %d &recv_data[%02d]: %p \n", rank, i, recv_data[i], i, &recv_data[i]);
			}
			fprintf(stderr, msg);
		}
		req = acp_nbfree_ch(ch);
		acp_wait_ch(req);
	}

  }
  free(recv_data);
  fflush(stderr);
  acp_sync();
  acp_finalize();


  return 0;
}
