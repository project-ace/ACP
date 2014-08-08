#ifndef __ACPBL_UDP_H__
#define __ACPBL_UDP_H__

#ifdef DEBUG
#define debug
#else
#define debug 1 ? (void)0 :
#endif

#define ACPBL_UDP_RANK_ERROR 0xffffffff
#define ACPBL_UDP_GPID_ERROR 0xffffffff

int iacpbludp_my_rank;
int iacpbludp_num_procs;
uint32_t  iacpbludp_taskid;

uint32_t* iacpbludp_rank_table;
uint16_t* iacpbludp_port_table;
uint32_t* iacpbludp_addr_table;

#define MY_RANK    iacpbludp_my_rank
#define NUM_PROCS  iacpbludp_num_procs
#define TASKID     iacpbludp_taskid

#define RANK_TABLE iacpbludp_rank_table
#define PORT_TABLE iacpbludp_port_table
#define ADDR_TABLE iacpbludp_addr_table

#endif /* acpbl_udp.h */
