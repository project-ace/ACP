#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#ifdef MPIACP
#include "mpi.h"
#endif /* MPIACP */
#include "acpbl_input.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#define _no_argument_       0
#define _required_argument_ 1
#define _optional_argument_ 2

static char *acp_header = "--acp-" ;
static int len_acp_header = 6 ;

typedef enum {
    UINT, DOUBLE, STRING
} acp_opttype_t ;

typedef struct {
    char         *name ;
    acp_opttype_t type ;
    uint64_t      u_default ;
    double        d_default ;
    char         *s_default ;
    uint64_t      u_min ;
    uint64_t      u_max ;
    double        d_min ;
    double        d_max ;
} acpbl_default_option_t ;

typedef struct {
    const char *name;
    int         has_arg;
    int        *flag;
    int         val;
} acpbl_option_t ;

#define __INSIDE_ACPBL_INPUT_C__
#include "acpbl_default_input.h"    /// default_opts[] and long_options[] are defined.
#undef  __INSIDE_ACPBL_INPUT_C__

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static int print_usage( char *comm, FILE *fout )
{
    fprintf( fout, "Usage:\n" ) ;
    fprintf( fout, "ACP connections by options:\n" ) ;
    fprintf( fout, "    %s\n", comm ) ;
    fprintf( fout, "     --acp-myrank          myrank\n" ) ;
    fprintf( fout, "     --acp-nprocs          nprocs\n" ) ;
    fprintf( fout, "     --acp-port-local      local_port\n" ) ;
    fprintf( fout, "     --acp-host-remote     remote_host\n" ) ;
    fprintf( fout, "     --acp-port-remote     remote_port\n" ) ;
    fprintf( fout, "     --acp-taskid          taskid\n" ) ;
    fprintf( fout, "   [ --acp-size-smem       starter_memory_size ( user region  ) ]\n" ) ;
    fprintf( fout, "   [ --acp-size-smem-cl    starter_memory_size ( comm.library ) ]\n" ) ;
    fprintf( fout, "   [ --acp-size-smem-dl    starter_memory_size ( data library ) ]\n" ) ;
    fprintf( fout, "   [ --acp-ethernet-speed  ethernet_speed                       ]\n" ) ;
    fprintf( fout, "ACP connections by portfile,\n" ) ;
    fprintf( fout, "used for Multiple MPI connections (ACP+MPI):\n" ) ;
    fprintf( fout, "    %s\n", comm ) ;
    fprintf( fout, "     --acp-portfile        port_filename\n" ) ;
    fprintf( fout, "     --acp-offsetrank      rank_offset\n" ) ;
    return 0 ;
}

static int print_error_argument( int ir, void *curr, FILE *fout )
{
    if ( default_opts[ ir ].type == UINT ) {
        fprintf( fout, "Error argument value: %lu, !( %lu <= value <= %lu ).\n",
            *(( uint64_t * ) curr), default_opts[ ir ].u_min, default_opts[ ir ].u_max ) ;
    } else if ( default_opts[ ir ].type == DOUBLE ) {
        fprintf( fout, "Error argument value: %e, ( %e <= value < %e ).\n",
            *(( double * ) curr), default_opts[ ir ].d_min, default_opts[ ir ].d_max ) ;
    } else if ( default_opts[ ir ].type == STRING ) {
        fprintf( fout, "Error argument value: %s.\n", ( char * ) curr ) ;
    } else {
        fprintf( fout, "Error argument value: %s.\n", ( char * ) curr ) ;
    }
    return 0 ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef MPIACP
static int read_portfile( acpbl_input_t *ait )
{
    int  myrank_runtime, nprocs_runtime ;
    char buf[ BUFSIZ ] ;
    char *filename = ait->s_inputs[ IR_PORTFILE ] ;

    ///
    MPI_Comm_rank( MPI_COMM_WORLD, &myrank_runtime ) ;
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs_runtime ) ;

///    fprintf( stdout, "%4d, %4d: %s, %lu\n", myrank_runtime, nprocs_runtime, ait->s_inputs[ IR_PORTFILE ], ait->u_inputs[ IR_OFFSETRANK ] ) ;

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
            if ( i >= (myrank_runtime + ait->u_inputs[ IR_OFFSETRANK ]) ) {
                break ;
            }
            i++ ;
        }
    }
    sscanf( buf, "%lu %lu %lu %lu %s %lu %lu %lu",
            &(ait->u_inputs[ IR_MYRANK ]), &(ait->u_inputs[ IR_NPROCS ]), &(ait->u_inputs[ IR_LPORT ]), &(ait->u_inputs[ IR_RPORT ]),
            ait->s_inputs[ IR_RHOST ],
            &(ait->u_inputs[ IR_SZSMEM_BL ]), &(ait->u_inputs[ IR_SZSMEM_CL ]), &(ait->u_inputs[ IR_SZSMEM_DL ]) ) ;
