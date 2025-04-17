#ifndef __EVSETS_HELPERS_H
#define __EVSETS_HELPERS_H

#include "../slice_partitioning/slicing.h"
#include "../util/util.h"
#include "evsets_defs.h"

inline void traverse_array(elem_t **head, size_t len, int reps)
{
    // Repeat the array access
    for (int r = 0; r < reps; r++)
    {
        // Backwards access to stop speculative overshoot
        for (size_t i = len - 1; i != 0; i--)
        {
            memaccess((void *)(head[i]));
        }
        if (r < reps - 1)
        {
            memory_fences();
        }
    }
}

inline void traverse_array_alt(elem_t **head, size_t len, int reps)
{
    // Repeat the array access
    for (int r = reps - 1; r >= 0; r--)
    {
        // Backwards access to stop speculative overshoot
        for (size_t i = len - 1; i != 0; i--)
        {
            memaccess((void *)(head[i]));
        }
        if (r >= 1)
        {
            serializing_cpuid();
        }
    }
}

// Thanks https://github.com/cgvwzq/evsets
inline void traverse_array_skylake(elem_t **head, size_t len, int reps)
{
    if (len == 1)
    {
        for (int r = 0; r < reps; r++)
        {
            memaccess(head[0]);
            memaccess(head[0]);
        }
    }
    else if (len == 2)
    {
        for (int r = 0; r < reps; r++)
        {
            memaccess(head[0]);
            memaccess(head[1]);
            memaccess(head[0]);
            memaccess(head[1]);
        }
    }
    else
    {
        for (int r = 0; r < reps; r++)
        {
            for (size_t i = 0; i < len - 2; i++)
            {
                memaccess(head[i]);
                memaccess(head[i + 1]);
                memaccess(head[i + 2]);
                memaccess(head[i]);
                memaccess(head[i + 1]);
                memaccess(head[i + 2]);
            }
        }
    }
}

inline void traverse_array_with_l2_occupy_set(elem_t **head, size_t len, int reps, evset_t *l2_evset)
{
    // Repeat the array access
    for (int r = 0; r < reps; r++)
    {
        // Backwards access to stop speculative overshoot
        for (size_t i = len - 1; i != 0; i--)
        {
            memaccess((void *)(head[i]));
        }
        if (r < reps)
        {
            // lfence();
            // if (len < 16)
            {
                L2_TRAVERSE(((evset_t *)l2_evset)->cs, ((evset_t *)l2_evset)->size, L2_TRAVERSE_REPEATS);
                lfence();
            }
        }
    }
}

#if IS_INCLUSIVE == 0
#define PRIME_CANDS_REPEAT 10
#else
#define PRIME_CANDS_REPEAT 1
#endif

#define PRIME_CANDS_STRIDE 12
#define PRIME_CANDS_BLOCK 24

inline void access_array_bwd(elem_t **addrs, size_t size)
{
    for (size_t i = size; i > 0; i--)
    {
        memaccess(addrs[i - 1]);
    }
}

// eviction pattern from Daniel Gruss
// Rowhammer.js: A Remote Software-Induced Fault Attack in JavaScript
inline void prime_cands_daniel(elem_t **cands, size_t len, size_t repeat, size_t stride, size_t block)
{
    block = _min(block, len);
    for (size_t s = 0; s < len; s += stride)
    {
        for (size_t c = 0; c < repeat; c++)
        {
            if (len >= block + s)
            {
                access_array_bwd(&cands[s], block);
            }
            else
            {
                uint32_t rem = len - s;
                access_array_bwd(&cands[s], rem);
                access_array_bwd(cands, block - rem);
            }
        }
    }
}

inline void skx_sf_cands_traverse_st(elem_t **cands, size_t len, void *l2_evset)
{
    for (size_t r = 0; r < 2; r++)
    {
        // access_array(cands, len);
        prime_cands_daniel(cands, len, PRIME_CANDS_REPEAT, PRIME_CANDS_STRIDE, PRIME_CANDS_BLOCK);
        lfence();
        // if (len < 16)
        {
            L2_TRAVERSE(((evset_t *)l2_evset)->cs, ((evset_t *)l2_evset)->size, L2_TRAVERSE_REPEATS);
            lfence();
        }
    }
}

