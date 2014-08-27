/*
 * ACP Basic Layer Header
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
#ifndef __ACPBL_H__
#define __ACPBL_H__

/* Multi-module support */

acp_ga_t iacp_query_starter_ga_dl(int);
acp_ga_t iacp_query_starter_ga_cl(int);
int iacp_init_dl(void);
int iacp_init_cl(void);
int iacp_finalize_dl(void);
int iacp_finalize_cl(void);
void iacp_abort_dl(void);
void iacp_abort_cl(void);

extern size_t iacp_starter_memory_size_dl;
extern size_t iacp_starter_memory_size_cl;

#endif /* acpbl.h */