///    fprintf( stdout, "%32lu %32lu %32lu %32lu %s %32lu %32lu %32lu\n", 
///            ait->u_inputs[ IR_MYRANK ], ait->u_inputs[ IR_NPROCS ], ait->u_inputs[ IR_LPORT ], ait->u_inputs[ IR_RPORT ],
///            ait->s_inputs[ IR_RHOST ],
///            ait->u_inputs[ IR_SZSMEM_BL ], ait->u_inputs[ IR_SZSMEM_CL ], ait->u_inputs[ IR_SZSMEM_DL ] );
    return 0 ;
}
#endif /* MPIACP */

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
static int check_acp_option_header( char *arg )
{
    int i ;
    if ( strlen( arg ) < len_acp_header ) {
        return 0 ;
    }
    for ( i = 0 ; i < len_acp_header ; i++ ) {
        if ( arg[ i ] != acp_header[ i ] ) {
            return 0 ;
        }
    }
    return 1 ;
}

static int match_acp_option( char *arg )
{
    int i ;
    for ( i = 0 ; i < _NIR_ ; i++ ) {
        ///if ( strncmp( arg, long_options[ i ].name, strlen( long_options[ i ].name ) ) == 0 ) {
        if ( strcmp( arg, long_options[ i ].name ) == 0 ) {
            return long_options[ i ].val ;
        }
    }
    return -1 ;
}

static int copy_arg( char *opt, char *optarg, int ir_default_opts, acpbl_input_t *ait )
{
    int ir = ir_default_opts ;
    if ( default_opts[ ir ].type == UINT ) {
        ait->u_inputs[ ir ] = strtoul( optarg, NULL, 0 ) ;
        if ( !(( default_opts[ ir ].u_min <= ait->u_inputs[ ir ] ) || ( ait->u_inputs[ ir ] <= default_opts[ ir ].u_max )) ) {
            print_error_argument( ir, &(ait->u_inputs[ ir ]), stderr ) ;
            exit( EXIT_FAILURE ) ;
        }
    } else if ( default_opts[ ir ].type == DOUBLE ) {
        ait->d_inputs[ ir ] = strtod( optarg, NULL ) ;
        if ( !(( default_opts[ ir ].d_min <= ait->d_inputs[ ir ] ) || ( ait->d_inputs[ ir ] <= default_opts[ ir ].d_max )) ) {
            print_error_argument( ir, &(ait->d_inputs[ ir ]), stderr ) ;
            exit( EXIT_FAILURE ) ;
        }
    } else if ( default_opts[ ir ].type == STRING ) {
        sscanf( optarg, "%s", ait->s_inputs[ ir ] ) ;
        if ( strlen( ait->s_inputs[ ir ] ) <= 0 ) {
            print_error_argument( ir, ait->s_inputs[ ir ], stderr ) ;
        }
    }
    ait->flg_set[ ir ] = 1 ;
    return 0 ;
}

