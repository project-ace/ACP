/*
 * ACP Basic Layer header for UDP
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
#ifndef __ACPBL_UDP_H__
#define __ACPBL_UDP_H__

#ifdef DEBUG
#define debug
#else
#define debug 1 ? (void)0 :
#endif

#define ACPBL_UDP_RANK_ERROR 0xffffffff
#define ACPBL_UDP_GPID_ERROR 0xffffffff

extern int iacpbludp_my_rank;
extern int iacpbludp_num_procs;
extern uint32_t  iacpbludp_taskid;

extern uint32_t* iacpbludp_rank_table;
extern uint16_t* iacpbludp_port_table;
extern uint32_t* iacpbludp_addr_table;

#define MY_RANK    iacpbludp_my_rank
#define NUM_PROCS  iacpbludp_num_procs
#define TASKID     iacpbludp_taskid

#define RANK_TABLE iacpbludp_rank_table
#define PORT_TABLE iacpbludp_port_table
#define ADDR_TABLE iacpbludp_addr_table

#endif /* acpbl_udp.h */
