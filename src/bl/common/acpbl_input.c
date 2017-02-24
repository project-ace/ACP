#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
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

iacpbl_option_t iacpbl_option = {
    //value,        min,    max
    { "" },
    { 1,            0,      10000000 },
    { 0,            0,      10000000 },
    { 1,            1,      10000000 },
    { 44256,        1024,   65535 },
    { "127.0.0.1" },
    0,
    { 44257,        1024,   65535 },
    { 0,            0,      0xffffffffffffffffLLU },
    { 10240,        0,      0xffffffffffffffffLLU },
    { 10240,        0,      0xffffffffffffffffLLU },
    { 10240,        0,      0xffffffffffffffffLLU },
    { 0,            0,      1 },
    { 0,            0,      1 },
    { 0,            0,      1 },
    { 128,          0,      0xffffffffffffffffLLU },
    { 131072,       0,      0xffffffffffffffffLLU },
    { 1000,         1,      10000000 }
};

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef enum {
    arg_nil, arg_uint, arg_uint_metric, arg_double, arg_string
} argument_type_t;

typedef struct {
    argument_type_t type;
    size_t offset;
    char* name;
    char* description;
} option_list_t;

static const option_list_t option_list[] = {
    //type,             offset,                                 name,                       description
    {arg_uint,          offsetof(iacpbl_option_t, szsmem),      "--acp-size-smem",          "starter memory size (user region)"},
    {arg_uint,          offsetof(iacpbl_option_t, szsmemcl),    "--acp-size-smem-cl",       "starter memory size (comm. library)"},
    {arg_uint,          offsetof(iacpbl_option_t, szsmemdl),    "--acp-size-smem-dl",       "starter memory size (data library)"},
    {arg_uint,          offsetof(iacpbl_option_t, mhooksmall),  "--acp-malloc-hook-small",  "malloc hook flag [0|1] for small size"},
    {arg_uint,          offsetof(iacpbl_option_t, mhooklarge),  "--acp-malloc-hook-large",  "malloc hook flag [0|1] for large size"},
    {arg_uint,          offsetof(iacpbl_option_t, mhookhuge),   "--acp-malloc-hook-huge",   "malloc hook flag [0|1] for small size"},
    {arg_uint,          offsetof(iacpbl_option_t, mhooklow),    "--acp-malloc-hook-low",    "mallok hook low threshold"},
    {arg_uint,          offsetof(iacpbl_option_t, mhookhigh),   "--acp-malloc-hook-high",   "mallok hook high threshold"},
    {arg_uint,          offsetof(iacpbl_option_t, ethspeed),    "--acp-ethernet-speed",     "ethernet speed (in Mbps)"},
    //
    {arg_uint,          offsetof(iacpbl_option_t, taskid),      "--acp-taskid",             "parallel task identifier"},
    //
    {arg_string,        offsetof(iacpbl_option_t, portfile),    "--acp-portfile",           "(for macprun) portfile name"},
    {arg_uint,          offsetof(iacpbl_option_t, offsetrank),  "--acp-offsetrank",         "(for macprun) rank offset"},
    //
    {arg_uint,          offsetof(iacpbl_option_t, myrank),      "--acp-myrank",             "(for acprun) process rank"},
    {arg_uint,          offsetof(iacpbl_option_t, nprocs),      "--acp-nprocs",             "(for acprun) number of processes"},
    {arg_uint,          offsetof(iacpbl_option_t, lport),       "--acp-port-local",         "(for acprun) port number"},
    {arg_string,        offsetof(iacpbl_option_t, rhost),       "--acp-host-remote",        "(for acprun) remote host address"},
    {arg_uint,          offsetof(iacpbl_option_t, rport),       "--acp-port-remote",        "(for acprun) remote host port number"}
};

static char *acp_header = "--acp-" ;
static int len_acp_header = 6 ;

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

static int print_usage( char *comm, FILE *fout )
{
    int i;
    fprintf( fout, "Usage:\n    %s\n", comm ) ;
    for ( i = 0 ; i < sizeof(option_list) / sizeof(option_list_t) ; i++ ) {
        fprintf( fout, "      %-24s  %s\n", option_list[i].name, option_list[i].description) ;
    }
    return 0 ;
}

