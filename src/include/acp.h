/*
 * Advanced Communication Primitives Library Header
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

/**
 * @file acp.h
 * @brief A header file for ACP.
 *         
 *  This is the ACP header file.
 *
 */

#ifndef __ACP_H__
#define __ACP_H__

/*****************************************************************************/
/***** Basic Layer                                                       *****/
/*****************************************************************************/
/**
 * @defgroup acpbl ACP Basic Layer
 *
 * ACP basic layer is a thin abstraction of underlying communication devices.
 *
 * The basic layer of ACP provides global address space shared among all of 
 * the processes. The global address of this space is represented by 64bit 
 * unsigned integer, so that it can be directly handled by the atomic operations 
 * of CPUs and devices. Any local memory space of any process can be mapped to 
 * this space via the registration function.
 */

/* Infrastructure */

/**
 * @defgroup infrastructure ACP Basic Layer Infrastructure
 * @ingroup acpbl
 *
 * These functions perform the infrastructure of ACP Basic Layer.
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 *
 * @brief ACPを初期化する関数。
 *
 * 他のACP関数を実行する前に呼び出す。
 * argcおよびargvにはmain関数の引数をポインタでそのまま渡す。
 * acp_init関数は全プロセスが初期化を完了すると戻る。
 * acp_init関数は内部で各MLモジュールの初期化関数を呼び出す。
 * @param argc 引数の数へのポインタ
 * @param argv 引数の値の配列へのポインタ
 * @retval 0 成功
 * @retval -1 失敗
 *
 * @EN
 * @brief ACP initialization
 *
 * Initializes the ACP library. Must be invoked before other 
 * functions of ACP. argc and argv are the pointers for the 
 * arguments of the main function. It returns after all of 
 * the processes complete initialization. In this function, 
 * the initialization functions of the modules of the 
 * middle layer are invoked.
 * @param argc A pointer for the number of arguments of the main function.
 * @param argv A pointer for the array of arguments of the main function.
 * @retval 0 Success
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_init(int *argc, char ***argv);

/**
 * @JP
 * @brief ACPの終了処理を行う関数。
 *
 * acp_finalize関数を呼び出す前に確保されていた資源は全て解放される。
 * acp_finalize関数は全プロセスが終了処理を完了すると戻る。
 * acp_finalize関数は内部で各MLモジュールの終了処理関数を呼び出す。
 *
 * @retval 0 成功
 * @retval -1 失敗
 *
 * @EN
 * @brief ACP finalization
 *
 * Finalizes the ACP library. All of the resources allocated 
 * in the library before this function are freed. It returns 
 * after all of the processes complete finalization. 
 * In this function, the finalization functions of the modules 
 * of the middle layer are invoked.
 *
 * @retval 0 Success
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_finalize(void);

/**
 * @JP
 * @brief ACPを再初期化する関数。
 *
 * rankには再初期化後のランク番号を指定する。
 * acp_reset関数を呼び出す前に確保されていた資源は全て解放され、
 * 各プロセスのランク番号もrankで指定した値に変更される。
 * スターターメモリはゼロクリアされる。
 * acp_reset関数は全プロセスが再初期化を完了すると戻る。
 * acp_reset関数は内部で各MLモジュールの終了処理関数と初期化関数を呼び出す。
 * @param rank 引数の数へのポインタ
 * @retval 0 成功
 * @retval -1 失敗
 *
 * @EN
 * @brief ACP Re-initialization
 *
 * Re-initializes the ACP library. As rank, the new rank number 
 * of this process after this function is specified. All of the 
 * resources allocated in the library before this function are 
 * freed. The starter memory of each process is cleared to be zero. 
 * This function returns after all of the processes complete 
 * re-initialization. In this function, the functions for the 
 * initialization and the finalization of the modules of the 
 * middle layer are invoked.
 *
 * @param rank New rank number of this process after re-initialization.
 * @retval 0 Success
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_reset(int rank);

/**
 * @JP
 * @brief ACPを強制終了する関数。
 *
 * 指定したエラーメッセージと組み込みエラーメッセージを出力する。
 * 組み込みエラーメッセージはACP_ERRNO変数の値によって異なる。
 * ACP_ERRNO変数はACPBL関数の失敗要因を表す値を保持している。
 *
 * @param str 追加エラーメッセージ
 *
 * @EN
 * @brief ACP abort
 *
 * Aborts the ACP library. It prints out the error message 
 * specified as the argument and the system message according 
 * to the error number ACP_ERRNO. ACP_ERRNO holds a number 
 * that shows the reason of the fail of the functions of ACP basic layer.
 *
 * @param str Additional error message.
 * @ENDL
 */
extern void acp_abort(const char* str);

/**
 * @JP
 * @brief 全プロセスを同期する関数。
 *
 * 全プロセスでacp_sync関数が呼び出されると戻る。
 *
 * @retval 0 成功
 * @retval -1 失敗
 *
 * @EN
 * @brief ACP Syncronization
 *
 * Synchronizes among all of the processes. 
 * Returns after all of the processes call this function.
 *
 * @retval 0 Success
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_sync(void);

/**
 * @JP
 * @brief プロセスランク取得関数
 *
 * 呼び出したプロセスのランク番号を取得する関数。
 *
 * @retval >0 ランク番号
 * @retval -1 失敗
 *
 * @EN
 * @brief Query for the process rank
 *
 * Returns the rank number of the process that called this function.
 *
 * @retval >=0 Rank number of the process.
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_rank(void);

/**
 * @JP
 * @brief 総プロセス数を取得する関数。
 *
 * 総プロセス数を取得する関数。
 * 
 * @retval >1 総プロセス数
 * @retval -1 失敗
 *
 * @EN
 * @brief Query for the number of processes
 *
 * Returns the number of the processes.
 * 
 * @retval >=1 Number of processes.
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_procs(void);
 
#ifdef __cplusplus
}
#endif
/* @} */ /* Infrastructure */

