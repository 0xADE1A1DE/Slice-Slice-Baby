#ifndef __UTIL_H__
#define __UTIL_H__

#include <assert.h>
#include <fcntl.h>
#include <linux/mman.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <sys/time.h>
#include <time.h>

#include "../config.h"

#define IS_POWER_OF_TWO(x) (__builtin_popcountll(x) == 1)

#define KB (1024UL)
#define MB (1024UL * KB)
#define GB (1024UL * MB)

#define CACHELINE (64UL)
#define CACHELINE_BITS (6UL)
#define L1D_SETS ((L1D_SIZE / CACHELINE) / L1D_ASSOCIATIVITY)
#define L1D_STRIDE (L1_CACHELINE * L1D_SETS)

#define L2_SETS ((L2_SIZE / CACHELINE) / L2_ASSOCIATIVITY)
#define L2_STRIDE (L2_CACHELINE * L2_SETS)

#define LLC_SETS (((LLC_SIZE / LLC_SLICES) / CACHELINE) / LLC_ASSOCIATIVITY)
#define LLC_STRIDE (LLC_CACHELINE * LLC_SETS) // This doesn't really work due to slicing

#define SF_SETS 2048 // From experimentation

#define SMALLPAGE (4UL * KB)
#define HUGEPAGE (2UL * MB)

#define GET_L1D_SET_BITS(addr) ((((uintptr_t)(addr)) >> CACHELINE_BITS) & (L1D_SETS - 1))
#define GET_L2_SET_BITS(addr) ((((uintptr_t)(addr)) >> CACHELINE_BITS) & (L2_SETS - 1))
#define GET_LLC_SET_BITS(addr) ((((uintptr_t)(addr)) >> CACHELINE_BITS) & (LLC_SETS - 1))

#define SMALLPAGE_BITS (12)
#define HUGEPAGE_BITS (21)

#if PAGE_SIZE == SMALLPAGE
#define PAGE_BITS SMALLPAGE_BITS
#elif PAGE_SIZE == HUGEPAGE
#define PAGE_BITS HUGEPAGE_BITS
#endif

#if IS_INCLUSIVE == 1
#define MAX_EVSET_PER_SET ((int)(LLC_SETS / L2_SETS))
#else
#define MAX_EVSET_PER_SET ((int)(SF_SETS / L2_SETS))
#endif

#define INDEX_OF_SET_BIT(x) (__builtin_ctz(x))

typedef enum
{
    L1,
    L2,
    LLC,
    SF,
    RAM,
    RAM_SMALLPAGE,
} address_state;

char *get_address_state_string(address_state state);

#define _swap(X, Y)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
        typeof(X) _tmp = (X);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
        (X) = (Y);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
        (Y) = _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    } while (0);

#define _min(x, y) ({ (x) > (y) ? (y) : (x); })
#define _max(x, y) ({ (x) > (y) ? (x) : (y); })

inline uint64_t rdtscp64()
{
    uint32_t low, high;
    asm volatile("rdtscp" : "=a"(low), "=d"(high)::"ecx");
    return (((uint64_t)high) << 32) | low;
}

inline void delayloop(uint64_t cycles)
{
    uint64_t start = rdtscp64();
    while ((rdtscp64() - start) < cycles)
        ;
}

inline void memory_fences()
{
    asm volatile("mfence\nlfence\n");
}

inline void lfence(void)
{
    asm volatile("lfence\n");
}

inline void mfence(void)
{
    asm volatile("mfence\n");
}

inline void prevent_reorder()
{
    asm volatile("NOP" :::);
}

inline int memaccess(void *v)
{
    int rv = 0;
    asm volatile("mov (%1), %0" : "+r"(rv) : "r"(v) :);
    return rv;
}

inline void clflush(void *p)
{
    asm volatile("clflush 0(%0)" : : "c"(p) : "rax");
}

inline void serializing_cpuid()
{
    asm volatile("xor %%eax, %%eax\n"
                 "cpuid"
                 :
                 :
                 : "eax", "ebx", "ecx", "edx");
}

inline uint64_t memaccesstime_cpuid(void *v)
{
    uint32_t rv = 0;
    serializing_cpuid();
    asm volatile("rdtscp\n"
                 "mov %%eax, %%esi\n"
                 "mov (%1), %%eax\n"
                 "rdtscp\n"
                 "sub %%esi, %%eax\n"
                 : "=&a"(rv)
                 : "r"(v)
                 : "ecx", "edx", "esi");
    return (uint64_t)rv;
}

inline uint64_t memaccesstime(void *v)
{
    uint32_t rv = 0;
    asm volatile("mfence\n"
                 "lfence\n"
                 "rdtscp\n"
                 "mov %%eax, %%esi\n"
                 "mov (%1), %%eax\n"
                 "rdtscp\n"
                 "sub %%esi, %%eax\n"
                 : "=&a"(rv)
                 : "r"(v)
                 : "ecx", "edx", "esi");
    return (uint64_t)rv;
}

char *get_address_state_string(address_state state);

int compare(const void *a, const void *b);
int compare_double(const void *a, const void *b);

double calculate_mean(uint32_t data[], uint32_t len);
double calculate_stddev(uint32_t data[], uint32_t len, double mean);
double calculate_absolute_deviation(uint32_t data[], int len, double mean);
double calculate_zscore(double datum, double mean, double stddev);

void print_addr_info(uintptr_t address);

void *initialise_memory(size_t size, size_t extra_flags, size_t page_size);

void reset_set_addr_state_l2_evset();
void print_access_times(address_state to_state);
void set_addr_state(uintptr_t address, address_state to_state);
uint64_t virtual_to_physical(uint64_t vaddr);

struct timeval start_timer();
double stop_timer(struct timeval timer);

#endif //__UTIL_H__
