#ifndef __ACPBL_H__
#define __ACPBL_H__

#include <stdint.h>

/* ACP constants */
#define ACP_GA_NULL      0xffffffffffffffffLLU 
#define ACP_ATKEY_NULL   0xffffffffffffffffLLU 
#define ACP_HANDLE_ALL   0xffffffffffffffffLLU
#define ACP_HANDLE_NULL  0x0LLU

typedef uint64_t acp_ga_t;
typedef uint64_t acp_atkey_t;
typedef int64_t acp_handle_t;
typedef uint64_t acp_size_t;

int acp_init(int *argc, char *** argv);
int acp_finalize(void);

int acp_rank(void);
int acp_procs(void);
int acp_colors(void);
int acp_query_rank(acp_ga_t ga);
uint32_t acp_query_color(acp_ga_t ga);
acp_ga_t acp_query_starter_ga(int rank);
acp_ga_t acp_query_ga(acp_atkey_t atkey, void* addr);
acp_atkey_t acp_register_memory(void* addr, acp_size_t size, int color);
void *acp_query_address(acp_ga_t ga);
acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, acp_size_t size, acp_handle_t order);
void acp_complete(acp_handle_t handle);
int acp_sync(void);
#endif
