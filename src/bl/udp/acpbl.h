/*****************************************************************************/
/***** ACP Basic Layer Header                                            *****/
/*****                                                                   *****/
/***** Copyright FUJITSU LIMITED 2014                                    *****/
/*****                                                                   *****/
/***** Specification Version: ACP-140312                                 *****/
/***** Version: 0.0                                                      *****/
/***** Module Version: 0.0                                               *****/
/*****                                                                   *****/
/***** Note:                                                             *****/
/*****************************************************************************/
#ifndef __ACPBL_H__
#define __ACPBL_H__
#include "acp.h"

#include "acpbl_sync.h"

/* Multi-module support */

acp_ga_t iacp_query_starter_ga_dl(int);
acp_ga_t iacp_query_starter_ga_cl(int);
acp_ga_t iacp_query_starter_ga_vd(int);
int iacp_init_dl(void);
int iacp_init_cl(void);
int iacp_init_vd(void);
int iacp_finalize_dl(void);
int iacp_finalize_cl(void);
int iacp_finalize_vd(void);
void iacp_abort_dl(void);
void iacp_abort_cl(void);
void iacp_abort_vd(void);

extern size_t iacp_starter_memory_size_dl;
extern size_t iacp_starter_memory_size_cl;
extern size_t iacp_starter_memory_size_vd;

#endif /* acpbl.h */
