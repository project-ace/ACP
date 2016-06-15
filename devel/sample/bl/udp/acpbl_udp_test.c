/*
 * ACP Basic Layer test program for UDP
 * 
 * Copyright (c) 2014-2014 FUJITSU LIMITED
 * Copyright (c) 2014      Kyushu University
 * Copyright (c) 2014      Institute of Systems, Information Technologies
 *                         and Nanotechnologies 2014
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>
#include "acpbl_sync.h"

//#define MHZ 2933.333 //hana Xeon 5160
#define MHZ 2266.667 //rx200 Xeon E5520
//#define MHZ 2200.000 //hx(tofu) Opteron 8354

int settings[][2] = {
  {       1, 1000 },
  {       2, 1000 },
  {       4, 1000 },
  {       8, 1000 },
  {      16, 1000 },
  {      32, 1000 },
  {      64, 1000 },
  {     128, 1000 },
  {     256, 1000 },
  {     512, 1000 },
  {    1024, 1000 },
  {    2048, 1000 },
  {    4096, 1000 },
  {    8192, 1000 },
  {   16384, 1000 },
  {   32768, 1000 },
  {   65536,  640 },
  {  131072,  320 },
  {  262144,  160 },
  {  524288,   80 },
  { 1048576,   40 },
  { 2097152,   20 },
  { 4194304,   10 },
  {      -1,    0 },
};

int main(int argc, char** argv)
{
  int rank;
  acp_ga_t ga[3];
  uint64_t t0, t1;
  int i, j, b, r;
  
//  setbuf(stdout, NULL);
  
  acp_init(&argc, &argv);
  
  rank = acp_rank();
  
  if (rank == 0) {
    ga[0] = acp_query_starter_ga(0);
    ga[1] = acp_query_starter_ga(1);
    ga[2] = acp_query_starter_ga(2);
    
    printf("# Parallel   Local to     Remote Copy\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
    for (i = 0; settings[i][0] >= 0; i++) {
      b = settings[i][0];
      r = settings[i][1];
      acp_copy(ga[1], ga[0], b, ACP_HANDLE_NULL);
      acp_complete(ACP_HANDLE_ALL);
      t0 = get_clock();
      for (j = 0; j < r; j++) {
        acp_copy(ga[1], ga[0], b, ACP_HANDLE_NULL);
      }
      acp_complete(ACP_HANDLE_ALL);
      t1 = get_clock();
      printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
    }
    
    printf("# Sequential Local to     Remote Copy\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
    for (i = 0; settings[i][0] >= 0; i++) {
      b = settings[i][0];
      r = settings[i][1];
      acp_copy(ga[1], ga[0], b, ACP_HANDLE_ALL);
      acp_complete(ACP_HANDLE_ALL);
      t0 = get_clock();
      for (j = 0; j < r; j++) {
        acp_copy(ga[1], ga[0], b, ACP_HANDLE_ALL);
      }
      acp_complete(ACP_HANDLE_ALL);
      t1 = get_clock();
      printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
    }
    
    printf("# Parallel   Remote to    Local Copy\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
    for (i = 0; settings[i][0] >= 0; i++) {
      b = settings[i][0];
      r = settings[i][1];
      acp_copy(ga[0], ga[1], b, ACP_HANDLE_NULL);
      acp_complete(ACP_HANDLE_ALL);
      t0 = get_clock();
      for (j = 0; j < r; j++) {
        acp_copy(ga[0], ga[1], b, ACP_HANDLE_NULL);
      }
      acp_complete(ACP_HANDLE_ALL);
      t1 = get_clock();
      printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
    }
    
    printf("# Sequential Remote to    Local Copy\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
    for (i = 0; settings[i][0] >= 0; i++) {
      b = settings[i][0];
      r = settings[i][1];
      acp_copy(ga[0], ga[1], b, ACP_HANDLE_ALL);
      acp_complete(ACP_HANDLE_ALL);
      t0 = get_clock();
      for (j = 0; j < r; j++) {
        acp_copy(ga[0], ga[1], b, ACP_HANDLE_ALL);
      }
      acp_complete(ACP_HANDLE_ALL);
      t1 = get_clock();
      printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
    }
    
    printf("# Parallel   Remote to    Remote Copy\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
    for (i = 0; settings[i][0] >= 0; i++) {
      b = settings[i][0];
      r = settings[i][1];
      acp_copy(ga[2], ga[1], b, ACP_HANDLE_NULL);
      acp_complete(ACP_HANDLE_ALL);
      t0 = get_clock();
      for (j = 0; j < r; j++) {
        acp_copy(ga[2], ga[1], b, ACP_HANDLE_NULL);
      }
      acp_complete(ACP_HANDLE_ALL);
      t1 = get_clock();
      printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
    }
    
    printf("# Sequential Remote to    Remote Copy\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
    for (i = 0; settings[i][0] >= 0; i++) {
      b = settings[i][0];
      r = settings[i][1];
      acp_copy(ga[2], ga[1], b, ACP_HANDLE_ALL);
      acp_complete(ACP_HANDLE_ALL);
      t0 = get_clock();
      for (j = 0; j < r; j++) {
        acp_copy(ga[2], ga[1], b, ACP_HANDLE_ALL);
      }
      acp_complete(ACP_HANDLE_ALL);
      t1 = get_clock();
      printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (MHZ*b*r)/(t1 - t0));
    }
    
    printf("# Sequential Local to    Remote to    Local Copy\n       #bytes #repetitions      t[usec]   Mbytes/sec\n");
    for (i = 0; settings[i][0] >= 0; i++) {
      b = settings[i][0];
      r = settings[i][1];
      acp_copy(ga[1], ga[0], b, ACP_HANDLE_ALL);
      acp_copy(ga[0], ga[1], b, ACP_HANDLE_ALL);
      acp_complete(ACP_HANDLE_ALL);
      t0 = get_clock();
      for (j = 0; j < r; j++) {
        acp_copy(ga[1], ga[0], b, ACP_HANDLE_ALL);
        acp_copy(ga[0], ga[1], b, ACP_HANDLE_ALL);
      }
      acp_complete(ACP_HANDLE_ALL);
      t1 = get_clock();
      printf("%13d%13d%13.3f%13.3f\n", b, r, (t1 - t0)/MHZ/r, (2.0*MHZ*b*r)/(t1 - t0));
    }
  }
  acp_finalize();
  
  return 0;
}

