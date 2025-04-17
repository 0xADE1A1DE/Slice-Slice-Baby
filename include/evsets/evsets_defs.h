#ifndef __EVSETS_DEFS_H
#define __EVSETS_DEFS_H

#include "../util/util.h"

#ifndef TEST_EVICTIONS
#define TEST_EVICTIONS 3
#endif

#define LLC_DEBUG 0

typedef enum
{
    L2_CHEAT,
    LLC_CHEAT,
    GROUP_TESTING_NEW,
    GROUP_TESTING_OPTIMISED_NEW,
    BINARY_SEARCH_ORIGINAL,
    BINARY_SEARCH_BRADM,
} evset_algorithm;

typedef enum
{
    L2_CANDIDATE_SET_FILTERING = (1 << 0),
    PERFECT_L2_CANDIDATE_SET_FILTERING = (1 << 1),
    SLICE_FILTERING = (1 << 2),
    PERFECT_SLICE_FILTERING = (1 << 3),
    FULL_SYSTEM = (1 << 4) // For LLC only
} evset_flags;

// Taken generously from https://github.com/cgvwzq/evsets
// I use the set variable to indicate whether this element is in use.
typedef struct elem_s
{
    struct elem_s *next;
    struct elem_s *prev;
    int8_t set;
    int8_t slice;
    int8_t cslice;
    uint32_t access_time;
    struct evset_s *l1_evset;
    struct evset_s *l2_evset;
    uint32_t l2_evset_index;
    struct evset_s *llc_evset;
    uint8_t pad[8]; // up to 64B
} elem_t;

typedef struct evset_s
{
    elem_t **cs;
    size_t size;
    elem_t *victim;
    int mirror;
} evset_t;

typedef struct cache_evsets_s
{
    evset_t **evsets;

    int *evsets_count;

    void *evict_mem;
    size_t evict_mem_len;

    address_state cache; // which cache we are making an eviction set for
    evset_algorithm algorithm;
    int flags;
    size_t candidate_set_size;
    int reductions;

    int evsets_per_offset; // number of eviction sets per L1 offset as per the desired cache.
    size_t page_offset_limit;

    size_t target_size;

    uint32_t measure_low;
    uint32_t measure_high;
} cache_evsets_t;

#endif //__EVSETS_DEFS_H