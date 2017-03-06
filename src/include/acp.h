/*
 * Advanced Communication Primitives Library Header
 * 
 * Copyright (c) 2014-2016 FUJITSU LIMITED
 * Copyright (c) 2014-2016 Kyushu University
 * Copyright (c) 2014-2016 Institute of Systems, Information Technologies
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
/**
 * @defgroup GMA ACP Basic Layer Global Memory Access
 * @ingroup acpbl
 *
 * Functions for Global memory access
 *
 * ..........
 * @{
 */

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

/**
 * @defgroup acpml ACP Middle Layer
 *
 * ACP middle layer is a set of operations for primitive communication 
 * patterns used by applications.
 *
 * .........
 */

/*****************************************************************************/
/***** Communication Library                                             *****/
/*****************************************************************************/
/**
 * @defgroup acpcl ACP Middle Layer Communication Library
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
/**
 * @defgroup acpdl ACP Middle Layer Data Library
 * @ingroup acpml
 *
 * This library provides various data handling functions:
 * Vector, List, Deque, Set, Map
 *
 * ...........
 * @{
 */

/* Work space */

typedef struct {
    size_t      size_ws ;
    size_t      size_default ;
    size_t      size_remainder ;
    size_t      ngas ;
    acp_atkey_t key_data ;
    acp_ga_t    ga_data ;
    acp_atkey_t *keys ;
    acp_ga_t    *gas ;
    int         *procs ;
    int         *sizes ;
    void        *data ;
} acp_wsditem ;

typedef acp_wsditem *acp_wsd_t ;
///typedef int64_t acp_wsd_t;

#define ACP_WSD_NULL (acp_wsd_t)(-1)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief ワークスペース生成
 *
 * ワークスペースを生成する。
 * 全プロセスで集団的に呼び出す必要がある。
 *
 * @param size ワークスペースのサイズ
 * @retval ACP_WSD_NULL 失敗
 * @retval acp_wsd_t ワークスペース記述子
 *
 * @EN
 * @brief Workspace creation
 *
 * Creates a workspace with the specified size.
 *
 * @param size Size of the workspace
 * @retval ACP_WSD_NULL Fial
 * @retval acp_wsd_t Workspace descriptor
 * @ENDL
 */
extern acp_wsd_t acp_create_ws(size_t size);

/**
 * @JP
 * @brief ワークスペースリセット
 *
 * ワークスペース開始ランク番号をリセットする．
 * 全プロセスで集団的に呼び出す必要がある。
 *
 * @param proc_start ワークスペースのスタートランク番号
 * @param size_default 各プロセスでのデフォルトアロケートサイズ
 * @retval いつも 0
 *
 * @EN
 * @brief Workspace set parameters
 *
 * Set parameters for distributing workspace among processes. Every
 * process need to specify the same value.
 *
 * @param proc_start Start rank for workspace
 * @param size_default Default allocated size on each process
 * @retval always 0
 * @ENDL
 */
extern int acp_setparams_ws( size_t proc_start, size_t size_default );

/**
 * @JP
 * @brief ワークスペース破棄
 *
 * ワークスペースを破棄する。
 * 全プロセスで集団的に呼び出す必要がある。
 *
 * @param WSD ワークスペース記述子
 *
 * @EN
 * @brief Workspace destruction
 *
 * Destroies a workspace.
 *
 * @param WSD Workspace descriptor
 * @ENDL
 */
extern void acp_destroy_ws(acp_wsd_t WSD);

/**
 * @JP
 * @brief ワークスペース読み出し
 *
 * ワークスペースの指定位置からデータを読み出す。
 *
 * @param WSD ワークスペース記述子
 * @param ga 読み出しデータ格納グローバルアドレス
 * @param size 読み出しデータサイズ
 * @param offset 読み出し開始位置
 * @retval 0 Success
 * @retval 1 Fail
 *
 * @EN
 * @brief Reading workspace 
 *
 * Read data in a workspace from the specified position.
 *
 * @param WSD Workspace descriptor
 * @param ga The global address of the storage location for the data
 * @param size Size of the data to read
 * @param offset The start point of the data
 * @retval 0 Success
 * @retval 1 Fail
 * @ENDL
 */
extern int acp_read_ws(acp_wsd_t WSD, acp_ga_t ga, size_t size, size_t offset);

/**
 * @JP
 * @brief ワークスペース書き込み
 *
 * ワークスペースの指定位置にデータを書き込む。
 *
 * @param WSD ワークスペース記述子
 * @param ga 書き込みデータ格納グローバルアドレス
 * @param size 書き込みデータサイズ
 * @param offset 書き込み開始位置
 * @retval 0 Success
 * @retval 1 Fail
 *
 * @EN
 * @brief Writing workspace 
 *
 * Write data in a workspace at the specified position.
 *
 * @param WSD Workspace descriptor
 * @param ga The global address of the storage location for the data
 * @param size Size of the data to write
 * @param offset The start point of the data
 * @retval 0 Success
 * @retval 1 Fail
 * @ENDL
 */
extern int acp_write_ws(acp_wsd_t WSD, const acp_ga_t ga, size_t size, size_t offset);

#ifdef __cplusplus
}
#endif

/* Global memory allocator */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief グローバルメモリ割り当て
 *
 * 任意のプロセスからグローバルメモリを取得する。
 *
 * @param size わり当てるメモリのサイズ
 * @param rank メモリをわり当てるプロセス
 * @retval ACP_GA_NULL 失敗
 * @retval acp_ga_t あり当てられたメモリの先頭アドレス
 *
 * @EN
 * @brief Global memory allocation
 *
 * Allocate a global memory on any process.
 *
 * @param size Size of the memory to allocate
 * @param rank Rank number.
 * @retval ACP_GA_NULL Fail
 * @retval acp_ga_t Global address of the allocated memory
 * @ENDL
 */
extern acp_ga_t acp_malloc(size_t size, int rank);

/**
 * @JP
 * @brief グローバルメモリ解放
 *
 * 取得したグローバルメモリを解放する。
 *
 * @param ga 先頭グローバルアドレス
 *
 * @EN
 * @brief Deallocate the global memory
 *
 * Deallocate the global memory allocation pointed to by ga
 *
 * @param ga The address of the global memory to deallocate
 * @ENDL
 */
extern void acp_free(acp_ga_t ga);

#ifdef __cplusplus
}
#endif

/* Function name concatenation macros */

#define acp_assign(type, ...)               acp_assign_##type(__VA_ARGS__)
#define acp_assign_range(type, ...)         acp_assign_range_##type(__VA_ARGS__)
#define acp_at(type, ...)                   acp_at_##type(__VA_ARGS__)
#define acp_back(type, ...)                 acp_back_##type(__VA_ARGS__)
#define acp_begin(type, ...)                acp_begin_##type(__VA_ARGS__)
#define acp_bucket(type, ...)               acp_bucket_##type(__VA_ARGS__)
#define acp_bucket_count(type, ...)         acp_bucket_count_##type(__VA_ARGS__)
#define acp_bucket_size(type, ...)          acp_bucket_size_##type(__VA_ARGS__)
#define acp_capacity(type, ...)             acp_capacity_##type(__VA_ARGS__)
#define acp_clear(type, ...)                acp_clear_##type(__VA_ARGS__)
#define acp_count(type, ...)                acp_count_##type(__VA_ARGS__)
#define acp_create(type, ...)               acp_create_##type(__VA_ARGS__)
#define acp_destroy(type, ...)              acp_destroy_##type(__VA_ARGS__)
#define acp_empty(type, ...)                acp_empty_##type(__VA_ARGS__)
#define acp_end(type, ...)                  acp_end_##type(__VA_ARGS__)
#define acp_erase(type, ...)                acp_erase_##type(__VA_ARGS__)
#define acp_erase_range(type, ...)          acp_erase_range_##type(__VA_ARGS__)
#define acp_find(type, ...)                 acp_find_##type(__VA_ARGS__)
#define acp_front(type, ...)                acp_front_##type(__VA_ARGS__)
#define acp_insert(type, ...)               acp_insert_##type(__VA_ARGS__)
#define acp_insert_range(type, ...)         acp_insert_range_##type(__VA_ARGS__)
#define acp_merge(type, ...)                acp_merge_##type(__VA_ARGS__)
#define acp_pop_back(type, ...)             acp_pop_back_##type(__VA_ARGS__)
#define acp_pop_front(type, ...)            acp_pop_front_##type(__VA_ARGS__)
#define acp_push_back(type, ...)            acp_push_back_##type(__VA_ARGS__)
#define acp_push_front(type, ...)           acp_push_front_##type(__VA_ARGS__)
#define acp_remove(type, ...)               acp_remove_##type(__VA_ARGS__)
#define acp_reserve(type, ...)              acp_reserve_##type(__VA_ARGS__)
#define acp_reverse(type, ...)              acp_reverse_##type(__VA_ARGS__)
#define acp_size(type, ...)                 acp_size_##type(__VA_ARGS__)
#define acp_sort(type, ...)                 acp_sort_##type(__VA_ARGS__)
#define acp_splice(type, ...)               acp_splice_##type(__VA_ARGS__)
#define acp_swap(type, ...)                 acp_swap_##type(__VA_ARGS__)
#define acp_unique(type, ...)               acp_unique_##type(__VA_ARGS__)

#define acp_advance(type, ...)              acp_advance_##type(__VA_ARGS__)
#define acp_decrement(type, ...)            acp_decrement_##type(__VA_ARGS__)
#define acp_dereference(type, ...)          acp_dereference_##type(__VA_ARGS__)
#define acp_dereference_value(type, ...)    acp_dereference_value_##type(__VA_ARGS__)
#define acp_distance(type, ...)             acp_distance_##type(__VA_ARGS__)
#define acp_increment(type, ...)            acp_increment_##type(__VA_ARGS__)
#define acp_size(type, ...)                 acp_size_##type(__VA_ARGS__)
#define acp_size_value(type, ...)           acp_size_value_##type(__VA_ARGS__)

/* Element, Pair */

typedef struct {
    acp_ga_t ga;
    size_t size;
} acp_element_t;

typedef struct {
    acp_element_t first;
    acp_element_t second;
} acp_pair_t;

/* Vector */
/**
 * @defgroup vector ACP Middle Layer Dara Library Vector
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
 * @brief ベクタ代入
 *
 * ベクタ間でデータをコピーする。以前のデータは破棄される。
 *
 * @param vector1 コピー先ベクトル型データの参照
 * @param vector2 コピー元ベクトル型データの参照
 *
 * @EN
 * @brief Vector assignment
 *
 * Copy vector data between two vectors.
 *
 * @param vector1 A reference of destination vector data.
 * @param vector2 A reference of source vector data.
 * @ENDL
 */
extern void acp_assign_vector(acp_vector_t vector1, acp_vector_t vector2);

/**
 * @JP
 * @brief ベクタ範囲代入
 *
 * ベクタにstartからend直前までのデータをコピーする。
 * startとendは同じベクタを指している必要がある。
 * 以前のデータは破棄される。
 *
 * @param vector コピー先ベクトル型データの参照
 * @param start コピーするデータの先頭を指すイテレータ
 * @param end コピーするデータの末尾の直後を指すイテレータ
 *
 * @EN
 * @brief Vector assignment with range
 *
 * Copy vector data from the point of "start" to "end".
 * 
 * @param vector A reference of destination vector data.
 * @param start A vector type data iterator for pointing the starting address
 * @param end A vector type data iterator for pointing the end address
 * @ENDL
 */
