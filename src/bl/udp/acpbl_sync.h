/*
 * ACP Basic Layer Sync Operations
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
#ifndef __ACPBL_SYNC_H__
#define __ACPBL_SYNC_H__

#ifdef FCC_SPARC64
static inline uint32_t sync_val_compare_and_swap_4(volatile uint32_t* ptr, uint32_t oldval, uint32_t newval)
{
    volatile uint32_t tmpr;
    __asm__ __volatile__("membar #StoreLoad | #LoadLoad");
    __asm__ __volatile__("cas [%1], %2, %3\n\tst %3, [%0]" : : "r"(&tmpr), "r"(ptr), "r"(oldval), "r"(newval) : "memory");
    __asm__ __volatile__("membar #StoreStore | #StoreLoad");
    return tmpr;
}

static inline uint64_t sync_val_compare_and_swap_8(volatile uint64_t* ptr, uint64_t oldval, uint64_t newval)
{
    volatile uint64_t tmpr;
    __asm__ __volatile__("membar #StoreLoad | #LoadLoad");
    __asm__ __volatile__("casx [%1], %2, %3\n\tstx %3, [%0]" : : "r"(&tmpr), "r"(ptr), "r"(oldval), "r"(newval) : "memory");
    __asm__ __volatile__("membar #StoreStore | #StoreLoad");
    return tmpr;
}

static inline uint32_t sync_swap_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_swap_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, value);
    } while (oldval != readval);
    return oldval;
}

static inline uint32_t sync_fetch_and_add_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, oldval + value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_fetch_and_add_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, oldval + value);
    } while (oldval != readval);
    return oldval;
}

static inline uint32_t sync_fetch_and_or_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, oldval | value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_fetch_and_or_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, oldval | value);
    } while (oldval != readval);
    return oldval;
}

static inline uint32_t sync_fetch_and_and_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, oldval & value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_fetch_and_and_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, oldval & value);
    } while (oldval != readval);
    return oldval;
}

static inline uint32_t sync_fetch_and_xor_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, oldval ^ value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_fetch_and_xor_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, oldval ^ value);
    } while (oldval != readval);
    return oldval;
}

static inline void sync_synchronize(void)
{
    __asm__ __volatile__("membar #StoreStore | #LoadStore | #StoreLoad | #LoadLoad" : : : "memory");
    return;
}

static inline uint64_t get_clock(void)
{
    uint64_t ret;
    __asm__ __volatile__(
        "rd  %%tick, %0\n\t"
        "mov %0, %0\t"
        : "=r" (ret)
    );
    return ret & ~((uint64_t)1 << 63);
}

#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)
#define sync_val_compare_and_swap_4(ptr, oldval, newval) __sync_val_compare_and_swap(ptr, oldval, newval)
#define sync_val_compare_and_swap_8(ptr, oldval, newval) __sync_val_compare_and_swap(ptr, oldval, newval)

static inline uint32_t sync_swap_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_swap_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, value);
    } while (oldval != readval);
    return oldval;
}

#define sync_fetch_and_add_4(ptr, value)                 __sync_fetch_and_add(ptr, value)
#define sync_fetch_and_add_8(ptr, value)                 __sync_fetch_and_add(ptr, value)
#define sync_fetch_and_or_4(ptr, value)                  __sync_fetch_and_or(ptr, value)
#define sync_fetch_and_or_8(ptr, value)                  __sync_fetch_and_or(ptr, value)
#define sync_fetch_and_and_4(ptr, value)                 __sync_fetch_and_and(ptr, value)
#define sync_fetch_and_and_8(ptr, value)                 __sync_fetch_and_and(ptr, value)
#define sync_fetch_and_xor_4(ptr, value)                 __sync_fetch_and_xor(ptr, value)
#define sync_fetch_and_xor_8(ptr, value)                 __sync_fetch_and_xor(ptr, value)
#define sync_synchronize()                               __sync_synchronize()

#ifdef __x86_64__
static inline uint64_t get_clock(void)
{
    uint64_t a, d;
    __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
    return (d << 32) | a;
}
#else
static inline uint64_t get_clock(void)
{
    struct timespec now;
#ifdef CLOCK_MONOTONIC_RAW
    clock_gettime(CLOCK_MONOTONIC_RAW, &now);
#elif defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &now);
#else
    clock_gettime(1, &now);
#endif
    return (uint64_t)((double)now.tv_sec * 1000000000.0) + (uint64_t)now.tv_nsec;
}
#endif /* __x86_64__ */

#elif defined(__GNUC__) && defined(__x86_64__)
static inline uint32_t sync_val_compare_and_swap_4(volatile uint32_t* ptr, uint32_t oldval, uint32_t newval)
{
    __asm__ __volatile__("lock\n cmpxchgl %2, %1" : "=a"(oldval), "+m"(*ptr) : "q"(newval), "0"(oldval) : "cc");
    return oldval;
}

static inline uint64_t sync_val_compare_and_swap_8(volatile uint64_t* ptr, uint64_t oldval, uint64_t newval)
{
    __asm__ __volatile__("lock\n cmpxchgq %2, %1" : "=a"(oldval), "+m"(*ptr) : "q"(newval), "0"(oldval) : "cc");
    return oldval;
}

static inline uint32_t sync_swap_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_swap_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, value);
    } while (oldval != readval);
    return oldval;
}

static inline uint32_t sync_fetch_and_add_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, oldval + value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_fetch_and_add_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, oldval + value);
    } while (oldval != readval);
    return oldval;
}

static inline uint32_t sync_fetch_and_or_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, oldval | value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_fetch_and_or_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, oldval | value);
    } while (oldval != readval);
    return oldval;
}

static inline uint32_t sync_fetch_and_and_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, oldval & value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_fetch_and_and_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, oldval & value);
    } while (oldval != readval);
    return oldval;
}

static inline uint32_t sync_fetch_and_xor_4(volatile uint32_t* ptr, uint32_t value)
{
    uint32_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_4(ptr, oldval, oldval ^ value);
    } while (oldval != readval);
    return oldval;
}

static inline uint64_t sync_fetch_and_xor_8(volatile uint64_t* ptr, uint64_t value)
{
    uint64_t oldval, readval;
    do {
        oldval = *ptr;
        readval = sync_val_compare_and_swap_8(ptr, oldval, oldval ^ value);
    } while (oldval != readval);
    return oldval;
}

static inline void sync_synchronize(void)
{
    __asm__ __volatile__("mfence" : : : "memory");
    return;
}

static inline uint64_t get_clock(void)
{
    uint64_t a, d;
    __asm__ volatile ("rdtsc" : "=a" (a), "=d" (d));
    return (d << 32) | a;
}

#endif
#endif /* acpbl_sync.h */
