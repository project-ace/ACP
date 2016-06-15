#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "acp.h"

//#define MSG_LENGTH 10

int main(int argc, char** argv)
{
	int rank, procs, i, r,c;
	int rep, len = 1;
	char *send_data, *recv_data;
	acp_ch_t ch;
	acp_request_t req;

	acp_init(&argc, &argv);
	procs = acp_procs();
	rank = acp_rank();

	while ((c = getopt(argc, argv, "l:r:")) != -1){
		switch(c){
		 case 'r':
			 rep = atoi(optarg);
			 break;
		 case 'l':
			 len = atoi(optarg);
			 if( len < 1){
				 len = 1;
			 }
			 break;
		default:
			 fprintf(stderr, "usage: %s [-r num] [-l num]\n", argv[0]);
		}
	}

	/* from proc.* to proc 0 */
	if(rank != 0){
		send_data = (char *)calloc(len, sizeof(char));
        ch = acp_create_ch(rank, 0 );
		for ( r=0; r<rep; r++){
			sprintf(send_data, "%-d", r );
  			req = acp_nbsend_ch(ch, send_data, len );
			acp_wait_ch(req);
		}
		req = acp_nbfree_ch(ch);
		fprintf(stderr, "rank %d to rank 0 | send_data: %s &send_data: %p\n", rank, send_data, send_data); 
		free(send_data);
	}else{
		for( i=1; i<procs; i++){
			recv_data = (char *)calloc(len, sizeof(char));
			ch = acp_create_ch(i, 0 );   
			for(r=0; r < rep ;r++){
				req = acp_nbrecv_ch(ch, recv_data, len );
				acp_wait_ch(req);
			}
		 	req = acp_nbfree_ch(ch);
			fprintf(stderr, "rank %d | recv_data: %d bytes|%s| &recv_data: %p \n", i, sizeof(recv_data), recv_data,   recv_data); 
			free(recv_data);
		}
  }
  acp_wait_ch(req);
  acp_finalize();
  return 0;
}
