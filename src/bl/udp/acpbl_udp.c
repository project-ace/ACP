/*
 * ACP Basic Layer for UDP
 * 
 * Copyright (c) 2014-2014 FUJITSU LIMITED
 * Copyright (c) 2014      Kyushu University
 * Copyright (c) 2014      Institute of Systems, Information Technologies
 *                         and Nanotechnologies 2014
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <acp.h>
#include "acpbl.h"
#include "acpbl_sync.h"
#include "acpbl_udp.h"
#include "acpbl_udp_gmm.h"
#include "acpbl_udp_gma.h"
/* H.Honda Nov.16 2015 begin */
#ifdef MPIACP
#include "mpi.h"
#endif /* MPIACP */
/* H.Honda Nov.16 2015 end   */

static uint16_t my_port;
static uint16_t parent_port;
static uint32_t parent_addr;
static int sock_listen, sock_accept0, sock_accept1, sock_connect;
static int num_child;
static uint64_t sync_sequence_number;

int iacpbludp_my_rank;
int iacpbludp_num_procs;
uint32_t  iacpbludp_taskid;

uint32_t* iacpbludp_rank_table;
uint16_t* iacpbludp_port_table;
uint32_t* iacpbludp_addr_table;

static int iacp_init(void)
{
    struct sockaddr_in addr_listen, addr_accept0, addr_accept1, addr_connect;
    uint32_t rank0, rank1;
    uint32_t taskid0, taskid1;
    uint32_t num_descendant0, num_descendant1, num_descendant;
    uint16_t port0, port1;
    uint32_t my_addr;
    const int option = 1;
    
//    acp_errno = 0;
    
    /* Allocate infrastructure tables */
    
    RANK_TABLE = malloc(NUM_PROCS * sizeof(uint32_t));
    PORT_TABLE = malloc(NUM_PROCS * sizeof(uint16_t));
    ADDR_TABLE = malloc(NUM_PROCS * sizeof(uint32_t));
    debug printf("rank %d - rank_table 0x%016" PRIx64 "\n", MY_RANK, (uint64_t)RANK_TABLE);
    debug printf("rank %d - port_table 0x%016" PRIx64 "\n", MY_RANK, (uint64_t)PORT_TABLE);
    debug printf("rank %d - addr_table 0x%016" PRIx64 "\n", MY_RANK, (uint64_t)ADDR_TABLE);
    
    /* Count branchs */
    
    num_child = NUM_PROCS - MY_RANK - MY_RANK;
    if (num_child < 0) num_child = 0;
    if (num_child > 2) num_child = 2;
    if (MY_RANK == 0) num_child = ((NUM_PROCS > 1) ? 1 : 0);
    debug printf("rank %d - num_child %d\n", MY_RANK, num_child);
    
    sync_sequence_number = TASKID;
    
    /* Establish TCP connection */
    
    if (num_child > 0) {
        unsigned int len;
        
        sock_listen = socket(AF_INET, SOCK_STREAM, 0);
        addr_listen.sin_family = AF_INET;
        addr_listen.sin_port = my_port;
        addr_listen.sin_addr.s_addr = INADDR_ANY;
        setsockopt(sock_listen, SOL_SOCKET, SO_REUSEADDR, (const void*)&option, sizeof(option));
        while (bind(sock_listen, (struct sockaddr *)&addr_listen, sizeof(addr_listen)) < 0)
            if (errno != EADDRINUSE) {
                printf("rank %d - ERROR in bind().\n", MY_RANK);
                exit(-1);
            }
        if (listen(sock_listen, 10) < 0) exit(-1);
        
        len = sizeof(addr_accept0);
        while ((sock_accept0 = accept(sock_listen, (struct sockaddr *)&addr_accept0, &len)) < 0)
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                printf("rank %d - ERROR in accept().\n", MY_RANK);
                exit(-1);
            }
        debug printf("rank %d - accept0(%s)\n", MY_RANK, inet_ntoa(addr_accept0.sin_addr));
        if (num_child > 1)
            while ((sock_accept1 = accept(sock_listen, (struct sockaddr *)&addr_accept1, &len)) < 0)
                if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
                    printf("rank %d - ERROR in accept().\n", MY_RANK);
                    exit(-1);
                }
#ifdef DEBUG
        if (num_child > 1)
            printf("rank %d - accept1(%s)\n", MY_RANK, inet_ntoa(addr_accept1.sin_addr));
