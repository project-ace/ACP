/***** Copyright FUJITSU LIMITED 2017 *****/

#ifndef __ACPBL_TOFU_SYS_H__
#define __ACPBL_TOFU_SYS_H__

// for acpbl_tofu.c
extern int      _acpblTofu_sys_init(int rank, int* ptr_jobid, int* ptr_num_procs);
extern int      _acpblTofu_sys_finalize(void);
extern void     _acpblTofu_sys_ga_init(uint64_t* lsb_color, uint64_t* lsb_localtag, uint64_t* lsb_rank, uint64_t* lsb_offset, uint64_t* mask_color, uint64_t* mask_localtag, uint64_t* mask_rank, uint64_t* mask_offset);
extern int      _acpblTofu_sys_get_rank(void);
extern int      _acpblTofu_sys_num_colors(void);
extern int      _acpblTofu_sys_barrier(void);

// for acpbl_tofu_gma.c
extern int      _acpblTofu_sys_get_data(int flags, uint64_t remote, uint64_t local, int length, int id);
extern int      _acpblTofu_sys_put_data(int flags, uint64_t remote, uint64_t local, int length, int id);
extern int      _acpblTofu_sys_put_data_imd(int flags, uint64_t remote, void *local, int length, int id);
extern int      _acpblTofu_sys_put_data_imd2(int flags0, uint64_t remote0, void *local0, int length0, int id0, int flags1, uint64_t remote1, void *local1, int length1, int id1);

// for acpbl_tofu_gmm.c
extern int      _acpblTofu_sys_register_memory(void *addr, uint64_t size, int color, int localtag);
extern int      _acpblTofu_sys_unregister_memory(int localtag);

// for acpbl_tofu_thread.c
extern int      _acpblTofu_sys_read_status(int* comm_id, uint64_t* offset);

/*** Tofu system definitions ***/
#define TOFU_SYS_STAT_TRANSEND	1
#define TOFU_SYS_STAT_RECEIVED	2
#define TOFU_SYS_STAT_TRANSERR	-1
#define TOFU_SYS_FLAG_NOTIFY	1
#define TOFU_SYS_FLAG_CONTINUE	2

/** commannd queue, delegation command codes **/
#define CMD_SYNC        0x02
#define CMD_COMPLETE    0x03

#define CMD_NEW_RANK    0x04
#define CMD_RANK_TABLE  0x05

#define CMD_COPY        0x0C

#define CMD_CAS4        0x10
#define CMD_SWAP4       0x14
#define CMD_ADD4        0x18
#define CMD_XOR4        0x1C
#define CMD_OR4         0x20
#define CMD_AND4        0x24

#define CMD_CAS8        0x40
#define CMD_SWAP8       0x44
#define CMD_ADD8        0x48
#define CMD_XOR8        0x4C
#define CMD_OR8         0x80
#define CMD_AND8        0x84

#endif /* acpbl_tofu_sys.h */