// //From https://github.com/KULeuven-COSIC/PRIME-SCOPE adapted to array access
inline void traverse_array_non_inclusive(elem_t **head, size_t len, int reps)
{
    for (int r = 0; r < reps; ++r)
    {
        for (int64_t i = 0; i < (int64_t)len - 1; i++)
        {
            asm volatile("movq (%0), %%rax;"
                         "movq (%1), %%rcx;"
                         "mfence;"
                         :
                         : "r"(head[i]), "r"(head[i + 1])
                         : "rax", "rcx", "cc", "memory");
        }
    }
}

// Thanks https://github.com/cgvwzq/evsets
inline void traverse_array_rrip(elem_t **head, size_t len, int reps)
{
    for (int r = 0; r < reps; r++)
    {
        size_t i = 0, j = 0;

        // First traversal: forward
        while (i < len)
        {
            memaccess(head[i]);
            memaccess(head[i]);
            memaccess(head[i]);
            memaccess(head[i]);
            i++;
        }

        // Second traversal: backward
        while (j < len - 1)
        {
            memaccess(head[len - 1 - j]);
            memaccess(head[len - 1 - j]);
            j++;
        }

        // Access the last element twice
        memaccess(head[len - 1 - j]);
        memaccess(head[len - 1 - j]);
    }
}

inline void traverse_array_time(elem_t **array, size_t len, int reps)
{
    uint32_t time;
    traverse_array(array, len, reps);
    for (int64_t i = len - 1; i >= 0; i--)
    {
        time = memaccesstime(array[i]);
        array[i]->access_time = time;
    }
}

inline uint64_t traverse_list_total_time(elem_t **array, size_t len)
{
    uint64_t time = rdtscp64();
    traverse_array(array, len, 1);
    memory_fences();
    time = rdtscp64() - time;
    return time;
}

inline void flush_array(elem_t **head, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        clflush(head[i]);
    }
}

uint32_t test_victim_eviction_L2(void *victim, elem_t **cs, size_t len);
uint32_t test_victim_eviction_LLC(void *victim, elem_t **cs, size_t len);
int test_victim_eviction_avg(cache_evsets_t *cache_evsets, void *victim, elem_t **cs, size_t len);

void clear_array(evset_t *evset);

int get_num_l2_same_set(void *victim, evset_t *evset);
int get_num_l2_same_slice_and_set(void *victim, evset_t *evset);
int get_num_l2_same_slice_and_set_user(void *victim, evset_t *evset);
int get_num_llc_same_slice(void *victim, evset_t *evset);
int get_num_llc_same_set(void *victim, evset_t *evset);
int get_num_llc_same_slice_and_set(void *victim, evset_t *evset);

void print_evset_info(void *victim, evset_t *evset);
void print_evset(evset_t *evset);
void print_evset_mirror(evset_t *evset, int page_offset);

char *evset_alg_to_str(evset_algorithm alg);

/////////////////////////////////////////////////////////////////////////// Import Area for https://github.com/zzrcxb/LLCFeasible ///////////////////////////////////////////////////////////////////////////
#define UNUSED __attribute__((unused))

#ifdef __clang__
#define OPTNONE __attribute__((optnone))
#else
#define OPTNONE __attribute__((optimize("O0")))
#endif

#define ALWAYS_INLINE inline __attribute__((__always_inline__))

#ifdef __KERNEL__
#define ALWAYS_INLINE_HEADER static ALWAYS_INLINE
#else
#define ALWAYS_INLINE_HEADER ALWAYS_INLINE
#endif

#define _SET_BIT(data, bit) ((data) | (1ull << (bit)))
#define _CLEAR_BIT(data, bit) ((data) & ~(1ull << (bit)))
#define _TOGGLE_BIT(data, bit) ((data) ^ (1ull << (bit)))
#define _WRITE_BIT(data, bit, val) (((data) & (~(1ull << (bit)))) | ((!!(val)) << (bit)))
#define _TEST_BIT(data, bit) (!!((data) & (1ull << (bit))))
#define _SEL_NOSPEC(MASK, T, F) (((MASK) & (typeof((MASK)))(T)) | (~(MASK) & (typeof((MASK)))(F)))

#define _SHIFT_MASK(shift) ((1ull << shift) - 1)
#define _ALIGNED(data, shift) (!((uint64_t)(data) & _SHIFT_MASK(shift)))
#define __ALIGN_UP(data, shift) ((((uint64_t)(data) >> (shift)) + 1) << (shift))
#define _ALIGN_UP(data, shift) ((typeof(data))(_ALIGNED(data, shift) ? (uint64_t)(data) : __ALIGN_UP(data, shift)))
#define _ALIGN_DOWN(data, shift) ((typeof(data))((uint64_t)(data) & (~_SHIFT_MASK(shift))))

