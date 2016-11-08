#define __MAIN_ACPBL_INPUT__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#define GNU_SOURCE
#ifdef GNU_SOURCE
 #include <getopt.h>
#endif

#ifdef MPIACP
#include "mpi.h"
#endif /* MPIACP */
#include "acpbl_input.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

typedef struct {
    char       *name ;
    int         flg_num ;
    uint64_t    n_default ;
    char       *s_default ;
    uint64_t    min ;
    uint64_t    max ;
} acpbl_input_opt_t ;

static acpbl_input_opt_t default_opts[] = {
    ///
    { "multirun",             0,            NULL,       "portfile",            NULL,            NULL }, /// 0
    { "multirun-offset",      1,               0,             NULL,               0,        10000000 }, /// 1
    ///
    { "myrank",               1,               0,             NULL,               0,        10000000 }, /// 2
    { "nprocs",               1,               1,             NULL,               0,        10000000 }, /// 3
    { "lport",                1,           44256,             NULL,           44256,           61000 }, /// 4
    { "rport",                1,           44256,             NULL,           44256,           61000 }, /// 5
    { "rhost",                0,            NULL,      "127.0.0.1",            NULL,            NULL }, /// 6
    ///
    { "taskid",               1,               1,             NULL,               0,        10000000 }, /// 7
    { "szsmem",               1,           10240,             NULL,               0,     10000000000 }, /// 8
    { "szsmemcl",             1,           10240,             NULL,               0,     10000000000 }, /// 9
    { "szsmemdl",             1,           10240,             NULL,               0,     10000000000 }, /// 10
    { NULL,                NULL,            NULL,             NULL,            NULL,            NULL }
} ;

////
////
////
static char short_options[] = "i:n:s:c:d:l:r:t:h:m:o:" ;
static struct option const long_options[] = {
    { "multirun",        required_argument, NULL, 'm' }, /// 0
    { "multirun-offset", required_argument, NULL, 'o' }, /// 1
    ///
    { "myrank",          required_argument, NULL, 'i' }, /// 2
    { "nprocs",          required_argument, NULL, 'n' }, /// 3
    { "port-local",      required_argument, NULL, 'l' }, /// 4
    { "port-remote",     required_argument, NULL, 'r' }, /// 5
    { "host-remote",     required_argument, NULL, 'h' }, /// 6
    ///
    { "taskid",          required_argument, NULL, 't' }, /// 7
    { "size-smem",       required_argument, NULL, 's' }, /// 8
    { "size-smem-cl",    required_argument, NULL, 'c' }, /// 9
    { "size-smem-dl",    required_argument, NULL, 'd' }, /// 10
    { 0,                 0,                 0,     0  }
} ;
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static int iacp_print_usage( char *comm, FILE *fout )
{
    fprintf( fout, "Usage:\n" ) ;
    fprintf( fout, "Normal ACP connections:\n" ) ;
    fprintf( fout, "    %s\n", comm ) ;
    fprintf( fout, "     -i / --myrank          myrank\n" ) ;
    fprintf( fout, "     -n / --nprocs          nprocs\n" ) ;
    fprintf( fout, "     -l / --port-local      local_port\n" ) ;
    fprintf( fout, "     -h / --host-remote     remote_host\n" ) ;
    fprintf( fout, "     -r / --port-remote     remote_port\n" ) ;
    fprintf( fout, "     -t / --taskid          taskid\n" ) ;
    fprintf( fout, "   [ -s / --size-smem       starter_memory_size ( user region  ) ]\n" ) ;
    fprintf( fout, "   [ -c / --size-smem-cl    starter_memory_size ( comm.library ) ]\n" ) ;
    fprintf( fout, "   [ -d / --size-smem-dl    starter_memory_size ( data library ) ]\n" ) ;
    fprintf( fout, "Multiple MPI connections (ACP+MPI):\n" ) ;
    fprintf( fout, "    %s\n", comm ) ;
    fprintf( fout, "     -m / --multirun        port_filename\n" ) ;
    fprintf( fout, "     -o / --multirun-offset rank_offset\n" ) ;
    fprintf( fout, "     -t / --taskid          taskid\n" ) ;
    fprintf( fout, "   [ -s / --size-smem       starter_memory_size ( user  region  ) ]\n" ) ;
    fprintf( fout, "   [ -c / --size-smem-cl    starter_memory_size ( comm. library ) ]\n" ) ;
    fprintf( fout, "   [ -d / --size-smem-dl    starter_memory_size ( data  library ) ]\n" ) ;
    return 0 ;
}

