#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"

///#define DEBUG

acp_atkey_t iacp_register_key_query_ga_ws( size_t size, void *array, int color, acp_atkey_t *key, acp_ga_t *ga ) ;
int iacp_dump_ws ( acp_wsd_t wsd, FILE *fp, char *comment ) ;
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

#define _WSD_OFFSET_STARTER_   0x96

///#define PROC_START_INIT     -100 
///#define SIZE_DEFAULT_INIT   0x1073741824      /// 10MB

#define PROC_START_INIT        0
#define SIZE_DEFAULT_INIT      0x1048576         /// 1MB

static int64_t proc_start_global   = PROC_START_INIT ;
static int64_t size_default_global = SIZE_DEFAULT_INIT ;

static int setup_dat_ws( acp_wsd_t wsd, size_t size_rw, size_t offset_rw, acp_ga_t ga_base, acp_ga_t *ga_src ) ;
static int setup_wsd_ws( acp_wsd_t wsd, size_t size_rw, size_t offset_rw, size_t *offsets,  size_t *sizes    ) ;

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
int iacp_init_ws()
{
    return 0 ;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
acp_atkey_t iacp_register_key_query_ga_ws( size_t size, void *array, int color, acp_atkey_t *key, acp_ga_t *ga )
{
    *key = acp_register_memory( array, size, color ) ;
    *ga  = acp_query_ga( *key, array ) ;
    return *key ;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
int acp_setparams_ws( size_t proc_start, size_t size_default )
{
    int myrank  = acp_rank( ) ;
//  if ( proc_start < 0 ) {
//      fprintf( stderr, "acp_setparams_ws: Error: rank%4d : proc_start:%8lu < 0 \n",   myrank, proc_start ) ;
//      exit ( 1 ) ;
//  }
    if ( size_default <= 0 ) {
        fprintf( stderr, "acp_setparams_ws: Error: rank%4d : size_default:%8lu < 0 \n", myrank, size_default ) ;
        exit ( 1 ) ;
    }
    proc_start_global   = proc_start ;
    size_default_global = size_default ;
    return 0 ;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
acp_wsd_t acp_create_ws( size_t size_ws )
{
    int          i ;
    acp_atkey_t  lkey ;
    acp_handle_t hdl ;
    size_t       offset_starter ;
    acp_ga_t     ga_src, ga_dst ;
    acp_ga_t     lga    = 0 ;
    int          myrank = acp_rank( ) ;
    int          nprocs = acp_procs( ) ;
    acp_wsd_t    wsd    = ( acp_wsd_t ) malloc( sizeof( acp_wsditem ) ) ;

    ////////////////////////////////////////////////
    offset_starter      = _WSD_OFFSET_STARTER_ ;

    ////////////////////////////////////////////////
    wsd->size_ws        = size_ws ;
    wsd->size_default   = size_default_global ;
    wsd->ngas           = size_ws / wsd->size_default + 1 ;
    wsd->size_remainder = size_ws % wsd->size_default ;

    ////////////////////////////////////////////////
    wsd->keys           = ( acp_atkey_t * ) malloc( wsd->ngas * sizeof( acp_atkey_t ) ) ;
    wsd->gas            = ( acp_ga_t    * ) malloc( wsd->ngas * sizeof( acp_ga_t    ) ) ;
    wsd->procs          = ( int         * ) malloc( wsd->ngas * sizeof( int         ) ) ;
    wsd->sizes          = ( int         * ) malloc( wsd->ngas * sizeof( int         ) ) ;

    ////////////////////////////////////////////////
    if ( (proc_start_global + wsd->ngas) > nprocs ) {
        fprintf( stderr, "acp_create_ws: Error: rank%4d : (proc_start_global%8lu + n_ws%8lu ) > nprocs%4d:\n",
                 myrank, proc_start_global, wsd->ngas, nprocs ) ;
        exit ( 1 ) ;
    }

    for ( i = 0 ; i < (wsd->ngas - 1) ; i++ ) {
        wsd->procs[ i ]           = proc_start_global ;
        if ( wsd->size_ws <= wsd->size_default ) {
            wsd->sizes[ i ]       = wsd->size_ws ;
        } else {
            wsd->sizes[ i ]       = wsd->size_default ;
        }
        proc_start_global++ ;
    }
    if ( wsd->size_remainder != 0 ) {
        wsd->procs[ wsd->ngas-1 ] = proc_start_global ;
        wsd->sizes[ wsd->ngas-1 ] = wsd->size_remainder ;
        proc_start_global++ ;
    }

#ifdef DEBUG
    if ( myrank == 0 ) {
        for ( i = 0 ; i < wsd->ngas ; i++ ) {
            fprintf( stderr, "iws%4d, ngas%4lu, procs%4d, sizes%10d\n",
                     i, wsd->ngas, wsd->procs[ i ], wsd->sizes[ i ] ) ;
        }
    }
#endif

    ////////////////////////////////////////////////
    if ( wsd->ngas >= 2 ) {
        if ( wsd->procs[ 0 ] <= myrank && myrank < wsd->procs[ wsd->ngas - 1 ] ) {
            if ( wsd->size_ws <= wsd->size_default ) {
                wsd->data = ( void * ) malloc( wsd->size_ws ) ;
            } else {
                wsd->data = ( void * ) malloc( wsd->size_default ) ;
            }
        }
        if ( myrank == wsd->procs[ wsd->ngas - 1 ] ) {
                wsd->data = ( void * ) malloc( wsd->size_remainder ) ;
        }
    }
    if ( wsd->ngas == 1 ) {
        if ( myrank == wsd->procs[ wsd->ngas - 1 ] ) {
                wsd->data = ( void * ) malloc( wsd->size_ws ) ;
        }
    }
    
    ////////////////////////////////////////////////
    wsd->ga_data = 0x0 ;
    if ( wsd->ngas >= 2 ) {
        if ( wsd->procs[ 0 ] <= myrank && myrank < wsd->procs[ wsd->ngas - 1 ] ) {
            if ( wsd->size_ws <= wsd->size_default ) {
                iacp_register_key_query_ga_ws( wsd->size_ws,        wsd->data, 0, &(wsd->key_data), &(wsd->ga_data) ) ;
            } else {
                iacp_register_key_query_ga_ws( wsd->size_default,   wsd->data, 0, &(wsd->key_data), &(wsd->ga_data) ) ;
            }
#ifdef DEBUG
                fprintf( stderr, "0.0: rank%4d, ga %16lx\n", myrank, wsd->ga_data ) ;
#endif
        }
        if ( myrank == wsd->procs[ wsd->ngas - 1 ] ) {
                iacp_register_key_query_ga_ws( wsd->size_remainder, wsd->data, 0, &(wsd->key_data), &(wsd->ga_data) ) ;
#ifdef DEBUG
                fprintf( stderr, "0.1: rank%4d, ga %16lx\n", myrank, wsd->ga_data ) ;
#endif
        }
    }
    if ( wsd->ngas == 1 ) {
        if ( myrank == wsd->procs[ wsd->ngas - 1 ] ) {
                iacp_register_key_query_ga_ws( wsd->size_ws,        wsd->data, 0, &(wsd->key_data), &(wsd->ga_data) ) ;
#ifdef DEBUG
                fprintf( stderr, "0.2: rank%4d, ga %16lx\n", myrank, wsd->ga_data ) ;
#endif
        }
    }

    memcpy( acp_query_address( acp_query_starter_ga( myrank ) + offset_starter ), &(wsd->ga_data), sizeof( acp_ga_t ) ) ;
    acp_sync() ;

    ////////////////////////////////////////////////
#ifdef DEBUG
    for ( i = 0 ; i < wsd->ngas ; i++ ) {
        fprintf( stderr, "acp_create_ws: rank:%4d, wsd->ngas:%4d, ga[ %4d ] = %16lx\n", myrank, wsd->ngas, i, wsd->gas[ i ] ) ;
    }
#endif
    iacp_register_key_query_ga_ws( sizeof( acp_ga_t ), wsd->gas, 0, &lkey, &lga ) ;
    for ( i = 0 ; i < wsd->ngas ; i++ ) {
        ga_dst = lga + sizeof( acp_ga_t ) * i ;
        ga_src = acp_query_starter_ga( wsd->procs[ i ] ) + offset_starter ;
        hdl    = acp_copy( ga_dst, ga_src, sizeof( acp_ga_t ), ACP_HANDLE_NULL ) ;
    }
    acp_complete( hdl ) ;
    acp_sync() ;

#ifdef DEBUG
    for ( i = 0 ; i < wsd->ngas ; i++ ) {
        fprintf( stderr, "rank%4d, ga[ %4d ] %16lx\n", myrank, i, wsd->gas[ i ] ) ;
    }
#endif
    
    ///
    return wsd ;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
void acp_free_ws ( acp_wsd_t wsd )
{
    free( wsd->keys  ) ;
    free( wsd->gas   ) ;
    free( wsd->procs ) ;
    free( wsd->sizes ) ;
    free( wsd->data  ) ;
    free( wsd        ) ;
}

void acp_destroy_ws ( acp_wsd_t wsd )
{
    acp_free_ws ( wsd ) ;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
static int setup_dat_ws( acp_wsd_t wsd, size_t size_rw, size_t offset_rw, acp_ga_t ga_base, acp_ga_t *ga_src )
{
    size_t i, proc_s, proc_e ;
    size_t size_default, remainder_offset ;//, remainder_data ;
    size_t ngas ;
///
    ngas             = wsd->ngas ;
    size_default     = wsd->size_default ;
    remainder_offset = offset_rw % size_default ;
//  remainder_data   = ( offset_rw + size_rw ) % size_default ;
    proc_s           = offset_rw / size_default ;
    proc_e           = ( offset_rw + size_rw ) / size_default ;
///
    for ( i = 0 ; i < proc_s ; i++ ) {
        ga_src[ i ]  = 0 ;
    }
    {
        i = proc_s ;
        ga_src[ i ]  = ga_base ;
    }
    for ( i = proc_s + 1 ; i < proc_e ; i++ ) {
        ga_src[ i ]  = ga_base + ( size_default - remainder_offset ) + size_default * ( i - ( proc_s + 1 ) ) ;
    }
    {
        i = proc_e ;
        ga_src[ i ]  = ga_base + ( size_default - remainder_offset ) + size_default * ( i - ( proc_s + 1 ) ) ;
    }
    for ( i = proc_e + 1 ; i < ngas ; i++ ) {
        ga_src[ i ]  = 0 ;
    }
#ifdef DEBUG
    for ( i = 0 ; i < ngas ; i++ ) {
        fprintf( stderr, "setup_dat_ws: ga_src[ %4lu ] = %16lx\n", i, ga_src[ i ] ) ;
    }
#endif
    return 0 ;
}

static int setup_wsd_ws ( acp_wsd_t wsd, size_t size_rw, size_t offset_rw, size_t *offsets, size_t *sizes )
{
    size_t i, proc_s, proc_e, remainder_offset, remainder_data ;
    size_t ngas,  size_default ;
///
    ngas             = wsd->ngas ;
    size_default     = wsd->size_default ;
    remainder_offset = offset_rw % size_default ;
    remainder_data   = ( offset_rw + size_rw ) % size_default ;
    proc_s           = offset_rw / size_default ;
    proc_e           = ( offset_rw + size_rw ) / size_default ;
///
#ifdef DEBUG
    fprintf( stderr, "proc_s, proc_e: %4lu, %4lu\n", proc_s, proc_e ) ;
#endif
    for ( i = 0 ; i < proc_s ; i++ ) {
        offsets[ i ] = 0 ;
        sizes  [ i ] = 0 ;
    }
    {
        i = proc_s ;
        sizes  [ i ] = size_default - remainder_offset ;
        offsets[ i ] = remainder_offset ;
    }
    for ( i = proc_s + 1 ; i < proc_e ; i++ ) {
        sizes  [ i ] = size_default ;
        offsets[ i ] = 0 ;
    }
    {
        i = proc_e ;
        sizes  [ i ] = remainder_data ;
        offsets[ i ] = 0 ;
    }
    for ( i = proc_e + 1 ; i < ngas ; i++ ) {
        sizes  [ i ] = 0 ;
        offsets[ i ] = 0 ;
    }
#ifdef DEBUG
    for ( i = 0 ; i < ngas ; i++ ) {
        fprintf( stderr, "setup_wsd_ws: sizes[ %4lu ] = %8lu,   offsets[ %4lu ] = %8lu\n", i, sizes[ i ], i, offsets[ i ] ) ;
    }
#endif
    return 0 ;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
int acp_read_ws ( acp_wsd_t wsd, acp_ga_t ga, size_t size, size_t offset )
{
    size_t       i ;
    acp_handle_t hdl ;
    size_t       *sizes_src   = ( size_t   * ) malloc( wsd->ngas * sizeof( size_t ) ) ;
    size_t       *offsets_src = ( size_t   * ) malloc( wsd->ngas * sizeof( size_t ) ) ;
    acp_ga_t     *gas_dst     = ( acp_ga_t * ) malloc( wsd->ngas * sizeof( size_t ) ) ;
///
    setup_dat_ws( wsd, size, offset, ga,          gas_dst   ) ;
    setup_wsd_ws( wsd, size, offset, offsets_src, sizes_src ) ;
///
    for ( i = 0 ; i < wsd->ngas ; i++ ) {
        if ( sizes_src[ i ] > 0 ) {
            hdl = acp_copy( gas_dst[ i ], wsd->gas[ i ] + offsets_src[ i ], sizes_src[ i ], ACP_HANDLE_NULL ) ;
        }
    }
    acp_complete( hdl ) ;
///
    free( gas_dst     ) ;
    free( sizes_src   ) ;
    free( offsets_src ) ;
///
    return 0 ;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
int acp_write_ws ( acp_wsd_t wsd, acp_ga_t ga, size_t size, size_t offset )
{
    int          i ;
    acp_handle_t hdl ;
    size_t       *sizes_dst   = ( size_t   * ) malloc( wsd->ngas * sizeof( size_t ) ) ;
    size_t       *offsets_dst = ( size_t   * ) malloc( wsd->ngas * sizeof( size_t ) ) ;
    acp_ga_t     *gas_src     = ( acp_ga_t * ) malloc( wsd->ngas * sizeof( size_t ) ) ;
///
    setup_dat_ws( wsd, size, offset, ga,          gas_src   ) ;
    setup_wsd_ws( wsd, size, offset, offsets_dst, sizes_dst ) ;
///
#ifdef DEBUG
    int myrank = acp_rank() ;
    {
        double *dp = ( double * ) acp_query_address( ga ) ;
        fprintf( stderr, "data: addr dp = %p\n", dp ) ;
        for ( i = 0 ; i < size/sizeof(double) ; i++ ) {
            fprintf( stderr, "data: dp[ %4d ] = %26.16e\n", i, dp[ i ] ) ;
        }
    }
#endif
///
#ifdef DEBUG
    fprintf( stderr, "W: rank%4d ,  ga: %16lx\n", myrank, ga ) ;
#endif
    for ( i = 0 ; i < wsd->ngas ; i++ ) {
#ifdef DEBUG
        fprintf( stderr, "W: rank%4d,  ga_dst[ %4d ] %16lx (%16lx + %8lu),  ga_src[ %4d ] %16lx,   sizes_dst[ %4d ] %8lu\n",
                 myrank, i, wsd->gas[ i ] + offsets_dst[ i ], wsd->gas[ i ], offsets_dst[ i ],
                         i, gas_src[ i ], i, sizes_dst[ i ] ) ;
#endif
        if ( sizes_dst[ i ] > 0 ) {
            hdl = acp_copy( wsd->gas[ i ] + offsets_dst[ i ], gas_src[ i ], sizes_dst[ i ], ACP_HANDLE_NULL ) ;
        }
    }
    acp_complete( hdl ) ;
///
    free( gas_src     ) ;
    free( sizes_dst   ) ;
    free( offsets_dst ) ;
///
    return 0 ;
}

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
int iacp_dump_ws ( acp_wsd_t wsd, FILE *fp, char *comment )
{
    int i, id, myrank ;
    size_t count ;
///
    myrank     = acp_rank() ;
    if ( wsd->procs[ 0 ] <= myrank && myrank <= wsd->procs[ wsd->ngas - 1 ] ) {
        id     = myrank - wsd->procs[ 0 ] ;
        count  = wsd->sizes[ id ] / sizeof( double ) ;
    } else {
        id     = -1 ;
        count  = 0 ;
    }

    fprintf( fp, "%s rank%4d,  id%4d,  count%6lu\n", comment, myrank, id, count ) ;
    for ( i = 0 ; i < count ; i++ ) {
        fprintf( fp, "%s %6d%26.16e\n", comment, i, (( double * ) wsd->data)[ i ] ) ;
    }
    return 0 ;
}