extern void acp_assign_range_vector(acp_vector_t vector, acp_vector_it_t start, acp_vector_it_t end);

/**
 * @JP
 * @brief ベクタ直接参照
 *
 * ベクタデータ上の任意の位置のグローバルアドレスを返す。
 *
 * @param vector ベクトル型データの参照
 * @param index ベクトル型データ先頭からの相対位置
 *
 * @EN
 * @brief Query for a global address of any point on a vector type data
 *
 * @param vector A reference of vector data
 * @param index The relative point from the head of vector data
 * @ENDL
 */
extern acp_ga_t acp_at_vector(acp_vector_t vector, int index);

/**
 * @JP
 * @brief ベクタ先頭イテレータ
 *
 * ベクタの先頭データを指すイテレータを返す。
 *
 * @param vector ベクトル型データの参照
 * @retval acp_vector_it_t ベクタの先頭データを指すイテレータ
 *
 * @EN
 * @brief Query for the iterator of the head of vector data
 *
 * @param vector A reference of vector data.
 * @retval acp_vector_it_t An iterator of the head of vector data
 * @ENDL
 */
extern acp_vector_it_t acp_begin_vector(acp_vector_t vector);

/**
 * @JP
 * @brief ベクタ容量
 *
 * ベクタが領域拡張せずに保持できるサイズを返す。
 *
 * @param vector ベクトル型データの参照
 * @retval size_t ベクタが領域拡張せずに保持できるサイズ
 *
 * @EN
 * @brief Capacity of vector
 *
 * Query for the capacity of the vector type data
 *
 * @param vector A reference of vector type data
 * @retval size_t The size of vector type data
 * @ENDL
 */
extern size_t acp_capacity_vector(acp_vector_t vector);

/**
 * @JP
 * @brief ベクタ消去
 *
 * ベクトルのサイズを０にする。
 *
 * @param vector ベクトル型データの参照
 *
 * @EN
 * @brief Vector elimination
 *
 * Set the size of the vector to be zero.
 *
 * @param vector A reference of vector data.
 * @ENDL
 */
extern void acp_clear_vector(acp_vector_t vector);

/**
 * @JP
 * @brief ベクタ生成
 *
 * 任意のプロセスでベクトル型データを生成する。
 *
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
 * @param size Size of element.
 * @param rank Rank number.
 * @retval "member ga == ACP_GA_NULL" Fail
 * @retval otherwise A reference of created vector data.
 * @ENDL
 */
extern acp_vector_t acp_create_vector(size_t size, int rank);

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
 * @brief ベクタ空チェック
 *
 * ベクタが空かどうかを返す。
 *
 * @param vector ベクトル型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for vector empty
 *
 * @param vector A reference of vector data.
 * @retval 1 Empty
 * @retval 0 There is a vector data
 * @ENDL
 */
extern int acp_empty_vector(acp_vector_t vector);

/**
 * @JP
 * @brief ベクタ末尾イテレータ
 *
 * ベクタの最終データの直後を指すイテレータを返す。
 *
 * @param vector ベクトル型データの参照
 * @retval acp_vector_it_t ベクタの最終データの直後を指すイテレータ
 *
 * @EN
 * @brief Query for the iterator of just behind of the end of vector data
 *
 * @param vector A reference of vector data.
 * @retval acp_vector_it_t An iterator of just behind of the end of vector data
 * @ENDL
 */
extern acp_vector_it_t acp_end_vector(acp_vector_t vector);

/**
 * @JP
 * @brief ベクタ削除
 *
 * ベクタからデータを削除する。
 *
 * @param it 削除するデータの先頭を指すイテレータ
 * @param size 削除するデータのサイズ
 * @retval acp_vector_it_t 削除されたデータの直後を指すイテレータ
 *
 * @EN
 * @brief Deletion of the vector data
 *
 * @param it An iterator of vector data to erase
 * @param size The size of data to erase
 * @retval acp_vector_it_t An iterator of just behind of the deleted vector data
 * @ENDL
 */
extern acp_vector_it_t acp_erase_vector(acp_vector_it_t it, size_t size);

/**
 * @JP
 * @brief ベクタ範囲削除
 *
 * ベクタからstartからend直前までのデータを削除する。
 *
 * @param start 削除するデータの先頭を指すイテレータ
 * @param end 削除するデータの末尾の直後を指すサイズ
 * @retval acp_vector_it_t 削除されたデータの直後を指すイテレータ
 *
 * @EN
 * @brief Deletion of the vector data from "start" to "end"
 *
 * @param start The iterator of vector data to erase
 * @param end The iterator of just behind of the deleting vector data
 * @retval acp_vector_it_t The iterator of just behind of the deleted vector data
 * @ENDL
 */
extern acp_vector_it_t acp_erase_range_vector(acp_vector_it_t start, acp_vector_it_t end);

/**
 * @JP
 * @brief ベクタ挿入
 *
 * 指定した位置にデータを挿入する。
 *
 * @param it データを挿入する位置を指すイテレータ
 * @param ga 挿入するデータを格納しているグルーバルアドレス
 * @param size 挿入するデータのサイズ
 * @retval acp_vector_it_t 挿入したデータの先頭を指すイテレータ
 *
 * @EN
 * @brief Insertion of the vector data
 *
 * @param it An iterator of the point for inserting data
 * @param ga The global address of the data to insert
 * @param size The size of the data to insert
 * @retval acp_vector_it_t An iterator of head address of the inserted data
 * @ENDL
 */
extern acp_vector_it_t acp_insert_vector(acp_vector_it_t it, const acp_ga_t ga, size_t size);

/**
 * @JP
 * @brief ベクタ範囲挿入
 *
 * 他のベクタの指定範囲を、指定した位置にデータを挿入する。
 *
 * @param it データを挿入する位置を指すイテレータ
 * @param start 挿入するデータを先頭を指すイテレータ
 * @param end 挿入するデータの末尾の直後を指すイテレータ
 * @retval acp_vector_it_t 挿入したデータの先頭を指すイテレータ
 *
 * @EN
 * @brief Insertion of the vector data from "start" to "end"
 *
 * @param it An iterator of the point for inserting data
 * @param start The iterator of head address of the data to insert
 * @param end The iterator of just behind address of the data to insert
 * @retval acp_vector_it_t An iterator of head address of the inserted data
 * @ENDL
 */
extern acp_vector_it_t acp_insert_range_vector(acp_vector_it_t it, acp_vector_it_t start, acp_vector_it_t end);

/**
 * @JP
 * @brief ベクタ末尾削除
 *
 * ベクタの末尾から、指定サイズのデータを削除する。
 *
 * @param vector データを削除するベクタ
 * @param size 削除するデータのサイズ
 *
 * @EN
 * @brief Data deletion at the end of the vector data 
 *
 * @param vector The vector data to erase
 * @param size The size of data to erase
 * @ENDL
 */
extern void acp_pop_back_vector(acp_vector_t vector, size_t size);

/**
 * @JP
 * @brief ベクタ末尾追加
 *
 * ベクタの末尾に、指定サイズのデータを追加する。
 *
 * @param vector データを追加するベクタデータの査証
 * @param ga 追加するデータを格納しているグローバルアドレス
 * @param size 追加するデータのサイズ
 *
 * @EN
 * @brief Data addition at the end of the vector data 
 *
 * @param vector A reference of the vector to which data is added
 * @param ga The global address of the data to insert
 * @param size The size of the data to insert
 * @ENDL
 */
extern void acp_push_back_vector(acp_vector_t vector, const acp_ga_t ga, size_t size);

/**
 * @JP
 * @brief ベクタ領域確保
 *
 * ベクタの領域を指定した要素数確保する。
 *
 * @param vector 領域を確保するベクタデータの参照
 * @param size 要素数
 *
 * @EN
 * @brief Reservation of a region in the vector type data
 *
 * @param vector A reference of the vector to reserve a region
 * @param size The number of element
 * @ENDL
 */
extern void acp_reserve_vector(acp_vector_t vector, size_t size);

/**
 * @JP
 * @brief ベクタサイズ
 *
 * ベクタに格納しているデータのサイズを返す。
 *
 * @param vector ベクタデータの参照
 * @retval size_t ベクタに格納しているデータのサイズ
 *
 * @EN
 * @brief Query of the data size in the vector
 *
 * @param vector A reference of the vector data
 * @retval size_t The data size in the vector
 * @ENDL
 */
extern size_t acp_size_vector(acp_vector_t vector);

/**
 * @JP
 * @brief ベクタ交換
 *
 * ２つのベクトル型データの内容を交換する。
 *
 * @param vector1 交換するベクトル型データの一方の参照
 * @param vector2 交換するベクトル型データのもう一方の参照
 * 
 * @EN
 * @brief Vector swap
 *
 * 
 *
 * @param vector1 A reference of vector data to be swapped.
 * @param vector2 Another reference of vector data to be swapped.
 *
 * @ENDL
 */
extern void acp_swap_vector(acp_vector_t vector1, acp_vector_t vector2);

/**
 * @JP
 * @brief ベクタイテレータ前進
 *
 * ベクタイテレータを前進する。
 *
 * @param it ベクタデータのイテレータ
 * @param n イテレータを進める数、負の値も指定可能
 * @retval acp_vector_it_t 前進したベクタイテレータ
 *
 * @EN
 * @brief Advancement of an iterator for vector type data
 *
 * @param it The iterator of vector type data
 * @param n The number for advancing
 * @retval acp_vector_it_t The advanced iterator of vector type data 
 * @ENDL
 */
extern acp_vector_it_t acp_advance_vector_it(acp_vector_it_t it, int n);

/**
 * @JP
 * @brief ベクタイテレータ間接参照
 *
 * ベクタイテレータの参照先グローバルアドレスを返す。
 *
 * @param it ベクタデータのイテレータ
 * @retval acp_ga_t ベクタイテレータの参照先グローバルアドレス
 *
 * @EN
 * @brief Query of the global address of a reference of vector tyep iterator
 *
 * @param it The iterator of vector type data
 * @retval acp_ga_t The global address of a reference of vector type iterator 
 * @ENDL
 */
extern acp_ga_t acp_dereference_vector_it(acp_vector_it_t it);

/**
 * @JP
 * @brief ベクタイテレータ距離
 *
 * ２つのベクタイテレータ first, last 間の距離を返す。
 *
 * @param first 先頭位置のベクタイテレータ
 * @param last 最終位置のベクタイテレータ
 * @retval int ２つのベクタイテレータ first, last 間の距離
 *
 * @EN
 * @brief Query of the distance of two iterators of vector type data between "first" and "last"
 *
 * @param first The iterator for head
 * @param last The iterator for end
 * @retval int The distance between "first" and "last"
 * @ENDL
 */
extern int acp_distance_vector_it(acp_vector_it_t first, acp_vector_it_t last);

#ifdef __cplusplus
}
#endif
/*@}*/ /* Vector */

