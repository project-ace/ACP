#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include"acpbl.h"

int main(int argc, char **argv){
  
  int myrank;/* my rank ID */
  int trank1, trank2;/* target rank ID */
  int nprocs;/* # of procs */
  acp_ga_t myga, tga1, tga2;/* ga of my rank, ga of target rank*/
  int *sm;/* starter memory address */
  acp_handle_t handle;
  int rc;
  int color = 0;
  
  /* initialization */
  rc = acp_init(&argc, &argv);
  if(rc == -1) exit(-1);
  
  myrank = acp_rank();
  nprocs = acp_procs();
  printf("myrank %d nprocs %d\n", myrank, nprocs);
  
  myga = acp_query_starter_ga(myrank);
  sm = (int *)acp_query_address(myga);
  sm[0] =  myrank;
  
  if(myrank == 1){   
    trank1 = (myrank + 1) % nprocs;
    trank2 = (myrank + 2) % nprocs; 
    
    tga1 = acp_query_starter_ga(trank1);
    tga2 = acp_query_starter_ga(trank2);
    
    printf("rank %d trank1 %d trank2 %d tga1 %lx tga2 %lx myga %lx %p\n",
	   myrank, trank1, trank2, tga1, tga2, myga, sm);
    
    printf("rank %d sm[0] %lx\n", myrank, sm[0]);
    handle = acp_copy(tga2 + sizeof(int), tga1, sizeof(int), ACP_HANDLE_NULL );
    printf("finish acp_copy\n");
    
    acp_complete(handle);
  } 
  
  /* synchronization */
  acp_sync();
  printf("rank %d sm[0] %d sm[1] %d\n", myrank, sm[0], sm[1]);  
  
  /* finalization */
  acp_finalize();
  
  return 0;
}