/* Global memory management */
/**
 * @defgroup GMM ACP Basic Layer Global Memory Management
 * @ingroup acpbl
 *
 * Functions for global memory management.
 *
 * @{
 */

/** Represents that no address translation key is available. */
#define ACP_ATKEY_NULL  0LLU
/** Null address of the global memory. */
#define ACP_GA_NULL     0LLU

/** Address translation key. An attribute to translate between a 
 * logical address and a global address. */
typedef uint64_t acp_atkey_t;

/** Global address. Commonly used among processes for byte-wise addressing 
 * of the global memory. */
typedef uint64_t acp_ga_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief スターターアドレス取得関数
 *
 * ランク番号を指定して、スターターメモリの先頭グローバルアドレスを
 * 取得する関数。
 *
 * @param rank ランク番号
 *
 * @retval ACP_GA_NULL以外 スターターメモリのグローバルアドレス
 * @retval ACP_GA_NULL 失敗
 *
 * @EN
 * @brief Query for the global address of the starter memory
 *
 * Returns the global address of the starter memory of the specified rank.
 *
 * @param rank Rank number
 *
 * @retval ga Global address of the starter memory
 * @retval ACP_GA_NULL Fail
 * @ENDL
 */
extern acp_ga_t acp_query_starter_ga(int rank);

/**
 * @JP
 * @brief メモリ登録関数
 *
 * メモリ領域をアドレス変換機構に登録し、アドレス変換キーを発行する関数。
 * GMAで使用されるカラー番号も同時に登録される。
 *
 * @param addr メモリ領域先頭論理アドレス
 * @param size メモリ領域サイズ
 * @param color カラー番号
 * 
 * @retval ACP_ATKEY_NULL以外 アドレス変換キー
 * @retval ACP_ATKEY_NULL 失敗
 *
 * @EN
 * @brief Memory registration
 *
 * Registers the specified memory region to global memory and 
 * returns an address translation key for it. 
 * The color that will be used for GMA with the address is 
 * also included in the key.
 *
 * @param addr Logical address of the top of the memory region to be registered.
 * @param size Size of the region to be registered.
 * @param color Color number that will be used for GMA with the global memory.
 * 
 * @retval ACP_ATKEY_NULL Fail.
 * @retval otherwise Address translation key.
 * @ENDL
 */
extern acp_atkey_t acp_register_memory(void *addr, size_t size, int color);

/**
 * @JP
 * @brief メモリ登録解除関数
 *
 * アドレス変換キーを指定して、アドレス変換機構に登録したメモリを解除する関数。
 *
 * @param atkey アドレス変換キー
 * @retval 0 成功
 * @retval -1 失敗
 *
 * @EN
 * @brief Memory unregistration 
 *
 * Unregister the memory region with the specified address translation key.
 *
 * @param atkey Address translation key.
 * @retval 0 Success
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_unregister_memory(acp_atkey_t atkey);

/**
 * @JP
 * @brief グローバルアドレス取得関数
 *
 * 変換キーと論理アドレスを指定し、論理アドレスをグローバルアドレスに
 * 変換する関数。
 *
 * @param atkey アドレス変換キー
 * @param addr 論理アドレス
 * 
 * @retval ACP_GA_NULL以外 グローバルアドレス
 * @retval ACP_GA_NULL 失敗
 *
 * @EN
 * @brief Query for the global address
 *
 * Returns the global address of the specified logical address 
 * translated by the specified address translation key.
 *
 * @param atkey Address translation key.
 * @param addr Logical address.
 * 
 * @retval ACP_GA_NULL Fail
 * @retval otherwise Global address of starter memory.
 * @ENDL
 */
extern acp_ga_t acp_query_ga(acp_atkey_t atkey, void *addr);

/**
 * @JP
 * @brief 論理アドレス取得関数
 *
 * グローバルアドレスに対応する論理アドレスを取得する関数。
 * グローバルアドレスが呼び出しプロセスとは別のプロセスを指している場合、
 * 本関数は失敗する。本関数はスターターメモリの論理アドレスも取得できる。
 *
 * @param ga グローバルアドレス
 * @retval NULL以外 論理アドレス
 * @retval NULL 失敗
 *
 * @EN
 * @brief Query for the logical address
 *
 * Returns the logical address of the specified global address. 
 * It fails if the process that keeps the logical region of the 
 * global address is different from the caller. 
 * It can be used for retrieving logical address of the starter memory.
 *
 * @param ga Global address.
 * @retval NULL Fail
 * @retval otherwise Logical address.
 * @ENDL
 */
extern void* acp_query_address(acp_ga_t ga);

/**
 * @JP
 * @brief ランク番号取得関数
 *
 * グローバルアドレスに対応するランク番号を取得する関数。
 * 本関数はスターターメモリのランク番号も取得できる。
 * gaにACP_GA_NULLを指定すると-1を返す。
 * 
 * @param ga グローバルアドレス
 * 
 * @retval >=0 ランク番号
 * @retval -1 失敗
 *
 * @EN
 * @brief Query for the rank of the global address
 *
 * Returns the rank of the process that keeps the logical region 
 * of the specified global address. It can be used for 
 * retrieving the rank of the starter memory. 
 * It returns -1 if the ACP_GA_NULL is specified as the global address.
 * 
 * @param ga Global address.
 * 
 * @retval >=0 Rank number.
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_query_rank(acp_ga_t ga);

/**
 * @JP
 * @brief カラー番号取得関数
 *
 * グローバルアドレスに対応するカラー番号を取得する関数。
 * スターターメモリのカラー番号は0固定。
 * gaにACP_GA_NULLを指定すると-1を返す。
 *
 * @param ga グローバルアドレス
 * 
 * @retval >=0 カラー番号
 * @retval -1 失敗
 *
 * @EN
 * @brief Query for the color of the global address
 *
 * Returns the color of the specified global address. 
 * It returns -1 if the ACP_GA_NULL is specified as the global address. 
 *
 * @param ga Global address
 * 
 * @retval >=0 Color number.
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_query_color(acp_ga_t ga);

/**
 * @JP
 * @brief 最大カラー数を取得する関数。
 *
 * 最大カラー数を取得する関数。
 * 
 * @retval >=1 最大カラー数
 * @retval -1 失敗
 *
 * @EN
 * @brief Query for the maximum number of colors
 *
 * Returns the maximum number of colors on this environment.
 * 
 * @retval >=1 Maximum number of colors.
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_colors(void);

#ifdef __cplusplus
}
#endif
/* @} */ /* Global memory management */