/* Deque */
/**
 * @defgroup deque ACP Middle Layer Dara Library Deque
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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief デック代入
 *
 * デック間でデータをコピーする。以前のデータは破棄される。
 *
 * @param deque1 コピー先デック型データの参照
 * @param deque2 コピー元デック型データの参照
 *
 * @EN
 * @brief Deque assignment
 *
 * Copy Deque data between two vectors.
 *
 * @param deque1 A reference of destination deque data.
 * @param deque2 A reference of source deque data.
 * @ENDL
 */
extern void acp_assign_deque(acp_deque_t deque1, acp_deque_t deque2);

/**
 * @JP
 * @brief デック範囲代入
 *
 * デックにstartからend直前までのデータをコピーする。
 * startとendは同じデックを指している必要がある。
 * 以前のデータは破棄される。
 *
 * @param deque コピー先デック型データの参照
 * @param start コピーするデータの先頭を指すイテレータ
 * @param end コピーするデータの末尾の直後を指すイテレータ
 *
 * @EN
 * @brief Deque assignment with range
 *
 * Copy a range of elements in a deque, specified by start and end, to
 * deque.  start and end must be in the same deque. Old data in deque is
 * overwritten.
 * 
 * @param deque A reference of destination deque data.
 * @param start A deque type data iterator for pointing the starting address
 * @param end A deque type data iterator for pointing the end address
 * @ENDL
 */
extern void acp_assign_range_deque(acp_deque_t deque, acp_deque_it_t start, acp_deque_it_t end);

/**
 * @JP
 * @brief デック直接参照
 *
 * デックデータ上の任意の位置のグローバルアドレスを返す。
 *
 * @param deque デック型データの参照
 * @param index デック型データ先頭からの相対位置
 * @retval ACP_GA_NULL 失敗
 * @retval ACP_GA_NULL以外 
 *
 * @EN
 * @brief Query for a global address of any point on a deque type data
 *
 * @param deque A reference of deque data
 * @param index The relative point from the head of deque data
 * @retval ACP_GA_NULL Fail
 * @retval otherwise A global address of deque data.
 * @ENDL
 */
extern acp_ga_t acp_at_deque(acp_deque_t deque, int index);

/**
 * @JP
 * @brief デック先頭イテレータ
 *
 * デックの先頭データを指すイテレータを返す。
 *
 * @param deque デック型データの参照
 * @retval acp_deque_it_t デックの先頭データを指すイテレータ
 *
 * @EN
 * @brief Query for the iterator of the head of deque data
 *
 * @param deque A reference of deque data.
 * @retval acp_deque_it_t An iterator of the head of deque data
 * @ENDL
 */
extern acp_deque_it_t acp_begin_deque(acp_deque_t deque);

/**
 * @JP
 * @brief デック容量
 *
 * デックが領域拡張せずに保持できるサイズを返す。
 *
 * @param deque デック型データの参照
 * @retval size_t デックが領域拡張せずに保持できるサイズ
 *
 * @EN
 * @brief Capacity of deque
 *
 * Query for the capacity of the deque type data
 *
 * @param deque A reference of deque type data
 * @retval size_t The size of deque type data
 * @ENDL
 */
extern size_t acp_capacity_deque(acp_deque_t deque);

/**
 * @JP
 * @brief デック消去
 *
 * デックを０にする。
 *
 * @param deque デック型データの参照
 *
 * @EN
 * @brief Deque elimination
 *
 * Set the size of the deque to be zero.
 *
 * @param deque A reference of deque data.
 * @ENDL
 */
extern void acp_clear_deque(acp_deque_t deque);

/**
 * @JP
 * @brief デック生成
 *
 * 任意のプロセスでデック型データを生成する。
 *
 * @param size 要素サイズ
 * @param rank ランク番号
 * @retval "member ga == ACP_GA_NULL" 失敗
 * @retval 以外 生成したデック型データの参照
 *
 * @EN
 * @brief Deque creation
 *
 * Creates a deque type data on any process.
 *
 * @param size Size of element.
 * @param rank Rank number.
 * @retval "member ga == ACP_GA_NULL" Fail
 * @retval otherwise A reference of created deque data.
 * @ENDL
 */
extern acp_deque_t acp_create_deque(size_t size, int rank);

/**
 * @JP
 * @brief デック破棄
 *
 * デック型データを破棄する。
 *
 * @param deque デック型データの参照
 *
 * @EN
 * @brief Deque destruction
 *
 * Destroies a deque type data.
 *
 * @param deque A reference of deque data.
 * @ENDL
 */
extern void acp_destroy_deque(acp_deque_t deque);

/**
 * @JP
 * @brief デック空チェック
 *
 * デックが空かどうかを返す。
 *
 * @param deque デック型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for deque empty
 *
 * @param deque A reference of deque data.
 * @retval 1 Empty
 * @retval 0 There is a deque data
 * @ENDL
 */
extern int acp_empty_deque(acp_deque_t deque);

/**
 * @JP
 * @brief デック末尾イテレータ
 *
 * デックの最終データの直後を指すイテレータを返す。
 *
 * @param deque デック型データの参照
 * @retval acp_deque_it_t デックの最終データの直後を指すイテレータ
 *
 * @EN
 * @brief Query for the iterator of just behind of the end of deque data
 *
 * @param deque A reference of deque data.
 * @retval acp_deque_it_t An iterator of just behind of the end of deque data
 * @ENDL
 */
extern acp_deque_it_t acp_end_deque(acp_deque_t deque);

/**
 * @JP
 * @brief デック削除
 *
 * デックからデータを削除する。
 *
 * @param it 削除するデータの先頭を指すイテレータ
 * @param size 削除するデータのサイズ
 * @retval acp_deque_it_t 削除されたデータの直後を指すイテレータ
 *
 * @EN
 * @brief Deletion of the deque data
 *
 * @param it An iterator of deque data to erase
 * @param size The size of data to erase
 * @retval acp_deque_it_t An iterator of just behind of the deleted deque data
 * @ENDL
 */
extern acp_deque_it_t acp_erase_deque(acp_deque_it_t it, size_t size);

/**
 * @JP
 * @brief デック範囲削除
 *
 * デックからstartからend直前までのデータを削除する。
 *
 * @param start 削除するデータの先頭を指すイテレータ
 * @param end 削除するデータの末尾の直後を指すサイズ
 * @retval acp_deque_it_t 削除されたデータの直後を指すイテレータ
 *
 * @EN
 * @brief Deletion of the deque data from "start" to "end"
 *
 * @param start The iterator of deque data to erase
 * @param end The iterator of just behind of the deleting deque data
 * @retval acp_deque_it_t The iterator of just behind of the deleted deque data
 * @ENDL
 */
extern acp_deque_it_t acp_erase_range_deque(acp_deque_it_t start, acp_deque_it_t end);

/**
 * @JP
 * @brief デック挿入
 *
 * 指定した位置にデータを挿入する。
 *
 * @param it データを挿入する位置を指すイテレータ
 * @param ga 挿入するデータを格納しているグルーバルアドレス
 * @param size 挿入するデータのサイズ
 * @retval acp_deque_it_t 挿入したデータの先頭を指すイテレータ
 *
 * @EN
 * @brief Insertion of the deque data
 *
 * @param it An iterator of the point for inserting data
 * @param ga The global address of the data to insert
 * @param size The size of the data to insert
 * @retval acp_deque_it_t An iterator of head address of the inserted data
 * @ENDL
 */
extern acp_deque_it_t acp_insert_deque(acp_deque_it_t it, const acp_ga_t ga, size_t size);

/**
 * @JP
 * @brief デック範囲挿入
 *
 * 他のデックの指定範囲を、指定した位置にデータを挿入する。
 *
 * @param it データを挿入する位置を指すイテレータ
 * @param start 挿入するデータを先頭を指すイテレータ
 * @param end 挿入するデータの末尾の直後を指すイテレータ
 * @retval acp_deque_it_t 挿入したデータの先頭を指すイテレータ
 *
 * @EN
 * @brief Insertion of the deque data from "start" to "end"
 *
 * @param it An iterator of the point for inserting data
 * @param start The iterator of head address of the data to insert
 * @param end The iterator of just behind address of the data to insert
 * @retval acp_deque_it_t An iterator of head address of the inserted data
 * @ENDL
 */
extern acp_deque_it_t acp_insert_range_deque(acp_deque_it_t it, acp_deque_it_t start, acp_deque_it_t end);

/**
 * @JP
 * @brief デック末尾削除
 *
 * デックの末尾から、指定サイズのデータを削除する。
 *
 * @param deque データを削除するデックの参照
 * @param size 削除するデータのサイズ
 *
 * @EN
 * @brief Data deletion at the end of the deque data 
 *
 * @param deque A reference of the deque data to erase
 * @param size The size of data to erase
 * @ENDL
 */
extern void acp_pop_back_deque(acp_deque_t deque, size_t size);

/**
 * @JP
 * @brief デック先頭削除
 *
 * デックの先頭から、指定サイズのデータを削除する。
 *
 * @param deque データを削除するデックの参照
 * @param size 削除するデータのサイズ
 *
 * @EN
 * @brief Data deletion at the head of the deque data 
 *
 * @param deque A reference of the deque data to erase
 * @param size The size of data to erase
 * @ENDL
 */
extern void acp_pop_front_deque(acp_deque_t deque, size_t size);

/**
 * @JP
 * @brief デック末尾追加
 *
 * デックの末尾に、指定サイズのデータを追加する。
 *
 * @param deque データを追加するデックデータの査証
 * @param ga 追加するデータを格納しているグローバルアドレス
 * @param size 追加するデータのサイズ
 *
 * @EN
 * @brief Data addition at the end of the deque data 
 *
 * @param deque A reference of the deque to which data is added
 * @param ga The global address of the data to insert
 * @param size The size of the data to insert
 * @ENDL
 */
extern void acp_push_back_deque(acp_deque_t deque, const acp_ga_t ga, size_t size);

/**
 * @JP
 * @brief デック先頭追加
 *
 * デックの先頭に、指定サイズのデータを追加する。
 *
 * @param deque データを追加するデックデータの査証
 * @param ga 追加するデータを格納しているグローバルアドレス
 * @param size 追加するデータのサイズ
 *
 * @EN
 * @brief Data addition at the end of the deque data 
 *
 * @param deque A reference of the deque to which data is added
 * @param ga The global address of the data to insert
 * @param size The size of the data to insert
 * @ENDL
 */
extern void acp_push_front_deque(acp_deque_t deque, const acp_ga_t ga, size_t size);

/**
 * @JP
 * @brief デック領域確保
 *
 * デックの領域を指定した要素数確保する。
 *
 * @param deque 領域を確保するデックデータの参照
 * @param size 要素数
 *
 * @EN
 * @brief Reservation of a region in the deque type data
 *
 * @param deque A reference of the deque to reserve a region
 * @param size The number of element
 * @ENDL
 */
extern void acp_reserve_deque(acp_deque_t deque, size_t size);

/**
 * @JP
 * @brief デックサイズ
 *
 * デックに格納しているデータのサイズを返す。
 *
 * @param deque デックデータの参照
 * @retval size_t デックに格納しているデータのサイズ
 *
 * @EN
 * @brief Query of the data size in the deque
 *
 * @param deque A reference of the deque data
 * @retval size_t The data size in the deque
 * @ENDL
 */
extern size_t acp_size_deque(acp_deque_t deque);

