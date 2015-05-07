/*
 * ACP Middle Layer: Communication Library header
 * 
 * Copyright (c) 2014-2014 Kyushu University
 * Copyright (c) 2014      Institute of Systems, Information Technologies
 *                         and Nanotechnologies 2014
 * Copyright (c) 2014      FUJITSU LIMITED
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 *
 */
/** \file acpcl.h
 * \ingroup acpcl
 */

#ifndef __ACPCL_H__
#define __ACPCL_H__

#include <acp.h>
#include "acpbl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct chreqitem *acp_request_t;

typedef struct chitem *acp_ch_t;

/** @brief Creates an endpoint of a channel to transfer messages from sender to receiver.
 *
 * Creates an endpoint of a channel to transfer messages from sender to receiver, 
 * and returns a handle of it. It returns error if sender and receiver is same, 
 * or the caller process is neither the sender nor the receiver.
 * This function does not wait for the completion of the connection 
 * between sender and receiver. The connection will be completed by the completion 
 * of the communication through this channel. There can be more than one channels 
 * for the same sender-receiver pair.
 *
 * @param sender   rank of the sender process of the channel
 * @param receiver  rank of the receiver process of the channel
 * @retval nonACP_CH_NULL handle of the endpoint of the channel
 * @retval ACP_CH_NULL fail
 */
//extern acp_ch_t acp_create_ch(int src, int dest); [ace-yt3 51]
extern acp_ch_t acp_create_ch(int sender, int receiver);

/** @brief Frees the endpoint of the channel specified by the handle.
 *
 * Frees the endpoint of the channel specified by the handle. 
 * It waits for the completion of negotiation with the counter peer 
 * of the channel for disconnection. It returns error if the caller 
 * process is neither the sender nor the receiver. 
 * Behavior of the communication with the handle of the channel endpoint 
 * that has already been freed is undefined.
 *
 * @param ch handle of the channel endpoint to be freed
 * @retval 0 success
 * @retval -1 fail
 */
extern int acp_free_ch(acp_ch_t ch);

/** @brief Starts a nonblocking free of the endpoint of the channel specified by the handle.
 *
 * It returns error if the caller process is neither the sender nor the receiver. 
 * Otherwise, it returns a handle of the request for waiting the completion of 
 * the free operation. Communication with the handle of the channel endpoint 
 * that has been started to be freed causes an error.
 *
 * @param ch handle of the channel endpoint to be freed
 * @retval nonACP_REQUEST_NULL a handle of the request for waiting the 
 * completion of this operation
 * @retval ACP_REQUEST_NULL fail
 */
extern acp_request_t acp_nbfree_ch(acp_ch_t ch);

/** @brief Blocking send via channels
 *
 * Performs a blocking send of a message through the channel 
 * specified by the handle. It blocks until the message has been stored somewhere 
 * so that the modification to the send buffer does not collapse the message. 
 * It returns error if the sender of the channel endpoint specified by the handle 
 * is not the caller process.
 *
 * @param ch handle of the channel endpoint to send a message
 * @param buf initial address of the send buffer
 * @param size size in byte of the message
 * @retval 0 success
 * @retval -1 fail
 */
extern int acp_send_ch(acp_ch_t ch, void * buf, size_t size);

/** @brief Blocking receive via channels
 *
 * Performs a blocking receive of a message from the channel specified by the handle. 
 * It waits for the arrival of the message. It returns error if the receiver of the 
 * channel endpoint specified by the handle is not the caller process. If the message 
 * is smaller than the size of the receive buffer, only the region of the message size, 
 * starting from the initial address of the receive buffer is modified. If the message 
 * is larger than the size of the receive buffer, the exceeded part of the message is discarded.
 *
 * @param ch handle of the channel endpoint to receive a message
 * @param buf initial address of the receive buffer
 * @param size size in byte of the receive buffer
 * @retval >=0 success. size of the received data
 * @retval -1 fail
 */
extern int acp_recv_ch(acp_ch_t ch, void * buf, size_t size);

/** @brief Non-Blocking send via channels
 *
 * Starts a nonblocking send of a message through the channel specified by the handle. 
 * It returns error if the sender of the channel endpoint specified by the handle is 
 * not the caller process. Otherwise, it returns a handle of the request for waiting 
 * the completion of the nonblocking send. 
 *
 * @param ch handle of the channel endpoint to send a message
 * @param buf initial address of the send buffer
 * @param size size in byte of the message
 * @retval nonACP_REQUEST_NULL a handle of the request for waiting the completion 
 * of this operation
 * @retval ACP_REQUEST_NULL fail
 */
extern acp_request_t acp_nbsend_ch(acp_ch_t ch, void * buf, size_t size);

/** @brief Non-Blocking receive via channels
 *
 * Starts a nonblocking receive of a message through the channel specified by the handle. 
 * It returns error if the receiver of the channel endpoint specified by the handle is 
 * not the caller process. Otherwise, it returns a handle of the request for waiting 
 * the completion of the nonblocking receive. 
 * If the message is smaller than the size of the receive buffer, only the region of 
 * the message size, starting from the initial address of the receive buffer is modified. 
 * If the message is larger than the size of the receive buffer, the exceeded part of 
 * the message is discarded.
 *
 * @param ch handle of the channel endpoint to receive a message
 * @param buf initial address of the receive buffer
 * @param size size in byte of the receive buffer
 * @retval nonACP_REQUEST_NULL a handle of the request for waiting the completion 
 * of this operation
 * @retval ACP_REQUEST_NULL fail
 */
extern acp_request_t acp_nbrecv_ch(acp_ch_t ch, void * buf, size_t size);

/** @brief Waits for the completion of the nonblocking operation 
 *
 * Waits for the completion of the nonblocking operation specified by the request handle. 
 * If the operation is a nonblocking receive, it retruns the size of the received data.
 *
 * @param request 　　handle of the request of a nonblocking operation
 * @retval >=0 success. if the operation is a nonblocking receive, the size of the received data.
 * @retval -1 fail
 */
extern size_t acp_wait_ch(acp_request_t request);

extern int acp_waitall_ch(acp_request_t *, int, size_t *);

#ifdef __cplusplus
}
#endif

#endif
