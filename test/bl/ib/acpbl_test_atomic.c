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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <acp.h>

size_t iacp_starter_memory_size_dl = 0;
size_t iacp_starter_memory_size_cl = 0;

int main(int argc, char **argv){
  
    int myrank;/* my rank ID */
    int trank1;/* target rank ID */
    int nprocs;/* # of procs */
    acp_ga_t myga, tga1;/* ga of my rank, ga of target rank*/
    acp_ga_t mygmga, tgmga1, tgmga2;/* ga of my rank, ga of target rank*/
    acp_ga_t *sm;/* starter memory address */
    acp_handle_t handle;
    int rc; 
    int *mygm;
    acp_atkey_t key;
    int color = 0;
    uint32_t u32data, u32data2;
    
    /* initialization */
    rc = acp_init(&argc, &argv);
    if (rc == -1) exit(-1);
    
    myrank = acp_rank();
    nprocs = acp_procs();
    printf("myrank %d nprocs %d\n", myrank, nprocs);

    trank1 = (myrank + 1) % nprocs;
    tga1 = acp_query_starter_ga(trank1);
    myga = acp_query_starter_ga(myrank);
    
    sm = (acp_ga_t *)acp_query_address(myga);
    printf("rank %d myga %lx %p\n", myrank, myga, sm);
    mygm = malloc(sizeof(int) * 2);
    
    key = acp_register_memory(mygm, sizeof(int) * 2, color);
    mygmga = acp_query_ga(key, mygm);
    
    printf("my rank = %d mygmga = %lx\n", myrank, mygmga);
    sm[0] = mygmga;
    
    acp_sync();
    
    printf("rank %d sm[0] %lx\n", myrank, sm[0]);
    handle = acp_copy(myga + sizeof(acp_ga_t), tga1,  sizeof(acp_ga_t), ACP_HANDLE_NULL );
    printf("finish acp_copy sm\n");
    
    acp_complete(handle);
    acp_sync();
    printf("rank %d: sm[0] %lx, sm[1] %lx\n", myrank, sm[0], sm[1]);

    tgmga1 = sm[1];
    tgmga2 = sm[0];
  
    u32data = 100;
    mygm[1] = 1111;
    mygm[0] = 100;

    acp_sync();
    
    if (myrank == 1) {
        printf("rank %d trank1 %d tgmga1 %lx tgmga2 %lx myga %lx sm %p\n",
	       myrank, trank1, tgmga1, tgmga2, myga, sm);
	
	//acp_add4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	handle = acp_add4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	printf("finish acp_copy ga\n");
	
	acp_complete(handle);
    } 
    
    /* synchronization */
    acp_sync();
    printf("rank %d myga[0] %d myga[1] %d\n", myrank, mygm[0], mygm[1]);  
    
    u32data = 100;
    u32data2 = 10000;
    
    mygm[1] = 1111;
    mygm[0] = 100;
    
    acp_sync();
    
    if (myrank == 1) {
      
	printf("rank %d trank1 %d tgmga1 %lx tgmga2 %lx myga %lx sm %p\n",
	       myrank, trank1, tgmga1, tgmga2, myga, sm);
	
	//acp_cas4(tgmga2 + sizeof(int), tgmga1, u32data, u32data2, ACP_HANDLE_NULL );
	handle = acp_cas4(tgmga2 + sizeof(int), tgmga1, u32data, u32data2, ACP_HANDLE_NULL );
	printf("finish acp_copy ga\n");
	
	acp_complete(handle);
    } 
    
    /* synchronization */
    acp_sync();
    printf("rank %d myga[0] %d myga[1] %d\n", myrank, mygm[0], mygm[1]);  
    
    u32data = 50;
    
    mygm[1] = 1111;
    mygm[0] = 100;

    acp_sync();
    
    if (myrank == 1) {
	
	printf("rank %d trank1 %d tgmga1 %lx tgmga2 %lx myga %lx sm %p\n",
	       myrank, trank1, tgmga1, tgmga2, myga, sm);
	
	//acp_swap4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	handle = acp_swap4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	printf("finish acp_copy ga\n");
	
	acp_complete(handle);
    } 
    
    /* synchronization */
    acp_sync();
    printf("rank %d myga[0] %d myga[1] %d\n", myrank, mygm[0], mygm[1]);  
    u32data = 100;

    mygm[1] = 1111;
    mygm[0] = 100;
    acp_sync();
    
    if(myrank == 1) {
	
	printf("rank %d trank1 %d tgmga1 %lx tgmga2 %lx myga %lx sm %p\n",
	       myrank, trank1, tgmga1, tgmga2, myga, sm);
	
	//acp_or4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	handle = acp_or4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	printf("finish acp_copy ga\n");
	
	acp_complete(handle);
    } 
    
    /* synchronization */
    acp_sync();
    printf("rank %d myga[0] %d myga[1] %d\n", myrank, mygm[0], mygm[1]);  
    
    u32data = 100;
    mygm[1] = 1111;
    mygm[0] = 100;

    acp_sync();
    
    if(myrank == 1) {
	
	printf("rank %d trank1 %d tgmga1 %lx tgmga2 %lx myga %lx sm %p\n",
	       myrank, trank1, tgmga1, tgmga2, myga, sm);
	
	//acp_xor4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	handle = acp_xor4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	printf("finish acp_copy ga\n");
	
	acp_complete(handle);
    } 
    /* synchronization */
    acp_sync();
    printf("rank %d myga[0] %d myga[1] %d\n", myrank, mygm[0], mygm[1]);  
    
    u32data = 100;
    mygm[1] = 1111;
    mygm[0] = 100;
    
    acp_sync();
    
    if(myrank == 1) {
	
	printf("rank %d trank1 %d tgmga1 %lx tgmga2 %lx myga %lx sm %p\n",
	       myrank, trank1, tgmga1, tgmga2, myga, sm);
	
	//acp_and4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	handle = acp_and4(tgmga2 + sizeof(int), tgmga1, u32data, ACP_HANDLE_NULL );
	printf("finish acp_copy ga\n");
	
	acp_complete(handle);
    } 
    /* synchronization */
    acp_sync();
    printf("rank %d myga[0] %d myga[1] %d\n", myrank, mygm[0], mygm[1]);  
    
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

