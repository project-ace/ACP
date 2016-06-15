#ifndef _INCLUDE_MPI_PLUS_ACP_
#define _INCLUDE_MPI_PLUS_ACP_

int macp_init_info();
int query_rank_intask(int gid);
int query_rank_global(int taskid, int rank_intask);
int query_num_tasks();
int query_procs_intask(int taskid);

#endif
