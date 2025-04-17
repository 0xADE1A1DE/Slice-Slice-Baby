#ifndef __EVSETS_ALGS_H
#define __EVSETS_ALGS_H

#include "../util/util.h"
#include "evsets_defs.h"
#include "evsets_helpers.h"

int prune_candidate_set(cache_evsets_t *cache_evsets, void *victim, evset_t *evset);

#endif //__EVSETS_ALGS_H