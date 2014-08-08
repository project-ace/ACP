/*****************************************************************************/
/***** Advanced Communication Primitives Library Header                  *****/
/*****                                                                   *****/
/***** Copyright FUJITSU LIMITED 2014                                    *****/
/***** Copyright Kyushu University 2014                                  *****/
/***** Copyright Institute of Systems, Information Technologies          *****/
/*****           and Nanotechnologies 2014                               *****/
/*****                                                                   *****/
/***** Specification Version: ACP-140312                                 *****/
/***** Version: 0.0                                                      *****/
/***** Module Version: 0.0                                               *****/
/*****                                                                   *****/
/***** Note:                                                             *****/
/***** This software is released under the BSD License, see LICENSE.     *****/
/*****************************************************************************/
#ifndef __ACP_H__
#define __ACP_H__

/*****************************************************************************/
/***** Basic Layer                                                       *****/
/*****************************************************************************/

/* Infrastructure */

int acp_init(int*, char***);
int acp_finalize(void);
int acp_reset(int);
void acp_abort(const char*);
int acp_sync(void);
int acp_rank(void);
int acp_procs(void);

/* Global memory management */

#define ACP_ATKEY_NULL  0LLU
#define ACP_GA_NULL     0LLU

typedef uint64_t acp_atkey_t;
typedef uint64_t acp_ga_t;

acp_ga_t acp_query_starter_ga(int);
acp_atkey_t acp_register_memory(void*, size_t, int);
int acp_unregister_memory(acp_atkey_t);
acp_ga_t acp_query_ga(acp_atkey_t, void*);
void* acp_query_address(acp_ga_t);
int acp_query_rank(acp_ga_t);
int acp_query_color(acp_ga_t);
int acp_colors(void);

/* Global memory access */

/* #ifdef ACPBL_TOFU
#define ACP_HANDLE_ALL  0xfffffffffffffffdLLU
#define ACP_HANDLE_CONT 0xfffffffffffffffeLLU
#define ACP_HANDLE_NULL 0xffffffffffffffffLLU
#else */
#define ACP_HANDLE_ALL  0xffffffffffffffffLLU
#define ACP_HANDLE_CONT 0xfffffffffffffffeLLU
#define ACP_HANDLE_NULL 0x0000000000000000LLU

typedef int64_t acp_handle_t;

acp_handle_t acp_copy(acp_ga_t, acp_ga_t, size_t, acp_handle_t);
acp_handle_t acp_cas4(acp_ga_t, acp_ga_t, uint32_t, uint32_t, acp_handle_t);
acp_handle_t acp_cas8(acp_ga_t, acp_ga_t, uint64_t, uint64_t, acp_handle_t);
acp_handle_t acp_swap4(acp_ga_t, acp_ga_t, uint32_t, acp_handle_t);
acp_handle_t acp_swap8(acp_ga_t, acp_ga_t, uint64_t, acp_handle_t);
acp_handle_t acp_add4(acp_ga_t, acp_ga_t, uint32_t, acp_handle_t);
acp_handle_t acp_add8(acp_ga_t, acp_ga_t, uint64_t, acp_handle_t);
acp_handle_t acp_xor4(acp_ga_t, acp_ga_t, uint32_t, acp_handle_t);
acp_handle_t acp_xor8(acp_ga_t, acp_ga_t, uint64_t, acp_handle_t);
acp_handle_t acp_or4(acp_ga_t, acp_ga_t, uint32_t, acp_handle_t);
acp_handle_t acp_or8(acp_ga_t, acp_ga_t, uint64_t, acp_handle_t);
acp_handle_t acp_and4(acp_ga_t, acp_ga_t, uint32_t, acp_handle_t);
acp_handle_t acp_and8(acp_ga_t, acp_ga_t, uint64_t, acp_handle_t);
void acp_complete(acp_handle_t);
int acp_inquire(acp_handle_t);

/*****************************************************************************/
/***** Communication Library                                             *****/
/*****************************************************************************/


/*****************************************************************************/
/***** Data Library                                                      *****/
/*****************************************************************************/

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

#define acp_vector_t acp_ga_t
#define acp_vector_it_t int

acp_vector_t acp_create_vector(size_t, size_t, int);
void acp_destroy_vector(acp_vector_t);
acp_vector_t acp_duplicate_vector(acp_vector_t, int);
void acp_swap_vector(acp_vector_t, acp_vector_t);
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
acp_vector_it_t acp_increment_vector(acp_vector_it_t*);
acp_vector_it_t acp_decrement_vector(acp_vector_it_t*);
int acp_max_size_vector(acp_vector_t);
int acp_empty_vector(acp_vector_t);
int acp_equal_vector(acp_vector_t, acp_vector_t);
int acp_not_equal_vector(acp_vector_t, acp_vector_t);
int acp_less_vector(acp_vector_t, acp_vector_t);
int acp_greater_vector(acp_vector_t, acp_vector_t);
int acp_less_or_equal_vector(acp_vector_t, acp_vector_t);
int acp_greater_or_equal_vector(acp_vector_t, acp_vector_t);

/* List */

#define acp_list_t acp_ga_t
#define acp_list_it_t acp_ga_t

acp_list_t acp_create_list(size_t, int);
void acp_destroy_list(acp_list_t);
acp_list_it_t acp_insert_list(acp_list_t, acp_list_it_t, void*, int);
acp_list_it_t acp_erase_list(acp_list_t, acp_list_it_t);
void acp_push_back_list(acp_list_t, void*, int);
acp_list_it_t acp_begin_list(acp_list_t);
acp_list_it_t acp_end_list(acp_list_t);
void acp_increment_list(acp_list_it_t*);
void acp_decrement_list(acp_list_it_t*);

/* Deque */

#define acp_deque_t acp_ga_t
#define acp_deque_it_t int

/* Set */

#define acp_set_t acp_ga_t
#define acp_set_it_t acp_ga_t

/* Map */

#define acp_map_t acp_ga_t
#define acp_map_it_t acp_ga_t

#endif /* acp.h */
