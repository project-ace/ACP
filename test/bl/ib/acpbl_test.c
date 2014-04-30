#include"acpbl.h"

int main(int argc, char **argv){
  
  int myrank;/* my rank ID */
  int torank;/* target rank ID */
  int nprocs;/* # of procs */
  acp_ga_t myga, toga;/* ga of my rank, ga of target rank*/
  acp_ga_t *sm;/* starter memory address */
  acp_handle_t handle;
  int rc;
  char *mygm;
  acp_atkey_t key;
  acp_ga_t mygmga, togmga;
  
  /* initialization */
  rc = acp_init(&argc, &argv);
  if(rc == -1) exit(-1);
  
  myrank = acp_rank();
  nprocs = acp_procs();
  printf("myrank %d\n", myrank);
  
  myga = acp_query_starter_ga(myrank);
  
  torank = (myrank + 1) % nprocs;
  toga = acp_query_starter_ga(torank);
  
  sm = (acp_ga_t *)acp_query_address(myga);
  printf("rank %d toga %lx myga %lx %p\n", myrank, toga, myga, sm);
 
  mygm = malloc(sizeof(char)*2);
  key = acp_register_memory(mygm, sizeof(char)*2, 0);
  mygmga = acp_query_ga(key, mygm);
  printf("my rank = %d mygmga = %lx\n", myrank, mygmga);
  sm[0] = mygmga;
  
  acp_sync();
  printf("rank %d sm[0] %lx\n", myrank, sm[0]);
  //handle = acp_copy(toga + sizeof(acp_ga_t), myga, sizeof(acp_ga_t), 0 );
  handle = acp_copy(myga + sizeof(acp_ga_t), toga, sizeof(acp_ga_t), 0 );
  printf("finish acp_copy\n");
 
  acp_complete(handle);
  acp_sync();
  
  printf("rank %d sm[1] %lx\n", myrank, sm[1]);
  togmga = sm[1];
  printf("rank %d sm[1] %lx\n", myrank, togmga);

  mygm[0] = 'a' + myrank;
  
  mygm[1] = 'x';
  printf("rank %d mygm[0] %c\n", myrank, mygm[0]);
  acp_sync();
  
  handle = acp_copy(togmga + sizeof(char), mygmga, sizeof(char), 0);
  acp_complete(handle);
  acp_sync();
  printf("rank %d mygm[1] %c\n", myrank, mygm[1]);
  
  /* finalization */
  acp_finalize();
  
  return 0;
}
