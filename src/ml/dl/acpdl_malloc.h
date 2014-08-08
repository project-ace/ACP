#ifndef __ACPDL_MALLOC_H__
#define __ACPDL_MALLOC_H__

void iacpdl_init_malloc(void);
void iacpdl_finalize_malloc(void);
acp_ga_t acp_malloc(size_t, int);
void acp_free(acp_ga_t);

#endif /* acpdl_malloc.h */
