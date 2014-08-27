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

    int i; /* general index */
    int myrank; /* my rank ID */
    int torank; /* target rank ID */
    int nprocs; /* # of procs */
    acp_ga_t myga, toga; /* ga of my rank, ga of target rank*/
    acp_ga_t *sm; /* starter memory address */
    acp_handle_t handle; /* acp handle */
    int rc; /* return code */
    char *mygm[256]; /* pointer my global memory */
    acp_atkey_t key[256];/* acp key pointer */
    acp_ga_t mygmga, togmga; /* my and target ga */
    int color = 0; /* color = 0 */
    
    /* initialization */
    rc = acp_init(&argc, &argv);
    if (rc == -1) exit(-1);
  
    /* get myrank */
    myrank = acp_rank();
    /* get # of processes */
    nprocs = acp_procs();
    printf("myrank %d nprocs %d\n", myrank, nprocs);
    
    myga = acp_query_starter_ga(myrank);
    
    torank = (myrank + 1) % nprocs;
    toga = acp_query_starter_ga(torank);
  
    sm = (acp_ga_t *)acp_query_address(myga);
    printf("rank %d toga %lx myga %lx %p\n", myrank, toga, myga, sm);
    
    for(i=0;i<255;i++){
	mygm[i] = malloc(sizeof(char) * 2);
    }
    for(i=0;i<255;i++){
	key[i] = acp_register_memory(mygm[i], sizeof(char) * 2, color);
    }

    /* first phase */
    printf("++++++++++++++++++++\n");
    mygmga = acp_query_ga(key[0], mygm[0]);
    printf("1:my rank = %d mygmga = %lx\n", myrank, mygmga);
    sm[0] = mygmga;
    
    acp_sync();
    printf("1:rank %d sm[0] %lx\n", myrank, sm[0]);
    handle = acp_copy(myga + sizeof(acp_ga_t), toga, sizeof(acp_ga_t), ACP_HANDLE_NULL );
    printf("1:finish acp_copy\n");
    acp_complete(handle);
    acp_sync();

    printf("1:rank %d sm[1] %lx\n", myrank, sm[1]);
    togmga = sm[1];
    printf("1:rank %d togmaga %lx\n", myrank, togmga);
    
    mygm[0][0] = 'a' + myrank;
    
    mygm[0][1] = 'x';
    printf("1:rank %d mygm[0][0] %c\n", myrank, mygm[0][0]);
    acp_sync();
    
    handle = acp_copy(togmga + sizeof(char), mygmga, sizeof(char), ACP_HANDLE_NULL);
    acp_complete(handle);
    acp_sync();
    printf("1:rank %d mygm[0][1] %c\n", myrank, mygm[0][1]);
    
    /* scond phase */
    printf("++++++++++++++++++++\n");
    acp_unregister_memory(key[0]); 
    mygm[255] = malloc(sizeof(char) * 2);
    key[255] = acp_register_memory(mygm[255], sizeof(char) * 2, color);
    mygmga = acp_query_ga(key[255], mygm[255]);
    sm[0] = mygmga;
    
    acp_sync();
    printf("2:rank %d sm[0] %lx\n", myrank, sm[0]);
    handle = acp_copy(myga + sizeof(acp_ga_t), toga, sizeof(acp_ga_t), ACP_HANDLE_NULL );
    printf("2:finish acp_copy\n");
    acp_complete(handle);
    acp_sync();
    
    printf("2:rank %d sm[1] %lx\n", myrank, sm[1]);
    togmga = sm[1];
    printf("2:rank %d sm[1] %lx\n", myrank, togmga);
    
    mygm[255][0] = 'A' + myrank;
    
    mygm[255][1] = 'X';
    printf("2:rank %d mygm[255][0] %c\n", myrank, mygm[255][0]);
    acp_sync();
    
    handle = acp_copy(togmga + sizeof(char), mygmga, sizeof(char), ACP_HANDLE_NULL);
    acp_complete(handle);
    acp_sync();
    printf("2:rank %d mygm[255][1] %c\n", myrank, mygm[255][1]);
    
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
