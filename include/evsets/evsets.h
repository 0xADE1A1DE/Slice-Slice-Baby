#ifndef __EVSETS_H
#define __EVSETS_H

#include "../slice_partitioning/evsp.h"
#include "../util/util.h"
#include "config.h"
#include "evsets_algs.h"
#include "evsets_defs.h"
#include "evsets_helpers.h"

cache_evsets_t *evsets_create(address_state cache, evset_algorithm algorithm, evset_flags flags, evsp_flags eflags, int candidate_set_size, int reductions, void *mem, size_t mem_len);
evset_t *evset_find_for_address(cache_evsets_t *cache_evsets, void *victim);
int evsets_test(cache_evsets_t *cache_evsets);
void evsets_release(cache_evsets_t *cache_evsets);

#endif //__EVSETS_H