/* Global memory access */
/** \defgroup GMA ACP Basic Layer Global Memory Access
 * @ingroup acpbl
 *
 * Functions for Global memory access
 *
 * ..........
 * @{
 * */

/** Represents all of the handles of GMAs that have been invoked so far. */
#define ACP_HANDLE_ALL  0xffffffffffffffffLLU

/** Represents the continuation of the previous GMA.(*). */
#define ACP_HANDLE_CONT 0xfffffffffffffffeLLU

/** Represents that no handle is available. */
#define ACP_HANDLE_NULL 0x0000000000000000LLU

/** Handle of GMA.  Used as identifiers of the invoked GMAs. */
typedef int64_t acp_handle_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief 任意のプロセス間でデータをコピーする関数。
 *
 * 任意のプロセス間でデータをコピーする関数。
 * コピー先およびコピー元のグローバルアドレスとコピーする
 * データのサイズを指定する。
 *
 * @param dst コピー先先頭グローバルアドレス
 * @param src コピー元先頭グローバルアドレス
 * @param size サイズ
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief Copy
 *
 * Copies data of the specified size between the specified global 
 * addresses of the global memory. Ranks of both of dst and src 
 * can be different from the rank of the caller process. 
 *
 * @param dst Global address of the head of the destination region of the copy.
 * @param src Global address of the head of the source region of the copy.
 * @param size Size of the data to be copied.
 * @param order The handle to be used as a condition for starting this GMA. 
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, 
			     size_t size, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の比較交換操作を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 比較交換する値は4バイトで、4バイト境界に整列されている必要がある。
 * 
 * @param dst 結果格納アドレス
 * @param src 比較交換アドレス
 * @param oldval 比較値
 * @param newval 交換値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 4byte Compare and Swap
 *
 * Performs an atomic compare-and-swap operation on the global address 
 * specified as src. The result of the operation is stored in the 
 * global address specified as dst. The rank of the dst must be 
 * the rank of the caller process. The values to be compared and 
 * swapped is 4byte. Global addresses must be 4byte aligned. 
 * 
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param oldval Old value to be compared.
 * @param newval New value to be swapped.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_cas4(acp_ga_t dst, acp_ga_t src, uint32_t oldval,
			     uint32_t newval, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の比較交換操作を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 比較交換する値は8バイトで、8バイト境界に整列されている必要がある。
 * 
 * @param dst 結果格納アドレス
 * @param src 比較交換アドレス
 * @param oldval 比較値
 * @param newval 交換値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 8byte Compare and Swap
 *
 * Performs an atomic compare-and-swap operation on the global address 
 * specified as src. The result of the operation is stored in the 
 * global address specified as dst. The rank of the dst must be the 
 * rank of the caller process. The values to be compared and swapped 
 * is 8byte. Global addresses must be 8byte aligned. 
 * 
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param oldval Old value to be compared.
 * @param newval New value to be swapped.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_cas8(acp_ga_t dst, acp_ga_t src, uint64_t oldval,
			     uint64_t newval, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の交換操作を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 比較交換する値は4バイトで、4バイト境界に整列されている必要がある。
 * 
 * @param dst 結果格納アドレス
 * @param src 交換アドレス
 * @param value 比較値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 4byte Swap
 *
 * Performs an atomic swap operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be swapped is 4byte. Global addresses must be 4byte aligned. 
 * 
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be swapped.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_swap4(acp_ga_t dst, acp_ga_t src, 
			      uint32_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の交換操作を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 比較交換する値は8バイトで、8バイト境界に整列されている必要がある。
 * 
 * @param dst 結果格納アドレス
 * @param src 交換アドレス
 * @param value 比較値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 8byte Swap
 *
 * Performs an atomic swap operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be swapped is 8byte. Global addresses must be 8byte aligned. 
 * 
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be swapped.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_swap8(acp_ga_t dst, acp_ga_t src, 
			      uint64_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の読出加算を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 読出加算する値は4バイトで、4バイト境界に整列されている必要がある。
 *
 * @param dst 結果格納アドレス
 * @param src 加算アドレス
 * @param value 加算値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 * 
 * @EN
 * @brief 4byte Add
 *
 * Performs an atomic add operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be added is 4byte. Global addresses must be 4byte aligned. 
 *
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be added.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_add4(acp_ga_t dst, acp_ga_t src, 
			     uint32_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の読出加算を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 読出加算する値は8バイトで、8バイト境界に整列されている必要がある。
 *
 * @param dst 結果格納アドレス
 * @param src 加算アドレス
 * @param value 加算値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 8byte Add
 *
 * Performs an atomic add operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be added is 8byte. Global addresses must be 8byte aligned. 
 *
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be added.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_add8(acp_ga_t dst, acp_ga_t src, 
			     uint64_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の読出排他的論理和演算を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 読出排他的論理和演算の値は4バイトで、4バイト境界に整列されている必要がある。
 *
 * @param dst 結果格納アドレス
 * @param src 排他的論理和演算アドレス
 * @param value 排他的論理和演算値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 * 
 * @EN
 * @brief 4byte Exclusive OR
 *
 * Performs an atomic XOR operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be applied is 4byte. Global addresses must be 4byte aligned. 
 *
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be applied the XOR operation.
 * @param order The handle to be used as a condition for starting this GMA
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_xor4(acp_ga_t dst, acp_ga_t src, 
			     uint32_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の読出排他的論理和演算を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 読出排他的論理和演算の値は8バイトで、8バイト境界に整列されている必要がある。
 *
 * @param dst 結果格納アドレス
 * @param src 排他的論理和演算アドレス
 * @param value 排他的論理和演算値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * Performs an atomic XOR operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be applied is 8byte. Global addresses must be 8byte aligned. 
 *
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be applied the XOR operation.
 * @param order The handle to be used as a condition for starting this GMA
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_xor8(acp_ga_t dst, acp_ga_t src, 
			     uint64_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の読出論理和演算を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 読出論理和演算の値は4バイトで、4バイト境界に整列されている必要がある。
 *
 * @param dst 結果格納アドレス
 * @param src 論理和演算アドレス
 * @param value 論理和演算値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 4byte OR
 *
 * Performs an atomic OR operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be applied is 4byte. Global addresses must be 4byte aligned. 
 *
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be applied the OR operation.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_or4(acp_ga_t dst, acp_ga_t src, 
			    uint32_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の読出論理和演算を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 読出論理和演算の値は8バイトで、8バイト境界に整列されている必要がある。
 *
 * @param dst 結果格納アドレス
 * @param src 論理和演算アドレス
 * @param value 論理和演算値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 8byte OR
 *
 * Performs an atomic OR operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be applied is 8byte. Global addresses must be 8byte aligned. 
 *
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be applied the OR operation.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_or8(acp_ga_t dst, acp_ga_t src, 
			    uint64_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の読出論理積演算を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 読出論理積演算の値は4バイトで、4バイト境界に整列されている必要がある。
 *
 * @param dst 結果格納アドレス
 * @param src 論理積演算アドレス
 * @param value 論理積演算値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 4byte AND
 *
 *Performs an atomic AND operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be applied is 4byte. Global addresses must be 4byte aligned. 
 *
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be applied the AND operation.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_and4(acp_ga_t dst, acp_ga_t src, 
			     uint32_t value, acp_handle_t order);

/**
 * @JP
 * @brief 任意のグローバルアドレスに対して不可分の読出論理積演算を行う関数。
 *
 * 結果格納アドレスは呼び出しプロセスのメモリである必要がある。
 * 読出論理積演算の値は8バイトで、8バイト境界に整列されている必要がある。
 *
 * @param dst 結果格納アドレス
 * @param src 論理積演算アドレス
 * @param value 論理積演算値
 * @param order 指定ハンドルおよびそれ以前のGMAが全て正常終了後に実行開始
 * @retval ACP_HANDLE_NULL以外 GMA ハンドル
 * @retval ACP_HANDLE_NULL 失敗
 *
 * @EN
 * @brief 8byte AND
 *
 *Performs an atomic AND operation on the global address specified as src. 
 * The result of the operation is stored in the global address specified 
 * as dst. The rank of the dst must be the rank of the caller process. 
 * The values to be applied is 8byte. Global addresses must be 8byte aligned. 
 *
 * @param dst Global address to store the result.
 * @param src Global address to apply the operation.
 * @param value Value to be applied the AND operation.
 * @param order The handle to be used as a condition for starting this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @retval otherwise A handle for this GMA.
 * @ENDL
 */
extern acp_handle_t acp_and8(acp_ga_t dst, acp_ga_t src, 
			     uint64_t value, acp_handle_t order);

/**
 * @JP
 * @brief 未完了GMAを発行順に完了する関数。
 *
 * 未完了GMAを発行順に完了する関数。
 * 実行中のGMAは終了するまで待機して完了する。
 * handleで指定したGMAまで完了する。
 * handleにACP_HANDLE_ALLを指定すると全未完了GMAを完了する。
 * handleにACP_HANDLE_NULL、完了済みGMAのGMAハンドル、
 * もしくは未発行のGMAハンドルを指定した場合、acp_complete関数は即座に戻る。
 *
 * @param handle 完了するGMAのGMAハンドル指定
 *
 * @EN
 * @brief Completion of GMA
 *
 * Complete GMAs in order. It waits until the GMA of the specified handle 
 * completes. This means all the GMAs invoked before that one are also 
 * completed. If ACP_HANDLE_ALL is specified, it completes all of the 
 * out-standing GMAs. If the specified handle is ACP_HANDLE_NULL, 
 * the handle of the GMA that has already been completed, 
 * or the handle of the GMA that has not been invoked, 
 * this function returns immediately.
 *
 * @param handle Handle of a GMA to be waited for the completion.
 * @ENDL
 */
extern void acp_complete(acp_handle_t handle);

/**
 * @JP
 * @brief 未完了GMAを発行順に、実行中GMAがあるか照会する関数。
 *
 * handleで指定したGMAまで照会して実行中GMAがなければ0を返し、あれば1を返す。
 * handleにACP_HANDLE_ALLを指定すると全未完了GMAを照会する。
 * handleにACP_HANDLE_NULL、完了済みGMAのGMAハンドル、
 * もしくは未発行のGMAハンドルを指定した場合、acp_inquire関数は0を返す。
 *
 * @param handle 状態を調べる未完了GMAのGMAハンドル
 * @retval 0 実行中の GMA なし
 * @retval 1 実行中の GMA あり
 *
 * @EN
 * @brief Query for the completion of GMA
 *
 * Queries if any of the GMAs that are invoked earlier than the GMA of 
 * the specified handle, including that GMA, are incomplete. It returns 
 * zero if all of those GMAs have been completed. Otherwise, it returns one. 
 * If ACP_HANDLE_ALL is specified, it checks of of the out-standing GMAs. 
 * If the specified handle is ACP_HANDLE_NULL, the handle of the GMA 
 * that has already been completed, or the handle of the GMA that has 
 * not been invoked, it returns zero.
 *
 * @param handle Handle of the GMA to be checked for the completion.
 * @retval 0 No incomlete GMAs.
 * @retval 1 There is at least one incomplete GMA.
 * @ENDL
 */
extern int acp_inquire(acp_handle_t handle);

#ifdef __cplusplus
}
#endif

/* @} */ /* Global memory access */

/** \defgroup acpml ACP Middle Layer
 *
 * ACP middle layer is a set of operations for primitive communication 
 * patterns used by applications.
 *
 * .........
 */

/*****************************************************************************/
/***** Communication Library                                             *****/
/*****************************************************************************/
/** \defgroup acpcl ACP Middle Layer Communication Library
 * @ingroup acpml
 *
 * This library provides \a Channel interface and \a collective communication
 * interface.
 *
 * .........
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct chreqitem *acp_request_t;

typedef struct chitem *acp_ch_t;

/**
 * @brief Creates an endpoint of a channel to transfer messages from sender to receiver.
 *
 * Creates an endpoint of a channel to transfer messages from sender to receiver, 
 * and returns a handle of it. It returns error if sender and receiver is same, 
 * or the caller process is neither the sender nor the receiver.
 * This function does not wait for the completion of the connection 
 * between sender and receiver. The connection will be completed by the completion 
 * of the communication through this channel. There can be more than one channels 
 * for the same sender-receiver pair.
 *
 * @param sender   Rank of the sender process of the channel.
 * @param receiver  Rank of the receiver process of the channel.
 * @retval ACP_CH_NULL fail
 * @retval otherwise A handle of the endpoint of the channel.
 */
//extern acp_ch_t acp_create_ch(int src, int dest); [ace-yt3 51]
extern acp_ch_t acp_create_ch(int sender, int receiver);

/**
 * @brief Frees the endpoint of the channel specified by the handle.
 *
 * Frees the endpoint of the channel specified by the handle. 
 * It waits for the completion of negotiation with the counter peer 
 * of the channel for disconnection. It returns error if the caller 
 * process is neither the sender nor the receiver. 
 * Behavior of the communication with the handle of the channel endpoint 
 * that has already been freed is undefined.
 *
 * @param ch Handle of the channel endpoint to be freed.
 * @retval 0 Success
 * @retval -1 Fail
 */
extern int acp_free_ch(acp_ch_t ch);

/**
 * @brief Starts a nonblocking free of the endpoint of the channel specified by t
he handle.
 *
 * It returns error if the caller process is neither the sender nor the receiver. 
 * Otherwise, it returns a handle of the request for waiting the completion of 
 * the free operation. Communication with the handle of the channel endpoint 
 * that has been started to be freed causes an error.
 *
 * @param ch Handle of the channel endpoint to be freed.
 * @retval ACP_REQUEST_NULL Fail
 * @retval otherwise A handle of the request for waiting the 
 * completion of this operation.
 */
extern acp_request_t acp_nbfree_ch(acp_ch_t ch);

/**
 * @brief Non-Blocking send via channels
 *
 * Starts a nonblocking send of a message through the channel specified by the handle. 
 * It returns error if the sender of the channel endpoint specified by the handle is 
 * not the caller process. Otherwise, it returns a handle of the request for waiting 
 * the completion of the nonblocking send. 
 *
 * @param ch Handle of the channel endpoint to send a message.
 * @param buf Initial address of the send buffer.
 * @param size Size in byte of the message.
 * @retval ACP_REQUEST_NULL Fail
 * @retval otherwise A handle of the request for waiting the completion 
 * of this operation.
 */
extern acp_request_t acp_nbsend_ch(acp_ch_t ch, void * buf, size_t size);

/**
 * @brief Non-Blocking receive via channels
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
 * @param ch Handle of the channel endpoint to receive a message.
 * @param buf Initial address of the receive buffer.
 * @param size Size in byte of the receive buffer.
 * @retval ACP_REQUEST_NULL Fail
 * @retval otherwise a handle of the request for waiting the completion 
 * of this operation.
 */
extern acp_request_t acp_nbrecv_ch(acp_ch_t ch, void * buf, size_t size);

/**
 * @brief Waits for the completion of the nonblocking operation 
 *
 * Waits for the completion of the nonblocking operation specified by the request handle. 
 * If the operation is a nonblocking receive, it retruns the size of the received data.
 *
 * @param request Handle of the request of a nonblocking operation.
 * @retval >=0 Success. if the operation is a nonblocking receive, the size of the received data.
 * @retval -1 Fail
 */
extern size_t acp_wait_ch(acp_request_t request);

extern int acp_waitall_ch(acp_request_t *, int, size_t *);

#ifdef __cplusplus
}
#endif

/* @} */ /* Communication Library */

/*****************************************************************************/
/***** Data Library                                                      *****/
/*****************************************************************************/
/** \defgroup acpdl ACP Middle Layer Data Library
 * @ingroup acpml
 *
 * This library provides various data handling functions:
 * Vector, List, Deque, Set, Map
 *
 * ...........
 * @{
 */

/* Global memory allocator */

acp_ga_t acp_malloc(size_t, int);
void acp_free(acp_ga_t);

/* Function name concatenation macros */

#define acp_create(type, ...)           acp_create_##type(__VA_ARGS__)
#define acp_destroy(type, ...)          acp_destroy_##type(__VA_ARGS__)
#define acp_duplicate(type, ...)        acp_duplicate_##type(__VA_ARGS__)
#define acp_swap(type, ...)             acp_swap_##type(__VA_ARGS__)
#define acp_clear(type, ...)            acp_clear_##type(__VA_ARGS__)
#define acp_insert(type, ...)           acp_insert_##type(__VA_ARGS__)
#define acp_erase(type, ...)            acp_erase_##type(__VA_ARGS__)
#define acp_push_back(type, ...)        acp_push_back_##type(__VA_ARGS__)
#define acp_pop_back(type, ...)         acp_pop_back_##type(__VA_ARGS__)
#define acp_element(type, ...)          acp_element_##type(__VA_ARGS__)
#define acp_front(type, ...)            acp_front_##type(__VA_ARGS__)
#define acp_back(type, ...)             acp_back_##type(__VA_ARGS__)
#define acp_begin(type, ...)            acp_begin_##type(__VA_ARGS__)
#define acp_end(type, ...)              acp_end_##type(__VA_ARGS__)
#define acp_rbegin(type, ...)           acp_rbegin_##type(__VA_ARGS__)
#define acp_rend(type, ...)             acp_rend_##type(__VA_ARGS__)
#define acp_increment(type, ...)        acp_increment_##type(__VA_ARGS__)
#define acp_decrement(type, ...)        acp_decrement_##type(__VA_ARGS__)
#define acp_max_size(type, ...)         acp_max_size_##type(__VA_ARGS__)
#define acp_empty(type, ...)            acp_empty_##type(__VA_ARGS__)
#define acp_equal(type, ...)            acp_equal_##type(__VA_ARGS__)
#define acp_not_equal(type, ...)        acp_not_equal_##type(__VA_ARGS__)
#define acp_less(type, ...)             acp_less_##type(__VA_ARGS__)
#define acp_greater(type, ...)          acp_greater_##type(__VA_ARGS__)
#define acp_less_or_equal(type, ...)    acp_less_or_equal_##type(__VA_ARGS__)
#define acp_greater_or_equal(type, ...) acp_greater_or_equal_##type(__VA_ARGS__)

/* Vector */
/** \defgroup vector ACP Middle Layer Dara Library Vector
 * @ingroup acpdl
 * 
 * ACP Middle Layer Dara Library Vector
 *
 * @{
 */

typedef struct {
    acp_ga_t ga;
} acp_vector_t;  /*!< Vector type. */

typedef struct {
    acp_vector_t vector;
    int index;
} acp_vector_it_t;     /*!< Iterater of Vector type. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief ベクタ生成
 *
 * 任意のプロセスでベクトル型データを生成する。
 *
 * @param nelem 要素数
 * @param size 要素サイズ
 * @param rank ランク番号
 * @retval "member ga == ACP_GA_NULL" 失敗
 * @retval 以外 生成したベクタ型データの参照
 *
 * @EN
 * @brief Vector creation
 *
 * Creates a vector type data on any process.
 *
 * @param nelem Number of elements.
 * @param size Size of element.
 * @param rank Rank number.
 * @retval "member ga == ACP_GA_NULL" Fail
 * @retval otherwise A reference of created vector data.
 * @ENDL
 */
extern acp_vector_t acp_create_vector(size_t nelem, size_t size, int rank);

/**
 * @JP
 * @brief ベクタ破棄
 *
 * ベクトル型データを破棄する。
 *
 * @param vector ベクトル型データの参照
 *
 * @EN
 * @brief Vector destruction
 *
 * Destroies a vector type data.
 *
 * @param vector A reference of vector data.
 * @ENDL
 */
extern void acp_destroy_vector(acp_vector_t vector);

/**
 * @JP
 * @brief ベクタ複製
 *
 * 指定したベクトル型データの複製を、任意のプロセスに生成する。
 *
 * @param vector 複製するベクトル型データの参照
 * @param rank 複製先ランク番号
 * @retval "member ga == ACP_GA_NULL" 失敗
 * @retval 以外 生成したベクタ型データの参照
 * 
 * 
 * @EN
 * @brief Vector duplicate
 *
 * Duplicate a specified vector type data on any processes.
 *
 * @param vector A reference of vector data to duplicate.
 * @param rank A rank number of the process on which a vector type data
 *        is duplicated.
 * @retval "member ga == ACP_GA_NULL" Fail
 * @retval otherwise A reference of created vector data.
 *
 * @ENDL
 */
extern acp_vector_t acp_duplicate_vector(acp_vector_t vector, int rank);

/**
 * @JP
 * @brief ベクタ交換
 *
 * ２つのベクトル型データの内容を交換する。
 *
 * @param v1 交換するベクトル型データの一方の参照
 * @param v2 交換するベクトル型データのもう一方の参照
 * 
 * @EN
 * @brief Vector swap
 *
 * 
 *
 * @param v1 A reference of vector data to be swapped.
 * @param v2 Another reference of vector data to be swapped.
 *
 * @ENDL
 */
extern void acp_swap_vector(acp_vector_t v1, acp_vector_t v2);

void acp_clear_vector(acp_vector_t);
void acp_insert_vector(acp_vector_t, acp_vector_it_t);
acp_vector_it_t acp_erase_vector(acp_vector_t, acp_vector_it_t);
void acp_push_back_vector(acp_vector_t, void*);
void acp_pop_back_vector(acp_vector_t);
acp_ga_t acp_element_vector(acp_vector_t, acp_vector_it_t);
acp_ga_t acp_front_vector(acp_vector_t);
acp_ga_t acp_back_vector(acp_vector_t);
acp_vector_it_t acp_begin_vector(acp_vector_t);
acp_vector_it_t acp_end_vector(acp_vector_t);
acp_vector_it_t acp_rbegin_vector(acp_vector_t);
acp_vector_it_t acp_rend_vector(acp_vector_t);
acp_vector_it_t acp_increment_vector(acp_vector_it_t);
acp_vector_it_t acp_decrement_vector(acp_vector_it_t);
int acp_max_size_vector(acp_vector_t);
int acp_empty_vector(acp_vector_t);
int acp_equal_vector(acp_vector_t, acp_vector_t);
int acp_not_equal_vector(acp_vector_t, acp_vector_t);
int acp_less_vector(acp_vector_t, acp_vector_t);
int acp_greater_vector(acp_vector_t, acp_vector_t);
int acp_less_or_equal_vector(acp_vector_t, acp_vector_t);
int acp_greater_or_equal_vector(acp_vector_t, acp_vector_t);

#ifdef __cplusplus
}
#endif
/*@}*/ /* Vector */

/* List */
/** \defgroup list ACP Middle Layer Dara Library List
 * @ingroup acpdl
 * 
 * ACP Middle Layer Dara Library List
 *
 * @{
 */

typedef struct {
    acp_ga_t ga;
} acp_list_t;     /*!< List data type. */

typedef struct {
    acp_list_t list;
    acp_ga_t elem;
} acp_list_it_t;  /*!< Iterater of list data type. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief リスト生成
 *
 * 任意のプロセスでリスト型データを生成する。
 *
 * @param rank ランク番号
 * @retval "member ga == ACP_GA_NULL" 失敗
 * @retval 以外 生成したリスト型データの参照
 *
 * @EN
 * @brief List creation
 *
 * Creates a list type data on any process.
 *
 * @param rank Rank number.
 * @retval "member ga == ACP_GA_NULL" Fail
 * @retval otherwise A reference of created list data.
 * @ENDL
 */
extern acp_list_t acp_create_list(int rank);

/**
 * @JP
 * @brief リスト破棄
 *
 * リスト型データを破棄する。
 *
 * @param list リスト型データの参照
 *
 * @EN
 * @brief List destruction
 *
 * Destroies a list type data.
 *
 * @param list A reference of list data.
 * @ENDL
 */
extern void acp_destroy_list(acp_list_t list);

/**
 * @JP
 * @brief リスト要素挿入
 *
 * 指定したプロセスに要素をコピーし、リスト型データの指定位置に挿入する。
 *
 * @param it リスト型のイテレータ
 * @param ptr 挿入する要素の先頭アドレス
 * @param size 挿入する要素のサイズ
 * @param rank 要素を複製するプロセス
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 挿入された要素を指すリスト型イテレータ
 *
 * @EN
 * @brief Insert a list element
 *
 * 
 *
 * @param it An iterater of list type data.
 * @param ptr The pointer of list element.
 * @param size Size of list element.
 * @param rank Rank of the process in which the element is copied.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The iterator that points to the inserted element.
 * @ENDL
 */
extern acp_list_it_t acp_insert_list(acp_list_it_t it, const void* ptr, size_t size, int rank);

/**
 * @JP
 * @brief リスト要素削除
 *
 * 指定した位置の要素をリスト型データから削除する。
 *
 * @param it 削除する要素を指すリスト型イテレータ
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 削除した要素の直後の要素を指すリスト型イテレータ
 *
 * @EN
 * @brief Erase a list element
 *
 * 
 *
 * @param it An iterator of list type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval oterhwise The iterator that points to the element which is immediately after the erased one.
 * @ENDL
 */
extern acp_list_it_t acp_erase_list(acp_list_it_t it);

/**
 * @JP
 * @brief リスト末尾要素追加
 *
 * 指定したプロセスに要素をコピーし、リスト型データの末尾に挿入する。
 *
 * @param list リスト型データの参照
 * @param ptr 挿入する要素の先頭アドレス
 * @param size 挿入する要素のサイズ
 * @param rank 要素を複製するプロセス
 *
 * @EN
 * @brief Erase a list element
 *
 * 
 *
 * @param list A reference of list type data.
 * @param ptr A pointer of list type data.
 * @param size Size of list type data.
 * @param rank Rank of the process in which the element is copied.
 * @ENDL
 */
extern acp_list_it_t acp_push_back_list(acp_list_t list, const void* ptr, size_t size, int rank);

/**
 * @JP
 * @brief リスト先頭イテレータ取得
 *
 * リスト型データの先頭要素を指すイテレータを取得する。
 *
 * @param list リスト型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 先頭イテレータ
 *
 * @EN
 * @brief Query for the head iterator of a list
 *
 * 
 *
 * @param list A reference of list type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The head iterator of the list.
 * @ENDL
 */
extern acp_list_it_t acp_begin_list(acp_list_t list);

/**
 * @JP
 * @brief リスト後端イテレータ取得
 *
 * リスト型データの後端要素を指すイテレータを取得する。
 *
 * @param list リスト型データの参照
 *
 * @EN
 * @brief Query for the tail iterator of a list
 *
 * 
 *
 * @param list A reference of list type data.
 * @retval it The tail iterator of the list.
 * @ENDL
 */
extern acp_list_it_t acp_end_list(acp_list_t list);

/**
 * @JP
 * @brief リスト先頭イテレータ加算
 *
 * リスト型イテレータを一つ増加させる。
 *
 * @param list リスト型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 インクリメントしたイテレータ
 *
 * @EN
 * @brief Increment an iterater of a list data
 *
 *
 * @param list A reference of list type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The next iterator of the specified one.
 * @ENDL
 */
extern acp_list_it_t  acp_increment_list(acp_list_it_t it);

/**
 * @JP
 * @brief リスト先頭イテレータ減算
 *
 * リスト型イテレータを一つ減少させる。
 *
 * @param list リスト型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 デクリメントしたイテレータ
 *
 * @EN
 * @brief Decrement an iterater of a list data
 *
 * Decrements an iterater of a list data.
 *
 * @param list A reference of list type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The previous iterator of the specified one.
 * @ENDL
 */
extern acp_list_it_t  acp_decrement_list(acp_list_it_t it);

#ifdef __cplusplus
}
#endif
/*@}*/ /* List */

/* Deque */
/** \defgroup deque ACP Middle Layer Dara Library Deque
 * @ingroup acpdl
 * 
 * ACP Middle Layer Data Library Deque
 *
 * @{
 */

typedef struct {
    acp_ga_t ga;
} acp_deque_t;	/*!< Deque data type. */

typedef struct {
    acp_deque_t deque;
    int index;
} acp_deque_it_t;	/*!< Iterater of deque data type. */

/*@}*/ /* Deque */

/* Set */
/** \defgroup set ACP Middle Layer Dara Library Set
 * @ingroup acpdl
 * 
 * ACP Middle Layer Data Library Set
 *
 * @{
 */

typedef struct {
    acp_ga_t ga;
    int num_ranks;
    int num_slots;
} acp_set_t;	/*!< Set data type. */

typedef struct {
    acp_set_t set;
    int rank;
    int slot;
    acp_ga_t elem;
} acp_set_it_t;	/*!< Iterater of set data type. */

typedef struct {
    acp_set_it_t it;
    int success;
} acp_set_ib_t;

/*@}*/ /* Set */

/* Map */
/** \defgroup map ACP Middle Layer Dara Library Map
 * @ingroup acpdl
 * 
 * ACP Middle Layer Data Library Map
 *
 * @{
 */

/** Represents that no map type data does NOT exist. */
/* #define ACP_MAP_NULL  0LLU */

typedef struct {
    acp_ga_t ga;
    uint64_t num_ranks;
    uint64_t num_slots;
} acp_map_t;	/*!< Map data type. */

typedef struct {
    acp_map_t map;
    int rank;
    int slot;
    acp_ga_t elem;
} acp_map_it_t;	/*!< Iterater of map data type. */

typedef struct {
    acp_map_it_t it;
    int success;
} acp_map_ib_t;

#ifdef __cplusplus
{
#endif

/**
 * @JP
 * @brief マップ生成
 *
 * 空のマップ型データを生成する。
 *
 * @param num_ranks マップを分散配置するランク数、0の場合は全ランク
 * @param ranks マップを分散配置するランクを列挙する配列へのポインタ
 * @param num_slots スロット数
 * @param rank マップ生成先ランク番号
 * @retval "member ga == ACP_GA_NULL" 失敗
 * @retval 以外 生成したマップ型データの参照
 *
 * @EN
 * @brief Map creation
 *
 * Creates a empty map type data on any process.
 *
 * @param num_ranks Number of the rank where maps are distributed. '0' means all ranks.
 * @param ranks Pointers of an array for ranks where maps are distributed 
 * @param num_slots Number of slots
 * @param rank Rank number where a map is created
 * @param rank Rank number.
 * @retval "member ga == ACP_MAP_NULL" Fail
 * @retval otherwise A reference of created map data.
 * @ENDL
 */
extern acp_map_t acp_create_map(int num_ranks, const int* ranks, int num_slots, int rank);

/**
 * @JP
 * @brief マップ内リスト消去
 *
 * マップ内リストの要素を消去する。
 *
 * @param map マップ型データの参照
 *
 * @EN
 * @brief Delete elements of lists in a map type data.
 *
 * @param map A reference of map data.
 * @ENDL
 */
extern void acp_clear_map(acp_map_t map);

/**
 * @JP
 * @brief マップ破棄
 *
 * マップ型データを破棄する。
 *
 * @param map マップ型データの参照
 *
 * @EN
 * @brief Map destruction
 *
 * Destroys a map type data.
 *
 * @param map A reference of map data.
 * @ENDL
 */
extern void acp_destroy_map(acp_map_t map);

/**
 * @JP
 * @brief マップ挿入
 *
 * マップにキーと変数を挿入する。
 *
 * @param map マップ型データの参照
 * @param key 挿入する key-value ペアの key
 * @param size_key key のサイズ
 * @param value 挿入する key-value ペアの value
 * @param size_value value のサイズ
 * @retval "member success == 0" 失敗、または既に同じ key-value ペアがある
 * @retval 以外 挿入したイテレータの参照
 *
 * @EN
 * @brief Map creation
 *
 * Inserts a key-value pair to a map.
 *
 * @param map A reference of a map type data.
 * @param key Key of the key-value pair.
 * @param size_key Size of the key.
 * @param value Value of the key-value pair.
 * @param size_value Size of the value.
 * @retval "member success == 0" Fail or the same key-value pair is already in the map.
 * @retval otherwise The item inserted to the map.
 * @ENDL
 */
extern acp_map_ib_t acp_insert_map(acp_map_t map, const void* key, size_t size_key, const void* value, size_t size_value);

/**
 * @JP
 * @brief マップ検索
 *
 * マップにあるキーと変数をキーで検索する
 *
 * @param map マップ型データの参照
 * @param key 検索する key
 * @param size_key key のサイズ
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 検索結果のイテレータの参照
 *
 * @EN
 * @brief Map finding
 *
 * Find a key-value pair according to a key in a map.
 *
 * @param map A reference of a map type data.
 * @param key Key to search.
 * @param size_key Size of the key.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The item found in the map.
 * @ENDL
 */
extern acp_map_it_t acp_find_map(acp_map_t map, const void* key, size_t size_key);

#ifdef __cplusplus
}
#endif
/*@}*/ /* Map */

/*@}*/ /* Data Library */
#endif /* acp.h */
