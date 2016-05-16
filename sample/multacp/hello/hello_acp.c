#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"

int main(int argc, char *argv[])
{
    int me_acp, np_acp ;
    char str[ 1024 ] ;

    acp_init( &argc, &argv ) ;

    me_acp = acp_rank() ;
    np_acp = acp_procs() ;
    gethostname( str, sizeof( str ) ) ;

    fprintf( stdout, "hello_acp:   host %s :   acp me %5d, acp np %5d\n", str, me_acp, np_acp ) ;

    acp_finalize() ;
    return 0 ;
}