/**
 * @JP
 * @brief デック交換
 *
 * ２つのデック型データの内容を交換する。
 *
 * @param deque1 交換するデック型データの一方の参照
 * @param deque2 交換するデック型データのもう一方の参照
 * 
 * @EN
 * @brief Deque swap
 *
 * @param deque1 A reference of deque data to be swapped.
 * @param deque2 Another reference of deque data to be swapped.
 *
 * @ENDL
 */
extern void acp_swap_deque(acp_deque_t deque1, acp_deque_t deque2);

/**
 * @JP
 * @brief デックイテレータ前進
 *
 * デックイテレータを前進する。
 *
 * @param it デックデータのイテレータ
 * @param n イテレータを進める数、負の値も指定可能
 * @retval acp_deque_it_t 前進したデックイテレータ
 *
 * @EN
 * @brief Advancement of an iterator for deque type data
 *
 * @param it The iterator of deque type data
 * @param n The number for advancing
 * @retval acp_deque_it_t The advanced iterator of deque type data 
 * @ENDL
 */
extern acp_deque_it_t acp_advance_deque_it(acp_deque_it_t it, int n);

/**
 * @JP
 * @brief デックイテレータ間接参照
 *
 * デックイテレータの参照先グローバルアドレスを返す。
 *
 * @param it デックデータのイテレータ
 * @param size size
 * @retval acp_pair_t デックイテレータの参照先グローバルアドレス
 *
 * @EN
 * @brief Query of the global address of a reference of deque tyep iterator
 *
 * @param it The iterator of deque type data
 * @param size size
 * @retval acp_pair_t The global address of a reference of deque type iterator 
 * @ENDL
 */
extern acp_pair_t acp_dereference_deque_it(acp_deque_it_t it, size_t size);

/**
 * @JP
 * @brief デックイテレータ距離
 *
 * ２つのデックイテレータ first, last 間の距離を返す。
 *
 * @param first 先頭位置のデックイテレータ
 * @param last 最終位置のデックイテレータ
 * @retval int ２つのデックイテレータ first, last 間の距離
 *
 * @EN
 * @brief Query of the distance of two iterators of deque type data between "first" and "last"
 *
 * @param first The iterator for head
 * @param last The iterator for end
 * @retval int The distance between "first" and "last"
 * @ENDL
 */
extern int acp_distance_deque_it(acp_deque_it_t first, acp_deque_it_t last);

#ifdef __cplusplus
}
#endif
/*@}*/ /* Deque */

/* List */
/**
 * @defgroup list ACP Middle Layer Dara Library List
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
 * @brief リスト代入
 *
 * リスト間でデータをコピーする。以前のデータは破棄される。
 *
 * @param list1 コピー先リスト型データの参照
 * @param list2 コピー元リスト型データの参照
 *
 * @EN
 * @brief List type data assignment
 *
 * Copy list type data between two lists.
 *
 * @param list1 A reference of destination list data.
 * @param list2 A reference of source list data.
 * @ENDL
 */
extern void acp_assign_list(acp_list_t list1, acp_list_t list2);

/**
 * @JP
 * @brief リスト範囲代入
 *
 * リストにstartからend直前までのデータをコピーする。
 * startとendは同じリストを指している必要がある。
 * 以前のデータは破棄される。
 *
 * @param list コピー先リスト型データの参照
 * @param start コピーするデータの先頭を指すイテレータ
 * @param end コピーするデータの末尾の直後を指すイテレータ
 *
 * @EN
 * @brief List assignment with range
 *
 * Copy list data from the point of "start" to "end".
 * 
 * @param list A reference of destination list data.
 * @param start A list type data iterator for pointing the starting address
 * @param end A list type data iterator for pointing the end address
 * @ENDL
 */
extern void acp_assign_range_list(acp_list_t list, acp_list_it_t start, acp_list_it_t end);

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
 * @brief リスト消去
 *
 * リストのサイズを０にする。
 *
 * @param list リスト型データの参照
 *
 * @EN
 * @brief List elimination
 *
 * Set the size of the list to be zero.
 *
 * @param list A reference of list data.
 * @ENDL
 */
extern void acp_clear_list(acp_list_t list);

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
 * @brief リスト空チェック
 *
 * リストが空かどうかを返す。
 *
 * @param list リスト型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for list empty
 *
 * @param list A reference of list data.
 * @retval 1 Empty
 * @retval 0 There is a list data
 * @ENDL
 */
extern int acp_empty_list(acp_list_t list);

/**
 * @JP
 * @brief リスト後端イテレータ取得
 *
 * リスト型データの後端要素を指すイテレータを取得する。
 *
 * @param list リスト型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 要素の直後の要素を指すリスト型イテレータ
 *
 * @EN
 * @brief Query for the tail iterator of a list
 *
 * 
 *
 * @param list A reference of list type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval oterhwise The iterator that points to the behind of the last element 
 * @ENDL
 */
extern acp_list_it_t acp_end_list(acp_list_t list);

/**
 * @JP
 * @brief リスト要素削除
 *
 * 指定した位置の要素をリスト型データから削除する。
 *
 * @param it 削除する要素を指すリストイテレータ
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 削除した要素の直後の要素を指すリストイテレータ
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
 * @brief リスト範囲削除
 *
 * リストからstartからend直前までのデータを削除する。
 *
 * @param start 削除するデータの先頭を指すイテレータ
 * @param end 削除するデータの末尾の直後を指すサイズ
 * @retval acp_list_it_t 削除されたデータの直後を指すイテレータ
 *
 * @EN
 * @brief Deletion of the list data from "start" to "end"
 *
 * @param start The iterator of list data to erase
 * @param end The iterator of just behind of the deleting list data
 * @retval acp_list_it_t The iterator of just behind of the deleted list data
 * @ENDL
 */
extern acp_list_it_t acp_erase_range_list(acp_list_it_t start, acp_list_it_t end);

/**
 * @JP
 * @brief リスト要素挿入
 *
 * 指定したプロセスに要素をコピーし、リスト型データの指定位置に挿入する。
 *
 * @param it リストイテレータ
 * @param elem 新しい要素に格納するデータ
 * @param rank 要素を複製するプロセス
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 挿入された要素を指すリストイテレータ
 *
 * @EN
 * @brief Insert a list element
 *
 * Copy an element to the specified process and inserts it into the specified position of the list.
 *
 * @param it An iterater of list type data.
 * @param elem The global address of the data to be added.
 * @param rank Rank of the process in which the element is copied.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The iterator that points to the inserted element.
 * @ENDL
 */
extern acp_list_it_t acp_insert_list(acp_list_it_t it, const acp_element_t elem, int rank);

/**
 * @JP
 * @brief リスト範囲挿入
 *
 * リストにstartからend直前までの要素をコピーする。
 * startとendは同じリストを指している必要がある。
 * 以前の要素は破棄される。新しい要素は代入元と同じランクに作られる。
 *
 * @param it データを挿入する位置を指すイテレータ
 * @param start 挿入するデータを先頭を指すイテレータ
 * @param end 挿入するデータの末尾の直後を指すイテレータ
 * @retval acp_list_it_t 挿入したデータの先頭を指すイテレータ
 *
 * @EN
 * @brief Insertion of the list data from "start" to "end"
 *
 * Copy deque data from the point of "start" to "end".
 *
 * @param it An iterator of the point for inserting data
 * @param start The iterator of head address of the data to insert
 * @param end The iterator of just behind address of the data to insert
 * @retval acp_list_it_t An iterator of head address of the inserted data
 * @ENDL
 */
extern acp_list_it_t acp_insert_range_list(acp_list_it_t it, acp_list_it_t start, acp_list_it_t end);

/**
 * @JP
 * @brief リスト併合
 *
 * ソート済みリスト同士を併合する。
 *
 * @param list1 併合先リスト型データの参照
 * @param list2 併合元リスト型データの参照
 * @param comp A function which return (1) negative number when elem1 < elem2, (2) 0 when elem1 = elem2, (3) positive number when elem1 > elem2
 *
 * @EN
 * @brief List type data merge
 *
 * Merge two sorted list type data.
 *
 * @param list1 A reference of destination list data.
 * @param list2 A reference of source list data.
 * @param comp A function which return (1) negative number when elem1 < elem2, (2) 0 when elem1 = elem2, (3) positive number when elem1 > elem2
 * @ENDL
 */
extern void acp_merge_list(acp_list_t list1, acp_list_t list2, int (*comp)(const acp_element_t elem1, const acp_element_t elem2));

/**
 * @JP
 * @brief リスト末尾削除
 *
 * リストの末尾から、要素を削除する。
 *
 * @param list データを削除するリスト型データの参照
 *
 * @EN
 * @brief Data deletion at the end of the list data 
 *
 * @param list A reference of the list type data to erase
 * @ENDL
 */
extern void acp_pop_back_list(acp_list_t list);

/**
 * @JP
 * @brief リスト先頭削除
 *
 * リストの先頭から、要素を削除する。
 *
 * @param list データを削除するリスト型データの参照
 *
 * @EN
 * @brief Data deletion at the head of the list data 
 *
 * @param list A reference of the list type data to erase
 * @ENDL
 */
extern void acp_pop_front_list(acp_list_t list);

/**
 * @JP
 * @brief リスト末尾要素追加
 *
 * 指定したプロセスに要素をコピーし、リスト型データの末尾に挿入する。
 *
 * @param list リスト型データの参照
 * @param elem 追加する要素の参照
 * @param rank 要素を複製するプロセス
 *
 * @EN
 * @brief Inserts a list element at the tail of the list
 *
 * Inserts a data with specified size into the tail of the list.
 *
 * @param list A reference of list type data.
 * @param elem A reference of element to added
 * @param rank Rank of the process in which the element is copied.
 * @ENDL
 */
extern void acp_push_back_list(acp_list_t list, const acp_element_t elem, int rank);

/**
 * @JP
 * @brief リスト先頭要素追加
 *
 * 指定したプロセスに要素をコピーし、リスト型データの先頭に挿入する。
 *
 * @param list リスト型データの参照
 * @param elem 追加する要素の参照
 * @param rank 要素を複製するプロセス
 *
 * @EN
 * @brief Insert a list element at the head of the list
 *
 * Inserts a data with specified size into the head of the list.
 *
 * @param list A reference of list type data.
 * @param elem A reference of element to add
 * @param rank Rank of the process in which the element is copied.
 * @ENDL
 */
extern void acp_push_front_list(acp_list_t list, const acp_element_t elem, int rank);

/**
 * @JP
 * @brief リスト除去
 *
 * 一致する要素をすべて削除する。
 *
 * @param list リスト型データの参照
 * @param elem 比較する要素の参照
 *
 * @EN
 * @brief Erase a list
 *
 * Remove matching elements in the list.
 *
 * @param list A reference of list type data.
 * @param elem A reference of element to compare
 * @ENDL
 */
extern void acp_remove_list(acp_list_t list, const acp_element_t elem);

/**
 * @JP
 * @brief リスト反転
 *
 * リストの要素を逆順に接続しなおす。
 *
 * @param list リスト型データの参照
 *
 * @EN
 * @brief Reconnect elements of a list in reverse sequence
 *
 * @param list A reference of the list type data
 * @ENDL
 */
extern void acp_reverse_list(acp_list_t list);

