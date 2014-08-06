#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acpbl.h"
#include "acpbl_sync.h"

//#define MHZ 2933.333
#define MHZ 2266.667

#define NUM 5
#define REP 16

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

size_t iacp_starter_memory_size_ds = 1024;
size_t iacp_starter_memory_size_ch = 1024;
size_t iacp_starter_memory_size_vd = 1024;

int iacp_init_ds(void) { return 0; };
int iacp_init_ch(void) { return 0; };
int iacp_init_vd(void) { return 0; };
int iacp_finalize_ds(void) { return 0; };
int iacp_finalize_ch(void) { return 0; };
int iacp_finalize_vd(void) { return 0; };
void iacp_abort_ds(void) { return; };
void iacp_abort_ch(void) { return; };
void iacp_abort_vd(void) { return; };

int old_main(int argc, char** argv)
{
  int rank, procs;
  acp_ga_t ga[NUM * REP];
  unsigned char* buf;
  uint32_t* array;
  uint64_t t0, t1;
  int i, j;
  
  acp_init(&argc, &argv);
  
  rank = acp_rank();
  procs = acp_procs();
  
  for (i = 0; i < REP; i++) ga[i] = acp_query_starter_ga((rank + (procs - 1) * i) % procs);
  buf = acp_query_address(ga[0]);
  for (i = 0; i < NUM; i++) buf[i] = 65 + (rank % 26);
  buf[NUM*REP] = 0;
  
  for (j = 0; j < REP; j++) {
    for (i = NUM; i < NUM * REP; i++) buf[i] = '.';
    acp_sync();
    t0 = get_clock();
    for (i = 0; i < j; i++) acp_copy(ga[i + 1] + NUM * (i + 1), ga[i] + NUM * i, NUM, ACP_HANDLE_ALL);
    acp_complete(ACP_HANDLE_ALL);
    t1 = get_clock();
    acp_sync();
    printf("rank %5d time %12.3f usec - %s\n", rank, (t1 - t0)/MHZ, buf);
  }
  
  for (i = 0; i < REP; i++) ga[i] = acp_query_starter_ga((rank + i) % procs);
  array = acp_query_address(ga[0]);
  
  array[0] = 11111000 * (rank + 1);
  array[1] = (rank + 1) * 10;
  acp_sync();
  t0 = get_clock();
  for (i = 0; i < 10; i++) acp_add4(ga[0] + 4, acp_query_starter_ga(0), 1, ACP_HANDLE_ALL);
  acp_complete(ACP_HANDLE_ALL);
  t1 = get_clock();
  acp_sync();
  printf("rank %5d time %12.3f usec - %10u %10u\n", rank, (t1 - t0)/MHZ, array[0], array[1]);
  
  array[0] = 11110000 * (rank + 1);
  array[1] = (rank + 1) * 100;
  acp_sync();
  t0 = get_clock();
  for (i = 0; i < 10; i++) acp_add4(ga[1] + 4, acp_query_starter_ga(1), 1, ACP_HANDLE_ALL);
  acp_complete(ACP_HANDLE_ALL);
  t1 = get_clock();
  acp_sync();
  printf("rank %5d time %12.3f usec - %10u %10u\n", rank, (t1 - t0)/MHZ, array[0], array[1]);
  
  array[0] = 11100000 * (rank + 1);
  array[1] = (rank + 1) * 1000;
  acp_sync();
  t0 = get_clock();
  for (i = 0; i < 10; i++) acp_add4(ga[2] + 4, acp_query_starter_ga(2), 1, ACP_HANDLE_ALL);
  acp_complete(ACP_HANDLE_ALL);
  t1 = get_clock();
  acp_sync();
  printf("rank %5d time %12.3f usec - %10u %10u\n", rank, (t1 - t0)/MHZ, array[0], array[1]);
  
  acp_finalize();
  
  return 0;
}