static int iacp_print_error_argument( int ir, void *curr, FILE *fout )
{
    if ( default_opts[ ir ].flg_num ) {
        fprintf( fout, "Error argument value: %lu, ( %lu <= val < %lu ).\n",
            ( uint64_t )( uint64_t * ) curr,
              default_opts[ ir ].min, default_opts[ ir ].max ) ;
    } else {
        fprintf( fout, "Error argument value: %s.\n", ( char *) curr ) ;
    }
    return 0 ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef MPIACP
static int iacp_read_multirun_file( acpbl_input_t *ait )
{
    int  myrank_runtime, nprocs_runtime ;
    char buf[ BUFSIZ ] ;
    char *filename = ait->s_inputs[ IR_MULTIRUN ] ;;

    ///
    MPI_Comm_rank( MPI_COMM_WORLD, &myrank_runtime ) ;
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs_runtime ) ;

    ///
    {   
        int  i ; 
        FILE *fp = fopen( filename, "r" ) ;
        if ( fp == (FILE *) NULL ) {
           fprintf( stderr, "File: %s :open error:\n", filename ) ;
           exit( 1 ) ;
        } 
        i = 0 ;
        while( fgets( buf, BUFSIZ, fp ) != NULL ) {
            if ( i >= (myrank_runtime + ait->n_inputs[ IR_MULTIRUN_OFFSET ]) ) {
                break ;
            }
            i++ ;
        }
    }
    sscanf( buf, "%lu %lu %lu %lu %s",
            &(ait->n_inputs[ IR_MYRANK ]), &(ait->n_inputs[ IR_NPROCS ]), &(ait->n_inputs[ IR_LPORT ]), &(ait->n_inputs[ IR_RPORT ]), ait->s_inputs[ IR_RHOST ] ) ;
    return 0 ;
}
#endif /* MPIACP */

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

int iacp_connection_information( int *argc, char ***argv, acpbl_input_t *ait )
{
    int i, opt, option_index ;

    for ( i = 0 ; i < _NIR_ ; i++ ) {
        ait->flg_set[ i ]  = 0 ;
        ait->s_inputs[ i ] = ( char * )malloc( BUFSIZ ) ;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
///#ifndef GNU_SOURCE
///    while ( (opt = getopt( *argc, *argv, "i:n:s:c:d:l:r:t:h:m:o:" )) != -1 ) {
///#else
    while ( (opt = getopt_long( *argc, *argv, short_options, long_options, &option_index )) != -1 ) {
///#endif
        for ( i = 0 ; i < _NIR_ ; i++ ) {
            if ( opt == long_options[ i ].val ) {
                ait->flg_set[ i ] = 1 ;
                if ( default_opts[ i ].flg_num ) {
                    ait->n_inputs[ i ] = strtoul( optarg, NULL, 0 ) ;
                    if ( !(( default_opts[ i ].min <= ait->n_inputs[ i ] ) || ( ait->n_inputs[ i ] < default_opts[ i ].max )) ) {
                        iacp_print_error_argument( i, &(ait->n_inputs[ i ]), stderr ) ;
                        exit( EXIT_FAILURE ) ;
                    }
                } else {
                    sscanf( optarg, "%s", ait->s_inputs[ i ] ) ;
                    if ( strlen( ait->s_inputs[ i ] ) <= 0 ) {
                        iacp_print_error_argument( i, ait->s_inputs[ i ], stderr ) ;
                    }
                }
                goto flg_found ;
            }
        }
        iacp_print_usage( (*argv)[ 0 ], stderr ) ; 
        exit( EXIT_FAILURE ) ;
flg_found: ;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    for ( i = IR_MYRANK ; i < _NIR_ ; i++ ) {
        if ( !(ait->flg_set[ i ]) ) {
            if ( default_opts[ i ].flg_num ) {
                ait->n_inputs[ i ] = default_opts[ i ].n_default ;;
            } else {
                strncpy( ait->s_inputs[ i ],
                         default_opts[ i ].s_default,
                         strlen( default_opts[ i ].s_default ) + 1 ) ;
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef MPIACP
    if ( ait->flg_set[ IR_MULTIRUN ] ) {
        ///////////////////////////////
        iacp_read_multirun_file( ait ) ;
        ///////////////////////////////
    } else {
#endif /* MPIACP */
        ;
#ifdef MPIACP
    }
#endif /* MPIACP */

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ait->n_inputs[ IR_RHOST ] = inet_addr( ait->s_inputs[ IR_RHOST ] );
    if ( ait->n_inputs[ IR_RHOST ] == 0xffffffff ) {
        struct hostent *host;
        if ((host = gethostbyname( ait->s_inputs[ IR_RHOST ] )) == NULL) {
            return -1 ;
        }
        ait->n_inputs[ IR_RHOST ] = *(uint32_t *)host->h_addr_list[0];
    }
    ///

    for ( i = 0 ; i < _NIR_ ; i++ ) {
        free( ait->s_inputs[ i ] ) ;
    }
    return 0 ;
}