/**
 * @JP
 * @brief リストサイズ
 *
 * リストに格納しているデータのサイズを返す。
 *
 * @param list リストデータの参照
 * @retval size_t リストに格納しているデータのサイズ
 *
 * @EN
 * @brief Query of the data size in the list
 *
 * @param list A reference of the list data
 * @retval size_t The data size in the list
 * @ENDL
 */
extern size_t acp_size_list(acp_list_t list);

/**
 * @JP
 * @brief リストソート
 *
 * リスト型データをソートする。
 *
 * @param list リスト型データの参照
 * @param comp A function which return (1) negative number when it1 < it2, (2) 0 when it1 = it2, (3) positive number when it1 > it2
 *
 * @EN
 * @brief Sorting List type data
 *
 * @param list A reference of list data.
 * @param comp A function which return (1) negative number when it1 < it2, (2) 0 when it1 = it2, (3) positive number when it1 > it2
 * @ENDL
 */
extern void acp_sort_list(acp_list_t list, int (*comp)(const acp_element_t elem1, const acp_element_t elem2));

/**
 * @JP
 * @brief リスト接合
 *
 * リスト型データから要素を抜き出し、別のリストの任意の位置に挿入する。
 *
 * @param it1 要素を挿入する位置のリスト型データのイテレータ
 * @param it2 抜き出す要素を指すリスト型データのイテレータ
 *
 * @EN
 * @brief Splice list type data
 *
 * @param it1 An iterator of list data where an element is inserting.
 * @param it2 An iterator of list data whose element is extracted.
 * @ENDL
 */
extern void acp_splice_list(acp_list_it_t it1, acp_list_it_t it2);

/**
 * @JP
 * @brief リスト範囲接合
 *
 * リストにstartからend直前までのデータを挿入する。
 * startとendは同じリストを指している必要がある。
 * 以前のデータは破棄される。
 *
 * @param it リストを挿入する位置のイテレータ
 * @param start 抜き出す要素の先頭を指すイテレータ
 * @param end 抜きさす要素の末尾の直後を指すイテレータ
 *
 * @EN
 * @brief Spilice list with range
 *
 * Move list data from the point of "start" to "end" to another list.
 * 
 * @param it A reference of destination list data.
 * @param start A list type data iterator for pointing the starting address
 * @param end A list type data iterator for pointing the end address
 * @ENDL
 */
extern void acp_splice_range_list(acp_list_it_t it, acp_list_it_t start, acp_list_it_t end);

/**
 * @JP
 * @brief リスト交換
 *
 * ２つのリスト型データの内容を交換する。
 *
 * @param list1 交換するリスト型データの一方の参照
 * @param list2 交換するリスト型データのもう一方の参照
 * 
 * @EN
 * @brief Swap list type data
 *
 * 
 *
 * @param list1 A reference of list data to be swapped.
 * @param list2 Another reference of list data to be swapped.
 *
 * @ENDL
 */
extern void acp_swap_list(acp_list_t list1, acp_list_t list2);

/**
 * @JP
 * @brief リスト重複除去
 *
 * リスト内で重複した要素を削除する。
 *
 * @param list リスト型データの参照
 * 
 * @EN
 * @brief Unique list type data
 *
 * @param list A reference of list data.
 *
 * @ENDL
 */
extern void acp_unique_list(acp_list_t list);

/**
 * @JP
 * @brief リストイテレータ前進
 *
 * リストイテレータを前進する。
 *
 * @param it リストデータのイテレータ
 * @param n イテレータを進める数、負の値も指定可能
 * @retval acp_list_it_t 前進したリストイテレータ
 *
 * @EN
 * @brief Advancement of an iterator for list type data
 *
 * @param it The iterator of list type data
 * @param n The number for advancing
 * @retval acp_list_it_t The advanced iterator of list type data 
 * @ENDL
 */
extern acp_list_it_t acp_advance_list_it(acp_list_it_t it, int n);

/**
 * @JP
 * @brief リストイテレータ減算
 *
 * リストイテレータを一つ減少させる。
 *
 * @param it リスト型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 デクリメントしたイテレータ
 *
 * @EN
 * @brief Decrement an iterater of a list data
 *
 * Decrements an iterater of a list data.
 *
 * @param it A reference of list type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The previous iterator of the specified one.
 * @ENDL
 */
extern acp_list_it_t acp_decrement_list_it(acp_list_it_t it);

/**
 * @JP
 * @brief リストイテレータ間接参照
 *
 * リストイテレータの参照先グローバルアドレスを返す。
 *
 * @param it リストデータのイテレータ
 * @retval acp_ga_t リストイテレータの参照先グローバルアドレス
 *
 * @EN
 * @brief Query of the global address of a reference of list tyep iterator
 *
 * @param it The iterator of list type data
 * @retval acp_ga_t The global address of a reference of list type iterator 
 * @ENDL
 */
extern acp_element_t acp_dereference_list_it(acp_list_it_t it);

/**
 * @JP
 * @brief リストイテレータ距離
 *
 * ２つのリストイテレータ first, last 間の距離を返す。
 *
 * @param first 先頭位置のリストイテレータ
 * @param last 最終位置のリストイテレータ
 * @retval int ２つのリストイテレータ first, last 間の距離
 *
 * @EN
 * @brief Query of the distance of two iterators of list type data between "first" and "last"
 *
 * @param first The iterator for head
 * @param last The iterator for end
 * @retval int The distance between "first" and "last"
 * @ENDL
 */
extern int acp_distance_list_it(acp_list_it_t first, acp_list_it_t last);

/**
 * @JP
 * @brief リストイテレータ加算
 *
 * リストイテレータを一つ増加させる。
 *
 * @param it リスト型データのイテレータ
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 インクリメントしたイテレータ
 *
 * @EN
 * @brief Increment an iterater of a list data
 *
 *
 * @param it A reference of list type iterator.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The next iterator of the specified one.
 * @ENDL
 */
extern acp_list_it_t acp_increment_list_it(acp_list_it_t it);

#ifdef __cplusplus
}
#endif
/*@}*/ /* List */

/* Set */
/**
 * @defgroup set ACP Middle Layer Dara Library Set
 * @ingroup acpdl
 * 
 * ACP Middle Layer Data Library Set
 *
 * @{
 */

typedef struct {
    acp_ga_t ga;
    uint64_t num_ranks;
    uint64_t num_slots;
} acp_set_t;	/*!< Set data type. */

typedef struct {
    acp_set_t set;
    int rank;
    int slot;
    acp_ga_t elem;
} acp_set_it_t;	/*!< Iterater of set data type. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief セットローカル代入
 *
 * 自プロセスに配置されたキーをコピーする。以前のデータは破棄される。
 *
 * @param set1 コピー先セット型データの参照
 * @param set2 コピー元セット型データの参照
 *
 * @EN
 * @brief Set type data assignment
 *
 * Among the keys of set2, copy the keys that are allocated in the
 * caller process. Keys of the destination set (set1) are destroyed.
 *
 * @param set1 A reference of destination set data.
 * @param set2 A reference of source set data.
 * @ENDL
 */
extern void acp_assign_local_set(acp_set_t set1, acp_set_t set2);

/**
 * @JP
 * @brief セット代入
 *
 * セット間でデータをコピーする。以前のデータは破棄される。
 *
 * @param set1 コピー先セット型データの参照
 * @param set2 コピー元セット型データの参照
 *
 * @EN
 * @brief Set type data assignment
 *
 * Copy keys of set2. Keys of the destination set (set1) are destroyed.
 *
 * @param set1 A reference of destination set data.
 * @param set2 A reference of source set data.
 * @ENDL
 */
extern void acp_assign_set(acp_set_t set1, acp_set_t set2);

/**
 * @JP
 * @brief セットローカル先頭イテレータ
 *
 * 自プロセスに配置されたキーの先頭を指すイテレータを返す。
 *
 * @param set セット型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 先頭イテレータ
 *
 * @EN
 * @brief Query for the local head iterator of a set
 *
 * Among the keys of set, query for the first key of the keys that are
 * allocated in the caller process.
 *
 * @param set A reference of set type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The head iterator of the set.
 * @ENDL
 */
extern acp_set_it_t acp_begin_local_set(acp_set_t set);

/**
 * @JP
 * @brief セット先頭イテレータ取得
 *
 * セット型データの先頭要素を指すイテレータを取得する。
 *
 * @param set セット型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 先頭イテレータ
 *
 * @EN
 * @brief Query for the head iterator of a set
 *
 * 
 *
 * @param set A reference of set type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The head iterator of the set.
 * @ENDL
 */
extern acp_set_it_t acp_begin_set(acp_set_t set);

/**
 * @JP
 * @brief セット消去
 *
 * セットのサイズを０にする。
 *
 * @param set セット型データの参照
 *
 * @EN
 * @brief Set elimination
 *
 * Set the size of the set to be zero.
 *
 * @param set A reference of set data.
 * @ENDL
 */
extern void acp_clear_set(acp_set_t set);

extern acp_list_t acp_collapse_set(acp_set_t set);

/**
 * @JP
 * @brief セット生成
 *
 * 任意のプロセスでセット型データを生成する。
 *
 * @param num_ranks バケットを配置するプロセス数
 * @param ranks バケットを配置するプロセスのランク番号配列
 * @param num_slots 1プロセスあたりのバケットスロット数
 * @param rank セットの制御情報を配置するランク番号
 * @retval "member ga == ACP_GA_NULL" 失敗
 * @retval 以外 生成したセット型データの参照
 *
 * @EN
 * @brief Set creation
 *
 * Creates a set type data on any process.
 *
 * @param num_ranks A process number for assigning buckets
 * @param ranks An array of rank number for assigning buckets
 * @param num_slots A number of bucket slot for one process
 * @param rank The rank number where has the control information of a set
 * @retval "member ga == ACP_GA_NULL" Fail
 * @retval otherwise A reference of created set data.
 * @ENDL
 */
extern acp_set_t acp_create_set(int num_ranks, const int* ranks, int num_slots, int rank);

/**
 * @JP
 * @brief セット破棄
 *
 * セット型データを破棄する。
 *
 * @param set セット型データの参照
 *
 * @EN
 * @brief Set destruction
 *
 * Destroies a set type data.
 *
 * @param set A reference of set data.
 * @ENDL
 */
extern void acp_destroy_set(acp_set_t set);

/**
 * @JP
 * @brief セットローカル空チェック
 *
 * セットに自プロセスに配置されたキーがないかどうかを返す。
 *
 * @param set セット型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for local set empty
 *
 * Query if, among the keys of set, the number of keys that are
 * allocated in the caller process is zero.
 *
 * @param set A reference of set data.  
 * @retval 1 Empty 
 * @retval 0 There is a set data 
 * @ENDL
 */
extern int acp_empty_local_set(acp_set_t set);

/**
 * @JP
 * @brief セット空チェック
 *
 * セットが空かどうかを返す。
 *
 * @param set セット型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for set empty
 *
 * @param set A reference of set data.
 * @retval 1 Empty
 * @retval 0 There is a set data
 * @ENDL
 */
extern int acp_empty_set(acp_set_t set);

/**
 * @JP
 * @brief セットローカル後端イテレータ取得
 *
 * セット型データの後端要素を指すイテレータを取得する。
 *
 * @param set セット型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 要素の直後の要素を指すセットイテレータ
 *
 * @EN
 * @brief Query for the tail iterator of a set
 *
 * Among the keys of set, query for the last key of the keys 
 * that are allocated in the caller process.
 *
 * @param set A reference of set type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval oterhwise The iterator that points to the behind of the last element 
 * @ENDL
 */
