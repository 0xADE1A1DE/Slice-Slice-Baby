#include <stdio.h>
#include <stdlib.h>

#include "../include/evsets/evsets.h"
#include "../include/slice_partitioning/evsp.h"
#include "../include/slice_partitioning/slicing.h"
#include "../include/util/util.h"

#define NUM_SAMPLES 100

int main()
{
    size_t len = LLC_SIZE * 3;
    uint8_t *mem = (uint8_t *)initialise_memory(len, 0, PAGE_SIZE);
    evset_flags evflags = 0;
    
    evflags |= L2_CANDIDATE_SET_FILTERING;
    evflags |= SLICE_FILTERING;

    cache_evsets_t *llc_evsets;

    if (IS_POWER_OF_TWO(LLC_SLICES))
    {
        llc_evsets = evsets_create(LLC, LLC_EVSET_ALGORITHM, evflags, EVSP_FLAG_BAYESIAN_PROPAGATION, LLC_CANDIDATE_SET_SIZE, 1, mem, len);
    }
    else
    {
        llc_evsets = evsets_create(LLC, LLC_EVSET_ALGORITHM, evflags, EVSP_FLAG_DECISION_TREE_PROPAGATION, LLC_CANDIDATE_SET_SIZE, 1, mem, len);
    }

    evsets_test(llc_evsets);
    evsets_release(llc_evsets);
}