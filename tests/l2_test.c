#include <stdio.h>
#include <stdlib.h>

#include "../include/evsets/evsets.h"
#include "../include/slice_partitioning/evsp.h"
#include "../include/slice_partitioning/slicing.h"
#include "../include/util/util.h"

#define NUM_SAMPLES 100

int main()
{
    size_t len = LLC_SIZE * 4;
    uint8_t *mem = (uint8_t *)initialise_memory(len, 0, PAGE_SIZE);
    cache_evsets_t *llc_evsets = evsets_create(L2, L2_EVSET_ALGORITHM, L2_EVSET_FLAGS, 0, L2_CANDIDATE_SET_SIZE, L2_REDUCTIONS, mem, len);
    evsets_test(llc_evsets);
    evsets_release(llc_evsets);
}