#endif
        close(sock_listen);
    }
    if (MY_RANK > 0) {
        sock_connect = socket(AF_INET, SOCK_STREAM, 0);
        addr_connect.sin_family = AF_INET;
        addr_connect.sin_port = parent_port;
        addr_connect.sin_addr.s_addr = parent_addr;
        while (connect(sock_connect, (struct sockaddr *)&addr_connect, sizeof(addr_connect)) < 0)
            if (errno != EINTR && errno != EAGAIN && errno != ECONNREFUSED) {
                printf("rank %d - ERROR in connect(), errno %d.\n", MY_RANK, errno);
                exit(-1);
            } else
                sleep(1);
        debug printf("rank %d - connect(%s)\n", MY_RANK, inet_ntoa(addr_connect.sin_addr));
    }
    
    /* Reduce rank */
    
    if (num_child == 1) {
        while (recv(sock_accept0, &rank0, sizeof(rank0), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
        debug printf("rank %d - recv rank0: %d\n", MY_RANK, rank0);
        if (MY_RANK == 0 && rank0 != 1)
            MY_RANK = ACPBL_UDP_RANK_ERROR;
        if (MY_RANK > 0 && rank0 != MY_RANK + MY_RANK)
            MY_RANK = ACPBL_UDP_RANK_ERROR;
    } else if (num_child == 2) {
        while (recv(sock_accept0, &rank0, sizeof(rank0), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
        while (recv(sock_accept1, &rank1, sizeof(rank1), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
        debug printf("rank %d - recv rank0: %d, rank1: %d\n", MY_RANK, rank0, rank1);
        if (rank0 == MY_RANK + MY_RANK + 1 && rank1 == MY_RANK + MY_RANK) {
            int sock_tmp;
            struct sockaddr_in addr_tmp;
            uint32_t rank_tmp;
            
            sock_tmp = sock_accept1;
            addr_tmp = addr_accept1;
            rank_tmp = rank1;
            sock_accept1 = sock_accept0;
            addr_accept1 = addr_accept0;
            rank1 = rank0;
            sock_accept0 = sock_tmp;
            addr_accept0 = addr_tmp;
            rank0 = rank_tmp;
            debug printf("rank %d - exchange accept0 <-> accept1\n", MY_RANK);
        }
        if (rank0 != MY_RANK + MY_RANK || rank1 != MY_RANK + MY_RANK + 1)
            MY_RANK = ACPBL_UDP_RANK_ERROR;
    }
    if (MY_RANK > 0)
        while (write(sock_connect, &MY_RANK, sizeof(MY_RANK)) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    
    /* Reduce task ID */
    
    taskid0 = taskid1 = TASKID;
    if (num_child > 0)
        while (recv(sock_accept0, &taskid0, sizeof(taskid0), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 1)
        while (recv(sock_accept1, &taskid1, sizeof(taskid1), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
#ifdef DEBUG
    if (num_child == 1)
        printf("rank %d - recv taskid0: 0x%08x\n", MY_RANK, taskid0);
    if (num_child == 2)
        printf("rank %d - recv taskid0: 0x%08x, taskid1: 0x%08x\n", MY_RANK, taskid0, taskid1);
#endif
    if (taskid0 != TASKID && taskid1 != TASKID)
        TASKID = ACPBL_UDP_GPID_ERROR;
    if (MY_RANK > 0)
        while (write(sock_connect, &TASKID, sizeof(TASKID)) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    
    /* Broadcast Rank and GPID Error */
    
    if (MY_RANK > 0)
        while (recv(sock_connect, &TASKID, sizeof(TASKID), MSG_WAITALL) < 0) ;
    else if (MY_RANK == ACPBL_UDP_RANK_ERROR)
        TASKID = ACPBL_UDP_GPID_ERROR;
    if (num_child > 0)
        while (write(sock_accept0, &TASKID, sizeof(TASKID)) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 1)
        while (write(sock_accept1, &TASKID, sizeof(TASKID)) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (TASKID == ACPBL_UDP_GPID_ERROR)
        exit(-1);
    debug printf("rank %d - allreduced taskid 0x%08x\n", MY_RANK, TASKID);
    
    /* Reduce number of descendants */
    
    num_descendant0 = num_descendant1 = 0;
    if (num_child > 0)
        while (recv(sock_accept0, &num_descendant0, sizeof(num_descendant0), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 1)
        while (recv(sock_accept1, &num_descendant1, sizeof(num_descendant1), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    num_descendant = num_child + num_descendant0 + num_descendant1;
    if (MY_RANK > 0)
        while (write(sock_connect, &num_descendant, sizeof(num_descendant)) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    debug printf("rank %d - num_descendant %d\n", MY_RANK, num_descendant);
    
    /* Transfer port number */
    
    if (MY_RANK > 0)
        while (write(sock_connect, &my_port, sizeof(my_port)) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 0)
        while (recv(sock_accept0, &port0, sizeof(port0), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 1)
        while (recv(sock_accept1, &port1, sizeof(port1), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
#ifdef DEBUG
    if (num_child > 0)
        printf("rank %d - branch0 %s:%d num_descendant %d\n", MY_RANK, inet_ntoa(addr_accept0.sin_addr), port0, num_descendant0);
    if (num_child > 1)
        printf("rank %d - branch1 %s:%d num_descendant %d\n", MY_RANK, inet_ntoa(addr_accept1.sin_addr), port1, num_descendant1);
#endif
    
    /* Transfer address */
    
    if (MY_RANK == 1 || (MY_RANK > 1 && (MY_RANK & 1) == 0))
        while (write(sock_connect, &parent_addr, sizeof(parent_addr)) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 0)
        while (recv(sock_accept0, &my_addr, sizeof(my_addr), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (MY_RANK == 0 && NUM_PROCS < 2)
        my_addr = inet_addr("127.0.0.1");
#ifdef DEBUG
    if (num_child > 0) {
        struct in_addr addr;
        addr.s_addr = my_addr;
        debug printf("rank %d - my_addr %s\n", MY_RANK, inet_ntoa(addr));
    }
#endif
    
    /* Gather rank, port and address */
    
    if (MY_RANK > 0) {
        if (num_child > 0) {
            RANK_TABLE[0] = rank0;
            PORT_TABLE[0] = port0;
            ADDR_TABLE[0] = addr_accept0.sin_addr.s_addr;
            if (num_descendant0 > 0) {
                while (recv(sock_accept0, RANK_TABLE + 1, sizeof(RANK_TABLE[0]) * num_descendant0, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
                while (recv(sock_accept0, PORT_TABLE + 1, sizeof(PORT_TABLE[0]) * num_descendant0, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
                while (recv(sock_accept0, ADDR_TABLE + 1, sizeof(ADDR_TABLE[0]) * num_descendant0, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
            }
        }
        if (num_child > 1) {
            RANK_TABLE[1 + num_descendant0] = rank1;
            PORT_TABLE[1 + num_descendant0] = port1;
            ADDR_TABLE[1 + num_descendant0] = addr_accept1.sin_addr.s_addr;
            if (num_descendant1 > 0) {
                while (recv(sock_accept1, RANK_TABLE + 2 + num_descendant0, sizeof(RANK_TABLE[0]) * num_descendant1, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
                while (recv(sock_accept1, PORT_TABLE + 2 + num_descendant0, sizeof(PORT_TABLE[0]) * num_descendant1, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
                while (recv(sock_accept1, ADDR_TABLE + 2 + num_descendant0, sizeof(ADDR_TABLE[0]) * num_descendant1, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
            }
        }
        if (num_descendant > 0) {
            while (write(sock_connect, RANK_TABLE, sizeof(RANK_TABLE[0]) * num_descendant) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
            while (write(sock_connect, PORT_TABLE, sizeof(PORT_TABLE[0]) * num_descendant) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
            while (write(sock_connect, ADDR_TABLE, sizeof(ADDR_TABLE[0]) * num_descendant) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
#ifdef DEBUG
    if (num_descendant > 0) {
        struct in_addr addr;
        int i;
        printf("rank %d - gathered info\n", MY_RANK);
        for (i = 0; i < num_descendant; i++) {
            addr.s_addr = ADDR_TABLE[i];
            printf("rank %d - rank %6d = %s:%d\n", MY_RANK, RANK_TABLE[i], inet_ntoa(addr), PORT_TABLE[i]);
        }
    }
#endif
        }
    } else {
        RANK_TABLE[0] = 0;
        PORT_TABLE[0] = my_port;
        ADDR_TABLE[0] = my_addr;
        RANK_TABLE[1] = 1;
        PORT_TABLE[1] = port0;
        ADDR_TABLE[1] = addr_accept0.sin_addr.s_addr;
        if (num_descendant0 > 0) {
            int i;
            while (recv(sock_accept0, RANK_TABLE + 2, sizeof(RANK_TABLE[0]) * num_descendant0, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
            for (i = 2; i < NUM_PROCS; i++)
                while (recv(sock_accept0, PORT_TABLE + RANK_TABLE[i], sizeof(PORT_TABLE[0]), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
            for (i = 2; i < NUM_PROCS; i++)
                while (recv(sock_accept0, ADDR_TABLE + RANK_TABLE[i], sizeof(ADDR_TABLE[0]), MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
        }
    }
    
    /* Broadcast port and address tables */
    
    if (MY_RANK > 0) {
        while (recv(sock_connect, PORT_TABLE, sizeof(PORT_TABLE[0]) * NUM_PROCS, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
        while (recv(sock_connect, ADDR_TABLE, sizeof(ADDR_TABLE[0]) * NUM_PROCS, MSG_WAITALL) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    }
    if (num_child > 0) {
        while (write(sock_accept0, PORT_TABLE, sizeof(PORT_TABLE[0]) * NUM_PROCS) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
        while (write(sock_accept0, ADDR_TABLE, sizeof(ADDR_TABLE[0]) * NUM_PROCS) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    }
    if (num_child > 1) {
        while (write(sock_accept1, PORT_TABLE, sizeof(PORT_TABLE[0]) * NUM_PROCS) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
        while (write(sock_accept1, ADDR_TABLE, sizeof(ADDR_TABLE[0]) * NUM_PROCS) < 0) if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    }
#ifdef DEBUG
    {
        struct in_addr addr;
        int i;
        printf("rank %d - broadcasted info\n", MY_RANK);
        for (i = 0; i < NUM_PROCS; i++) {
            addr.s_addr = ADDR_TABLE[i];
            printf("rank %d - rank %6d = %s:%d\n", MY_RANK, i, inet_ntoa(addr), PORT_TABLE[i]);
        }
    }
#endif
    
    /* Initialize GSM and GMA */
    
    if (iacpbludp_init_gmm()) return -1;
    
    if (iacpbludp_init_gma()) return -1;
    acp_sync();
    
    /* Initialize Middle Layer */
    
///    if (iacp_init_dl()) return -1;
///    if (iacp_init_cl()) return -1;
    /* if (iacp_init_vd()) return -1; */
    
    return 0;
}

int acp_init(int* argc, char*** argv)
{
/* H.Honda Nov.16 2015 begin */
    ///int myrank, nprocs ;
    int      rank, offset_rank,   nprocs,   rank_runtime, size_smem, myrank_runtime, nprocs_runtime ;
    int64_t  rank64, offset_rank64, nprocs64, rank_runtime64, size_smem64 ;
    uint64_t lport64, rport64, taskid64 ;
    uint32_t taskid ;
    uint16_t lport, rport ;
    char     rhost[ BUFSIZ ] ;
    ///fprintf( stderr, "zero: *argc: %d\n", *argc ) ;
///
#ifdef MPIACP
    if ( (*argc >= 4) &&
         (strcmp( (*argv)[ 1 ], "--acp-multrun" ) == 0)) {
        char buf[ BUFSIZ ] ;
        char *filename = strdup( (*argv)[ 2 ] ) ;
        offset_rank = atoi( (*argv)[ 3 ] ) ;
///
        MPI_Comm_rank( MPI_COMM_WORLD, &myrank_runtime ) ;
        MPI_Comm_size( MPI_COMM_WORLD, &nprocs_runtime ) ;
        ///fprintf( stderr, "first: np, me, offset: %d, %d, %d\n", nprocs_runtime, myrank_runtime, offset_rank ) ;
///
        {
            int  i ;
            FILE *fp = fopen( filename, "r" ) ;
            if ( fp == (FILE *)NULL ) {
               fprintf( stderr, "File: %s :open error:\n", filename ) ;
               exit( 1 ) ;
            }
            i = 0 ;
            while( fgets( buf, BUFSIZ, fp ) != NULL ) {
                ///fprintf ( stderr, "%4d%4d : buf %s\n", offset_rank, myrank_runtime, buf ) ;
                if ( i >= (myrank_runtime + offset_rank)) {
                    break ;
                }
                i++ ;
            }
            free( filename ) ;
        }
        sscanf( buf, "%ld %ld %lu %lu %s %lu %ld",  &rank64, &nprocs64, &lport64, &rport64, rhost, &taskid64, &size_smem64 ) ;
        ///fprintf ( stderr, "read: %s => read: %d%d: %4d%4d%10lu%10lu %s %10lu%10ld\n", buf, myrank_runtime, offset_rank, rank64, nprocs64, lport64, rport64, rhost, taskid64, size_smem64 ) ;
        rank                = ( int ) rank64 ;
        nprocs              = ( int ) nprocs64 ;
        lport               = ( uint16_t ) lport64 ;
        rport               = ( uint16_t ) rport64 ;
        taskid              = ( uint32_t ) taskid64 ;
        size_smem           = ( int ) size_smem64 ;
        ///fprintf ( stderr, "rank: %6d%6d:%6d%6d:%7u%7u %10u%10ld   %s\n", myrank_runtime, offset_rank, rank, nprocs, lport, rport, taskid, size_smem, rhost ) ;
///
        MY_RANK     = offset_rank + myrank_runtime ;
        NUM_PROCS   = nprocs ;
        TASKID      = taskid ;
        SMEM_SIZE   = size_smem ;
        my_port     = lport ;
        parent_port = rport ;
        parent_addr = inet_addr( rhost );
        if ( parent_addr == 0xffffffff ) {
            struct hostent *host;
            if ((host = gethostbyname( rhost )) == NULL) return -1;
            parent_addr = *(uint32_t *)host->h_addr_list[0];
        }
///
        (*argv)[3] = (*argv)[0];
        (*argc) -= 3 ;
        (*argv) += 3 ;
///
    } else {
#endif /* MPIACP */
    if (( *argc >= 8) &&
               (strcmp( (*argv)[ 1 ], "--acp-options" ) == 0)) {
        MY_RANK     = strtol((*argv)[2], NULL, 0);
        NUM_PROCS   = strtol((*argv)[3], NULL, 0);
        TASKID      = strtol((*argv)[4], NULL, 0);
        SMEM_SIZE   = strtol((*argv)[5], NULL, 0);
        my_port     = strtol((*argv)[6], NULL, 0);
        parent_port = strtol((*argv)[7], NULL, 0);
        parent_addr = inet_addr((*argv)[8]);
        if (parent_addr == 0xffffffff) {
            struct hostent *host;
            if ((host = gethostbyname((*argv)[8])) == NULL) return -1;
            parent_addr = *(uint32_t *)host->h_addr_list[0];
        }
///
        (*argv)[8] = (*argv)[0];
        (*argc) -= 8 ;
        (*argv) += 8 ;
///
    } else {
        MY_RANK     = strtol(getenv("ACP_MYRANK"),          NULL ,0);
        NUM_PROCS   = strtol(getenv("ACP_NUMPROCS"),        NULL, 0);
        TASKID      = strtol(getenv("ACP_TASKID"),          NULL, 0);
        SMEM_SIZE   = strtol(getenv("ACP_STARTER_MEMSIZE"), NULL, 0);
        my_port     = strtol(getenv("ACP_LPORT"),           NULL, 0);
        parent_port = strtol(getenv("ACP_RPORT"),           NULL, 0);
        parent_addr = inet_addr(getenv("ACP_RHOST"));
        if (parent_addr == 0xffffffff) {
            struct hostent *host;
            if ((host = gethostbyname(getenv("ACP_RHOST"))) == NULL) return -1;
            parent_addr = *(uint32_t *)host->h_addr_list[0];
        }
    }
#ifdef MPIACP
    }
#endif /* MPIACP */
///
///    fprintf ( stderr, "final: %d: %10d%10u%10d%10u%10u%20x\n", MY_RANK, NUM_PROCS, TASKID, SMEM_SIZE, my_port, parent_port, parent_addr ) ;
/* H.Honda Nov.16 2015 end   */
 
/* original version
 *  if (*argc < 8) return -1;
 *  MY_RANK     = strtol((*argv)[1], NULL, 0);
 *  NUM_PROCS   = strtol((*argv)[2], NULL, 0);
 *  TASKID      = strtol((*argv)[3], NULL, 0);
 *  SMEM_SIZE   = strtol((*argv)[4], NULL, 0);
 *  my_port     = strtol((*argv)[5], NULL, 0);
 *  parent_port = strtol((*argv)[6], NULL, 0);
 *  parent_addr = inet_addr((*argv)[7]);
 *  if (parent_addr == 0xffffffff) {
 *      struct hostent *host;
 *      if ((host = gethostbyname((*argv)[7])) == NULL) return -1;
 *      parent_addr = *(uint32_t *)host->h_addr_list[0];
 *  }
 *  
 *  (*argv)[7] = (*argv)[0];
 *  *argc -= 7;
 *  *argv += 7;
 */
    debug printf("rank %d - args: procs %d, taskid 0x%x, sseg_size %d, port 0x%x, pport 0x%x, paddr 0x%x\n",
                 MY_RANK, NUM_PROCS, TASKID, SMEM_SIZE, my_port, parent_port, parent_addr);
 
    return iacp_init();
}

static int iacp_finalize(void)
{
    unsigned char buf[256];
    
    /* Finalize GMA and GSM */
    
    iacpbludp_finalize_gma();
    
    iacpbludp_finalize_gmm();
    
    /* Close TCP connection */
    
    if (num_child > 0){
        while (recv(sock_accept0, buf, sizeof(buf), MSG_WAITALL));
        close(sock_accept0);
    }
    if (num_child > 1){
        while (recv(sock_accept1, buf, sizeof(buf), MSG_WAITALL));
        close(sock_accept1);
    }
    if (MY_RANK > 0){
        shutdown(sock_connect, SHUT_RDWR);
        while (recv(sock_connect, buf, sizeof(buf), MSG_WAITALL));
        close(sock_connect);
    }
    
    return 0;
}

static void iacp_abort(void)
{
    /* Abort GMA and GSM */
    
    iacpbludp_abort_gma();
    
    iacpbludp_abort_gmm();
    
    /* Shutdown TCP connection */
    
    if (num_child > 0){
        shutdown(sock_accept0, SHUT_RDWR);
        close(sock_accept0);
    }
    if (num_child > 1){
        shutdown(sock_accept1, SHUT_RDWR);
        close(sock_accept1);
    }
    if (MY_RANK > 0){
        shutdown(sock_connect, SHUT_RDWR);
        close(sock_connect);
    }
    
    return;
}

int acp_finalize(void)
{
    int r;
    
    /* iacp_finalize_vd(); */
///    iacp_finalize_cl();
///    iacp_finalize_dl();
    
    acp_complete(ACP_HANDLE_ALL);
    acp_sync();
    r = iacp_finalize();
    
    free(RANK_TABLE);
    free(PORT_TABLE);
    free(ADDR_TABLE);
    RANK_TABLE = NULL;
    PORT_TABLE = NULL;
    ADDR_TABLE = NULL;
    
    return r;
}

int acp_reset(int rank)
{
    MY_RANK = rank;
    
    if (iacp_finalize()) return -1;
    
    return iacp_init();
}

void acp_abort(const char* str)
{
    /* iacp_abort_vd(); */
///    iacp_abort_cl();
///    iacp_abort_dl();
    
    iacp_abort();
    
    free(RANK_TABLE);
    free(PORT_TABLE);
    free(ADDR_TABLE);
    RANK_TABLE = NULL;
    PORT_TABLE = NULL;
    ADDR_TABLE = NULL;
    
    return;
}

int acp_sync(void)
{
    uint64_t seq0, seq1;
    
    /* Reduce sequence number */
    
    seq0 = seq1 = sync_sequence_number;
    if (num_child > 0)
        while (recv(sock_accept0, &seq0, sizeof(uint64_t), MSG_WAITALL) < 0)
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 1)
        while (recv(sock_accept1, &seq1, sizeof(uint64_t), MSG_WAITALL) < 0)
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (seq0 != sync_sequence_number || seq1 != sync_sequence_number) exit(-1);
    if (MY_RANK > 0) {
        while (write(sock_connect, &sync_sequence_number, sizeof(uint64_t)) < 0)
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    } else
        sync_sequence_number++;
    
    /* Broadcast result */
    
    if (MY_RANK > 0)
        while (recv(sock_connect, &sync_sequence_number, sizeof(uint64_t), MSG_WAITALL) < 0)
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 0)
        while (write(sock_accept0, &sync_sequence_number, sizeof(uint64_t)) < 0)
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    if (num_child > 1)
        while (write(sock_accept1, &sync_sequence_number, sizeof(uint64_t)) < 0)
            if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) exit(-1);
    
    return 0;
}

int acp_rank(void)
{
    return MY_RANK;
}

int acp_procs(void)
{
    return NUM_PROCS;
}

int acp_colors(void)
{
    return 1;
}

