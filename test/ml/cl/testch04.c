#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "acp.h"

#define MSG_LENGTH 100

int main(int argc, char** argv)
{
	int rank, procs, i, j, r, c;
	int rep, len = 1;
	char  *data;
	char msg[1024], buf[16*100*1024];
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
			 if (len < 1){
				 len = 1;
			 }
			 break;
		 default:
			 fprintf(stderr, "usage: %s [-r num] [-l num]\n", argv[0]);
		}
	}
	/* send proc.0 to other procs */
	sprintf(msg, "rank %d: r=%d, l=%d--------------------\n", rank, rep, len);
	strcat(buf,msg);
	if(rank == 0){
		for( i=1; i<procs; i++){
			data = (char *)calloc(len+1, sizeof(char));
			ch = acp_create_ch( 0, i );
			for (r = 0; r < rep; r++){
				sprintf(data, "%0.*d", len, r );
				req = acp_nbsend_ch(ch, data, len);
				acp_wait_ch(req);
			}
			req = acp_nbfree_ch(ch);
			sprintf(msg, "rank %02d to   rank %02d |data|%s |&data: %p\n", rank, i, data, data);
			strcat(buf,msg);
			free(data);
		}
	}else{
		/* data = ""; */
		data = (char *)calloc(len, sizeof(char));
		ch = acp_create_ch( 0, rank );
		for (r = 0; r < rep; r++){
			req = acp_nbrecv_ch(ch, data, len);
			acp_wait_ch(req);
			sprintf(msg, "rank %02d from rank 00 |data|%s |&data: %p\n", rank, data, data);
			strcat(buf, msg);
		}
		req = acp_nbfree_ch(ch);
		free(data);
  }
	fputs(buf, stderr);
	acp_finalize();
  return 0;
}