#define PAGE_SHIFT (12u)
#define PAGE_MASK (PAGE_SIZE - 1)

static inline uintptr_t page_offset(void *ptr)
{
    return (uintptr_t)ptr & PAGE_MASK;
}

static inline elem_t *tlb_warmup_ptr(elem_t *ptr)
{
    elem_t *page = _ALIGN_DOWN(ptr, PAGE_SHIFT);
    uint32_t target_offset = page_offset(ptr);
    uint32_t tlb_offset = (target_offset + PAGE_SIZE / 2) % PAGE_SIZE;
    return page + tlb_offset;
}

static inline uint64_t _rdtscp_aux(uint32_t *aux)
{
    uint64_t rax;
    asm volatile("rdtscp\n\t"
                 "shl $32, %%rdx\n\t"
                 "or %%rdx, %0\n\t"
                 "mov %%ecx, %1\n\t"
                 : "=a"(rax), "=r"(*aux)::"rcx", "rdx", "memory", "cc");
    return rax;
}

// https://github.com/google/highwayhash/blob/master/highwayhash/tsc_timer.h
static inline uint64_t _rdtsc_google_begin(void)
{
    uint64_t t;
    asm volatile("mfence\n\t"
                 "lfence\n\t"
                 "rdtsc\n\t"
                 "shl $32, %%rdx\n\t"
                 "or %%rdx, %0\n\t"
                 "lfence"
                 : "=a"(t)
                 :
                 // "memory" avoids reordering. rdx = TSC >> 32.
                 // "cc" = flags modified by SHL.
                 : "rdx", "memory", "cc");
    return t;
}

static inline uint64_t _rdtscp_google_end(void)
{
    uint64_t t;
    asm volatile("rdtscp\n\t"
                 "shl $32, %%rdx\n\t"
                 "or %%rdx, %0\n\t"
                 "lfence"
                 : "=a"(t)
                 :
                 // "memory" avoids reordering.
                 // rcx = TSC_AUX. rdx = TSC >> 32.
                 // "cc" = flags modified by SHL.
                 : "rcx", "rdx", "memory", "cc");
    return t;
}

static inline uint64_t _rdtscp_google_end_aux(uint32_t *aux)
{
    uint64_t t;
    asm volatile("rdtscp\n\t"
                 "shl $32, %%rdx\n\t"
                 "or %%rdx, %0\n\t"
                 "lfence"
                 : "=a"(t), "=c"(*aux)
                 :
                 // "memory" avoids reordering.
                 // rcx = TSC_AUX. rdx = TSC >> 32.
                 // "cc" = flags modified by SHL.
                 : "rdx", "memory", "cc");
    return t;
}

// use google's method by default
#define _timer_start _rdtsc_google_begin
#define _timer_end _rdtscp_google_end
#define _timer_end_aux _rdtscp_google_end_aux

static inline uint64_t _timer_warmup(void)
{
    uint64_t lat = _timer_start();
    lat = _timer_end() - lat;
    return lat;
}

#define _force_addr_calc(PTR)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
        asm volatile("mov %0, %0\n\t" : "+r"(PTR)::"memory");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    } while (0)

#define _time_p_action(P, ACTION)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
    ({                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
        typeof((P)) __ptr = (P);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
        uint64_t __tsc;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        /* make sure that address computation is done before _timer_start */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
        _force_addr_calc(__ptr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
        __tsc = _timer_start();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
        ACTION(__ptr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
        _timer_end() - __tsc;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    })

#define _time_p_action_aux(P, ACTION, end_tsc, end_aux)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    ({                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
        typeof((P)) __ptr = (P);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
        uint64_t __tsc;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
        /* make sure that address computation is done before _timer_start */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
        _force_addr_calc(__ptr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
        __tsc = _timer_start();                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
        ACTION(__ptr);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
        (end_tsc) = _timer_end_aux(&(end_aux));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
        (end_tsc) - __tsc;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    })

#define _time_maccess(P) _time_p_action(P, memaccess)
#define _time_icall(P) _time_p_action(P, _icall)
#define _time_maccess_aux(P, end_tsc, end_aux) _time_p_action_aux(P, memaccess, end_tsc, end_aux)

#endif //__EVSETS_HELPERS_H