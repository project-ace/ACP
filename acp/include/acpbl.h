/** \file acpbl.h
 * \ingroup acpbl
 *  @brief A header file for ACP BL.
 *         
 *  This is the ACP BL header file.
 */

#ifndef __ACPBL_H__
#define __ACPBL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* XXX acp_size_t or size_t? : size_t [ace-yt3 51]*/
/* typedef uint64_t acp_size_t; */
//#define acp_size_t size_t;

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
 * @param argc A pointer for the number of arguments of the main function
 * @param argv A pointer for the array of arguments of the main function
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
 * acp_reset関数は内部で各MLモジュールの終了処理関数と初期化関数を呼び出す。a
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
//int acp_reset(int rank, size_t size); [ace-yt3 51]
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
 * @param str additional error message
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
//void acp_sync(void);
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
 * @retval >0 Rank number of the process
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
 * @retval >1 Number of processes
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_procs(void);

#ifdef __cplusplus
}
#endif

extern int acp_errno;

/** \ingroup acpbl
 * \name Global Segment Management
 */
/*@{*/
#define ACP_ATKEY_NULL	0LLU  /*!< Represents that no address 
				translation key is available. */
#define ACP_GA_NULL	0LLU  /*!< Null address of the global memory. */
  /*#if defined (ACPBL_UDP)
    # define ACP_ATKEY_NULL   0LLU
    # define ACP_GA_NULL      0LLU
    #elif defined (ACPBL_IB)
    # define ACP_ATKEY_NULL   0xffffffffffffffffLLU 
    # define ACP_GA_NULL      0xffffffffffffffffLLU 
    #else
    # define ACP_ATKEY_NULL   0LLU
    # define ACP_GA_NULL      0LLU
    #endif
*/

typedef uint64_t acp_atkey_t;	/*!< Address translation key. 
				  An attribute to translate between a 
				  logical address and a global address. */
typedef uint64_t acp_ga_t;	/*!< Global address. Commonly used among 
				  processes for byte-wise addressing 
				  of the global memory. */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @JP
 * @brief スターターアドレス取得関数
 *
 * ランク番号を指定して、スターターメモリの先頭グローバルアドレスを取得する関数。
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
 * @retval atkey Address translation key
 * @retval ACP_ATKEY_NULL Fail
 * @ENDL
 */
extern acp_atkey_t acp_register_memory(void* addr, size_t size, int color);

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
 * @param atkey Address translation key
 * @retval 0 Success
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_unregister_memory(acp_atkey_t atkey);

/**
 * @JP
 * @brief グローバルアドレス取得関数
 *
 * 変換キーと論理アドレスを指定し、論理アドレスをグローバルアドレスに変換する関数。
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
 * @param atkey Address translation key
 * @param addr Logical address
 * 
 * @retval ga Global address of starter memory
 * @retval ACP_GA_NULL Fail
 * @ENDL
 */
extern acp_ga_t acp_query_ga(acp_atkey_t atkey, void* addr);

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
 * @param ga Global address
 * @retval address Logical address
 * @retval NULL Fail
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
 * @retval >0 ランク番号
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
 * @param ga Global address
 * 
 * @retval >0 Rank number
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
 * @retval >0 カラー番号
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
 * @retval >0 Color number
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
 * @retval >1 最大カラー数
 * @retval -1 失敗
 *
 * @EN
 * @brief Query for the maximum number of colors
 *
 * Returns the maximum number of colors on this environment.
 * 
 * @retval >1 Maximum number of colors
 * @retval -1 Fail
 * @ENDL
 */
extern int acp_colors(void);
/*@}*/ /* Global Segment Management */
 
#ifdef __cplusplus
}
#endif

/** \ingroup acpbl
 * \name Global Memory Access
 */
/*@{*/
# define ACP_HANDLE_ALL  0xffffffffffffffffLLU  /*!< Represents all of 
						  the handles of GMAs 
						  that have been invoked 
						  so far. */
# define ACP_HANDLE_NULL 0x0000000000000000LLU  /*!< Represents that no 
						  handle is available. */
/* #if defined (ACPBL_UDP)
# define ACP_HANDLE_ALL  0xffffffffffffffffLLU
# define ACP_HANDLE_NULL 0x0000000000000000LLU
#elif defined (ACPBL_IB)
# define ACP_HANDLE_ALL  0xffffffffffffffffLLU
# define ACP_HANDLE_NULL 0x0000000000000000LLU
#else
# define ACP_HANDLE_ALL  0xffffffffffffffffLLU
# define ACP_HANDLE_NULL 0x0000000000000000LLU
#endif
*/

typedef uint64_t acp_handle_t; /*!< Handle of GMA. 
				   Used as identifiers of the invoked GMAs. */
typedef struct {
  int dummy;
} acp_errstat_t;

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_copy(acp_ga_t dst, acp_ga_t src, size_t size, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA
 * @retval ACP_HANDLE_NULL Fail
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
 * @retval acp_handle A handle for this GMA
 * @retval ACP_HANDLE_NULL Fail
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
 * @param src Global address to apply the operation
 * @param value Value to be swapped
 * @param order The handle to be used as a condition for starting this GMA
 * @retval acp_handle A handle for this GMA GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_swap4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order);

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
 * @param src Global address to apply the operation
 * @param value Value to be swapped
 * @param order The handle to be used as a condition for starting this GMA
 * @retval acp_handle A handle for this GMA
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_swap8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_add4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_add8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_xor4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_xor8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_or4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_or8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_and4(acp_ga_t dst, acp_ga_t src, uint32_t value, acp_handle_t order);

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
 * @retval acp_handle A handle for this GMA.
 * @retval ACP_HANDLE_NULL Fail
 * @ENDL
 */
extern acp_handle_t acp_and8(acp_ga_t dst, acp_ga_t src, uint64_t value, acp_handle_t order);

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
 * @param Handle of a GMA to be waited for the completion.
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
/*@}*/

#ifdef  __cplusplus
}
#endif

#endif /* acpbl.h */
