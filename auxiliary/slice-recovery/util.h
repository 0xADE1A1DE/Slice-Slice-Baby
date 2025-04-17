#ifndef __UTIL_H
#define __UTIL_H

#include "helpers.h"
#include <perf_counters.h>
#include <sys/mman.h>
#include <sys/time.h>

// Affinity to run the uncore performance counter interface on
// Can isolate the core then run this tool on it
#define AFFINITY 0

#define MAX_ADDR_BITS 64

#define UNCORE_PERFMON_SAMPLES 1000ULL

// Cache Info (L3 not needed)
#define L1_SETS (L1D / L1_ASSOCIATIVITY / L1_CACHELINE)
#define L1_STRIDE (L1_CACHELINE * L1_SETS)

#define L2_SETS (L2 / L2_ASSOCIATIVITY / L2_CACHELINE)
#define L2_STRIDE (L2_CACHELINE * L2_SETS)

#define MAX_ID 65536ULL

//////////////////////////////////////////////////////////////////////////////////////

typedef struct address_ptr_s
{
    uint64_t address;
    uint8_t *ptr;
    uint8_t *mem;
    int mem_fd;
} ADDRESS_PTR_T;

typedef struct measurement_util_s
{

} MEASUREMENT_UTIL_T;

int open_physical_address_ptr(ADDRESS_PTR_T *address_ptr);
int close_physical_address_ptr(ADDRESS_PTR_T *address_ptr);

//////////////////////////////////////////////////////////////////////////////////////

int get_address_slice(uncore_perfmon_t *u, uint64_t address);
int get_address_slice_sim(uint64_t address, uint64_t *xor_mask, int reduction_bits, int addr_bits, int *base_sequence, size_t seq_len);
int get_sim_params(char const *argv[], size_t *num_cbos, uint64_t **xor_mask, size_t *reduction_bits, size_t *addr_bits, int **base_sequence, size_t *seq_len);

//////////////////////////////////////////////////////////////////////////////////////

void update_xor_mask(uint64_t **xor_mask, size_t *reduction_bits, int addr_bit, int slice);
void get_xor_mask(uncore_perfmon_t *u, uint64_t *xor_mask, int *addr_bits);

//////////////////////////////////////////////////////////////////////////////////////

void print_xor_mask(uint64_t *xor_mask, int reduction_bits);
void print_base_sequence(int *base_sequence, size_t seq_len);

//////////////////////////////////////////////////////////////////////////////////////

uint64_t calculate_xor_reduction(uint64_t addr, int *xor_map, int addr_bits);
int calculate_address_slice(uint64_t paddr, int *base_sequence, size_t seq_len, int *xor_map, int addr_bits);

//////////////////////////////////////////////////////////////////////////////////////

void start_timer();
double stop_timer();

#endif //__UTIL_H