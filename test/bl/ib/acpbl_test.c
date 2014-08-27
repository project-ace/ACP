/*
 * ACP Basic Layer test program for InfiniBand
 * 
 * Copyright (c) 2014-2014 Kyushu University
 * Copyright (c) 2014      Institute of Systems, Information Technologies
 *                         and Nanotechnologies 2014
 * Copyright (c) 2014      FUJITSU LIMITED
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 *
 */
#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<acp.h>

size_t iacp_starter_memory_size_dl = 0;
size_t iacp_starter_memory_size_cl = 0;

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
    int color = 0;
    
    /* initialization */
    rc = acp_init(&argc, &argv);
    if (rc == -1) exit(-1);
    
    myrank = acp_rank();
    nprocs = acp_procs();
    printf("myrank %d nprocs %d\n", myrank, nprocs);
    
    myga = acp_query_starter_ga(myrank);
    
    torank = (myrank + 1) % nprocs;
    toga = acp_query_starter_ga(torank);
    
    sm = (acp_ga_t *)acp_query_address(myga);
    printf("rank %d toga %lx myga %lx %p\n", myrank, toga, myga, sm);
    
    mygm = malloc(sizeof(char)*2);
    key = acp_register_memory(mygm, sizeof(char)*2, color);
    mygmga = acp_query_ga(key, mygm);
    printf("my rank = %d mygmga = %lx\n", myrank, mygmga);
    sm[0] = mygmga;
    
    acp_sync();
    printf("rank %d sm[0] %lx\n", myrank, sm[0]);
    //handle = acp_copy(toga + sizeof(acp_ga_t), myga, sizeof(acp_ga_t), ACP_HANDLE_NULL );
    handle = acp_copy(myga + sizeof(acp_ga_t), toga, sizeof(acp_ga_t), ACP_HANDLE_NULL );
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
    
    handle = acp_copy(togmga + sizeof(char), mygmga, sizeof(char), ACP_HANDLE_NULL);
    acp_complete(handle);
    acp_sync();
    printf("rank %d mygm[1] %c\n", myrank, mygm[1]);
    
    /* finalization */
    acp_finalize();
  
    return 0;
}

int iacp_init_dl(){return 0;}
int iacp_init_cl(){return 0;}
int iacp_finalize_dl(){return 0;}
int iacp_finalize_cl(){return 0;}
void iacp_abort_cl(){return;}
void iacp_abort_dl(){return;}
