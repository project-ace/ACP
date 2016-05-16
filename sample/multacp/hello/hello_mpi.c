#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"
#include "mpi.h"

int main ( int argc, char *argv[] )
{
    int np_mpi, me_mpi ;
    int np_acp, me_acp ;
    char str[ 1024 ] ;

    MPI_Init ( &argc, &argv ) ;
    acp_init ( &argc, &argv ) ;

    MPI_Comm_size ( MPI_COMM_WORLD, &np_mpi ) ;
    MPI_Comm_rank ( MPI_COMM_WORLD, &me_mpi ) ;
    np_acp = acp_procs() ;
    me_acp = acp_rank() ;
    gethostname( str, sizeof( str ) ) ;

    fprintf( stdout, "hello_mpi:   host %s :   acp me %5d, acp np %5d :   mpi me %5d, mpi np %5d\n", str, me_acp, np_acp, me_mpi, np_mpi ) ;

    acp_finalize() ;
    MPI_Finalize() ;

    return 0 ;
}
