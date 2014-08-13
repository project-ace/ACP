#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include"acpbl.h"

size_t iacp_starter_memory_size_dl = 0;
size_t iacp_starter_memory_size_cl = 0;
size_t iacp_starter_memory_size_vd = 0;

int main(int argc, char **argv){

    int myrank; /* my rank ID */
    int torank; /* target rank ID */
    int nprocs; /* # of procs */
    acp_ga_t myga, toga; /* ga of my rank, ga of target rank*/
    acp_ga_t *sm; /* starter memory address */
    acp_handle_t handle, handle2; /* acp handles */
    int rc; /* return code */
    char *mygm; /* my global memory */
    acp_atkey_t key; /* acp atkey */
    acp_ga_t mygmga, togmga; /* ga of global memory */
    int color = 0; /* color 0 */
    
    /* initialization */
    rc = acp_init(&argc, &argv);
    if(rc == -1) exit(-1);
    
    myrank = acp_rank();
    nprocs = acp_procs();
    printf("myrank %d nprocs %d\n", myrank, nprocs);
    
    myga = acp_query_starter_ga(myrank);
    
    torank = (myrank + 1) % nprocs;
    toga = acp_query_starter_ga(torank);
    
    sm = (acp_ga_t *)acp_query_address(myga);
    printf("rank %d toga %lx myga %lx %p\n", myrank, toga, myga, sm);
    
    mygm = malloc(sizeof(char) * 4);
    key = acp_register_memory(mygm, sizeof(char) * 4, color);
    mygmga = acp_query_ga(key, mygm);
    printf("my rank = %d mygmga = %lx\n", myrank, mygmga);
    sm[0] = mygmga;
    
    mygm[0] = 'a' + myrank;  
    mygm[1] = 'x';
    mygm[2] = 'y';
    
    acp_sync();
    printf("rank %d sm[0] %lx\n", myrank, sm[0]);
    
    handle = acp_copy(toga + sizeof(acp_ga_t), 
		      myga, sizeof(acp_ga_t), ACP_HANDLE_NULL );
    acp_complete(handle);
    acp_sync();
    togmga = sm[1];
    printf("rank %d sm[1] %lx togmga %lx\n", myrank, sm[1], togmga);
    printf("finish acp_copy\n");  
    acp_sync();
    
    handle = acp_copy(mygmga + sizeof(char), togmga, sizeof(char), ACP_HANDLE_NULL);
    handle2 = acp_copy(mygmga + sizeof(char) * 2, mygmga + sizeof(char), sizeof(char), handle);
    acp_complete(handle2);
    acp_sync();
    printf("rank %d mygm %c %c %c\n", myrank, mygm[0], mygm[1], mygm[2]);
    
    /* finalization */
    acp_finalize();
    
    return 0;
}
