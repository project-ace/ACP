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

int main(int argc, char** argv)
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