extern acp_set_it_t acp_end_local_set(acp_set_t set);

/**
 * @JP
 * @brief セット後端イテレータ取得
 *
 * セット型データの後端要素を指すイテレータを取得する。
 *
 * @param set セット型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 要素の直後の要素を指すセットイテレータ
 *
 * @EN
 * @brief Query for the tail iterator of a set
 *
 * 
 *
 * @param set A reference of set type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval oterhwise The iterator that points to the behind of the last element 
 * @ENDL
 */
extern acp_set_it_t acp_end_set(acp_set_t set);

/**
 * @JP
 * @brief セット検索
 *
 * 一致するキーを検索する。
 *
 * @param set セット型データへの参照
 * @param key 検索するキー
 * @retval イテレータ　一致したキーを指すイテレータ
 * @retval イテレータ　一致したキーがない場合は末尾イテレータ
 *
 * @EN
 * @brief Search for the key in set that matches with key.
 *
 * @param set A reference of set type data
 * @param key Key
 * @retval iterator An iterator of the key that matches with key.
 * @retval iterator The end of the iterator.
 * @ENDL
 */
extern acp_set_it_t acp_find_set(acp_set_t set, acp_element_t key);

/**
 * @JP
 * @brief セット挿入
 *
 * セットに新しいキーを挿入する。
 * 既にキーが存在する場合は挿入されないが、戻り値は成功になる。
 *
 * @param set セット型データへの参照
 * @param key 挿入するキー
 * @retval 1 成功
 * @retval 0 失敗
 *
 * @EN
 * @brief Insert a set element
 *
 * @param set A reference of set type data
 * @param key An inserting key
 * @retval 1 Success
 * @retval 0 Fail
 * @ENDL
 */
extern int acp_insert_set(acp_set_t set, acp_element_t key);

/**
 * @JP
 * @brief セットローカル併合
 *
 * 自プロセスに配置されたキーを併合する。
 *
 * @param set1 併合先セット型データへの参照
 * @param set2 併合元セット型データへの参照
 *
 * @EN
 * @brief Merge the local keys
 *
 * Among the keys of set2, merge the keys that are allocated 
 * in the caller process to set1.
 *
 * @param set1 A reference of the destination set type data
 * @param set2 A reference of the source set type data
 * @ENDL
 */
extern void acp_merge_local_set(acp_set_t set1, acp_set_t set2);

/**
 * @JP
 * @brief セット併合
 *
 * 併合元セットの全要素を併合先セットに併合する。
 *
 * @param set1 併合先セット型データへの参照
 * @param set2 併合元セット型データへの参照
 *
 * @EN
 * @brief Merge the keys
 *
 * Merge set2 to set1.
 *
 * @param set1 A reference of the destination set type data
 * @param set2 A reference of the source set type data
 * @ENDL
 */
extern void acp_merge_set(acp_set_t set1, acp_set_t set2);

/**
 * @JP
 * @brief セットローカル移動
 *
 * 自プロセスに配置されたキーを移動する。
 *
 * @param set1 移動先セット型データへの参照
 * @param set2 移動元セット型データへの参照
 *
 * @EN
 * @brief Move the local keys
 *
 * Among the keys of set2, move the keys that are allocated 
 * in the caller process to set1.
 *
 * @param set1 A reference of the destination set type data
 * @param set2 A reference of the source set type data
 * @ENDL
 */
extern void acp_move_local_set(acp_set_t set1, acp_set_t set2);

/**
 * @JP
 * @brief セット移動
 *
 * 移動元セットの全要素を移動先セットに移動する。
 *
 * @param set1 移動先セット型データへの参照
 * @param set2 移動元セット型データへの参照
 *
 * @EN
 * @brief Move the keys
 *
 * Move the keys of set2 to set1.
 *
 * @param set1 A reference of the destination set type data
 * @param set2 A reference of the source set type data
 * @ENDL
 */
extern void acp_move_set(acp_set_t set1, acp_set_t set2);

/**
 * @JP
 * @brief セット除去
 *
 * セットからキーを削除する。
 *
 * @param set セット
 * @param key 削除する key
 *
 * @EN
 * @brief Erase a set element
 *
 * Delete the key of set that matches with key.
 *
 * @param set set
 * @param key key
 * @ENDL
 */
extern void acp_remove_set(acp_set_t set, acp_element_t key);

/**
 * @JP
 * @brief セットローカルサイズ
 *
 * 自プロセスに配置されているキーの数を返す。
 *
 * @param set セットデータの参照
 * @retval size_t 自プロセスに配置されているキーの数
 *
 * @EN
 * @brief Query of the number ot local keys in the set
 *
 * @param set A reference of the set data
 * @retval size_t Numbers of keys
 * @ENDL
 */
extern size_t acp_size_local_set(acp_set_t set);

/**
 * @JP
 * @brief セットサイズ
 *
 * セットに格納しているデータのサイズを返す。
 *
 * @param set セットデータの参照
 * @retval size_t セットに格納しているデータのサイズ
 *
 * @EN
 * @brief Query of the data size in the set
 *
 * @param set A reference of the set data
 * @retval size_t The data size in the set
 * @ENDL
 */
extern size_t acp_size_set(acp_set_t set);

/**
 * @JP
 * @brief セット交換
 *
 * ２つのセット型データの内容を交換する。
 *
 * @param set1 交換するセット型データの一方の参照
 * @param set2 交換するセット型データのもう一方の参照
 * 
 * @EN
 * @brief Swap set type data
 *
 * @param set1 A reference of set data to be swapped.
 * @param set2 Another reference of set data to be swapped.
 *
 * @ENDL
 */
extern void acp_swap_set(acp_set_t set1, acp_set_t set2);

/**
 * @JP
 * @brief セットイテレータ間接参照
 *
 * セットイテレータの参照先のキーを返す。
 *
 * @param it セットデータのイテレータ
 * @retval member ga 参照先キーのグローバルアドレス
 * @retval member size 参照先キーのサイズ
 *
 * @EN
 * @brief Query of the key of a reference of set tyep iterator
 *
 * @param it The iterator of set type data
 * @retval member ga The global address of the key referenced by the specified iterator
 * @retval member size The size of the key referenced by the specified iterator
 * @ENDL
 */
extern acp_element_t acp_dereference_set_it(acp_set_it_t it);

/**
 * @JP
 * @brief セットイテレータ加算
 *
 * セットイテレータを一つ増加させる。
 *
 * @param it セット型データのイテレータ
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 インクリメントしたイテレータ
 *
 * @EN
 * @brief Increment an iterater of a set data
 *
 *
 * @param it A reference of set type iterator.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The next iterator of the specified one.
 * @ENDL
 */
extern acp_set_it_t acp_increment_set_it(acp_set_it_t it);

#ifdef __cplusplus
}
#endif
/*@}*/ /* Set */

/* Multi-set */
/**
 * @defgroup set ACP Middle Layer Dara Library Multiset
 * @ingroup acpdl
 * 
 * ACP Middle Layer Data Library Multiset
 *
 * @{
 */

typedef struct {
    acp_ga_t ga;
    uint64_t num_ranks;
    uint64_t num_slots;
} acp_multiset_t;	/*!< Multiset data type. */

typedef struct {
    acp_multiset_t set;
    int rank;
    int slot;
    acp_ga_t elem;
} acp_multiset_it_t;	/*!< Iterater of multiset data type. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief マルチセットローカル代入
 *
 * 自プロセスに配置されたキーをコピーする。以前のデータは破棄される。
 *
 * @param set1 コピー先マルチセット型データの参照
 * @param set2 コピー元マルチセット型データの参照
 *
 * @EN
 * @brief Multiset type data assignment
 *
 * Among the keys of set2, copy the keys that are allocated in the
 * caller process. Keys of the destination set (set1) are destroyed.
 *
 * @param set1 A reference of destination multiset data.
 * @param set2 A reference of source multiset data.
 * @ENDL
 */
extern void acp_assign_local_multiset(acp_multiset_t set1, acp_multiset_t set2);

/**
 * @JP
 * @brief マルチセット代入
 *
 * マルチセット間でデータをコピーする。以前のデータは破棄される。
 *
 * @param set1 コピー先マルチセット型データの参照
 * @param set2 コピー元マルチセット型データの参照
 *
 * @EN
 * @brief Multiet type data assignment
 *
 * Copy keys of set2. Keys of the destination set (set1) are destroyed.
 *
 * @param set1 A reference of destination multiset data.
 * @param set2 A reference of source multiset data.
 * @ENDL
 */
extern void acp_assign_multiset(acp_multiset_t set1, acp_multiset_t set2);

/**
 * @JP
 * @brief マルチセットローカル先頭イテレータ
 *
 * 自プロセスに配置されたキーの先頭を指すイテレータを返す。
 *
 * @param set マルチセット型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 先頭イテレータ
 *
 * @EN
 * @brief Query for the local head iterator of a set
 *
 * Among the keys of set, query for the first key of the keys that are
 * allocated in the caller process.
 *
 * @param set A reference of multiset type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The head iterator of the multiset.
 * @ENDL
 */
extern acp_multiset_it_t acp_begin_local_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセット先頭イテレータ取得
 *
 * マルチセット型データの先頭要素を指すイテレータを取得する。
 *
 * @param set マルチセット型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 先頭イテレータ
 *
 * @EN
 * @brief Query for the head iterator of a multiset
 *
 * 
 *
 * @param set A reference of multiset type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The head iterator of the multiset.
 * @ENDL
 */
extern acp_multiset_it_t acp_begin_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセット消去
 *
 * マルチセットのサイズを０にする。
 *
 * @param set マルチセット型データの参照
 *
 * @EN
 * @brief Multiset elimination
 *
 * Set the size of the multiset to be zero.
 *
 * @param set A reference of multiset data.
 * @ENDL
 */
extern void acp_clear_multiset(acp_multiset_t set);

extern acp_set_t acp_collapse_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセット生成
 *
 * 任意のプロセスでマルチセット型データを生成する。
 *
 * @param num_ranks バケットを配置するプロセス数
 * @param ranks バケットを配置するプロセスのランク番号配列
 * @param num_slots 1プロセスあたりのバケットスロット数
 * @param rank マルチセットの制御情報を配置するランク番号
 * @retval "member ga == ACP_GA_NULL" 失敗
 * @retval 以外 生成したセット型データの参照
 *
 * @EN
 * @brief Multiet creation
 *
 * Creates a multiset type data on any process.
 *
 * @param num_ranks A process number for assigning buckets
 * @param ranks An array of rank number for assigning buckets
 * @param num_slots A number of bucket slot for one process
 * @param rank The rank number where has the control information of a multiset
 * @retval "member ga == ACP_GA_NULL" Fail
 * @retval otherwise A reference of created set data.
 * @ENDL
 */
extern acp_multiset_t acp_create_multiset(int num_ranks, const int* ranks, int num_slots, int rank);

/**
 * @JP
 * @brief マルチセット破棄
 *
 * マルチセット型データを破棄する。
 *
 * @param set マルチセット型データの参照
 *
 * @EN
 * @brief Multiset destruction
 *
 * Destroies a multiset type data.
 *
 * @param set A reference of multiset data.
 * @ENDL
 */
