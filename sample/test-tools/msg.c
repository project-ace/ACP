/*
 * msg.c
 *
 * - About this tool
 *  Msg is a sample tool for message passing between two processes.
 *  This tool is quite slow, but memory efficient.
 *  
 *
 * - Usage
 *  To use these functions, include msg.h in the ACP code,
 *  and link the code with this file, msg.c, along with procinfo.c and lock.c.
 * 
 *  In the code, before performing message passing, all process need to call
 *  msg_setup to setup the facilities. Then any process can send a message to
 *  any process by msg_push. The message can be retrieved by the destination
 *  process by msg_pop. The facilities for Msg can be freed by the function 
 *  msg_free.
 *
 * - Definitions of functions:
 *
 *  -- int msg_setup()
 *    
 *    Setup facilities to enable message passing among processes. This is 
 *    blocking and collective.
 *    
 *  -- int msg_push(int rank, int tag, void *addr, int size)
 *
 *    Send a message represented by tag, addr and size to the destination 
 *    process represented by rank. This is blocking and asynchronous. It 
 *    does not wait for the receiver to receive the message. It just 
 *    guaranteesthat the message to be sent is stored securely in somewhere 
 *    else so that the original region can be overwritten.
 *
 *  -- int msg_pop(int rank, int tag, void *addr, int size)
 *
 *    Receive a message that matches with specified tag and rank. If the 
 *    size of the matching message is different from the size specified 
 *    in this function, the smaller size will be applied. This is blocking
 *    and synchronous. It waits for the arrival of the matching message.
 *
 *  -- int msg_free()
 *
 *    Frees the facilities for message passing.
 *    
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include "acp.h"
#include "procinfo.h"
#include "lock.h"

struct msgbuf{
    acp_list_t localbuf;
    procinfo_t remotebufs;
    void *tmpmsg;
    acp_atkey_t tmpmsgkey;
    acp_ga_t tmpmsgga;
    lock_t lock;
};

struct msgbuf *mb;

#define MSGBUF_HDRSZ 12
int msgbuf_maxsz = 64;

int msg_setup()
{
    int myrank, procs;

    myrank = acp_rank();
    procs = acp_procs();
   
    mb = malloc(sizeof(struct msgbuf));
    mb->localbuf = acp_create_list(myrank);
    if ((mb->localbuf).ga == ACP_GA_NULL){
        fprintf(stderr, "%d: msg_setup : create_list failed\n", myrank);
        free(mb);
        return -1;
    }
    mb->remotebufs = procinfo_create(&(mb->localbuf), sizeof(acp_list_t));
    mb->tmpmsg = malloc(msgbuf_maxsz + MSGBUF_HDRSZ);
    mb->tmpmsgkey = acp_register_memory(mb->tmpmsg, msgbuf_maxsz+MSGBUF_HDRSZ, 0);
    mb->tmpmsgga = acp_query_ga(mb->tmpmsgkey, mb->tmpmsg);
    mb->lock = lock_create();

    return 0;
}

int msg_push(int rank, int tag, void *addr, int size)
{
    int *hdr;
    int myrank;
    acp_list_t buf;
    acp_list_it_t it;
    acp_element_t elem;

    myrank = acp_rank();

    if (size > msgbuf_maxsz){
        fprintf(stderr, "Error: rank %d : msg_push: size %d exceeded the limit %d.\n", 
                myrank, size, msgbuf_maxsz);
        iacp_abort_cl();
    }

    hdr = (int *)(mb->tmpmsg);
    hdr[0] = myrank;
    hdr[1] = tag;
    hdr[2] = size;

    memcpy(mb->tmpmsg+MSGBUF_HDRSZ, addr, size);
    procinfo_queryinfo(mb->remotebufs, rank, &buf);

    elem.ga = mb->tmpmsgga;
    elem.size = size+MSGBUF_HDRSZ;
    lock_acquire(mb->lock, rank);
    acp_push_back_list(buf, elem, rank);
    lock_release(mb->lock, rank);

    return 0;
}

void show_all_msg()
{
    acp_list_t buf;
    acp_list_it_t it;
    int *hdr;
    int i;
    int myrank = acp_rank();

    buf = mb->localbuf;
    hdr = (int *)(mb->tmpmsg);

    i = 0; 

    lock_acquire(mb->lock, acp_query_rank(buf.ga));
    it = acp_begin_list(buf);
    if (it.elem == ACP_GA_NULL){
        fprintf(stderr, "%d:   EMPTY\n", myrank);
    } else{
        while (it.elem != ACP_GA_NULL){
            acp_copy(mb->tmpmsgga, it.elem+24, msgbuf_maxsz + MSGBUF_HDRSZ, ACP_HANDLE_NULL);
            acp_complete(ACP_HANDLE_ALL);
            fprintf(stderr, "%d:   %p  %d %d %d %d\n", 
                    myrank, it.elem, i, hdr[0], hdr[1], hdr[2]);
            i++;
            it = acp_increment_list_it(it);
        }
        fprintf(stderr, "%d:   END\n", myrank);
    }
    lock_release(mb->lock, acp_query_rank(buf.ga));
}

int msg_pop(int rank, int tag, void *addr, int size)
{
    int *hdr;
    acp_list_t buf;
    acp_list_it_t it;
    acp_element_t elem;
    int myrank, realsz;

    myrank = acp_rank();

    if (size > msgbuf_maxsz) {
        fprintf(stderr, "Error: rank %d : msg_pop: size %d exceeded the limit %d.\n", 
                myrank, size, msgbuf_maxsz);
        iacp_abort_cl();
    }

    hdr = (int *)(mb->tmpmsg);
    buf = mb->localbuf;
    
    realsz = -1;
    lock_acquire(mb->lock, acp_query_rank(buf.ga));
    it = acp_begin_list(buf);
    while (it.elem != ACP_GA_NULL){
        elem = acp_dereference_list_it(it);
        acp_copy(mb->tmpmsgga, elem.ga, 
                 msgbuf_maxsz+MSGBUF_HDRSZ, ACP_HANDLE_NULL);
        acp_complete(ACP_HANDLE_ALL);

        if ((rank == hdr[0]) && (tag == hdr[1])){
            realsz = (size < hdr[2]) ? size : hdr[2];
            memcpy(addr, mb->tmpmsg+MSGBUF_HDRSZ, realsz);
            acp_erase_list(it);
            break;
        } else {
           it = acp_increment_list_it(it);
        }
    }
    lock_release(mb->lock, acp_query_rank(buf.ga));

    return realsz;
}

int msg_free()
{
    acp_sync();
    lock_free(mb->lock);
    procinfo_free(mb->remotebufs);
    acp_unregister_memory(mb->tmpmsgkey);
    free(mb->tmpmsg);
    free(mb);
    acp_sync();
}

