#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include "acpbl.h"
#include "acpbl_mms.h"
#include "acpbl_sync.h"
#include "acpbl_udp.h"
#include "acpbl_udp_gmm.h"
#include "acpbl_udp_gma.h"

int iacpbludp_init_gmm(void)
{
  FILE *fp;
  char s[4096];
  uint64_t start_addr, end_addr;
  uint64_t seg_top, seg_bottom;
  uint64_t seg_half, seg_full, bottom;
  int i;

  iacp_starter_memory_size_ch = ACPCI_STARTER_MEMORY_SIZE;
  
  BIT_RANK = 1;
  while ((1LLU << BIT_RANK) <= NUM_PROCS) BIT_RANK++;
  BIT_OFFSET = BIT_GA - BIT_SEG - BIT_RANK;
  MASK_RANK   = (1LLU << BIT_RANK) - 1;
  MASK_OFFSET = (1LLU << BIT_OFFSET) - 1;
  
  debug printf("rank %d - global address {rank[63:%d], seg[%d:%d], addr[%d:0]}\n", MY_RANK, BIT_SEG + BIT_OFFSET, BIT_SEG + BIT_OFFSET - 1, BIT_OFFSET, BIT_OFFSET - 1);
  
  SEGMENT[SEGST][0] = (uintptr_t)malloc(SMEM_SIZE);
  SEGMENT[SEGST][1] = SEGMENT[SEGST][0] + SMEM_SIZE;
  SEGMENT[SEGDS][0] = (uintptr_t)malloc(iacp_starter_memory_size_ds);
  SEGMENT[SEGDS][1] = SEGMENT[SEGDS][0] + iacp_starter_memory_size_ds;
  SEGMENT[SEGCH][0] = (uintptr_t)malloc(iacp_starter_memory_size_ch);
  SEGMENT[SEGCH][1] = SEGMENT[SEGCH][0] + iacp_starter_memory_size_ch;
  SEGMENT[SEGVD][0] = (uintptr_t)malloc(iacp_starter_memory_size_vd);
  SEGMENT[SEGVD][1] = SEGMENT[SEGVD][0] + iacp_starter_memory_size_vd;
  
  sprintf(s, "/proc/%d/maps", getpid());
  if((fp = fopen(s, "r")) == NULL){
    printf("Cannot open file %s.", s);
    return 1;
  }
  seg_top = 0LLU;
  seg_full = (1LLU << BIT_OFFSET);
  seg_half = (seg_full >> 1);
  bottom = 0LLU - seg_full;
  NUM_SEGMENT = 0;
  while (NUM_SEGMENT < SEGMAX && fgets(s, 4096, fp) != NULL){
    sscanf(s, "%llx-%llx", &start_addr, &end_addr);
    if(seg_top == 0){
      seg_top = start_addr;
      seg_bottom = end_addr;
    } else if (end_addr < seg_top + seg_half){
      seg_bottom = end_addr;
    } else {
      SEGMENT[NUM_SEGMENT][0] = (seg_bottom < seg_half) ? 0 : (seg_top > bottom) ? bottom : seg_bottom - seg_half;
      SEGMENT[NUM_SEGMENT][1] = SEGMENT[NUM_SEGMENT][0] + seg_full;
      NUM_SEGMENT++;
      seg_top = start_addr;
      seg_bottom = end_addr;
    }
  }
  if (seg_top != 0){
    SEGMENT[NUM_SEGMENT][0] = (seg_bottom < seg_half) ? 0 : (seg_top > bottom) ? bottom : seg_bottom - seg_half;
    SEGMENT[NUM_SEGMENT][1] = SEGMENT[NUM_SEGMENT][0] + seg_full;
    NUM_SEGMENT++;
  }
  fclose(fp);
  for (i = NUM_SEGMENT; i < SEGMAX; i++)
    SEGMENT[i][1] = SEGMENT[i][0] = 0xffffffffffffffffLLU;
#ifdef DEBUG
  printf("rank %d - num_segment %d\n", MY_RANK, NUM_SEGMENT);
  for (i = 0; i < 16; i++)
    printf("rank %d - segment[%2d] 0x%016llx - 0x%016llx\n", MY_RANK, i, SEGMENT[i][0], SEGMENT[i][1]);
#endif
  
  return 0;
}

int iacpbludp_finalize_gmm(void)
{
  free((uintptr_t*)SEGMENT[SEGVD][0]);
#ifdef DEBUG
  printf("end free segvd\n");
  fflush(stdout);
#endif
  free((uintptr_t*)SEGMENT[SEGCH][0]);
#ifdef DEBUG
  printf("end free segch\n");
  fflush(stdout);
#endif

  free((uintptr_t*)SEGMENT[SEGDS][0]);
#ifdef DEBUG
  printf("end free segds\n");
  fflush(stdout);
#endif
  free((uintptr_t*)SEGMENT[SEGST][0]);
#ifdef DEBUG
  printf("end free segst\n");
  fflush(stdout);
#endif
  
  return 0;
}

void iacpbludp_abort_gmm(void)
{
  free((uintptr_t*)SEGMENT[SEGVD][0]);
  free((uintptr_t*)SEGMENT[SEGCH][0]);
  free((uintptr_t*)SEGMENT[SEGDS][0]);
  free((uintptr_t*)SEGMENT[SEGST][0]);
  
  return;
}

acp_ga_t acp_query_starter_ga(int rank)
{
  return (acp_ga_t)(((uint64_t)(rank + 1) << (BIT_SEG + BIT_OFFSET)) | ((uint64_t)SEGST << BIT_OFFSET));
}

acp_ga_t iacp_query_starter_ga_ds(int rank)
{
  return (acp_ga_t)(((uint64_t)(rank + 1) << (BIT_SEG + BIT_OFFSET)) | ((uint64_t)SEGDS << BIT_OFFSET));
}

acp_ga_t iacp_query_starter_ga_ch(int rank)
{
  return (acp_ga_t)(((uint64_t)(rank + 1) << (BIT_SEG + BIT_OFFSET)) | ((uint64_t)SEGCH << BIT_OFFSET));
}

acp_ga_t iacp_query_starter_ga_vd(int rank)
{
  return (acp_ga_t)(((uint64_t)(rank + 1) << (BIT_SEG + BIT_OFFSET)) | ((uint64_t)SEGVD << BIT_OFFSET));
}

acp_atkey_t acp_register_memory(void* addr, size_t size, int color)
{
  return (acp_atkey_t)address2ga(addr, size);
}

int acp_unregister_memory(acp_atkey_t atkey)
{
  return 0;
}

acp_ga_t acp_query_ga(acp_atkey_t atkey, void* addr)
{
  return atkey2ga(atkey, addr);
}

void* acp_query_address(acp_ga_t ga)
{
  return ga2address(ga);
}

int acp_query_rank(acp_ga_t ga)
{
  return ga2rank(ga);
}

int acp_query_color(acp_ga_t ga)
{
  return 0;
}
