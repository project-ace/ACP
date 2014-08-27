/*
 * ACP Basic Layer GMA header for UDP
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
#ifndef __ACPBL_UDP_GMA_H__
#define __ACPBL_UDP_GMA_H__

/* Maximum Fragment Size */
#ifndef ACPBL_UDP_MFS
#define ACPBL_UDP_MFS 1440
#endif

/* Command Queue Size in binary exponent */
#ifndef ACPBL_UDP_CQ_SIZE
#define ACPBL_UDP_CQ_SIZE 8
#endif

/* Command Station Size in binary exponent */
#ifndef ACPBL_UDP_CS_SIZE
#define ACPBL_UDP_CS_SIZE 4
#endif

/* Delegate Station Size in binary exponent */
#ifndef ACPBL_UDP_DS_SIZE
#define ACPBL_UDP_DS_SIZE 4
#endif

/* Delegate Window Size in binary exponent */
#ifndef ACPBL_UDP_SW_SIZE
#define ACPBL_UDP_SW_SIZE 3
#endif

extern int iacpbludp_init_gma(void);
extern int iacpbludp_finalize_gma(void);
extern void iacpbludp_abort_gma(void);

#endif /* acpbl_udp_gma.h */