extern void acp_destroy_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセットローカル空チェック
 *
 * マルチセットに自プロセスに配置されたキーがないかどうかを返す。
 *
 * @param set マルチセット型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for local multiset empty
 *
 * Query if, among the keys of multiset, the number of keys that are
 * allocated in the caller process is zero.
 *
 * @param set A reference of multiset data.  
 * @retval 1 Empty 
 * @retval 0 There is a multiset data 
 * @ENDL
 */
extern int acp_empty_local_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセット空チェック
 *
 * マルチセットが空かどうかを返す。
 *
 * @param set マルチセット型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for multiset empty
 *
 * @param set A reference of multiset data.
 * @retval 1 Empty
 * @retval 0 There is a multiset data
 * @ENDL
 */
extern int acp_empty_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセットローカル後端イテレータ取得
 *
 * マルチセット型データの後端要素を指すイテレータを取得する。
 *
 * @param set マルチセット型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 要素の直後の要素を指すマルチセットイテレータ
 *
 * @EN
 * @brief Query for the tail iterator of a multiset
 *
 * Among the keys of multiset, query for the last key of the keys 
 * that are allocated in the caller process.
 *
 * @param set A reference of multiset type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval oterhwise The iterator that points to the behind of the last element 
 * @ENDL
 */
extern acp_multiset_it_t acp_end_local_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセット後端イテレータ取得
 *
 * マルチセット型データの後端要素を指すイテレータを取得する。
 *
 * @param set マルチセット型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 要素の直後の要素を指すマルチセットイテレータ
 *
 * @EN
 * @brief Query for the tail iterator of a multiset
 *
 * 
 *
 * @param set A reference of multiset type data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval oterhwise The iterator that points to the behind of the last element 
 * @ENDL
 */
extern acp_multiset_it_t acp_end_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセット検索
 *
 * 一致するキーを検索する。
 *
 * @param set マルチセット型データへの参照
 * @param key 検索するキー
 * @retval イテレータ　一致したキーを指すイテレータ
 * @retval イテレータ　一致したキーがない場合は末尾イテレータ
 *
 * @EN
 * @brief Search for the key in multiset that matches with key.
 *
 * @param set A reference of multiset type data
 * @param key Key
 * @retval iterator An iterator of the key that matches with key.
 * @retval iterator The end of the iterator.
 * @ENDL
 */
extern acp_multiset_it_t acp_find_multiset(acp_multiset_t set, acp_element_t key);

/**
 * @JP
 * @brief マルチセット挿入
 *
 * マルチセットに新しいキーを挿入する。
 * 既にキーが存在する場合は、カウンタ値が1加算される。
 *
 * @param set マルチセット型データへの参照
 * @param key 挿入するキー
 * @retval 1 成功
 * @retval 0 失敗
 *
 * @EN
 * @brief Insert a multiset element
 *
 * Insert the key to the multiset or increment the counter value of the key
 * if the key already exists in the multiset
 *
 * @param set A reference of multiset type data
 * @param key An inserting key
 * @retval 1 Success
 * @retval 0 Fail
 * @ENDL
 */
extern int acp_insert_multiset(acp_multiset_t set, acp_element_t key);

/**
 * @JP
 * @brief マルチセットローカル併合
 *
 * 自プロセスに配置されたキーを併合する。
 *
 * @param set1 併合先マルチセット型データへの参照
 * @param set2 併合元マルチセット型データへの参照
 *
 * @EN
 * @brief Merge the local keys
 *
 * Among the keys of set2, merge the keys that are allocated 
 * in the caller process to set1.
 *
 * @param set1 A reference of the destination multiset type data
 * @param set2 A reference of the source multiset type data
 * @ENDL
 */
extern void acp_merge_local_multiset(acp_multiset_t set1, acp_multiset_t set2);

/**
 * @JP
 * @brief マルチセット併合
 *
 * 併合元マルチセットの全要素を併合先マルチセットに併合する。
 *
 * @param set1 併合先マルチセット型データへの参照
 * @param set2 併合元マルチセット型データへの参照
 *
 * @EN
 * @brief Merge the keys
 *
 * Merge set2 to set1.
 *
 * @param set1 A reference of the destination multiset type data
 * @param set2 A reference of the source multiset type data
 * @ENDL
 */
extern void acp_merge_multiset(acp_multiset_t set1, acp_multiset_t set2);

/**
 * @JP
 * @brief マルチセットローカル移動
 *
 * 自プロセスに配置されたキーを移動する。
 *
 * @param set1 移動先マルチセット型データへの参照
 * @param set2 移動元マルチセット型データへの参照
 *
 * @EN
 * @brief Move the local keys
 *
 * Among the keys of set2, move the keys that are allocated 
 * in the caller process to set1.
 *
 * @param set1 A reference of the destination multiset type data
 * @param set2 A reference of the source multiset type data
 * @ENDL
 */
extern void acp_move_local_multiset(acp_multiset_t set1, acp_multiset_t set2);

/**
 * @JP
 * @brief マルチセット移動
 *
 * 移動元マルチセットの全要素を移動先マルチセットに移動する。
 *
 * @param set1 移動先マルチセット型データへの参照
 * @param set2 移動元マルチセット型データへの参照
 *
 * @EN
 * @brief Move the keys
 *
 * Move the keys of set2 to set1.
 *
 * @param set1 A reference of the destination multiset type data
 * @param set2 A reference of the source multiset type data
 * @ENDL
 */
extern void acp_move_multiset(acp_multiset_t set1, acp_multiset_t set2);

/**
 * @JP
 * @brief マルチセット除去
 *
 * マルチセットからキーを削除する。
 * カウンタ値が2以上の場合、カウンタ値を1減算し、キーは削除しない。
 *
 * @param set マルチセット
 * @param key 削除する key
 *
 * @EN
 * @brief Erase a multiset element
 *
 * Delete the key of multiset that matches with key, or decrement
 * the counter value of the key if the value is equal or greater than two.
 *
 * @param set multiset
 * @param key key
 * @ENDL
 */
extern void acp_remove_multiset(acp_multiset_t set, acp_element_t key);

/**
 * @JP
 * @brief マルチセット全除去
 *
 * カウンタ値に関わらず、マルチセットからキーを削除する。
 *
 * @param set マルチセット
 * @param key 削除する key
 *
 * @EN
 * @brief Erase a multiset element
 *
 * Delete the key of multiset that matches with key, regardless of
 * the counter value of the key.
 *
 * @param set multiset
 * @param key key
 * @ENDL
 */
extern void acp_remove_all_multiset(acp_multiset_t set, acp_element_t key);

/**
 * @JP
 * @brief マルチセット取得
 *
 * キーが一致する要素のカウンタ値を取得する。
 *
 * @param set マルチセットデータの参照
 * @param key キー
 * @retval 0 一致したキーなし
 * @retval カウンタ値
 *
 * @EN
 * @brief Retrieve the map
 *
 * From multiset, retrieve the counter value of the stored key
 * that matches with the specified key.
 *
 * @param map A reference of the multiset data
 * @param key key
 * @retval 0 No matching key.
 * @retval Counter value.
 * @ENDL
 */
extern uint64_t acp_retrieve_multiset(acp_multiset_t set, acp_element_t key);

/**
 * @JP
 * @brief マルチセットローカルサイズ
 *
 * 自プロセスに配置されているキーの数を返す。
 *
 * @param set マルチセットデータの参照
 * @retval size_t 自プロセスに配置されているキーの数
 *
 * @EN
 * @brief Query of the number ot local keys in the multiset
 *
 * @param set A reference of the multiset data
 * @retval size_t Numbers of keys
 * @ENDL
 */
extern size_t acp_size_local_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセットサイズ
 *
 * マルチセットに格納しているデータのサイズを返す。
 *
 * @param set マルチセットデータの参照
 * @retval size_t セットに格納しているデータのサイズ
 *
 * @EN
 * @brief Query of the data size in the multiset
 *
 * @param set A reference of the multiset data
 * @retval size_t The data size in the multiset
 * @ENDL
 */
extern size_t acp_size_multiset(acp_multiset_t set);

/**
 * @JP
 * @brief マルチセット交換
 *
 * ２つのマルチセット型データの内容を交換する。
 *
 * @param set1 交換するマルチセット型データの一方の参照
 * @param set2 交換するマルチセット型データのもう一方の参照
 * 
 * @EN
 * @brief Swap multiset type data
 *
 * @param set1 A reference of multiset data to be swapped.
 * @param set2 Another reference of multiset data to be swapped.
 *
 * @ENDL
 */
extern void acp_swap_multiset(acp_multiset_t set1, acp_multiset_t set2);

/**
 * @JP
 * @brief マルチセットイテレータ間接参照
 *
 * マルチセットイテレータの参照先のキーを返す。
 *
 * @param it マルチセットデータのイテレータ
 * @retval member ga 参照先キーのグローバルアドレス
 * @retval member size 参照先キーのサイズ
 *
 * @EN
 * @brief Query of the key of a reference of multiset tyep iterator
 *
 * @param it The iterator of multiset type data
 * @retval member ga The global address of the key referenced by the specified iterator
 * @retval member size The size of the key referenced by the specified iterator
 * @ENDL
 */
extern acp_element_t acp_dereference_multiset_it(acp_multiset_it_t it);

/**
 * @JP
 * @brief マルチセットイテレータ加算
 *
 * マルチセットイテレータを一つ増加させる。
 *
 * @param it マルチセット型データのイテレータ
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 インクリメントしたイテレータ
 *
 * @EN
 * @brief Increment an iterater of a multiset data
 *
 *
 * @param it A reference of multiset type iterator.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The next iterator of the specified one.
 * @ENDL
 */
extern acp_multiset_it_t acp_increment_multiset_it(acp_multiset_it_t it);

#ifdef __cplusplus
}
#endif
/*@}*/ /* Multiset */

