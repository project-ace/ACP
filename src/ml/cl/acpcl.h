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

typedef struct segbufitem *acp_segbuf_t;


/** 
 * @brief Creates the source side endpoint of a pair of segmented buffers.
 * 
 * Creates the source side end point of a pair of segmented buffers. It returns error if the 
 * values of the parameters are invalid. The destination rank (dst_rank) cannot be the rank
 * it self. Also, it returns error if the internal resource for connecting segmented buffers
 * is not sufficient. Otherwise, it returns a handle of the segmented buffer. This function
 * just prepares the local side of the segmented buffer. Connection of the source and the 
 * destination side of the segmented buffer should be done explicitly, after this function.
 * There can be more than one segmented buffers with the same destination rank.
 * 
 * @param dst_rank  Rank of the destination process of the pair of segmented buffer.
 * @param buf       Local buffer for this endpoint. Provided from the application.
 * @param segsize   Size of the segment.
 * @param segnum    Number of segments in a buffer.
 * @retval ACP_SEGBUF_NULL  fail
 * @retval otherwise        A handle of the endpoint of the segmented buffer.
 */
extern acp_segbuf_t acp_create_src_segbuf(int dst_rank, void *buf, size_t segsize, size_t segnum);

/** 
 * @brief Creates the destination side endpoint of a pair of segmented buffers.
 * 
 * Creates the destination side end point of a pair of segmented buffers. It returns error if 
 * the  values of the parameters are invalid. The source rank (dst_rank) cannot be the rank 
 * itself. Also, it returns error if the internal resource for connecting segmented buffers 
 * is not sufficient. Otherwise, it returns a handle of the segmented buffer. This function 
 * just prepares the local side of the segmented buffer. Connection of the source and the
 * destination side of the segmented buffer should be done explicitly, after this 
 * function. There can be more than one segmented buffers with the same source rank.
 * 
 * @param src_rank  Rank of the source process of the pair of segmented buffer.
 * @param buf       Local buffer for this endpoint. Provided from the application.
 * @param segsize   Size of the segment.
 * @param segnum    Number of segments in a buffer.
 * @retval ACP_SEGBUF_NULL  Fail
 * @retval otherwise        A handle of the endpoint of the segmented buffer.
 */
extern acp_segbuf_t acp_create_dst_segbuf(int dst_rank, void *buf, size_t segsize, size_t segnum);

/** 
 * @brief Connect the pair of segmented buffer
 * 
 * Connect the end point of the pair of segmented buffers with the other end. This function
 * is blocking, which waits for the completion of the connection. Segmented data transfer
 * with the specified handle becomes available after this function. 
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval 0  Success
 * @retval 1  Fail
 */
extern int acp_connect_segbuf(acp_segbuf_t segbuf);

/** 
 * @brief Query if the pair of segmented buffer are connected
 * 
 * Check the connection between the source and the destination of the pair of segmented 
 * buffers.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval 0  The segmented buffer is connected.
 * @retval 1  The segmented buffer is not connected.
 */
extern int acp_isconnected_segbuf(acp_segbuf_t segbuf);

/** 
 * @brief Notify to the source side that one segment has become ready to be overwritten.
 * 
 * Notify from the destination side to the source side that one segment at the head has 
 * become ready to be overwritten. At the creation of the endpoint, all segments had been
 * initialized as ready to be  overwritten. Then, segments become not ready by 
 * acp_ack_segbuf function. acp_ready_segbuf sets the segment at the head to be ready 
 * again, and increments the head of the segmented buffer by one.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval 0  Success
 * @retval 1  Fail
 */
extern int acp_ready_segbuf(acp_segbuf_t segbuf);

/** 
 * @brief Acknowledge the destination side that one segment has become available to retrieve.
 * 
 * Send an acknowledgement from the source side to the destination side that one segment
 * has become available to be retrieved at the destination side. This function increments
 * the tail of the segmented bffer by one.
 *
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval 0  Success.
 * @retval 1  Fail
 */
extern int acp_ack_segbuf(acp_segbuf_t segbuf);

/** 
 * @brief Query if there is any segments that are ready to be overwritten.
 * 
 * Check if there is any segments that are ready to be overwritten.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval 0  There is at least one segment ready to be overwritten.
 * @retval 1  There is no ready segment.
 */
extern int acp_isready_segbuf(acp_segbuf_t segbuf);

/** 
 * @brief Query if there is any segments that are available to be retrieved.
 * 
 * Check if there is any segments that are available to be retrieved by the 
 * destination rank.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval 0  The segmented buffer is connected.
 * @retval 1  The segmented buffer is not connected.
 */
extern int acp_isacked_segbuf(acp_segbuf_t segbuf);

/** 
 * @brief Query the head index of the segmented buffer.
 * 
 * Return the index that points to the head of the segmented buffer.
 * Head represents the position in a segmented buffer from which valid data
 * in the segmented buffer begins.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval -1 Head index is not available.
 * @retval otherwise  The index of the head.
 */
extern int64_t acp_query_segbuf_head(acp_segbuf_t segbuf);

/** 
 * @brief Query the tail index of the segmented buffer.
 * 
 * Return the index that points to the tail of the segmented buffer.
 * Tail represents the position in a segmented buffer until which valid data
 * in the segmented buffer begins.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval -1 Tail index is not available.
 * @retval otherwise  The index of the tail.
 */
extern int64_t acp_query_segbuf_tail(acp_segbuf_t segbuf);

/** 
 * @brief Query the index that represents the point where data has been already sent.
 * 
 * Return the index that points to the position in the segmented buffer until which
 * the data transfer has been completed. This function is only available to the 
 * source rank, because this function is used to check whether some of the segments
 * at the source side can be overwritten with the new value.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval -1 Sent index is not available.
 * @retval otherwise  The index of the sent.
 */
extern int64_t acp_query_segbuf_sent(acp_segbuf_t segbuf);

/** 
 * @brief Disconnect a segmented buffer
 * 
 * Disconnect the segmented buffer specfied by segbuf. After this function, the
 * specified segmented buffer cannot be used.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval 0  Success
 * @retval 1  Fail
 */
extern int acp_disconnect_segbuf(acp_segbuf_t segbuf);

/** 
 * @brief Free the endpoint of a segmented buffer
 * 
 * Free the endpoint of the segmented buffer specified by segbuf. Before this
 * function, the endpoint must be disconnected.
 *
 * @param segbuf   Handle of the segmented buffer.
 * @retval 0  Success
 * @retval 1  Fail
 */
extern int acp_free_segbuf(volatile acp_segbuf_t segbuf);

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
