#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"
#include "mpi.h"
#include "multmpi.h"

static int *taskoffsets;
static int numtasks;
static int mytaskid;

int macp_init_info()
{
    int np, me, np_acp, me_acp;
    acp_ga_t mystga, tmpstga;
    int *mystla;
    int i, t, tmpnp;
    int *tmpnps;

    MPI_Comm_size ( MPI_COMM_WORLD, &np ) ;
    MPI_Comm_rank ( MPI_COMM_WORLD, &me ) ;
    np_acp = acp_procs() ;
    me_acp = acp_rank() ;

    mystga = acp_query_starter_ga(me_acp);
    mystla = (int *)acp_query_address(mystga);

    if (me_acp != 0)
        *mystla = np;

    acp_sync();

    if (me_acp == 0){
        t = 1; 
        tmpnp = np;
        tmpnps = (int *)malloc(np_acp * sizeof(int));
        tmpnps[0] = np;

        while (tmpnp < np_acp){
            tmpstga = acp_query_starter_ga(tmpnp);
            acp_copy(mystga, tmpstga, sizeof(int), ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            tmpnps[t] = *mystla;
            tmpnp += tmpnps[t];
            t++;
            fprintf(stderr, "t %d, tmpnp %d\n", t, tmpnp);
        }

        taskoffsets = (int *)malloc((t+1) * sizeof(int));

        taskoffsets[0] = 0;
        for (i = 1; i <= t; i++){
            taskoffsets[i] = tmpnps[i-1] + taskoffsets[i-1];
        }
        free(tmpnps);

        numtasks = t;
        *mystla = numtasks;
        memcpy(mystla+1, taskoffsets, (numtasks+1) * sizeof(int));
    }

    acp_sync();

    if (me_acp != 0){
        tmpstga = acp_query_starter_ga(0);
        acp_copy(mystga, tmpstga, sizeof(int), ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        numtasks = *(mystla);
        taskoffsets = (int *)malloc((numtasks+1) * sizeof(int));
        acp_copy(mystga, (tmpstga + sizeof(int)), (numtasks+1) * sizeof(int), 
                 ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);
        for (i = 0; i <= numtasks; i++)
            taskoffsets[i] = mystla[i];
    }

    mytaskid = -1;
    for (i = 1; i <= numtasks; i++){
        if (taskoffsets[i] > me_acp){
            mytaskid = i-1;
            break;
        }
    }

    if (mytaskid < 0)
        fprintf(stderr, "%d: wrong taskid\n", me_acp);

#ifdef DEBUG
    fprintf(stderr, "%d: taskid %d / %d : ", me_acp, mytaskid, numtasks);
    for (i = 0; i < numtasks; i++)
        fprintf(stderr, "%d ", tasknps[i]);
    fprintf(stderr, "\n");
#endif

}

int query_taskid(int gid)
{
    int i, ret;

    ret = -1;
    for (i = 1; i <= numtasks; i++){
        if (taskoffsets[i] > gid){
            ret = i-1;
            break;
        }
    }

    return ret;
}

int query_rank_intask(int gid)
{
    int taskid, ret;

    taskid = query_taskid(gid);
    ret = gid - taskoffsets[taskid];
    
    return ret;
}

int query_rank_global(int taskid, int rank_intask)
{
    int ret;

    ret = taskoffsets[taskid] + rank_intask;

    return ret;
}

int query_num_tasks()
{
    return numtasks;
}

int query_procs_intask(int taskid)
{
    int ret;

    ret = taskoffsets[taskid+1] - taskoffsets[taskid];

    return ret;
}
