#ifndef __ACPBL_MMS_H__
#define __ACPBL_MMS_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Multi-module support */

acp_ga_t iacp_query_starter_ga_ds(int);
acp_ga_t iacp_query_starter_ga_ch(int);
acp_ga_t iacp_query_starter_ga_vd(int);
int iacp_init_ds(void);
int iacp_init_ch(void);
int iacp_init_vd(void);
int iacp_finalize_ds(void);
int iacp_finalize_ch(void);
int iacp_finalize_vd(void);
void iacp_abort_ds(void);
void iacp_abort_ch(void);
void iacp_abort_vd(void);

size_t iacp_starter_memory_size_ds;
size_t iacp_starter_memory_size_ch;
size_t iacp_starter_memory_size_vd;

#ifdef  __cplusplus
}
#endif

#endif /* acpbl_mms.h */
