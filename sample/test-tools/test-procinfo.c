#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"
#include "procinfo.h"

void procinfo_showall(procinfo_t pi)
{
    int p, np, myid, data;

    np = acp_procs();
    myid = acp_rank();

    for (p = 0; p < np; p++){
        procinfo_queryinfo(pi, p, &data);
        printf("%d: %d %d\n", myid, p, data);
    }

}

int main(int argc, char *argv[])
{
    int myrank, procs, data;
    procinfo_t pi;


    acp_init(&argc, &argv);

    procs = acp_procs ( ) ;
    myrank = acp_rank ( ) ;

    data = myrank;
    pi = procinfo_create(&data, sizeof(int));

    procinfo_showall(pi);

    procinfo_free(pi);

    acp_finalize();

}

