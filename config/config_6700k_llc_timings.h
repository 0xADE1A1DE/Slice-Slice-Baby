#ifndef __CONFIG_H__
#define __CONFIG_H__

#define INTEL_GEN6_4_SLICE
#define IS_INCLUSIVE 1
#define LLC_SLICES 4

#define VERBOSITY 0

#define MEASURE_SAMPLES 10

#define CG_MEASUREMENT 1 // 0 for RTDSCP, 1 for CG (Comparator gate)
#define CG_GROUND_TRUTH_CALIBRATE 1

#define CG_BALANCER_DELAY 2
#define CG_SIGNAL_EVICT LLC
#define CG_FILTER_ENABLE 0
#define CG_FILTER_LOWER (0.5)
#define CG_FILTER_UPPER (0.5)

#define N_BEST_ELEMENTS 3
#define MIN_DISTANCE 1

#define PAGE_SIZE SMALLPAGE

#define LLC_BOUND_LOW 40
#define LLC_BOUND_HIGH 90
#define RAM_BOUND_HIGH 1000

////////////////////// EVSETS //////////////////////
#define L2_EVSET_ALGORITHM GROUP_TESTING_NEW
#define L2_EVSET_FLAGS 0
#define L2_REDUCTIONS 1                                                 // How many reductions we do on a new candidate set to get the required associativity of our evset
#define L2_CANDIDATE_SIZE_TARGET (L1D_ASSOCIATIVITY + L2_ASSOCIATIVITY) // Skylake
#define L2_CANDIDATE_SET_SIZE ((L2_SETS / L1D_SETS) * (L2_CANDIDATE_SIZE_TARGET))
#define L2_TRAVERSE (traverse_array_skylake)
#define L2_TRAVERSE_REPEATS 3

#define LLC_EVSET_ALGORITHM GROUP_TESTING_OPTIMISED_NEW
#define LLC_CANDIDATE_SIZE_TARGET (LLC_ASSOCIATIVITY)
#define LLC_CANDIDATE_SET_SIZE ((L2_SETS / L1D_SETS) * (LLC_CANDIDATE_SIZE_TARGET) * LLC_SLICES)
#define LLC_TRAVERSE (traverse_array_rrip)
#define LLC_TRAVERSE_REPEATS 2

// Need this for modified NOT gate
#define ROB_SIZE 224

#endif //__CONFIG_H__
