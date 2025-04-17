#ifndef __CONFIG_H__
#define __CONFIG_H__

#define INTEL_XEON_GOLD_6130
#define IS_INCLUSIVE 0
#define LLC_SLICES 16

#define VERBOSITY 1

#define MEASURE_SAMPLES 10

#define CG_MEASUREMENT 1 // 0 for RTDSCP, 1 for CG (Comparator gate)
#define CG_GROUND_TRUTH_CALIBRATE 1

#define CG_BALANCER_DELAY 1
#define CG_SIGNAL_EVICT LLC
#define CG_FILTER_ENABLE 0
#define CG_FILTER_LOWER (0.5)
#define CG_FILTER_UPPER (1.0)

#define N_BEST_ELEMENTS 3

#define PAGE_SIZE SMALLPAGE

#define LLC_BOUND_LOW 27
#define LLC_BOUND_HIGH 90
#define RAM_BOUND_HIGH 1000

////////////////////// EVSETS //////////////////////
#define L2_EVSET_ALGORITHM GROUP_TESTING_NEW
#define L2_EVSET_FLAGS 0
#define L2_REDUCTIONS 1 // How many reductions we do on a new candidate set to get the required associativity of our evset
#define L2_CANDIDATE_SIZE_TARGET (L1D_ASSOCIATIVITY + L2_ASSOCIATIVITY)
#define L2_CANDIDATE_SET_SIZE ((L2_SETS / L1D_SETS) * L2_CANDIDATE_SIZE_TARGET)
#define L2_TRAVERSE (traverse_array_skylake)
#define L2_TRAVERSE_REPEATS 4 // could be 2

#define LLC_EVSET_ALGORITHM BINARY_SEARCH_ORIGINAL // GROUP_TESTING_OPTIMISED_NEW // BINARY_SEARCH_ORIGINAL
#define LLC_CANDIDATE_SIZE_TARGET (LLC_ASSOCIATIVITY + 1)
#define LLC_CANDIDATE_SET_SIZE ((SF_SETS / L1D_SETS) * (LLC_CANDIDATE_SIZE_TARGET) * LLC_SLICES)
#define LLC_TRAVERSE (traverse_array) // Does nothing on non-inclusive
#define LLC_TRAVERSE_REPEATS 4        // could be 2

#endif //__CONFIG_H__