/* Map */
/**
 * @defgroup map ACP Middle Layer Dara Library Map
 * @ingroup acpdl
 * 
 * ACP Middle Layer Data Library Map
 *
 * @{
 */

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
extern "C" {
#endif

/**
 * @JP
 * @brief マップローカル代入
 *
 * 自プロセスに配置された要素をコピーする。以前の要素は破棄される。
 *
 * @param map1 コピー先マップ型データの参照
 * @param map2 コピー元マップ型データの参照
 *
 * @EN
 * @brief Map local assignment
 *
 * Among the elements of map2, copy the elements that are allocated 
 * in the caller process. Elements of the destination map (map1) are destroyed.
 *
 * @param map1 A reference of destination map data.
 * @param map2 A reference of source map data.
 * @ENDL
 */
extern void acp_assign_local_map(acp_map_t map1, acp_map_t map2);

/**
 * @JP
 * @brief マップ代入
 *
 * マップ間で要素をコピーする。以前の要素は破棄される。
 *
 * @param map1 コピー先マップ型データの参照
 * @param map2 コピー元マップ型データの参照
 *
 * @EN
 * @brief Map assignment
 *
 * Copy elements of map2 to map1.
 * Elements of the destination map (map1) are destroyed.
 *
 * @param map1 A reference of destination map data.
 * @param map2 A reference of source map data.
 * @ENDL
 */
extern void acp_assign_map(acp_map_t map1, acp_map_t map2);

/**
 * @JP
 * @brief マップローカル先頭イテレータ
 *
 * 自プロセスに配置された要素の先頭を指すイテレータを返す。
 *
 * @param map マップ型データの参照
 * @retval acp_map_it_t マップの先頭データを指すイテレータ
 *
 * @EN
 * @brief Query for the local iterator of the head of map data
 *
 * Among the elements of map, query for the first element of 
 * the ones that are allocated in the caller process.
 *
 * @param map A reference of map data.
 * @retval acp_map_it_t An iterator of the head of map data
 * @ENDL
 */
extern acp_map_it_t acp_begin_local_map(acp_map_t map);

/**
 * @JP
 * @brief マップ先頭イテレータ
 *
 * マップの先頭要素を指すイテレータを返す。
 *
 * @param map マップ型データの参照
 * @retval acp_map_it_t マップの先頭データを指すイテレータ
 *
 * @EN
 * @brief Query for the iterator of the head of map data
 *
 * Query for the iterator that points to the first element of map.
 *
 * @param map A reference of map data.
 * @retval acp_map_it_t An iterator of the head of map data
 * @ENDL
 */
extern acp_map_it_t acp_begin_map(acp_map_t map);

/**
 * @JP
 * @brief マップローカル消去
 *
 * 自プロセスに配置された要素を削除する。
 *
 * @param map マップ型データの参照
 *
 * @EN
 * @brief Delete elements of lists in a map type data.
 *
 * Among the elements of map, delete all of the ones that are 
 * allocated in the caller process.
 *
 * @param map A reference of map data.
 * @ENDL
 */
extern void acp_clear_local_map(acp_map_t map);

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
 * Creates a map type data on any set of processes.
 *
 * @param num_ranks Number of processes.
 * @param ranks Array of the rank numbers of the processes to distribute map.
 * @param num_slots Number of slots
 * @param rank Rank number to place the information of the map.
 * @retval "member ga == ACP_MAP_NULL" Fail
 * @retval otherwise A reference of created map data.
 * @ENDL
 */
extern acp_map_t acp_create_map(int num_ranks, const int* ranks, int num_slots, int rank);

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
 * @brief マップローカル空チェック
 *
 * 自プロセスに配置されたデータがないかどうかを返す。
 *
 * @param map マップ型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for the local map is empty or not
 *
 * Query if, in the map, the number of elements that are 
 * allocated in the caller process is zero.
 *
 * @param map A reference of map data.
 * @retval 1 Empty
 * @retval 0 Not empty
 * @ENDL
 */
extern int acp_empty_local_map(acp_map_t map);

/**
 * @JP
 * @brief マップ空チェック
 *
 * マップが空かどうかを返す。
 *
 * @param map マップ型データの参照
 * @retval 1 空
 * @retval 0 データが存在する
 *
 * @EN
 * @brief Query for the map is empty or not
 *
 * Query for the emptiness of map.
 *
 * @param map A reference of map data.
 * @retval 1 Empty
 * @retval 0 Not empty
 * @ENDL
 */
extern int acp_empty_map(acp_map_t map);

/**
 * @JP
 * @brief マップローカル末尾イテレータ
 *
 * 自プロセスに配置された最後の要素の直後を指すイテレータを返す。
 *
 * @param map マップ型データの参照
 * @retval イテレータ 自プロセスに配置された最後の要素の直後を指すイテレータ
 *
 * @EN
 * @brief Loacl map end iterator
 *
 * Among the elements of map, query for the iterator just after 
 * the last element of the elements that are allocated in the caller process.
 *
 * @param map A reference of map data.
 * @retval iterator The iterator just after the tail element of a map.
 * @ENDL
 */
extern acp_map_it_t acp_end_local_map(acp_map_t map);

/**
 * @JP
 * @brief マップ末尾イテレータ
 *
 * マップの最終要素の直後を指すイテレータを返す。
 *
 * @param map マップ型データの参照
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 検索結果のイテレータの参照
 *
 * @EN
 * @brief Map end iterator
 *
 * Query for the iterator just after the tail element of a map.
 *
 * @param map A reference of map data.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The iterator just after the tail element of a map.
 * @ENDL
 */
extern acp_map_it_t acp_end_map(acp_map_t map);

/**
 * @JP
 * @brief マップ検索
 *
 * マップにあるキーと変数をキーで検索する
 *
 * @param map マップ型データの参照
 * @param key 検索する key のグローバルアドレス
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 検索結果のイテレータの参照
 *
 * @EN
 * @brief Map finding
 *
 * Find a key-value pair according to a key in a map.
 *
 * @param map A reference of a map type data.
 * @param key Global address of the key to search.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The item found in the map.
 * @ENDL
 */
extern acp_map_it_t acp_find_map(acp_map_t map, acp_element_t key);

/**
 * @JP
 * @brief マップ挿入
 *
 * マップにキーと変数を挿入する。
 *
 * @param map マップ型データの参照
 * @param pair 新しい要素に格納するキー、データのペア
 * @retval 1 成功
 * @retval 0 失敗
 *
 * @EN
 * @brief Map creation
 *
 * Inserts a key-value pair to a map.
 *
 * @param map A reference of a map type data.
 * @param pair
 * @retval 1 Success
 * @retval 0 Fail
 * @ENDL
 */
extern int acp_insert_map(acp_map_t map, acp_pair_t pair);

/**
 * @JP
 * @brief マップローカル併合
 *
 * 自プロセスに配置された要素を併合する。
 *
 * @param map1 併合先マップ型データの参照
 * @param map2 併合元マップ型データの参照
 *
 * @EN
 * @brief map type data local merge
 *
 * Among the keys of map2, merge the keys that are 
 * allocated in the caller process to map1.
 *
 * @param map1 A reference of destination map data.
 * @param map2 A reference of source map data.
 * @ENDL
 */
extern void acp_merge_local_map(acp_map_t map1, acp_map_t map2);

/**
 * @JP
 * @brief マップ併合
 *
 * 併合元マップの全要素を併合先マップに併合する。
 *
 * @param map1 併合先マップ型データの参照
 * @param map2 併合元マップ型データの参照
 *
 * @EN
 * @brief map type data merge
 *
 * Merge map2 to map1.
 *
 * @param map1 A reference of destination map data.
 * @param map2 A reference of source map data.
 * @ENDL
 */
extern void acp_merge_map(acp_map_t map1, acp_map_t map2);

/**
 * @JP
 * @brief セットローカル移動
 *
 * 自プロセスに配置された要素を移動する。
 *
 * @param map1 移動先マップ型データへの参照
 * @param map2 移動元マップ型データへの参照
 *
 * @EN
 * @brief Move the local keys
 *
 * Among the keys of map2, move the keys that are allocated 
 * in the caller process to map1.
 *
 * @param map1 A reference of the destination map type data
 * @param map2 A reference of the source map type data
 * @ENDL
 */
extern void acp_move_local_map(acp_map_t map1, acp_map_t map2);

/**
 * @JP
 * @brief マップ移動
 *
 * 移動元マップの全要素を移動先マップに移動する。
 *
 * @param map1 移動先マップ型データへの参照
 * @param map2 移動元マップ型データへの参照
 *
 * @EN
 * @brief Move the keys
 *
 * Move the keys of map2 to map1.
 *
 * @param map1 A reference of the destination map type data
 * @param map2 A reference of the source map type data
 * @ENDL
 */
extern void acp_move_map(acp_map_t map1, acp_map_t map2);


/**
 * @JP
 * @brief マップ除去
 *
 * マップから要素を削除する。
 *
 * @param map セット
 * @param key 削除する key
 *
 * @EN
 * @brief Erase the key of map
 *
 * Delete the key of map that matches with key.
 *
 * @param map map
 * @param key key
 * @ENDL
 */
extern void acp_remove_map(acp_map_t map, acp_element_t key);

/**
 * @JP
 * @brief マップ取得
 *
 * キーが一致する要素を取得する。
 *
 * @param map マップデータの参照
 * @param pair キーおよび取得先バッファ
 * @retval 0 一致したキーなし
 * @retval データサイズ 取得先バッファにコピーしたデータサイズ
 *
 * @EN
 * @brief Retrieve the map
 *
 * From map, retrieve the element that matches with the specified 
 * key in pair. The value of the element is copied in the second 
 * member of the pair.
 *
 * @param map A reference of the map data
 * @param pair Pair of the key and the buffer for retrieving value.
 * @retval size_t Size of the data retrieved to the buffer in the pair.
 * @retval 0 No matching key.
 * @ENDL
 */
extern size_t acp_retrieve_map(acp_map_t map, acp_pair_t pair);

/**
 * @JP
 * @brief マップローカルサイズ
 *
 * 自プロセスに配置された要素数を返す。
 *
 * @param map マップデータの参照
 * @retval size_t 自プロセスに配置され要素数
 *
 * @EN
 * @brief Query of the number ot local keys in the map
 *
 * Among the elements of map, query for the number of elements 
 * that are allocated in the caller process.
 *
 * @param map A reference of the map data
 * @retval size_t Numbers of elements
 * @ENDL
 */
extern size_t acp_size_local_map(acp_map_t map);

/**
 * @JP
 * @brief マップサイズ
 *
 * マップに格納している要素数を返す。
 *
 * @param map マップデータの参照
 * @retval size_t マップに格納している要素数
 *
 * @EN
 * @brief Query for the number of elements of map
 *
 * @param map A reference of the map data
 * @retval size_t Number of elements.
 * @ENDL
 */
extern size_t acp_size_map(acp_map_t map);

/**
 * @JP
 * @brief マップ交換
 *
 * ２つのマップの要素を交換する。
 *
 * @param map1 交換するマップの一方の参照
 * @param map2 交換するマップのもう一方の参照
 * 
 * @EN
 * @brief Swap map type data
 *
 * Swap keys between map1 and map2.
 *
 * @param map1 A reference of map data to be swapped.
 * @param map2 Another reference of map data to be swapped.
 *
 * @ENDL
 */
extern void acp_swap_map(acp_map_t map1, acp_map_t map2);

/**
 * @JP
 * @brief マップイテレータ間接参照
 *
 * マップイテレータの参照先要素のキー、データのペアを返す。
 *
 * @param it マップイテレータ
 * @retval マップイテレータの参照先要素のキー、データのペア
 *
 * @EN
 * @brief Query for the element referenced by "it".
 *
 * @param it Iterator of a map.
 * @retval The key and value pair referenced by it
 * @ENDL
 */
extern acp_pair_t acp_dereference_map_it(acp_map_it_t it);

/**
 * @JP
 * @brief マップイテレータ加算
 *
 * マップイテレータを一つ増加させる。
 *
 * @param it マップ型データのイテレータ
 * @retval "member elem == ACP_GA_NULL" 失敗
 * @retval 以外 インクリメントしたイテレータ
 *
 * @EN
 * @brief Increment an iterater of a map data
 *
 * @param it A reference of map type iterator.
 * @retval "member elem == ACP_GA_NULL" Fail
 * @retval otherwise The next iterator of the specified one.
 * @ENDL
 */
extern acp_map_it_t acp_increment_map_it(acp_map_it_t it);

#ifdef __cplusplus
}
#endif
/*@}*/ /* Map */

/*@}*/ /* Data Library */
#endif /* acp.h */