static int print_error_argument( int ir, void *curr, FILE *fout )
{
    if ( option_list[ ir ].type == arg_uint || option_list[ ir ].type == arg_uint_metric ) {
        iacpbl_option_uint_t* ptr = (iacpbl_option_uint_t*)((uintptr_t)&iacpbl_option + option_list[ ir ].offset) ;
        fprintf( fout, "Error argument value: %lu, !( %lu <= value <= %lu ).\n",
            *(( uint64_t * ) curr), ptr->min, ptr->max ) ;
    } else if ( option_list[ ir ].type == arg_double ) {
        iacpbl_option_double_t* ptr = (iacpbl_option_double_t*)((uintptr_t)&iacpbl_option + option_list[ ir ].offset) ;
        fprintf( fout, "Error argument value: %e, ( %e <= value < %e ).\n",
            *(( double * ) curr), ptr->min, ptr->max ) ;
    } else if ( option_list[ ir ].type == arg_string ) {
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
static int read_portfile( void )
{
    int  myrank_runtime, nprocs_runtime ;
    char buf[ BUFSIZ ] ;
    char *filename = iacpbl_option.portfile.string ;

    ///
    MPI_Comm_rank( MPI_COMM_WORLD, &myrank_runtime ) ;
    MPI_Comm_size( MPI_COMM_WORLD, &nprocs_runtime ) ;

///    fprintf( stdout, "%4d, %4d: %s, %lu\n", myrank_runtime, nprocs_runtime, iacpbl_option.portfile.string, iacpbl_option.offsetrank.value ) ;

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
            if ( i >= (myrank_runtime + iacpbl_option.offsetrank.value) ) {
                break ;
            }
            i++ ;
        }
    }
    sscanf( buf, "%lu %lu %lu %lu %s %lu %lu %lu",
            &iacpbl_option.myrank.value, &iacpbl_option.nprocs.value, &iacpbl_option.lport.value, &iacpbl_option.rport.value,
            iacpbl_option.rhost.string,
            &iacpbl_option.szsmem.value, &iacpbl_option.szsmemcl.value, &iacpbl_option.szsmemdl.value ) ;
///    fprintf( stdout, "%32lu %32lu %32lu %32lu %s %32lu %32lu %32lu\n", 
///            iacpbl_option.myrank.value, iacpbl_option.nprocs.value, iacpbl_option.lport.value, iacpbl_option.rport.value,
///            iacpbl_option.rhost.string,
///            iacpbl_option.szsmem.value, iacpbl_option.szsmemcl.value, iacpbl_option.szsmemdl.value ) ;
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
    for ( i = 0 ; i < sizeof(option_list) / sizeof(option_list_t) ; i++) {
        ///if ( strncmp( arg, option_list[ i ].name, strlen( option_list[ i ].name ) ) == 0 ) {
        if ( strcmp( arg, option_list[ i ].name ) == 0 ) {
            return i ;
        }
    }
    return -1 ;
}

static int copy_arg( char *opt, char *optarg, int ir )
{
    if ( option_list[ ir ].type == arg_uint || option_list[ ir ].type == arg_uint_metric ) {
        iacpbl_option_uint_t* ptr = (iacpbl_option_uint_t*)((uintptr_t)&iacpbl_option + option_list[ ir ].offset) ;
        ptr->value = strtoul( optarg, NULL, 0 ) ;
        if ( ptr->min > ptr->value || ptr->max < ptr->value ) {
            print_error_argument( ir, &ptr->value, stderr ) ;
            exit( EXIT_FAILURE ) ;
        }
    } else if ( option_list[ ir ].type == arg_double ) {
        iacpbl_option_double_t* ptr = (iacpbl_option_double_t*)((uintptr_t)&iacpbl_option + option_list[ ir ].offset) ;
        ptr->value = strtod( optarg, NULL ) ;
        if ( ptr->min > ptr->value || ptr->max < ptr->value ) {
            print_error_argument( ir, &ptr->value, stderr ) ;
            exit( EXIT_FAILURE ) ;
        }
    } else if ( option_list[ ir ].type == arg_string ) {
        iacpbl_option_string_t* ptr = (iacpbl_option_string_t*)((uintptr_t)&iacpbl_option + option_list[ ir ].offset) ;
        sscanf( optarg, "%s", ptr->string ) ;
        if ( strlen( ptr->string ) <= 0 ) {
            print_error_argument( ir, ptr->string, stderr ) ;
        }
    }
    return 0 ;
}

static int mygetopt_long_only( int *argc, char ***argv )
{
    int ir, ind ;
    int c = 0 ;
    char** v = *argv ;
///
    ind = 0 ;
    while ( ind < (*argc) ) {
        if ( check_acp_option_header( (*argv)[ ind ] ) ) {
            if ( (ir = match_acp_option( (*argv)[ ind ] )) != -1 ) {
                if ( ind + 1 < (*argc) ) {
                    if ( (*argv)[ ind + 1 ][ 0 ] != '-' ) {
                        copy_arg( (*argv)[ ind ], (*argv)[ ind + 1 ], ir ) ;
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
            v[ c++ ] = (*argv)[ ind++ ] ;
        }
    }
    *argc = c;
    return 0 ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

int iacpbl_interpret_option( int *argc, char ***argv )
{
    mygetopt_long_only( argc, argv ) ;
///    int  i ;
///
///    fprintf( stderr, "%4d: ", *argc ) ;
///    for ( i = 0 ; i < *argc ; i++ ) {
///        fprintf( stderr, "\"%s\" ", (*argv)[ i ] ) ;
///    }
///    fprintf( stderr, "\n" ) ;

    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef MPIACP
    if ( strlen( iacpbl_option.portfile.string ) > 0 ) {
        ///////////////////////////////
        read_portfile( ) ;
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
    iacpbl_option.rhost_ip = inet_addr( iacpbl_option.rhost.string );
    if ( iacpbl_option.rhost_ip == 0xffffffff ) {
        struct hostent *host;
        if ( (host = gethostbyname( iacpbl_option.rhost.string )) == NULL ) {
            return -1 ;
        }
        iacpbl_option.rhost_ip = *(uint32_t *)host->h_addr_list[0];
    }
    ///

    return 0 ;
}