static int mygetopt_long_only( int *argc, char ***argv, acpbl_input_t *ait )
{
    int i, ir, ind ;
    ait->argc = 0 ;
    ait->argv = ( char ** ) malloc( (*argc) * sizeof( char ** ) ) ;
    for ( i = 0 ; i < (*argc) ; i++ ) {
        ait->argv[ i ] = ( char * ) malloc( (*argc) * sizeof( char * ) ) ;
    }
///
    ind = 0 ;
    while ( ind < (*argc) ) {
        if ( check_acp_option_header( (*argv)[ ind ] ) ) {
            if ( (ir = match_acp_option( (*argv)[ ind ] )) != -1 ) {
                if ( ind + 1 < (*argc) ) {
                    if ( (*argv)[ ind + 1 ][ 0 ] != '-' ) {
                        copy_arg( (*argv)[ ind ], (*argv)[ ind + 1 ], ir, ait ) ;
                    } else {
                        fprintf( stderr, "Error: %s: invalid optarg: \"%s\" for acp-option: \"%s\".\n", (*argv)[ 0 ], (*argv)[ ind + 1 ], (*argv)[ ind ] ) ;
                        exit( EXIT_FAILURE ) ;
                    }
                } else {
                    fprintf( stderr, "Error: %s: could not find optarg for acp-option: \"%s\".\n", (*argv)[ 0 ], (*argv)[ ind ] ) ;
                    exit( EXIT_FAILURE ) ;
                }
            } else {
                fprintf( stderr, "Error: %s: invalid option \"%s\"\n", (*argv)[ 0 ], (*argv)[ ind ] ) ;
                print_usage( (*argv)[ 0 ], stderr ) ;
                exit( EXIT_FAILURE ) ;
            }
            ind       += 2 ;
        } else {
            ait->argv[ ait->argc ] = (*argv)[ ind ] ;
            ait->argc += 1 ;
            ind       += 1 ;
        }
    }
    return 0 ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

int iacp_connection_information( int *argc, char ***argv, acpbl_input_t *ait )
{
    int  i ;
    for ( i = 0 ; i < _NIR_ ; i++ ) {
        ait->flg_set [ i ] = 0 ;
        ait->u_inputs[ i ] = default_opts[ i ].u_default ;
        ait->d_inputs[ i ] = default_opts[ i ].d_default ;
        ait->s_inputs[ i ] = ( char * )malloc( BUFSIZ ) ;
        if ( ait->s_inputs[ i ] && default_opts[ i ].s_default && ( strlen( default_opts[ i ].s_default ) < BUFSIZ ) ) {
            strcpy( ait->s_inputs[ i ], default_opts[ i ].s_default ) ;
        }
    }

    mygetopt_long_only( argc, argv, ait ) ;
    *argc = ait->argc ;
    for ( i = 0 ; i < (*argc) ; i++ ) {
        (*argv)[ i ] = ait->argv[ i ] ;	
    }
///    fprintf( stderr, "%4d: ", ait->argc ) ;
///    for ( i = 0 ; i < ait->argc ; i++ ) {
///        fprintf( stderr, "\"%s\" ", ait->argv[ i ] ) ;
///    }
///    fprintf( stderr, "\n" ) ;
///
///    for ( i = 0 ; i < _NIR_ ; i++ ) {
///        fprintf( stdout, "flg: %4d -> %4d\n", i, ait->flg_set[ i ] ) ;
///    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef MPIACP
    if ( ait->flg_set[ IR_PORTFILE ] ) {
        ///////////////////////////////
        read_portfile( ait ) ;
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
    ait->u_inputs[ IR_RHOST ] = inet_addr( ait->s_inputs[ IR_RHOST ] );
    if ( ait->u_inputs[ IR_RHOST ] == 0xffffffff ) {
        struct hostent *host;
        if ((host = gethostbyname( ait->s_inputs[ IR_RHOST ] )) == NULL) {
            return -1 ;
        }
        ait->u_inputs[ IR_RHOST ] = *(uint32_t *)host->h_addr_list[0];
    }
    ///

    ///for ( i = 0 ; i < _NIR_ ; i++ ) {
    ///    free( ait->s_inputs[ i ] ) ;
    ///}
    return 0 ;
}
